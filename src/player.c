#include "player.h"
#include "input_mgr.h"
#include "sprites_data.h"
#include "game_state.h"

#define RUN_FRAME_LEN   6   /* frames between run-cycle toggles */

void player_init(Player *p, s16 startX, s16 y, u8 spriteSlot, u8 pal, u8 lives)
{
    p->x = startX;
    p->y = y;
    p->lives = lives;
    p->spriteSlot = spriteSlot;
    p->pal = pal;
    p->pose = POSE_STAND;
    p->poseTimer = 0;
    p->animFrame = 0;
    p->animCounter = 0;
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
    u16 base = TILE_PLAYER_STAND;
    bool flip = FALSE;

    switch (p->pose)
    {
        case POSE_RUN:
            base = TILE_PLAYER_RUN;
            /* Same lifted-leg art, mirrored: a front-facing "one leg
             * forward" pose flipped horizontally reads as the other
             * leg forward, giving a 2-frame gait from one tile. */
            flip = (p->animFrame != 0);
            break;
        case POSE_THROW:
            base = TILE_PLAYER_THROW;
            break;
        case POSE_CATCH:
            base = TILE_PLAYER_CATCH;
            break;
        default:
            break;
    }

    VDP_setSpriteFull(p->spriteSlot, p->x, p->y, SPRITE_SIZE(2, 2),
                       TILE_ATTR_FULL(p->pal, 0, FALSE, flip, base),
                       p->spriteSlot + 1);
}
