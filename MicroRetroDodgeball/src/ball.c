#include "ball.h"
#include "sprites_data.h"
#include "game_state.h"
#include "ui_data.h"
#include "court_bg.h"

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
    b->shadowSlot = spriteSlot + 1;
    b->eliminatorArt = FALSE;
    b->contactKind = BALL_CONTACT_NONE;
    b->contactVX = b->contactVY = 0;
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
    b->contactKind = BALL_CONTACT_NONE;
}

/* Convert an airborne side-wall crossing immediately into a loose inward
 * rebound. Waiting for the next loose-ball frame allowed the thrown sprite to
 * appear outside the projected court for one or more frames. */
static void ball_startFlightSideBounce(Ball *b, u8 contactKind,
                                       s16 oldX, s16 oldY)
{
    s16 stepX = b->x - oldX;
    s16 stepY = b->y - oldY;
    s16 heightPx = b->y - ball_visualY(b);
    s16 depth = COURT_DEPTH_OF(b->x, b->y);
    s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
    s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;

    if (stepX == 0)
        stepX = (b->targetX > b->startX) ? 1 : -1;
    if (contactKind == BALL_CONTACT_LEFT_WALL) b->x = minX;
    else b->x = maxX;
    b->y = COURT_Y_AT_DEPTH_X(depth, b->x);

    b->looseFarSide = depth < COURT_CENTER_DEPTH;
    b->state = BALL_LOOSE;
    b->preciseX = (s32)b->x << 8;
    b->preciseY = (s32)b->y << 8;
    b->contactKind = contactKind;
    b->contactVX = (s16)(stepX * 256);
    b->contactVY = (s16)(stepY * 256);
    b->velocityX = (contactKind == BALL_CONTACT_LEFT_WALL)
        ? (s16)(abs(stepX) * 192) : (s16)(-abs(stepX) * 192);
    b->velocityY = (s16)(stepY * 192);
    b->height = (s16)(heightPx << 8);
    b->velocityZ = (b->progress < 128) ? 128 : -64;
    b->bounceCount = 0;
}

bool ball_update(Ball *b)
{
    s16 oldX, oldY;
    if ((b->state != BALL_FLYING_TO_A) && (b->state != BALL_FLYING_TO_B))
        return FALSE;

    if (b->progress >= 255)
    {
        b->x = b->targetX;
        b->y = b->targetY;
        return TRUE;
    }

    oldX = b->x;
    oldY = b->y;
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

    /* Curved throws can meet a sloping side before their intended back-wall
     * endpoint. In a normal match the centre net is a hard possession rule:
     * a legal throw must first enter the receiving half. Before that point we
     * contain the curve just inside the rail but do NOT turn it into a loose
     * rebound; once it is BALL_NET_MARGIN beyond the divider, wall contacts
     * are enabled normally. Eliminator has no divider, so it remains eligible
     * for side-wall ricochets throughout its flight. */
    {
        s16 depth = COURT_DEPTH_OF(b->x, b->y);
        s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
        s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;
        bool crossedNet = b->eliminatorArt ||
            (b->state == BALL_FLYING_TO_B
                ? depth <= COURT_CENTER_DEPTH - BALL_NET_MARGIN
                : depth >= COURT_CENTER_DEPTH + BALL_NET_MARGIN);
        if (b->x < minX)
        {
            if (crossedNet)
            {
                ball_startFlightSideBounce(b, BALL_CONTACT_LEFT_WALL,
                                           oldX, oldY);
                return TRUE;
            }
            /* Leave a visible sliver between ball and rail: this is flight
             * containment, not a pre-net impact. */
            b->x = minX + 3;
            b->y = COURT_Y_AT_DEPTH_X(depth, b->x);
        }
        else if (b->x > maxX)
        {
            if (crossedNet)
            {
                ball_startFlightSideBounce(b, BALL_CONTACT_RIGHT_WALL,
                                           oldX, oldY);
                return TRUE;
            }
            b->x = maxX - 3;
            b->y = COURT_Y_AT_DEPTH_X(depth, b->x);
        }
    }

    return (b->progress >= 255) ? TRUE : FALSE;
}

void ball_startRicochet(Ball *b)
{
    s16 incomingX = b->x - b->startX;
    s16 incomingY = b->y - b->startY;
    BallState incomingState = b->state;

    b->looseFarSide = (incomingState == BALL_FLYING_TO_B);
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
    /* A completed miss ends with the ball touching the receiving back wall.
     * Preserve that contact so the scene can draw/sound the impact at once. */
    b->contactKind = (incomingState == BALL_FLYING_TO_B)
        ? BALL_CONTACT_FAR_WALL : BALL_CONTACT_NEAR_WALL;
    b->contactVX = (s16)((incomingX * 256L) / 56L);
    b->contactVY = (s16)((incomingY * 256L) / 56L);
}

