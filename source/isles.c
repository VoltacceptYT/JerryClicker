#include "isles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "daycycle.h"
#include "gfxdata.h"
#include "islanddata.h"

/* ---------------- Terrain ---------------- */

#define SCENE_Y 18                                  // first row sits right under the header
#define COLS 16
#define ROWS 11                                     // last row is clipped by the screen edge

// ~ water, s sand, g grass. Water is autotiled around the island, grass over the sand.
static const char *const MAP[ROWS] = {
    "~~~~~~~~~~~~~~~~",
    "~~~~ssssssss~~~~",
    "~~ssssssssssss~~",
    "~ssggggggsgggss~",
    "~ssggggggsgggss~",
    "~ssggggggssssss~",
    "~ssggggggs~~~ss~",
    "~ssggggggs~~~ss~",
    "~~ssssssssssss~~",
    "~~~~ssssssss~~~~",
    "~~~~~~~~~~~~~~~~",
};

enum { DEC_DYN = 1, DEC_BLOCK = 2 };  // depth-sorted with critters / blocks the cells it stands on

typedef struct {
    uint8_t obj, cx, cy;
    int8_t offx;
    uint8_t flags, blockW;  // blockW: cells blocked, centred on cx
} Decor;

// Tall things are depth-sorted and solid; the small ones are baked into the ground.
static const Decor DECOR[] = {
    {OBJ_PALM, 2, 3, 0, DEC_DYN | DEC_BLOCK, 1},
    {OBJ_PALM, 13, 3, 0, DEC_DYN | DEC_BLOCK, 1},
    {OBJ_PALM, 13, 8, 0, DEC_DYN | DEC_BLOCK, 1},
    {OBJ_PALM, 5, 9, 0, DEC_DYN | DEC_BLOCK, 1},
    {OBJ_PARASOL, 9, 4, 0, DEC_DYN | DEC_BLOCK, 1},
    {OBJ_BENCH, 9, 9, 0, DEC_DYN | DEC_BLOCK, 3},
    {OBJ_ROCK_BIG, 1, 6, 0, DEC_DYN | DEC_BLOCK, 1},
    {OBJ_ROCK_SMALL, 2, 8, 0, DEC_DYN | DEC_BLOCK, 1},
    {OBJ_ROCK_BOULDER, 14, 4, 0, DEC_DYN | DEC_BLOCK, 1},
    {OBJ_ROCK_PAIR_L, 14, 7, 0, DEC_DYN | DEC_BLOCK, 1},
    // ground details
    {OBJ_FLOWERS, 5, 4, 0, 0, 0},   {OBJ_TUFT_A, 7, 6, 0, 0, 0},    {OBJ_TUFT_B, 4, 7, 0, 0, 0},
    {OBJ_TUFT_C, 11, 3, 0, 0, 0},   {OBJ_STARFISH, 3, 8, 0, 0, 0},  {OBJ_SHELL, 10, 2, 0, 0, 0},
    {OBJ_GSHELL, 7, 9, 0, 0, 0},    {OBJ_CORAL, 12, 5, 0, 0, 0},    {OBJ_SPIRAL, 5, 1, 0, 0, 0},
    {OBJ_CONCH, 12, 8, 0, 0, 0},    {OBJ_CRAB, 2, 5, 0, 0, 0},      {OBJ_STONE, 9, 8, 0, 0, 0},
    {OBJ_SPROUT, 8, 2, 0, 0, 0},    {OBJ_BUSH, 11, 8, 0, 0, 0},
};
#define DECOR_COUNT ((int)(sizeof(DECOR) / sizeof(DECOR[0])))

static px *bgBuf;                 // untinted terrain, built once
static px tintBuf[SCREEN_W * SCREEN_H] __attribute__((aligned(32)));  // bgBuf tinted for the time of day
static uint8_t walk[ROWS][COLS];  // 1 = critters may stand here

/* ---------------- Day/night tint ----------------
 * The light follows the real clock (daycycle.c). Tinting every pixel each frame is too slow for the
 * ARM9, so the terrain is tinted once into tintBuf whenever the light has moved enough to see, and the
 * sprites (decor and critters) are tinted through the same lookup tables as they are drawn. Each table
 * maps a 5-bit channel value straight to its tinted value, already shifted into place in the pixel. */
