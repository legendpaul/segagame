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
 * STAND, RUN, THROW and CATCH each have their own AI-derived 32x32 block.
 * RUN mirrors its dedicated stride pose for a cheap second phase, while
 * THROW and CATCH use separate Pixel-Art-XL generations
 * (a real wind-up reach and a real diving/leaping grab), run through the
 * same flood-fill/crop/pad/quantize pipeline as the stand pose. Fixing
 * a ComfyUI request-encoding bug (PowerShell was writing a UTF-8 BOM
 * into the JSON body, which the server's JSON parser silently 500'd on)
 * is what unblocked generating these - see docs/planning.md. Each pose
 * still needs its own full 16-tile upload since Genesis hardware sprites
 * read one CONSECUTIVE tile block per draw call - swapping "pose" at
 * runtime just means player_draw() picks a different base tile index.
 */
#ifndef _SPRITES_DATA_H_
#define _SPRITES_DATA_H_

#include "genesis.h"

/* Player pose tile blocks - each a full 16 consecutive tiles (4x4, 32x32px) */
#define TILE_PLAYER_STAND   (TILE_USER_INDEX + 0)
#define TILE_PLAYER_THROW   (TILE_USER_INDEX + 16)
#define TILE_PLAYER_CATCH   (TILE_USER_INDEX + 32)
/* RUN used to alias STAND+hflip as a cheap sway - Qwen's top-ranked
 * graphics critique flagged that as reading like a placeholder ("probably
 * costing 80% of perceived polish"). It's now its own genuine 32x32 block:
 * a real Pixel-Art-XL mid-stride running pose run through the same
 * pipeline as THROW/CATCH (see docs/planning.md). player_draw() now keeps
 * the team-facing direction stable and uses a one-pixel body bob, while the base
 * silhouette itself is now real running art, not the idle pose. */
#define TILE_PLAYER_RUN     (TILE_USER_INDEX + 48)

#define TILE_BALL           (TILE_USER_INDEX + 64)
#define TILE_BALL_SHADOW    (TILE_USER_INDEX + 65)

/* Dedicated 24x24 (3x3 tile) far-side size. Genesis sprites cannot be
 * hardware-scaled, so this is a separately encoded reduction of the
 * real STAND artwork rather than a runtime transform. */
#define TILE_PLAYER_FAR_STAND (TILE_USER_INDEX + 66)
#define TILE_PLAYER_FAR_RUN   (TILE_USER_INDEX + 75)
#define TILE_PLAYER_FAR_THROW (TILE_USER_INDEX + 84)
#define TILE_PLAYER_FAR_CATCH (TILE_USER_INDEX + 93)

/* Two-tile ground stars: yellow identifies the controlled player and red
 * identifies possession/wind-up. They use PAL_BALL so kit recolouring
 * never changes their meaning. */
#define TILE_MARKER_YELLOW  (TILE_USER_INDEX + 102) /* two 8x8 tiles */
#define TILE_MARKER_RED     (TILE_USER_INDEX + 104) /* two 8x8 tiles */

/* First tile index free for court_bg.c to use */
#define TILE_COURT_BASE     (TILE_USER_INDEX + 106)

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

/* Briefly whites-out a team's kit-ramp colors for an impact "flash" on
 * catches/hits, the same visual feedback trick real sports games use.
 * Only 4 palette lines exist total (PAL0 court/font, PAL1/PAL2 teams,
 * PAL3 ball) so this flashes the whole team's shared palette, not just
 * the one player involved - an honest hardware-budget tradeoff, not a
 * per-sprite effect. Caller is responsible for restoring real colors
 * a few frames later via sprites_data_apply_teams(). */
void sprites_data_flash_team(u8 palLine);

#endif /* _SPRITES_DATA_H_ */
