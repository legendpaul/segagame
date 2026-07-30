#include "genesis.h"
#include "scene_match.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "player.h"
#include "ball.h"
#include "ai_mgr.h"
#include "sound_mgr.h"
#include "sprites_data.h"
#include "court_bg.h"
#include "ui_data.h"
#include "flag_data.h"
#include "impact_fx.h"
#include "screen_transition.h"

#define REF_FRAME_COUNT 4
#define REF_FRAME_TILE_COUNT 16
/* The referee tiles live in the boot-logo VRAM region: that splash is never
 * shown again after startup, and the court base tileset ends just before it
 * while court foreground/flags start after these 16 slots, so it's dead space
 * during a match. */
#define TILE_REFEREE   (TILE_COURT_BASE + COURT_TILE_COUNT)
/* Frame zero occupies the dead boot-logo region. The other three frames use
 * the title-art bank, which is mutually exclusive with live match play. */
#define TILE_REFEREE_EXTRA TILE_TITLE_BASE
/* Rear-view stand fits between shared impact art and the court foreground.
 * Three rear run frames use the final 48 tiles immediately before the flag
 * bank. Both gaps coexist with live match play and avoid the HUD flag tiles. */
#define TILE_REFEREE_BACK       (TILE_IMPACT_GREY + 4)
#define TILE_REFEREE_BACK_EXTRA (TILE_FLAG_BASE - 48)
#if (TILE_REFEREE_BACK + 16) > TILE_COURT_FG_BASE
#error "Rear referee stand overlaps court foreground VRAM"
#endif
#if (TILE_REFEREE_BACK_EXTRA + 48) > TILE_FLAG_BASE
#error "Rear referee run frames overlap HUD flag VRAM"
#endif
#if (TILE_REFEREE_EXTRA + 48) > TILE_UI_BASE
#error "Front referee run frames overlap UI VRAM"
#endif
/* One more tile immediately after frame zero: a short white dash used to build
 * the dotted vertical line that shows how high the ball is off the ground. */
#define TILE_BALL_TETHER (TILE_REFEREE + REF_FRAME_TILE_COUNT)
static const u32 tile_ball_tether[8] = {
    0x00011000,
    0x00011000,
    0x00000000,
    0x00000000,
    0x00011000,
    0x00011000,
    0x00000000,
    0x00000000
};
#define REF_SKIN_LIGHT 0xD8A878
#define REF_SKIN_DARK  0x9C6C40

#define SLOT_TEAM_A   0    /* initial slots; reassigned by projected depth each frame */
#define SLOT_TEAM_B   3    /* initial slots; reassigned by projected depth each frame */
#define SLOT_BALL     6    /* initial ball slot; reassigned with player depth */
#define AIR_DOT_COUNT 6
/* Six players, ball, impact, referee and six airborne guide dots occupy the
 * depth-sorted slots 0..14. Only ground underlays remain behind that list. */
#define SLOT_BALL_SHADOW 15
#define SLOT_MARKER      16
#define SLOT_MARKER2     17   /* player 2's ring in shared-team play */

typedef enum {
    MS_ANNOUNCE = 0,
    MS_A_HOLD,
    MS_B_HOLD,
    MS_A_WINDUP,
    MS_B_WINDUP,
    MS_FLY_TO_B,
    MS_FLY_TO_A,
    MS_LOOSE_B,
    MS_LOOSE_A,
    MS_HIT_B,
    MS_HIT_A,
    MS_ESCORT,
    MS_ROUND_END
} MatchState;

static MatchState state;
static Player teamA[TEAM_SIZE];    /* human side */
static Player teamB[TEAM_SIZE];    /* CPU side */
static Ball ball;

static u8  activeA;                 /* which teamA slot the human currently controls */
static u8  holderA, holderB;        /* which slot currently holds the ball on each side */
static u8  responderA, responderB;  /* which slot is defending the ball currently in flight */

static u16 announceTimer;
static u16 matchSeconds;
static u8  clockFrameCounter;
static u16 aiDelay;
/* Which team B player pad 2 is driving in 2 PLAYER VS. Team B is otherwise
 * CPU-run, so this is only meaningful while human_b() is true. */
static u8  activeB;
/* TRUE when the far side is a second human rather than the AI. */
static bool human_b(void)
{
    return gPlayerMode == PLAYERS_2P_VS;
}

/* TRUE when both humans share the NEAR side, a player each. */
static bool team_share(void)
{
    return gPlayerMode == PLAYERS_2P_TEAM;
}

/* Player 2's man on the shared near side. */
static u8 activeA2;
static u8 count_in_play(Player team[]);
static u8 first_in_play_from(Player team[], u8 start);
static bool aiCarrierRepositions;
static s8  aiCarrierOffset;
static u8  aiCarrierMoveFloor; /* minimum visible movement before an armed CPU throws */
static s8  aiEvadeDir[TEAM_SIZE]; /* stable sidestep direction for an incoming throw */
static u16 roundEndTimer;
static u8  windupTimer;
static u8  impactTimer;
static u8  hitstopTimer;    /* brief freeze makes a successful strike read with weight */
static bool hitExitStarted;
static s8  hitKnockX;       /* signed screen-space direction of the incoming ball */
static s8  hitKnockY;
static u8  looseTimer;      /* prevents instant pickup at the impact point */
/* Shot-clock on a loose ball: the responsible player must retrieve AND
 * throw it before this runs out or they're eliminated. */
static u16 pickupClock;     /* frames left, 0 = no clock running */
static bool pickupIsA;      /* TRUE: team A (human) on the clock, FALSE: AI */
static u16  aiLooseReact;   /* frames the CPU still waits before chasing a loose ball */

/* Referee escort: on a shot-clock timeout the ref runs in from the right,
 * walks the offending player off, then the ball (dropped on the spot) goes
 * loose again for the next player - or the round/match ends if that was the
 * team's last player. */
static s16  refX, refY;     /* referee sprite position (screen space) */
static u8   refSpriteSlot;
static u8   airDotSpriteSlots[AIR_DOT_COUNT];
static u8   escortIdx;      /* which player on the escorted side is being walked off */
static bool escortIsA;      /* TRUE: escorting a team A (human) player */
static u8   escortPhase;    /* 0 = running in to the player, 1 = walking them out */
static u8   refAnim;        /* walk-frame counter */
static bool refFacingLeft;  /* horizontal mirror follows actual X movement */
static bool refBackView;    /* rear bank follows last vertical move upward */
static bool shotClockShown; /* whether the on-screen countdown is currently drawn */
static u16  shotClockSecondsShown; /* cached value: prevents per-frame tile/palette rewrites */
#define PICKUP_CLOCK_SECS  10
#define PICKUP_CLOCK_FRAMES (u16)((SYS_isPAL() ? 50 : 60) * PICKUP_CLOCK_SECS)
static s8  pendingSpin;
static s16 pendingTargetX;
static s16 pendingTargetY;
static u8  server;          /* 0 = team A serves, 1 = team B serves */
static u8  roundWinnerIsA;
static u16 ambientTick;      /* gentle off-ball repositioning cadence */
static u8  markerPulseTick;  /* low-frequency light breathing inside control ring */

static u8  flashTimer;      /* frames left in the current impact flash, 0 = none */
static u8  shakeTimer;      /* frames left in the current screen shake, 0 = none */
static s8  worldOffsetY;    /* applied to BG_B and every world sprite together */

/* In-match pause overlay: START freezes the whole update loop (all timers,
 * AI and physics) and shows a small centred menu. Sprites are parked while
 * paused; the normal render tail redraws them on resume. */
static bool matchPaused;
static u8   pauseRow;       /* 0 = RESUME, 1 = EXIT MATCH */
#define PAUSE_X  12
#define PAUSE_Y  9
#define PAUSE_W  16
#define PAUSE_H  10

/* Safety watchdog: if the match state hasn't changed for an
 * unreasonably long time (STALL_LIMIT frames), something has gone
 * wrong - force the current state's natural timer/condition to
 * complete rather than let a real player get soft-locked waiting on
 * the CPU forever. Discovered during extended live playtesting: the
 * CPU-serve path could sit indefinitely with no visible cause found
 * on code review, so this is a defensive net on top of, not a
 * replacement for, actually finding that root cause later. */
#define STALL_LIMIT   400   /* ~6.6s at 60fps - well beyond any real delay */
#define HIT_RECOIL_FRAMES 12
#define HIT_FALL_FRAMES   24
#define HIT_TOTAL_FRAMES  (HIT_RECOIL_FRAMES + HIT_FALL_FRAMES)
static MatchState lastState;
static u16 stallCounter;
static bool stallTrackerInit;

/* --- small helpers -------------------------------------------------- */

/* Whites-out the team whose player the ball just reached, for a couple
 * of frames - real impact feedback on a hit, not
 * just the sound effect. Restored automatically once flashTimer hits 0
 * (see scene_match_update()). */
static void trigger_flash(u8 palLine)
{
    sprites_data_flash_team(palLine);
    flashTimer = 4;
}

/* Decaying vertical jolt on the court plane (BG_B only - HUD text lives on
 * BG_A and deliberately stays put, same as real games keep the score
 * readable while the game world shakes) on a hard hit. Cheap (one register
 * write per frame, no extra tiles/sprites) but real, per-frame-decaying
 * "impact" feedback - a second-opinion suggestion (Qwen) flagged this as
 * one of the single highest-leverage, lowest-cost polish moves available
 * given this project's hardware constraints. Restored to 0 automatically
 * once shakeTimer hits 0 (see scene_match_update()). */
static const s8 shakePattern[6] = { 3, -3, 2, -2, 1, 0 };

static void trigger_shake(void)
{
    shakeTimer = 6;
}

/* TRUE while the human-controlled slot is the one actually carrying the
 * ball (held or mid-windup), shared by the ground marker colour and the
 * carrying movement penalty so the two never drift out of sync. */
static bool activeA_has_ball(void)
{
    return (activeA == holderA) &&
           (state == MS_A_HOLD || state == MS_A_WINDUP ||
            (state == MS_ANNOUNCE && server == 0));
}

/* Button presses are throws while team A owns the ball. At every other
 * playable moment they select a defender instead: A/C use current screen-X
 * order while B prioritises distance to the live ball. */
static bool teamA_has_possession(void)
{
    return state == MS_A_HOLD || state == MS_A_WINDUP ||
           (state == MS_ANNOUNCE && server == 0);
}

/* A hit or escort only disables the player involved.  Their team-mates stay
 * controllable, which removes the long whole-team input lock after contact. */
