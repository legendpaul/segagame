/*
 * player.h - A court-side entity (human or CPU side), rendered as a
 * 32x32 near-side or 24x24 far-side hardware sprite, recolored per team
 * via its "pal" palette slot, with a small pose/animation system: idle, a
 * four-beat run/idle motion plus throw, pickup, impact, fall and
 * raised-fist celebration sequences.
 */
#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "genesis.h"

typedef enum {
    POSE_STAND = 0,
    POSE_RUN,
    POSE_THROW,
    POSE_PICKUP,
    POSE_HIT,
    POSE_FALL,
    POSE_CELEBRATE
} PlayerPose;

/* Every player state now resolves through eight animation beats. The previous
 * four-phase cycles repeated single silhouettes too abruptly, especially for
 * throws, pickups and falls. */
#define PLAYER_ANIM_FRAMES 8
#define PLAYER_ANIM_MASK   (PLAYER_ANIM_FRAMES - 1)

/* Shared projected ground-contact point used by movement, half-court target
 * generation and boundary clamping. Keep AI navigation on the player's feet,
 * never on the sprite's upper-left origin. */
#define PLAYER_FEET_DX   8
#define PLAYER_FEET_DY  16
#define PLAYER_HALF_W    8

typedef struct {
    s16 x;
    s16 y;
    s16 homeX;        /* lane position this slot returns to when brought back into play */
    s16 homeY;
    bool eliminated;  /* TRUE = out of the round and no longer targetable */
    bool exiting;     /* eliminated player is still visibly running off to the right */
    u8  spriteSlot;
    u8  pal;
    u8  pose;
    u8  poseTimer;    /* frames left before a transient pose reverts */
    u8  animFrame;    /* 0..7 - run, idle and action animation phase */
    u8  animCounter;  /* frame counter that paces the run cycle */
    u8  small;        /* TRUE = render the dedicated 24x24 far-side size */
    bool farSide;     /* gameplay half, independent of visual sprite scale */
    bool freeRoam;    /* TRUE = no centre net, may use the WHOLE court */
    /* Optional scene-local 16-tile pose block. Zero uses the normal shared
     * player art; Eliminator uses one recoloured slot per fighter. */
    u16  tileBaseOverride;
    bool facingLeft;  /* horizontal travel direction; mirrors current front/rear bank */
} Player;

void player_init(Player *p, s16 startX, s16 y, u8 spriteSlot, u8 pal);
/* Removes the player from targeting and starts their visible run-off. */
void player_eliminate(Player *p);
/* Advances an eliminated player's run to the right. Returns TRUE once
 * the complete sprite has cleared the screen and can be forgotten. */
bool player_updateExit(Player *p);
/* Brings an eliminated player back into play at their home lane. */
void player_restore(Player *p);
/* hasBall slows movement (see player.c) so the carrier reads as
 * slightly more vulnerable/committed than an off-ball defender. */
void player_moveHuman(Player *p, bool hasBall);
/* Keeps a player inside their half of the projected isometric court. */
void player_clampToCourt(Player *p);
/* Advances all action animation and, once any transient pose has timed
 * out, settles back to running or standing based on
 * "isMoving". Called every frame for both the human and the CPU side. */
void player_tickAnim(Player *p, bool isMoving);
/* Forces a transient action pose for "timer" frames. */
void player_setPose(Player *p, u8 pose, u8 timer);
/* Returns the exact 16-tile art block selected by pose/view/animation. */
u16 player_currentTileBase(const Player *p);
/* Returns the rendered feet / ground-contact point after the current pose's
 * authored pixel offsets have been applied. Depth sorting must use this
 * rather than the unshifted logical position, especially during hit/fall. */
void player_visualGround(const Player *p, s16 *x, s16 *y);
void player_draw(Player *p);

#endif /* _PLAYER_H_ */
