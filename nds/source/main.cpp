#include <nds.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "sprites.h"

#define W 256
#define H 192
#define UPGRADE_COUNT 22
#define PAGE_SIZE 8
#define JERRY_X 70
#define JERRY_Y 42
#define JERRY_W 116
#define JERRY_H 116
#define GOLDEN_W 40
#define GOLDEN_H 40

struct Upgrade {
    const char* id;
    const char* name;
    const char* effect;
    uint64_t baseCost;
    uint64_t cost;
    double value;
    uint32_t owned;
    uint8_t type;
    bool oneShot;
};

static Upgrade upgrades[UPGRADE_COUNT] = {
    {"inflatable_jerry", "Inflatable Jerry", "+0.1 candy/sec", 75, 75, 0.1, 0, 0, false},
    {"jerry_box_green", "Jerry Box Green", "+0.5 candies/sec", 600, 600, 0.5, 0, 0, false},
    {"jerry_box_blue", "Jerry Box Blue", "+3 candies/sec", 5000, 5000, 3, 0, 0, false},
    {"jerry_box_purple", "Jerry Box Purple", "+15 candies/sec", 40000, 40000, 15, 0, 0, false},
    {"jerry_box_golden", "Jerry Box Golden", "+80 candies/sec", 500000, 500000, 80, 0, 0, false},
    {"jerry_box_mega", "Jerry Box Mega", "+500 candies/sec", 8000000, 8000000, 500, 0, 0, false},
    {"jerry_staff", "Jerry Staff", "x4 auto-gen", 40000000, 40000000, 4, 0, 1, true},
    {"jerry_rune", "Jerry Rune I", "+2.5K/sec", 80000000, 80000000, 2500, 0, 0, false},
    {"jerry_rune_2", "Jerry Rune II", "+15K/sec", 1500000000ULL, 1500000000ULL, 15000, 0, 0, false},
    {"jerry_rune_3", "Jerry Rune III", "+90K/sec", 25000000000ULL, 25000000000ULL, 90000, 0, 0, false},
    {"jerry_talisman_green", "Talisman Green", "+550K/sec", 400000000000ULL, 400000000000ULL, 550000, 0, 0, false},
    {"jerry_talisman_blue", "Talisman Blue", "+3.5M/sec", 6000000000000ULL, 6000000000000ULL, 3500000, 0, 0, false},
    {"jerry_talisman_purple", "Talisman Purple", "+22M/sec", 80000000000000ULL, 80000000000000ULL, 22000000, 0, 0, false},
    {"jerry_talisman_golden", "Talisman Golden", "+150M/sec", 1000000000000000ULL, 1000000000000000ULL, 150000000, 0, 0, false},
    {"jerry_stone", "Jerry Stone", "+50 per click", 200000, 200000, 50, 0, 2, true},
    {"aspect_of_the_jerry", "Aspect of Jerry", "Unlock prestige", 750000, 750000, 1, 0, 3, true},
    {"aspect_of_the_jerry_signature", "Jerry Signature", "Double prestige gain", 2000000, 2000000, 2, 0, 4, true},
    {"pet_item_toy_jerry", "3D Glasses", "Toy Jerry look", 25000000, 25000000, 1, 0, 5, true},
    {"pet_item_lord_jerry", "Lord Crown", "Lord Jerry look", 25000000, 25000000, 1, 0, 5, true},
    {"pet_item_scuba_jerry", "Scuba Mask", "Scuba Jerry look", 25000000, 25000000, 1, 0, 5, true},
    {"pet_item_angel_jerry", "Holy Halo", "Angel Jerry look", 25000000, 25000000, 1, 0, 5, true},
    {"pet_item_iron_jerry", "Iron Totem", "Iron Jerry look", 25000000, 25000000, 1, 0, 5, true},
};

static uint64_t candies = 0;
static double perClick = 1.0;
static double autoRate = 0.0;
static double autoRemainder = 0.0;
static double prestigeMultiplier = 1.0;
static uint32_t prestigeLevel = 0;
static int selected = 0;
static int page = 0;
static bool goldenActive = false;
static int goldenX = 12;
static int goldenY = 35;
static int goldenFrames = 0;
static int frenzyFrames = 0;
static bool touchWasDown = false;
static u16* topBitmap;
static PrintConsole lowerConsole;

static const JerrySprite* sprite(const char* path) {
    for (size_t i = 0; i < jerry_sprite_count; ++i) {
        if (strcmp(jerry_sprites[i].path, path) == 0) return &jerry_sprites[i];
    }
    return NULL;
}

static void pixel(int x, int y, u16 color) {
    if ((unsigned)x < W && (unsigned)y < H) topBitmap[y * W + x] = color;
}

static void fillRect(int x, int y, int w, int h, u16 color) {
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) pixel(xx, yy, color);
}