static bool control_candidate(u8 index)
{
    Player *p = &teamA[index];
    if (p->eliminated || p->exiting) return FALSE;
    if (state == MS_HIT_A && index == responderA) return FALSE;
    if (state == MS_ESCORT && escortIsA && index == escortIdx) return FALSE;
    /* In shared-team play the two humans can never grab the same body: player
     * 2's man is off limits to player 1's switching. The exception is the last
     * survivor, who has to remain selectable by whoever already holds him. */
    if (team_share() && index == activeA2 && count_in_play(teamA) > 1)
        return FALSE;
    return TRUE;
}

static void rotate_controlled_player(bool right, bool audible)
{
    s16 currentX = teamA[activeA].x;
    s16 bestDistance = 32767;
    s16 wrapX = right ? 32767 : -32767;
    u8 best = activeA;
    u8 wrap = activeA;
    u8 i;

    for (i = 0; i < TEAM_SIZE; i++)
    {
        s16 distance;
        if (i == activeA || !control_candidate(i)) continue;

        distance = teamA[i].x - currentX;
        if ((right && distance > 0 && distance < bestDistance) ||
            (!right && distance < 0 && -distance < bestDistance))
        {
            bestDistance = right ? distance : -distance;
            best = i;
        }

        /* Right wraps to the leftmost player; left wraps to the rightmost. */
        if ((right && teamA[i].x < wrapX) ||
            (!right && teamA[i].x > wrapX))
        {
            wrapX = teamA[i].x;
            wrap = i;
        }
    }

    if (best == activeA) best = wrap;
    if (best != activeA)
    {
        activeA = best;
        if (audible) sound_mgr_blip();
    }
}

/* B is the tactical switch: take the selectable player nearest the ball. If
 * that player is already controlled, select the second-nearest instead so a
 * repeated press remains useful rather than appearing to do nothing. */
static void select_controlled_nearest_to_ball(bool audible)
{
    u32 nearestDistance = 0xFFFFFFFF;
    u32 nextDistance = 0xFFFFFFFF;
    u8 nearest = activeA;
    u8 next = activeA;
    u8 i;

    for (i = 0; i < TEAM_SIZE; i++)
    {
        s32 dx, dy;
        u32 distance;

        if (!control_candidate(i)) continue;
        dx = teamA[i].x + PLAYER_PICKUP_DX - ball.x;
        dy = teamA[i].y + PLAYER_PICKUP_DY - ball.y;
        distance = (u32)(dx * dx + dy * dy);

        if (distance < nearestDistance)
        {
            nextDistance = nearestDistance;
            next = nearest;
            nearestDistance = distance;
            nearest = i;
        }
        else if (distance < nextDistance)
        {
            nextDistance = distance;
            next = i;
        }
    }

    if (activeA == nearest && nextDistance != 0xFFFFFFFF)
        nearest = next;

    if (nearest != activeA && control_candidate(nearest))
    {
        activeA = nearest;
        if (audible) sound_mgr_blip();
    }
}

static bool activeA_can_move(void)
{
    if (state == MS_ROUND_END || !control_candidate(activeA)) return FALSE;
    if (state == MS_A_WINDUP && activeA == holderA) return FALSE;
    return TRUE;
}

/* Wide ground ring under the controlled player: yellow while defending or
 * moving without the ball, red while holding/winding up a throw. */
static void draw_control_marker(void)
{
    Player *p = &teamA[activeA];
    bool hasBall = activeA_has_ball();
    u16 markerTile = hasBall ? TILE_RING_RED : TILE_RING_YELLOW;
    if ((markerPulseTick & 15) == 0)
        sprites_data_set_ring_pulse((u8)((markerPulseTick >> 4) & 3), FALSE);
    markerPulseTick++;
    /* With a second human on either side the chain carries on to their ring. */
    u8 link = (team_share() || human_b()) ? SLOT_MARKER2 : 0;
    if (!control_candidate(activeA) && !(team_share() && activeA == activeA2))
    {
        VDP_setSpriteFull(SLOT_MARKER, -24, -24, SPRITE_SIZE(3, 2),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, markerTile),
                           link);
    }
    else
    {
        /* A 24px open ellipse stays readable without covering the feet. */
        VDP_setSpriteFull(SLOT_MARKER, p->x - 4, p->y + 8 + worldOffsetY,
                           SPRITE_SIZE(3, 2),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, markerTile),
                           link);
    }

    /* Player 2's man wears a DIFFERENT coloured ring so the two humans can
     * always tell their own player apart at a glance - on the shared near side
     * in team play, or on the far side in VS. */
    if (team_share() || human_b())
    {
        Player *q = human_b() ? &teamB[activeB] : &teamA[activeA2];
        bool p2Ball = human_b()
                      ? ((holderB == activeB) &&
                         (state == MS_B_HOLD || state == MS_B_WINDUP))
                      : ((holderA == activeA2) &&
                         (state == MS_A_HOLD || state == MS_A_WINDUP));
        bool show = !q->eliminated && !q->exiting &&
                    (human_b() || (activeA2 != activeA));
        u16 tile2 = p2Ball ? TILE_RING_YELLOW : TILE_RING_RED;
        VDP_setSpriteFull(SLOT_MARKER2,
                           show ? (s16)(q->x - 4) : (s16)-24,
                           show ? (s16)(q->y + 8 + worldOffsetY) : (s16)-24,
                           SPRITE_SIZE(3, 2),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, tile2), 0);
    }
}

/* The six ex-player-shadow slots now draw a dotted white line from the ball's
 * ground point up to the ball whenever it is off the ground, so its height is
 * readable at a glance. Unused dots are parked off-screen; the link chain and
 * its terminator are unchanged. The dots take the SAME net priority as the
 * ball, so the centre board occludes them exactly like it occludes the ball. */
static void draw_ball_air_dots(void)
{
    bool held = (ball.state == BALL_HELD_A) || (ball.state == BALL_HELD_B);
    /* The dots ALWAYS hang directly below the ball sprite itself, so the
     * column lines up with the ball - not with the carrier's body. For a held
     * ball the ground reference is the carrier's feet; otherwise it is the
     * ball's own ground track. */
    const Player *carrier = (ball.state == BALL_HELD_A) ? &teamA[holderA]
                                                       : &teamB[holderB];
    s16 groundX = ball.x;
    s16 groundY = held ? (carrier->y + 16 + worldOffsetY)
                       : (ball.y + worldOffsetY);
    s16 gap     = groundY - (ball_visualY(&ball) + worldOffsetY);
    /* Any loose ball with real height counts as airborne - including the low
     * bounces after it ricochets off a wall back into play. */
    bool airborne = held ||
                    (ball.state == BALL_FLYING_TO_A) ||
                    (ball.state == BALL_FLYING_TO_B) ||
                    ((ball.state == BALL_LOOSE) && ((ball.height >> 8) > 0));
    u16 netPriority = (COURT_DEPTH_OF(ball.x, ball.y) >= COURT_CENTER_DEPTH) ? 1 : 0;
    u8 dots = 0, i;

    if (airborne && gap >= 6)
    {
        dots = (u8)(gap / 8);
        if (dots < 1) dots = 1;      /* a low bounce still shows one dot */
        if (dots > 6) dots = 6;
    }

    for (i = 0; i < AIR_DOT_COUNT; i++)
    {
        u8 slot = airDotSpriteSlots[i];
        u8 link = (u8)(slot + 1);
        if (i < dots)
        {
            s16 dotY = groundY - 10 - (s16)i * 8;
            u16 dotPriority = (ui_sprite_behind_panel(groundX - 4, dotY, 8, 8) ||
                               court_bg_spriteBehindRoof(groundX - 4, dotY, 8, 8))
                ? 0 : netPriority;
            VDP_setSpriteFull(slot, groundX - 4, dotY,
                               SPRITE_SIZE(1, 1),
                               TILE_ATTR_FULL(PAL_BALL, dotPriority, FALSE, FALSE,
                                              TILE_BALL_TETHER), link);
        }
        else
            VDP_setSpriteFull(slot, -16, -16, SPRITE_SIZE(1, 1),
                               TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE,
                                              TILE_BALL_SHADOW), link);
    }
}

static s16 lane_x(u8 i)
{
    return COURT_LEFT_X + (s16)((i + 1) * (COURT_RIGHT_X - COURT_LEFT_X) / (TEAM_SIZE + 1));
}

typedef struct {
    Player *player;
    s16 groundX;
    s16 groundY;
    bool isBall;
    bool isImpact;
    bool isReferee;
    s8 airDot;
} DepthActor;

static bool actor_is_nearer(const DepthActor *a, const DepthActor *b)
{
    if (a->groundY != b->groundY) return a->groundY > b->groundY;
    return a->groundX > b->groundX;
}