static uint16_t lutR[32], lutG[32], lutB[32];
static bool tintIdentity = true;  // full daylight: nothing to do
static bool tintSprites;          // set while isles_draw() runs; off while the terrain is being built
static DayTint bakedTint = {256, 256, 256};
static bool tintBaked;

static inline px tint_px(px v) {
    return (px)(0x8000 | lutR[v & 31] | lutG[(v >> 5) & 31] | lutB[(v >> 10) & 31]);
}

static void build_lut(uint16_t *lut, int mul, int shift) {
    for (int i = 0; i < 32; i++) {
        int v = (i * mul + 128) >> 8;
        lut[i] = (uint16_t)((v > 31 ? 31 : v) << shift);
    }
}

static void bake_tint(DayTint t) {
    bakedTint = t;
    tintBaked = true;
    tintIdentity = t.r >= 256 && t.g >= 256 && t.b >= 256;
    if (tintIdentity) {
        memcpy(tintBuf, bgBuf, sizeof tintBuf);
        return;
    }
    build_lut(lutR, t.r, 0);
    build_lut(lutG, t.g, 5);
    build_lut(lutB, t.b, 10);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) tintBuf[i] = tint_px(bgBuf[i]);
}

static int tint_delta(DayTint a, DayTint b) {
    int dr = (int)a.r - (int)b.r, dg = (int)a.g - (int)b.g, db = (int)a.b - (int)b.b;
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    return dr > dg ? (dr > db ? dr : db) : (dg > db ? dg : db);
}

void isles_update_clock(void) {
    if (!bgBuf) return;
    DayTint t = daycycle_tint();
    // Re-bake only once the light has moved by 4/256 or more on some channel: far below what the
    // eye can pick out in 5-bit colour, and it spaces the re-bakes minutes apart during dawn/dusk.
    if (!tintBaked || tint_delta(t, bakedTint) >= 4) bake_tint(t);
}

static inline int foot_x(const Decor *d) { return d->cx * 16 + 8 + d->offx; }
static inline int foot_y(const Decor *d) { return SCENE_Y + d->cy * 16 + 12; }

static char cell(int cx, int cy) {
    if (cx < 0 || cx >= COLS || cy < 0 || cy >= ROWS) return '~';  // outside the screen is open sea
    return MAP[cy][cx];
}

// Autotile piece (0..12, see islanddata.h) for a cell of terrain `kind`.
static int autotile(int cx, int cy, char kind) {
    bool n = cell(cx, cy - 1) == kind, e = cell(cx + 1, cy) == kind;
    bool s = cell(cx, cy + 1) == kind, w = cell(cx - 1, cy) == kind;
    if (!n) return !w ? 0 : (!e ? 2 : 1);
    if (!s) return !w ? 6 : (!e ? 8 : 7);
    if (!w) return 3;
    if (!e) return 5;
    if (cell(cx + 1, cy + 1) != kind) return 9;
    if (cell(cx - 1, cy + 1) != kind) return 10;
    if (cell(cx + 1, cy - 1) != kind) return 11;
    if (cell(cx - 1, cy - 1) != kind) return 12;
    return 4;
}

/* Object blit: bit 15 = opaque, 0x0001 = soft shadow (darken 25%), 0 = transparent. */
static void blit_obj(Canvas *c, const ObjSprite *o, int x, int y) {
    for (int yy = 0; yy < o->h; yy++) {
        int py = y + yy;
        if (py < c->cy0 || py >= c->cy1) continue;
        for (int xx = 0; xx < o->w; xx++) {
            int pxx = x + xx;
            if (pxx < c->cx0 || pxx >= c->cx1) continue;
            uint16_t v = o->px[yy * o->w + xx];
            px *dst = &c->buf[py * SCREEN_W + pxx];
            if (v & 0x8000) {
                *dst = (tintSprites && !tintIdentity) ? tint_px(v) : v;
            } else if (v == 1) {
                uint16_t d = *dst & 0x7FFF;
                *dst = 0x8000 | (((d >> 1) & 0x3DEF) + ((d >> 2) & 0x1CE7));
            }
        }
    }
}

