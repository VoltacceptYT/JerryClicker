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

enum { ST_IDLE, ST_WALK, ST_FOLLOW, ST_GREET, ST_PLAY, ST_BUILD };
enum { EM_NONE, EM_HEART, EM_NOTE, EM_BANG };

typedef struct {
    uint8_t on, up, idx, state, spd, hop, emote, emoteT;
    int32_t x, y;        // feet position, 24.8 fixed point (screen pixels)
    int16_t tx, ty;      // walk target (pixels)
    int16_t vx, vy;      // 8.8 px per frame
    int16_t timer, cool;
    int8_t partner;
    int8_t toy;          // ST_PLAY: ball index, ST_BUILD: castle index
    uint8_t phase;       // ST_BUILD: 0 = walking to the site, 1 = digging
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
    c->toy = -1;
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

static void release_toys(int slot);
static void sync_toys(const Game *g);

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
        else { crit[s].on = 0; drop_partner_refs(s); release_toys(s); }
    }
    for (int k = 0; k < nw; k++) {
        if (have[k]) continue;
        for (int s = 0; s < ISL_MAX_CRITTERS; s++)
            if (!crit[s].on) { spawn(s, wantUp[k], wantIdx[k]); break; }
    }
    sync_toys(g);
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

/* ---------------- Toys ----------------
 * Playthings (beach ball, squeaky mouse, magic 8-ball, balloon snake, golden ball) all share the Ball
 * simulation, tuned per kind in BALL_KIND[]. Critters kick a toy away from themselves, it arcs through the
 * air, hops off the ground losing some height each time and ricochets off water, palms and rocks. Idle
 * critters sometimes chase one. Beach ball: classic bounce. White dice: critters roll it and the number
 * they rolled (d20) pops up once it lands. Magic 8-ball: heavy,
 * barely bounces, rolls far. Squeaky mouse: scurries about on its own. Balloon snake: floats and drifts.
 * Plastic shovels (Game.toys[TOY_SHOVEL]): up to as many critters as there are shovels walk to a sand
 * patch, dig until a sandcastle stands there, and the castle later crumbles away to make room for more. */

#define BALL_GRAV 40          // 8.8 px/frame^2
#define BALL_MIN_BOUNCE 90    // below this vertical speed the ball just rolls
#define BUILD_FRAMES 270
#define CASTLE_LIFE (50 * 60)
#define CRUMBLE_STEP 30

typedef struct {
    uint8_t on, cool, kind; // kind: index into BALL_KIND[]
    int32_t x, y, z;        // ground position and height, 24.8 fixed point (pixels)
    int16_t vx, vy, vz;     // 8.8 px per frame
    int16_t timer;          // mouse: frames until the next scurry; balloon: frames until the next drift change
    uint8_t roll, showRoll; // dice: the d20 result of the last throw, and frames left to show it once landed
} Ball;

typedef struct {
    uint8_t toy;            // Game.toys[] index
    int8_t grav;            // 8.8 px/frame^2 (balloons ignore it and hover)
    int16_t minBounce;      // below this vertical speed the toy just rolls
    uint8_t bounceNum, bounceDen;   // fraction of the vertical speed kept on each bounce
    uint8_t fricNum, fricDen;       // ground friction per frame while rolling
    int16_t kickSpeed, kickRand;    // horizontal kick: base + random
    int16_t kickUp, kickUpRand;     // vertical kick: base + random
    uint8_t kickReach;      // how high above the ground a critter can still reach it (px)
} BallKind;

static const BallKind BALL_KIND[ISL_BALL_KINDS] = {
    {TOY_BALL,    40, 90, 3, 4, 15, 16, 200, 220, 380, 200, 10},
    {TOY_MOUSE,   40, 60, 1, 4, 7,  8,  120, 100, 160, 80,  10},
    {TOY_EIGHT,   48, 80, 1, 2, 31, 32, 230, 200, 240, 120, 10},
    {TOY_BALLOON, 0,  0,  1, 1, 63, 64, 90,  80,  220, 80,  44},
    {TOY_DICE,    48, 120, 1, 2, 7, 8,  200, 120, 260, 100, 10},
};

