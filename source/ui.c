#include "ui.h"

#include <stdio.h>
#include <string.h>

#define C_WHITE RGB8(255, 255, 255)
#define C_DIM RGB8(170, 170, 170)
#define C_GOLD RGB8(255, 170, 0)
#define C_GREEN RGB8(85, 255, 85)
#define C_RED RGB8(255, 85, 85)
#define C_PURPLE RGB8(191, 111, 255)

#define ROW_X 3
#define ROW_W 243
#define ROW_H 22
#define ROW_PITCH 23
#define LIST_Y 20

void ui_draw_backgrounds(px *topBg, px *botBg) {
    Canvas c;
    gfx_begin(&c, topBg);
    gfx_tile(&c, tex_stone, 0, 0, SCREEN_W, SCREEN_H);
    gfx_begin(&c, botBg);
    gfx_tile(&c, tex_stone, 0, 0, SCREEN_W, SCREEN_H);
}

static void panel(Canvas *c, int x, int y, int w, int h) {
    gfx_bevel_panel(c, x, y, w, h, RGB8(34, 34, 34), RGB8(85, 85, 85), RGB8(0, 0, 0));
}

/* ---------------- Jerry Isles page ---------------- */

// The scene fills the whole page below the header.
#define ISLE_X 3
#define ISLE_Y 20
#define ISLE_W 250
#define ISLE_H 170
#define ISLE_PER 5          // one sprite per this many purchases of an upgrade
#define ISLE_MAX_PER 20     // per-upgrade cap so the scene stays fast (= 100 purchases)

typedef struct { int16_t x, y; uint8_t icon; } IsleSprite;
static IsleSprite isleSprites[UPGRADE_COUNT * ISLE_MAX_PER];

