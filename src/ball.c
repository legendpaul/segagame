#include "ball.h"
#include "sprites_data.h"
#include "game_state.h"

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
    b->preciseX = (s32)x << 8;
    b->preciseY = (s32)y << 8;
    b->velocityX = 0;
    b->velocityY = 0;
    b->height = 0;
    b->velocityZ = 0;
    b->bounceCount = 0;
    b->looseFarSide = FALSE;
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

    /* Spin produces a bow plus a late break. The throw still starts toward
     * its fixed lane point, but enough spin can carry it past every body;
     * this is gameplay geometry, not a cosmetic sprite flip. */
    if (b->spin)
    {
        s32 t = b->progress;
        s32 bow = (12L * 4L * t * (255 - t)) / (255L * 255L);
        s32 lateBreak = (20L * t * t) / (255L * 255L);
        s32 curve = bow + lateBreak;
        b->x += (s16)(curve * b->spin);
    }

    return (b->progress >= 255) ? TRUE : FALSE;
}

void ball_startRicochet(Ball *b)
{
    s16 incomingX = b->targetX - b->startX;
    s16 incomingY = b->targetY - b->startY;

    b->looseFarSide = (b->state == BALL_FLYING_TO_B);
    b->state = BALL_LOOSE;
    b->preciseX = (s32)b->x << 8;
    b->preciseY = (s32)b->y << 8;
    /* Reverse a restrained portion of the incoming motion. The extra
     * lateral component preserves the chosen spin after the throw lands. */
    b->velocityX = (s16)(-(incomingX * 256L) / 56L + b->spin * 96);
    b->velocityY = (s16)(-(incomingY * 256L) / 56L);
    b->height = 3 << 8;
    b->velocityZ = 2 << 8;
    b->bounceCount = 0;
}

void ball_dropAt(Ball *b, s16 x, s16 y)
{
    bool landedFarSide = (b->state == BALL_FLYING_TO_B);

    b->x = x;
    b->y = y;
    b->looseFarSide = landedFarSide;
    b->state = BALL_LOOSE;
    b->preciseX = (s32)x << 8;
    b->preciseY = (s32)y << 8;
    b->velocityX = b->spin * 48;
    b->velocityY = 0;
    b->height = 2 << 8;
    b->velocityZ = 192;
    b->bounceCount = 1;
}

bool ball_updateLoose(Ball *b)
{
    bool contact = FALSE;
    s16 depth;
    const s16 minDepth = b->looseFarSide ? COURT_FAR_DEPTH + 10
                                         : COURT_CENTER_DEPTH + 6;
    const s16 maxDepth = b->looseFarSide ? COURT_CENTER_DEPTH - 6
                                         : COURT_NEAR_DEPTH - 10;

    if (b->state != BALL_LOOSE) return FALSE;

    b->preciseX += b->velocityX;
    b->preciseY += b->velocityY;
    b->x = (s16)(b->preciseX >> 8);
    b->y = (s16)(b->preciseY >> 8);

    /* Clamp depth first, then derive the sloping side rails from that same
     * projected depth. This is the exact quadrilateral painted by the court
     * converter rather than the old screen-aligned invisible rectangle. */
    depth = b->y - (b->x >> 2);
    if (depth < minDepth)
    {
        b->y = minDepth + (b->x >> 2);
        b->preciseY = (s32)b->y << 8;
        b->velocityY = (s16)(-b->velocityY * 3 / 4);
        contact = TRUE;
    }
    else if (depth > maxDepth)
    {
        b->y = maxDepth + (b->x >> 2);
        b->preciseY = (s32)b->y << 8;
        b->velocityY = (s16)(-b->velocityY * 3 / 4);
        contact = TRUE;
    }

    depth = b->y - (b->x >> 2);
    {
        const s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
        const s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;
        if (b->x < minX)
        {
            b->x = minX;
            b->y = depth + (b->x >> 2);
            b->preciseX = (s32)b->x << 8;
            b->preciseY = (s32)b->y << 8;
            b->velocityX = (s16)(-b->velocityX * 3 / 4);
            contact = TRUE;
        }
        else if (b->x > maxX)
        {
            b->x = maxX;
            b->y = depth + (b->x >> 2);
            b->preciseX = (s32)b->x << 8;
            b->preciseY = (s32)b->y << 8;
            b->velocityX = (s16)(-b->velocityX * 3 / 4);
            contact = TRUE;
        }
    }
    b->velocityX = (s16)(b->velocityX * 31 / 32);
    b->velocityY = (s16)(b->velocityY * 31 / 32);
    if (abs(b->velocityX) < 24) b->velocityX = 0;
    if (abs(b->velocityY) < 24) b->velocityY = 0;

    b->height += b->velocityZ;
    b->velocityZ -= 64;
    if (b->height <= 0)
    {
        b->height = 0;
        if (b->bounceCount < 2)
        {
            b->velocityZ = (b->bounceCount == 0) ? 288 : 144;
            b->bounceCount++;
            contact = TRUE;
        }
        else b->velocityZ = 0;
    }

    return contact;
}