typedef struct {
    uint8_t on, stage;      // stage 0 = site reserved, nothing dug yet; 1..3 = mound, tower, castle
    int8_t builder;         // critter slot digging it, -1 once finished
    int16_t x, y;           // feet (pixels)
    int32_t age;            // build progress while building, then frames since it was finished
} Castle;

static Ball balls[ISL_MAX_BALLS];
static Castle castles[ISL_MAX_CASTLES];
static int ballsWanted, shovelsWanted;  // ballsWanted: all ball-like toys together

int isles_castle_count(void) {
    int n = 0;
    for (int i = 0; i < ISL_MAX_CASTLES; i++) n += castles[i].on && castles[i].stage > 0;
    return n;
}

static bool ball_walk(int32_t fx, int32_t fy) {
    int x = (int)(fx >> 8), y = (int)(fy >> 8);
    return walk_cell(x, y) && walk_cell(x - 3, y) && walk_cell(x + 3, y);
}

static void spawn_ball(Ball *b, int kind) {
    memset(b, 0, sizeof *b);
    b->on = 1;
    b->kind = (uint8_t)kind;
    b->timer = (int16_t)(30 + rr(90));
    for (int tries = 0; tries < 60; tries++) {
        int cx = rr(COLS), cy = rr(ROWS);
        if (!walk[cy][cx]) continue;
        b->x = ((int32_t)(cx * 16 + 8 + rr(5) - 2)) << 8;
        b->y = ((int32_t)(SCENE_Y + cy * 16 + 8 + rr(5) - 2)) << 8;
        if (ball_walk(b->x, b->y)) break;
    }
    b->z = (int32_t)70 << 8;  // drops in from above and bounces
    if (BALL_KIND[kind].toy == TOY_MOUSE) b->z = 0;
}

static void update_ball(Ball *b) {
    const BallKind *k = &BALL_KIND[b->kind];
    if (b->cool) b->cool--;
    if (k->toy == TOY_BALLOON) {
        // Hover about 24px up, bobbing gently, and drift on a slow random breeze.
        int32_t target = ((int32_t)24 << 8) + (((frameNo >> 4) & 1) ? 256 : -256);
        b->vz = (int16_t)(b->vz + (target - b->z) / 48);
        b->vz = (int16_t)(b->vz * 15 / 16);
        b->z += b->vz;
        if (b->z < 0) { b->z = 0; b->vz = 0; }
        if (--b->timer <= 0) {
            b->timer = (int16_t)(90 + rr(120));
            b->vx = (int16_t)(rr(81) - 40);
            b->vy = (int16_t)(rr(81) - 40);
        }
        b->vx = (int16_t)(b->vx * k->fricNum / k->fricDen);
        b->vy = (int16_t)(b->vy * k->fricNum / k->fricDen);
    } else {
        if (k->toy == TOY_MOUSE && b->z == 0 && --b->timer <= 0) {
            // scurry off in a random direction with a little hop
            b->timer = (int16_t)(60 + rr(120));
            b->vx = (int16_t)(rr(301) - 150);
            b->vy = (int16_t)(rr(301) - 150);
            b->vz = (int16_t)(120 + rr(60));
        }
        b->vz = (int16_t)(b->vz - k->grav);
        b->z += b->vz;
        if (b->z <= 0) {
            b->z = 0;
            if (b->vz < -k->minBounce) {
                b->vz = (int16_t)(-b->vz * k->bounceNum / k->bounceDen);
                b->vx = (int16_t)(b->vx * 7 / 8);
                b->vy = (int16_t)(b->vy * 7 / 8);
            } else {
                b->vz = 0;
                b->vx = (int16_t)(b->vx * k->fricNum / k->fricDen);
                b->vy = (int16_t)(b->vy * k->fricNum / k->fricDen);
            }
        }
    }
    if (k->toy == TOY_DICE && b->roll) {  // once it has settled, show what was rolled for a couple of seconds
        if (b->showRoll) { if (--b->showRoll == 0) b->roll = 0; }
        else if (b->z == 0 && iabs(b->vx) + iabs(b->vy) < 24) b->showRoll = 150;
    }
    int32_t nx = b->x + b->vx, ny = b->y + b->vy;
    if (ball_walk(nx, ny)) {
        b->x = nx; b->y = ny;
    } else if (ball_walk(nx, b->y)) {
        b->x = nx; b->vy = (int16_t)(-b->vy * 3 / 4);
    } else if (ball_walk(b->x, ny)) {
        b->y = ny; b->vx = (int16_t)(-b->vx * 3 / 4);
    } else {
        b->vx = (int16_t)(-b->vx * 3 / 4);
        b->vy = (int16_t)(-b->vy * 3 / 4);
    }
}

