/*
 * scene_eliminator.c - GLOBAL ELIMINATOR free-for-all.
 *
 * One player from every nation shares a single open court: the centre net is
 * gone, so everyone roams the whole pitch. Three balls are live at once. There
 * are no teams and no rounds - a hit puts you out, and the last player left
 * standing takes it. If the last survivors are hit in the same instant the
 * result is a draw.
 *
 * Palette note: the VDP only has two sprite palette lines free for kits
 * (PAL0 is court/UI, PAL3 is the ball), so ten distinct national kits cannot
 * coexist. The player's own nation wears PAL_TEAM_A and every rival wears
 * PAL_TEAM_B - the read that actually matters in a free-for-all is "me versus
 * everyone else". Who is left is listed by name in the HUD.
 */
#include "genesis.h"
#include "scene_eliminator.h"
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

#define ELIM_PLAYERS   NUM_TEAMS      /* one per nation */
#define ELIM_BALLS     3
#define NO_BALL        0xFF
#define NO_OWNER       0xFF

/* Sprite slots: players 0..9, then each ball takes two (ball + shadow). */
#define SLOT_PLAYERS   0
#define SLOT_BALLS     ELIM_PLAYERS               /* 10, 12, 14 */
#define SLOT_MARKER    (SLOT_BALLS + ELIM_BALLS * 2)   /* 16 */

typedef struct {
    Player p;
    u8   team;        /* nation index */
    bool out;
    u8   ball;        /* index of the ball being carried, or NO_BALL */
    u16  think;       /* AI countdown: throw when carrying, re-target when not */
    s16  wanderX, wanderY;
} Fighter;

static Fighter fighters[ELIM_PLAYERS];
static Ball    balls[ELIM_BALLS];
static u8      ballOwner[ELIM_BALLS];
/* Who threw each ball: a ball in flight must not strike the player who let it
 * go, since it starts life inside their own hitbox. Cleared once it settles. */
static u8      ballThrower[ELIM_BALLS];
static u8      humanIdx;
static u8      aliveCount;
static u16     endTimer;
static u8      result;        /* 0 = playing, 1 = winner, 2 = draw */
static u8      winnerIdx;
static u16     hudTick;

/* ---------------------------------------------------------------- helpers */

static s16 court_min_x(s16 depth) { return COURT_MIN_X_AT_DEPTH(depth) + 10; }
static s16 court_max_x(s16 depth) { return COURT_MAX_X_AT_DEPTH(depth) - 10; }

static void pick_wander(Fighter *f)
{
    s16 depth = COURT_FAR_DEPTH + 6 +
                (s16)(random() % (COURT_NEAR_DEPTH - COURT_FAR_DEPTH - 12));
    s16 lo = court_min_x(depth), hi = court_max_x(depth);
    f->wanderX = lo + (s16)(random() % (hi - lo));
    f->wanderY = COURT_Y_AT_DEPTH_X(depth, f->wanderX);
}

