/*
 * sprites_data.h - Hand-authored tile art for player and ball sprites,
 * plus the palettes used to recolor the same shapes per team.
 *
 * No image assets / resource compiler needed: tiles are plain 8x8 4bpp
 * nibble-packed arrays, uploaded straight to VRAM at boot.
 *
 * A Genesis 2x2 hardware sprite always reads 4 CONSECUTIVE VRAM tiles
 * (TL,BL,TR,BR) starting at one base index - you cannot mix quadrants
 * from different tile blocks at runtime. So each player *pose* (stand,
 * run, throw, catch) gets its own full 4-tile block; poses that share
 * most of a standing player's shape just re-upload the same source
 * array into a different quadrant slot, only 3 quadrants are genuinely
 * new art (run legs, throw arm, catch arm - see sprites_data.c). The
 * mirrored run frame is free: it's the same block shown with hardware
 * hflip, since a front-facing "one leg forward" pose flips into "other
 * leg forward" and gives a proper 2-frame gait for one extra tile.
 */
#ifndef _SPRITES_DATA_H_
#define _SPRITES_DATA_H_

#include "genesis.h"

/* Player pose tile blocks - each is 4 consecutive tiles (2x2, 16x16px) */
#define TILE_PLAYER_STAND   (TILE_USER_INDEX + 0)
#define TILE_PLAYER_RUN     (TILE_USER_INDEX + 4)
#define TILE_PLAYER_THROW   (TILE_USER_INDEX + 8)
#define TILE_PLAYER_CATCH   (TILE_USER_INDEX + 12)

#define TILE_BALL           (TILE_USER_INDEX + 16)
#define TILE_BALL_SHADOW    (TILE_USER_INDEX + 17)

/* A single small 8x8 sprite (no pose variants) used only for the far
 * (CPU) side. Genesis sprites can't be hardware-scaled, so this fakes
 * the "far player reads smaller than the near player" depth cue real
 * elevated-camera sports games (FIFA International Soccer included)
 * rely on, by hand-authoring a separate tiny tile instead of scaling
 * the 16x16 one. Paired with court_bg.c's tapered sidelines for a
 * consistent perspective illusion. */
#define TILE_PLAYER_SMALL   (TILE_USER_INDEX + 18)

/* First tile index free for court_bg.c to use */
#define TILE_COURT_BASE     (TILE_USER_INDEX + 19)

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
