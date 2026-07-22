/*
 * player.h - A court-side entity (human or CPU side), rendered as a
 * 32x32 near-side or 24x24 far-side hardware sprite, recolored per team
 * via its "pal" palette slot, with a small pose/animation system: idle, a
 * four-beat run/idle motion and brief throw, pickup and hit poses.
 */
#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "genesis.h"

typedef enum {
    POSE_STAND = 0,
    POSE_RUN,
    POSE_THROW,
    POSE_PICKUP,
    POSE_HIT
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
    u8  poseTimer;    /* frames left before a transient pose reverts */
    u8  animFrame;    /* 0..3 - run, idle and action animation phase */
    u8  animCounter;  /* frame counter that paces the run cycle */
    u8  small;        /* TRUE = render the dedicated 24x24 far-side size */
    bool farSide;     /* gameplay half, independent of visual sprite scale */
    bool facingLeft;  /* stable team direction: every player faces opposition */
} Player;

void player_init(Player *p, s16 startX, s16 y, u8 spriteSlot, u8 pal);
/* Removes the player from the round: parks them off-screen and marks
 * them un-targetable. */
void player_eliminate(Player *p);
/* Brings an eliminated player back into play at their home lane. */
void player_restore(Player *p);
void player_moveHuman(Player *p);
/* Keeps a player inside their half of the projected isometric court. */
void player_clampToCourt(Player *p);
/* Advances all action animation and, once any transient pose has timed
 * out, settles back to running or standing based on
 * "isMoving". Called every frame for both the human and the CPU side. */
void player_tickAnim(Player *p, bool isMoving);
/* Forces a transient action pose for "timer" frames. */
void player_setPose(Player *p, u8 pose, u8 timer);
void player_draw(Player *p);

#endif /* _PLAYER_H_ */
