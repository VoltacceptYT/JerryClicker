// Platform-independent game logic (ported from the original script.js).
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Each purchase multiplies an upgrade's price by this much.
#define COST_GROWTH 1.15
#define UPGRADE_COUNT 22
#define FRENZY_FRAMES (30 * 60)

typedef enum {
    UP_AUTO,
    UP_CLICK,
    UP_AUTOMULT,
    UP_PRESTIGE,
    UP_PRESTIGEMULT,
    UP_COSMETIC
} UpType;

typedef struct {
    const char *id;
    const char *name;
    const char *effect;
    double cost;
    UpType type;
    double value;
} UpgradeDef;

extern const UpgradeDef UPGRADES[UPGRADE_COUNT];

typedef struct {
    int32_t count;
    double cost;
} UpgradeState;

typedef struct {
    double candies;
    double perClick;
    double autoRate;
    UpgradeState up[UPGRADE_COUNT];
    int32_t prestigeLevel;
    double prestigeMult;
} GameState;

typedef struct {
    GameState s;
    int frenzyFrames; // not saved
} Game;

typedef enum { GR_WINDFALL, GR_FRENZY, GR_LUCKY } GoldenKind;
typedef struct {
    GoldenKind kind;
    double amount; // candies granted (can be 0 for a fresh frenzy)
} GoldenReward;

void game_init(Game *g);

double game_per_click(const Game *g);   // effective (includes frenzy)
double game_auto_rate(const Game *g);   // effective (includes frenzy)
void game_tick(Game *g);                // call once per frame (60/s)
double game_click(Game *g);             // returns candies gained

bool game_is_maxed(const Game *g, int idx);
bool game_can_buy(const Game *g, int idx);
bool game_buy(Game *g, int idx);
int game_active_cosmetic(const Game *g); // upgrade index or -1

bool game_prestige_unlocked(const Game *g);
double game_prestige_requirement(const Game *g);
double game_prestige_next_mult(const Game *g);
bool game_can_prestige(const Game *g);
void game_do_prestige(Game *g);

GoldenReward game_claim_golden(Game *g, double roll);

// Saving
size_t game_save_size(void);
void game_pack(const Game *g, uint8_t *out);
bool game_unpack(Game *g, const uint8_t *in, size_t len);

void format_number(double num, char *out, size_t outSize);

#endif
