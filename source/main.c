// Jerry Clicker for Nintendo DS
//  Bottom screen (touch): stats, Jerry, buttons.   Top screen: upgrade list.
//  Controls: Touch Jerry or B = click | Up/Down = select | L/R = page | A = buy
//            Start = prestige | Select = music on/off | Touch SAVE / PRESTIGE buttons
//  Both screens are software-rendered (see gfx.c / ui.c) using the original font and sprites.
#include <fat.h>
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "audio.h"
#include "game.h"
#include "gfx.h"
#include "sprites.h"
#include "ui.h"

#define SAVE_PATH "JerryClicker.sav"
#define AUTOSAVE_FRAMES (30 * 60)
#define FB_BYTES (SCREEN_W * SCREEN_H * 2)

static px fbTop[SCREEN_W * SCREEN_H] __attribute__((aligned(32)));
static px fbBot[SCREEN_W * SCREEN_H] __attribute__((aligned(32)));
static px bgTopBuf[SCREEN_W * SCREEN_H] __attribute__((aligned(32)));
static px bgBotBuf[SCREEN_W * SCREEN_H] __attribute__((aligned(32)));
static u16 *vramTop, *vramBot;

static u16 *jerryGfx[JERRY_COUNT];
static u16 *goldenGfx, *candyGfx;

static Game game;
static bool fatOk = false;
static int sel = 0, scroll = 0;
static u32 frame = 0;

// Messages (bottom screen)
static char msg[2][64];
static int msgTimer = 0;
static char saveStatus[32] = "";
static bool confirmPrestige = false;

// Jerry
static int pressFrames = 0;
static int pressedBtn = 0; // bottom-screen button currently held by the stylus (0 none, 1 left, 2 right)

// Floating candies
#define FLOATERS 4
static struct { bool on; int x, y, life; } floaters[FLOATERS];

// Golden Jerry
static bool goldenOn = false;
static int goldenX = 0, goldenY = 0, goldenLife = 0, goldenSpawnIn = 0;

static void upload(const void *src, void *dst, u32 n) {
    DC_FlushRange(src, n);
    dmaCopy(src, dst, n);
}

static void set_msg(const char *a, const char *b, int frames) {
    snprintf(msg[0], sizeof msg[0], "%s", a);
    snprintf(msg[1], sizeof msg[1], "%s", b);
    msgTimer = frames;
}

/* ---------------- Saving ---------------- */

static bool save_game(void) {
    if (!fatOk) {
        snprintf(saveStatus, sizeof saveStatus, "No SD: saving off");
        return false;
    }
    uint8_t buf[512];
    size_t n = game_save_size();
    if (n > sizeof buf) return false;
    game_pack(&game, buf);
    FILE *f = fopen(SAVE_PATH, "wb");
    if (!f) {
        snprintf(saveStatus, sizeof saveStatus, "Save failed");
        return false;
    }
    size_t w = fwrite(buf, 1, n, f);
    fclose(f);
    snprintf(saveStatus, sizeof saveStatus, w == n ? "Saved" : "Save failed");
    return w == n;
}

static void load_game(void) {
    if (!fatOk) {
        snprintf(saveStatus, sizeof saveStatus, "No SD: saving off");
        return;
    }
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f) {
        snprintf(saveStatus, sizeof saveStatus, "New game");
        return;
    }
    uint8_t buf[512];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    Game loaded;
    if (game_unpack(&loaded, buf, n)) {
        game = loaded;
        snprintf(saveStatus, sizeof saveStatus, "Save loaded");
    } else {
        snprintf(saveStatus, sizeof saveStatus, "Bad save ignored");
    }
}

/* ---------------- Setup ---------------- */