static void shade_ellipse(Canvas *c, int cx, int cy) {  // small blob shadow under a critter
    static const int8_t half[3] = {3, 5, 3};
    for (int r = 0; r < 3; r++) {
        int y = cy - 1 + r;
        if (y < c->cy0 || y >= c->cy1) continue;
        for (int x = cx - half[r]; x <= cx + half[r]; x++) {
            if (x < c->cx0 || x >= c->cx1) continue;
            uint16_t d = c->buf[y * SCREEN_W + x] & 0x7FFF;
            c->buf[y * SCREEN_W + x] = 0x8000 | (((d >> 1) & 0x3DEF) + ((d >> 2) & 0x1CE7));
        }
    }
}

// Same as gfx_blit16 (no blending) but through the day/night tint.
static void blit16_tint(Canvas *c, const uint16_t *tex, int x, int y) {
    for (int yy = 0; yy < 16; yy++) {
        int py = y + yy;
        if (py < c->cy0 || py >= c->cy1) continue;
        for (int xx = 0; xx < 16; xx++) {
            int pxx = x + xx;
            if (pxx < c->cx0 || pxx >= c->cx1) continue;
            uint16_t v = tex[yy * 16 + xx];
            if (v & 0x8000) c->buf[py * SCREEN_W + pxx] = tint_px(v);
        }
    }
}

static void build_terrain(void) {
    Canvas c;
    gfx_begin(&c, bgBuf);
    for (int cy = -1; cy < ROWS; cy++) {  // row -1 keeps the strip behind the header sea-coloured
        for (int cx = 0; cx < COLS; cx++) {
            int y = SCENE_Y + cy * 16;
            char t = cell(cx, cy);
            if (t == 's') gfx_blit16(&c, isl_sand, cx * 16, y, 0);
            else if (t == 'g') gfx_blit16(&c, isl_grass[autotile(cx, cy, 'g')], cx * 16, y, 0);
            else gfx_blit16(&c, isl_water[autotile(cx, cy, '~')], cx * 16, y, 0);
        }
    }

    memset(walk, 0, sizeof walk);
    for (int cy = 0; cy < ROWS; cy++)
        for (int cx = 0; cx < COLS; cx++) walk[cy][cx] = cell(cx, cy) != '~';
    for (int i = 0; i < DECOR_COUNT; i++) {
        const Decor *d = &DECOR[i];
        if (d->flags & DEC_BLOCK)
            for (int k = 0; k < d->blockW; k++) {
                int cx = d->cx - d->blockW / 2 + k;
                if (cx >= 0 && cx < COLS) walk[d->cy][cx] = 0;
            }
        if (!(d->flags & DEC_DYN)) {
            const ObjSprite *o = &isl_objs[d->obj];
            blit_obj(&c, o, foot_x(d) - o->ax, foot_y(d) - o->ay);
        }
    }
}

/* ---------------- Critters ---------------- */

enum { ST_IDLE, ST_WALK, ST_FOLLOW, ST_GREET };
enum { EM_NONE, EM_HEART, EM_NOTE, EM_BANG };

typedef struct {
    uint8_t on, up, idx, state, spd, hop, emote, emoteT;
    int32_t x, y;        // feet position, 24.8 fixed point (screen pixels)
    int16_t tx, ty;      // walk target (pixels)
    int16_t vx, vy;      // 8.8 px per frame
    int16_t timer, cool;
    int8_t partner;
} Critter;

static Critter crit[ISL_MAX_CRITTERS];
static uint32_t rngState = 0x1234567u;
static uint32_t frameNo;

static uint32_t rnd(void) {
    uint32_t x = rngState;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rngState = x;
}
static int rr(int n) { return (int)(rnd() % (uint32_t)n); }  // 0..n-1
static int iabs(int v) { return v < 0 ? -v : v; }
static int dist_approx(int dx, int dy) {  // within ~12% of the real distance
    dx = iabs(dx); dy = iabs(dy);
    return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
}

