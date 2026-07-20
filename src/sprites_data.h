/*
 * sprites_data.h - Player/ball tile art, plus the palettes used to
 * recolor the same shapes per team.
 *
 * Player art is a full 4x4 hardware sprite block (32x32px, the actual
 * max single-sprite size the Genesis VDP supports) - not the earlier
 * 2x2 (16x16px) block. That earlier size was the real problem with the
 * first AI-art pass: a 512x512 AI-generated reference collapsed into a
 * 16x16 sprite lost almost all of its detail and read as a small blob
 * in-game. At 32x32 the same source detail (head, raised arm, jersey
 * shading, lunging legs) actually survives on screen. A Genesis NxM
 * hardware sprite always reads N*M CONSECUTIVE VRAM tiles in
 * column-major order (col0 top-to-bottom, then col1, ...) starting at
 * one base index - you cannot mix tiles from different blocks at
 * runtime, so the full 16-tile block is uploaded as one unit.
 *
 * All 4 poses (stand/run/throw/catch) currently share this one AI-
 * derived 32x32 block - see the honest note in sprites_data.c and
 * docs/planning.md about why per-pose art isn't split out yet.
 */
#ifndef _SPRITES_DATA_H_
#define _SPRITES_DATA_H_

#include "genesis.h"

/* Player pose tile block - 16 consecutive tiles (4x4, 32x32px) */
#define TILE_PLAYER_STAND   (TILE_USER_INDEX + 0)
#define TILE_PLAYER_RUN     TILE_PLAYER_STAND
#define TILE_PLAYER_THROW   TILE_PLAYER_STAND
#define TILE_PLAYER_CATCH   TILE_PLAYER_STAND

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
