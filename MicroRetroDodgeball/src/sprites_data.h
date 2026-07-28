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
 * STAND, RUN, THROW and PICKUP each have their own 32x32 block.
 * RUN mirrors its dedicated stride pose for a cheap second phase, while
 * THROW and PICKUP use separate Pixel-Art-XL generations
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
#define TILE_PLAYER_FRONT_STAND    (TILE_USER_INDEX + 0)
#define TILE_PLAYER_FRONT_RUN      (TILE_USER_INDEX + 16)
#define TILE_PLAYER_FRONT_RUN_ALT  (TILE_USER_INDEX + 32)
#define TILE_PLAYER_FRONT_THROW    (TILE_USER_INDEX + 48)
#define TILE_PLAYER_FRONT_PICKUP   (TILE_USER_INDEX + 64)

/* Separate rear three-quarter silhouettes let the near team genuinely face
 * up-court.  These are authored pixel blocks, not horizontal flips of faces. */
#define TILE_PLAYER_BACK_STAND     (TILE_USER_INDEX + 80)
#define TILE_PLAYER_BACK_RUN       (TILE_USER_INDEX + 96)
#define TILE_PLAYER_BACK_RUN_ALT   (TILE_USER_INDEX + 112)
#define TILE_PLAYER_BACK_THROW     (TILE_USER_INDEX + 128)

/* Compatibility names for non-match previews. */
#define TILE_PLAYER_STAND   TILE_PLAYER_FRONT_STAND
#define TILE_PLAYER_THROW   TILE_PLAYER_FRONT_THROW
#define TILE_PLAYER_PICKUP  TILE_PLAYER_FRONT_PICKUP
/* RUN used to alias STAND+hflip as a cheap sway - Qwen's top-ranked
 * graphics critique flagged that as reading like a placeholder ("probably
 * costing 80% of perceived polish"). It's now its own genuine 32x32 block:
 * a real Pixel-Art-XL mid-stride running pose run through the same
 * pipeline as the other actions (see docs/planning.md). player_draw() now keeps
 * the team-facing direction stable and uses a one-pixel body bob, while the base
 * silhouette itself is now real running art, not the idle pose. */
#define TILE_PLAYER_RUN     TILE_PLAYER_FRONT_RUN

#define TILE_BALL_SHADOW     (TILE_USER_INDEX + 144)
#define TILE_BALL_SHADOW_AIR (TILE_USER_INDEX + 145)

/* Four complete 16x16 rotation frames (four tiles each), ready for the ball
 * renderer to select without mirroring a symmetrical image. */
#define TILE_BALL16_FRAME_0 (TILE_USER_INDEX + 146)
#define TILE_BALL16_FRAME_1 (TILE_USER_INDEX + 150)
#define TILE_BALL16_FRAME_2 (TILE_USER_INDEX + 154)
#define TILE_BALL16_FRAME_3 (TILE_USER_INDEX + 158)

/* 24x16 elliptical selection rings (3x2 hardware tiles). */
#define TILE_RING_YELLOW    (TILE_USER_INDEX + 162)
#define TILE_RING_RED       (TILE_USER_INDEX + 168)

#define TILE_PLAYER_FRONT_HIT       (TILE_USER_INDEX + 174)
#define TILE_PLAYER_FRONT_FALL      (TILE_USER_INDEX + 190)
#define TILE_PLAYER_FRONT_CELEBRATE (TILE_USER_INDEX + 206)
#define TILE_PLAYER_BACK_HIT        (TILE_USER_INDEX + 222)
#define TILE_PLAYER_BACK_FALL       (TILE_USER_INDEX + 238)
#define TILE_PLAYER_BACK_CELEBRATE  (TILE_USER_INDEX + 254)
#define TILE_PLAYER_FRONT_RUN_PASS  (TILE_USER_INDEX + 270)
#define TILE_PLAYER_BACK_RUN_PASS   (TILE_USER_INDEX + 286)
#define TILE_BALL_HELD              (TILE_USER_INDEX + 302)

