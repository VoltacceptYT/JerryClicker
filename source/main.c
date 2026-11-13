// Jerry Clicker for Nintendo DS
//  Bottom screen (touch): stats, Jerry, buttons.   Top screen: upgrade list.
//  Controls: Touch Jerry or B = click | Up/Down = select | L/R = page | A = buy
//            Start = prestige | Touch [SAVE]/[PRESTIGE] buttons
#include <fat.h>
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game.h"
#include "sprites.h"

#define SAVE_PATH "JerryClicker.sav"
#define VISIBLE 9
#define JERRY_X 96
#define JERRY_Y 44
#define AUTOSAVE_FRAMES (30 * 60)

static PrintConsole topCon, botCon;
static u16 *iconGfx[UPGRADE_COUNT];
static u16 *jerryGfx[JERRY_COUNT];
static u16 *goldenGfx, *candyGfx;

static Game game;
static bool fatOk = false;
static int sel = 0, scroll = 0;
static u32 frame = 0;

// Messages (bottom screen)
static char msg[2][40];
static int msgTimer = 0;
static char saveStatus[32] = "";
static bool confirmPrestige = false;

// Jerry
static int pressFrames = 0;

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
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE);

    consoleInit(&topCon, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&botCon, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
    BG_PALETTE[0] = RGB15(2, 2, 6);
    BG_PALETTE_SUB[0] = RGB15(2, 4, 8);

    oamInit(&oamMain, SpriteMapping_1D_128, false);
    oamInit(&oamSub, SpriteMapping_1D_128, false);

    upload(top_pal, SPRITE_PALETTE, 512);
    upload(bottom_pal, SPRITE_PALETTE_SUB, 512);

    for (int i = 0; i < UPGRADE_COUNT; i++) {
        iconGfx[i] = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
        if (iconGfx[i]) upload(icon_gfx[i], iconGfx[i], 256);
    }
    for (int i = 0; i < JERRY_COUNT; i++) {
        jerryGfx[i] = oamAllocateGfx(&oamSub, SpriteSize_64x64, SpriteColorFormat_256Color);
        if (jerryGfx[i]) upload(jerry_gfx[i], jerryGfx[i], 4096);
    }
    goldenGfx = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_256Color);
    if (goldenGfx) upload(golden_gfx, goldenGfx, 1024);
    candyGfx = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_256Color);
    if (candyGfx) upload(candy_gfx, candyGfx, 256);

    keysSetRepeat(15, 4);
}

/* ---------------- Text rendering ---------------- */

#define ROWS 24
#define ROWLEN 128
static char rows[ROWS][ROWLEN];

static void clear_rows(void) { memset(rows, 0, sizeof rows); }

static void flush_rows(PrintConsole *con) {
    consoleSelect(con);
    consoleClear();
    for (int r = 0; r < ROWS; r++) {
        if (r < ROWS - 1) printf("%s\n", rows[r]);
        else printf("%s", rows[r]);
    }
}

#define C_RESET "\x1b[0m"
#define C_YELLOW "\x1b[33;1m"
#define C_GREEN "\x1b[32;1m"
#define C_CYAN "\x1b[36;1m"
#define C_GREY "\x1b[30;1m"
#define C_WHITE "\x1b[37;1m"
#define C_RED "\x1b[31;1m"

