/*
 * ball.h - The dodgeball entity: a lerp-based flight between two points
 * (thrower -> target), a parabolic height arc layered on top so it
 * visibly lifts off the ground mid-flight, plus held and loose states.
 * A completed throw can become a damped ricochet inside the court's
 * invisible plastic surround; it is never teleported to a player.
 *
 * The ball uses TWO hardware sprite slots: spriteSlot for the ball
 * itself (which moves up off its true y while airborne) and
 * spriteSlot+1 for a shadow that stays on the ground track, giving a
 * clear visual read on where the ball will land.
 */
#ifndef _BALL_H_
#define _BALL_H_

#include "genesis.h"

typedef enum {
    BALL_HELD_A = 0,    /* resting with the human player */
    BALL_HELD_B,        /* resting with the CPU */
    BALL_FLYING_TO_B,    /* travelling up toward the CPU */
    BALL_FLYING_TO_A,    /* travelling down toward the player */
    BALL_LOOSE           /* bouncing/rolling on the projected court */
} BallState;

typedef struct {
    s16 x, y;
    s16 startX, startY;
    s16 targetX, targetY;
    u16 progress;       /* 0..255 */
    s8 spin;            /* -1 left curve, 0 straight, +1 right curve */
    s32 preciseX, preciseY; /* 8.8 fixed-point loose-ball position */
    s16 velocityX, velocityY;
    s16 height, velocityZ;  /* 8.8 fixed-point bounce height/velocity */
    u8  bounceCount;
    bool looseFarSide;     /* receiving half owns the rebound box */
    BallState state;
    u8  spriteSlot;
} Ball;

#define BALL_STEP   9   /* progress added per frame; ~28 frames per flight */

void ball_init(Ball *b, u8 spriteSlot, s16 x, s16 y, BallState heldState);
void ball_startThrow(Ball *b, s16 toX, s16 toY, BallState flightState, s8 spin);
bool ball_update(Ball *b);   /* returns TRUE the frame it reaches its target */
void ball_startRicochet(Ball *b);
/* Converts an airborne hit into a loose ball placed exactly at the
 * victim's feet, retaining only a tiny spin kick and vertical bounce. */
void ball_dropAt(Ball *b, s16 x, s16 y);
bool ball_updateLoose(Ball *b); /* TRUE when the ball contacts floor/wall */
/* Visible flight Y, including the parabola used by ball_draw(). */
s16 ball_visualY(const Ball *b);
void ball_draw(Ball *b);

#endif /* _BALL_H_ */
