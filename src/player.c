#include "player.h"
#include "input_mgr.h"
#include "sprites_data.h"
#include "game_state.h"

#define RUN_FRAME_LEN   4   /* frames between four-beat run phases */
#define IDLE_FRAME_LEN  28  /* restrained breathing cadence */

#define OFFSCREEN_X   -100
#define OFFSCREEN_Y   -100

void player_init(Player *p, s16 startX, s16 y, u8 spriteSlot, u8 pal)
{
    p->x = startX;
    p->y = y;
    p->homeX = startX;
    p->homeY = y;
    p->eliminated = FALSE;
    p->exiting = FALSE;
    p->spriteSlot = spriteSlot;
    p->pal = pal;
    p->pose = POSE_STAND;
    p->poseTimer = 0;
    p->animFrame = 0;
    p->animCounter = 0;
    p->small = FALSE;
    p->farSide = FALSE;
    p->facingLeft = FALSE;
}

void player_eliminate(Player *p)
{
    p->eliminated = TRUE;
    p->exiting = TRUE;
    p->pose = POSE_RUN;
    p->poseTimer = 0;
    p->animCounter = 0;
}

bool player_updateExit(Player *p)
{
    if (!p->exiting) return TRUE;

    /* A defeated player leaves along the projected touchline rather than
     * popping out of existence on contact. Keep the shallow diagonal so
     * the run belongs to the same isometric ground plane as normal play. */
    p->x += 3;
    p->y += 1;
    p->facingLeft = FALSE;
    if (p->x > SCREEN_W + 24)
    {
        p->x = OFFSCREEN_X;
        p->y = OFFSCREEN_Y;
        p->exiting = FALSE;
        return TRUE;
    }
    return FALSE;
}

void player_restore(Player *p)
{
    p->eliminated = FALSE;
    p->exiting = FALSE;
    p->x = p->homeX;
    p->y = p->homeY;
    p->pose = POSE_STAND;
    p->poseTimer = 0;
}

void player_moveHuman(Player *p)
{
    s16 oldX = p->x;
    /* Both world axes project diagonally in the reference camera.
     * This makes every d-pad direction change screen X and Y instead
     * of sliding players along one flat horizontal baseline. */
    if (input_held(BUTTON_LEFT))  { p->x -= 2; p->y -= 1; }
    if (input_held(BUTTON_RIGHT)) { p->x += 2; p->y += 1; }
    if (input_held(BUTTON_UP))    { p->x += 1; p->y -= 2; }
    if (input_held(BUTTON_DOWN))  { p->x -= 1; p->y += 2; }

    player_clampToCourt(p);
    if (p->x != oldX) p->facingLeft = (p->x < oldX);

    /* Animation is advanced once, centrally, after the match state update.
     * Ticking here as well made the controlled player run at double cadence. */
}

void player_clampToCourt(Player *p)
{
    s16 depth = p->y - (p->x >> 2);
    s16 minDepth = p->farSide ? (COURT_FAR_DEPTH + 6) : (COURT_CENTER_DEPTH + 8);
    s16 maxDepth = p->farSide ? (COURT_CENTER_DEPTH - 8) : (COURT_NEAR_DEPTH - 6);

    if (depth < minDepth) p->y += minDepth - depth;
    if (depth > maxDepth) p->y -= depth - maxDepth;

    depth = p->y - (p->x >> 2);
    /* The parallel sidelines drift left toward the near edge. */
    s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
    s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;
    if (p->x < minX) p->x = minX;
    if (p->x > maxX) p->x = maxX;
}

void player_tickAnim(Player *p, bool isMoving)
{
    if (p->poseTimer > 0)
    {
        /* Three readable beats for every action: anticipation/contact,
         * commitment, then recovery. Even with one authored silhouette
         * per action, offsets and timing now create actual motion. */
        if (++p->animCounter >= 4)
        {
            p->animCounter = 0;
            if (p->pose == POSE_CELEBRATE)
                p->animFrame = (p->animFrame + 1) & 3;
            else if (p->animFrame < 3)
                p->animFrame++;
        }
        p->poseTimer--;
        return;
    }

    p->pose = isMoving ? POSE_RUN : POSE_STAND;

    if (p->pose == POSE_RUN)
    {
        if (++p->animCounter >= RUN_FRAME_LEN)
        {
            p->animCounter = 0;
            p->animFrame = (p->animFrame + 1) & 3;
        }
    }
    else
    {
        if (++p->animCounter >= IDLE_FRAME_LEN)
        {
            p->animCounter = 0;
            p->animFrame = (p->animFrame + 1) & 3;
        }
    }
}