/* Nearest living rival to a fighter, or NO_OWNER when it stands alone. */
static u8 nearest_rival(u8 self)
{
    u8 i, best = NO_OWNER;
    u32 bestD = 0xFFFFFFFF;
    for (i = 0; i < ELIM_PLAYERS; i++)
    {
        s32 dx, dy; u32 d;
        if (i == self || fighters[i].out) continue;
        dx = fighters[i].p.x - fighters[self].p.x;
        dy = fighters[i].p.y - fighters[self].p.y;
        d = (u32)(dx * dx + dy * dy);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

/* Which living player is closest to a given ball - only that one chases it,
 * so the field does not collapse into a single scrum around every loose ball. */
static u8 closest_to_ball(u8 b)
{
    u8 i, best = NO_OWNER;
    u32 bestD = 0xFFFFFFFF;
    for (i = 0; i < ELIM_PLAYERS; i++)
    {
        s32 dx, dy; u32 d;
        if (fighters[i].out || fighters[i].ball != NO_BALL) continue;
        dx = balls[b].x - fighters[i].p.x;
        dy = balls[b].y - fighters[i].p.y;
        d = (u32)(dx * dx + dy * dy);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

/* Nearest ball lying loose and unclaimed, or NO_BALL. */
static u8 nearest_loose_ball(u8 self)
{
    u8 b, best = NO_BALL;
    u32 bestD = 0xFFFFFFFF;
    for (b = 0; b < ELIM_BALLS; b++)
    {
        s32 dx, dy; u32 d;
        if (ballOwner[b] != NO_OWNER) continue;
        dx = balls[b].x - fighters[self].p.x;
        dy = balls[b].y - fighters[self].p.y;
        d = (u32)(dx * dx + dy * dy);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

static void hold_ball_in_hand(Fighter *f)
{
    Ball *b = &balls[f->ball];
    s16 dir = f->p.facingLeft ? -1 : 1;
    b->x = f->p.x + 8 + dir * 9;
    b->y = f->p.y - 2;
    b->preciseX = (s32)b->x << 8;
    b->preciseY = (s32)b->y << 8;
}

static void throw_ball(u8 who, s16 targetX, s16 targetY)
{
    Fighter *f = &fighters[who];
    Ball *b = &balls[f->ball];
    ball_startThrow(b, targetX, targetY, BALL_FLYING_TO_B, 0);
    ballOwner[f->ball] = NO_OWNER;
    ballThrower[f->ball] = who;
    f->ball = NO_BALL;
    player_setPose(&f->p, POSE_THROW, 16);
    sound_mgr_throw();
}

static void eliminate(u8 who)
{
    fighters[who].out = TRUE;
    player_setPose(&fighters[who].p, POSE_FALL, 255);
    if (fighters[who].ball != NO_BALL)
    {
        /* A carried ball is dropped on the spot, never taken out of play. */
        ballOwner[fighters[who].ball] = NO_OWNER;
        ball_dropAt(&balls[fighters[who].ball],
                    fighters[who].p.x + 8, fighters[who].p.y + 10);
        fighters[who].ball = NO_BALL;
    }
    if (aliveCount) aliveCount--;
    sound_mgr_hit();
}

/* ------------------------------------------------------------------- draw */

static void draw_hud(void)
{
    char buf[4];
    ui_set_palette(PAL0);
    ui_apply_palette();
    flag_data_fill_panel(0, 0, 40, 3);
    ui_draw_panel(0, 0, 40, 3, FALSE);
    ui_draw_text("GLOBAL ELIMINATOR", 2, 1, UI_CYAN);
    ui_draw_text(teamNames[gTeamAIndex], 22, 1,
                 fighters[humanIdx].out ? UI_CYAN : UI_GOLD);
    intToStr(aliveCount, buf, 1);
    ui_draw_text("LEFT", 32, 1, UI_WHITE);
    ui_draw_text(buf, 37, 1, UI_GOLD);
}

static void draw_result(void)
{
    flag_data_fill_panel(6, 11, 28, 6);
    ui_draw_panel(6, 11, 28, 6, TRUE);
    if (result == 2)
    {
        ui_draw_big_center("DRAW", 13, UI_GOLD);
    }
    else
    {
        ui_draw_text_center(teamNames[fighters[winnerIdx].team], 13, UI_GOLD);
        ui_draw_text_center("LAST ONE STANDING", 15, UI_WHITE);
    }
}

/* ------------------------------------------------------------------ enter */

void scene_eliminator_enter(void)
{
    u8 i, rival;

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearSprites();
    VDP_setTextPalette(PAL0);
    VDP_clearTileMapRect(BG_A, 0, 0, 40, 28);
    court_bg_draw();
    /* Deliberately NO court_bg_drawForeground(): this mode has no centre net. */

    /* Player's nation in kit A, everyone else in the contrasting kit B. */
    rival = (u8)((gTeamAIndex + 5) % NUM_TEAMS);
    sprites_data_apply_teams(gTeamAIndex, rival);

    humanIdx = 0;
    aliveCount = ELIM_PLAYERS;
    result = 0;
    winnerIdx = 0;
    endTimer = 0;
    hudTick = 0;

    /* Spread the field over the whole court: five columns, two depth bands. */
    for (i = 0; i < ELIM_PLAYERS; i++)
    {
        u8 col = i % 5, row = i / 5;
        s16 depth = COURT_FAR_DEPTH + 18 + (s16)row *
                    ((COURT_NEAR_DEPTH - COURT_FAR_DEPTH - 36));
        s16 lo = court_min_x(depth), hi = court_max_x(depth);
        s16 x = lo + (s16)((hi - lo) * (col + 1) / 6);
        s16 y = COURT_Y_AT_DEPTH_X(depth, x);

        fighters[i].team = (i == humanIdx) ? gTeamAIndex
                         : (u8)((gTeamAIndex + i) % NUM_TEAMS);
        fighters[i].out  = FALSE;
        fighters[i].ball = NO_BALL;
        fighters[i].think = (u16)(30 + (random() % 90));
        player_init(&fighters[i].p, x, y, (u8)(SLOT_PLAYERS + i),
                    (i == humanIdx) ? PAL_TEAM_A : PAL_TEAM_B);
        fighters[i].p.freeRoam = TRUE;      /* no net: the whole court is live */
        fighters[i].p.farSide  = (row == 0);
        fighters[i].p.facingLeft = (col >= 3);
        pick_wander(&fighters[i]);
    }

    /* Three balls spread down the court - clustering them in the middle drew
     * the entire field into one scrum at the whistle. */
    for (i = 0; i < ELIM_BALLS; i++)
    {
        s16 depth = COURT_FAR_DEPTH + 25 + (s16)i *
                    ((COURT_NEAR_DEPTH - COURT_FAR_DEPTH - 50) / 2);
        s16 lo = court_min_x(depth), hi = court_max_x(depth);
        s16 x = lo + (s16)((hi - lo) * (i + 1) / 4);
        s16 y = COURT_Y_AT_DEPTH_X(depth, x);
        ball_init(&balls[i], (u8)(SLOT_BALLS + i * 2), x, y, BALL_HELD_A);
        balls[i].state = BALL_LOOSE;
        balls[i].x = x; balls[i].y = y;
        balls[i].preciseX = (s32)x << 8;
        balls[i].preciseY = (s32)y << 8;
        ball_settle(&balls[i]);
        ballOwner[i] = NO_OWNER;
        ballThrower[i] = NO_OWNER;
    }

    draw_hud();
    sound_mgr_whistle();
}

/* ----------------------------------------------------------------- update */

/* Step toward (or, with away=TRUE, directly away from) a point. */
static void step_towards(Fighter *f, s16 tx, s16 ty, bool away)
{
    s16 sx = away ? -PLAYER_SPEED : PLAYER_SPEED;
    if (f->p.x < tx - 2) { f->p.x += sx; f->p.facingLeft = away; }
    else if (f->p.x > tx + 2) { f->p.x -= sx; f->p.facingLeft = !away; }
    if (f->p.y < ty - 2) f->p.y += away ? -1 : 1;
    else if (f->p.y > ty + 2) f->p.y += away ? 1 : -1;
    player_clampToCourt(&f->p);
}

static void update_ai(u8 i)
{
    Fighter *f = &fighters[i];
    u8 target;

    if (f->ball != NO_BALL)
    {
        hold_ball_in_hand(f);
        target = nearest_rival(i);
        if (f->think > 0)
        {
            /* Back off while winding up, so throws are not point blank. */
            f->think--;
            if (target != NO_OWNER)
                step_towards(f, fighters[target].p.x, fighters[target].p.y, TRUE);
            hold_ball_in_hand(f);
            return;
        }
        if (target != NO_OWNER)
            throw_ball(i, ai_pickTargetX(fighters[target].p.x + 8),
                       fighters[target].p.y + 6);
        f->think = ai_pickThrowDelay();
        return;
    }

    /* Only the player closest to a given ball goes for it. Everyone else keeps
     * their distance from whoever is armed, which stops the whole field from
     * converging on the middle and wiping itself out in one scrum. */
    {
        u8 b = nearest_loose_ball(i);
        if (b != NO_BALL && closest_to_ball(b) == i)
        {
            step_towards(f, balls[b].x - 8, balls[b].y - 10, FALSE);
            return;
        }

        /* Evade the nearest carrier if one is armed and close. */
        {
            u8 j, threat = NO_OWNER;
            u32 bestD = 0xFFFFFFFF;
            for (j = 0; j < ELIM_PLAYERS; j++)
            {
                s32 dx, dy; u32 d;
                if (j == i || fighters[j].out || fighters[j].ball == NO_BALL) continue;
                dx = fighters[j].p.x - f->p.x;
                dy = fighters[j].p.y - f->p.y;
                d = (u32)(dx * dx + dy * dy);
                if (d < bestD) { bestD = d; threat = j; }
            }
            if (threat != NO_OWNER && bestD < (90u * 90u))
            {
                step_towards(f, fighters[threat].p.x, fighters[threat].p.y, TRUE);
                return;
            }
        }

        if (f->think > 0) f->think--;
        else { pick_wander(f); f->think = (u16)(60 + (random() % 120)); }
        step_towards(f, f->wanderX, f->wanderY, FALSE);
    }
}

void scene_eliminator_update(void)
{
    u8 i, b;
    u8 struckThisFrame[ELIM_PLAYERS];
    u8 struckCount = 0;

    input_mgr_update();

    if (result != 0)
    {
        /* Result is up: hold it, then back to the menu. */
        if (endTimer > 0) endTimer--;
        else if (input_pressed(BUTTON_START) || input_pressed(BUTTON_A) ||
                 input_pressed(BUTTON_C))
        {
            PAL_fadeOutAll(20, FALSE);
            gCurrentScene = GS_MENU;
            return;
        }
    }
    else
    {
        /* --- human --- */
        Fighter *me = &fighters[humanIdx];
        if (!me->out)
        {
            player_moveHuman(&me->p, me->ball != NO_BALL);
            if (me->ball != NO_BALL)
            {
                hold_ball_in_hand(me);
                if (input_pressed(BUTTON_A) || input_pressed(BUTTON_B) ||
                    input_pressed(BUTTON_C))
                {
                    u8 t = nearest_rival(humanIdx);
                    if (t != NO_OWNER)
                        throw_ball(humanIdx, fighters[t].p.x + 8,
                                   fighters[t].p.y + 6);
                }
            }
        }

        /* --- CPU --- */
        for (i = 0; i < ELIM_PLAYERS; i++)
            if (i != humanIdx && !fighters[i].out) update_ai(i);

        /* --- balls --- */
        for (b = 0; b < ELIM_BALLS; b++)
        {
            Ball *ba = &balls[b];
            if (ballOwner[b] != NO_OWNER) continue;

            if (ba->state == BALL_FLYING_TO_A || ba->state == BALL_FLYING_TO_B)
            {
                bool arrived = ball_update(ba);
                /* A ball in flight strikes the first player it reaches. Every
                 * strike this frame is collected before any of them resolves,
                 * so simultaneous hits can be judged together.
                 * It only ARMS after leaving the thrower's vicinity, so a ball
                 * released in a crowd cannot instantly strike whoever happens
                 * to be standing beside the thrower. */
                for (i = 0; ba->progress > 40 && i < ELIM_PLAYERS; i++)
                {
                    Player *p = &fighters[i].p;
                    if (fighters[i].out || i == ballThrower[b]) continue;
                    if (abs((p->x + 8) - ba->x) <= HIT_WINDOW_X &&
                        abs((p->y - 2) - ball_visualY(ba)) <= HIT_WINDOW_Y)
                    {
                        struckThisFrame[struckCount++] = i;
                        ball_dropAt(ba, ba->x, ba->y);
                        break;
                    }
                }
                if (arrived && ba->state != BALL_LOOSE) ball_startRicochet(ba);
            }
            else
            {
                if (ball_updateLoose(ba)) sound_mgr_bounce();
                /* First player to reach a settled ball picks it up - but the
                 * thrower gets a moment's grace so they cannot instantly
                 * re-collect their own throw. */
                for (i = 0; i < ELIM_PLAYERS; i++)
                {
                    Fighter *f = &fighters[i];
                    if (f->out || f->ball != NO_BALL) continue;
                    if (i == ballThrower[b]) continue;
                    if (abs((f->p.x + 8) - ba->x) <= PICKUP_WINDOW_X &&
                        abs((f->p.y + 10) - ba->y) <= PICKUP_WINDOW_Y)
                    {
                        f->ball = b;
                        ballOwner[b] = i;
                        ballThrower[b] = NO_OWNER;
                        ball_settle(ba);
                        f->think = ai_pickThrowDelay();
                        player_setPose(&f->p, POSE_PICKUP, 10);
                        sound_mgr_pickup();
                        break;
                    }
                }
            }
        }

        /* Resolve every strike from this frame together, so two players who
         * knock each other out at the same instant both go. */
        for (i = 0; i < struckCount; i++) eliminate(struckThisFrame[i]);

        if (aliveCount <= 1)
        {
            u8 last = NO_OWNER;
            for (i = 0; i < ELIM_PLAYERS; i++)
                if (!fighters[i].out) { last = i; break; }
            if (last == NO_OWNER) { result = 2; }        /* mutual knockout */
            else { result = 1; winnerIdx = last; }
            endTimer = 90;
            draw_result();
            sound_mgr_crowdGameOver();
        }
    }

    /* ------------------------------------------------------------- render */
    if (result != 0)
    {
        /* Clear the field so nothing is drawn over the result banner. */
        VDP_clearSprites();
        sprites_data_hide_all_sprites();
        return;
    }

    for (i = 0; i < ELIM_PLAYERS; i++)
    {
        Fighter *f = &fighters[i];
        bool moving = (!f->out) && (i != humanIdx);
        /* Face the camera when in the near half, away when up the far end. */
        if (!f->out)
            f->p.farSide = (COURT_DEPTH_OF(f->p.x + 8, f->p.y + 16)
                            < COURT_CENTER_DEPTH);
        player_tickAnim(&f->p, moving);
        player_draw(&f->p);
    }
    for (b = 0; b < ELIM_BALLS; b++) ball_draw(&balls[b]);

    /* Ring under the human so they can always find themselves in the crowd. */
    if (!fighters[humanIdx].out)
        VDP_setSpriteFull(SLOT_MARKER, fighters[humanIdx].p.x + 4,
                          fighters[humanIdx].p.y + 12, SPRITE_SIZE(3, 2),
                          TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE,
                                         TILE_RING_YELLOW), 0);
    else
        VDP_setSpriteFull(SLOT_MARKER, -32, -32, SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE,
                                         TILE_BALL_SHADOW), 0);

    if (++hudTick >= 30) { hudTick = 0; if (result == 0) draw_hud(); }
}
