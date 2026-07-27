#include "teams.h"

const char* const teamNames[NUM_TEAMS] = {
    "SPAIN",
    "ARGENTINA",
    "FRANCE",
    "ENGLAND",
    "BRAZIL",
    "MOROCCO",
    "PORTUGAL",
    "BELGIUM",
    "NETHERLANDS",
    "MEXICO"
};

u8 cup_opponent(u8 playerTeam, u8 stage)
{
    /* Tournament gauntlet opponent for a given stage: the teams after the
     * player's own, in order, wrapping and always skipping the player's. */
    u8 idx = (u8)((playerTeam + 1 + stage) % NUM_TEAMS);
    if (idx == playerTeam) idx = (u8)((idx + 1) % NUM_TEAMS);
    return idx;
}