void player_setPose(Player *p, u8 pose, u8 timer)
{
    p->pose = pose;
    p->poseTimer = timer;
    p->animFrame = 0;
    p->animCounter = 0;
}

void player_draw(Player *p)
{
    /* Hardware sprites form a linked list starting at slot 0; match play
     * assigns player slots by ground depth every frame, while the link itself
     * remains the continuous slot N -> N+1 chain. */
    /* Court side selects the camera-facing animation bank; horizontal
     * movement only mirrors that bank. This preserves true front/rear
     * anatomy while letting runners face their actual travel direction. */
    bool backView = !p->farSide;
    u16 base = backView ? TILE_PLAYER_BACK_STAND : TILE_PLAYER_FRONT_STAND;
    bool flip = p->facingLeft;
    s16 poseOffsetX = 0;
    s16 poseOffsetY = 0;
    s16 direction = p->facingLeft ? -1 : 1;

    /* RUN and RUN_ALT are separately authored contact poses with exactly
     * two legs each; no partial-row reflection or ghost limbs. */
    if (p->pose == POSE_RUN)
    {
        static const s8 runX[4] = { -1, 0, 1, 0 };
        if (backView)
            base = (p->animFrame & 1) ? TILE_PLAYER_BACK_RUN_ALT : TILE_PLAYER_BACK_RUN;
        else
            base = (p->animFrame & 1) ? TILE_PLAYER_FRONT_RUN_ALT : TILE_PLAYER_FRONT_RUN;
        static const s8 runY[4] = { 0, -2, -1, 1 };
        poseOffsetX = runX[p->animFrame & 3];
        poseOffsetY = runY[p->animFrame & 3];
    }
    else if (p->pose == POSE_THROW)
    {
        base = backView ? TILE_PLAYER_BACK_THROW : TILE_PLAYER_FRONT_THROW;
        poseOffsetX = ((p->animFrame == 0) ? -1 :
                      (p->animFrame < 3 ? 3 : 1)) * direction;
        poseOffsetY = (p->animFrame == 0) ? 0 :
                      (p->animFrame < 3 ? -4 : -1);
    }
    else if (p->pose == POSE_PICKUP)
    {
        base = backView ? TILE_PLAYER_BACK_STAND : TILE_PLAYER_FRONT_PICKUP;
        poseOffsetX = (p->animFrame < 2) ? 0 : direction;
        poseOffsetY = (p->animFrame < 2) ? 0 : 4;
    }
    else if (p->pose == POSE_HIT)
    {
        base = backView ? TILE_PLAYER_BACK_HIT : TILE_PLAYER_FRONT_HIT;
        poseOffsetX = -direction * (2 + (p->animFrame & 1));
        poseOffsetY = p->animFrame & 1;
    }
    else if (p->pose == POSE_FALL)
    {
        base = backView ? TILE_PLAYER_BACK_FALL : TILE_PLAYER_FRONT_FALL;
        /* Settle by two pixels after the initial collapse instead of freezing
         * the fallen silhouette at exactly one coordinate. */
        poseOffsetX = -direction * (p->animFrame > 1 ? 2 : 0);
        poseOffsetY = p->animFrame > 1 ? 2 : 0;
    }
    else if (p->pose == POSE_CELEBRATE)
    {
        static const s8 victoryY[4] = { 0, -3, -1, -3 };
        base = backView ? TILE_PLAYER_BACK_CELEBRATE : TILE_PLAYER_FRONT_CELEBRATE;
        poseOffsetY = victoryY[p->animFrame & 3];
    }
    else poseOffsetY = (p->animFrame == 3) ? -1 : 0;

    VDP_setSpriteFull(p->spriteSlot, p->x - 8 + poseOffsetX,
                       p->y - 16 + poseOffsetY, SPRITE_SIZE(4, 4),
                       TILE_ATTR_FULL(p->pal, 0, FALSE, flip, base),
                       p->spriteSlot + 1);
}
