#include "ui.h"
#include "isles.h"

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
    const px *base = (v->page == PAGE_ISLES && isles_bg()) ? isles_bg() : bg;
    memcpy(fb, base, SCREEN_W * SCREEN_H * sizeof(px));
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
        isles_draw(&c);
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

static void blit16_x2(Canvas *c, const uint16_t *tex, int x, int y) {
    for (int yy = 0; yy < 16; yy++)
        for (int xx = 0; xx < 16; xx++) {
            uint16_t v = tex[yy * 16 + xx];
            if (v & 0x8000) gfx_fill(c, x + xx * 2, y + yy * 2, 2, 2, v);
        }
}

static void blit_rect(Canvas *c, const uint16_t *tex, int w, int h, int x, int y) {
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < c->cy0 || py >= c->cy1) continue;
        for (int xx = 0; xx < w; xx++) {
            int pxx = x + xx;
            if (pxx < c->cx0 || pxx >= c->cx1) continue;
            uint16_t v = tex[yy * w + xx];
            if (v & 0x8000) c->buf[py * SCREEN_W + pxx] = v;
        }
    }
}

#define C_WOOD_FILL RGB8(110, 72, 44)
#define C_WOOD_LIGHT RGB8(175, 126, 87)
#define C_WOOD_DARK RGB8(60, 52, 48)

static void wood_panel(Canvas *c, int x, int y, int w, int h) {
    gfx_bevel_panel(c, x, y, w, h, C_WOOD_FILL, C_WOOD_LIGHT, C_WOOD_DARK);
}

static const char *toy_label(int t) {  // short names that fit a shop window
    static const char *const L[TOY_COUNT] = {"Beach Ball", "Shovel", "Mouse", "8-Ball", "Balloon", "Dice"};
    return L[t];
}

static int toy_x(int t) { return UI_TOY_X0 + (t % UI_TOY_COLS) * UI_TOY_PITCH_X; }
static int toy_y(int t) { return UI_TOY_Y0 + (t / UI_TOY_COLS) * UI_TOY_PITCH_Y; }

int ui_toy_at(int x, int y) {
    for (int t = 0; t < TOY_COUNT; t++)
        if (x >= toy_x(t) && x < toy_x(t) + UI_TOY_W && y >= toy_y(t) && y < toy_y(t) + UI_TOY_H) return t;
    return -1;
}

static void toy_window(Canvas *c, const Game *g, int t, bool selected) {
    char a[48];
    bool maxed = game_toy_maxed(g, t);
    bool afford = game_can_buy_toy(g, t);
    int x = toy_x(t), y = toy_y(t);
    bool unlocked = game_toy_unlocked(g, t);
    // Wooden frame, then the glass stretched (not tiled) over the 3px-framed interior
    gfx_nineslice(c, tex_shop_window, x, y, UI_TOY_W, UI_TOY_H);
    gfx_stretch(c, unlocked ? tex_shop_glass : tex_shop_glass_dark, SHOP_GLASS_W, SHOP_GLASS_H,
                x + 3, y + 3, UI_TOY_W - 6, UI_TOY_H - 6);

    if (!unlocked) {
        // Hidden behind the dark window: just a padlock and what it takes to open it
        gfx_blit16(c, tex_shop_lock, x + UI_TOY_W / 2 - 8, y + 10, 0);
        snprintf(a, sizeof a, "PRESTIGE %d", game_toy_unlock_level(t));
        gfx_text_centered(c, &font10, x + UI_TOY_W / 2, y + 31, a, C_DIM, 1);
        if (selected) {
            gfx_frame(c, x - 1, y - 1, UI_TOY_W + 2, UI_TOY_H + 2, C_GOLD);
            gfx_frame(c, x - 2, y - 2, UI_TOY_W + 4, UI_TOY_H + 4, RGB8(200, 130, 0));
        }
        return;
    }

    gfx_text_centered(c, &font10, x + UI_TOY_W / 2, y + 4, toy_label(t), C_WHITE, 1);

    blit16_x2(c, toy_rgb[t], x + 6, y + 15);
    if (!afford && !maxed) gfx_blit16(c, tex_locked, x + 14, y + 23, 1);

    snprintf(a, sizeof a, "%d/%d", (int)g->s.toys[t], TOY_MAX);
    gfx_text_centered(c, &font10, x + 58, y + 16, a, maxed ? C_GREEN : C_WHITE, 1);

    int tx = x + 41, ty = y + 28, tw = 35, th = 17;
    wood_panel(c, tx, ty, tw, th);
    if (maxed) {
        gfx_text_centered(c, &font10, tx + tw / 2, ty + 4, "MAX", C_GREEN, 1);
    } else {
        double cost = game_toy_cost(g, t);
        if (cost < 1000) snprintf(a, sizeof a, "%d", (int)cost);
        else format_number(cost, a, sizeof a);
        gfx_text_centered(c, &font10, tx + tw / 2, ty + 4, a, afford ? C_GREEN : RGB8(255, 120, 120), 1);
    }

    if (selected) {
        gfx_frame(c, x - 1, y - 1, UI_TOY_W + 2, UI_TOY_H + 2, C_GOLD);
        gfx_frame(c, x - 2, y - 2, UI_TOY_W + 4, UI_TOY_H + 4, RGB8(200, 130, 0));
    }
}