static bool walk_cell(int x, int y) {
    if (x < 0 || x >= SCREEN_W || y < SCENE_Y) return false;
    int cy = (y - SCENE_Y) >> 4;
    return cy < ROWS && walk[cy][x >> 4];
}
// A critter is ~8px wide at the feet: check both sides so it never visibly steps into water or rocks.
static bool walk_at(int32_t fx, int32_t fy) {
    int x = (int)(fx >> 8), y = (int)(fy >> 8);
    return walk_cell(x, y) && walk_cell(x - 4, y) && walk_cell(x + 4, y);
}

static void set_idle(Critter *c, int frames) {
    c->state = ST_IDLE;
    c->vx = c->vy = 0;
    c->timer = (int16_t)frames;
}

static void pick_target(Critter *c) {
    int px0 = (int)(c->x >> 8), py0 = (int)(c->y >> 8);
    for (int tries = 0; tries < 10; tries++) {
        int tx, ty;
        if (rr(4) == 0) {  // sometimes head for shade: that is where critters bump into each other
            const Decor *d = &DECOR[rr(6)];  // the first six entries are palms, parasol and bench
            tx = foot_x(d) + rr(25) - 12;
            ty = foot_y(d) + 8 + rr(8);
        } else {
            tx = px0 + rr(113) - 56;
            ty = py0 + rr(81) - 40;
        }
        if (!walk_at((int32_t)tx << 8, (int32_t)ty << 8)) continue;
        int dx = tx - px0, dy = ty - py0;
        int len = dist_approx(dx, dy);
        if (len < 6) continue;
        c->tx = (int16_t)tx; c->ty = (int16_t)ty;
        c->vx = (int16_t)(dx * c->spd / len);
        c->vy = (int16_t)(dy * c->spd / len);
        c->state = ST_WALK;
        c->timer = (int16_t)(len * 256 / c->spd + 1);
        return;
    }
    set_idle(c, 20 + rr(30));
}

static void spawn(int slot, int up, int idx) {
    Critter *c = &crit[slot];
    memset(c, 0, sizeof *c);
    c->on = 1; c->up = (uint8_t)up; c->idx = (uint8_t)idx;
    c->spd = (uint8_t)(70 + rr(50));
    c->partner = -1;
    for (int tries = 0; tries < 60; tries++) {
        int cx = rr(COLS), cy = rr(ROWS);
        if (!walk[cy][cx]) continue;
        c->x = ((int32_t)(cx * 16 + 8 + rr(5) - 2)) << 8;
        c->y = ((int32_t)(SCENE_Y + cy * 16 + 8 + rr(5) - 2)) << 8;
        if (walk_at(c->x, c->y)) break;
    }
    c->hop = 8;  // pop in
    set_idle(c, rr(60));
}

static void drop_partner_refs(int slot) {
    for (int i = 0; i < ISL_MAX_CRITTERS; i++)
        if (crit[i].on && crit[i].partner == slot) {
            crit[i].partner = -1;
            if (crit[i].state == ST_GREET || crit[i].state == ST_FOLLOW) set_idle(&crit[i], 20);
        }
}

void isles_sync(const Game *g) {
    uint8_t wantUp[ISL_MAX_CRITTERS], wantIdx[ISL_MAX_CRITTERS];
    int nw = 0;
    // Round-robin over the upgrades so every owned upgrade keeps a presence when the island is full.
    for (int r = 0; nw < ISL_MAX_CRITTERS; r++) {
        bool any = false;
        for (int i = 0; i < UPGRADE_COUNT; i++) {
            if (g->s.up[i].count / ISL_PER > r) {
                any = true;
                if (nw < ISL_MAX_CRITTERS) { wantUp[nw] = (uint8_t)i; wantIdx[nw] = (uint8_t)r; nw++; }
            }
        }
        if (!any) break;
    }
    bool have[ISL_MAX_CRITTERS];
    memset(have, 0, sizeof have);
    for (int s = 0; s < ISL_MAX_CRITTERS; s++) {
        if (!crit[s].on) continue;
        int k;
        for (k = 0; k < nw; k++)
            if (!have[k] && wantUp[k] == crit[s].up && wantIdx[k] == crit[s].idx) break;
        if (k < nw) have[k] = true;
        else { crit[s].on = 0; drop_partner_refs(s); }
    }
    for (int k = 0; k < nw; k++) {
        if (have[k]) continue;
        for (int s = 0; s < ISL_MAX_CRITTERS; s++)
            if (!crit[s].on) { spawn(s, wantUp[k], wantIdx[k]); break; }
    }
}