static void assign_actor_depth_slots(void)
{
    DepthActor order[TEAM_SIZE * 2 + 3 + AIR_DOT_COUNT];
    const u8 ballIndex = TEAM_SIZE * 2;
    const u8 impactIndex = ballIndex + 1;
    const u8 refereeIndex = impactIndex + 1;
    const u8 dotIndex = refereeIndex + 1;
    const u8 actorCount = dotIndex + AIR_DOT_COUNT;
    s16 ballGroundX, ballGroundY;
    u8 i;

    for (i = 0; i < TEAM_SIZE; i++)
    {
        order[i].player = &teamA[i];
        if (teamA[i].eliminated && !teamA[i].exiting)
        {
            order[i].groundX = -32767;
            order[i].groundY = -32767;
        }
        else player_visualGround(&teamA[i], &order[i].groundX,
                                  &order[i].groundY);
        order[i].isBall = FALSE;
        order[i].isImpact = FALSE;
        order[i].isReferee = FALSE;
        order[i].airDot = -1;
        order[TEAM_SIZE + i].player = &teamB[i];
        if (teamB[i].eliminated && !teamB[i].exiting)
        {
            order[TEAM_SIZE + i].groundX = -32767;
            order[TEAM_SIZE + i].groundY = -32767;
        }
        else player_visualGround(&teamB[i],
                                  &order[TEAM_SIZE + i].groundX,
                                  &order[TEAM_SIZE + i].groundY);
        order[TEAM_SIZE + i].isBall = FALSE;
        order[TEAM_SIZE + i].isImpact = FALSE;
        order[TEAM_SIZE + i].isReferee = FALSE;
        order[TEAM_SIZE + i].airDot = -1;
    }

    /* Sort the ball from its ground track, never its airborne screen Y.
     * A held near-side ball sits just behind its rear-facing owner; the
     * far-side/front-facing holder presents it just in front of the torso. */
    order[ballIndex].player = NULL;
    order[ballIndex].isBall = TRUE;
    order[ballIndex].isImpact = FALSE;
    order[ballIndex].isReferee = FALSE;
    order[ballIndex].airDot = -1;
    if (ball.state == BALL_HELD_A)
    {
        ballGroundX = teamA[holderA].x;
        ballGroundY = teamA[holderA].y - 1;
    }
    else if (ball.state == BALL_HELD_B)
    {
        ballGroundX = teamB[holderB].x;
        ballGroundY = teamB[holderB].y + 1;
    }
    else
    {
        ballGroundX = ball.x;
        ballGroundY = ball.y;
    }
    order[ballIndex].groundX = ballGroundX;
    order[ballIndex].groundY = ballGroundY;

    order[impactIndex].player = NULL;
    order[impactIndex].isBall = FALSE;
    order[impactIndex].isImpact = TRUE;
    order[impactIndex].isReferee = FALSE;
    order[impactIndex].airDot = -1;
    order[impactIndex].groundX = impact_fx_active()
        ? impact_fx_sortX() : -32767;
    order[impactIndex].groundY = impact_fx_active()
        ? impact_fx_sortY() : -32767;

    order[refereeIndex].player = NULL;
    order[refereeIndex].isBall = FALSE;
    order[refereeIndex].isImpact = FALSE;
    order[refereeIndex].isReferee = TRUE;
    order[refereeIndex].airDot = -1;
    order[refereeIndex].groundX = (state == MS_ESCORT) ? refX : -32767;
    order[refereeIndex].groundY = (state == MS_ESCORT) ? refY : -32767;

    /* The vertical guide belongs to the ball's ground-depth layer. Sorting
     * every dot prevents its column from cutting across a nearer body. */
    for (i = 0; i < AIR_DOT_COUNT; i++)
    {
        order[dotIndex + i].player = NULL;
        order[dotIndex + i].isBall = FALSE;
        order[dotIndex + i].isImpact = FALSE;
        order[dotIndex + i].isReferee = FALSE;
        order[dotIndex + i].airDot = (s8)i;
        order[dotIndex + i].groundX = ballGroundX;
        order[dotIndex + i].groundY = ballGroundY;
    }

    /* This compact list is small enough for a stable insertion sort. Exact
     * ties retain their prior actor order, preventing one-frame overlap flicker. */
    for (i = 1; i < actorCount; i++)
    {
        DepthActor key = order[i];
        u8 j = i;
        while ((j > 0) && actor_is_nearer(&key, &order[j - 1]))
        {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = key;
    }

    for (i = 0; i < actorCount; i++)
    {
        if (order[i].isBall) ball.spriteSlot = i;
        else if (order[i].isImpact) impact_fx_setSpriteSlot(i);
        else if (order[i].isReferee) refSpriteSlot = i;
        else if (order[i].airDot >= 0)
            airDotSpriteSlots[(u8)order[i].airDot] = i;
        else order[i].player->spriteSlot = i;
    }
    ball.shadowSlot = SLOT_BALL_SHADOW;
}

static u8 count_in_play(Player team[])
{
    u8 i, n = 0;
    for (i = 0; i < TEAM_SIZE; i++)
        if (!team[i].eliminated) n++;
    return n;
}

/* First in-play slot at or after "preferred" (wrapping) - used to keep
 * "active"/"holder" pointed at someone who's actually still playing. */
static u8 first_in_play_from(Player team[], u8 preferred)
{
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        u8 idx = (preferred + i) % TEAM_SIZE;
        if (!team[idx].eliminated) return idx;
    }
    return preferred; /* shouldn't happen - round should already have ended */
}

static u8 closest_in_play(Player team[], s16 x, s16 y)
{
    u8 i, best = first_in_play_from(team, 0);
    u32 bestDistance = 0xFFFFFFFF;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        s32 dx, dy;
        u32 distance;
        if (team[i].eliminated) continue;
        dx = team[i].x + PLAYER_PICKUP_DX - x;
        dy = team[i].y + PLAYER_PICKUP_DY - y;
        distance = (u32)(dx * dx + dy * dy);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

static bool move_toward_point(Player *p, s16 targetX, s16 targetY)
{
    bool moved = FALSE;
    s16 oldX = p->x;
    if (p->x < targetX - 1) { p->x += PLAYER_SPEED; moved = TRUE; }
    else if (p->x > targetX + 1) { p->x -= PLAYER_SPEED; moved = TRUE; }
    if (p->y < targetY - 1) { p->y++; moved = TRUE; }
    else if (p->y > targetY + 1) { p->y--; moved = TRUE; }
    player_clampToCourt(p);
    if (p->x != oldX) p->facingLeft = (p->x < oldX);
    return moved;
}

static bool move_toward_ball(Player *p)
{
    return move_toward_point(p, ball.x - PLAYER_PICKUP_DX,
                             ball.y - PLAYER_PICKUP_DY);
}

static bool move_cpu_carrier(Player *p, const Player *rival, s8 laneOffset)
{
    /* Build the target from a legal FEET position in Team B's far half. The
     * former target projected Y from the carrier's old X, so moving sideways
     * silently changed its intended court depth and made it ride the net clamp. */
    const s16 setDepth = COURT_CENTER_DEPTH - 12;
    const s16 minFeetX = COURT_MIN_X_AT_DEPTH(setDepth) + PLAYER_HALF_W;
    const s16 maxFeetX = COURT_MAX_X_AT_DEPTH(setDepth) - PLAYER_HALF_W;
    s16 targetFeetX = rival->x + PLAYER_FEET_DX + laneOffset;
    s16 targetY;

    if (targetFeetX < minFeetX) targetFeetX = minFeetX;
    if (targetFeetX > maxFeetX) targetFeetX = maxFeetX;
    targetY = COURT_Y_AT_DEPTH_X(setDepth, targetFeetX) - PLAYER_FEET_DY;
    return move_toward_point(p, targetFeetX - PLAYER_FEET_DX, targetY);
}

static void choose_ai_carrier_style(void)
{
    /* Most possessions include a short, visible reposition before the throw.
     * Some are deliberately planted so armed CPU players do not feel robotic. */
    if (gDifficulty == DIFF_EASY)
        aiCarrierRepositions = ((random() & 1) != 0);
    else if (gDifficulty == DIFF_HARD)
        aiCarrierRepositions = ((random() & 7) != 0);
    else
        aiCarrierRepositions = ((random() & 3) != 0);
    aiCarrierOffset = (random() & 1) ? 18 : -18;
    /* HARD still releases quickly, but no longer snaps immediately from a
     * pickup into a throw. This floor makes every selected carrier run read
     * on screen while preserving the occasional planted possession. */
    aiCarrierMoveFloor = aiCarrierRepositions ?
        (gDifficulty == DIFF_HARD ? 10 :
         gDifficulty == DIFF_EASY ? 20 : 16) : 0;
}

/* Cache one evasive direction per defender when Team A releases the ball.
 * Keeping this stable prevents the left/right jitter that results from
 * recalculating "away" after the defender crosses the flight lane. */
static void prepare_cpu_evades(void)
{
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        s16 centre = teamB[i].x + PLAYER_PICKUP_DX;
        bool reacts = gDifficulty == DIFF_HARD ? TRUE :
                      gDifficulty == DIFF_EASY ? ((random() % 3) == 0) :
                                                ((random() & 3) != 0);
        if (!reacts || teamB[i].eliminated) aiEvadeDir[i] = 0;
        else if (centre < pendingTargetX) aiEvadeDir[i] = -1;
        else if (centre > pendingTargetX) aiEvadeDir[i] = 1;
        else aiEvadeDir[i] = (random() & 1) ? 1 : -1;
    }
}

/* Dodge only when the incoming lane genuinely threatens this defender. The
 * reaction point is difficulty-scaled; CPU players therefore look aware of
 * the throw without gaining an impossible instant read on EASY/NORMAL. */
static bool move_cpu_evade(Player *p, u8 index)
{
    u16 reactionPoint = gDifficulty == DIFF_HARD ? 72 :
                        gDifficulty == DIFF_EASY ? 198 : 126;
    s16 centre = p->x + PLAYER_PICKUP_DX;
    s16 feetY = p->y + PLAYER_PICKUP_DY;
    s16 oldX = p->x;
    s16 oldY = p->y;

    if (aiEvadeDir[index] == 0 || ball.progress < reactionPoint ||
        abs(centre - pendingTargetX) > (HIT_WINDOW_X + 28) ||
        abs(feetY - pendingTargetY) > 34)
        return FALSE;

    /* A lateral burst is readable in the isometric view and gets the body out
     * of the collision lane while player_clampToCourt keeps it in Team B's 50%. */
    p->x += aiEvadeDir[index] *
            (gDifficulty == DIFF_HARD ? 3 : PLAYER_SPEED);
    p->y += aiEvadeDir[index];
    player_clampToCourt(p);
    if (p->x != oldX) p->facingLeft = (p->x < oldX);
    return p->x != oldX || p->y != oldY;
}

/* Aim at a real living opponent rather than choosing an unrelated lane.
 * The landing remains in the legal back court, while Normal/Hard compensate
 * for the authored curve so intentional spin does not sabotage CPU aim. */
static void plan_cpu_throw(void)
{
    const s16 depth = COURT_NEAR_DEPTH - BALL_WALL_MARGIN;
    const s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
    const s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;
    Player *target;

    responderA = closest_in_play(teamA,
                                 teamB[holderB].x + PLAYER_PICKUP_DX,
                                 teamB[holderB].y + PLAYER_PICKUP_DY);
    activeA = responderA;
    target = &teamA[responderA];
    pendingSpin = (s8)((random() % 3) - 1);
    pendingTargetX = ai_pickTargetX(target->x + PLAYER_PICKUP_DX);
    if (gDifficulty != DIFF_EASY)
        pendingTargetX -= pendingSpin * 18;
    if (pendingTargetX < minX) pendingTargetX = minX;
    if (pendingTargetX > maxX) pendingTargetX = maxX;
    pendingTargetY = COURT_Y_AT_DEPTH_X(depth, pendingTargetX);
}

/* Independent off-ball movement. Human-side teammates retain a restrained
 * home formation; CPU players patrol broad tactical lanes across their own
 * 50% of the court, with every target authored in projected feet/depth space. */
#define AMBIENT_SLOTS   6
#define WANDER_RANGE_X  10
#define WANDER_RANGE_Y   6
static u16 wanderTimer[AMBIENT_SLOTS];   /* frames until this player re-targets */
static s16 wanderTX[AMBIENT_SLOTS];
static s16 wanderTY[AMBIENT_SLOTS];
static u8  wanderPace[AMBIENT_SLOTS];    /* 1 = ambles, 2 = brisker - per player */