static void setup(void) {
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE);

    int bgT = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int bgB = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bgSetPriority(bgT, 3);
    bgSetPriority(bgB, 3);
    vramTop = bgGetGfxPtr(bgT);
    vramBot = bgGetGfxPtr(bgB);

    oamInit(&oamSub, SpriteMapping_1D_128, false);
    upload(bottom_pal, SPRITE_PALETTE_SUB, 512);

    for (int i = 0; i < JERRY_COUNT; i++) {
        jerryGfx[i] = oamAllocateGfx(&oamSub, SpriteSize_64x64, SpriteColorFormat_256Color);
        if (jerryGfx[i]) upload(jerry_gfx[i], jerryGfx[i], 4096);
    }
    goldenGfx = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_256Color);
    if (goldenGfx) upload(golden_gfx, goldenGfx, 1024);
    candyGfx = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_256Color);
    if (candyGfx) upload(candy_gfx, candyGfx, 256);

    ui_draw_backgrounds(bgTopBuf, bgBotBuf);
    keysSetRepeat(15, 4);
}

/* ---------------- Rendering ---------------- */

static void make_view(UiView *v) {
    v->game = &game;
    v->sel = sel;
    v->scroll = scroll;
    v->confirmPrestige = confirmPrestige;
    v->msg0 = msg[0];
    v->msg1 = msg[1];
    v->msgTimer = msgTimer;
    v->saveStatus = saveStatus;
    v->fatOk = fatOk;
    v->pressedBtn = pressedBtn;
}

static void hide_sub(int id, SpriteSize size, u16 *gfx) {
    oamSet(&oamSub, id, 0, 0, 0, 0, size, SpriteColorFormat_256Color, gfx, -1, false, true, false, false, false);
}

static int jerry_variant(int *wobble) {
    *wobble = 0;
    int c = game_active_cosmetic(&game);
    if (c < 0) return 0;
    const char *id = UPGRADES[c].id;
    if (strcmp(id, "pet_item_lord_jerry") == 0) return 1;
    if (strcmp(id, "pet_item_scuba_jerry") == 0) return 2;
    if (strcmp(id, "pet_item_iron_jerry") == 0) return 3;
    if (strcmp(id, "pet_item_angel_jerry") == 0) return 4;
    // 3D glasses: base Jerry with a trippy side-to-side shimmy
    *wobble = ((frame >> 3) & 1) ? 3 : -3;
    return 0;
}

static void update_sprites(void) {
    int wobble;
    int v = jerry_variant(&wobble);
    int dy = pressFrames > 0 ? 3 : 0;
    oamSet(&oamSub, 5, UI_JERRY_X + wobble, UI_JERRY_Y + dy, 0, 0, SpriteSize_64x64,
           SpriteColorFormat_256Color, jerryGfx[v], -1, false, false, false, false, false);

    for (int i = 0; i < FLOATERS; i++) {
        if (floaters[i].on) {
            oamSet(&oamSub, i, floaters[i].x, floaters[i].y, 0, 0, SpriteSize_16x16,
                   SpriteColorFormat_256Color, candyGfx, -1, false, false, false, false, false);
        } else {
            hide_sub(i, SpriteSize_16x16, candyGfx);
        }
    }

    if (goldenOn) {
        static const int bob[4] = {0, -2, -4, -2};
        oamSet(&oamSub, 4, goldenX, goldenY + bob[(frame >> 3) & 3], 0, 0, SpriteSize_32x32,
               SpriteColorFormat_256Color, goldenGfx, -1, false, false, false, false, false);
    } else {
        hide_sub(4, SpriteSize_32x32, goldenGfx);
    }
}

static void spawn_floater(int x, int y) {
    for (int i = 0; i < FLOATERS; i++) {
        if (!floaters[i].on) {
            floaters[i].on = true;
            floaters[i].x = x - 8;
            floaters[i].y = y - 8;
            floaters[i].life = 40;
            return;
        }
    }
}

/* ---------------- Game actions ---------------- */

static void schedule_golden(void) { goldenSpawnIn = 10800 + rand() % 7200; } // 3-5 minutes

