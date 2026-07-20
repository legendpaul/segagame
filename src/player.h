/*
 * player.h - A court-side entity (human or CPU side), rendered as a
 * 16x16 hardware sprite (a 2x2 tile block), recolored per team via its
 * "pal" palette slot, with a small pose/animation system: idle, a
 * 2-frame run cycle, and brief throw/catch poses.
 */
#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "genesis.h"

typedef enum {
    POSE_STAND = 0,
    POSE_RUN,
    POSE_THROW,
    POSE_CATCH
} PlayerPose;

typedef struct {
    s16 x;
    s16 y;
    s16 homeX;        /* lane position this slot returns to when brought back into play */
    s16 homeY;
    bool eliminated;  /* TRUE = out of the round; parked off-screen, no longer targetable */
    u8  spriteSlot;
    u8  pal;
    u8  pose;
    u8  poseTimer;    /* frames left before a transient pose (throw/catch) reverts */
    u8  animFrame;    /* 0/1 - drives the run cycle's mirrored second frame */
    u8  animCounter;  /* frame counter that paces the run cycle */
    u8  small;        /* TRUE = render as the tiny far-side sprite (no poses) */
} Player;

void player_init(Player *p, s16 startX, s16 y, u8 spriteSlot, u8 pal);
/* Removes the player from the round: parks them off-screen and marks
 * them un-targetable. */
void player_eliminate(Player *p);
/* Brings an eliminated player back into play at their home lane. */
void player_restore(Player *p);
void player_moveHuman(Player *p);
/* Advances the run-cycle animation and, once any transient pose (throw/
 * catch) has timed out, settles back to running or standing based on
 * "isMoving". Called every frame for both the human and the CPU side. */
void player_tickAnim(Player *p, bool isMoving);
/* Forces a transient pose (POSE_THROW / POSE_CATCH) for "timer" frames. */
void player_setPose(Player *p, u8 pose, u8 timer);
void player_draw(Player *p);

#endif /* _PLAYER_H_ */
