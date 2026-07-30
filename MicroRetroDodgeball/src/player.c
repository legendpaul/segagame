#include "player.h"
#include "input_mgr.h"
#include "sprites_data.h"
#include "game_state.h"
#include "ui_data.h"
#include "court_bg.h"

#define RUN_FRAME_LEN   2   /* eight beats retain the old full-cycle speed */
#define IDLE_FRAME_LEN  14  /* eight-beat breathing retains its slow cadence */

static u8 action_frame_len(u8 pose)
{
    if (pose == POSE_PICKUP) return 1;
    if (pose == POSE_THROW || pose == POSE_HIT) return 2;
    if (pose == POSE_FALL) return 3;
    return 4; /* celebration */
}

/* The player's ground-contact point (feet / control-ring centre) sits 8px
 * right of and 8px below the sprite origin - the same point the game uses for
 * the held ball and the ground marker. Boundary clamping must use THIS point,
 * not the raw origin, and keep half the stance width inside every touchline so
 * no foot or marker overhangs the line. */
/* Middle-bottom of the 32x32 sprite (drawn at x-8, y-16): the feet/ground
 * point the control ring already marks. All court alignment keys off this. */
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
    p->freeRoam = FALSE;
    p->tileBaseOverride = 0;
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

void player_moveHuman(Player *p, bool hasBall)
{
    player_moveHumanPad(p, hasBall, 0);
}

void player_moveHumanPad(Player *p, bool hasBall, u8 pad)
{
    s16 oldX = p->x;

    /* A celebration is a committed action, just like a throw or fall. Keep
     * the player's ground position locked until its authored pose timer has
     * completed; directional input becomes active again on the next frame. */
    if (p->pose == POSE_CELEBRATE && p->poseTimer > 0) return;

    /* Carrying the ball costs a step: position only advances on every
     * other frame instead of scaling the per-frame delta down. That
     * keeps the exact 2:1 diagonal ratio (so facing/clamping math never
     * sees a fractional pixel) while still reading as a real speed
     * penalty rather than input lag. The run animation itself is paced
     * from input_held() in scene_match.c, not from this gate, so the
     * carrier's legs keep moving smoothly instead of stuttering. */
    static u8 carryGate = 0;
    /* Phase for the fractional part of the court-axis steps (see below). */
    static u8 movePhase = 0;
    bool canStep = TRUE;
    if (hasBall)
    {
        carryGate ^= 1;
        canStep = (carryGate != 0);
    }

    if (canStep)
    {
        /* Run along the COURT's own axes, so a player tracks the painted
         * lines instead of drifting across them:
         *   left/right = parallel to the touchline, slope 1/4  -> (4, 1)
         *   up/down    = parallel to the sideline,  slope -7/4 -> (4, -7)
         * Integer steps can't express 1/4 or 7/4 per frame, so the fractional
         * part is spread over a short phase cycle: sideways moves x by 2 every
         * frame and y by 1 every other frame (= 4:1 over two frames), while
         * up/down moves y by 2 three frames in four and 1 on the fourth
         * (= 7 per 4 frames against x's 4). Average direction is exact. */
        if (input_held_p(pad, BUTTON_LEFT))  { p->x -= 2; if (movePhase & 1) p->y -= 1; }
        if (input_held_p(pad, BUTTON_RIGHT)) { p->x += 2; if (movePhase & 1) p->y += 1; }
        if (input_held_p(pad, BUTTON_UP))    { p->x += 1; p->y -= ((movePhase & 3) ? 2 : 1); }
        if (input_held_p(pad, BUTTON_DOWN))  { p->x -= 1; p->y += ((movePhase & 3) ? 2 : 1); }
        movePhase++;
    }

    player_clampToCourt(p);
    if (p->x != oldX) p->facingLeft = (p->x < oldX);

    /* Animation is advanced once, centrally, after the match state update.
     * Ticking here as well made the controlled player run at double cadence. */
}