static void draw_top(void) {
    clear_rows();
    char num[48], num2[48];

    snprintf(rows[0], ROWLEN, C_YELLOW "  JERRY CLICKER - UPGRADES" C_RESET);
    format_number(game.s.candies, num, sizeof num);
    snprintf(rows[1], ROWLEN, " Candies: " C_WHITE "%.20s" C_RESET, num);

    for (int k = 0; k < VISIBLE; k++) {
        int i = scroll + k;
        if (i >= UPGRADE_COUNT) break;
        int r = 2 + k * 2;
        bool maxed = game_is_maxed(&game, i);
        bool afford = game_can_buy(&game, i);
        const char *col = maxed ? C_CYAN : (afford ? C_GREEN : C_GREY);

        snprintf(rows[r], ROWLEN, "%s%c" C_RESET "  %s%.28s" C_RESET,
                 C_YELLOW, i == sel ? '>' : ' ', col, UPGRADES[i].name);

        format_number(game.s.up[i].cost, num2, sizeof num2);
        char body[64];
        if (maxed) snprintf(body, sizeof body, "MAX (owned)");
        else snprintf(body, sizeof body, "Cost %s x%d", num2, (int)game.s.up[i].count);
        snprintf(rows[r + 1], ROWLEN, "   %s%.28s" C_RESET, col, body);
    }

    // Description of selected upgrade, wrapped to two rows
    const char *e = UPGRADES[sel].effect;
    int len = (int)strlen(e);
    int cut = len;
    if (len > 31) {
        cut = 31;
        while (cut > 0 && e[cut] != ' ') cut--;
        if (cut == 0) cut = 31;
    }
    snprintf(rows[20], ROWLEN, C_WHITE " %.*s" C_RESET, cut, e);
    if (cut < len) snprintf(rows[21], ROWLEN, C_WHITE " %.30s" C_RESET, e + cut + (e[cut] == ' ' ? 1 : 0));
    snprintf(rows[23], ROWLEN, C_GREY "A:Buy B:Tap L/R:Page Start:Prest" C_RESET);

    flush_rows(&topCon);
}

static void draw_bottom(void) {
    clear_rows();
    char a[48];

    format_number(game.s.candies, a, sizeof a);
    snprintf(rows[0], ROWLEN, C_YELLOW "Candies: " C_WHITE "%.20s" C_RESET, a);
    format_number(game_per_click(&game), a, sizeof a);
    snprintf(rows[1], ROWLEN, "Per click: " C_CYAN "%.18s" C_RESET, a);
    format_number(game_auto_rate(&game), a, sizeof a);
    snprintf(rows[2], ROWLEN, "Per sec:   %s%.18s" C_RESET, game.frenzyFrames > 0 ? C_YELLOW : C_CYAN, a);
    if (game_prestige_unlocked(&game) || game.s.prestigeLevel > 0)
        snprintf(rows[3], ROWLEN, "Prestige: x%.1f (level %d)", game.s.prestigeMult, (int)game.s.prestigeLevel);

    if (game.frenzyFrames > 0)
        snprintf(rows[14], ROWLEN, C_YELLOW "FRENZY! 2x production  %ds" C_RESET, game.frenzyFrames / 60 + 1);

    if (confirmPrestige) {
        snprintf(rows[16], ROWLEN, C_YELLOW "%.31s" C_RESET, msg[0]);
        snprintf(rows[17], ROWLEN, C_YELLOW "%.31s" C_RESET, msg[1]);
        snprintf(rows[21], ROWLEN, " " C_GREEN "[  YES  ]" C_RESET "            " C_RED "[  NO  ]" C_RESET);
    } else {
        if (msgTimer > 0) {
            snprintf(rows[16], ROWLEN, C_WHITE "%.31s" C_RESET, msg[0]);
            snprintf(rows[17], ROWLEN, C_WHITE "%.31s" C_RESET, msg[1]);
        }
        const char *saveCol = fatOk ? C_CYAN : C_GREY;
        const char *prCol = game_can_prestige(&game) ? C_CYAN : C_GREY;
        snprintf(rows[21], ROWLEN, " %s[ SAVE ]" C_RESET "            %s[PRESTIGE]" C_RESET, saveCol, prCol);
    }
    snprintf(rows[23], ROWLEN, C_GREY " %.30s" C_RESET, saveStatus);

    flush_rows(&botCon);
}

