/*
 * scene_eliminator.c - GLOBAL ELIMINATOR free-for-all.
 *
 * One player from every nation shares a single open court: the centre net is
 * gone, so everyone roams the whole pitch. Two balls are live at once. There
 * are no teams and no rounds - a hit puts you out, and the last player left
 * standing takes it. If the last survivors are hit in the same instant the
 * result is a draw.
 *
 * Three player palette lines share compact national kit ramps. Recoloured
 * per-player pose slots select the appropriate kit colours, so all ten
 * countries retain their identity and full action silhouettes.
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
#include "impact_fx.h"
#include "screen_transition.h"

#define ELIM_PLAYERS   NUM_TEAMS      /* one per nation */
#define ELIM_BALLS     2
#define NO_BALL        0xFF
#define NO_OWNER       0xFF

/* Ten 16-tile fighter caches plus 30 ball/ring tiles may reuse scene-local
 * title art, but must remain below the persistent UI font. */
#if (TILE_ELIM_EFFECT_BASE + 30) > TILE_UI_BASE
#error "Eliminator sprite cache overlaps UI VRAM"
#endif

/* Ten players, two balls and the impact use depth-sorted slots 0..12. Ball
 * shadows and the human marker are deliberate ground underlays at 13..15. */
#define SLOT_PLAYERS   0
#define SLOT_BALLS     ELIM_PLAYERS                /* initial only; reassigned */
#define SLOT_BALL_SHADOWS (ELIM_PLAYERS + ELIM_BALLS + 1) /* 13, 14 */
#define SLOT_MARKER    (SLOT_BALL_SHADOWS + ELIM_BALLS)   /* 15 */

typedef struct {
    Player p;
    u8   team;        /* nation index */
    bool out;
    u8   ball;        /* index of the ball being carried, or NO_BALL */
    u8   fallTimer;   /* frames left showing the hit before walking off */
    bool gone;        /* has finished leaving the pitch */
    u16  think;       /* AI countdown: throw when carrying, re-target when not */
    s16  wanderX, wanderY;
    bool moving;
    bool repositionWhileHolding;
    u16  loadedTileBase;
} Fighter;

static Fighter fighters[ELIM_PLAYERS];
static Ball    balls[ELIM_BALLS];
static u8      ballOwner[ELIM_BALLS];
/* Who threw each ball: a ball in flight must not strike the player who let it
 * go, since it starts life inside their own hitbox, and they cannot instantly
 * scoop their own throw back up. The lock is a COOLDOWN, not permanent - left
 * permanent it deadlocked the match, with every survivor stranded on top of a
 * ball they were never allowed to collect. */
static u8      ballThrower[ELIM_BALLS];
static u16     ballLock[ELIM_BALLS];
#define BALL_LOCK_FRAMES 45
#define HIT_FRAMES       12
#define FALL_FRAMES      36
/* Ground-space body buffer. Sprites may overlap naturally in depth, but their
 * feet/contact points cannot occupy the same horizontal or vertical pocket. */
#define PLAYER_BUFFER_X  20
#define PLAYER_BUFFER_Y  12
#define PLAYER_BOUNCE     2
static u8      humanIdx;
/* Fighter slots the humans occupy: player 1 is always slot 0, player 2 slot 1
 * in a two-player game. */
#define HUMAN2_IDX 1
static u8 human_index(u8 pad) { return pad ? HUMAN2_IDX : humanIdx; }
static bool is_human(u8 idx)
{
    return (idx == humanIdx) || (TWO_PLAYERS() && idx == HUMAN2_IDX);
}
static u8      aliveCount;
static u16     endTimer;
static u8      result;        /* 0 playing, 1 winner, 2 draw, 3 clearing pitch */
static u8      pendingResult; /* winner/draw to reveal once every loser is gone */
static u8      winnerIdx;
static u16     hudTick;
static u8      markerPulseTick;

/* ---------------------------------------------------------------- helpers */

static s16 court_min_x(s16 depth) { return COURT_MIN_X_AT_DEPTH(depth) + 10; }
static s16 court_max_x(s16 depth) { return COURT_MAX_X_AT_DEPTH(depth) - 10; }

/* Eliminator has no fixed team halves, so front/back art represents the last
 * vertical movement direction: back view looks up-court, front view looks
 * down-court.  Keep the remembered control direction and visible sprite bank
 * locked together. */
