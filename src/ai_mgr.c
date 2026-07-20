#include "ai_mgr.h"
#include "game_state.h"

#define AI_CATCH_CHANCE   70   /* out of 100 */
#define AI_AIM_SPREAD     40   /* +/- px inaccuracy */

u16 ai_pickThrowDelay(void)
{
    return AI_REACTION_MIN + (random() % AI_REACTION_VAR);
}

s16 ai_pickTargetX(s16 playerX)
{
    s16 offset = (s16)(random() % (AI_AIM_SPREAD * 2)) - AI_AIM_SPREAD;
    s16 target = playerX + offset;

    if (target < COURT_LEFT_X)  target = COURT_LEFT_X;
    if (target > COURT_RIGHT_X) target = COURT_RIGHT_X;

    return target;
}

bool ai_willCatch(void)
{
    return ((random() % 100) < AI_CATCH_CHANCE) ? TRUE : FALSE;
}