static void wander_pick(Player *p, u8 slot)
{
    if (p->farSide)
    {
        /* Team B owns depths 25..76. Leave a visual margin at both the far
         * baseline and centre board, then split the available width into three
         * lanes so teammates cover the half instead of forming one scrum. */
        const s16 minDepth = COURT_FAR_DEPTH + 8;
        const s16 maxDepth = COURT_CENTER_DEPTH - 12;
        const s16 depth = minDepth + (s16)(random() % (maxDepth - minDepth + 1));
        const s16 minFeetX = COURT_MIN_X_AT_DEPTH(depth) + PLAYER_HALF_W;
        const s16 maxFeetX = COURT_MAX_X_AT_DEPTH(depth) - PLAYER_HALF_W;
        const u8 role = (u8)(slot - TEAM_SIZE);
        const s16 width = maxFeetX - minFeetX;
        const s16 laneCentre = minFeetX +
            (s16)(width * (role + 1) / (TEAM_SIZE + 1));
        s16 roamingRadius = width / 5;
        s16 feetX;

        if (roamingRadius < 18) roamingRadius = 18;
        feetX = laneCentre +
            (s16)(random() % (roamingRadius * 2 + 1)) - roamingRadius;
        if (feetX < minFeetX) feetX = minFeetX;
        if (feetX > maxFeetX) feetX = maxFeetX;

        wanderTX[slot] = feetX - PLAYER_FEET_DX;
        wanderTY[slot] = COURT_Y_AT_DEPTH_X(depth, feetX) - PLAYER_FEET_DY;
        /* Short, difficulty-scaled plants make the CPU look alert instead of
         * waiting in three fixed lanes. They still pause often enough for a
         * human to read the formation and choose a throw. */
        if (gDifficulty == DIFF_EASY)
        {
            wanderTimer[slot] = (u16)(24 + (random() % 42));
            wanderPace[slot] = ((random() & 1) == 0) ? 1 : 2;
        }
        else if (gDifficulty == DIFF_HARD)
        {
            wanderTimer[slot] = (u16)(3 + (random() % 15));
            wanderPace[slot] = 2;
        }
        else
        {
            wanderTimer[slot] = (u16)(8 + (random() % 23));
            wanderPace[slot] = ((random() & 3) == 0) ? 1 : 2;
        }
    }
    else
    {
        wanderTX[slot] = p->homeX +
            (s16)(random() % (WANDER_RANGE_X * 2 + 1)) - WANDER_RANGE_X;
        wanderTY[slot] = p->homeY +
            (s16)(random() % (WANDER_RANGE_Y * 2 + 1)) - WANDER_RANGE_Y;
        wanderTimer[slot] = (u16)(55 + (random() % 85));
        wanderPace[slot] = 1;
    }
}

/* Seed all six with staggered, independent targets at round start. */
static void wander_init(void)
{
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        wander_pick(&teamA[i], i);
        wander_pick(&teamB[i], (u8)(i + TEAM_SIZE));
    }
}

static bool move_ambient(Player *p, u8 slot)
{
    bool moved = FALSE;
    s16 oldX = p->x;
    s16 tx = wanderTX[slot];
    s16 ty = wanderTY[slot];
    u8  pace = wanderPace[slot];
    bool arrived;

    if (p->x < tx) { p->x += (p->x + pace <= tx) ? pace : 1; moved = TRUE; }
    else if (p->x > tx) { p->x -= (p->x - pace >= tx) ? pace : 1; moved = TRUE; }

    /* CPU depth changes every frame so it genuinely covers its full half;
     * human-side supporting players keep the gentler half-rate shuffle. */
    if (p->farSide || ((ambientTick + slot * 11) & 1) == 0)
    {
        if (p->y < ty) { p->y++; moved = TRUE; }
        else if (p->y > ty) { p->y--; moved = TRUE; }
    }

    /* Dwell time starts after arrival rather than expiring during the run.
     * This produces readable run/plant/run decisions instead of perpetual drift. */
    arrived = (p->x == tx) && (p->y == ty);
    if (arrived)
    {
        if (wanderTimer[slot] > 0) wanderTimer[slot]--;
        else wander_pick(p, slot);
    }

    player_clampToCourt(p);
    if (p->x != oldX) p->facingLeft = (p->x < oldX);
    return moved;
}

static void place_ball_in_hand(Player *p, bool windup)
{
    static const s8 frontHandX[4] = { 10, 10, 11, 10 };
    static const s8 frontHandY[4] = { -9, -10, -9, -10 };
    static const s8 rearHandX[4]  = { 7, 7, 8, 7 };
    static const s8 rearHandY[4]  = { -11, -12, -11, -12 };
    u8 frame = (p->animFrame >> 1) & 3;
    s16 direction = p->facingLeft ? -1 : 1;
    s16 bodyCenterX = p->x + 8;
    s16 handX = p->farSide ? frontHandX[frame] : rearHandX[frame];
    s16 handY = p->farSide ? frontHandY[frame] : rearHandY[frame];

    /* Explicit wrist anchors keep the ball attached to the authored pose.
     * Wind-up extends the same anchor instead of scaling or snapping the ball. */
    ball.x = bodyCenterX + direction * (handX + (windup ? 4 : 0));
    ball.y = p->y + handY - (windup ? 5 : 0);
}

/* A/B/C address the visible left/middle/right opponent lanes. If that
 * exact slot is out, use the nearest surviving lane rather than silently
 * choosing a random target. */
static bool ball_overlaps_player(const Player *p)
{
    /* Test the visible parabolic path, not an intended target or the
     * invisible ground shadow. A curved throw can therefore pass cleanly. */
    return !p->eliminated &&
           abs((p->x + 4) - ball.x) <= HIT_WINDOW_X &&
           abs((p->y - 8) - ball_visualY(&ball)) <= HIT_WINDOW_Y;
}

static s8 first_ball_hit(Player team[])
{
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
        if (ball_overlaps_player(&team[i])) return (s8)i;
    return -1;
}

static void fixed_back_target(bool farSide, u8 lane, s16 *x, s16 *y)
{
    s16 depth = farSide ? (COURT_FAR_DEPTH + BALL_WALL_MARGIN)
                        : (COURT_NEAR_DEPTH - BALL_WALL_MARGIN);
    /* Derive the three back-court points from the projected side walls at
     * this depth. The near edge is wider/shifted left; using the old flat
     * lane_x values would not line up with the isometric court. */
    s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
    s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;
    *x = minX + (s16)((maxX - minX) * lane / 2);
    *y = COURT_Y_AT_DEPTH_X(depth, *x);
}

static bool player_reached_ball(const Player *p)
{
    return abs((p->x + PLAYER_PICKUP_DX) - ball.x) <= PICKUP_WINDOW_X &&
           abs((p->y + PLAYER_PICKUP_DY) - ball.y) <= PICKUP_WINDOW_Y;
}

static void trigger_surface_impact(Ball *b)
{
    impact_fx_trigger(b->x, ball_visualY(b), b->x, b->y,
                      COURT_DEPTH_OF(b->x, b->y) >= COURT_CENTER_DEPTH,
                      FALSE);
}

static void update_loose_ball(void)
{
    if (ball_updateLoose(&ball))
    {
        sound_mgr_bounce();
        if (ball.contactKind >= BALL_CONTACT_LEFT_WALL)
            trigger_surface_impact(&ball);
    }
}

static void trigger_player_impact(const Player *victim)
{
    s16 travelY = ball.y - ball.startY;
    /* Collision is tested against the projected ball, but its airborne screen
     * Y can sit well above the body. Pin feedback to the victim's torso so the
     * contact always reads as a player hit rather than a nearby wall spark. */
    impact_fx_trigger(victim->x + 8, victim->y - 4,
                      victim->x, victim->y + ((travelY >= 0) ? 1 : -1),
                      !victim->farSide, TRUE);
}

static void eliminate_from(Player team[], u8 idx)
{
    player_eliminate(&team[idx]);
}

static void reset_team(Player team[], u8 baseSlot, u8 pal, s16 baseDepth, bool farSide)
{
    static const s8 depthOffset[TEAM_SIZE] = { -8, 10, 0 };
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        s16 x = lane_x(i);
        s16 y = COURT_Y_AT_DEPTH_X(baseDepth + depthOffset[i], x);
        player_init(&team[i], x, y, baseSlot + i, pal);
        team[i].farSide = farSide;
        team[i].facingLeft = farSide;
    }
}

static void draw_hud(void)
{
    char buf[8];
    char clock[6];
    u16 minutes = matchSeconds / 60;
    u16 seconds = matchSeconds % 60;
    ui_set_palette(PAL0);
    ui_apply_palette();
    ui_draw_panel(0, 0, 40, 3, FALSE);
    flag_data_draw_small(gTeamAIndex, 1, 1, PAL3);
    flag_data_draw_small(gTeamBIndex, 37, 1, PAL3);
    /* FIFA-style fixed-width identifiers keep every team clear of the clock. */
    ui_draw_text(teamCodes[gTeamAIndex], 4, 1, UI_WHITE);
    ui_draw_text(teamCodes[gTeamBIndex], 33, 1, UI_WHITE);

    clock[0] = '0' + ((minutes / 10) % 10);
    clock[1] = '0' + (minutes % 10);
    clock[2] = ':';
    clock[3] = '0' + (seconds / 10);
    clock[4] = '0' + (seconds % 10);
    clock[5] = 0;
    ui_draw_text(clock, 13, 1, UI_CYAN);

    intToStr(gScoreA, buf, 1);
    ui_draw_text(buf, 20, 1, UI_GOLD);
    ui_draw_text("-", 21, 1, UI_WHITE);
    intToStr(gScoreB, buf, 1);
    ui_draw_text(buf, 22, 1, UI_GOLD);
}

/* Per-second update: redraw ONLY the five clock digits. They are always the
 * same 5 cells and fully overwrite the previous value, so there is no clear,
 * no panel refill and no palette re-apply - and therefore no scoreboard flash
 * (unlike calling the whole draw_hud() every second, which repainted the
 * entire bar mid-frame). */
static void draw_hud_clock(void)
{
    char clock[6];
    u16 minutes = matchSeconds / 60;
    u16 seconds = matchSeconds % 60;
    clock[0] = '0' + ((minutes / 10) % 10);
    clock[1] = '0' + (minutes % 10);
    clock[2] = ':';
    clock[3] = '0' + (seconds / 10);
    clock[4] = '0' + (seconds % 10);
    clock[5] = 0;
    ui_draw_text(clock, 13, 1, UI_CYAN);
}

/* Round banner rect, shared by the fill and the restore in
 * clear_playfield_text() so the two can never drift apart. */
#define INTRO_X  2
#define INTRO_Y 20
#define INTRO_W 36
#define INTRO_H  7