static void face_vertical(Fighter *f, bool up)
{
    f->p.farSide = up ? FALSE : TRUE;
}

static void pick_wander(Fighter *f)
{
    s16 currentDepth = COURT_DEPTH_OF(f->p.x + 8, f->p.y + 16);
    s16 depth = currentDepth + (s16)(random() % 29) - 14;
    s16 lo, hi;
    s16 targetX = f->p.x + 8 + (s16)(random() % 41) - 20;

    if (depth < COURT_FAR_DEPTH + 8) depth = COURT_FAR_DEPTH + 8;
    if (depth > COURT_NEAR_DEPTH - 8) depth = COURT_NEAR_DEPTH - 8;
    lo = court_min_x(depth);
    hi = court_max_x(depth);
    if (targetX < lo) targetX = lo;
    if (targetX > hi) targetX = hi;
    f->wanderX = targetX - 8;
    f->wanderY = COURT_Y_AT_DEPTH_X(depth, targetX) - 16;
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
        /* The thrower is temporarily ineligible, so it must not reserve the
         * ball as "closest" and prevent every eligible CPU from pursuing it. */
        if (ballLock[b] > 0 && ballThrower[b] == i) continue;
        dx = balls[b].x - fighters[i].p.x;
        dy = balls[b].y - fighters[i].p.y;
        d = (u32)(dx * dx + dy * dy);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

/* Nearest loose ball assigned to this fighter, or NO_BALL.  Assignment is
 * tested per ball before comparing distances: choosing a personal nearest
 * first could leave both balls unchased when that fighter was not the globally
 * closest player to its first choice. */
static u8 nearest_loose_ball(u8 self)
{
    u8 b, best = NO_BALL;
    u32 bestD = 0xFFFFFFFF;
    for (b = 0; b < ELIM_BALLS; b++)
    {
        s32 dx, dy; u32 d;
        if (ballOwner[b] != NO_OWNER) continue;
        if (balls[b].state != BALL_LOOSE) continue;
        if (ballThrower[b] == self) continue;   /* cannot collect own throw yet */
        if (closest_to_ball(b) != self) continue;
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
    /* Keep rendering state in step with the visible front/back pose. Besides
     * hiding the ground shadow, this gives the held ball the correct court
     * priority whenever the carrier turns. */
    b->state = f->p.farSide ? BALL_HELD_B : BALL_HELD_A;
}

/* The same three back-court lanes used by a normal match. */
static void fixed_throw_target(bool up, u8 lane, s16 *x, s16 *y)
{
    s16 depth = up ? (COURT_FAR_DEPTH + BALL_WALL_MARGIN)
                   : (COURT_NEAR_DEPTH - BALL_WALL_MARGIN);
    s16 minX = COURT_MIN_X_AT_DEPTH(depth) + 8;
    s16 maxX = COURT_MAX_X_AT_DEPTH(depth) - 8;
    *x = minX + (s16)((maxX - minX) * lane / 2);
    *y = COURT_Y_AT_DEPTH_X(depth, *x);
}

static void throw_ball(u8 who, s16 targetX, s16 targetY, bool up)
{
    Fighter *f = &fighters[who];
    Ball *b = &balls[f->ball];
    ball_startThrow(b, targetX, targetY,
                    up ? BALL_FLYING_TO_B : BALL_FLYING_TO_A, 0);
    ballOwner[f->ball] = NO_OWNER;
    ballThrower[f->ball] = who;
    ballLock[f->ball] = BALL_LOCK_FRAMES;
    f->ball = NO_BALL;
    player_setPose(&f->p, POSE_THROW, 16);
    sound_mgr_throw();
}

static void eliminate(u8 who)
{
    Fighter *f = &fighters[who];
    if (f->out) return; /* two balls may register the same fighter this frame */
    f->out = TRUE;
    f->gone = FALSE;
    if (f->ball != NO_BALL)
    {
        /* Finish ball ownership first, then force the impact pose. This keeps
         * dropping a carried ball from bypassing the carrier's hit/fall. */
        ballOwner[f->ball] = NO_OWNER;
        ballThrower[f->ball] = NO_OWNER;
        ballLock[f->ball] = 0;
        ball_dropAt(&balls[f->ball], f->p.x + 8, f->p.y + 10);
        f->ball = NO_BALL;
    }
    /* A readable impact beat, then the grounded fall, then run off court. */
    f->fallTimer = HIT_FRAMES + FALL_FRAMES;
    player_setPose(&f->p, POSE_HIT, 255);
    if (aliveCount) aliveCount--;
    sound_mgr_hit();
    sound_mgr_crowdKnockout();
}

/* ------------------------------------------------------------------- draw */

static void draw_hud(void)
{
    char alive[4];
    ui_set_palette(PAL0);
    ui_apply_palette();
    ui_draw_panel(0, 0, 40, 3, FALSE);
    intToStr(aliveCount, alive, 1);
    ui_draw_text("ALIVE", 2, 1, UI_CYAN);
    ui_draw_text(alive, 8, 1, UI_GOLD);
    ui_draw_text_center("ELIMINATOR", 1, UI_WHITE);
    ui_draw_text(teamCodes[fighters[humanIdx].team], 35, 1, UI_GOLD);
}

static void draw_result(void)
{
    ui_draw_panel(5, 10, 30, 6, TRUE);
    if (result == 2)
    {
        ui_draw_text_center("ELIMINATOR RESULT", 11, UI_CYAN);
        ui_draw_big_center("DRAW", 13, UI_GOLD);
    }
    else
    {
        ui_draw_text_center("ELIMINATOR CHAMPION", 11, UI_CYAN);
        ui_draw_big_center(teamNames[fighters[winnerIdx].team], 13, UI_GOLD);
    }
}

/* ------------------------------------------------------------------ enter */

void scene_eliminator_enter(void)
{
    u8 i;

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearSprites();
    VDP_setTextPalette(PAL0);
    ui_set_sprite_panel_mask(FALSE, 0, 0, 0, 0);
    VDP_clearTileMapRect(BG_A, 0, 0, 40, 28);
    court_bg_drawEliminator();
    /* Roof-only foreground: open championship floor, no centre net, while
     * players/balls still pass beneath the near and right stadium canopy. */
    court_bg_drawEliminatorForeground();

    sprites_data_load_eliminator_ball_art();
    sprites_data_load_impact_art();
    sprites_data_apply_eliminator_teams();
    impact_fx_init();

    humanIdx = 0;
    aliveCount = ELIM_PLAYERS;
    result = 0;
    pendingResult = 0;
    winnerIdx = 0;
    endTimer = 0;
    hudTick = 0;
    markerPulseTick = 0;

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
                         : (TWO_PLAYERS() && i == HUMAN2_IDX && gPlayer2Team != NO_TEAM)
                           ? gPlayer2Team
                         : (u8)((gTeamAIndex + i) % NUM_TEAMS);
        fighters[i].out  = FALSE;
        fighters[i].ball = NO_BALL;
        fighters[i].fallTimer = 0;
        fighters[i].gone = FALSE;
        fighters[i].think = (u16)(30 + (random() % 90));
        /* Start both rows looking toward the centre; subsequent vertical
         * movement becomes the remembered facing/throw direction. */
        fighters[i].moving = FALSE;
        fighters[i].repositionWhileHolding = FALSE;
        fighters[i].loadedTileBase = 0xFFFF;
        /* Spread ten teams over all three sprite palette lines so every kit
         * retains two shades instead of flattening the final countries. */
        player_init(&fighters[i].p, x, y, (u8)(SLOT_PLAYERS + i),
                    fighters[i].team < 4 ? PAL_TEAM_A
                  : fighters[i].team < 7 ? PAL_TEAM_B : PAL_BALL);
        fighters[i].p.animCounter = (u8)(i & 3); /* stagger pose-slot uploads */
        /* One contiguous cache bank remains wholly below the shared UI font;
         * no fighter pose upload can overwrite HUD glyphs. */
        fighters[i].p.tileBaseOverride =
            (u16)(TILE_ELIM_PLAYER_LOW_BASE + i * 16);
        fighters[i].p.freeRoam = TRUE;      /* no net: the whole court is live */
        face_vertical(&fighters[i], row != 0);
        fighters[i].p.facingLeft = (col >= 3);
        pick_wander(&fighters[i]);
    }

    /* Two balls start on opposite sides of the championship crest. This keeps
     * the opening readable and prevents all ten fighters forming one scrum. */
    for (i = 0; i < ELIM_BALLS; i++)
    {
        s16 depth = 62 + (s16)i * 44;
        s16 lo = court_min_x(depth), hi = court_max_x(depth);
        s16 x = lo + (s16)((hi - lo) * (i + 1) / 3);
        s16 y = COURT_Y_AT_DEPTH_X(depth, x);
        ball_init(&balls[i], (u8)(SLOT_BALLS + i), x, y, BALL_HELD_A);
        balls[i].shadowSlot = (u8)(SLOT_BALL_SHADOWS + i);
        balls[i].eliminatorArt = TRUE;
        balls[i].state = BALL_LOOSE;
        balls[i].x = x; balls[i].y = y;
        balls[i].preciseX = (s32)x << 8;
        balls[i].preciseY = (s32)y << 8;
        ball_settle(&balls[i]);
        ballOwner[i] = NO_OWNER;
        ballThrower[i] = NO_OWNER;
        ballLock[i] = 0;
    }

    draw_hud();
    screen_transition_fade_in();
    sound_mgr_whistle();
}

/* ----------------------------------------------------------------- update */

/* Step toward (or, with away=TRUE, directly away from) a point. */
static void step_towards(Fighter *f, s16 tx, s16 ty, bool away)
{
    s16 oldX = f->p.x;
    s16 oldY = f->p.y;
    s16 oldDepth = COURT_DEPTH_OF(f->p.x + 8, f->p.y + 16);
    s16 newDepth;
    s16 sx = away ? -PLAYER_SPEED : PLAYER_SPEED;
    if (f->p.x < tx - 2) { f->p.x += sx; f->p.facingLeft = away; }
    else if (f->p.x > tx + 2) { f->p.x -= sx; f->p.facingLeft = !away; }
    if (f->p.y < ty - 2) f->p.y += away ? -1 : 1;
    else if (f->p.y > ty + 2) f->p.y += away ? 1 : -1;
    player_clampToCourt(&f->p);
    newDepth = COURT_DEPTH_OF(f->p.x + 8, f->p.y + 16);
    if (newDepth < oldDepth) face_vertical(f, TRUE);
    else if (newDepth > oldDepth) face_vertical(f, FALSE);
    f->moving = (f->p.x != oldX || f->p.y != oldY);
}

/* Resolve fighter-on-fighter overlap along one of the four cardinal axes.
 * Three passes handle small crowds without an expensive physics system. */
static void separate_fighters(void)
{
    u8 pass, i, j;
    for (pass = 0; pass < 3; pass++)
    {
        for (i = 0; i < ELIM_PLAYERS; i++)
        {
            Fighter *a = &fighters[i];
            if (a->out) continue;
            for (j = (u8)(i + 1); j < ELIM_PLAYERS; j++)
            {
                Fighter *b = &fighters[j];
                s16 dx, dy, ax, ay, push;
                if (b->out) continue;

                dx = (b->p.x + 8) - (a->p.x + 8);
                dy = (b->p.y + 16) - (a->p.y + 16);
                ax = abs(dx);
                ay = abs(dy);
                if (ax >= PLAYER_BUFFER_X || ay >= PLAYER_BUFFER_Y) continue;

                /* Use the shallower penetration axis, producing an explicit
                 * left/right or up/down rebound rather than diagonal drift. */
                if ((PLAYER_BUFFER_X - ax) * PLAYER_BUFFER_Y <
                    (PLAYER_BUFFER_Y - ay) * PLAYER_BUFFER_X)
                {
                    push = (s16)((PLAYER_BUFFER_X - ax + 1) / 2 + PLAYER_BOUNCE);
                    if (dx < 0 || (dx == 0 && (i & 1)))
                    {
                        a->p.x += push;
                        b->p.x -= push;
                    }
                    else
                    {
                        a->p.x -= push;
                        b->p.x += push;
                    }
                }
                else
                {
                    push = (s16)((PLAYER_BUFFER_Y - ay + 1) / 2 + PLAYER_BOUNCE);
                    if (dy < 0 || (dy == 0 && (i & 1)))
                    {
                        a->p.y += push;
                        b->p.y -= push;
                    }
                    else
                    {
                        a->p.y -= push;
                        b->p.y += push;
                    }
                }
                player_clampToCourt(&a->p);
                player_clampToCourt(&b->p);
                a->moving = b->moving = TRUE;
            }
        }
    }
}

/* Hardware sprite order is depth order. Keep nearer feet in earlier slots so
 * bodies cross cleanly instead of one nation permanently drawing over all
 * others just because of its array index. */
typedef struct {
    Fighter *fighter;
    Ball *ball;
    s16 x, y;
    bool impact;
} ElimDepthActor;

static void assign_fighter_depth_slots(void)
{
    ElimDepthActor order[ELIM_PLAYERS + ELIM_BALLS + 1];
    const u8 impactIndex = ELIM_PLAYERS + ELIM_BALLS;
    u8 i, b;
    for (i = 0; i < ELIM_PLAYERS; i++)
    {
        order[i].fighter = &fighters[i];
        order[i].ball = NULL;
        if (fighters[i].gone)
        {
            order[i].x = -32767;
            order[i].y = -32767;
        }
        else player_visualGround(&fighters[i].p, &order[i].x, &order[i].y);
        order[i].impact = FALSE;
    }

    for (b = 0; b < ELIM_BALLS; b++)
    {
        const u8 actor = ELIM_PLAYERS + b;
        order[actor].fighter = NULL;
        order[actor].ball = &balls[b];
        order[actor].impact = FALSE;
        if (ballOwner[b] != NO_OWNER)
        {
            Fighter *owner = &fighters[ballOwner[b]];
            order[actor].x = owner->p.x;
            /* Back-view hands sit behind the torso; front-view hands present
             * the ball in front. The one-pixel bias resolves exact ties. */
            order[actor].y = owner->p.y + (owner->p.farSide ? 1 : -1);
        }
        else
        {
            /* Airborne height never changes ground depth: the ball crosses a
             * body only when its projected ground track crosses that body. */
            order[actor].x = balls[b].x;
            order[actor].y = balls[b].y;
        }
    }

    order[impactIndex].fighter = NULL;
    order[impactIndex].ball = NULL;
    order[impactIndex].x = impact_fx_active() ? impact_fx_sortX() : -32767;
    order[impactIndex].y = impact_fx_active() ? impact_fx_sortY() : -32767;
    order[impactIndex].impact = TRUE;

    for (i = 1; i < ELIM_PLAYERS + ELIM_BALLS + 1; i++)
    {
        ElimDepthActor key = order[i];
        u8 j = i;
        while (j > 0 &&
              (key.y > order[j - 1].y ||
              (key.y == order[j - 1].y && key.x > order[j - 1].x)))
        {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = key;
    }

    for (i = 0; i < ELIM_PLAYERS + ELIM_BALLS + 1; i++)
    {
        if (order[i].impact) impact_fx_setSpriteSlot(i);
        else if (order[i].ball != NULL)
            order[i].ball->spriteSlot = i;
        else order[i].fighter->p.spriteSlot = i;
    }
    for (b = 0; b < ELIM_BALLS; b++)
        balls[b].shadowSlot = (u8)(SLOT_BALL_SHADOWS + b);
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
            s32 dx, dy;
            f->think--;
            if (target != NO_OWNER)
            {
                dx = fighters[target].p.x - f->p.x;
                dy = fighters[target].p.y - f->p.y;
                /* Usually close into useful range while armed. A minority of
                 * possessions stay planted and throw immediately for variety. */
                if (f->repositionWhileHolding &&
                    (u32)(dx * dx + dy * dy) > (42u * 42u))
                    step_towards(f, fighters[target].p.x,
                                 fighters[target].p.y, FALSE);
                else f->moving = FALSE;
            }
            hold_ball_in_hand(f);
            return;
        }
        if (target != NO_OWNER)
        {
            s16 tx, ty;
            /* The authored back sprite looks up; the front sprite looks down.
             * Read that visible orientation directly so CPU aim can never
             * contradict the throw animation on screen. */
            bool up = !f->p.farSide;
            fixed_throw_target(up, ai_pickSlot(3), &tx, &ty);
            throw_ball(i, tx, ty, up);
        }
        f->think = ai_pickThrowDelay();
        return;
    }

    /* Only the player closest to a given ball goes for it. Everyone else keeps
     * their distance from whoever is armed, which stops the whole field from
     * converging on the middle and wiping itself out in one scrum. */
    {
        u8 b = nearest_loose_ball(i);
        if (b != NO_BALL)
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
            if (threat != NO_OWNER && bestD < (68u * 68u))
            {
                Fighter *armed = &fighters[threat];
                s16 selfDepth = COURT_DEPTH_OF(f->p.x + 8, f->p.y + 16);
                s16 armedDepth = COURT_DEPTH_OF(armed->p.x + 8,
                                                armed->p.y + 16);
                s16 laneGap = abs((f->p.x + 8) - (armed->p.x + 8));
                bool inFront = armed->p.farSide ? (selfDepth > armedDepth)
                                                : (selfDepth < armedDepth);
                /* Only dodge a carrier who is actually facing this player and
                 * shares their lane. Other fighters hold their spacing. */
                if (inFront && laneGap < 48)
                {
                    step_towards(f, armed->p.x, armed->p.y, TRUE);
                    return;
                }
            }
        }

        if (f->think > 0) f->think--;
        else { pick_wander(f); f->think = (u16)(60 + (random() % 120)); }
        step_towards(f, f->wanderX, f->wanderY, FALSE);
    }
}

