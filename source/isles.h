// Jerry Isles: tiled island scene (autotiled from assets/island/island_tiles.png) with
// wandering, interacting critters (one per ISL_PER purchases of each upgrade).
#ifndef ISLES_H
#define ISLES_H

#include <stdint.h>
#include "game.h"
#include "gfx.h"

#define ISL_PER 5            // one critter per this many purchases of an upgrade
#define ISL_MAX_CRITTERS 48  // total on the island (shared fairly between upgrades)
#define ISL_BALL_KINDS 5                  // beach ball, squeaky mouse, 8-ball, balloon snake, white dice
#define ISL_MAX_BALLS (TOY_MAX * ISL_BALL_KINDS)
#define ISL_MAX_CASTLES (TOY_MAX * 2)  // each plastic shovel lets two sandcastles stand at once

// Builds the static terrain into bgBuf (SCREEN_W x SCREEN_H) and resets the simulation.
void isles_init(px *bgBuf, uint32_t seed);
const px *isles_bg(void);
// Re-reads the day/night cycle (see daycycle.h) and re-tints the island when the light has shifted
// noticeably. Cheap; call about once a second, whichever page is showing.
void isles_update_clock(void);
// Call whenever purchase counts or owned toys change (buy, prestige, load).
void isles_sync(const Game *g);
// One simulation step (60/s). Only needed while the page is visible.
void isles_tick(void);
// Draws depth-sorted decor and critters on top of a copy of isles_bg().
void isles_draw(Canvas *c);
int isles_critter_count(void);
int isles_castle_count(void);

#endif
