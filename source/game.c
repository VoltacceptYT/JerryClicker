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
    {"jerry_box_mega", "Jerry Box (Mega)", "+500 candies/sec auto-gen", 8000000, UP_AUTO, 500},
    {"jerry_staff", "Jerry Staff", "Quadruples auto-gen rate", 40000000, UP_AUTOMULT, 4},
    {"jerry_rune", "Jerry Rune I", "+2.5k candies/sec auto-gen", 80000000, UP_AUTO, 2500},
    {"jerry_rune_2", "Jerry Rune II", "+15k candies/sec auto-gen", 1500000000.0, UP_AUTO, 15000},
    {"jerry_rune_3", "Jerry Rune III", "+90k candies/sec auto-gen", 25000000000.0, UP_AUTO, 90000},
    {"jerry_talisman_green", "Jerry Talisman (Green)", "+550k candies/sec auto-gen", 400000000000.0, UP_AUTO, 550000},
    {"jerry_talisman_blue", "Jerry Talisman (Blue)", "+3.5M candies/sec auto-gen", 6000000000000.0, UP_AUTO, 3500000},
    {"jerry_talisman_purple", "Jerry Talisman (Purple)", "+22M candies/sec auto-gen", 80000000000000.0, UP_AUTO, 22000000},
    {"jerry_talisman_golden", "Jerry Talisman (Golden)", "+150M candies/sec auto-gen", 1000000000000000.0, UP_AUTO, 150000000},
    {"jerry_stone", "Jerry Stone", "+50 candies per click", 200000, UP_CLICK, 50},
    {"aspect_of_the_jerry", "Aspect of the Jerry", "Unlock prestige system", 750000, UP_PRESTIGE, 1},
    {"aspect_of_the_jerry_signature", "Aspect of the Jerry (Sig.)", "Prestige multiplier x2", 2000000, UP_PRESTIGEMULT, 2},
    {"pet_item_toy_jerry", "Pet Item: Jerry 3D Glasses", "Adds trippy 3D anaglyph effect to Jerry", 25000000, UP_COSMETIC, 1},
    {"pet_item_lord_jerry", "Pet Item: Lord's Crown", "Converts Jerry into Lord Jerry", 25000000, UP_COSMETIC, 1},
    {"pet_item_scuba_jerry", "Pet Item: Scuba Mask", "Converts Jerry into Scuba Jerry", 25000000, UP_COSMETIC, 1},
    {"pet_item_angel_jerry", "Pet Item: Holy Halo", "Converts Jerry into Angel Jerry", 25000000, UP_COSMETIC, 1},
    {"pet_item_iron_jerry", "Pet Item: Iron Totem", "Converts Jerry into Iron Jerry", 25000000, UP_COSMETIC, 1},
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

static uint32_t checksum(const GameState *s) {
    const uint8_t *p = (const uint8_t *)s;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof(*s); i++) {
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
    b.checksum = checksum(&b.state);
    memcpy(out, &b, sizeof(b));
}

static bool sane(double v) { return isfinite(v) && v >= 0; }

bool game_unpack(Game *g, const uint8_t *in, size_t len) {
    if (len < sizeof(SaveBlob)) return false;
    SaveBlob b;
    memcpy(&b, in, sizeof(b));
    if (b.magic != SAVE_MAGIC || b.size != sizeof(SaveBlob)) return false;
    if (b.checksum != checksum(&b.state)) return false;

    GameState *s = &b.state;
    if (!sane(s->candies) || !sane(s->perClick) || !sane(s->autoRate) || !sane(s->prestigeMult)) return false;
    if (s->prestigeLevel < 0) return false;
    for (int i = 0; i < UPGRADE_COUNT; i++) {
        if (s->up[i].count < 0 || s->up[i].count > 100000) return false;
        // Re-derive price from the base cost so old pricing never sticks.
        s->up[i].cost = UPGRADES[i].cost * pow(COST_GROWTH, s->up[i].count);
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