int isles_critter_count(void) {
    int n = 0;
    for (int i = 0; i < ISL_MAX_CRITTERS; i++) n += crit[i].on;
    return n;
}

static void start_greet(int i, int j) {
    Critter *a = &crit[i], *b = &crit[j];
    a->state = b->state = ST_GREET;
    a->vx = a->vy = b->vx = b->vy = 0;
    a->partner = (int8_t)j; b->partner = (int8_t)i;
    a->timer = b->timer = 110;
    a->emote = b->emote = (a->up == b->up) ? EM_HEART : EM_NOTE;
    a->emoteT = b->emoteT = 110;
}

static void end_greet(int i) {
    Critter *a = &crit[i];
    int p = a->partner;
    a->cool = 300;
    a->emote = EM_NONE;
    if (p >= 0 && crit[p].on && i < p && rr(2) == 0) {  // sometimes one tags along behind the other
        a->state = ST_FOLLOW;
        a->timer = 240;
    } else {
        set_idle(a, 30 + rr(60));
        a->partner = -1;
    }
}

static void step_to(Critter *c, int32_t nx, int32_t ny) {  // slide along walls instead of sticking
    if (walk_at(nx, ny)) { c->x = nx; c->y = ny; }
    else if (walk_at(nx, c->y)) c->x = nx;
    else if (walk_at(c->x, ny)) c->y = ny;
}

static void update_critter(int i) {
    Critter *c = &crit[i];
    if (c->cool > 0) c->cool--;
    if (c->hop > 0) c->hop--;
    if (c->emoteT > 0 && --c->emoteT == 0) c->emote = EM_NONE;

    switch (c->state) {
    case ST_IDLE:
        if (--c->timer <= 0) pick_target(c);
        break;
    case ST_WALK: {
        int32_t nx = c->x + c->vx, ny = c->y + c->vy;
        if (!walk_at(nx, ny)) {
            if (!walk_at(nx, c->y) && !walk_at(c->x, ny)) { set_idle(c, 10 + rr(30)); break; }
            step_to(c, nx, ny);
        } else {
            c->x = nx; c->y = ny;
        }
        if (--c->timer <= 0) set_idle(c, 30 + rr(150));
        break;
    }
    case ST_FOLLOW: {
        Critter *p = (c->partner >= 0) ? &crit[(int)c->partner] : NULL;
        if (!p || !p->on || --c->timer <= 0) { set_idle(c, 20); c->partner = -1; c->cool = 200; break; }
        int dx = (int)((p->x - c->x) >> 8), dy = (int)((p->y - c->y) >> 8);
        int len = dist_approx(dx, dy);
        if (len > 16) {
            step_to(c, c->x + dx * c->spd / len, c->y + dy * c->spd / len);
            c->vx = 1;
        } else {
            c->vx = 0;
        }
        break;
    }
    case ST_GREET: {
        Critter *p = (c->partner >= 0) ? &crit[(int)c->partner] : NULL;
        if (!p || !p->on) { set_idle(c, 10); c->partner = -1; c->emote = EM_NONE; break; }
        int dx = (int)((p->x - c->x) >> 8), dy = (int)((p->y - c->y) >> 8);
        int len = dist_approx(dx, dy);
        if (len > 12 && len > 0) step_to(c, c->x + dx * 64 / len, c->y + dy * 64 / len);
        // take turns hopping
        if ((c->timer % 24) == (i < c->partner ? 0 : 12)) c->hop = 8;
        if (--c->timer <= 0) end_greet(i);
        break;
    }
    }
}

