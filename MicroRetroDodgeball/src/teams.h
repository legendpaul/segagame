/*
 * teams.h - National-team roster, ordered by the 20 July 2026 FIFA
 * men's world ranking.
 */
#ifndef _TEAMS_H_
#define _TEAMS_H_

#include <types.h>

#define NUM_TEAMS 10

extern const char* const teamNames[NUM_TEAMS];

/* Tournament opponent for a stage (0-based), skipping the player's team. */
u8 cup_opponent(u8 playerTeam, u8 stage);

#endif /* _TEAMS_H_ */
