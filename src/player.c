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
    p->facingLeft = FALSE;
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
    /* Both world axes project diagonally in the reference camera.
     * This makes every d-pad direction change screen X and Y instead
     * of sliding players along one flat horizontal baseline. */
    if (input_held(BUTTON_LEFT))  { p->x -= 2; p->y -= 1; }
    if (input_held(BUTTON_RIGHT)) { p->x += 2; p->y += 1; }
    if (input_held(BUTTON_UP))    { p->x += 1; p->y -= 2; }
    if (input_held(BUTTON_DOWN))  { p->x -= 1; p->y += 2; }

    player_clampToCourt(p);

    /* Animation is advanced once, centrally, after the match state update.
     * Ticking here as well made the controlled player run at double cadence. */
}

void player_clampToCourt(Player *p)
{
    s16 depth = p->y - (p->x >> 2);
    s16 minDepth = p->small ? (COURT_FAR_DEPTH + 6) : (COURT_CENTER_DEPTH + 8);
    s16 maxDepth = p->small ? (COURT_CENTER_DEPTH - 8) : (COURT_NEAR_DEPTH - 6);

    if (depth < minDepth) p->y += minDepth - depth;
    if (depth > maxDepth) p->y -= depth - maxDepth;

    depth = p->y - (p->x >> 2);
    /* The parallel sidelines drift left toward the near edge. */
    s16 minX = 64 - ((depth - COURT_FAR_DEPTH) >> 1) + 8;
    s16 maxX = 312 - ((depth - COURT_FAR_DEPTH) >> 1) - 8;
    if (p->x < minX) p->x = minX;
    if (p->x > maxX) p->x = maxX;
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
        /* The far side uses a separately authored 24x24 reduction: big
         * enough to preserve pose/team readability while remaining a
         * distinct depth step from the near side's 32x32 art. Offsets
         * preserve the same visual center and feet baseline. */
        u16 farBase = TILE_PLAYER_FAR_STAND;
        bool farFlip = p->facingLeft;
        s16 farOffsetY = 0;
        if (p->pose == POSE_RUN)
        {
            farBase = TILE_PLAYER_FAR_RUN;
            farOffsetY = p->animFrame ? -1 : 0;
        }
        else if (p->pose == POSE_THROW)
        {
            farBase = TILE_PLAYER_FAR_THROW;
            farOffsetY = -2;
        }
        else if (p->pose == POSE_CATCH)
        {
            farBase = TILE_PLAYER_FAR_CATCH;
            farOffsetY = 1;
        }
        else if (p->pose == POSE_HIT)
        {
            farBase = TILE_PLAYER_FAR_CATCH;
            farOffsetY = 3;
        }
        VDP_setSpriteFull(p->spriteSlot, p->x - 4, p->y - 8 + farOffsetY, SPRITE_SIZE(3, 3),
                           TILE_ATTR_FULL(p->pal, 0, FALSE, farFlip, farBase),
                           p->spriteSlot + 1);
        return;
    }

    u16 base = TILE_PLAYER_STAND;
    bool flip = p->facingLeft;
    s16 poseOffsetY = 0;

    /* Animation never changes team facing. The second run beat is a subtle
     * body bob, so the silhouette keeps looking toward the opposition. */
    if (p->pose == POSE_RUN)
    {
        base = TILE_PLAYER_RUN;
        poseOffsetY = p->animFrame ? -1 : 0;
    }
    else if (p->pose == POSE_THROW)
    {
        base = TILE_PLAYER_THROW;
        poseOffsetY = -3;
    }
    else if (p->pose == POSE_CATCH)
    {
        base = TILE_PLAYER_CATCH;
        poseOffsetY = 2;
    }
    else if (p->pose == POSE_HIT)
    {
        /* Reuse the dynamic catch silhouette as a lowered recoil frame. */
        base = TILE_PLAYER_CATCH;
        poseOffsetY = 4;
    }

    VDP_setSpriteFull(p->spriteSlot, p->x - 8, p->y - 16 + poseOffsetY, SPRITE_SIZE(4, 4),
                       TILE_ATTR_FULL(p->pal, 0, FALSE, flip, base),
                       p->spriteSlot + 1);
}