void ball_startHitBounce(Ball *b, s16 x, s16 y)
{
    s16 travelX = b->x - b->startX;
    s16 travelY = b->y - b->startY;
    s16 kickX = (s16)((travelX * 256L) / 96L + b->spin * 48);
    s16 kickY = (s16)((travelY * 256L) / 96L);
    bool landedFarSide = (b->state == BALL_FLYING_TO_B);

    /* Cap unusually long diagonal throws: every hit should clear the body by
     * roughly a player width, not fire a second full-strength shot. */
    if (kickX > 320) kickX = 320;
    if (kickX < -320) kickX = -320;
    if (kickY > 320) kickY = 320;
    if (kickY < -320) kickY = -320;

    b->x = x;
    b->y = y;
    b->looseFarSide = landedFarSide;
    b->state = BALL_LOOSE;
    b->preciseX = (s32)x << 8;
    b->preciseY = (s32)y << 8;
    /* Carry a restrained amount of the incoming direction through the body.
     * This keeps a normal-match rebound on the receiving half while still
     * moving it clearly away from the struck player. */
    b->velocityX = kickX;
    b->velocityY = kickY;
    b->height = 3 << 8;
    b->velocityZ = 320;
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

/* Keep every loose ball inside a region a player can physically enter.  A
 * normal match reserves the centre rail; Eliminator has no net, so its two
 * balls use the full court instead of bouncing off an invisible divider. */
static bool clamp_loose_ball(Ball *b)
{
    bool contact = FALSE;
    s16 depth;
    const s16 minDepth = b->eliminatorArt ? COURT_FAR_DEPTH + BALL_WALL_MARGIN
                       : b->looseFarSide  ? COURT_FAR_DEPTH + BALL_WALL_MARGIN
                                          : COURT_CENTER_DEPTH + BALL_NET_MARGIN;
    const s16 maxDepth = b->eliminatorArt ? COURT_NEAR_DEPTH - BALL_WALL_MARGIN
                       : b->looseFarSide  ? COURT_CENTER_DEPTH - BALL_NET_MARGIN
                                          : COURT_NEAR_DEPTH - BALL_WALL_MARGIN;

    /* Clamp depth first, then derive the sloping side rails from that same
     * projected depth. This is the exact quadrilateral painted by the court
     * converter rather than the old screen-aligned invisible rectangle. */
    depth = COURT_DEPTH_OF(b->x, b->y);
    if (depth < minDepth)
    {
        b->contactKind = BALL_CONTACT_FAR_WALL;
        b->contactVX = b->velocityX;
        b->contactVY = b->velocityY;
        b->y = COURT_Y_AT_DEPTH_X(minDepth, b->x);
        b->preciseY = (s32)b->y << 8;
        b->velocityY = (s16)(-b->velocityY * 3 / 4);
        contact = TRUE;
    }
    else if (depth > maxDepth)
    {
        b->contactKind = BALL_CONTACT_NEAR_WALL;
        b->contactVX = b->velocityX;
        b->contactVY = b->velocityY;
        b->y = COURT_Y_AT_DEPTH_X(maxDepth, b->x);
        b->preciseY = (s32)b->y << 8;
        b->velocityY = (s16)(-b->velocityY * 3 / 4);
        contact = TRUE;
    }

    depth = COURT_DEPTH_OF(b->x, b->y);
    {
        const s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
        const s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;
        if (b->x < minX)
        {
            b->contactKind = BALL_CONTACT_LEFT_WALL;
            b->contactVX = b->velocityX;
            b->contactVY = b->velocityY;
            b->x = minX;
            b->y = COURT_Y_AT_DEPTH_X(depth, b->x);
            b->preciseX = (s32)b->x << 8;
            b->preciseY = (s32)b->y << 8;
            b->velocityX = (s16)(-b->velocityX * 3 / 4);
            contact = TRUE;
        }
        else if (b->x > maxX)
        {
            b->contactKind = BALL_CONTACT_RIGHT_WALL;
            b->contactVX = b->velocityX;
            b->contactVY = b->velocityY;
            b->x = maxX;
            b->y = COURT_Y_AT_DEPTH_X(depth, b->x);
            b->preciseX = (s32)b->x << 8;
            b->preciseY = (s32)b->y << 8;
            b->velocityX = (s16)(-b->velocityX * 3 / 4);
            contact = TRUE;
        }
    }

    return contact;
}

bool ball_updateLoose(Ball *b)
{
    bool contact;

    if (b->state != BALL_LOOSE) return FALSE;

    b->contactKind = BALL_CONTACT_NONE;
    b->contactVX = b->velocityX;
    b->contactVY = b->velocityY;

    b->preciseX += b->velocityX;
    b->preciseY += b->velocityY;
    b->x = (s16)(b->preciseX >> 8);
    b->y = (s16)(b->preciseY >> 8);

    contact = clamp_loose_ball(b);

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
            if (b->contactKind == BALL_CONTACT_NONE)
                b->contactKind = BALL_CONTACT_FLOOR;
            b->velocityZ = (b->bounceCount == 0) ? 288 : 144;
            b->bounceCount++;
            contact = TRUE;
        }
        else b->velocityZ = 0;
    }

    return contact;
}

