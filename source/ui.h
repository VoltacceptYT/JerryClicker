// Screen layouts (portable; rendered with gfx.c).
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "game.h"
#include "gfx.h"

#define UI_VISIBLE 6

// Top-screen pages (L/R cycles through them)
#define PAGE_UPGRADES 0
#define PAGE_ISLES 1
#define PAGE_COUNT 2

// Bottom-screen layout (shared with touch handling in main.c)
#define UI_JERRY_X 96
#define UI_JERRY_Y 69
#define UI_BTN_Y 165
#define UI_BTN_H 24
#define UI_BTN_L_X 4
#define UI_BTN_R_X 136
#define UI_BTN_W 116

// Jerry Isles bottom screen: the whole screen is the toy store (a 3x2 grid of shop windows, one per toy;
// the SAVE / PRESTIGE buttons sit on the shop counter at the bottom).
#define UI_TOY_COLS 3
#define UI_TOY_W 80
#define UI_TOY_H 50
#define UI_TOY_X0 4
#define UI_TOY_PITCH_X 84
#define UI_TOY_Y0 36
#define UI_TOY_PITCH_Y 54
#define UI_TOY_ROWS ((TOY_COUNT + UI_TOY_COLS - 1) / UI_TOY_COLS)

typedef struct {
    const Game *game;
    int page;      // PAGE_UPGRADES or PAGE_ISLES
    int sel, scroll;
    bool confirmPrestige;
    const char *msg0, *msg1; // shown while msgTimer > 0 or confirming
    int msgTimer;
    const char *saveStatus;
    bool fatOk;
    int toySel;    // toy card highlighted on the Isles page
    int pressedBtn; // 0 = none, 1 = left button held, 2 = right button held
} UiView;

void ui_draw_backgrounds(px *topBg, px *botBg); // static stone textures
void ui_draw_top(px *fb, const px *bg, const UiView *v);
void ui_draw_bottom(px *fb, const px *bg, const UiView *v);
int ui_toy_at(int x, int y); // toy under a touch point, or -1

#endif
