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

#define SLOT_TEAM_A   0    /* teamA uses sprite slots 0,1,2 */
#define SLOT_TEAM_B   3    /* teamB uses sprite slots 3,4,5 */
#define SLOT_BALL     6    /* ball uses slots 6 (ball) and 7 (shadow) */
#define SLOT_MARKER   8    /* coloured ground star (see draw_control_marker) */
#define SLOT_SHADOWS  9    /* six grounded player shadows use slots 9..14 */

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
static u16 roundEndTimer;
static u8  windupTimer;
static u8  impactTimer;
static bool hitExitStarted;
static u8  looseTimer;      /* prevents instant pickup at the impact point */
static s8  pendingSpin;
static s16 pendingTargetX;
static s16 pendingTargetY;
static u8  server;          /* 0 = team A serves, 1 = team B serves */
static u8  roundWinnerIsA;

static u8  flashTimer;      /* frames left in the current impact flash, 0 = none */
static u8  shakeTimer;      /* frames left in the current screen shake, 0 = none */
static s8  worldOffsetY;    /* applied to BG_B and every world sprite together */

/* Safety watchdog: if the match state hasn't changed for an
 * unreasonably long time (STALL_LIMIT frames), something has gone
 * wrong - force the current state's natural timer/condition to
 * complete rather than let a real player get soft-locked waiting on
 * the CPU forever. Discovered during extended live playtesting: the
 * CPU-serve path could sit indefinitely with no visible cause found
 * on code review, so this is a defensive net on top of, not a
 * replacement for, actually finding that root cause later. */
#define STALL_LIMIT   400   /* ~6.6s at 60fps - well beyond any real delay */
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

/* Wide ground ring under the controlled player: yellow while defending or
 * moving without the ball, red while holding/winding up a throw. */
static void draw_control_marker(void)
{
    Player *p = &teamA[activeA];
    bool hasBall = (activeA == holderA) &&
                   (state == MS_A_HOLD || state == MS_A_WINDUP ||
                    (state == MS_ANNOUNCE && server == 0));
    u16 markerTile = hasBall ? TILE_RING_RED : TILE_RING_YELLOW;
    if (p->eliminated)
    {
        VDP_setSpriteFull(SLOT_MARKER, -24, -24, SPRITE_SIZE(3, 2),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, markerTile),
                           SLOT_SHADOWS);
        return;
    }
    /* A 24px open ellipse stays readable without covering the runner's feet. */
    VDP_setSpriteFull(SLOT_MARKER, p->x - 4, p->y + 8 + worldOffsetY, SPRITE_SIZE(3, 2),
                       TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, markerTile),
                       SLOT_SHADOWS);
}

static void hide_unselected_player_dots(void)
{
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        u8 slotA = SLOT_SHADOWS + i;
        u8 slotB = SLOT_SHADOWS + TEAM_SIZE + i;
        /* The old six one-tile shadows read as selection dots under every
         * player. The wide two-tile control marker is the sole indicator;
         * retain these slots only to keep the hardware sprite link intact. */
        VDP_setSpriteFull(slotA, -16, -16, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, TILE_BALL_SHADOW),
                           slotA + 1);
        VDP_setSpriteFull(slotB, -16, -16, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, TILE_BALL_SHADOW),
                           (i == TEAM_SIZE - 1) ? 0 : (slotB + 1));
    }
}

