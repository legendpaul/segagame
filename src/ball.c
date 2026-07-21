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
    b->spin = 0;
    b->state = heldState;
    b->spriteSlot = spriteSlot;
}

void ball_startThrow(Ball *b, s16 toX, s16 toY, BallState flightState, s8 spin)
{
    b->startX = b->x;
    b->startY = b->y;
    b->targetX = toX;
    b->targetY = toY;
    b->progress = 0;
    b->spin = spin;
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

    /* D-pad spin bends the ground track by up to 12px at mid-flight,
     * returning exactly to the requested lane at the target. */
    if (b->spin)
    {
        s32 t = b->progress;
        s32 curve = (12L * 4L * t * (255 - t)) / (255L * 255L);
        b->x += (s16)(curve * b->spin);
    }

    return (b->progress >= 255) ? TRUE : FALSE;
}

#define ARC_HEIGHT   22   /* px, peak visual lift at the midpoint of a throw */

void ball_draw(Ball *b)
{
    bool inFlight = (b->state == BALL_FLYING_TO_A) || (b->state == BALL_FLYING_TO_B);
    s16 drawY = b->y;
    u16 ballTile = TILE_BALL;

    if (inFlight)
    {
        /* Parabolic arc (0 at both ends, peak at the midpoint) layered
         * on top of the straight-line lerp, so the ball visibly lifts
         * off the ground and comes back down instead of sliding along
         * a flat 2D line. progress is 0..255. */
        s32 t = b->progress;
        s32 height = (ARC_HEIGHT * 4 * t * (255 - t)) / (255L * 255L);
        drawY = b->y - (s16)height;

        /* Four authored seam positions now make rotation readable. Spin
         * reverses their order instead of merely flipping a symmetric ball. */
        {
            u16 frame = (b->progress >> 4) & 3;
            if (b->spin < 0) frame = (4 - frame) & 3;
            ballTile += frame;
        }

        /* Shadow stays on the true ground track - the read on where
         * the ball will actually land. Links on to spriteSlot+2, the
         * controlled-player ground star (see scene_match.c) - the shadow is
         * no longer the last sprite in the chain. */
        VDP_setSpriteFull(b->spriteSlot + 1, b->x, b->y, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE,
                               (height > 12) ? TILE_BALL_SHADOW_AIR : TILE_BALL_SHADOW),
                           b->spriteSlot + 2);
    }
    else
    {
        /* Held: no shadow needed, park it off-screen rather than
         * leaving a stray dot under the holding player. */
        VDP_setSpriteFull(b->spriteSlot + 1, -16, -16, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, TILE_BALL_SHADOW),
                           b->spriteSlot + 2);
    }

    /* The ball links to the shadow, which now links on to the ground star
     * (see scene_match.c) so all three stay reachable from slot 0. */
    VDP_setSpriteFull(b->spriteSlot, b->x, drawY, SPRITE_SIZE(1, 1),
                       TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, ballTile),
                       b->spriteSlot + 1);
}