static void drawScaled(const JerrySprite* image, int x, int y, int w, int h) {
    if (!image || image->width <= 0 || image->height <= 0) return;
    for (int dy = 0; dy < h; ++dy) {
        int sy = dy * image->height / h;
        for (int dx = 0; dx < w; ++dx) {
            int sx = dx * image->width / w;
            u16 color = image->pixels[sy * image->width + sx];
            if (color & 0x8000) pixel(x + dx, y + dy, color);
        }
    }
}

static void drawSprite(const char* path, int x, int y, int scale) {
    const JerrySprite* image = sprite(path);
    if (image) drawScaled(image, x, y, image->width * scale, image->height * scale);
}

static uint64_t clickGain(void) {
    double gain = perClick * prestigeMultiplier * (frenzyFrames ? 2.0 : 1.0);
    return gain < 1.0 ? 1 : (uint64_t)gain;
}

static void clickJerry(void) { candies += clickGain(); }

static void buyUpgrade(int index) {
    if (index < 0 || index >= UPGRADE_COUNT) return;
    Upgrade* up = &upgrades[index];
    if (up->oneShot && up->owned) return;
    if (candies < up->cost) return;
    candies -= up->cost;
    ++up->owned;
    up->cost = (uint64_t)((double)up->cost * 1.5);
    switch (up->type) {
        case 0: autoRate += up->value * prestigeMultiplier; break;
        case 1: autoRate *= up->value; break;
        case 2: perClick += up->value * prestigeMultiplier; break;
        case 3: break; // Prestige availability is owned count.
        case 4: break; // Signature is applied when prestiging.
        case 5: break; // Cosmetic is selected when drawing Jerry.
    }
}

static int activeCosmetic(void) {
    const int indices[] = {17, 18, 19, 20, 21};
    for (unsigned i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i)
        if (upgrades[indices[i]].owned) return indices[i];
    return -1;
}

static const char* jerrySpritePath(void) {
    switch (activeCosmetic()) {
        case 18: return "assets/img/cosmetics/lord_jerry.png";
        case 19: return "assets/img/cosmetics/scuba_jerry.png";
        case 20: return "assets/img/cosmetics/angel_jerry.png";
        case 21: return "assets/img/cosmetics/iron_jerry.png";
        default: return "assets/img/cosmetics/jerry.png";
    }
}

static uint64_t prestigeRequirement(void) { return (uint64_t)(150000000.0 * prestigeMultiplier); }

static void doPrestige(void) {
    if (!upgrades[15].owned || candies < prestigeRequirement()) return;
    double bonus = upgrades[16].owned ? 2.0 : 1.0;
    prestigeMultiplier = 1.0 + (prestigeLevel + 1) * 0.1 * bonus;
    ++prestigeLevel;
    candies = 0;
    perClick = 1.0;
    autoRate = 0.0;
    autoRemainder = 0.0;
    for (int i = 0; i < UPGRADE_COUNT; ++i) {
        if (i == 15 || i == 16) continue; // Keep prestige unlock and signature.
        upgrades[i].owned = 0;
        upgrades[i].cost = upgrades[i].baseCost;
    }
}

static void spawnGolden(void) {
    goldenActive = true;
    goldenFrames = 60 * (180 + (rand() % 121));
    goldenX = 8 + rand() % 208;
    goldenY = 28 + rand() % 108;
}

static void claimGolden(void) {
    if (!goldenActive) return;
    unsigned roll = rand() % 100;
    if (roll < 40) {
        uint64_t windfallA = (uint64_t)(perClick * 30.0);
        uint64_t windfallB = (uint64_t)(autoRate * 900.0);
        candies += windfallA > windfallB ? windfallA : windfallB;
    } else if (roll < 75) {
        if (!frenzyFrames) frenzyFrames = 30 * 60;
        else candies += (uint64_t)(perClick * 20.0 > autoRate * 480.0 ? perClick * 20.0 : autoRate * 480.0);
    } else {
        uint64_t luckyA = (uint64_t)(perClick * 30.0);
        uint64_t luckyB = candies / 10;
        candies += luckyA > luckyB ? luckyA : luckyB;
    }
    goldenActive = false;
}

static void drawTop(void) {
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            u16 c = (y < 22) ? RGB15(18, 12, 31) : RGB15(10, 7, 21);
            if (x > 212 && y > 22) c = RGB15(20, 14, 33);
            pixel(x, y, c);
        }
    }
    fillRect(0, 22, W, 2, RGB15(26, 19, 40));

    // Actual artwork from the repo, nearest-neighbour scaled for the DS bitmap.
    drawScaled(sprite(jerrySpritePath()), JERRY_X, JERRY_Y, JERRY_W, JERRY_H);
    drawScaled(sprite("assets/img/ui/button.png"), 62, 157, 132, 28);
    drawScaled(sprite("api/resources/jerry_candy.png"), 10, 5, 14, 14);
    drawScaled(sprite("assets/img/ui/button_prestige.png"), 206, 3, 42, 17);
    if (goldenActive) drawScaled(sprite("assets/img/powerups/golden_jerry.png"), goldenX, goldenY, GOLDEN_W, GOLDEN_H);
}