static void kick_ball(Ball *b) {
    const BallKind *k = &BALL_KIND[b->kind];
    if (b->cool || b->z > ((int32_t)k->kickReach << 8)) return;
    int bx = (int)(b->x >> 8), by = (int)(b->y >> 8);
    for (int i = 0; i < ISL_MAX_CRITTERS; i++) {
        Critter *c = &crit[i];
        if (!c->on || c->state == ST_GREET || c->state == ST_BUILD) continue;
        int dx = bx - (int)(c->x >> 8), dy = by - (int)(c->y >> 8);
        if (dx > 12 || dx < -12 || dy > 12 || dy < -12) continue;
        int d = dist_approx(dx, dy);
        if (d > 11) continue;
        if (d == 0) { dx = rr(3) - 1; dy = 1; d = iabs(dx) + 1; }
        int sp = k->kickSpeed + rr(k->kickRand);
        b->vx = (int16_t)(dx * sp / d + rr(61) - 30);
        b->vy = (int16_t)(dy * sp / d + rr(61) - 30);
        b->vz = (int16_t)(k->kickUp + rr(k->kickUpRand));
        if (k->toy == TOY_DICE) { b->roll = (uint8_t)(1 + rr(20)); b->showRoll = 0; }
        b->cool = 18;
        c->hop = 8;
        return;
    }
}

static int builder_count(void) {
    int n = 0;
    for (int i = 0; i < ISL_MAX_CASTLES; i++) n += castles[i].on && castles[i].builder >= 0;
    return n;
}

static bool start_build(Critter *c, int slot) {
    int ci = -1;
    for (int k = 0; k < ISL_MAX_CASTLES && k < shovelsWanted * 2; k++)
        if (!castles[k].on) { ci = k; break; }
    if (ci < 0) return false;
    for (int tries = 0; tries < 24; tries++) {
        int cx = rr(COLS), cy = rr(ROWS);
        if (cell(cx, cy) != 's' || !walk[cy][cx]) continue;
        int x = cx * 16 + 8 + rr(5) - 2, y = SCENE_Y + cy * 16 + 10 + rr(3);
        if (!walk_at((int32_t)x << 8, (int32_t)y << 8)) continue;
        bool clear = true;
        for (int k = 0; k < ISL_MAX_CASTLES; k++)
            if (castles[k].on && dist_approx(x - castles[k].x, y - castles[k].y) < 24) clear = false;
        if (!clear) continue;
        Castle *k = &castles[ci];
        k->on = 1; k->stage = 0; k->builder = (int8_t)slot; k->x = (int16_t)x; k->y = (int16_t)y; k->age = 0;
        c->state = ST_BUILD; c->phase = 0; c->toy = (int8_t)ci; c->vx = c->vy = 0; c->timer = 900;
        return true;
    }
    return false;
}

