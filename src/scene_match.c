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
static s8  outStackA[TEAM_SIZE], outStackB[TEAM_SIZE];  /* elimination order, LIFO */
static u8  outCountA, outCountB;

static u16 announceTimer;
static u16 aiDelay;
static u16 roundEndTimer;
static u8  windupTimer;
static u8  impactTimer;
static s8  pendingSpin;
static s16 pendingTargetX;
static s16 pendingTargetY;
static u8  server;          /* 0 = team A serves, 1 = team B serves */
static u8  roundWinnerIsA;

static u8  flashTimer;      /* frames left in the current impact flash, 0 = none */
static u8  shakeTimer;      /* frames left in the current screen shake, 0 = none */

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
 * of frames - real "impact" feedback on both a catch and a hit, not
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

/* Ground star under the controlled player: yellow while defending or
 * moving without the ball, red while holding/winding up a throw. */
static void draw_control_marker(void)
{
    Player *p = &teamA[activeA];
    bool hasBall = (activeA == holderA) &&
                   (state == MS_A_HOLD || state == MS_A_WINDUP ||
                    (state == MS_ANNOUNCE && server == 0));
    u16 markerTile = hasBall ? TILE_MARKER_RED : TILE_MARKER_YELLOW;
    VDP_setSpriteFull(SLOT_MARKER, p->x, p->y + 10, SPRITE_SIZE(2, 1),
                       TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, markerTile),
                       SLOT_SHADOWS);
}

