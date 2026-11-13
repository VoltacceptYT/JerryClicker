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

void ui_draw_top(px *fb, const px *bg, const UiView *v) {
    Canvas c;
    memcpy(fb, bg, SCREEN_W * SCREEN_H * sizeof(px));
    gfx_begin(&c, fb);
    const Game *g = v->game;
    char a[96], b[96];

    // Header
    panel(&c, 0, 0, SCREEN_W, 18);
    gfx_text(&c, &font10, 6, 5, "UPGRADES", C_GOLD, 1);
    format_number(g->s.candies, a, sizeof a);
    int tw = gfx_text_width(&font10, a);
    gfx_text_right(&c, &font10, 250, 5, a, C_WHITE, 1);
    gfx_blit16(&c, candy_rgb, 250 - tw - 18, 1, 0);

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

static void button(Canvas *c, const uint16_t *tex, int x, const char *label, px color) {
    gfx_nineslice(c, tex, x, UI_BTN_Y, UI_BTN_W, UI_BTN_H);
    gfx_text_centered(c, &font10, x + UI_BTN_W / 2, UI_BTN_Y + 8, label, color, 1);
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
        button(&c, tex_ok, UI_BTN_L_X, "YES", C_WHITE);
        button(&c, tex_cancel, UI_BTN_R_X, "NO", C_WHITE);
    } else {
        button(&c, tex_button, UI_BTN_L_X, v->fatOk ? "SAVE" : "NO SD", v->fatOk ? C_WHITE : C_DIM);
        button(&c, tex_prestige, UI_BTN_R_X, "PRESTIGE", game_can_prestige(g) ? C_WHITE : RGB8(190, 170, 220));
    }
}
