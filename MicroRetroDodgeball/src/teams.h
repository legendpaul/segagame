/*
 * teams.h - National-team roster, ordered by the 20 July 2026 FIFA
 * men's world ranking.
 */
#ifndef _TEAMS_H_
#define _TEAMS_H_

#include <types.h>

#define NUM_TEAMS 10

extern const char* const teamNames[NUM_TEAMS];

/* --- 8-team single-elimination cup ------------------------------------- *
 * Three rounds: quarter-final (8), semi-final (4), final (2). The bracket is
 * fixed at draw time; each round the player plays their pair partner and the
 * remaining pairs are simulated, so the bracket screen can always show every
 * team, who has been knocked out, and who is still in. */
#define CUP_TEAMS      8
#define CUP_ROUNDS     3
#define CUP_TBD     0xFF   /* slot not decided yet */

extern u8 cupQF[CUP_TEAMS];   /* the draw, in bracket order */
extern u8 cupSF[4];           /* quarter-final winners */
extern u8 cupF[2];            /* semi-final winners */
extern u8 cupChampion;

/* Draw a fresh bracket containing the player's team plus 7 random rivals. */
void cup_build(u8 playerTeam);
/* The player's opponent in the current round (round 0..2). */
u8   cup_opponent_now(u8 playerTeam, u8 round);
/* Record the player's win in this round and simulate the other ties. */
void cup_advance(u8 playerTeam, u8 round);
/* TRUE once this team has lost and is out of the competition. */
bool cup_is_out(u8 team, u8 round);

#endif /* _TEAMS_H_ */
