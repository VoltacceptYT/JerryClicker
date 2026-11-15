#include "game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const UpgradeDef UPGRADES[UPGRADE_COUNT] = {
    {"inflatable_jerry", "Inflatable Jerry", "+0.1 candy/sec auto-gen", 75, UP_AUTO, 0.1},
    {"jerry_box_green", "Jerry Box (Green)", "+0.5 candies/sec auto-gen", 600, UP_AUTO, 0.5},
    {"jerry_box_blue", "Jerry Box (Blue)", "+3 candies/sec auto-gen", 5000, UP_AUTO, 3},
    {"jerry_box_purple", "Jerry Box (Purple)", "+15 candies/sec auto-gen", 40000, UP_AUTO, 15},
    {"jerry_box_golden", "Jerry Box (Golden)", "+80 candies/sec auto-gen", 500000, UP_AUTO, 80},
    {"unfulfilled_jerryseed", "Unfulfilled Jerryseed", "+100 candies/sec auto-gen", 1500000, UP_AUTO, 100},
    {"jerry_box_mega", "Jerry Box (Mega)", "+500 candies/sec auto-gen", 8000000, UP_AUTO, 500},
    {"unripe_jerryseed", "Unripe Jerryseed", "+1k candies/sec auto-gen", 20000000, UP_AUTO, 1000},
    {"jerry_staff", "Jerry Staff", "Quadruples auto-gen rate", 40000000, UP_AUTOMULT, 4},
    {"jerry_rune", "Jerry Rune I", "+2.5k candies/sec auto-gen", 80000000, UP_AUTO, 2500},
    {"fertilized_jerryseed", "Fertilized Jerryseed", "+6k candies/sec auto-gen", 300000000.0, UP_AUTO, 6000},
    {"jerry_rune_2", "Jerry Rune II", "+15k candies/sec auto-gen", 1500000000.0, UP_AUTO, 15000},
    {"jerryflower", "Jerryflower", "+40k candies/sec auto-gen", 5000000000.0, UP_AUTO, 40000},
    {"jerry_rune_3", "Jerry Rune III", "+90k candies/sec auto-gen", 25000000000.0, UP_AUTO, 90000},
    {"jerry_talisman_green", "Jerry Talisman (Green)", "+550k candies/sec auto-gen", 400000000000.0, UP_AUTO, 550000},
    {"jerry_talisman_blue", "Jerry Talisman (Blue)", "+3.5M candies/sec auto-gen", 6000000000000.0, UP_AUTO, 3500000},
    {"jerry_talisman_purple", "Jerry Talisman (Purple)", "+22M candies/sec auto-gen", 80000000000000.0, UP_AUTO, 22000000},
    {"jerry_talisman_golden", "Jerry Talisman (Golden)", "+150M candies/sec auto-gen", 1000000000000000.0, UP_AUTO, 150000000},
    {"jerry_stone", "Jerry Stone", "+50 candies per click", 200000, UP_CLICK, 50},
    {"aspect_of_the_jerry", "Aspect of the Jerry", "Unlock prestige system", 750000, UP_PRESTIGE, 1},
    {"aspect_of_the_jerry_signature", "Aspect of the Jerry (Sig.)", "Prestige multiplier x2", 2000000, UP_PRESTIGEMULT, 2},
    {"dye_jerry", "Jerry Dye", "+2.5k candies per click", 10000000, UP_CLICK, 2500},
    {"pet_item_toy_jerry", "Pet Item: Jerry 3D Glasses", "Adds trippy 3D anaglyph effect to Jerry", 25000000, UP_COSMETIC, 1},
    {"pet_item_lord_jerry", "Pet Item: Lord's Crown", "Converts Jerry into Lord Jerry", 25000000, UP_COSMETIC, 1},
    {"pet_item_scuba_jerry", "Pet Item: Scuba Mask", "Converts Jerry into Scuba Jerry", 25000000, UP_COSMETIC, 1},
    {"pet_item_angel_jerry", "Pet Item: Holy Halo", "Converts Jerry into Angel Jerry", 25000000, UP_COSMETIC, 1},
    {"pet_item_iron_jerry", "Pet Item: Iron Totem", "Converts Jerry into Iron Jerry", 25000000, UP_COSMETIC, 1},
};

const ToyDef TOYS[TOY_COUNT] = {
    {"beach_ball", "Beach Ball", "Bounces around", 250},
    {"plastic_shovel", "Plastic Shovel", "Builds sandcastles", 500},
    {"squeaky_mouse", "Squeaky Mouse", "Scurries about", 750},
    {"magic_8_ball", "Magic 8-Ball", "Heavy, rolls far", 1500},
    {"balloon_snake", "Balloon Snake", "Floats on the breeze", 3000},
    {"white_dice", "White Dice", "Roll for initiative!", 6000},
};