#define ARC_HEIGHT   28   /* px, strong arcade arc at the midpoint */

s16 ball_visualY(const Ball *b)
{
    if ((b->state == BALL_FLYING_TO_A) || (b->state == BALL_FLYING_TO_B))
    {
        s32 t = b->progress;
        s32 height = (ARC_HEIGHT * 4 * t * (255 - t)) / (255L * 255L);
        return b->y - (s16)height;
    }
    if (b->state == BALL_LOOSE) return b->y - (b->height >> 8);
    return b->y;
}

void ball_draw(Ball *b)
{
    bool inFlight = (b->state == BALL_FLYING_TO_A) || (b->state == BALL_FLYING_TO_B);
    s16 drawY = b->y;
    u16 ballTile = TILE_BALL16_FRAME_0;
    /* Court half, not ball state, decides divider occlusion. A near-side held
     * ball must clear the board with its owner; sprite-table order still lets
     * the player's body cover the ball at the rear hand anchor. */
    u16 netPriority = ((b->y - (b->x >> 2)) >= COURT_CENTER_DEPTH) ? 1 : 0;

    if (inFlight)
    {
        /* Parabolic arc (0 at both ends, peak at the midpoint) layered
         * on top of the straight-line lerp, so the ball visibly lifts
         * off the ground and comes back down instead of sliding along
         * a flat 2D line. progress is 0..255. */
        s32 height = b->y - ball_visualY(b);
        drawY = ball_visualY(b);

        /* Four authored seam positions now make rotation readable. Spin
         * reverses their order instead of merely flipping a symmetric ball. */
        {
            u16 frame = (b->progress >> 3) & 3;
            if (b->spin < 0) frame = (4 - frame) & 3;
            ballTile += frame * 4;
        }

        /* Shadow stays on the true ground track - the read on where
         * the ball will actually land. Links on to spriteSlot+2, the
         * controlled-player ground star (see scene_match.c) - the shadow is
         * no longer the last sprite in the chain. */
        VDP_setSpriteFull(b->spriteSlot + 1, b->x, b->y, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, netPriority, FALSE, FALSE,
                               (height > 12) ? TILE_BALL_SHADOW_AIR : TILE_BALL_SHADOW),
                           b->spriteSlot + 2);
    }
    else if (b->state == BALL_LOOSE)
    {
        s16 looseHeight = b->height >> 8;
        drawY = b->y - looseHeight;
        ballTile += (((b->x + b->y) >> 2) & 3) * 4;
        VDP_setSpriteFull(b->spriteSlot + 1, b->x, b->y, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, netPriority, FALSE, FALSE,
                               looseHeight > 4 ? TILE_BALL_SHADOW_AIR : TILE_BALL_SHADOW),
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
    VDP_setSpriteFull(b->spriteSlot, b->x - 4, drawY - 4, SPRITE_SIZE(2, 2),
                       TILE_ATTR_FULL(PAL_BALL, netPriority, FALSE, FALSE, ballTile),
                       b->spriteSlot + 1);
}