static void do_click(int fx, int fy) {
    game_click(&game);
    pressFrames = 4;
    spawn_floater(fx, fy);
}

static void claim_golden(void) {
    double roll = (double)(rand() % 10000) / 10000.0;
    GoldenReward r = game_claim_golden(&game, roll);
    char a[48], b[64], c[48];
    switch (r.kind) {
    case GR_WINDFALL:
        format_number(r.amount, c, sizeof c);
        snprintf(a, sizeof a, "Candy Windfall!");
        snprintf(b, sizeof b, "+%s candies", c);
        break;
    case GR_FRENZY:
        snprintf(a, sizeof a, "Frenzy!");
        if (r.amount > 0) {
            format_number(r.amount, c, sizeof c);
            snprintf(b, sizeof b, "+%s candies", c);
        } else {
            snprintf(b, sizeof b, "2x production for 30s");
        }
        break;
    default:
        format_number(r.amount, c, sizeof c);
        snprintf(a, sizeof a, "Lucky!");
        snprintf(b, sizeof b, "+%s candies", c);
        break;
    }
    set_msg(a, b, 150);
    goldenOn = false;
    schedule_golden();
}

static void request_prestige(void) {
    char a[48], b[64];
    if (!game_prestige_unlocked(&game)) {
        set_msg("Prestige is locked.", "Buy Aspect of the Jerry!", 150);
        return;
    }
    if (!game_can_prestige(&game)) {
        format_number(game_prestige_requirement(&game), a, sizeof a);
        snprintf(b, sizeof b, "%s candies", a);
        set_msg("Prestige needs at least", b, 200);
        return;
    }
    snprintf(a, sizeof a, "Reset progress for a %.1fx", game_prestige_next_mult(&game));
    set_msg(a, "multiplier?  (A=Yes B=No)", 0);
    confirmPrestige = true;
}

static void clamp_scroll(void) {
    if (sel < 0) sel = 0;
    if (sel >= UPGRADE_COUNT) sel = UPGRADE_COUNT - 1;
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + UI_VISIBLE) scroll = sel - UI_VISIBLE + 1;
}

/* ---------------- Main loop ---------------- */