static double frenzy_mult(const Game *g) { return g->frenzyFrames > 0 ? 2.0 : 1.0; }

void game_init(Game *g) {
    memset(g, 0, sizeof(*g));
    g->s.perClick = 1;
    g->s.prestigeMult = 1;
    for (int i = 0; i < UPGRADE_COUNT; i++) {
        g->s.up[i].count = 0;
        g->s.up[i].cost = UPGRADES[i].cost;
    }
}

double game_per_click(const Game *g) { return g->s.perClick * frenzy_mult(g); }
double game_auto_rate(const Game *g) { return g->s.autoRate * frenzy_mult(g); }

void game_tick(Game *g) {
    if (g->s.autoRate > 0) g->s.candies += game_auto_rate(g) / 60.0;
    if (g->frenzyFrames > 0) g->frenzyFrames--;
}

double game_click(Game *g) {
    double gain = floor(game_per_click(g));
    g->s.candies += gain;
    return gain;
}

bool game_is_maxed(const Game *g, int idx) {
    UpType t = UPGRADES[idx].type;
    bool single = (t == UP_PRESTIGE || t == UP_PRESTIGEMULT || t == UP_COSMETIC || t == UP_CLICK);
    // Jerry Stone is flagged as a one-time upgrade in the original.
    return single && g->s.up[idx].count >= 1;
}

bool game_can_buy(const Game *g, int idx) {
    if (idx < 0 || idx >= UPGRADE_COUNT) return false;
    if (game_is_maxed(g, idx)) return false;
    return g->s.candies >= g->s.up[idx].cost;
}

int game_active_cosmetic(const Game *g) {
    for (int i = 0; i < UPGRADE_COUNT; i++)
        if (UPGRADES[i].type == UP_COSMETIC && g->s.up[i].count > 0) return i;
    return -1;
}

static void apply_upgrade(Game *g, int idx) {
    const UpgradeDef *u = &UPGRADES[idx];
    double mult = g->s.prestigeMult;
    switch (u->type) {
    case UP_CLICK: g->s.perClick += u->value * mult; break;
    case UP_AUTO: g->s.autoRate += u->value * mult; break;
    case UP_AUTOMULT: g->s.autoRate *= u->value; break;
    default: break; // prestige / cosmetics are handled elsewhere
    }
}

bool game_buy(Game *g, int idx) {
    if (!game_can_buy(g, idx)) return false;
    const UpgradeDef *u = &UPGRADES[idx];

    if (u->type == UP_COSMETIC) {
        int old = game_active_cosmetic(g);
        if (old >= 0 && old != idx) {
            // Swapping cosmetics refunds half the old one's price.
            g->s.candies += (double)(long long)(g->s.up[old].cost / 2);
            g->s.up[old].count = 0;
            g->s.up[old].cost = UPGRADES[old].cost;
        }
    }

    // Buying a cosmetic may have refunded candies; re-check affordability.
    if (g->s.candies < g->s.up[idx].cost) return false;

    g->s.candies -= g->s.up[idx].cost;
    g->s.up[idx].count++;
    g->s.up[idx].cost *= COST_GROWTH;
    apply_upgrade(g, idx);
    return true;
}

static int idx_of(const char *id) {
    for (int i = 0; i < UPGRADE_COUNT; i++)
        if (strcmp(UPGRADES[i].id, id) == 0) return i;
    return 0;
}

double game_toy_cost(const Game *g, int toy) {
    return TOYS[toy].cost * pow(TOY_GROWTH, g->s.toys[toy]);
}

bool game_toy_maxed(const Game *g, int toy) { return g->s.toys[toy] >= TOY_MAX; }

bool game_can_buy_toy(const Game *g, int toy) {
    if (toy < 0 || toy >= TOY_COUNT || game_toy_maxed(g, toy)) return false;
    return g->s.candies >= game_toy_cost(g, toy);
}

bool game_buy_toy(Game *g, int toy) {
    if (!game_can_buy_toy(g, toy)) return false;
    g->s.candies -= game_toy_cost(g, toy);
    g->s.toys[toy]++;
    return true;
}

bool game_prestige_unlocked(const Game *g) {
    return g->s.up[idx_of("aspect_of_the_jerry")].count > 0;
}

double game_prestige_requirement(const Game *g) { return 150000000.0 * g->s.prestigeMult; }

