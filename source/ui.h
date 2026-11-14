// Screen layouts (portable; rendered with gfx.c).
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "game.h"
#include "gfx.h"

#define UI_VISIBLE 6

// Bottom-screen layout (shared with touch handling in main.c)
#define UI_JERRY_X 96
#define UI_JERRY_Y 69
#define UI_BTN_Y 165
#define UI_BTN_H 24
#define UI_BTN_L_X 4
#define UI_BTN_R_X 136
#define UI_BTN_W 116

typedef struct {
    const Game *game;
    int sel, scroll;
    bool confirmPrestige;
    const char *msg0, *msg1; // shown while msgTimer > 0 or confirming
    int msgTimer;
    const char *saveStatus;
    bool fatOk;
    int pressedBtn; // 0 = none, 1 = left button held, 2 = right button held
} UiView;

void ui_draw_backgrounds(px *topBg, px *botBg); // static stone textures
void ui_draw_top(px *fb, const px *bg, const UiView *v);
void ui_draw_bottom(px *fb, const px *bg, const UiView *v);

#endif