static void draw_player_shadows(void)
{
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        u8 slotA = SLOT_SHADOWS + i;
        u8 slotB = SLOT_SHADOWS + TEAM_SIZE + i;
        VDP_setSpriteFull(slotA, teamA[i].x + 4, teamA[i].y + 10, SPRITE_SIZE(1, 1),
                           TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, TILE_BALL_SHADOW),
                           slotA + 1);
        VDP_setSpriteFull(slotB, teamB[i].x + 4, teamB[i].y + 10, SPRITE_SIZE(1, 1),
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

/* A/B/C address the visible left/middle/right opponent lanes. If that
 * exact slot is out, use the nearest surviving lane rather than silently
 * choosing a random target. */
static u8 lane_target(Player team[], u8 requested)
{
    u8 distance;
    if (!team[requested].eliminated) return requested;
    for (distance = 1; distance < TEAM_SIZE; distance++)
    {
        if (requested >= distance && !team[requested - distance].eliminated)
            return requested - distance;
        if (requested + distance < TEAM_SIZE && !team[requested + distance].eliminated)
            return requested + distance;
    }
    return first_in_play_from(team, 0);
}

static bool ball_overlaps_player(const Player *p)
{
    /* Continuous feet/torso box, checked every flight frame. This replaces
     * the old endpoint-only test against the original target coordinates. */
    return ball.progress >= 96 &&
           abs((p->x + 4) - ball.x) <= CATCH_WINDOW_X &&
           abs((p->y - 3) - ball.y) <= CATCH_WINDOW_Y;
}

static void eliminate_from(Player team[], u8 idx, s8 outStack[], u8 *outCount)
{
    player_eliminate(&team[idx]);
    outStack[*outCount] = (s8)idx;
    (*outCount)++;
}

/* Most-recently-eliminated player comes back first. */
static void return_one(Player team[], s8 outStack[], u8 *outCount)
{
    if (*outCount == 0) return;
    u8 idx = (u8)outStack[--(*outCount)];
    player_restore(&team[idx]);
}

static void reset_team(Player team[], u8 baseSlot, u8 pal, s16 baseDepth)
{
    static const s8 depthOffset[TEAM_SIZE] = { -8, 10, 0 };
    u8 i;
    for (i = 0; i < TEAM_SIZE; i++)
    {
        s16 x = lane_x(i);
        s16 y = baseDepth + depthOffset[i] + (x >> 2);
        player_init(&team[i], x, y, baseSlot + i, pal);
    }
}

static void draw_hud(void)
{
    VDP_drawText("A:LEFT B:MID C:RIGHT  HOLD <> SPIN", 3, 0);
    VDP_drawTextFill(teamNames[gTeamAIndex], 1, 1, 14);
    VDP_drawTextFill(teamNames[gTeamBIndex], 25, 1, 14);

    char buf[8];
    intToStr(gScoreA, buf, 1);
    VDP_drawTextFill(buf, 18, 1, 2);
    VDP_drawText("-", 20, 1);
    intToStr(gScoreB, buf, 1);
    VDP_drawTextFill(buf, 21, 1, 2);

    VDP_drawText("IN", 1, 2);
    intToStr(count_in_play(teamA), buf, 1);
    VDP_drawTextFill(buf, 4, 2, 2);

    VDP_drawText("IN", 36, 2);
    intToStr(count_in_play(teamB), buf, 1);
    VDP_drawTextFill(buf, 39, 2, 1);
}

/* SGDK's text-line clear writes opaque font-space tiles, which created
 * the full-width black stripe that made the old court look corrupted.
 * Clearing the plane's tilemap and restoring only the compact HUD makes
 * the isometric BG_B court visible again everywhere else. */
static void clear_playfield_text(void)
{
    VDP_clearPlane(VDP_BG_A, TRUE);
    draw_hud();
}

static void begin_announce(void)
{
    /* Keep state messaging out of the projected playfield. The old
     * full-width text row broke the court into two flat halves. */
    clear_playfield_text();

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
    reset_team(teamA, SLOT_TEAM_A, PAL_TEAM_A, TEAM_A_DEPTH);
    reset_team(teamB, SLOT_TEAM_B, PAL_TEAM_B, TEAM_B_DEPTH);
    /* Both sides use the same 32x32 art. Perspective comes from placement,
     * shadows and the court projection—not "men versus midgets" scaling. */

    outCountA = 0;
    outCountB = 0;
    activeA = 0;

    draw_hud();
    begin_announce();
}

void scene_match_enter(void)
{
    VDP_clearPlane(VDP_BG_A, TRUE);
    VDP_clearPlane(VDP_BG_B, TRUE);
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
    VDP_setVerticalScroll(VDP_BG_B, 0);
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

/* Resolves a completed throw at team B: catch = the thrower (holderA)
 * is eliminated and a teammate returns for team A; a miss/hit = the
 * responder is eliminated and team A keeps the ball. */
static void resolve_throw_to_B(void)
{
    bool inRange = ball_overlaps_player(&teamB[responderB]);
    bool caught = inRange && ai_willCatch();

    if (!inRange)
    {
        /* A genuine miss: no invisible endpoint hit. The defending lane
         * recovers the loose ball and play continues. */
        holderB = first_in_play_from(teamB, responderB);
        ball_init(&ball, SLOT_BALL, teamB[holderB].x - 3,
                  teamB[holderB].y - 8, BALL_HELD_B);
        state = MS_B_HOLD;
        aiDelay = ai_pickThrowDelay();
        return;
    }

    if (caught)
    {
        trigger_flash(PAL_TEAM_B);
        sound_mgr_catch();
        player_setPose(&teamB[responderB], POSE_CATCH, 10);
        eliminate_from(teamA, holderA, outStackA, &outCountA);
        return_one(teamB, outStackB, &outCountB);

        if (count_in_play(teamA) == 0) { go_round_end(FALSE); return; }

        holderB = responderB;
        activeA = first_in_play_from(teamA, activeA);
        draw_hud();
        ball_init(&ball, SLOT_BALL, teamB[holderB].x - 3, teamB[holderB].y - 8, BALL_HELD_B);
        state = MS_B_HOLD;
        aiDelay = ai_pickThrowDelay();
    }
    else
    {
        sound_mgr_hit();
        trigger_flash(PAL_TEAM_B);
        trigger_shake();
        player_setPose(&teamB[responderB], POSE_HIT, 12);
        teamB[responderB].x += (ball.spin < 0) ? -5 : 5;
        impactTimer = 8;
        state = MS_HIT_B;
    }
}

static void finish_hit_to_B(void)
{
    eliminate_from(teamB, responderB, outStackB, &outCountB);
    draw_hud();
    if (count_in_play(teamB) == 0) { go_round_end(TRUE); return; }
    holderA = first_in_play_from(teamA, holderA);
    activeA = holderA;
    ball_init(&ball, SLOT_BALL, teamA[holderA].x + 9,
              teamA[holderA].y - 11, BALL_HELD_A);
    state = MS_A_HOLD;
}

/* Same resolution, mirrored for a throw at team A. The responder might
 * be the human's actively-controlled slot (needs BUTTON_A held in
 * range) or a teammate the human isn't currently driving (falls back
 * to the same catch-chance roll the CPU uses). */
static void resolve_throw_to_A(void)
{
    bool isHuman = (responderA == activeA);
    bool inRange = ball_overlaps_player(&teamA[responderA]);
    bool caught = isHuman ? (inRange && input_held(BUTTON_A))
                           : (inRange && ai_willCatch());

    if (!inRange)
    {
        holderA = first_in_play_from(teamA, responderA);
        activeA = holderA;
        ball_init(&ball, SLOT_BALL, teamA[holderA].x + 9,
                  teamA[holderA].y - 11, BALL_HELD_A);
        state = MS_A_HOLD;
        return;
    }

    if (caught)
    {
        trigger_flash(PAL_TEAM_A);
        sound_mgr_catch();
        player_setPose(&teamA[responderA], POSE_CATCH, 10);
        eliminate_from(teamB, holderB, outStackB, &outCountB);
        return_one(teamA, outStackA, &outCountA);

        if (count_in_play(teamB) == 0) { go_round_end(TRUE); return; }

        holderA = responderA;
        activeA = responderA;
        draw_hud();
        ball_init(&ball, SLOT_BALL, teamA[holderA].x + 9, teamA[holderA].y - 11, BALL_HELD_A);
        state = MS_A_HOLD;
    }
    else
    {
        sound_mgr_hit();
        trigger_flash(PAL_TEAM_A);
        trigger_shake();
        player_setPose(&teamA[responderA], POSE_HIT, 12);
        teamA[responderA].x += (ball.spin < 0) ? -5 : 5;
        impactTimer = 8;
        state = MS_HIT_A;
    }
}

static void finish_hit_to_A(void)
{
    eliminate_from(teamA, responderA, outStackA, &outCountA);
    draw_hud();
    if (count_in_play(teamA) == 0) { go_round_end(FALSE); return; }
    if (responderA == activeA) activeA = first_in_play_from(teamA, activeA);
    holderB = first_in_play_from(teamB, holderB);
    ball_init(&ball, SLOT_BALL, teamB[holderB].x - 3,
              teamB[holderB].y - 8, BALL_HELD_B);
    state = MS_B_HOLD;
    aiDelay = ai_pickThrowDelay();
}

void scene_match_update(void)
{
    bool cpuMoved = FALSE;
    u8 i;

    input_mgr_update();

    if (flashTimer > 0)
    {
        flashTimer--;
        if (flashTimer == 0)
            sprites_data_apply_teams(gTeamAIndex, gTeamBIndex);
    }

    if (shakeTimer > 0)
    {
        VDP_setVerticalScroll(VDP_BG_B, shakePattern[6 - shakeTimer]);
        shakeTimer--;
        if (shakeTimer == 0)
            VDP_setVerticalScroll(VDP_BG_B, 0);
    }

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
                responderB = lane_target(teamB, lane);
                pendingSpin = input_held(BUTTON_LEFT) ? -1 :
                              input_held(BUTTON_RIGHT) ? 1 : 0;
                pendingTargetX = teamB[responderB].x + 4;
                pendingTargetY = teamB[responderB].y - 3;

                sound_mgr_throw();
                player_setPose(&teamA[holderA], POSE_THROW, 18);
                windupTimer = 8;
                state = MS_A_WINDUP;
            }
            break;
        }

        case MS_A_WINDUP:
        {
            ball.x = teamA[holderA].x + 9;
            ball.y = teamA[holderA].y - 11;
            if (windupTimer > 0) windupTimer--;
            else
            {
                ball_startThrow(&ball, pendingTargetX, pendingTargetY,
                                BALL_FLYING_TO_B, pendingSpin);
                state = MS_FLY_TO_B;
            }
            break;
        }

        case MS_B_HOLD:
        {
            if (aiDelay > 0) aiDelay--;
            else
            {
                responderA = nth_in_play(teamA, ai_pickSlot(count_in_play(teamA)));
                /* With C now reserved for right-lane throws, defence
                 * automatically hands control to the targeted player. */
                activeA = responderA;
                pendingTargetX = ai_pickTargetX(teamA[responderA].x);
                pendingTargetY = teamA[responderA].y - 3;
                pendingSpin = (random() % 3) - 1;

                sound_mgr_throw();
                player_setPose(&teamB[holderB], POSE_THROW, 18);
                windupTimer = 8;
                state = MS_B_WINDUP;
            }
            break;
        }

        case MS_B_WINDUP:
        {
            ball.x = teamB[holderB].x - 3;
            ball.y = teamB[holderB].y - 8;
            if (windupTimer > 0) windupTimer--;
            else
            {
                ball_startThrow(&ball, pendingTargetX, pendingTargetY,
                                BALL_FLYING_TO_A, pendingSpin);
                state = MS_FLY_TO_A;
            }
            break;
        }

        case MS_FLY_TO_B:
        {
            /* Only the designated responder chases the ball. */
            if (teamB[responderB].x < ball.targetX) { teamB[responderB].x += PLAYER_SPEED; cpuMoved = TRUE; }
            else if (teamB[responderB].x > ball.targetX) { teamB[responderB].x -= PLAYER_SPEED; cpuMoved = TRUE; }
            if (teamB[responderB].y < ball.targetY) { teamB[responderB].y++; cpuMoved = TRUE; }
            else if (teamB[responderB].y > ball.targetY) { teamB[responderB].y--; cpuMoved = TRUE; }
            player_clampToCourt(&teamB[responderB]);

            {
                bool arrived = ball_update(&ball);
                if (ball_overlaps_player(&teamB[responderB]) || arrived)
                    resolve_throw_to_B();
            }
            break;
        }

        case MS_FLY_TO_A:
        {
            /* If the responder isn't the human's active player, drive
             * them with the same simple chase AI the CPU side uses. */
            if (responderA != activeA)
            {
                if (teamA[responderA].x < ball.targetX) teamA[responderA].x += PLAYER_SPEED;
                else if (teamA[responderA].x > ball.targetX) teamA[responderA].x -= PLAYER_SPEED;
                if (teamA[responderA].y < ball.targetY) teamA[responderA].y++;
                else if (teamA[responderA].y > ball.targetY) teamA[responderA].y--;
                player_clampToCourt(&teamA[responderA]);
            }

            {
                bool arrived = ball_update(&ball);
                if (ball_overlaps_player(&teamA[responderA]) || arrived)
                    resolve_throw_to_A();
            }
            break;
        }

        case MS_HIT_B:
            if (impactTimer > 0) impactTimer--;
            else finish_hit_to_B();
            break;

        case MS_HIT_A:
            if (impactTimer > 0) impactTimer--;
            else finish_hit_to_A();
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

    /* Animate + draw every player on both sides (eliminated ones are
     * parked off-screen by player_eliminate() so this stays simple). */
    for (i = 0; i < TEAM_SIZE; i++)
    {
        bool aMoving = (i == activeA) && !teamA[i].eliminated &&
                       (input_held(BUTTON_LEFT) || input_held(BUTTON_RIGHT) ||
                        input_held(BUTTON_UP) || input_held(BUTTON_DOWN));
        player_tickAnim(&teamA[i], aMoving);
        player_draw(&teamA[i]);

        bool bMoving = cpuMoved && (i == responderB) && (state == MS_FLY_TO_B);
        player_tickAnim(&teamB[i], bMoving);
        player_draw(&teamB[i]);
    }

    ball_draw(&ball);
    draw_control_marker();
    draw_player_shadows();
}