// The whole bottom screen: shop wall, awning, sign with the candy count, the toy windows and a counter.
static void toy_store_screen(Canvas *c, const Game *g, const UiView *v) {
    char a[96];
    gfx_tile(c, tex_shop_wall, 0, 0, SCREEN_W, 162);
    gfx_clip(c, 0, 0, SCREEN_W, 13);
    for (int x = 0; x < SCREEN_W; x += 12) blit_rect(c, shop_awning, 12, 13, x, 0);
    gfx_unclip(c);

    // Sign: shop name on the left, candy balance on the right
    wood_panel(c, 4, 16, 72, 15);
    gfx_text_centered(c, &font10, 40, 20, "JERRY TOYS", C_GOLD, 1);
    format_number(g->s.candies, a, sizeof a);
    int tw = gfx_text_width(&font10, a);
    gfx_text_right(c, &font10, 250, 20, a, C_WHITE, 1);
    gfx_blit16(c, candy_rgb, 250 - tw - 18, 16, 0);

    for (int t = 0; t < TOY_COUNT; t++) toy_window(c, g, t, t == v->toySel);

    // Counter along the bottom (the buttons are drawn on it)
    wood_panel(c, 0, 160, SCREEN_W, 32);

    if (v->confirmPrestige || v->msgTimer > 0) {
        px col = v->confirmPrestige ? C_GOLD : C_WHITE;
        gfx_text_centered(c, &font10, 128, 143, v->msg0 ? v->msg0 : "", col, 1);
        gfx_text_centered(c, &font10, 128, 151, v->msg1 ? v->msg1 : "", col, 1);
        return;
    }
    if (game_prestige_unlocked(g) || g->s.prestigeLevel > 0) {
        snprintf(a, sizeof a, "PRESTIGE X%.1f (LV %d)", g->s.prestigeMult, (int)g->s.prestigeLevel);
        gfx_text(c, &font10, 6, 142, a, C_PURPLE, 1);
    }
    if (g->frenzyFrames > 0) {
        snprintf(a, sizeof a, "FRENZY! 2X  %ds", g->frenzyFrames / 60 + 1);
        gfx_text_right(c, &font10, 250, 142, a, C_GOLD, 1);
    }
    if (game_toy_unlocked(g, v->toySel)) {
        gfx_text_centered(c, &font10, 128, 152, TOYS[v->toySel].effect, C_WHITE, 1);
    } else {
        snprintf(a, sizeof a, "Unlocks after %d prestige%s", game_toy_unlock_level(v->toySel),
                 game_toy_unlock_level(v->toySel) == 1 ? "" : "s");
        gfx_text_centered(c, &font10, 128, 152, a, C_WHITE, 1);
    }
    if (v->saveStatus && v->saveStatus[0]) gfx_text_right(c, &font10, 252, 152, v->saveStatus, C_DIM, 0);
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

    if (v->page == PAGE_ISLES) {
        toy_store_screen(&c, g, v);
        goto buttons;
    }

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

buttons:
    // Buttons
    if (v->confirmPrestige) {
        button(&c, tex_ok, tex_ok_active, UI_BTN_L_X, "YES", C_WHITE, v->pressedBtn == 1);
        button(&c, tex_cancel, tex_cancel_active, UI_BTN_R_X, "NO", C_WHITE, v->pressedBtn == 2);
    } else {
        button(&c, tex_button, tex_button_active, UI_BTN_L_X, v->fatOk ? "SAVE" : "NO SD", v->fatOk ? C_WHITE : C_DIM, v->pressedBtn == 1);
        button(&c, tex_prestige, tex_prestige_active, UI_BTN_R_X, "PRESTIGE", game_can_prestige(g) ? C_WHITE : RGB8(190, 170, 220), v->pressedBtn == 2);
    }
}
