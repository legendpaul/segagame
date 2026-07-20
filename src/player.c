#include "player.h"
#include "input_mgr.h"
#include "sprites_data.h"
#include "game_state.h"

#define RUN_FRAME_LEN   6   /* frames between run-cycle toggles */

#define OFFSCREEN_X   -100
#define OFFSCREEN_Y   -100

void player_init(Player *p, s16 startX, s16 y, u8 spriteSlot, u8 pal)
{
    p->x = startX;
    p->y = y;
    p->homeX = startX;
    p->homeY = y;
    p->eliminated = FALSE;
    p->spriteSlot = spriteSlot;
    p->pal = pal;
    p->pose = POSE_STAND;
    p->poseTimer = 0;
    p->animFrame = 0;
    p->animCounter = 0;
    p->small = FALSE;
}

void player_eliminate(Player *p)
{
    p->eliminated = TRUE;
    p->x = OFFSCREEN_X;
    p->y = OFFSCREEN_Y;
}

void player_restore(Player *p)
{
    p->eliminated = FALSE;
    p->x = p->homeX;
    p->y = p->homeY;
    p->pose = POSE_STAND;
    p->poseTimer = 0;
}

void player_moveHuman(Player *p)
{
    bool moved = FALSE;

    if (input_held(BUTTON_LEFT))  { p->x -= PLAYER_SPEED; moved = TRUE; }
    if (input_held(BUTTON_RIGHT)) { p->x += PLAYER_SPEED; moved = TRUE; }

    if (p->x < COURT_LEFT_X)  p->x = COURT_LEFT_X;
    if (p->x > COURT_RIGHT_X) p->x = COURT_RIGHT_X;

    player_tickAnim(p, moved);
}

void player_tickAnim(Player *p, bool isMoving)
{
    if (p->poseTimer > 0)
    {
        p->poseTimer--;
        return;
    }

    p->pose = isMoving ? POSE_RUN : POSE_STAND;

    if (p->pose == POSE_RUN)
    {
        if (++p->animCounter >= RUN_FRAME_LEN)
        {
            p->animCounter = 0;
            p->animFrame ^= 1;
        }
    }
    else
    {
        p->animCounter = 0;
    }
}

void player_setPose(Player *p, u8 pose, u8 timer)
{
    p->pose = pose;
    p->poseTimer = timer;
}

void player_draw(Player *p)
{
    /* Hardware sprites form a linked list starting at slot 0; a sprite
     * whose slot isn't reachable via some other sprite's "link" is never
     * rendered. We keep a fixed chain: slot N links to slot N+1. */
    if (p->small)
    {
        /* Genesis sprites can't be hardware-scaled, so the far/CPU
         * side's "further away, therefore smaller" look is faked with
         * a dedicated tiny 8x8 tile instead of the 16x16 pose set - no
         * per-pose animation at this scale, just the depth cue. +8 on Y
         * roughly aligns its "feet" with where the 16x16 sprite's feet
         * would have been, rather than anchoring from the same top-left. */
        VDP_setSpriteFull(p->spriteSlot, p->x, p->y + 8, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(p->pal, 0, FALSE, FALSE, TILE_PLAYER_SMALL),
                           p->spriteSlot + 1);
        return;
    }

    u16 base = TILE_PLAYER_STAND;
    bool flip = FALSE;

    /* All 4 poses currently alias the same 32x32 AI-derived tile block
     * (see sprites_data.c) - hflip during RUN still gives a cheap
     * side-to-side sway since the pose itself is asymmetric. */
    if (p->pose == POSE_RUN)
        flip = (p->animFrame != 0);

    VDP_setSpriteFull(p->spriteSlot, p->x - 8, p->y - 16, SPRITE_SIZE(4, 4),
                       TILE_ATTR_FULL(p->pal, 0, FALSE, flip, base),
                       p->spriteSlot + 1);
}
