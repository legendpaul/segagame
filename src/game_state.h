/*
 * game_state.h - Shared constants, enums and cross-scene globals.
 *
 * Part of MEGA DODGEBALL, a Sega Mega Drive game.
 * See docs/planning.md for the original design brief.
 */
#ifndef _GAME_STATE_H_
#define _GAME_STATE_H_

#include "genesis.h"

/* --- Screen / court geometry --- */
#define SCREEN_W            320
#define SCREEN_H            224

/* Isometric screen-space court. A player's depth is y - x/4; moving
 * along either diagonal axis changes both screen coordinates. */
#define COURT_FAR_DEPTH      24
#define COURT_NEAR_DEPTH     144
#define COURT_CENTER_DEPTH   84
#define TEAM_B_DEPTH         52
#define TEAM_A_DEPTH         116
#define COURT_LEFT_X         8
#define COURT_RIGHT_X        304

/* --- Gameplay tuning --- */
#define PLAYER_SPEED        2       /* px per frame */
#define CATCH_WINDOW_X      16      /* horizontal alignment needed to catch */
#define CATCH_WINDOW_Y      12      /* screen-depth alignment needed to catch */
#define AI_REACTION_MIN     20      /* frames CPU waits before throwing */
#define AI_REACTION_VAR     30

#define TEAM_SIZE           3       /* real dodgeball squad size per side */
#define WIN_SCORE           3       /* rounds (full-team eliminations) to win the match */

/* --- Scene management --- */
typedef enum {
    GS_BOOT = 0,
    GS_MENU,
    GS_MATCH,
    GS_GAMEOVER
} GameScene;

extern GameScene gCurrentScene;

/* Persist across scenes */
extern u8 gTeamAIndex;     /* player's team */
extern u8 gTeamBIndex;     /* CPU's team */
extern u8 gScoreA;
extern u8 gScoreB;

#endif /* _GAME_STATE_H_ */
