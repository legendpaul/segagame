#include "ball.h"
#include "sprites_data.h"

void ball_init(Ball *b, u8 spriteSlot, s16 x, s16 y, BallState heldState)
{
    b->x = x;
    b->y = y;
    b->startX = x;
    b->startY = y;
    b->targetX = x;
    b->targetY = y;
    b->progress = 0;
    b->state = heldState;
    b->spriteSlot = spriteSlot;
}

void ball_startThrow(Ball *b, s16 toX, s16 toY, BallState flightState)
{
    b->startX = b->x;
    b->startY = b->y;
    b->targetX = toX;
    b->targetY = toY;
    b->progress = 0;
    b->state = flightState;
}

bool ball_update(Ball *b)
{
    if ((b->state != BALL_FLYING_TO_A) && (b->state != BALL_FLYING_TO_B))
        return FALSE;

    if (b->progress >= 255)
    {
        b->x = b->targetX;
        b->y = b->targetY;
        return TRUE;
    }

    b->progress += BALL_STEP;
    if (b->progress > 255) b->progress = 255;

    b->x = b->startX + (s16)(((s32)(b->targetX - b->startX) * b->progress) / 255);
    b->y = b->startY + (s16)(((s32)(b->targetY - b->startY) * b->progress) / 255);

    return (b->progress >= 255) ? TRUE : FALSE;
}

void ball_draw(Ball *b)
{
    /* Last sprite in the link chain (see player_draw()): link = 0 ends it. */
    VDP_setSpriteFull(b->spriteSlot, b->x, b->y, SPRITE_SIZE(1, 1),
                       TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, TILE_BALL),
                       0);
}