static void trigger_eliminator_surface_impact(Ball *b)
{
    impact_fx_trigger(b->x, ball_visualY(b), b->x, b->y,
                      COURT_DEPTH_OF(b->x, b->y) >= COURT_CENTER_DEPTH,
                      FALSE);
}

static void update_eliminator_loose_ball(Ball *b)
{
    if (ball_updateLoose(b))
    {
        sound_mgr_bounce();
        if (b->contactKind >= BALL_CONTACT_LEFT_WALL)
            trigger_eliminator_surface_impact(b);
    }
}

void scene_eliminator_update(void)
{
    u8 i, b;
    u8 struckThisFrame[ELIM_PLAYERS];
    u8 struckCount = 0;

    input_mgr_update();
    impact_fx_update();

    if (result == 1 || result == 2)
    {
        /* The result remains visible for one minute. Any newly pressed
         * controller button dismisses it sooner; directions alone do not. */
        if (endTimer > 0) endTimer--;
        if (endTimer == 0 || input_pressed_any(BUTTON_BTN))
        {
            screen_transition_fade_out();
            gMenuEntry = MENU_ENTRY_TITLE;
            gCurrentScene = GS_MENU;
            return;
        }
    }
    else if (result == 0)
    {
        for (i = 0; i < ELIM_PLAYERS; i++) fighters[i].moving = FALSE;

        /* --- humans --- one pad each; player 2 only exists in a 2 player game */
        {
            u8 h;
            for (h = 0; h < (TWO_PLAYERS() ? 2 : 1); h++)
            {
                u8 idx = human_index(h);
                Fighter *me = &fighters[idx];
                if (me->out) continue;

                player_moveHumanPad(&me->p, me->ball != NO_BALL, h);
                /* Animate from intent, as normal match play does. A carrier
                 * only advances every other frame and court edges can clamp
                 * movement; neither should make held input look frozen. */
                me->moving = input_held_p(h, BUTTON_LEFT) ||
                             input_held_p(h, BUTTON_RIGHT) ||
                             input_held_p(h, BUTTON_UP) ||
                             input_held_p(h, BUTTON_DOWN);
                if (input_held_p(h, BUTTON_UP)) face_vertical(me, TRUE);
                else if (input_held_p(h, BUTTON_DOWN)) face_vertical(me, FALSE);
                if (me->ball != NO_BALL)
                {
                    hold_ball_in_hand(me);
                    if (input_pressed_p(h, BUTTON_A) ||
                        input_pressed_p(h, BUTTON_B) ||
                        input_pressed_p(h, BUTTON_C))
                    {
                        u8 lane = input_pressed_p(h, BUTTON_A) ? 0 :
                                  input_pressed_p(h, BUTTON_B) ? 1 : 2;
                        s16 tx, ty;
                        fixed_throw_target(!me->p.farSide, lane, &tx, &ty);
                        throw_ball(idx, tx, ty, !me->p.farSide);
                    }
                }
            }
        }

        /* --- CPU --- everyone who is not a human */
        for (i = 0; i < ELIM_PLAYERS; i++)
            if (!is_human(i) && !fighters[i].out) update_ai(i);

        separate_fighters();
        /* Separation happens after movement, so refresh every carried ball's
         * hand anchor before collision/pickup processing and rendering. */
        for (i = 0; i < ELIM_PLAYERS; i++)
            if (!fighters[i].out && fighters[i].ball != NO_BALL)
                hold_ball_in_hand(&fighters[i]);

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
                for (i = 0; ba->state != BALL_LOOSE &&
                            ba->progress > 40 && i < ELIM_PLAYERS; i++)
                {
                    Player *p = &fighters[i].p;
                    if (fighters[i].out || i == ballThrower[b]) continue;
                    if (abs((p->x + 8) - ba->x) <= HIT_WINDOW_X &&
                        abs((p->y - 2) - ball_visualY(ba)) <= HIT_WINDOW_Y)
                    {
                        s16 travelY = ba->y - ba->startY;
                        struckThisFrame[struckCount++] = i;
                        /* Anchor player-hit feedback to the torso, not the
                         * airborne ball position used by collision testing. */
                        impact_fx_trigger(p->x + 8, p->y - 4, p->x,
                                          p->y + ((travelY >= 0) ? 1 : -1),
                                          !p->farSide, TRUE);
                        ball_startHitBounce(ba,
                                            p->x + PLAYER_PICKUP_DX,
                                            p->y + PLAYER_PICKUP_DY);
                        break;
                    }
                }
                if (arrived && ba->state != BALL_LOOSE) ball_startRicochet(ba);
                if (arrived && ba->contactKind >= BALL_CONTACT_LEFT_WALL)
                {
                    sound_mgr_bounce();
                    trigger_eliminator_surface_impact(ba);
                }
            }
            else
            {
                update_eliminator_loose_ball(ba);
                /* Release the thrower's claim once the ball has settled for a
                 * moment, so no ball can become permanently uncollectable. */
                if (ballLock[b] > 0 && --ballLock[b] == 0)
                    ballThrower[b] = NO_OWNER;
                /* First player to reach a settled ball picks it up - but the
                 * thrower gets a moment's grace so they cannot instantly
                 * re-collect their own throw. */
                for (i = 0; i < ELIM_PLAYERS; i++)
                {
                    Fighter *f = &fighters[i];
                    if (f->out || f->ball != NO_BALL) continue;
                    if (i == ballThrower[b]) continue;
                    if (abs((f->p.x + PLAYER_PICKUP_DX) - ba->x) <= PICKUP_WINDOW_X &&
                        abs((f->p.y + PLAYER_PICKUP_DY) - ba->y) <= PICKUP_WINDOW_Y)
                    {
                        f->ball = b;
                        ballOwner[b] = i;
                        ballThrower[b] = NO_OWNER;
                        ballLock[b] = 0;
                        ball_settle(ba);
                        f->think = ai_pickThrowDelay();
                        f->repositionWhileHolding =
                            (i != humanIdx) && ((random() & 3) != 0);
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
            pendingResult = last == NO_OWNER ? 2 : 1;
            result = 3; /* freeze gameplay, but keep rendering the finish */
            if (last != NO_OWNER)
            {
                Fighter *winner = &fighters[last];
                winnerIdx = last;
                /* Free both hands for the celebration if the winner happened
                 * to be carrying a ball when the final opponent was hit. */
                if (winner->ball != NO_BALL)
                {
                    ballOwner[winner->ball] = NO_OWNER;
                    ballThrower[winner->ball] = NO_OWNER;
                    ballLock[winner->ball] = 0;
                    ball_dropAt(&balls[winner->ball],
                                winner->p.x + 8, winner->p.y + 10);
                    winner->ball = NO_BALL;
                }
                player_setPose(&winner->p, POSE_CELEBRATE, 255);
            }
        }
    }

    /* ------------------------------------------------------------- render */
    if (result == 1 || result == 2)
    {
        /* Clear the field so nothing is drawn over the result banner. */
        VDP_clearSprites();
        sprites_data_hide_all_sprites();
        return;
    }

    /* Knocked-out players take a beat on the floor, then run off the pitch and
     * are removed from the board entirely. */
    for (i = 0; i < ELIM_PLAYERS; i++)
    {
        Fighter *f = &fighters[i];
        if (!f->out || f->gone) continue;
        if (f->fallTimer > 0)
        {
            f->fallTimer--;
            if (f->fallTimer == FALL_FRAMES)
                player_setPose(&f->p, POSE_FALL, 255);
            if (f->fallTimer == 0) player_eliminate(&f->p);   /* start the exit */
        }
        else if (player_updateExit(&f->p))
        {
            f->gone = TRUE;   /* cleared the screen - stop drawing them */
        }
    }

    if (result == 3)
    {
        bool losersGone = TRUE;
        for (i = 0; i < ELIM_PLAYERS; i++)
        {
            if (fighters[i].out && !fighters[i].gone)
            {
                losersGone = FALSE;
                break;
            }
        }
        if (losersGone)
        {
            result = pendingResult;
            endTimer = (u16)((SYS_isPAL() ? 50 : 60) *
                             RESULT_SCREEN_TIMEOUT_SECONDS);
            draw_result();
            sound_mgr_crowdGameOver();
            VDP_clearSprites();
            sprites_data_hide_all_sprites();
            return;
        }
    }

    /* Advance/load the exact pose that will be drawn before calculating
     * hardware depth. This is essential while a defeated fighter changes
     * from hit to fall to run-off. */
    for (i = 0; i < ELIM_PLAYERS; i++)
    {
        Fighter *f = &fighters[i];
        bool moving = (!f->out) && f->moving;
        u16 sourceBase;
        if (f->gone) continue;
        if (f->out) moving = f->p.exiting;   /* legs move while running off */
        /* Front/back orientation is retained from the last vertical movement,
         * not recomputed from which half of the open court they occupy. */
        player_tickAnim(&f->p, moving);
        sourceBase = player_currentTileBase(&f->p);
        if (sourceBase != f->loadedTileBase)
        {
            sprites_data_load_eliminator_player(f->team, sourceBase,
                                                 f->p.tileBaseOverride);
            f->loadedTileBase = sourceBase;
        }
    }

    assign_fighter_depth_slots();

    for (i = 0; i < ELIM_PLAYERS; i++)
    {
        Fighter *f = &fighters[i];
        if (f->gone)
        {
            /* Park the slot off-screen but keep the sprite link chain intact. */
            VDP_setSpriteFull(f->p.spriteSlot, -64, -64, SPRITE_SIZE(1, 1),
                              TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE,
                                             TILE_BALL_SHADOW),
                              (u8)(f->p.spriteSlot + 1));
            continue;
        }
        player_draw(&f->p);
    }
    for (b = 0; b < ELIM_BALLS; b++) ball_draw(&balls[b]);
    impact_fx_draw(0);

    /* Ring under the human so they can always find themselves in the crowd. */
    if ((markerPulseTick & 15) == 0)
        sprites_data_set_ring_pulse((u8)((markerPulseTick >> 4) & 3), TRUE);
    markerPulseTick++;
    /* With two humans the chain must CARRY ON to player 2's ring - terminating
     * here left their marker unreachable and therefore invisible. */
    {
        u8 markerLink = TWO_PLAYERS() ? (u8)(SLOT_MARKER + 1) : 0;
        if (!fighters[humanIdx].out)
            VDP_setSpriteFull(SLOT_MARKER, fighters[humanIdx].p.x - 4,
                              fighters[humanIdx].p.y + 8, SPRITE_SIZE(3, 2),
                              TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                                             fighters[humanIdx].ball == NO_BALL
                                                 ? TILE_ELIM_RING_YELLOW
                                                 : TILE_ELIM_RING_RED),
                              markerLink);
        else
            VDP_setSpriteFull(SLOT_MARKER, -32, -32, SPRITE_SIZE(1, 1),
                              TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE,
                                             TILE_BALL_SHADOW), markerLink);
    }

    /* Player 2 gets their own ring in their own colours, so neither human can
     * lose track of which body is theirs in a ten-way scrap. */
    if (TWO_PLAYERS())
    {
        Fighter *p2 = &fighters[HUMAN2_IDX];
        bool show = !p2->out;
        VDP_setSpriteFull(SLOT_MARKER + 1,
                          show ? (s16)(p2->p.x - 4) : (s16)-32,
                          show ? (s16)(p2->p.y + 8) : (s16)-32,
                          SPRITE_SIZE(3, 2),
                          TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                                         p2->ball == NO_BALL
                                             ? TILE_ELIM_RING_BLUE
                                             : TILE_ELIM_RING_PINK), 0);
    }

    if (++hudTick >= 30) { hudTick = 0; if (result == 0) draw_hud(); }
}