/* First tile index free for court_bg.c to use */
#define TILE_COURT_BASE     (TILE_USER_INDEX + 303)

/* Shared impact art occupies the eight-tile gap immediately after the
 * referee and before the standard-match foreground bank. It must not share
 * the small-flag bank: those flags remain visible in the gameplay HUD. */
#define TILE_IMPACT_BURST   (TILE_USER_INDEX + 869)
#define TILE_IMPACT_GREY    (TILE_IMPACT_BURST + 4)

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

/* GLOBAL ELIMINATOR spreads ten compact national kits across PAL1/PAL2/PAL3.
 * Each fighter owns one scene-local tile slot refreshed when its pose changes.
 * Poses retain the Exhibition hair, boots and near-black outline; only the
 * country-dependent kit ramp is compacted. */
void sprites_data_load_eliminator_player(u8 teamIndex, u16 sourceBase,
                                         u16 destination);
void sprites_data_apply_eliminator_teams(void);
void sprites_data_load_eliminator_ball_art(void);
/* Rebuilds the two 24x16 control rings with a faint four-phase interior glow.
 * Call only when the phase changes; the marker remains one ground sprite. */
void sprites_data_set_ring_pulse(u8 phase, bool eliminator);
void sprites_data_load_impact_art(void);

/* Builds four front and four rear official frames from the same authored
 * 32x32 player anatomy. Stand/run groups use scene-local VRAM gaps. */
void sprites_data_load_referee_art(u16 frontStandDestination,
                                   u16 frontRunDestination,
                                   u16 backStandDestination,
                                   u16 backRunDestination);

/* Eliminator owns one 16-tile pose cache per fighter. Keep all ten caches in
 * one contiguous scene-local bank above the 75-tile roof foreground and below
 * TILE_UI_BASE. The title/flag banks do not coexist with live Eliminator play,
 * so they are safe to reuse; the shared HUD font is not. */
#define TILE_ELIM_PLAYER_LOW_BASE  (TILE_USER_INDEX + 1000)

/* Eliminator-only ball and ring art follows all ten player slots. This
 * deliberately overlaps the boot/menu logo and flag bank, which is absent in
 * Eliminator. The shared impact art above cannot use this range because the
 * standard-match HUD keeps its small flags resident during gameplay. */
#define TILE_ELIM_EFFECT_BASE     (TILE_ELIM_PLAYER_LOW_BASE + 10 * 16)
#define TILE_ELIM_BALL_SHADOW     (TILE_ELIM_EFFECT_BASE + 0)
#define TILE_ELIM_BALL_SHADOW_AIR (TILE_ELIM_EFFECT_BASE + 1)
#define TILE_ELIM_BALL16_FRAME_0  (TILE_ELIM_EFFECT_BASE + 2)
#define TILE_ELIM_RING_YELLOW     (TILE_ELIM_EFFECT_BASE + 18)
#define TILE_ELIM_RING_RED        (TILE_ELIM_EFFECT_BASE + 24)

/* Briefly whites-out a team's kit-ramp colors for an impact flash.
 * Only 4 palette lines exist total (PAL0 court/font, PAL1/PAL2 teams,
 * PAL3 ball) so this flashes the whole team's shared palette, not just
 * the one player involved - an honest hardware-budget tradeoff, not a
 * per-sprite effect. Caller is responsible for restoring real colors
 * a few frames later via sprites_data_apply_teams(). */
void sprites_data_flash_team(u8 palLine);

/* Writes the 3-shade kit ramp (light, mid, dark) for a team into out[0..2]
 * as VDP colour words. Used by the matchup screen's recolourable figures. */
void sprites_data_kit_ramp(u8 teamIndex, u16 *out);

/* Parks every gameplay sprite slot off-screen with a terminating link chain,
 * so stale match sprites (players/ball/shadows) can't bleed onto a
 * non-gameplay screen. Call on entry to menu/matchup/game-over. */
void sprites_data_hide_all_sprites(void);

#endif /* _SPRITES_DATA_H_ */
