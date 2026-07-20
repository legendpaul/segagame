/*
 * sprites_data.h - Hand-authored tile art for player and ball sprites,
 * plus the palettes used to recolor the same shape per team.
 *
 * No image assets / resource compiler needed: tiles are plain 8x8 4bpp
 * nibble-packed arrays, uploaded straight to VRAM at boot.
 *
 * Players are now a 2x2 tile block (16x16px) sharing ONE tile set: only
 * the palette differs, so both the human's and CPU's court sprite use
 * TILE_PLAYER, just recolored per the team actually selected on the menu
 * (see sprites_data_apply_teams). Previously each side had a hardcoded
 * color regardless of chosen team - that mismatch was the "scoring looks
 * broken" bug: the HUD named the right winner, but the sprite on court
 * never matched the team name.
 */
#ifndef _SPRITES_DATA_H_
#define _SPRITES_DATA_H_

#include "genesis.h"

/* Tile indices (relative to TILE_USER_INDEX) */
#define TILE_PLAYER         (TILE_USER_INDEX + 0)   /* 4 consecutive tiles, 2x2 */
#define TILE_BALL           (TILE_USER_INDEX + 4)   /* 1 tile */

/* Palette lines: PAL0 is used by the system font + pitch background,
 * so sprites use 1-3. PAL1/PAL2 are *slots*, not fixed teams - which
 * actual team colors live there is set per-match by
 * sprites_data_apply_teams(). */
#define PAL_TEAM_A          PAL1
#define PAL_TEAM_B          PAL2
#define PAL_BALL            PAL3

void sprites_data_init(void);

/* Loads the real per-team colors into the A/B palette slots for the
 * teams actually chosen on the menu. Call once per match, before the
 * first player_draw(). */
void sprites_data_apply_teams(u8 teamAIndex, u8 teamBIndex);

#endif /* _SPRITES_DATA_H_ */
