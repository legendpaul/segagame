/*
 * ball.h - The dodgeball entity: a lerp-based flight between two points
 * (thrower -> target) plus simple "held" states.
 */
#ifndef _BALL_H_
#define _BALL_H_

#include "genesis.h"

typedef enum {
    BALL_HELD_A = 0,    /* resting with the human player */
    BALL_HELD_B,        /* resting with the CPU */
    BALL_FLYING_TO_B,    /* travelling up toward the CPU */
    BALL_FLYING_TO_A     /* travelling down toward the player */
} BallState;

typedef struct {
    s16 x, y;
    s16 startX, startY;
    s16 targetX, targetY;
    u16 progress;       /* 0..255 */
    BallState state;
    u8  spriteSlot;
} Ball;

#define BALL_STEP   9   /* progress added per frame; ~28 frames per flight */

void ball_init(Ball *b, u8 spriteSlot, s16 x, s16 y, BallState heldState);
void ball_startThrow(Ball *b, s16 toX, s16 toY, BallState flightState);
bool ball_update(Ball *b);   /* returns TRUE the frame it reaches its target */
void ball_draw(Ball *b);

#endif /* _BALL_H_ */
