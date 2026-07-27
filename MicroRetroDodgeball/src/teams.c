#include "genesis.h"
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

u8 cupQF[CUP_TEAMS];
u8 cupSF[4];
u8 cupF[2];
u8 cupChampion;

/* Slots still contested in a given round: 8, 4 then 2. */
static u8 *round_slots(u8 round, u8 *count)
{
    if (round == 0) { *count = CUP_TEAMS; return cupQF; }
    if (round == 1) { *count = 4;         return cupSF; }
    *count = 2;                            return cupF;
}

void cup_build(u8 playerTeam)
{
    u8 pool[NUM_TEAMS];
    u8 poolCount = 0, i, slot;

    for (i = 0; i < NUM_TEAMS; i++)
        if (i != playerTeam) pool[poolCount++] = i;

    /* Shuffle the rivals so every cup run has a different draw. */
    for (i = poolCount; i > 1; i--)
    {
        u8 j = (u8)(random() % i);
        u8 tmp = pool[i - 1]; pool[i - 1] = pool[j]; pool[j] = tmp;
    }

    /* The player takes a random bracket slot; rivals fill the rest, so the
     * player is not always in the same tie. */
    slot = (u8)(random() % CUP_TEAMS);
    poolCount = 0;
    for (i = 0; i < CUP_TEAMS; i++)
        cupQF[i] = (i == slot) ? playerTeam : pool[poolCount++];

    for (i = 0; i < 4; i++) cupSF[i] = CUP_TBD;
    cupF[0] = cupF[1] = CUP_TBD;
    cupChampion = CUP_TBD;
}

u8 cup_opponent_now(u8 playerTeam, u8 round)
{
    u8 count, i;
    u8 *slots = round_slots(round, &count);
    for (i = 0; i < count; i++)
        if (slots[i] == playerTeam)
            return slots[i ^ 1];    /* pair partner: 0-1, 2-3, 4-5, 6-7 */
    return slots[0];
}

void cup_advance(u8 playerTeam, u8 round)
{
    u8 count, i, w = 0;
    u8 *slots = round_slots(round, &count);
    u8 *next = (round == 0) ? cupSF : (round == 1) ? cupF : &cupChampion;

    for (i = 0; i < count; i += 2)
    {
        u8 a = slots[i], b = slots[i + 1];
        u8 winner;
        if (a == playerTeam || b == playerTeam)
            winner = playerTeam;              /* the player just won this tie */
        else
            winner = (random() & 1) ? a : b;  /* simulate the other ties */
        next[w++] = winner;
    }
}

bool cup_is_out(u8 team, u8 round)
{
    /* A team is out if it appeared in an earlier round but not in the current
     * one. Rounds not yet played are still all-TBD, so nobody reads as out. */
    u8 count, i, r;
    for (r = 0; r <= round && r < CUP_ROUNDS; r++)
    {
        u8 *slots = round_slots(r, &count);
        for (i = 0; i < count; i++) if (slots[i] == team) break;
        if (i == count) return TRUE;   /* missing from a round it should be in */
    }
    return FALSE;
}