// Stable position hash: the k-th sprite of upgrade i always lands on the same spot.
static uint32_t isle_hash(uint32_t a, uint32_t b) {
    uint32_t h = (a + 1) * 0x9E3779B1u ^ (b + 1) * 0x85EBCA6Bu;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

static void draw_isles(Canvas *c, const Game *g) {
    // Ground: bevelled frame with a two-tone grass checkerboard.
    gfx_bevel_panel(c, ISLE_X, ISLE_Y, ISLE_W, ISLE_H, RGB8(52, 130, 52), RGB8(110, 190, 110), RGB8(20, 60, 20));
    for (int ty = 0; ty < (ISLE_H - 2 + 15) / 16; ty++)
        for (int tx = 0; tx < (ISLE_W - 2 + 15) / 16; tx++)
            if ((tx + ty) & 1) {
                int w = ISLE_W - 2 - tx * 16; if (w > 16) w = 16;
                int h = ISLE_H - 2 - ty * 16; if (h > 16) h = 16;
                gfx_fill(c, ISLE_X + 1 + tx * 16, ISLE_Y + 1 + ty * 16, w, h, RGB8(58, 142, 58));
            }

    // Collect one sprite per ISLE_PER purchases of every upgrade.
    int n = 0;
    for (int i = 0; i < UPGRADE_COUNT; i++) {
        int k = g->s.up[i].count / ISLE_PER;
        if (k > ISLE_MAX_PER) k = ISLE_MAX_PER;
        for (int j = 0; j < k; j++) {
            uint32_t h = isle_hash((uint32_t)i, (uint32_t)j);
            isleSprites[n].x = ISLE_X + 4 + (int)((h & 0xFFFF) % (ISLE_W - 8 - 16));
            isleSprites[n].y = ISLE_Y + 4 + (int)((h >> 16) % (ISLE_H - 8 - 16));
            isleSprites[n].icon = (uint8_t)i;
            n++;
        }
    }

    // Depth sort by y (insertion sort) so lower sprites overlap higher ones.
    for (int a = 1; a < n; a++) {
        IsleSprite t = isleSprites[a];
        int b = a - 1;
        while (b >= 0 && isleSprites[b].y > t.y) { isleSprites[b + 1] = isleSprites[b]; b--; }
        isleSprites[b + 1] = t;
    }

    for (int a = 0; a < n; a++) {
        gfx_fill(c, isleSprites[a].x + 3, isleSprites[a].y + 14, 10, 2, RGB8(30, 90, 30));
        gfx_blit16(c, icon_rgb[isleSprites[a].icon], isleSprites[a].x, isleSprites[a].y, 0);
    }

    if (n == 0) {
        gfx_text_centered(c, &font10, 128, 98, "THE ISLES ARE EMPTY", C_WHITE, 1);
        gfx_text_centered(c, &font10, 128, 110, "BUY 5 OF AN UPGRADE TO", C_DIM, 1);
        gfx_text_centered(c, &font10, 128, 120, "SEE IT APPEAR HERE", C_DIM, 1);
    }
}

void ui_draw_top(px *fb, const px *bg, const UiView *v) {
    Canvas c;
    memcpy(fb, bg, SCREEN_W * SCREEN_H * sizeof(px));
    gfx_begin(&c, fb);
    const Game *g = v->game;
    char a[96], b[96];

    // Header (shared by every page)
    panel(&c, 0, 0, SCREEN_W, 18);
    gfx_text(&c, &font10, 6, 5, v->page == PAGE_ISLES ? "JERRY ISLES" : "UPGRADES", C_GOLD, 1);
    format_number(g->s.candies, a, sizeof a);
    int tw = gfx_text_width(&font10, a);
    gfx_text_right(&c, &font10, 250, 5, a, C_WHITE, 1);
    gfx_blit16(&c, candy_rgb, 250 - tw - 18, 1, 0);

    if (v->page == PAGE_ISLES) {
        draw_isles(&c, g);
        return;
    }

    // List
    for (int k = 0; k < UI_VISIBLE; k++) {
        int i = v->scroll + k;
        if (i >= UPGRADE_COUNT) break;
        int y = LIST_Y + k * ROW_PITCH;
        bool maxed = game_is_maxed(g, i);
        bool afford = game_can_buy(g, i);

        px fill, light, dark, nameCol, costCol;
        if (maxed) {
            fill = RGB8(0, 51, 0); light = RGB8(0, 100, 0); dark = RGB8(0, 20, 0);
            nameCol = C_WHITE; costCol = C_GREEN;
        } else if (afford) {
            fill = RGB8(48, 48, 48); light = RGB8(110, 110, 110); dark = RGB8(16, 16, 16);
            nameCol = C_WHITE; costCol = C_GREEN;
        } else {
            fill = RGB8(70, 10, 10); light = RGB8(122, 0, 0); dark = RGB8(28, 0, 0);
            nameCol = C_DIM; costCol = C_RED;
        }
        gfx_bevel_panel(&c, ROW_X, y, ROW_W, ROW_H, fill, light, dark);

        // Icon slot
        gfx_bevel_panel(&c, ROW_X + 3, y + 2, 18, 18, RGB8(40, 40, 40), RGB8(16, 16, 16), RGB8(100, 100, 100));
        gfx_blit16(&c, icon_rgb[i], ROW_X + 4, y + 3, 0);
        if (!afford && !maxed) gfx_blit16(&c, tex_locked, ROW_X + 4, y + 3, 1);

        gfx_text(&c, &font10, ROW_X + 26, y + 3, UPGRADES[i].name, nameCol, 1);
        if (maxed) {
            snprintf(a, sizeof a, "OWNED");
        } else {
            format_number(g->s.up[i].cost, b, sizeof b);
            snprintf(a, sizeof a, "COST %s", b);
        }
        gfx_text(&c, &font10, ROW_X + 26, y + 13, a, costCol, 0);
        if (g->s.up[i].count > 0 && !maxed) {
            snprintf(a, sizeof a, "x%d", (int)g->s.up[i].count);
            gfx_text_right(&c, &font10, ROW_X + ROW_W - 5, y + 3, a, C_GOLD, 1);
        }

        if (i == v->sel) {
            gfx_frame(&c, ROW_X, y, ROW_W, ROW_H, C_GOLD);
            gfx_frame(&c, ROW_X + 1, y + 1, ROW_W - 2, ROW_H - 2, RGB8(200, 130, 0));
        }
    }

    // Scrollbar
    int trackH = UI_VISIBLE * ROW_PITCH - 1;
    gfx_fill(&c, 249, LIST_Y, 4, trackH, RGB8(20, 20, 20));
    int thumbH = trackH * UI_VISIBLE / UPGRADE_COUNT;
    int maxScroll = UPGRADE_COUNT - UI_VISIBLE;
    int thumbY = LIST_Y + (maxScroll > 0 ? (trackH - thumbH) * v->scroll / maxScroll : 0);
    gfx_fill(&c, 249, thumbY, 4, thumbH, RGB8(170, 170, 170));

    // Footer
    panel(&c, 3, 160, 250, 30);
    gfx_text(&c, &font10, 8, 164, UPGRADES[v->sel].effect, C_WHITE, 1);
    gfx_text(&c, &font10, 8, 177, "A BUY  B TAP  L/R PAGE  START PRESTIGE", C_DIM, 0);
}

// pressed: draw the _active texture and nudge the label down 1px
static void button(Canvas *c, const uint16_t *tex, const uint16_t *texActive, int x, const char *label, px color, bool pressed) {
    gfx_nineslice(c, pressed ? texActive : tex, x, UI_BTN_Y, UI_BTN_W, UI_BTN_H);
    gfx_text_centered(c, &font10, x + UI_BTN_W / 2, UI_BTN_Y + 8 + (pressed ? 1 : 0), label, color, 1);
}

void ui_draw_bottom(px *fb, const px *bg, const UiView *v) {
    Canvas c;
    memcpy(fb, bg, SCREEN_W * SCREEN_H * sizeof(px));
    gfx_begin(&c, fb);
    const Game *g = v->game;
    char a[96], b[96];

    // Stats
    panel(&c, 4, 3, 248, 34);
    gfx_text(&c, &font10, 10, 7, "JERRY CANDIES", C_DIM, 0);
    format_number(g->s.candies, a, sizeof a);
    gfx_text(&c, &font14, 10, 18, a, C_GOLD, 1);

    panel(&c, 4, 40, 122, 24);
    gfx_text(&c, &font10, 10, 43, "PER CLICK", C_DIM, 0);
    format_number(game_per_click(g), a, sizeof a);
    gfx_text(&c, &font10, 10, 53, a, C_WHITE, 1);

    panel(&c, 130, 40, 122, 24);
    gfx_text(&c, &font10, 136, 43, "PER SECOND", C_DIM, 0);
    format_number(game_auto_rate(g), b, sizeof b);
    gfx_text(&c, &font10, 136, 53, b, g->frenzyFrames > 0 ? C_GOLD : C_WHITE, 1);

    // Jerry stage (Jerry himself is a hardware sprite)
    gfx_bevel_panel(&c, 4, 67, 248, 68, RGB8(26, 26, 26), RGB8(0, 0, 0), RGB8(85, 85, 85));

    if (game_prestige_unlocked(g) || g->s.prestigeLevel > 0) {
        snprintf(a, sizeof a, "PRESTIGE X%.1f (LV %d)", g->s.prestigeMult, (int)g->s.prestigeLevel);
        gfx_text(&c, &font10, 6, 138, a, C_PURPLE, 1);
    }
    if (g->frenzyFrames > 0) {
        snprintf(a, sizeof a, "FRENZY! 2X  %ds", g->frenzyFrames / 60 + 1);
        gfx_text_right(&c, &font10, 250, 138, a, C_GOLD, 1);
    }

    // Messages
    if (v->confirmPrestige || v->msgTimer > 0) {
        px col = v->confirmPrestige ? C_GOLD : C_WHITE;
        gfx_text_centered(&c, &font10, 128, 147, v->msg0 ? v->msg0 : "", col, 1);
        gfx_text_centered(&c, &font10, 128, 155, v->msg1 ? v->msg1 : "", col, 1);
    } else if (v->saveStatus && v->saveStatus[0]) {
        gfx_text_centered(&c, &font10, 128, 155, v->saveStatus, C_DIM, 0);
    }

    // Buttons
    if (v->confirmPrestige) {
        button(&c, tex_ok, tex_ok_active, UI_BTN_L_X, "YES", C_WHITE, v->pressedBtn == 1);
        button(&c, tex_cancel, tex_cancel_active, UI_BTN_R_X, "NO", C_WHITE, v->pressedBtn == 2);
    } else {
        button(&c, tex_button, tex_button_active, UI_BTN_L_X, v->fatOk ? "SAVE" : "NO SD", v->fatOk ? C_WHITE : C_DIM, v->pressedBtn == 1);
        button(&c, tex_prestige, tex_prestige_active, UI_BTN_R_X, "PRESTIGE", game_can_prestige(g) ? C_WHITE : RGB8(190, 170, 220), v->pressedBtn == 2);
    }
}