static void draw_match_intro(void)
{
    char roundBuf[4];
    u8 roundNumber = gScoreA + gScoreB + 1;
    /* The shared panel painter supplies its own solid BG_B backer.
     * clear_playfield_text() restores the court here when the banner goes. */
    ui_draw_panel(INTRO_X, INTRO_Y, INTRO_W, INTRO_H, TRUE);
    ui_draw_text("ROUND", 16, 21, UI_CYAN);
    intToStr(roundNumber, roundBuf, 1);
    ui_draw_text(roundBuf, 22, 21, UI_GOLD);
    flag_data_draw_large(gTeamAIndex, 4, 23, PAL3);
    flag_data_draw_large(gTeamBIndex, 32, 23, PAL3);
    ui_draw_text(teamCodes[gTeamAIndex], 9, 23, UI_WHITE);
    ui_draw_text(teamCodes[gTeamBIndex], 28, 23, UI_WHITE);
    ui_draw_big_text("VS", 18, 23, UI_GOLD);
}

/* SGDK's text-line clear writes opaque font-space tiles, which created
 * the full-width black stripe that made the old court look corrupted.
 * Clearing the plane's tilemap and restoring only the compact HUD makes
 * the isometric BG_B court visible again everywhere else. */
static void clear_playfield_text(void)
{
    /* Use the immediate rectangular clear here. The asynchronous full-plane
     * clear could remain queued behind the per-second HUD writes, leaving a
     * supposedly temporary lower-third stuck over live play. */
    VDP_clearTileMapRect(BG_A, 0, 0, 40, 28);
    /* Put the court back underneath the round banner. draw_match_intro() lays a
     * solid navy fill on BG_B so its text is readable; without restoring it
     * here that fill would stay as a permanent blue box over the pitch. */
    court_bg_redraw_rect(INTRO_X, INTRO_Y, INTRO_W, INTRO_H);
    draw_hud();
    court_bg_drawForeground();
}

static void begin_announce(void)
{
    /* Keep state messaging out of the projected playfield. The old
     * full-width text row broke the court into two flat halves. */
    clear_playfield_text();
    draw_match_intro();
    sound_mgr_whistle();

    announceTimer = 60;
    state = MS_ANNOUNCE;

    if (server == 0)
    {
        holderA = activeA;
        ball_init(&ball, SLOT_BALL, 0, 0, BALL_HELD_A);
        place_ball_in_hand(&teamA[holderA], FALSE);
    }
    else
    {
        holderB = first_in_play_from(teamB, ai_pickSlot(TEAM_SIZE));
        ball_init(&ball, SLOT_BALL, 0, 0, BALL_HELD_B);
        place_ball_in_hand(&teamB[holderB], FALSE);
    }
}

static void start_round(void)
{
    reset_team(teamA, SLOT_TEAM_A, PAL_TEAM_A, TEAM_A_DEPTH, FALSE);
    reset_team(teamB, SLOT_TEAM_B, PAL_TEAM_B, TEAM_B_DEPTH, TRUE);
    /* Both sides use the same 32x32 art. Perspective comes from placement,
     * shadows and the court projection—not "men versus midgets" scaling. */

    wander_init();   /* independent per-player off-ball movement */
    activeA = 0;
    activeB = 0;
    activeA2 = (TEAM_SIZE > 1) ? 1 : 0;   /* the two humans start on different men */

    draw_hud();
    begin_announce();
}

void scene_match_enter(void)
{
    u8 i;

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearSprites();
    VDP_setTextPalette(PAL0);
    ui_set_sprite_panel_mask(FALSE, 0, 0, 0, 0);
    /* VDP_clearPlane alone can leave stale text tiles behind on a scene
     * change; explicitly clearing the text area guarantees a clean slate. */
    VDP_clearTextArea(0, 0, 40, 28);
    court_bg_draw();
    sprites_data_load_impact_art();
    impact_fx_init();

    /* Recolor the shared player tile art to the teams actually picked on
     * the menu - without this both sides always rendered in the same
     * hardcoded colors no matter which team you chose. */
    sprites_data_apply_teams(gTeamAIndex, gTeamBIndex);

    /* The official now shares the players' authored anatomy and run poses,
     * remapped to a striped white/black shirt with black shorts, socks and
     * shoes. */
    sprites_data_load_referee_art(TILE_REFEREE, TILE_REFEREE_EXTRA,
                                  TILE_REFEREE_BACK,
                                  TILE_REFEREE_BACK_EXTRA);
    VDP_loadTileData(tile_ball_tether, TILE_BALL_TETHER, 1, DMA);

    /* Solid navy behind the HUD strip so the score/clock glyphs never reveal
     * the crowd through their transparent pixels (and no fans sit under it). */
    flag_data_fill_panel(0, 0, 40, 4);

    flashTimer = 0;
    shakeTimer = 0;
    hitstopTimer = 0;
    looseTimer = 0;
    pickupClock = 0;
    shotClockShown = FALSE;
    shotClockSecondsShown = 0;
    refAnim = 0;
    refFacingLeft = TRUE;
    refBackView = FALSE;
    worldOffsetY = 0;
    matchSeconds = 0;
    ambientTick = 0;
    markerPulseTick = 0;
    aiCarrierMoveFloor = 0;
    for (i = 0; i < TEAM_SIZE; i++) aiEvadeDir[i] = 0;
    clockFrameCounter = 0;
    VDP_setVerticalScroll(BG_B, 0);
    stallTrackerInit = FALSE;
    server = 0;
    matchPaused = FALSE;
    start_round();
    screen_transition_fade_in();
}

static void go_round_end(u8 winnerIsA)
{
    Player *winners;
    u8 i;

    pickupClock = 0;
    roundWinnerIsA = winnerIsA;
    /* The round-end celebration does not tick loose-ball physics either, so
     * make sure the ball is resting on the ground rather than stuck mid-air. */
    ball_settle(&ball);
    sound_mgr_score();
    sound_mgr_crowdVictory();

    if (winnerIsA) gScoreA++;
    else gScoreB++;

    draw_hud();

    clear_playfield_text();

    /* Keep the match camera on the winning side long enough to enjoy the
     * result. Every surviving teammate joins the scorer's raised-fist loop. */
    winners = winnerIsA ? teamA : teamB;
    for (i = 0; i < TEAM_SIZE; i++)
        if (!winners[i].eliminated)
            player_setPose(&winners[i], POSE_CELEBRATE, 255);

    /* Losing team serves next, to keep matches competitive. */
    server = winnerIsA ? 1 : 0;
    roundEndTimer = ((gScoreA >= WIN_SCORE) || (gScoreB >= WIN_SCORE)) ? 150 : 90;
    state = MS_ROUND_END;
}

static void begin_loose_for_B(void)
{
    holderB = closest_in_play(teamB, ball.x, ball.y);
    /* A miss is still airborne and needs its landing ricochet. After a hit,
     * ball_startHitBounce() has already made the directional loose rebound;
     * do not relaunch it or change receiving halves. */
    if (ball.state != BALL_LOOSE) ball_startRicochet(&ball);
    looseTimer = 10;
    /* Team 2 observes the same visible possession clock; reliable navigation
     * replaces the old scripted fumble and hidden immunity. */
    pickupClock = PICKUP_CLOCK_FRAMES;
    pickupIsA = FALSE;
    aiLooseReact = ai_looseReactionFrames();   /* difficulty-scaled hesitation */
    state = MS_LOOSE_B;
}

static void begin_loose_for_A(void)
{
    activeA = closest_in_play(teamA, ball.x, ball.y);
    holderA = activeA;
    if (ball.state != BALL_LOOSE) ball_startRicochet(&ball);
    looseTimer = 10;
    /* Start the human's 10s retrieve-and-throw shot clock. */
    pickupClock = PICKUP_CLOCK_FRAMES;
    pickupIsA = TRUE;
    state = MS_LOOSE_A;
}

/* Lock the actual start-to-contact travel vector before ball_dropAt() changes
 * the ball state. The authored hit/fall silhouette leans left unflipped and
 * right when mirrored, so facingLeft deliberately follows a rightward knock. */
static void lock_hit_direction(Player *victim)
{
    s16 travelX = ball.x - ball.startX;
    s16 travelY = ball.y - ball.startY;

    hitKnockX = (travelX > 2) ? 1 : ((travelX < -2) ? -1 : 0);
    hitKnockY = (travelY > 2) ? 1 : ((travelY < -2) ? -1 : 0);
    if (hitKnockX != 0) victim->facingLeft = (hitKnockX > 0);
}

static void advance_hit_recoil(Player *victim)
{
    /* Six one-pixel steps carry the body along the incoming vector during
     * recoil. The grounded fall then holds that displaced contact point. */
    if ((impactTimer >= HIT_FALL_FRAMES) &&
        (((impactTimer - HIT_FALL_FRAMES) & 1) == 0))
    {
        victim->x += hitKnockX;
        victim->y += hitKnockY;
    }
}

/* Every airborne overlap is a hit. A miss enters the same loose-ball
 * rally as a hit; possession is earned only by reaching the rebound. */
static void resolve_throw_to_B(void)
{
    s8 hit = first_ball_hit(teamB);
    if (hit < 0)
    {
        sound_mgr_bounce();
        begin_loose_for_B();
        if (ball.contactKind >= BALL_CONTACT_LEFT_WALL)
            trigger_surface_impact(&ball);
        return;
    }

    responderB = (u8)hit;
    lock_hit_direction(&teamB[responderB]);
    trigger_player_impact(&teamB[responderB]);
    ball_startHitBounce(&ball,
                        teamB[responderB].x + PLAYER_PICKUP_DX,
                        teamB[responderB].y + PLAYER_PICKUP_DY);
    sound_mgr_hit();
    trigger_flash(PAL_TEAM_B);
    trigger_shake();
    player_setPose(&teamB[responderB], POSE_HIT, HIT_RECOIL_FRAMES);
    player_setPose(&teamA[holderA], POSE_CELEBRATE, HIT_TOTAL_FRAMES);
    impactTimer = HIT_TOTAL_FRAMES;
    hitstopTimer = 4;
    hitExitStarted = FALSE;
    state = MS_HIT_B;
}

static void finish_hit_to_B(void)
{
    draw_hud();
    if (count_in_play(teamB) == 0) { go_round_end(TRUE); return; }
    begin_loose_for_B();
}

/* Same hit-or-loose resolution mirrored for a throw at team A. */
static void resolve_throw_to_A(void)
{
    s8 hit = first_ball_hit(teamA);
    if (hit < 0)
    {
        sound_mgr_bounce();
        begin_loose_for_A();
        if (ball.contactKind >= BALL_CONTACT_LEFT_WALL)
            trigger_surface_impact(&ball);
        return;
    }

    responderA = (u8)hit;
    lock_hit_direction(&teamA[responderA]);
    trigger_player_impact(&teamA[responderA]);
    ball_startHitBounce(&ball,
                        teamA[responderA].x + PLAYER_PICKUP_DX,
                        teamA[responderA].y + PLAYER_PICKUP_DY);
    sound_mgr_hit();
    trigger_flash(PAL_TEAM_A);
    trigger_shake();
    player_setPose(&teamA[responderA], POSE_HIT, HIT_RECOIL_FRAMES);
    player_setPose(&teamB[holderB], POSE_CELEBRATE, HIT_TOTAL_FRAMES);
    impactTimer = HIT_TOTAL_FRAMES;
    hitstopTimer = 4;
    hitExitStarted = FALSE;
    state = MS_HIT_A;
    if (activeA == responderA)
        rotate_controlled_player(TRUE, FALSE);
}

