/*
 * sprites_data.h - Hand-authored tile art for player and ball sprites,
 * plus the palettes used to recolor the same shape per team.
 *
 * No image assets / resource compiler needed: tiles are plain 8x8 4bpp
 * nibble-packed arrays, uploaded straight to VRAM at boot.
 */
#ifndef _SPRITES_DATA_H_
#define _SPRITES_DATA_H_

#include "genesis.h"

/* Tile indices (relative to TILE_USER_INDEX) */
#define TILE_PLAYER_TOP     (TILE_USER_INDEX + 0)
#define TILE_PLAYER_BOTTOM  (TILE_USER_INDEX + 1)
#define TILE_BALL           (TILE_USER_INDEX + 2)

/* Palette lines: PAL0 is used by the system font, so sprites use 1-3 */
#define PAL_TEAM_A          PAL1
#define PAL_TEAM_B          PAL2
#define PAL_BALL            PAL3

void sprites_data_init(void);

#endif /* _SPRITES_DATA_H_ */