void player_clampToCourt(Player *p)
{
    /* Every boundary is enforced against the sprite's MIDDLE-BOTTOM (the feet /
     * control-ring point), so the visible feet - not the sprite origin 16px
     * above them - are what actually line up with the painted court lines.
     * Both the near/far baselines and the sidelines use this one point. */
    /* Outer baselines use a 1px margin so the feet actually REACH the painted
     * end line (a larger margin, on top of the feet reference, left a visible
     * gap of bare grass). The centre-line margins stay larger so players don't
     * stand inside the net. */
    s16 feetX = p->x + PLAYER_FEET_DX;
    s16 depth = COURT_DEPTH_OF(feetX, p->y + PLAYER_FEET_DY);
    /* Free-roam (no centre net) players own the entire court; otherwise each
     * side is held inside its own half by the centre line. */
    s16 minDepth = p->freeRoam ? (COURT_FAR_DEPTH + 1)
                 : p->farSide  ? (COURT_FAR_DEPTH + 1) : (COURT_CENTER_DEPTH + 8);
    s16 maxDepth = p->freeRoam ? (COURT_NEAR_DEPTH - 1)
                 : p->farSide  ? (COURT_CENTER_DEPTH - 8) : (COURT_NEAR_DEPTH - 1);

    if (depth < minDepth) p->y += minDepth - depth;
    if (depth > maxDepth) p->y -= depth - maxDepth;

    /* Re-project the feet depth after the baseline clamp, then keep half the
     * stance width inside each drifting sideline. */
    depth = COURT_DEPTH_OF(feetX, p->y + PLAYER_FEET_DY);
    s16 minX = COURT_MIN_X_AT_DEPTH(depth) + PLAYER_HALF_W;
    s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - PLAYER_HALF_W;
    if (feetX < minX) p->x += (minX - feetX);
    if (feetX > maxX) p->x -= (feetX - maxX);
}