double game_prestige_next_mult(const Game *g) {
    bool sig = g->s.up[idx_of("aspect_of_the_jerry_signature")].count > 0;
    return 1.0 + (g->s.prestigeLevel + 1) * 0.1 * (sig ? 2 : 1);
}

bool game_can_prestige(const Game *g) {
    return game_prestige_unlocked(g) && g->s.candies >= game_prestige_requirement(g);
}

void game_do_prestige(Game *g) {
    double newMult = game_prestige_next_mult(g);
    g->s.prestigeLevel++;
    g->s.prestigeMult = newMult;
    g->s.candies = 0;
    g->s.perClick = 1;
    g->s.autoRate = 0;
    g->frenzyFrames = 0;
    for (int i = 0; i < UPGRADE_COUNT; i++) {
        g->s.up[i].count = 0;
        g->s.up[i].cost = UPGRADES[i].cost;
    }
}

static double dmax(double a, double b) { return a > b ? a : b; }

GoldenReward game_claim_golden(Game *g, double roll) {
    GoldenReward r;
    if (roll < 0.40) {
        r.kind = GR_WINDFALL;
        r.amount = dmax(floor(game_per_click(g) * 30), floor(game_auto_rate(g) * 60 * 15));
    } else if (roll < 0.75) {
        r.kind = GR_FRENZY;
        if (g->frenzyFrames <= 0) {
            g->frenzyFrames = FRENZY_FRAMES;
            r.amount = 0;
        } else {
            r.amount = dmax(floor(game_per_click(g) * 20), floor(game_auto_rate(g) * 60 * 8));
        }
    } else {
        r.kind = GR_LUCKY;
        r.amount = dmax(floor(game_per_click(g) * 30), floor(g->s.candies * 0.10));
    }
    g->s.candies += r.amount;
    return r;
}

/* ---------------- Saving ---------------- */

#define SAVE_MAGIC 0x314B434Au /* "JCK1" */

typedef struct {
    uint32_t magic;
    uint32_t size;
    GameState state;
    uint32_t checksum;
} SaveBlob;

static uint32_t checksum_bytes(const void *data, size_t n) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

size_t game_save_size(void) { return sizeof(SaveBlob); }

void game_pack(const Game *g, uint8_t *out) {
    SaveBlob b;
    memset(&b, 0, sizeof(b));
    b.magic = SAVE_MAGIC;
    b.size = (uint32_t)sizeof(SaveBlob);
    b.state = g->s;
    b.checksum = checksum_bytes(&b.state, sizeof(b.state));
    memcpy(out, &b, sizeof(b));
}

/* Saves from before the toys update: same as GameState without the toy counts. */
typedef struct {
    double candies;
    double perClick;
    double autoRate;
    UpgradeState up[UPGRADE_COUNT];
    int32_t prestigeLevel;
    double prestigeMult;
} PreToysState;

typedef struct {
    uint32_t magic;
    uint32_t size;
    PreToysState state;
    uint32_t checksum;
} PreToysBlob;

/* Saves from when there were only two toys (beach ball, plastic shovel). */
typedef struct {
    double candies;
    double perClick;
    double autoRate;
    UpgradeState up[UPGRADE_COUNT];
    int32_t prestigeLevel;
    double prestigeMult;
    int32_t toys[2];
} TwoToysState;

typedef struct {
    uint32_t magic;
    uint32_t size;
    TwoToysState state;
    uint32_t checksum;
} TwoToysBlob;

/* Saves from before the Jerryseed/Jerryflower/Dye update held 22 upgrades in this order. */
#define LEGACY_COUNT 22
static const char *const LEGACY_IDS[LEGACY_COUNT] = {
    "inflatable_jerry", "jerry_box_green", "jerry_box_blue", "jerry_box_purple",
    "jerry_box_golden", "jerry_box_mega", "jerry_staff", "jerry_rune", "jerry_rune_2",
    "jerry_rune_3", "jerry_talisman_green", "jerry_talisman_blue", "jerry_talisman_purple",
    "jerry_talisman_golden", "jerry_stone", "aspect_of_the_jerry",
    "aspect_of_the_jerry_signature", "pet_item_toy_jerry", "pet_item_lord_jerry",
    "pet_item_scuba_jerry", "pet_item_angel_jerry", "pet_item_iron_jerry",
};

typedef struct {
    double candies;
    double perClick;
    double autoRate;
    UpgradeState up[LEGACY_COUNT];
    int32_t prestigeLevel;
    double prestigeMult;
} LegacyState;