static void drawLower(void) {
    consoleSelect(&lowerConsole);
    consoleClear();
    iprintf(" JERRY CLICKER - DS\n");
    iprintf(" Candies: %llu\n", (unsigned long long)candies);
    iprintf(" Click: %llu  Auto/s: %d\n", (unsigned long long)clickGain(), (int)autoRate);
    iprintf(" Prestige x%d.%d  Level %lu\n", (int)prestigeMultiplier, (int)(prestigeMultiplier * 10) % 10, (unsigned long)prestigeLevel);
    iprintf(" ------------------------\n");
    int start = page * PAGE_SIZE;
    for (int row = 0; row < PAGE_SIZE; ++row) {
        int i = start + row;
        if (i >= UPGRADE_COUNT) break;
        Upgrade* up = &upgrades[i];
        iprintf("%c%-2d %-18s %llu\n", i == selected ? '>' : ' ', i + 1, up->name, (unsigned long long)up->cost);
    }
    iprintf(" ------------------------\n");
    iprintf("Tap row to buy; L/R page; X buy\n");
    iprintf("A tap Jerry; Y prestige when ready\n");
    if (frenzyFrames) iprintf(" FRENZY: %d sec\n", frenzyFrames / 60);
    if (goldenActive) iprintf(" Golden Jerry! Tap it on top!\n");
}

int main(void) {
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    BG2_CR = BG_BMP16_256x256 | BG_BMP_BASE(0) | BG_PRIORITY(0);
    topBitmap = (u16*)BG_BMP_RAM(0);
    consoleInit(&lowerConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    srand((unsigned)timerElapsed(0));
    int goldenWait = 60 * (180 + (rand() % 121));
    unsigned tick = 0;
    while (1) {
        scanKeys();
        u16 down = keysDown();
        u16 held = keysHeld();
        touchPosition touch;
        bool touchDown = (held & KEY_TOUCH) != 0;
        bool touchPressed = touchDown && !touchWasDown;
        touchWasDown = touchDown;

        if (touchPressed) {
            touchRead(&touch);
            if (touch.py < H) {
                int x = touch.px;
                int y = touch.py;
                if (goldenActive && x >= goldenX && x < goldenX + GOLDEN_W && y >= goldenY && y < goldenY + GOLDEN_H) claimGolden();
                else if (x >= JERRY_X && x < JERRY_X + JERRY_W && y >= JERRY_Y && y < JERRY_Y + JERRY_H) clickJerry();
                else if (y >= 157 && y <= 185 && x >= 62 && x <= 194) clickJerry();
                else if (y < 22 && x >= 206 && upgrades[15].owned && candies >= prestigeRequirement()) doPrestige();
            }
            // Lower-screen touch selects/buys one of the visible upgrade rows.
            if (touch.py >= 80 && touch.py < 176) {
                int row = (touch.py - 80) / 12;
                int index = page * PAGE_SIZE + row;
                if (index < UPGRADE_COUNT) { selected = index; buyUpgrade(index); }
            }
        }

        if (down & KEY_A) clickJerry();
        if (down & KEY_X) buyUpgrade(selected);
        if (down & KEY_LEFT) { if (selected > 0) --selected; page = selected / PAGE_SIZE; }
        if (down & KEY_RIGHT) { if (selected + 1 < UPGRADE_COUNT) ++selected; page = selected / PAGE_SIZE; }
        if (down & KEY_L) { page = (page + UPGRADE_COUNT / PAGE_SIZE) % ((UPGRADE_COUNT + PAGE_SIZE - 1) / PAGE_SIZE); selected = page * PAGE_SIZE; }
        if (down & KEY_R) { page = (page + 1) % ((UPGRADE_COUNT + PAGE_SIZE - 1) / PAGE_SIZE); selected = page * PAGE_SIZE; }
        if (down & KEY_Y) doPrestige();

        autoRemainder += autoRate / 60.0 * (frenzyFrames ? 2.0 : 1.0);
        if (autoRemainder >= 1.0) { uint64_t gain = (uint64_t)autoRemainder; candies += gain; autoRemainder -= gain; }
        if (frenzyFrames > 0) --frenzyFrames;
        if (goldenActive) { if (--goldenFrames <= 0) goldenActive = false; }
        else if (--goldenWait <= 0) { spawnGolden(); goldenWait = 60 * (180 + (rand() % 121)); }

        drawTop();
        if ((tick++ % 8) == 0) drawLower();
        swiWaitForVBlank();
    }
    return 0;
}
