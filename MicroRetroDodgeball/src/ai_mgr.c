#include "ai_mgr.h"
#include "game_state.h"

#define AI_AIM_SPREAD     40   /* +/- px inaccuracy at NORMAL */

/* Wind-up delay before the CPU throws, scaled by difficulty: EASY dawdles,
 * HARD snaps the ball out fast. */
u16 ai_pickThrowDelay(void)
{
    u16 base = AI_REACTION_MIN + (random() % AI_REACTION_VAR);
    if (gDifficulty == DIFF_EASY)  return base + 45;
    if (gDifficulty == DIFF_HARD)  return (base > 22) ? (base - 18) : 6;
    return base;
}

/* Aim inaccuracy, scaled by difficulty: EASY sprays wide, HARD is sharp. */
s16 ai_pickTargetX(s16 playerX)
{
    s16 spread = AI_AIM_SPREAD;
    s16 offset, target;
    if (gDifficulty == DIFF_EASY)  spread = AI_AIM_SPREAD + 34;
    else if (gDifficulty == DIFF_HARD) spread = 12;

    offset = (s16)(random() % (spread * 2)) - spread;
    target = playerX + offset;

    if (target < COURT_LEFT_X)  target = COURT_LEFT_X;
    if (target > COURT_RIGHT_X) target = COURT_RIGHT_X;

    return target;
}

u8 ai_pickSlot(u8 count)
{
    if (count <= 1) return 0;
    return (u8)(random() % count);
}