static bool start_play(Critter *c) {
    int best = -1, bestD = 0;
    int px0 = (int)(c->x >> 8), py0 = (int)(c->y >> 8);
    for (int i = 0; i < ISL_MAX_BALLS; i++) {
        if (!balls[i].on) continue;
        int d = dist_approx((int)(balls[i].x >> 8) - px0, (int)(balls[i].y >> 8) - py0);
        if (best < 0 || d < bestD) { best = i; bestD = d; }
    }
    if (best < 0) return false;
    c->state = ST_PLAY; c->toy = (int8_t)best; c->timer = (int16_t)(240 + rr(240)); c->vx = 0; c->vy = 0;
    return true;
}

// An idle critter that has finished resting may pick up a toy instead of just wandering.
static bool try_toy(Critter *c, int slot) {
    if (shovelsWanted > 0 && builder_count() < shovelsWanted && rr(3) == 0 && start_build(c, slot)) return true;
    if (ballsWanted > 0 && rr(2) == 0 && start_play(c)) return true;
    return false;
}

static void release_toys(int slot) {
    for (int k = 0; k < ISL_MAX_CASTLES; k++)
        if (castles[k].on && castles[k].builder == slot) {
            if (castles[k].stage < 3) castles[k].on = 0;
            castles[k].builder = -1;
            castles[k].age = 0;
        }
}

static void update_castle(Castle *k) {
    if (!k->on || k->builder >= 0) return;
    k->age++;
    if (k->age > CASTLE_LIFE) {  // washes away: tower by tower
        int t = (k->age - CASTLE_LIFE) / CRUMBLE_STEP;
        if (t >= 3) k->on = 0;
        else k->stage = (uint8_t)(3 - t);
    }
}

static void sync_toys(const Game *g) {
    shovelsWanted = g->s.toys[TOY_SHOVEL] > TOY_MAX ? TOY_MAX : g->s.toys[TOY_SHOVEL];
    ballsWanted = 0;
    // Slot layout: kind k owns balls[k * TOY_MAX .. k * TOY_MAX + TOY_MAX - 1].
    for (int k = 0; k < ISL_BALL_KINDS; k++) {
        int want = g->s.toys[BALL_KIND[k].toy];
        if (want > TOY_MAX) want = TOY_MAX;
        ballsWanted += want;
        for (int i = 0; i < TOY_MAX; i++) {
            Ball *b = &balls[k * TOY_MAX + i];
            if (i < want && !b->on) spawn_ball(b, k);
            else if (i >= want) b->on = 0;
        }
    }
}