static void finish_hit_to_A(void)
{
    draw_hud();
    if (count_in_play(teamA) == 0) { go_round_end(FALSE); return; }
    begin_loose_for_A();
}

/* Draw the referee (4x4, PAL3) in the same dynamic depth list as players,
 * balls and effects. These are player-derived stand/run silhouettes in an
 * official's kit, so proportions and gait match the athletes exactly. */
static void draw_referee(void)
{
    static const s8 bob[REF_FRAME_COUNT] = { 0, -1, 0, -1 };
    u8 frame = (refAnim >> 2) & (REF_FRAME_COUNT - 1);
    u16 base;
    s16 drawY = refY - 16 + bob[frame] + worldOffsetY;
    u8 priority = escortIsA ? 1 : 0;

    if (refBackView)
        base = frame == 0 ? TILE_REFEREE_BACK
                          : TILE_REFEREE_BACK_EXTRA +
                            (frame - 1) * REF_FRAME_TILE_COUNT;
    else
        base = frame == 0 ? TILE_REFEREE
                          : TILE_REFEREE_EXTRA +
                            (frame - 1) * REF_FRAME_TILE_COUNT;
    if (ui_sprite_behind_panel(refX - 8, drawY, 32, 32) ||
        court_bg_spriteBehindRoof(refX - 8, drawY, 32, 32))
        priority = 0;
    VDP_setSpriteFull(refSpriteSlot, refX - 8,
                      drawY,
                      SPRITE_SIZE(4, 4),
                      TILE_ATTR_FULL(PAL_BALL, priority,
                                     FALSE, refFacingLeft, base),
                      (u8)(refSpriteSlot + 1));
}

static void hide_referee(void)
{
    VDP_setSpriteFull(refSpriteSlot, -32, -32, SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, 0),
                      (u8)(refSpriteSlot + 1));
}

/* Big, prominent loose-ball countdown at the top-right under the HUD. Ceil to
 * whole seconds so it reads 10..1; flips to cyan for the last 3 seconds to
 * grab attention. Shown for whichever side is on the clock. */
#define SC_X 31
#define SC_Y 4
#define SC_W 8
#define SC_H 4
static void draw_shot_clock(u16 secs, bool drawPanel)
{
    u16 numberWidth = (secs >= 10) ? 4 : 2;
    char buf[4];

    intToStr(secs, buf, 1);
    ui_set_palette(PAL0);

    if (drawPanel)
    {
        /* Build the framed navy box only when the clock first appears. The
         * old code repainted both planes and PAL3 every frame, which was the
         * visible flicker. */
        ui_draw_panel(SC_X, SC_Y, SC_W, SC_H, TRUE);
    }
    else
    {
        /* A two-digit value is four tiles wide while a one-digit value is
         * only two, so clear the complete interior before drawing the next
         * cached value. This is just twelve tile writes once per second. */
        ui_clear_panel_interior(SC_X, SC_Y, SC_W, SC_H);
    }

    ui_draw_big_text(buf, SC_X + (SC_W - numberWidth) / 2, SC_Y + 1,
                     (secs <= 3) ? UI_CYAN : UI_GOLD);
}

/* The shot clock ran out on a loose ball - drop the ball on the spot and send
 * the referee on to walk the offending player off (see MS_ESCORT). */
static void trigger_pickup_timeout(void)
{
    Player *v;
    pickupClock = 0;
    escortIsA = pickupIsA;
    /* Penalise the player who is actually nearest the loose/held ball when
     * time expires. Stored active/holder slots can be stale after a rebound
     * or manual player switch and previously caused the wrong player to go. */
    escortIdx = pickupIsA ? closest_in_play(teamA, ball.x, ball.y)
                          : closest_in_play(teamB, ball.x, ball.y);
    v = escortIsA ? &teamA[escortIdx] : &teamB[escortIdx];

    /* The ball must NOT warp to the player. If it was still being held it
     * drops straight down at its current spot; if it was already loose it is
     * left completely untouched where it settled. */
    if (ball.state == BALL_HELD_A || ball.state == BALL_HELD_B)
        ball_dropAt(&ball, ball.x, ball.y);
    /* Nothing ticks the loose-ball physics during the escort, so settle the
     * ball flat on the ground now - otherwise it hangs frozen in mid-bounce
     * for the whole walk-off. */
    ball_settle(&ball);

    /* Referee enters from the corner on the player's own half so it never
     * appears to cross the centre net: bottom-right for a near-side team A
     * player, top-right for a far-side team B player. It then approaches the
     * player diagonally (see MS_ESCORT). */
    refX = 340;
    refY = escortIsA ? 206 : 60;
    escortPhase = 0;
    refAnim = 0;
    /* The official enters from the right, so the native right-facing art is
     * mirrored while moving left. Initial front/rear view follows the first
     * vertical step toward the offender. */
    refFacingLeft = TRUE;
    refBackView = v->y < refY;
    /* PAL3 indices 7/15 are unused by the ball art - lend them to the ref for
     * skin tones so it can share the ball palette line during the escort. */
    PAL_setColor(PAL_BALL * 16 + 7,  RGB24_TO_VDPCOLOR(REF_SKIN_LIGHT));
    PAL_setColor(PAL_BALL * 16 + 15, RGB24_TO_VDPCOLOR(REF_SKIN_DARK));

    player_setPose(v, POSE_STAND, 255);
    sound_mgr_whistle();
    state = MS_ESCORT;
    if (escortIsA && activeA == escortIdx)
        rotate_controlled_player(TRUE, FALSE);
}

/* Draw just the two option rows so cursor moves don't redraw the whole box. */
static void draw_pause_rows(void)
{
    ui_draw_text("RESUME",    15, PAUSE_Y + 4, (pauseRow == 0) ? UI_GOLD : UI_WHITE);
    ui_draw_text("EXIT MATCH", 15, PAUSE_Y + 6, (pauseRow == 1) ? UI_GOLD : UI_WHITE);
    /* ui_draw_text deliberately skips spaces, so drawing " " never erased
     * the previous arrow. Clear the two actual tilemap cells explicitly. */
    VDP_clearTileMapRect(BG_A, 13, PAUSE_Y + 4, 1, 1);
    VDP_clearTileMapRect(BG_A, 13, PAUSE_Y + 6, 1, 1);
    ui_draw_text(">", 13, PAUSE_Y + 4 + (pauseRow * 2), UI_GOLD);
}

static void pause_enter(void)
{
    matchPaused = TRUE;
    pauseRow = 0;
    sound_mgr_confirm();
    VDP_clearSprites();
    sprites_data_hide_all_sprites();
    ui_draw_panel(PAUSE_X, PAUSE_Y, PAUSE_W, PAUSE_H, FALSE);
    ui_draw_big_center("PAUSED", PAUSE_Y + 1, UI_GOLD);
    draw_pause_rows();
}

static void pause_resume(void)
{
    matchPaused = FALSE;
    sound_mgr_cancel();
    /* Wipe the overlay from both planes; the render tail restores the sprites.
     * NOTE: use the rectangular tilemap clear, NOT VDP_clearTextArea(). The
     * latter stamps the FONT's space tile, and the UI/title art has reclaimed
     * that font VRAM - so it painted a block of garbage (vertical white
     * stripes) over the pitch on resume. */
    VDP_clearTileMapRect(BG_A, PAUSE_X, PAUSE_Y, PAUSE_W, PAUSE_H);
    court_bg_redraw_rect(PAUSE_X, PAUSE_Y, PAUSE_W, PAUSE_H);
    /* The panel also covered part of the high-priority net overlay on BG_A,
     * which the BG_B court redraw cannot bring back - restore it explicitly. */
    court_bg_drawForeground();
    draw_hud();
    /* The pause panel may have covered the clock. Force one clean rebuild on
     * the next gameplay frame instead of trusting the cached tile state. */
    shotClockShown = FALSE;
    shotClockSecondsShown = 0;
}

static void pause_menu_update(void)
{
    if (input_pressed(BUTTON_UP) || input_pressed(BUTTON_DOWN))
    {
        pauseRow ^= 1;
        sound_mgr_blip();
        draw_pause_rows();
    }
    else if (input_pressed(BUTTON_START))
    {
        /* START is always the immediate resume shortcut, regardless of which
         * menu row is highlighted. */
        pause_resume();
    }
    else if (input_pressed(BUTTON_A) || input_pressed(BUTTON_B) ||
             input_pressed(BUTTON_C))
    {
        /* All three face buttons activate the currently highlighted row. */
        if (pauseRow == 0)
        {
            pause_resume();
        }
        else
        {
            sound_mgr_confirm();
            matchPaused = FALSE;
            screen_transition_fade_out();
            gCurrentScene = GS_MENU;
        }
    }
}

