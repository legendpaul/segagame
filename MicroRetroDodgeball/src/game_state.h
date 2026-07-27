/*
 * game_state.h - Shared constants, enums and cross-scene globals.
 *
 * Part of MICRO RETRO DODGEBALL, a Sega Mega Drive game.
 * See docs/planning.md for the original design brief.
 */
#ifndef _GAME_STATE_H_
#define _GAME_STATE_H_

#include "genesis.h"

/* --- Screen / court geometry --- */
#define SCREEN_W            320
#define SCREEN_H            224

/* Isometric screen-space court. The ONE projection parameter is the screen
 * skew: how much screen-Y rises per unit of screen-X across the court. Depth
 * (distance up the court) is therefore  y - skew(x), and a point at a given
 * depth sits at screen-y = depth + skew(x). Centralising it here means the
 * whole coordinate system - players, ball, spawns, AI aim and the court art -
 * derives the angle from these macros, so the pitch angle is tunable in one
 * place. COURT_SKEW_SHIFT 2 == the historical x/4 shear; a smaller shift makes
 * a steeper, more FIFA-like angle. */
#define COURT_SKEW_SHIFT     2
#define COURT_SKEW(x)        ((x) >> COURT_SKEW_SHIFT)
#define COURT_DEPTH_OF(x, y) ((y) - COURT_SKEW(x))

/* Sideline drift with depth (how the parallel touchlines lean). */
#define COURT_SIDE_SHIFT     1

#define COURT_FAR_DEPTH      24
#define COURT_NEAR_DEPTH     144
#define COURT_CENTER_DEPTH   84
#define TEAM_B_DEPTH         52
#define TEAM_A_DEPTH         116
#define COURT_LEFT_X         8
#define COURT_RIGHT_X        304

/* Projected side rails used by players, loose-ball physics and authored art. */
#define COURT_MIN_X_AT_DEPTH(d) (64 - (((d) - COURT_FAR_DEPTH) >> COURT_SIDE_SHIFT))
#define COURT_MAX_X_AT_DEPTH(d) (312 - (((d) - COURT_FAR_DEPTH) >> COURT_SIDE_SHIFT))
#define COURT_Y_AT_DEPTH_X(d, x) ((d) + COURT_SKEW(x))

/* --- Gameplay tuning --- */
#define PLAYER_SPEED        2       /* px per frame */
#define HIT_WINDOW_X        13      /* airborne torso collision half-width */
#define HIT_WINDOW_Y        10      /* airborne torso collision half-height */
#define PICKUP_WINDOW_X     11      /* feet must physically reach loose ball */
#define PICKUP_WINDOW_Y      9
#define AI_REACTION_MIN     20      /* frames CPU waits before throwing */
#define AI_REACTION_VAR     30

#define TEAM_SIZE           3       /* real dodgeball squad size per side */
#define WIN_SCORE           3       /* rounds (full-team eliminations) to win the match */

/* --- Scene management --- */
typedef enum {
    GS_BOOT = 0,
    GS_MENU,
    GS_ELIMINATOR,
    GS_MATCH,
    GS_GAMEOVER
} GameScene;

extern GameScene gCurrentScene;

/* --- Game modes / options --- */
typedef enum {
    MODE_EXHIBITION = 0,   /* pick both teams, single match */
    MODE_TOURNAMENT,       /* pick your team, gauntlet the rest to be champion */
    MODE_ELIMINATOR,       /* every nation, one player each, no net, 3 balls */
    MODE_COUNT
} GameMode;

typedef enum {
    DIFF_EASY = 0,
    DIFF_NORMAL,
    DIFF_HARD
} Difficulty;

/* The cup is an 8-team single-elimination bracket: see CUP_ROUNDS in teams.h
 * (quarter-final, semi-final, final). gCupStage is the current round, 0..2. */

/* Persist across scenes */
extern u8 gTeamAIndex;     /* player's team */
extern u8 gTeamBIndex;     /* CPU's team */
extern u8 gScoreA;
extern u8 gScoreB;

extern u8 gGameMode;       /* GameMode */
extern u8 gDifficulty;     /* Difficulty */
extern u8 gCupStage;       /* 0-based opponent index in tournament mode */

/* How GS_MENU should present itself when (re)entered. Lets a mid-cup match
 * win return to the tournament ladder instead of the title screen. */
typedef enum {
    MENU_ENTRY_TITLE = 0,
    MENU_ENTRY_CUP_LADDER
} MenuEntry;
extern u8 gMenuEntry;      /* MenuEntry */

#endif /* _GAME_STATE_H_ */