int main(void) {
    srand((unsigned)time(NULL));
    game_init(&game);
    setup();

    fatOk = fatInitDefault();
    load_game();
    schedule_golden();
    audio_init(); // looping background music (plays on the ARM7 sound hardware)

    u32 sinceSave = 0;
    bool dirtyTop = true, dirtyBot = true;
    bool readyTop = false, readyBot = false; // framebuffer drawn, waiting to be copied to VRAM
    bool copyBotNext = false;

    while (1) {
        swiWaitForVBlank();

        // Copy at most one framebuffer per vblank (a full copy takes a good part of it)
        if (readyBot && (!readyTop || copyBotNext)) {
            DC_FlushRange(fbBot, FB_BYTES);
            dmaCopy(fbBot, vramBot, FB_BYTES);
            readyBot = false;
            copyBotNext = false;
        } else if (readyTop) {
            DC_FlushRange(fbTop, FB_BYTES);
            dmaCopy(fbTop, vramTop, FB_BYTES);
            readyTop = false;
            copyBotNext = true;
        }
        oamUpdate(&oamSub);

        scanKeys();
        u32 down = keysDown();
        u32 rep = keysDownRepeat();
        touchPosition t;
        memset(&t, 0, sizeof t);
        bool touched = (down & KEY_TOUCH) != 0;
        if (touched) touchRead(&t);

        // Which button (if any) is the stylus currently holding down?
        int heldBtn = 0;
        if (keysHeld() & KEY_TOUCH) {
            touchPosition h;
            touchRead(&h);
            if (h.py >= UI_BTN_Y && h.py < UI_BTN_Y + UI_BTN_H) heldBtn = h.px < 128 ? 1 : 2;
        }
        if (heldBtn != pressedBtn) {
            pressedBtn = heldBtn;
            dirtyBot = true;
        }

        if (confirmPrestige) {
            bool yes = (down & KEY_A) || (touched && t.py >= UI_BTN_Y && t.px < 128);
            bool no = (down & KEY_B) || (touched && t.py >= UI_BTN_Y && t.px >= 128);
            if (yes) {
                game_do_prestige(&game);
                confirmPrestige = false;
                set_msg("Prestiged!", "Your progress was reset.", 150);
                save_game();
                dirtyTop = dirtyBot = true;
            } else if (no) {
                confirmPrestige = false;
                msgTimer = 0;
                dirtyBot = true;
            }
        } else {
            if (rep & KEY_UP) { sel--; dirtyTop = true; }
            if (rep & KEY_DOWN) { sel++; dirtyTop = true; }
            if (down & KEY_L) { sel -= UI_VISIBLE; dirtyTop = true; }
            if (down & KEY_R) { sel += UI_VISIBLE; dirtyTop = true; }
            clamp_scroll();
            if (down & KEY_A) {
                game_buy(&game, sel);
                dirtyTop = dirtyBot = true;
            }
            if (down & KEY_B) {
                do_click(UI_JERRY_X + 32, UI_JERRY_Y + 16);
                dirtyBot = true;
            }
            if (down & KEY_START) {
                request_prestige();
                dirtyBot = true;
            }
            if (down & KEY_SELECT) {
                if (!audio_music_playing()) {
                    set_msg("Music unavailable", "No free sound channel", 90);
                } else if (audio_music_toggle()) {
                    set_msg("Music: ON", "Select = mute", 90);
                } else {
                    set_msg("Music: OFF", "Select = unmute", 90);
                }
                dirtyBot = true;
            }
            if (touched) {
                if (goldenOn && t.px >= goldenX - 4 && t.px < goldenX + 36 &&
                    t.py >= goldenY - 8 && t.py < goldenY + 36) {
                    claim_golden();
                } else if (t.py >= UI_BTN_Y && t.py < UI_BTN_Y + UI_BTN_H) {
                    if (t.px < 128) save_game();
                    else request_prestige();
                } else if (t.px >= UI_JERRY_X - 12 && t.px < UI_JERRY_X + 76 &&
                           t.py >= UI_JERRY_Y - 8 && t.py < UI_JERRY_Y + 72) {
                    do_click(t.px, t.py);
                }
                dirtyTop = dirtyBot = true;
            }
        }

        // Simulation
        game_tick(&game);

        if (pressFrames > 0) pressFrames--;
        if (msgTimer > 0 && --msgTimer == 0) dirtyBot = true;
        for (int i = 0; i < FLOATERS; i++) {
            if (floaters[i].on) {
                floaters[i].y -= 1;
                if (--floaters[i].life <= 0) floaters[i].on = false;
            }
        }
        if (goldenOn) {
            if (--goldenLife <= 0) {
                goldenOn = false;
                schedule_golden();
            }
        } else if (--goldenSpawnIn <= 0) {
            goldenOn = true;
            goldenLife = 13 * 60;
            goldenX = 8 + rand() % (256 - 48);
            goldenY = 24 + rand() % 100;
        }

        if (fatOk && ++sinceSave >= AUTOSAVE_FRAMES) {
            sinceSave = 0;
            save_game();
            dirtyBot = true;
        }

        frame++;
        UiView view;
        make_view(&view);
        if ((dirtyTop || frame % 10 == 0) && !readyTop) {
            ui_draw_top(fbTop, bgTopBuf, &view);
            readyTop = true;
            dirtyTop = false;
        }
        if ((dirtyBot || frame % 6 == 0) && !readyBot) {
            ui_draw_bottom(fbBot, bgBotBuf, &view);
            readyBot = true;
            dirtyBot = false;
        }
        update_sprites();
    }
    return 0;
}