void scene_match_update(void)
{
    bool cpuMoved = FALSE;
    bool ambientMovedA[TEAM_SIZE] = { FALSE, FALSE, FALSE };
    bool ambientMovedB[TEAM_SIZE] = { FALSE, FALSE, FALSE };
    u8 i;

    input_mgr_update();

    /* Pause gate: while paused, nothing else in the loop runs, so every
     * timer, the AI and all physics are frozen until the player resumes. */
    if (matchPaused)
    {
        pause_menu_update();
        return;
    }
    /* START opens the pause menu, but never mid-elimination/round-break so
     * the overlay can't strand an in-progress escort or banner. */
    if (input_pressed(BUTTON_START) &&
        state != MS_ANNOUNCE && state != MS_ROUND_END && state != MS_ESCORT)
    {
        pause_enter();
        return;
    }

    /* Off-ball controls: A/C rotate left/right by current X position. B
     * selects the player nearest the live ball (or second-nearest when the
     * nearest is already active). During possession all three remain throws. */
    if (!teamA_has_possession() && state != MS_ROUND_END)
    {
        if (input_pressed(BUTTON_A))
            rotate_controlled_player(FALSE, TRUE);
        else if (input_pressed(BUTTON_B))
            select_controlled_nearest_to_ball(TRUE);
        else if (input_pressed(BUTTON_C))
            rotate_controlled_player(TRUE, TRUE);
    }

    impact_fx_update();

    if (++clockFrameCounter >= (SYS_isPAL() ? 50 : 60))
    {
        clockFrameCounter = 0;
        if (matchSeconds < 5999) matchSeconds++;
        draw_hud_clock();   /* clock digits only - no whole-bar repaint/flash */
    }

    if (flashTimer > 0)
    {
        flashTimer--;
        if (flashTimer == 0)
            sprites_data_apply_teams(gTeamAIndex, gTeamBIndex);
    }

    if (shakeTimer > 0)
    {
        worldOffsetY = shakePattern[6 - shakeTimer];
        VDP_setVerticalScroll(BG_B, worldOffsetY);
        shakeTimer--;
        if (shakeTimer == 0)
        {
            worldOffsetY = 0;
            VDP_setVerticalScroll(BG_B, 0);
        }
    }

    if (looseTimer > 0) looseTimer--;
    if (pickupClock > 0)
    {
        pickupClock--;
        if (pickupClock == 0)
        {
            trigger_pickup_timeout();
        }
    }
    ambientTick++;

    if (state != MS_ANNOUNCE && state != MS_ROUND_END &&
        state != MS_HIT_A && state != MS_HIT_B && state != MS_ESCORT)
    {
        for (i = 0; i < TEAM_SIZE; i++)
        {
            bool cpuBusy = (i == holderB) &&
                (state == MS_B_HOLD || state == MS_B_WINDUP || state == MS_LOOSE_B);
            if (i != activeA && !teamA[i].eliminated && !teamA[i].exiting)
                ambientMovedA[i] = move_ambient(&teamA[i], i);
            /* In 2 PLAYER VS the man player 2 is steering must never be driven
             * by the CPU wander as well, or he fights the pad. */
            if (human_b() && i == activeB) continue;
            if (!cpuBusy && !teamB[i].eliminated && !teamB[i].exiting)
            {
                if (state == MS_FLY_TO_B)
                    ambientMovedB[i] = move_cpu_evade(&teamB[i], i);
                if (!ambientMovedB[i])
                    ambientMovedB[i] = move_ambient(&teamB[i], i + TEAM_SIZE);
            }
        }
    }

    /* The player you're actively controlling always moves on input,
     * whether or not they're the one currently resolving a play - lets
     * you reposition a teammate while another exchange is in flight. */
    if (activeA_can_move())
        player_moveHuman(&teamA[activeA], activeA_has_ball());

    /* 2 PLAYER VS: player 2's man moves on input EVERY frame, exactly like
     * player 1's - previously they could only be steered while holding the
     * ball or chasing a loose one, which made the far side look CPU-run. */
    if (human_b() && !teamB[activeB].eliminated && !teamB[activeB].exiting &&
        state != MS_B_WINDUP && state != MS_HIT_A && state != MS_HIT_B &&
        state != MS_ROUND_END && state != MS_ESCORT && state != MS_ANNOUNCE)
        player_moveHumanPad(&teamB[activeB], (holderB == activeB) &&
                            (state == MS_B_HOLD), 1);

    /* Shared-team play: player 2 steers their own man on the near side. When
     * only one team mate is left, whoever already had him keeps him and the
     * other pad simply has nobody to move. */
    if (team_share())
    {
        if (teamA[activeA2].eliminated || teamA[activeA2].exiting)
        {
            u8 next = first_in_play_from(teamA, (u8)(activeA2 + 1));
            /* Never take over player 1's man unless he is the last one left. */
            if (next != activeA || count_in_play(teamA) <= 1) activeA2 = next;
        }
        if (activeA2 != activeA && !teamA[activeA2].eliminated &&
            state != MS_A_WINDUP && state != MS_HIT_A && state != MS_HIT_B &&
            state != MS_ROUND_END && state != MS_ESCORT)
            player_moveHumanPad(&teamA[activeA2], holderA == activeA2, 1);
    }

    /* Player 2 keeps a valid team B player: START cycles to the next one still
     * in, and an eliminated selection is handed on automatically. */
    if (human_b())
    {
        if (teamB[activeB].eliminated || teamB[activeB].exiting)
            activeB = first_in_play_from(teamB, (u8)(activeB + 1));
        if (input_pressed_p(1, BUTTON_START))
            activeB = first_in_play_from(teamB, (u8)(activeB + 1));
    }

    /* Watchdog: force whatever this state is waiting on to complete if
     * it's gone on far longer than any real delay ever should. */
    if (!stallTrackerInit) { stallTrackerInit = TRUE; lastState = state; stallCounter = 0; }
    if (state == lastState)
    {
        if (++stallCounter > STALL_LIMIT)
        {
            switch (state)
            {
                case MS_ANNOUNCE:  announceTimer = 0; break;
                case MS_B_HOLD:    aiDelay = 0; break;
                case MS_A_WINDUP:
                case MS_B_WINDUP:  windupTimer = 0; break;
                case MS_FLY_TO_B:
                case MS_FLY_TO_A:  ball.progress = 255; break;
                case MS_LOOSE_B:
                case MS_LOOSE_A:   looseTimer = 0; break;
                case MS_HIT_B:
                case MS_HIT_A:     impactTimer = 0; break;
                case MS_ROUND_END: roundEndTimer = 0; break;
                default: break;
            }
            stallCounter = 0;
        }
    }
    else
    {
        lastState = state;
        stallCounter = 0;
    }

    switch (state)
    {
        case MS_ANNOUNCE:
        {
            if (announceTimer > 0) announceTimer--;
            else
            {
                clear_playfield_text();
                /* Put the server on the shot clock from the very first
                 * possession, so the opening throw is timed too (not just
                 * loose-ball retrievals after the first throw). */
                pickupClock = PICKUP_CLOCK_FRAMES;
                if (server == 0)
                {
                    state = MS_A_HOLD;
                    pickupIsA = TRUE;    /* human server must throw in time */
                }
                else
                {
                    state = MS_B_HOLD;
                    aiDelay = ai_pickThrowDelay();
                    choose_ai_carrier_style();
                    pickupIsA = FALSE;
                }
            }
            break;
        }

        case MS_A_HOLD:
        {
            place_ball_in_hand(&teamA[holderA], FALSE);

            if (activeA == holderA &&
                (input_pressed(BUTTON_A) || input_pressed(BUTTON_B) ||
                 input_pressed(BUTTON_C)))
            {
                u8 lane = input_pressed(BUTTON_A) ? 0 :
                          input_pressed(BUTTON_B) ? 1 : 2;
                pendingSpin = input_held(BUTTON_LEFT) ? -1 :
                              input_held(BUTTON_RIGHT) ? 1 : 0;
                fixed_back_target(TRUE, lane, &pendingTargetX, &pendingTargetY);

                player_setPose(&teamA[holderA], POSE_THROW, 18);
                windupTimer = 8;
                state = MS_A_WINDUP;
            }
            break;
        }

        case MS_A_WINDUP:
        {
            place_ball_in_hand(&teamA[holderA], windupTimer < 4);
            if (windupTimer > 0) windupTimer--;
            else
            {
                prepare_cpu_evades();
                ball_startThrow(&ball, pendingTargetX, pendingTargetY,
                                BALL_FLYING_TO_B, pendingSpin);
                sound_mgr_throw();
                pickupClock = 0;   /* thrown in time - clock off */
                state = MS_FLY_TO_B;
            }
            break;
        }

        case MS_B_HOLD:
        {
            place_ball_in_hand(&teamB[holderB], FALSE);
            if (human_b())
            {
                /* Player 2 carries and throws exactly like player 1 does. The
                 * movement itself happens in the common per-frame block above;
                 * here we only read the throw. */
                place_ball_in_hand(&teamB[holderB], FALSE);
                if (input_pressed_p(1, BUTTON_A) || input_pressed_p(1, BUTTON_B) ||
                    input_pressed_p(1, BUTTON_C))
                {
                    u8 lane = input_pressed_p(1, BUTTON_A) ? 0 :
                              input_pressed_p(1, BUTTON_B) ? 1 : 2;
                    pendingSpin = input_held_p(1, BUTTON_LEFT) ? -1 :
                                  input_held_p(1, BUTTON_RIGHT) ? 1 : 0;
                    fixed_back_target(FALSE, lane, &pendingTargetX, &pendingTargetY);
                    player_setPose(&teamB[holderB], POSE_THROW, 18);
                    windupTimer = 8;
                    state = MS_B_WINDUP;
                }
                break;
            }
            if (aiDelay > 0 || aiCarrierMoveFloor > 0)
            {
                /* Most armed CPU possessions reposition toward a useful lane;
                 * some deliberately stay planted and throw from the pickup. */
                if (aiCarrierRepositions)
                {
                    responderA = closest_in_play(teamA,
                                                 teamB[holderB].x + PLAYER_PICKUP_DX,
                                                 teamB[holderB].y + PLAYER_PICKUP_DY);
                    cpuMoved = move_cpu_carrier(&teamB[holderB],
                                                &teamA[responderA],
                                                aiCarrierOffset);
                    if (!cpuMoved) aiCarrierMoveFloor = 0;
                }
                place_ball_in_hand(&teamB[holderB], FALSE);
                if (aiDelay > 0) aiDelay--;
                if (aiCarrierMoveFloor > 0) aiCarrierMoveFloor--;
            }
            else
            {
                plan_cpu_throw();

                player_setPose(&teamB[holderB], POSE_THROW, 18);
                windupTimer = 8;
                state = MS_B_WINDUP;
            }
            break;
        }

        case MS_B_WINDUP:
        {
            place_ball_in_hand(&teamB[holderB], windupTimer < 4);
            if (windupTimer > 0) windupTimer--;
            else
            {
                ball_startThrow(&ball, pendingTargetX, pendingTargetY,
                                BALL_FLYING_TO_A, pendingSpin);
                sound_mgr_throw();
                pickupClock = 0;
                state = MS_FLY_TO_A;
            }
            break;
        }

        case MS_FLY_TO_B:
        {
            {
                s8 hit;
                bool arrived = ball_update(&ball);
                hit = (ball.state == BALL_FLYING_TO_B)
                    ? first_ball_hit(teamB) : -1;
                if (hit >= 0) { responderB = (u8)hit; resolve_throw_to_B(); }
                else if (arrived)
                    resolve_throw_to_B();
            }
            break;
        }

        case MS_FLY_TO_A:
        {
            {
                s8 hit;
                bool arrived = ball_update(&ball);
                hit = (ball.state == BALL_FLYING_TO_A)
                    ? first_ball_hit(teamA) : -1;
                if (hit >= 0) { responderA = (u8)hit; resolve_throw_to_A(); }
                else if (arrived)
                    resolve_throw_to_A();
            }
            break;
        }

        case MS_LOOSE_B:
        {
            update_loose_ball();
            if (human_b())
            {
                /* Player 2 chases the loose ball themselves - no AI reaction
                 * delay, and no nearest-player reassignment: the man they are
                 * steering is the one who can collect it. Movement comes from
                 * the common per-frame block above. */
                aiLooseReact = 0;
                holderB = activeB;
                cpuMoved = TRUE;
            }
            /* Hesitate for the difficulty-scaled reaction window before the CPU
             * commits to the ball, so EASY gives the human a real head start. */
            else if (aiLooseReact > 0) aiLooseReact--;
            else
            {
                /* Reassign every frame as the rebound rolls: this is the same
                 * nearest-eligible-player principle used by Eliminator. */
                holderB = closest_in_play(teamB, ball.x, ball.y);
                cpuMoved = move_toward_ball(&teamB[holderB]);
            }
            if (aiLooseReact == 0 && looseTimer == 0 &&
                player_reached_ball(&teamB[holderB]))
            {
                sound_mgr_pickup();
                player_setPose(&teamB[holderB], POSE_PICKUP, 10);
                ball_init(&ball, SLOT_BALL, 0, 0, BALL_HELD_B);
                place_ball_in_hand(&teamB[holderB], FALSE);
                aiDelay = ai_pickThrowDelay();
                choose_ai_carrier_style();
                state = MS_B_HOLD;
            }
            break;
        }

        case MS_LOOSE_A:
        {
            update_loose_ball();
            if (looseTimer == 0 && player_reached_ball(&teamA[activeA]))
            {
                holderA = activeA;
                sound_mgr_pickup();
                player_setPose(&teamA[holderA], POSE_PICKUP, 10);
                ball_init(&ball, SLOT_BALL, 0, 0, BALL_HELD_A);
                place_ball_in_hand(&teamA[holderA], FALSE);
                state = MS_A_HOLD;
            }
            break;
        }

        case MS_HIT_B:
            /* Freeze the world action for four frames while the palette flash
             * and shake continue. This is short enough to stay responsive but
             * gives the contact frame a deliberate arcade impact. */
            if (hitstopTimer > 0) { hitstopTimer--; break; }
            update_loose_ball();
            if (impactTimer > 0)
            {
                impactTimer--;
                advance_hit_recoil(&teamB[responderB]);
                if (impactTimer == HIT_FALL_FRAMES)
                {
                    player_setPose(&teamB[responderB], POSE_FALL, HIT_FALL_FRAMES);
                    sound_mgr_crowdKnockout();
                }
            }
            else if (!hitExitStarted)
            {
                eliminate_from(teamB, responderB);
                hitExitStarted = TRUE;
            }
            else if (player_updateExit(&teamB[responderB])) finish_hit_to_B();
            break;

        case MS_HIT_A:
            if (hitstopTimer > 0) { hitstopTimer--; break; }
            update_loose_ball();
            if (impactTimer > 0)
            {
                impactTimer--;
                advance_hit_recoil(&teamA[responderA]);
                if (impactTimer == HIT_FALL_FRAMES)
                {
                    player_setPose(&teamA[responderA], POSE_FALL, HIT_FALL_FRAMES);
                    sound_mgr_crowdKnockout();
                }
            }
            else if (!hitExitStarted)
            {
                eliminate_from(teamA, responderA);
                hitExitStarted = TRUE;
            }
            else if (player_updateExit(&teamA[responderA])) finish_hit_to_A();
            break;

        case MS_ESCORT:
        {
            Player *v = escortIsA ? &teamA[escortIdx] : &teamB[escortIdx];
            const s16 SPD = 2;
            s16 oldRefX = refX;
            s16 oldRefY = refY;

            update_loose_ball();  /* keep it settled */
            refAnim++;

            if (escortPhase == 0)
            {
                /* Diagonal approach from the corner: close in x and drift in y
                 * onto the player's line, staying on their half of the court. */
                if (refX > v->x + 12) refX -= SPD;
                if (refY < v->y) refY++;
                else if (refY > v->y) refY--;
                if (refX <= v->x + 12 && refY == v->y)
                {
                    escortPhase = 1;
                    player_setPose(v, POSE_RUN, 255);
                    v->facingLeft = FALSE;
                }
            }
            else
            {
                /* Walk the player back out to the exact corner the ref came in
                 * from (bottom-right for team A, top-right for team B), so the
                 * ref exits where it entered and never crosses the net. */
                s16 cornerY = escortIsA ? 206 : 60;
                refX += SPD;
                v->x += SPD;
                if (v->y < cornerY) v->y++;
                else if (v->y > cornerY) v->y--;
                refY = v->y;
                if ((ambientTick & 3) == 0)
                    v->animFrame = (v->animFrame + 1) & PLAYER_ANIM_MASK;

                if (v->x > 336)
                {
                    eliminate_from(escortIsA ? teamA : teamB, escortIdx);
                    if (escortIsA)
                    {
                        if (count_in_play(teamA) == 0) { go_round_end(FALSE); break; }
                        begin_loose_for_A();   /* ball still on the spot -> restart clock */
                    }
                    else
                    {
                        if (count_in_play(teamB) == 0) { go_round_end(TRUE); break; }
                        begin_loose_for_B();
                    }
                }
            }
            /* Face from the movement that actually happened this frame.
             * Horizontal travel controls mirroring; vertical travel selects
             * front/rear anatomy and remains remembered on flat steps. */
            if (refX != oldRefX) refFacingLeft = refX < oldRefX;
            if (refY != oldRefY) refBackView = refY < oldRefY;
            break;
        }

        case MS_ROUND_END:
        {
            if (roundEndTimer > 0) roundEndTimer--;
            else
            {
                if ((gScoreA >= WIN_SCORE) || (gScoreB >= WIN_SCORE))
                {
                    /* Tournament: a match win advances the gauntlet to the
                     * next opponent instead of ending the game - unless that
                     * was the final, in which case you're crowned champion. */
                    if (gGameMode == MODE_TOURNAMENT && gScoreA >= WIN_SCORE)
                    {
                        /* Record the win and simulate the other ties in this
                         * round, so the bracket fills in like a real cup. */
                        cup_advance(gTeamAIndex, gCupStage);
                        gCupStage++;
                        if (gCupStage >= CUP_ROUNDS)
                        {
                            /* Won the final: go back to the bracket so the
                             * player sees the completed competition with their
                             * name as winner (exit only, nothing to continue). */
                            screen_transition_fade_out();
                            gMenuEntry = MENU_ENTRY_CUP_LADDER;
                            gCurrentScene = GS_MENU;
                            return;
                        }
                        gTeamBIndex = cup_opponent_now(gTeamAIndex, gCupStage);
                        gScoreA = 0;
                        gScoreB = 0;
                        screen_transition_fade_out();
                        /* Back to the ladder so the player sees the bracket
                         * update (beaten rival ticked, next flagged NOW) and
                         * can Continue or Exit before the next tie. */
                        gMenuEntry = MENU_ENTRY_CUP_LADDER;
                        gCurrentScene = GS_MENU;
                        return;
                    }
                    if (gGameMode == MODE_TOURNAMENT)
                    {
                        /* A tournament defeat still has to resolve this tie
                         * before the timed result screen returns to the
                         * bracket. Advance the rival and the simulated ties,
                         * then show the next-round board with the player out. */
                        cup_advance(gTeamBIndex, gCupStage);
                        gCupStage++;
                    }
                    screen_transition_fade_out();
                    gCurrentScene = GS_GAMEOVER;
                    return;
                }

                clear_playfield_text();
                start_round();
            }
            break;
        }
    }

    /* The FIFA-style lower-third opens on a clean stadium establishing
     * shot. Gameplay sprites appear together when the banner clears,
     * avoiding the hardware-sprite layer cutting through the graphic. */
    if (state == MS_ANNOUNCE)
    {
        VDP_clearSprites();
        return;
    }

    /* Activate the clock mask before drawing sprites on its first frame. That
     * prevents a high-priority player or ball punching through before the
     * panel's tilemap reaches the VDP. */
    ui_set_sprite_panel_mask(pickupClock > 0,
                             SC_X * 8, SC_Y * 8, SC_W * 8, SC_H * 8);

    /* Advance every pose before assigning slots. Hit/fall/run frames carry
     * authored pixel offsets, and the sorter must see the same frame that is
     * about to be drawn or a knockout can be one depth step out of date. */
    for (i = 0; i < TEAM_SIZE; i++)
    {
        bool aMoving = teamA[i].exiting || ambientMovedA[i] ||
                       ((i == activeA) && !teamA[i].eliminated &&
                       (input_held(BUTTON_LEFT) || input_held(BUTTON_RIGHT) ||
                        input_held(BUTTON_UP) || input_held(BUTTON_DOWN)));
        player_tickAnim(&teamA[i], aMoving);

        bool bMoving = teamB[i].exiting || ambientMovedB[i] ||
                       (cpuMoved && (i == holderB) &&
                        (state == MS_LOOSE_B || state == MS_B_HOLD));
        player_tickAnim(&teamB[i], bMoving);
    }

    /* Sprite table order is visual depth on equal-priority hardware sprites.
     * Rebuild it from the current rendered feet so nearer bodies cover farther
     * ones throughout hit, fall and eliminated run-off animation. */
    assign_actor_depth_slots();

    /* Draw every player on both sides. Fully eliminated actors remain parked
     * at the tail of the linked list and cannot disturb visible depth slots. */
    for (i = 0; i < TEAM_SIZE; i++)
    {
        teamA[i].y += worldOffsetY;
        player_draw(&teamA[i]);
        teamA[i].y -= worldOffsetY;

        teamB[i].y += worldOffsetY;
        player_draw(&teamB[i]);
        teamB[i].y -= worldOffsetY;
    }

    ball.y += worldOffsetY;
    ball_draw(&ball);
    ball.y -= worldOffsetY;
    impact_fx_draw(worldOffsetY);
    if (state == MS_ESCORT) draw_referee();
    else hide_referee();
    draw_control_marker();
    draw_ball_air_dots();

    /* On-screen shot clock for whichever side is on the loose-ball timer. */
    if (pickupClock > 0)
    {
        u16 fps = SYS_isPAL() ? 50 : 60;
        u16 secs = (pickupClock + fps - 1) / fps;

        if (!shotClockShown)
        {
            draw_shot_clock(secs, TRUE);
            shotClockShown = TRUE;
            shotClockSecondsShown = secs;
        }
        else if (secs != shotClockSecondsShown)
        {
            draw_shot_clock(secs, FALSE);
            shotClockSecondsShown = secs;
        }
    }
    else if (shotClockShown)
    {
        /* Remove the box and restore the court that was behind it. */
        VDP_clearTileMapRect(BG_A, SC_X, SC_Y, SC_W, SC_H);
        court_bg_redraw_rect(SC_X, SC_Y, SC_W, SC_H);
        shotClockShown = FALSE;
        shotClockSecondsShown = 0;
    }
}