void player_tickAnim(Player *p, bool isMoving)
{
    if (p->poseTimer > 0)
    {
        /* Eight beats give every action anticipation, commitment, contact and
         * recovery. The frame length is fitted to each state's real timer so
         * even the short pickup animation reaches its recovery pose. */
        if (++p->animCounter >= action_frame_len(p->pose))
        {
            p->animCounter = 0;
            if (p->pose == POSE_CELEBRATE)
                p->animFrame = (p->animFrame + 1) & PLAYER_ANIM_MASK;
            else if (p->animFrame < PLAYER_ANIM_MASK)
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
            p->animFrame = (p->animFrame + 1) & PLAYER_ANIM_MASK;
        }
    }
    else
    {
        if (++p->animCounter >= IDLE_FRAME_LEN)
        {
            p->animCounter = 0;
            p->animFrame = (p->animFrame + 1) & PLAYER_ANIM_MASK;
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

u16 player_currentTileBase(const Player *p)
{
    bool backView = !p->farSide;
    u8 phase = p->animFrame & PLAYER_ANIM_MASK;
    u8 gait = phase >> 1;
    if (p->pose == POSE_RUN)
    {
        if (gait & 1)
            return backView ? TILE_PLAYER_BACK_RUN_PASS : TILE_PLAYER_FRONT_RUN_PASS;
        if (backView)
            return (gait == 2) ? TILE_PLAYER_BACK_RUN_ALT : TILE_PLAYER_BACK_RUN;
        return (gait == 2) ? TILE_PLAYER_FRONT_RUN_ALT : TILE_PLAYER_FRONT_RUN;
    }
    if (p->pose == POSE_THROW)
        return (phase < 2 || phase == 7)
            ? (backView ? TILE_PLAYER_BACK_STAND : TILE_PLAYER_FRONT_STAND)
            : (backView ? TILE_PLAYER_BACK_THROW : TILE_PLAYER_FRONT_THROW);
    if (p->pose == POSE_PICKUP)
        return (phase < 2 || phase == 7)
            ? (backView ? TILE_PLAYER_BACK_STAND : TILE_PLAYER_FRONT_STAND)
            : (backView ? TILE_PLAYER_BACK_STAND : TILE_PLAYER_FRONT_PICKUP);
    if (p->pose == POSE_HIT)
        return phase == 0
            ? (backView ? TILE_PLAYER_BACK_STAND : TILE_PLAYER_FRONT_STAND)
            : (phase >= 6
                ? (backView ? TILE_PLAYER_BACK_FALL : TILE_PLAYER_FRONT_FALL)
                : (backView ? TILE_PLAYER_BACK_HIT : TILE_PLAYER_FRONT_HIT));
    if (p->pose == POSE_FALL)
        return phase == 0
            ? (backView ? TILE_PLAYER_BACK_HIT : TILE_PLAYER_FRONT_HIT)
            : (backView ? TILE_PLAYER_BACK_FALL : TILE_PLAYER_FRONT_FALL);
    if (p->pose == POSE_CELEBRATE)
        return backView ? TILE_PLAYER_BACK_CELEBRATE : TILE_PLAYER_FRONT_CELEBRATE;
    return backView ? TILE_PLAYER_BACK_STAND : TILE_PLAYER_FRONT_STAND;
}

/* Keep pose placement in one place. The depth sorter and renderer must see
 * exactly the same displaced body or a falling/eliminated player can retain
 * the wrong hardware-sprite slot while its artwork crosses another actor. */
static void player_poseOffset(const Player *p, s16 *offsetX, s16 *offsetY)
{
    u8 phase = p->animFrame & PLAYER_ANIM_MASK;
    s16 direction = p->facingLeft ? -1 : 1;

    *offsetX = 0;
    *offsetY = 0;
    if (p->pose == POSE_RUN)
    {
        static const s8 runX[PLAYER_ANIM_FRAMES] = { 0, 0, 0, 1, 0, 0, -1, 0 };
        static const s8 runY[PLAYER_ANIM_FRAMES] = { 0, -1, -1, -2, 0, -1, -1, -2 };
        *offsetX = runX[phase] * direction;
        *offsetY = runY[phase];
    }
    else if (p->pose == POSE_THROW)
    {
        static const s8 throwX[PLAYER_ANIM_FRAMES] = { 0, -1, -1, 1, 3, 3, 2, 1 };
        static const s8 throwY[PLAYER_ANIM_FRAMES] = { 0, 1, -1, -2, -4, -3, -2, -1 };
        *offsetX = throwX[phase] * direction;
        *offsetY = throwY[phase];
    }
    else if (p->pose == POSE_PICKUP)
    {
        static const s8 pickupX[PLAYER_ANIM_FRAMES] = { 0, 0, 0, 1, 2, 2, 1, 0 };
        static const s8 pickupY[PLAYER_ANIM_FRAMES] = { 0, 1, 2, 4, 5, 4, 2, 0 };
        *offsetX = pickupX[phase] * direction;
        *offsetY = pickupY[phase];
    }
    else if (p->pose == POSE_HIT)
    {
        static const s8 hitX[PLAYER_ANIM_FRAMES] = { 0, 1, 2, 3, 4, 4, 3, 2 };
        static const s8 hitY[PLAYER_ANIM_FRAMES] = { 0, 0, 1, 0, 1, 2, 2, 2 };
        *offsetX = -direction * hitX[phase];
        *offsetY = hitY[phase];
    }
    else if (p->pose == POSE_FALL)
    {
        static const s8 fallX[PLAYER_ANIM_FRAMES] = { 0, 0, 1, 1, 2, 2, 3, 3 };
        static const s8 fallY[PLAYER_ANIM_FRAMES] = { 0, 1, 1, 2, 2, 3, 3, 3 };
        *offsetX = -direction * fallX[phase];
        *offsetY = fallY[phase];
    }
    else if (p->pose == POSE_CELEBRATE)
    {
        static const s8 victoryY[PLAYER_ANIM_FRAMES] = { 0, -2, -4, -2, 0, -3, -5, -2 };
        *offsetY = victoryY[phase];
    }
    else
    {
        static const s8 breatheY[PLAYER_ANIM_FRAMES] = { 0, 0, -1, -1, -2, -1, 0, 0 };
        *offsetY = breatheY[phase];
    }
}

void player_visualGround(const Player *p, s16 *x, s16 *y)
{
    s16 offsetX, offsetY;
    player_poseOffset(p, &offsetX, &offsetY);
    *x = p->x + PLAYER_FEET_DX + offsetX;
    *y = p->y + PLAYER_FEET_DY + offsetY;
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
    u16 base = player_currentTileBase(p);
    bool flip = p->facingLeft;
    s16 poseOffsetX = 0;
    s16 poseOffsetY = 0;
    u8 spritePriority;

    /* Eight gait beats hold each authored contact/pass silhouette for two
     * subtly different body positions, making the cycle read smoothly at 50Hz. */
    player_poseOffset(p, &poseOffsetX, &poseOffsetY);

    if (p->tileBaseOverride != 0) base = p->tileBaseOverride;

    /* The centre board is a high-priority BG_A foreground. Near-half players
     * must use high sprite priority to cover it; far-half players remain low
     * priority so the same board correctly passes in front of them. */
    spritePriority = backView ? 1 : 0;
    /* High-priority sprites outrank high-priority planes on Mega Drive. Drop
     * only a player intersecting the score strip or live shot-clock panel to
     * low priority so those UI surfaces always remain in front. */
    if (ui_sprite_behind_panel(p->x - 8 + poseOffsetX,
                               p->y - 16 + poseOffsetY, 32, 32) ||
        court_bg_spriteBehindRoof(p->x - 8 + poseOffsetX,
                                  p->y - 16 + poseOffsetY, 32, 32))
        spritePriority = 0;
    VDP_setSpriteFull(p->spriteSlot, p->x - 8 + poseOffsetX,
                       p->y - 16 + poseOffsetY, SPRITE_SIZE(4, 4),
                       TILE_ATTR_FULL(p->pal, spritePriority, FALSE, flip, base),
                       p->spriteSlot + 1);
}