static void pair_interactions(void) {
    for (int i = 0; i < ISL_MAX_CRITTERS; i++) {
        Critter *a = &crit[i];
        if (!a->on) continue;
        for (int j = i + 1; j < ISL_MAX_CRITTERS; j++) {
            Critter *b = &crit[j];
            if (!b->on) continue;
            int dx = (int)((b->x - a->x) >> 8), dy = (int)((b->y - a->y) >> 8);
            if (dx > 24 || dx < -24 || dy > 24 || dy < -24) continue;
            int d = dist_approx(dx, dy);

            // Meet & greet: same kind of Jerry item bonds more readily
            if (d < 22 && a->state <= ST_WALK && b->state <= ST_WALK && a->cool == 0 && b->cool == 0 &&
                rr(a->up == b->up ? 50 : 200) == 0) {
                start_greet(i, j);
                continue;
            }
            if (d >= 10) continue;

            // Bump: bounce off each other, give an "!" and head somewhere else
            if (a->state == ST_WALK && b->state == ST_WALK && a->hop == 0 && b->hop == 0) {
                a->hop = b->hop = 8;
                a->emote = b->emote = EM_BANG;
                a->emoteT = b->emoteT = 30;
                set_idle(a, 6 + rr(14));
                set_idle(b, 6 + rr(14));
            }
            // Never overlap: nudge apart (greeting pairs keep a closer 8px distance)
            int minD = (a->state == ST_GREET && a->partner == j) ? 8 : 10;
            if (d < minD) {
                if (d == 0) { dx = rr(3) - 1; dy = 1; d = 1; }
                int push = (minD - d) * 96;  // 24.8 per side
                int32_t px = dx * push / d, py = dy * push / d;
                step_to(a, a->x - px, a->y - py);
                step_to(b, b->x + px, b->y + py);
            }
        }
    }
}

void isles_tick(void) {
    frameNo++;
    for (int i = 0; i < ISL_MAX_CRITTERS; i++)
        if (crit[i].on) update_critter(i);
    pair_interactions();
}

/* ---------------- Drawing ---------------- */

static const uint8_t HEART[6] = {0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08};  // 7 wide, bit 6 = left
static const uint8_t NOTE[6] = {0x0C, 0x0A, 0x08, 0x38, 0x78, 0x30};   // 6 wide, bit 5 = left
static const uint8_t BANG[7] = {0x3, 0x3, 0x3, 0x3, 0x0, 0x3, 0x3};    // 2 wide, bit 1 = left

static void draw_bits(Canvas *c, const uint8_t *rows, int h, int w, int x, int y, px col) {
    for (int r = 0; r < h; r++)
        for (int b = 0; b < w; b++)
            if (rows[r] & (1 << (w - 1 - b))) gfx_fill(c, x + b, y + r, 1, 1, col);
}

static void draw_emote(Canvas *c, const Critter *k, int x, int y) {
    if (k->emote == EM_NONE || (k->emoteT < 12 && (k->emoteT & 2))) return;  // blink out
    const uint8_t *rows; int h, w; px col, dark;
    switch (k->emote) {
    case EM_HEART: rows = HEART; h = 6; w = 7; col = RGB8(255, 90, 140); dark = RGB8(90, 10, 40); break;
    case EM_NOTE: rows = NOTE; h = 6; w = 6; col = RGB8(120, 200, 255); dark = RGB8(10, 40, 90); break;
    default: rows = BANG; h = 7; w = 2; col = RGB8(255, 230, 60); dark = RGB8(90, 60, 0); break;
    }
    draw_bits(c, rows, h, w, x - w / 2 + 1, y + 1, dark);
    draw_bits(c, rows, h, w, x - w / 2, y, col);
}

// The time of day, painted on the bench's seat like part of the sprite: dark wood-stain letters with a
// light edge below, tinted by the day/night light exactly like the bench itself.
static void draw_bench_clock(Canvas *c, const Decor *d) {
    const ObjSprite *o = &isl_objs[d->obj];
    char t[16];
    daycycle_time_string(t, sizeof t);
    int x = foot_x(d) - o->ax + (o->w - gfx_text_width(&font10, t)) / 2;
    int y = foot_y(d) - o->ay + (o->h - font10.height) / 2;
    px dark = RGB8(62, 34, 26), edge = RGB8(176, 120, 92);
    if (!tintIdentity) { dark = tint_px(dark); edge = tint_px(edge); }
    gfx_text(c, &font10, x + 1, y + 1, t, edge, 0);
    gfx_text(c, &font10, x, y, t, dark, 0);
}