static void update_critter(int i) {
    Critter *c = &crit[i];
    if (c->cool > 0) c->cool--;
    if (c->hop > 0) c->hop--;
    if (c->emoteT > 0 && --c->emoteT == 0) c->emote = EM_NONE;

    switch (c->state) {
    case ST_IDLE:
        if (--c->timer <= 0 && !try_toy(c, i)) pick_target(c);
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
    case ST_PLAY: {
        Ball *b = (c->toy >= 0) ? &balls[(int)c->toy] : NULL;
        if (!b || !b->on || --c->timer <= 0) { set_idle(c, 20 + rr(40)); c->toy = -1; break; }
        int dx = (int)((b->x - c->x) >> 8), dy = (int)((b->y - c->y) >> 8);
        int len = dist_approx(dx, dy);
        if (len > 8) {
            int sp = c->spd + 40;
            step_to(c, c->x + dx * sp / len, c->y + dy * sp / len);
            c->vx = 1;
        } else {
            c->vx = 0;
        }
        break;
    }
    case ST_BUILD: {
        Castle *k = (c->toy >= 0) ? &castles[(int)c->toy] : NULL;
        if (!k || !k->on || k->builder != i) { set_idle(c, 20); c->toy = -1; break; }
        if (c->phase == 0) {
            int dx = k->x - (int)(c->x >> 8), dy = k->y - (int)(c->y >> 8);
            int len = dist_approx(dx, dy);
            if (len <= 3) {
                c->phase = 1; c->vx = 0; k->age = 0;
            } else if (--c->timer <= 0) {
                k->on = 0; set_idle(c, 30); c->toy = -1;
            } else {
                step_to(c, c->x + dx * (c->spd + 20) / len, c->y + dy * (c->spd + 20) / len);
                c->vx = 1;
            }
            break;
        }
        k->age++;
        uint8_t stage = k->age < 80 ? 1 : (k->age < 180 ? 2 : 3);
        if (stage != k->stage) { k->stage = stage; c->hop = 8; }
        else if (k->age % 30 == 0) c->hop = 8;
        if (k->age >= BUILD_FRAMES) {
            k->stage = 3; k->builder = -1; k->age = 0;
            c->emote = EM_HEART; c->emoteT = 90; c->hop = 8; c->cool = 200;
            set_idle(c, 60 + rr(60)); c->toy = -1;
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
    for (int i = 0; i < ISL_MAX_BALLS; i++)
        if (balls[i].on) { kick_ball(&balls[i]); update_ball(&balls[i]); }
    for (int i = 0; i < ISL_MAX_CASTLES; i++) update_castle(&castles[i]);
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

static void blit_sprite(Canvas *c, const uint16_t *tex, int w, int h, int x, int y) {
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < c->cy0 || py >= c->cy1) continue;
        for (int xx = 0; xx < w; xx++) {
            int pxx = x + xx;
            if (pxx < c->cx0 || pxx >= c->cx1) continue;
            uint16_t v = tex[yy * w + xx];
            if (v & 0x8000) c->buf[py * SCREEN_W + pxx] = (tintSprites && !tintIdentity) ? tint_px(v) : v;
        }
    }
}

static void sand_rect(Canvas *c, int x, int y, int w, int h, px col) {
    gfx_fill(c, x, y, w, h, tintIdentity ? col : tint_px(col));
}

static void draw_castle(Canvas *c, const Castle *k) {
    const px lite = RGB8(240, 222, 160), mid = RGB8(214, 192, 122), dark = RGB8(136, 112, 62);
    const px door = RGB8(86, 66, 36), pole = RGB8(110, 80, 50), flag = RGB8(230, 60, 60);
    int fx = k->x, fy = k->y;
    if (k->stage == 0) return;
    shade_ellipse(c, fx, fy - 1);
    if (k->stage == 1) {
        sand_rect(c, fx - 5, fy - 3, 10, 3, mid);
        sand_rect(c, fx - 3, fy - 5, 6, 2, lite);
        sand_rect(c, fx - 5, fy - 1, 10, 1, dark);
    } else if (k->stage == 2) {
        sand_rect(c, fx - 7, fy - 4, 14, 4, mid);
        sand_rect(c, fx - 7, fy - 1, 14, 1, dark);
        sand_rect(c, fx - 4, fy - 8, 8, 4, lite);
        sand_rect(c, fx + 3, fy - 8, 1, 4, mid);
    } else {
        sand_rect(c, fx - 8, fy - 5, 16, 5, mid);
        sand_rect(c, fx - 8, fy - 1, 16, 1, dark);
        sand_rect(c, fx - 8, fy - 11, 5, 6, lite);       // left tower
        sand_rect(c, fx - 4, fy - 11, 1, 6, mid);
        sand_rect(c, fx + 3, fy - 11, 5, 6, lite);       // right tower
        sand_rect(c, fx + 7, fy - 11, 1, 6, mid);
        for (int i = 0; i < 3; i++) {
            sand_rect(c, fx - 8 + i * 2, fy - 12, 1, 1, lite);
            sand_rect(c, fx + 3 + i * 2, fy - 12, 1, 1, lite);
            sand_rect(c, fx - 3 + i * 2, fy - 10, 1, 1, lite);
        }
        sand_rect(c, fx - 3, fy - 9, 6, 4, lite);        // keep
        sand_rect(c, fx - 1, fy - 4, 2, 4, door);
        sand_rect(c, fx, fy - 16, 1, 6, pole);
        sand_rect(c, fx + 1, fy - 16, 3, 2, flag);
    }
}

typedef struct { int16_t y; uint8_t kind, id; } DrawItem;  // kind 0 = decor, 1 = critter, 2 = ball, 3 = castle

void isles_draw(Canvas *c) {
    static const uint8_t hopTab[8] = {0, 2, 4, 5, 5, 4, 2, 1};
    static const uint8_t walkTab[4] = {0, 1, 2, 1};
    DrawItem items[ISL_MAX_CRITTERS + DECOR_COUNT + ISL_MAX_BALLS + ISL_MAX_CASTLES];
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
    for (int i = 0; i < ISL_MAX_BALLS; i++)
        if (balls[i].on) {
            items[n].y = (int16_t)(balls[i].y >> 8); items[n].kind = 2; items[n].id = (uint8_t)i; n++;
        }
    for (int i = 0; i < ISL_MAX_CASTLES; i++)
        if (castles[i].on && castles[i].stage > 0) {
            items[n].y = castles[i].y; items[n].kind = 3; items[n].id = (uint8_t)i; n++;
        }
    for (int a = 1; a < n; a++) {  // insertion sort by feet y: things lower on screen draw on top
        DrawItem t = items[a];
        int b = a - 1;
        while (b >= 0 && items[b].y > t.y) { items[b + 1] = items[b]; b--; }
        items[b + 1] = t;
    }

    int lift[ISL_MAX_CRITTERS];
    for (int a = 0; a < n; a++) {
        if (items[a].kind == 2) {
            const Ball *b = &balls[items[a].id];
            int x = (int)(b->x >> 8), y = (int)(b->y >> 8), z = (int)(b->z >> 8);
            shade_ellipse(c, x, y - 1);
            blit_sprite(c, toy_rgb[BALL_KIND[b->kind].toy], 16, 16, x - 8, y - 14 - z);
            continue;
        }
        if (items[a].kind == 3) {
            draw_castle(c, &castles[items[a].id]);
            continue;
        }
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
        else if (k->state == ST_WALK || ((k->state == ST_FOLLOW || k->state == ST_PLAY || k->state == ST_BUILD) && k->vx)) up = walkTab[((frameNo >> 3) + id) & 3];
        lift[id] = up;
        shade_ellipse(c, x, y - 1);
        if (tintIdentity) gfx_blit16(c, icon_rgb[k->up], x - 8, y - 15 - up, 0);
        else blit16_tint(c, icon_rgb[k->up], x - 8, y - 15 - up);
        if (k->state == ST_BUILD) blit_sprite(c, shovel_small_rgb, 8, 8, x + 6, y - 9 - up);
    }
    tintSprites = false;  // emotes and the hint panel stay bright so they read at night
    for (int a = 0; a < n; a++) {  // emotes (and dice results) on top of everything
        if (items[a].kind == 1) {
            int id = items[a].id;
            draw_emote(c, &crit[id], (int)(crit[id].x >> 8), (int)(crit[id].y >> 8) - 27 - lift[id]);
        } else if (items[a].kind == 2) {
            const Ball *b = &balls[items[a].id];
            if (BALL_KIND[b->kind].toy == TOY_DICE && b->showRoll) {
                char num[8];
                snprintf(num, sizeof num, "%d", (int)b->roll);
                px col = b->roll == 20 ? RGB8(255, 170, 0) : (b->roll == 1 ? RGB8(255, 85, 85) : RGB8(255, 255, 255));
                gfx_text_centered(c, &font10, (int)(b->x >> 8), (int)(b->y >> 8) - 25 - (int)(b->z >> 8), num, col, 1);
            }
        }
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
    memset(balls, 0, sizeof balls);
    memset(castles, 0, sizeof castles);
    build_terrain();
    tintBaked = false;
    isles_update_clock();
}

const px *isles_bg(void) { return bgBuf ? tintBuf : NULL; }