void ball_settle(Ball *b)
{
    /* Forced settlements (timeouts, opening placement, post-hit drops) get the
     * same reachable bounds as moving rebounds; no state path may strand one
     * beneath the centre rail. */
    if (b->state == BALL_LOOSE) clamp_loose_ball(b);
    b->height = 0;
    b->velocityZ = 0;
    b->velocityX = 0;
    b->velocityY = 0;
    b->bounceCount = 2;
    b->preciseX = (s32)b->x << 8;
    b->preciseY = (s32)b->y << 8;
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
    bool held = (b->state == BALL_HELD_A) || (b->state == BALL_HELD_B);
    bool ballFlip = FALSE;
    s16 drawY = b->y;
    u16 ballTile = b->eliminatorArt ? TILE_ELIM_BALL16_FRAME_0
                                    : TILE_BALL16_FRAME_0;
    u16 shadowTile = b->eliminatorArt ? TILE_ELIM_BALL_SHADOW
                                      : TILE_BALL_SHADOW;
    u16 shadowAirTile = b->eliminatorArt ? TILE_ELIM_BALL_SHADOW_AIR
                                         : TILE_BALL_SHADOW_AIR;
    u8 ballSize = SPRITE_SIZE(2, 2);
    s16 ballOffset = 8;
    /* Court half, not ball state, decides divider occlusion. A near-side held
     * ball must clear the board with its owner; sprite-table order still lets
     * the player's body cover the ball at the rear hand anchor. */
    u16 netPriority = (b->state == BALL_HELD_A) ? 1 :
                      (b->state == BALL_HELD_B) ? 0 :
                      ((COURT_DEPTH_OF(b->x, b->y) >= COURT_CENTER_DEPTH) ? 1 : 0);
    u16 shadowPriority = (b->y - 4 < 24) ? 0 : netPriority;

    if (court_bg_spriteBehindRoof(b->x - 4, b->y - 4, 8, 8))
        shadowPriority = 0;

    if (inFlight)
    {
        /* Parabolic arc (0 at both ends, peak at the midpoint) layered
         * on top of the straight-line lerp, so the ball visibly lifts
         * off the ground and comes back down instead of sliding along
         * a flat 2D line. progress is 0..255. */
        s32 height = b->y - ball_visualY(b);
        drawY = ball_visualY(b);

        /* Four authored seams plus their mirrored in-betweens provide eight
         * visible rotation phases. Spin reverses the complete cycle. */
        {
            u16 phase = (b->progress >> 3) & 7;
            if (b->spin < 0) phase = (8 - phase) & 7;
            ballTile += (phase >> 1) * 4;
            ballFlip = (phase & 1) != 0;
        }

        /* Shadow stays on the true ground track - the read on where
         * the ball will actually land. Links on to spriteSlot+2, the
         * controlled-player ground star (see scene_match.c) - the shadow is
         * no longer the last sprite in the chain. */
        VDP_setSpriteFull(b->shadowSlot, b->x - 4, b->y - 4, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, shadowPriority, FALSE, FALSE,
                               (height > 12) ? shadowAirTile : shadowTile),
                           b->shadowSlot + 1);
    }
    else if (b->state == BALL_LOOSE)
    {
        s16 looseHeight = b->height >> 8;
        drawY = b->y - looseHeight;
        {
            u16 phase = ((b->x + b->y) >> 2) & 7;
            ballTile += (phase >> 1) * 4;
            ballFlip = (phase & 1) != 0;
        }
        VDP_setSpriteFull(b->shadowSlot, b->x - 4, b->y - 4, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, shadowPriority, FALSE, FALSE,
                               looseHeight > 4 ? shadowAirTile : shadowTile),
                           b->shadowSlot + 1);
    }
    else if (held)
    {
        /* Held, airborne and loose states must share one apparent diameter.
         * The former 8x8 held tile visibly grew into a 16x16 ball at release
         * and shrank again on pickup. Keep the same compact silhouette in the
         * same 16x16 container; only airborne/loose seam frames rotate. */
        ballTile = b->eliminatorArt ? TILE_ELIM_BALL16_FRAME_0
                                    : TILE_BALL16_FRAME_0;
        VDP_setSpriteFull(b->shadowSlot, -16, -16, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, shadowTile),
                           b->shadowSlot + 1);
    }

    /* The ball links to the shadow, which now links on to the ground star
     * (see scene_match.c) so all three stay reachable from slot 0. */
    /* Like players, an airborne ball can visually enter the HUD even while
     * its ground-depth priority says it is near the camera. Let the high BG
     * panel mask it only for those frames. */
    if (ui_sprite_behind_panel(b->x - ballOffset, drawY - ballOffset,
                               16, 16) ||
        court_bg_spriteBehindRoof(b->x - ballOffset, drawY - ballOffset,
                                  16, 16))
        netPriority = 0;
    VDP_setSpriteFull(b->spriteSlot, b->x - ballOffset, drawY - ballOffset, ballSize,
                       TILE_ATTR_FULL(PAL_BALL, netPriority, FALSE, ballFlip, ballTile),
                       b->spriteSlot + 1);
}