static s16 lane_x(u8 i)
{
    return COURT_LEFT_X + (s16)((i + 1) * (COURT_RIGHT_X - COURT_LEFT_X) / (TEAM_SIZE + 1));
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

/* Picks the Nth in-play slot (0-based) on a team - used to turn an
 * ai_pickSlot() count-based roll into a real team index. */
static u8 nth_in_play(Player team[], u8 n)
{
    u8 idx, seen = 0;
    for (idx = 0; idx < TEAM_SIZE; idx++)
    {
        if (team[idx].eliminated) continue;
        if (seen == n) return idx;
        seen++;
    }
    return 0;
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
        dx = team[i].x + 4 - x;
        dy = team[i].y + 5 - y;
        distance = (u32)(dx * dx + dy * dy);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

static bool move_toward_ball(Player *p)
{
    bool moved = FALSE;
    s16 targetX = ball.x - 4;
    s16 targetY = ball.y - 5;
    if (p->x < targetX - 1) { p->x += PLAYER_SPEED; moved = TRUE; }
    else if (p->x > targetX + 1) { p->x -= PLAYER_SPEED; moved = TRUE; }
    if (p->y < targetY - 1) { p->y++; moved = TRUE; }
    else if (p->y > targetY + 1) { p->y--; moved = TRUE; }
    player_clampToCourt(p);
    return moved;
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
    s16 depth = farSide ? (COURT_FAR_DEPTH + 12)
                        : (COURT_NEAR_DEPTH - 12);
    /* Derive the three back-court points from the projected side walls at
     * this depth. The near edge is wider/shifted left; using the old flat
     * lane_x values would not line up with the isometric court. */
    s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
    s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;
    *x = minX + (s16)((maxX - minX) * lane / 2);
    *y = depth + (*x >> 2);
}

static bool player_reached_ball(const Player *p)
{
    return abs((p->x + 4) - ball.x) <= PICKUP_WINDOW_X &&
           abs((p->y + 5) - ball.y) <= PICKUP_WINDOW_Y;
}

static void eliminate_from(Player team[], u8 idx)
{
    player_eliminate(&team[idx]);
}

static void reset_team(Player team[], u8 baseSlot, u8 pal, s16 baseDepth, bool facingLeft)
{
    static const s8 depthOffset[TEAM_SIZE] = { -8, 10, 0 };
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        s16 x = lane_x(i);
        s16 y = baseDepth + depthOffset[i] + (x >> 2);
        player_init(&team[i], x, y, baseSlot + i, pal);
        team[i].farSide = facingLeft;
        team[i].facingLeft = facingLeft;
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
    ui_draw_text(teamNames[gTeamAIndex], 4, 1, UI_WHITE);
    ui_draw_text(teamNames[gTeamBIndex], 36 - strlen(teamNames[gTeamBIndex]), 1, UI_WHITE);

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

static void draw_match_intro(void)
{
    char roundBuf[4];
    u8 roundNumber = gScoreA + gScoreB + 1;
    ui_draw_panel(2, 20, 36, 7, TRUE);
    ui_draw_text("ROUND", 16, 21, UI_CYAN);
    intToStr(roundNumber, roundBuf, 1);
    ui_draw_text(roundBuf, 22, 21, UI_GOLD);
    flag_data_draw_large(gTeamAIndex, 4, 23, PAL3);
    flag_data_draw_large(gTeamBIndex, 32, 23, PAL3);
    ui_draw_text(teamNames[gTeamAIndex], 9, 23, UI_WHITE);
    ui_draw_text(teamNames[gTeamBIndex], 31 - strlen(teamNames[gTeamBIndex]), 23, UI_WHITE);
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
    draw_hud();
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
        ball_init(&ball, SLOT_BALL, teamA[holderA].x + 9, teamA[holderA].y - 11, BALL_HELD_A);
    }
    else
    {
        holderB = first_in_play_from(teamB, ai_pickSlot(TEAM_SIZE));
        ball_init(&ball, SLOT_BALL, teamB[holderB].x - 3, teamB[holderB].y - 8, BALL_HELD_B);
    }
}

static void start_round(void)
{
    reset_team(teamA, SLOT_TEAM_A, PAL_TEAM_A, TEAM_A_DEPTH, FALSE);
    reset_team(teamB, SLOT_TEAM_B, PAL_TEAM_B, TEAM_B_DEPTH, TRUE);
    /* Both sides use the same 32x32 art. Perspective comes from placement,
     * shadows and the court projection—not "men versus midgets" scaling. */

    activeA = 0;

    draw_hud();
    begin_announce();
}

void scene_match_enter(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearSprites();
    VDP_setTextPalette(PAL0);
    /* VDP_clearPlane alone can leave stale text tiles behind on a scene
     * change; explicitly clearing the text area guarantees a clean slate. */
    VDP_clearTextArea(0, 0, 40, 28);
    court_bg_draw();

    /* Recolor the shared player tile art to the teams actually picked on
     * the menu - without this both sides always rendered in the same
     * hardcoded colors no matter which team you chose. */
    sprites_data_apply_teams(gTeamAIndex, gTeamBIndex);

    flashTimer = 0;
    shakeTimer = 0;
    looseTimer = 0;
    worldOffsetY = 0;
    matchSeconds = 0;
    clockFrameCounter = 0;
    VDP_setVerticalScroll(BG_B, 0);
    stallTrackerInit = FALSE;
    server = 0;
    start_round();
}

static void go_round_end(u8 winnerIsA)
{
    roundWinnerIsA = winnerIsA;
    sound_mgr_score();

    if (winnerIsA) gScoreA++;
    else gScoreB++;

    draw_hud();

    clear_playfield_text();

    /* Losing team serves next, to keep matches competitive. */
    server = winnerIsA ? 1 : 0;
    roundEndTimer = 90;
    state = MS_ROUND_END;
}

static void begin_loose_for_B(void)
{
    holderB = closest_in_play(teamB, ball.x, ball.y);
    /* A miss is still airborne and needs its landing ricochet. After a
     * hit, ball_dropAt() has already made a bounded loose ball at the
     * victim's feet; do not relaunch it or change receiving halves. */
    if (ball.state != BALL_LOOSE) ball_startRicochet(&ball);
    looseTimer = 10;
    state = MS_LOOSE_B;
}

static void begin_loose_for_A(void)
{
    activeA = closest_in_play(teamA, ball.x, ball.y);
    holderA = activeA;
    if (ball.state != BALL_LOOSE) ball_startRicochet(&ball);
    looseTimer = 10;
    state = MS_LOOSE_A;
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
        return;
    }

    responderB = (u8)hit;
    ball_dropAt(&ball, teamB[responderB].x + 4, teamB[responderB].y + 5);
    sound_mgr_hit();
    trigger_flash(PAL_TEAM_B);
    trigger_shake();
    player_setPose(&teamB[responderB], POSE_HIT, 14);
    teamB[responderB].x += (ball.spin < 0) ? -6 : 6;
    impactTimer = 8;
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
        return;
    }

    responderA = (u8)hit;
    ball_dropAt(&ball, teamA[responderA].x + 4, teamA[responderA].y + 5);
    sound_mgr_hit();
    trigger_flash(PAL_TEAM_A);
    trigger_shake();
    player_setPose(&teamA[responderA], POSE_HIT, 14);
    teamA[responderA].x += (ball.spin < 0) ? -6 : 6;
    impactTimer = 8;
    hitExitStarted = FALSE;
    state = MS_HIT_A;
}