typedef struct { int16_t y; uint8_t kind, id; } DrawItem;  // kind 0 = decor, 1 = critter

void isles_draw(Canvas *c) {
    static const uint8_t hopTab[8] = {0, 2, 4, 5, 5, 4, 2, 1};
    static const uint8_t walkTab[4] = {0, 1, 2, 1};
    DrawItem items[ISL_MAX_CRITTERS + DECOR_COUNT];
    int n = 0;
    tintSprites = true;
    for (int i = 0; i < DECOR_COUNT; i++)
        if (DECOR[i].flags & DEC_DYN) {
            items[n].y = (int16_t)foot_y(&DECOR[i]); items[n].kind = 0; items[n].id = (uint8_t)i; n++;
        }
    for (int i = 0; i < ISL_MAX_CRITTERS; i++)
        if (crit[i].on) {
            items[n].y = (int16_t)(crit[i].y >> 8); items[n].kind = 1; items[n].id = (uint8_t)i; n++;
        }
    for (int a = 1; a < n; a++) {  // insertion sort by feet y: things lower on screen draw on top
        DrawItem t = items[a];
        int b = a - 1;
        while (b >= 0 && items[b].y > t.y) { items[b + 1] = items[b]; b--; }
        items[b + 1] = t;
    }

    int lift[ISL_MAX_CRITTERS];
    for (int a = 0; a < n; a++) {
        if (items[a].kind == 0) {
            const Decor *d = &DECOR[items[a].id];
            const ObjSprite *o = &isl_objs[d->obj];
            blit_obj(c, o, foot_x(d) - o->ax, foot_y(d) - o->ay);
            if (d->obj == OBJ_BENCH) draw_bench_clock(c, d);
            continue;
        }
        int id = items[a].id;
        const Critter *k = &crit[id];
        int x = (int)(k->x >> 8), y = (int)(k->y >> 8);
        int up = 0;
        if (k->hop > 0) up = hopTab[k->hop > 8 ? 7 : k->hop - 1];
        else if (k->state == ST_WALK || (k->state == ST_FOLLOW && k->vx)) up = walkTab[((frameNo >> 3) + id) & 3];
        lift[id] = up;
        shade_ellipse(c, x, y - 1);
        if (tintIdentity) gfx_blit16(c, icon_rgb[k->up], x - 8, y - 15 - up, 0);
        else blit16_tint(c, icon_rgb[k->up], x - 8, y - 15 - up);
    }
    tintSprites = false;  // emotes and the hint panel stay bright so they read at night
    for (int a = 0; a < n; a++)  // emotes on top of everything
        if (items[a].kind == 1) {
            int id = items[a].id;
            draw_emote(c, &crit[id], (int)(crit[id].x >> 8), (int)(crit[id].y >> 8) - 27 - lift[id]);
        }

    if (isles_critter_count() == 0) {
        gfx_bevel_panel(c, 34, 120, 188, 28, RGB8(34, 34, 34), RGB8(85, 85, 85), RGB8(0, 0, 0));
        gfx_text_centered(c, &font10, 128, 124, "BUY 5 OF AN UPGRADE TO", RGB8(255, 255, 255), 1);
        gfx_text_centered(c, &font10, 128, 135, "POPULATE THE ISLES", RGB8(255, 255, 255), 1);
    }
}

/* ---------------- Setup ---------------- */

void isles_init(px *bg, uint32_t seed) {
    bgBuf = bg;
    rngState = seed ? seed : 0x1234567u;
    memset(crit, 0, sizeof crit);
    build_terrain();
    tintBaked = false;
    isles_update_clock();
}

const px *isles_bg(void) { return bgBuf ? tintBuf : NULL; }
