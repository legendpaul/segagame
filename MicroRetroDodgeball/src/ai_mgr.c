#include "ai_mgr.h"
#include "game_state.h"

#define AI_AIM_SPREAD     40   /* +/- px inaccuracy at NORMAL */

/* Wind-up delay before the CPU throws, scaled hard by difficulty so the three
 * levels feel clearly different: EASY dithers for well over a second before
 * lobbing one out, HARD fires almost immediately. */
u16 ai_pickThrowDelay(void)
{
    u16 base = AI_REACTION_MIN + (random() % AI_REACTION_VAR);   /* 20..49 */
    if (gDifficulty == DIFF_EASY)  return base + 70;             /* ~1.5-2.0s */
    if (gDifficulty == DIFF_HARD)  return (base > 30) ? (base - 24) : 5; /* snappy */
    return base;                                                 /* ~0.3-0.8s */
}

/* Aim inaccuracy, scaled by difficulty: EASY sprays wide and often misses the
 * target entirely, NORMAL is loose, HARD is near dead-on. */
s16 ai_pickTargetX(s16 playerX)
{
    s16 spread = AI_AIM_SPREAD;
    s16 offset, target;
    if (gDifficulty == DIFF_EASY)  spread = AI_AIM_SPREAD + 50;  /* very wild */
    else if (gDifficulty == DIFF_HARD) spread = 8;              /* laser */

    offset = (s16)(random() % (spread * 2)) - spread;
    target = playerX + offset;

    if (target < COURT_LEFT_X)  target = COURT_LEFT_X;
    if (target > COURT_RIGHT_X) target = COURT_RIGHT_X;

    return target;
}

/* How long the CPU hesitates before chasing a loose ball. On EASY it reacts
 * slowly, giving the human room; on HARD it pounces instantly. This is the
 * lever that makes retrieval battles feel easy or punishing. */
u16 ai_looseReactionFrames(void)
{
    if (gDifficulty == DIFF_EASY) return 36;   /* ~0.6s dither */
    if (gDifficulty == DIFF_HARD) return 0;    /* instant */
    return 8;                                  /* NORMAL reacts promptly */
}

u8 ai_pickSlot(u8 count)
{
    if (count <= 1) return 0;
    return (u8)(random() % count);
}