static void finish_hit_to_A(void)
{
    draw_hud();
    if (count_in_play(teamA) == 0) { go_round_end(FALSE); return; }
    begin_loose_for_A();
}

void scene_match_update(void)
{
    bool cpuMoved = FALSE;
    u8 i;

    input_mgr_update();

    if (++clockFrameCounter >= (SYS_isPAL() ? 50 : 60))
    {
        clockFrameCounter = 0;
        if (matchSeconds < 5999) matchSeconds++;
        draw_hud();
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

    /* The player you're actively controlling always moves on input,
     * whether or not they're the one currently resolving a play - lets
     * you reposition a teammate while another exchange is in flight. */
    if (!teamA[activeA].eliminated &&
        state != MS_A_WINDUP && state != MS_HIT_A &&
        state != MS_HIT_B && state != MS_ROUND_END)
        player_moveHuman(&teamA[activeA]);

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
                if (server == 0) state = MS_A_HOLD;
                else { state = MS_B_HOLD; aiDelay = ai_pickThrowDelay(); }
            }
            break;
        }

        case MS_A_HOLD:
        {
            ball.x = teamA[holderA].x + 9;
            ball.y = teamA[holderA].y - 11;

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
            ball.x = teamA[holderA].x + (windupTimer < 4 ? 13 : 9);
            ball.y = teamA[holderA].y - (windupTimer < 4 ? 17 : 11);
            if (windupTimer > 0) windupTimer--;
            else
            {
                ball_startThrow(&ball, pendingTargetX, pendingTargetY,
                                BALL_FLYING_TO_B, pendingSpin);
                sound_mgr_throw();
                state = MS_FLY_TO_B;
            }
            break;
        }

        case MS_B_HOLD:
        {
            if (aiDelay > 0) aiDelay--;
            else
            {
                u8 lane = ai_pickSlot(TEAM_SIZE);
                responderA = nth_in_play(teamA, ai_pickSlot(count_in_play(teamA)));
                /* With C now reserved for right-lane throws, defence
                 * automatically hands control to the targeted player. */
                activeA = responderA;
                fixed_back_target(FALSE, lane, &pendingTargetX, &pendingTargetY);
                pendingSpin = (random() % 3) - 1;

                player_setPose(&teamB[holderB], POSE_THROW, 18);
                windupTimer = 8;
                state = MS_B_WINDUP;
            }
            break;
        }

        case MS_B_WINDUP:
        {
            ball.x = teamB[holderB].x - (windupTimer < 4 ? 7 : 3);
            ball.y = teamB[holderB].y - (windupTimer < 4 ? 14 : 8);
            if (windupTimer > 0) windupTimer--;
            else
            {
                ball_startThrow(&ball, pendingTargetX, pendingTargetY,
                                BALL_FLYING_TO_A, pendingSpin);
                sound_mgr_throw();
                state = MS_FLY_TO_A;
            }
            break;
        }

        case MS_FLY_TO_B:
        {
            {
                s8 hit;
                bool arrived = ball_update(&ball);
                hit = first_ball_hit(teamB);
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
                hit = first_ball_hit(teamA);
                if (hit >= 0) { responderA = (u8)hit; resolve_throw_to_A(); }
                else if (arrived)
                    resolve_throw_to_A();
            }
            break;
        }

        case MS_LOOSE_B:
        {
            if (ball_updateLoose(&ball)) sound_mgr_bounce();
            cpuMoved = move_toward_ball(&teamB[holderB]);
            if (looseTimer == 0 && player_reached_ball(&teamB[holderB]))
            {
                sound_mgr_pickup();
                player_setPose(&teamB[holderB], POSE_PICKUP, 10);
                ball_init(&ball, SLOT_BALL, teamB[holderB].x - 3,
                          teamB[holderB].y - 8, BALL_HELD_B);
                aiDelay = ai_pickThrowDelay();
                state = MS_B_HOLD;
            }
            break;
        }

        case MS_LOOSE_A:
        {
            if (ball_updateLoose(&ball)) sound_mgr_bounce();
            if (looseTimer == 0 && player_reached_ball(&teamA[activeA]))
            {
                holderA = activeA;
                sound_mgr_pickup();
                player_setPose(&teamA[holderA], POSE_PICKUP, 10);
                ball_init(&ball, SLOT_BALL, teamA[holderA].x + 9,
                          teamA[holderA].y - 11, BALL_HELD_A);
                state = MS_A_HOLD;
            }
            break;
        }

        case MS_HIT_B:
            if (ball_updateLoose(&ball)) sound_mgr_bounce();
            if (impactTimer > 0) impactTimer--;
            else if (!hitExitStarted)
            {
                eliminate_from(teamB, responderB);
                hitExitStarted = TRUE;
            }
            else if (player_updateExit(&teamB[responderB])) finish_hit_to_B();
            break;

        case MS_HIT_A:
            if (ball_updateLoose(&ball)) sound_mgr_bounce();
            if (impactTimer > 0) impactTimer--;
            else if (!hitExitStarted)
            {
                eliminate_from(teamA, responderA);
                hitExitStarted = TRUE;
            }
            else if (player_updateExit(&teamA[responderA])) finish_hit_to_A();
            break;

        case MS_ROUND_END:
        {
            if (roundEndTimer > 0) roundEndTimer--;
            else
            {
                if ((gScoreA >= WIN_SCORE) || (gScoreB >= WIN_SCORE))
                {
                    PAL_fadeOutAll(20, FALSE);
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

    /* Animate + draw every player on both sides (eliminated ones are
     * parked off-screen by player_eliminate() so this stays simple). */
    for (i = 0; i < TEAM_SIZE; i++)
    {
        bool aMoving = teamA[i].exiting || ((i == activeA) && !teamA[i].eliminated &&
                       (input_held(BUTTON_LEFT) || input_held(BUTTON_RIGHT) ||
                        input_held(BUTTON_UP) || input_held(BUTTON_DOWN)));
        player_tickAnim(&teamA[i], aMoving);
        teamA[i].y += worldOffsetY;
        player_draw(&teamA[i]);
        teamA[i].y -= worldOffsetY;

        bool bMoving = teamB[i].exiting ||
                       (cpuMoved && (i == holderB) && (state == MS_LOOSE_B));
        player_tickAnim(&teamB[i], bMoving);
        teamB[i].y += worldOffsetY;
        player_draw(&teamB[i]);
        teamB[i].y -= worldOffsetY;
    }

    ball.y += worldOffsetY;
    ball_draw(&ball);
    ball.y -= worldOffsetY;
    draw_control_marker();
    hide_unselected_player_dots();
}