typedef struct {
    uint32_t magic;
    uint32_t size;
    LegacyState state;
    uint32_t checksum;
} LegacyBlob;

static bool sane(double v) { return isfinite(v) && v >= 0; }

bool game_unpack(Game *g, const uint8_t *in, size_t len) {
    GameState cur;
    memset(&cur, 0, sizeof(cur));
    uint32_t magic = 0, size = 0;
    if (len < 8) return false;
    memcpy(&magic, in, 4);
    memcpy(&size, in + 4, 4);
    if (magic != SAVE_MAGIC) return false;

    if (size == sizeof(SaveBlob) && len >= sizeof(SaveBlob)) {
        SaveBlob b;
        memcpy(&b, in, sizeof(b));
        if (b.checksum != checksum_bytes(&b.state, sizeof(b.state))) return false;
        cur = b.state;
    } else if (size == sizeof(TwoToysBlob) && len >= sizeof(TwoToysBlob)) {
        TwoToysBlob b;
        memcpy(&b, in, sizeof(b));
        if (b.checksum != checksum_bytes(&b.state, sizeof(b.state))) return false;
        cur.candies = b.state.candies;
        cur.perClick = b.state.perClick;
        cur.autoRate = b.state.autoRate;
        memcpy(cur.up, b.state.up, sizeof(cur.up));
        cur.prestigeLevel = b.state.prestigeLevel;
        cur.prestigeMult = b.state.prestigeMult;
        cur.toys[0] = b.state.toys[0];
        cur.toys[1] = b.state.toys[1];
    } else if (size == sizeof(PreToysBlob) && len >= sizeof(PreToysBlob)) {
        PreToysBlob b;
        memcpy(&b, in, sizeof(b));
        if (b.checksum != checksum_bytes(&b.state, sizeof(b.state))) return false;
        cur.candies = b.state.candies;
        cur.perClick = b.state.perClick;
        cur.autoRate = b.state.autoRate;
        memcpy(cur.up, b.state.up, sizeof(cur.up));
        cur.prestigeLevel = b.state.prestigeLevel;
        cur.prestigeMult = b.state.prestigeMult;
    } else if (size == sizeof(LegacyBlob) && len >= sizeof(LegacyBlob)) {
        LegacyBlob lb;
        memcpy(&lb, in, sizeof(lb));
        if (lb.checksum != checksum_bytes(&lb.state, sizeof(lb.state))) return false;
        cur.candies = lb.state.candies;
        cur.perClick = lb.state.perClick;
        cur.autoRate = lb.state.autoRate;
        cur.prestigeLevel = lb.state.prestigeLevel;
        cur.prestigeMult = lb.state.prestigeMult;
        for (int i = 0; i < LEGACY_COUNT; i++) {
            int idx = idx_of(LEGACY_IDS[i]);
            cur.up[idx].count = lb.state.up[i].count;
        }
    } else {
        return false;
    }

    GameState *s = &cur;
    if (!sane(s->candies) || !sane(s->perClick) || !sane(s->autoRate) || !sane(s->prestigeMult)) return false;
    if (s->prestigeLevel < 0) return false;
    for (int i = 0; i < UPGRADE_COUNT; i++) {
        if (s->up[i].count < 0 || s->up[i].count > 100000) return false;
        s->up[i].cost = UPGRADES[i].cost * pow(COST_GROWTH, s->up[i].count);
    }
    for (int i = 0; i < TOY_COUNT; i++) {
        if (s->toys[i] < 0) return false;
        if (s->toys[i] > TOY_MAX) s->toys[i] = TOY_MAX;
    }
    game_init(g);
    g->s = *s;
    return true;
}

/* ---------------- Formatting ---------------- */

void format_number(double num, char *out, size_t outSize) {
    static const char *suffixes[] = {
        "", "K", "M", "B", "T", "Quadrillion", "Quintillion", "Sextillion", "Septillion",
        "Octillion", "Nonillion", "Decillion", "Undecillion", "Duodecillion", "Tredecillion",
        "Quattuordecillion", "Quindecillion"};
    const int maxTier = (int)(sizeof(suffixes) / sizeof(suffixes[0])) - 1;

    if (isnan(num)) { snprintf(out, outSize, "NaN"); return; }
    if (isinf(num)) { snprintf(out, outSize, "Infinity"); return; }

    int tier = 0;
    while (num >= 1000 && tier < maxTier) {
        num /= 1000;
        tier++;
    }
    if (tier == 0) snprintf(out, outSize, "%.2f", num);
    else snprintf(out, outSize, "%.2f %s", num, suffixes[tier]);
}