/* ---------------- Sprites ---------------- */

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
    // Top: upgrade icons beside their rows
    for (int k = 0; k < VISIBLE; k++) {
        int i = scroll + k;
        if (i < UPGRADE_COUNT && iconGfx[i])
            oamSet(&oamMain, k, 8, (2 + k * 2) * 8, 0, 0, SpriteSize_16x16, SpriteColorFormat_256Color,
                   iconGfx[i], -1, false, false, false, false, false);
        else
            oamSet(&oamMain, k, 0, 0, 0, 0, SpriteSize_16x16, SpriteColorFormat_256Color,
                   iconGfx[0], -1, false, true, false, false, false);
    }

    // Bottom: Jerry
    int wobble;
    int v = jerry_variant(&wobble);
    int dy = pressFrames > 0 ? 3 : 0;
    oamSet(&oamSub, 0, JERRY_X + wobble, JERRY_Y + dy, 1, 0, SpriteSize_64x64, SpriteColorFormat_256Color,
           jerryGfx[v], -1, false, false, false, false, false);

    // Floating candies
    for (int i = 0; i < FLOATERS; i++) {
        if (floaters[i].on) {
            oamSet(&oamSub, 1 + i, floaters[i].x, floaters[i].y, 0, 0, SpriteSize_16x16,
                   SpriteColorFormat_256Color, candyGfx, -1, false, false, false, false, false);
        } else {
            hide_sub(1 + i, SpriteSize_16x16, candyGfx);
        }
    }

    // Golden Jerry
    if (goldenOn) {
        static const int bob[4] = {0, -2, -4, -2};
        oamSet(&oamSub, 5, goldenX, goldenY + bob[(frame >> 3) & 3], 0, 0, SpriteSize_32x32,
               SpriteColorFormat_256Color, goldenGfx, -1, false, false, false, false, false);
    } else {
        hide_sub(5, SpriteSize_32x32, goldenGfx);
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
    char a[48], b[48], c[48];
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
    char a[48], b[48];
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
    if (sel >= scroll + VISIBLE) scroll = sel - VISIBLE + 1;
}

/* ---------------- Main loop ---------------- */

int main(void) {
    srand((unsigned)time(NULL));
    game_init(&game);
    setup();

    consoleSelect(&botCon);
    consoleClear();
    iprintf("Starting...");
    fatOk = fatInitDefault();
    load_game();
    schedule_golden();

    u32 sinceSave = 0;
    bool dirtyTop = true, dirtyBot = true;

    while (1) {
        swiWaitForVBlank();
        oamUpdate(&oamMain);
        oamUpdate(&oamSub);

        scanKeys();
        u32 down = keysDown();
        u32 rep = keysDownRepeat();
        touchPosition t;
        memset(&t, 0, sizeof t);
        bool touched = (down & KEY_TOUCH) != 0;
        if (touched) touchRead(&t);

        if (confirmPrestige) {
            bool yes = (down & KEY_A) || (touched && t.py >= 160 && t.px < 128);
            bool no = (down & KEY_B) || (touched && t.py >= 160 && t.px >= 128);
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
            if (down & KEY_L) { sel -= VISIBLE; dirtyTop = true; }
            if (down & KEY_R) { sel += VISIBLE; dirtyTop = true; }
            clamp_scroll();
            if (down & KEY_A) {
                game_buy(&game, sel);
                dirtyTop = dirtyBot = true;
            }
            if (down & KEY_B) {
                do_click(JERRY_X + 32, JERRY_Y + 16);
                dirtyBot = true;
            }
            if (down & KEY_START) {
                request_prestige();
                dirtyBot = true;
            }
            if (touched) {
                if (goldenOn && t.px >= goldenX - 4 && t.px < goldenX + 36 &&
                    t.py >= goldenY - 8 && t.py < goldenY + 36) {
                    claim_golden();
                } else if (t.py >= 160 && t.py < 184) {
                    if (t.px < 112) {
                        save_game();
                    } else {
                        request_prestige();
                    }
                } else if (t.px >= JERRY_X - 12 && t.px < JERRY_X + 76 &&
                           t.py >= JERRY_Y - 12 && t.py < JERRY_Y + 76) {
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
            goldenY = 30 + rand() % 110;
        }

        if (fatOk && ++sinceSave >= AUTOSAVE_FRAMES) {
            sinceSave = 0;
            save_game();
            dirtyBot = true;
        }

        frame++;
        if (dirtyTop || frame % 10 == 0) { draw_top(); dirtyTop = false; }
        if (dirtyBot || frame % 6 == 0) { draw_bottom(); dirtyBot = false; }
        update_sprites();
    }
    return 0;
}
