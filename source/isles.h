// Jerry Isles: tiled island scene (autotiled from assets/island/island_tiles.png) with
// wandering, interacting critters (one per ISL_PER purchases of each upgrade).
#ifndef ISLES_H
#define ISLES_H

#include <stdint.h>
#include "game.h"
#include "gfx.h"

#define ISL_PER 5            // one critter per this many purchases of an upgrade
#define ISL_MAX_CRITTERS 48  // total on the island (shared fairly between upgrades)

// Builds the static terrain into bgBuf (SCREEN_W x SCREEN_H) and resets the simulation.
void isles_init(px *bgBuf, uint32_t seed);
const px *isles_bg(void);
// Call whenever purchase counts change (buy, prestige, load).
void isles_sync(const Game *g);
// One simulation step (60/s). Only needed while the page is visible.
void isles_tick(void);
// Draws depth-sorted decor and critters on top of a copy of isles_bg().
void isles_draw(Canvas *c);
int isles_critter_count(void);

#endif
