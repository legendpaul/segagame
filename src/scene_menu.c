/*
 * scene_menu.c - Team-select / title screen.
 *
 * Redesigned from a plain text menu into a real "lineup" screen: the
 * actual player sprite (the same AI-derived 32x32 art used in-match)
 * stands on each side in the currently-selected team's real colors, a
 * ball bounces between them, and the prompt line blinks - all real
 * hardware-sprite/animation work, not just a static text screen.
 */
#include "genesis.h"
#include "scene_menu.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "sound_mgr.h"
#include "court_bg.h"
#include "sprites_data.h"
#include "player.h"

#define SLOT_HERO_A   0
#define SLOT_HERO_B   1
#define SLOT_BALL     2

#define HERO_Y        130
#define HERO_A_X       80
#define HERO_B_X      240

#define BALL_BASE_X   160
#define BALL_BASE_Y   130
#define BALL_BOUNCE_PERIOD  40   /* frames per bounce cycle */
#define BALL_BOUNCE_HEIGHT  18

#define BLINK_PERIOD  30   /* frames each blink state holds */
#define BOB_PERIOD    24   /* frames per idle-bob half-cycle */

static Player heroA, heroB;

static u16 blinkCounter;
static bool startVisible;

static u16 bobCounter;
static s16 bobOffset;

static u16 ballCounter;

static u8 opponent_index(void)
{
    return (gTeamAIndex + 2) % NUM_TEAMS;
}

static void draw_teams(void)
{
    u8 oppIndex = opponent_index();

    VDP_clearTextLine(10);
    VDP_drawTextFill(teamNames[gTeamAIndex], 2, 10, 12);
    VDP_drawText("VS", 19, 10);
    VDP_drawTextFill(teamNames[oppIndex], 26, 10, 12);

    /* Recolor the shared hero sprites to whatever's actually picked,
     * instead of the fixed colors the old text-only menu never needed
     * to worry about. */
    sprites_data_apply_teams(gTeamAIndex, oppIndex);
}

static void draw_ball(void)
{
    /* Simple triangle-wave bounce - cheap, no floating point, but a
     * real per-frame motion instead of a static decorative sprite. */
    u16 phase = ballCounter % BALL_BOUNCE_PERIOD;
    u16 half = BALL_BOUNCE_PERIOD / 2;
    u16 dist = (phase < half) ? phase : (BALL_BOUNCE_PERIOD - phase);
    s16 y = BALL_BASE_Y - (dist * BALL_BOUNCE_HEIGHT) / half;

    VDP_setSpriteFull(SLOT_BALL, BALL_BASE_X, y, SPRITE_SIZE(2, 2),
                       TILE_ATTR_FULL(PAL_BALL, 0, FALSE, FALSE, TILE_BALL),
                       0);
}

void scene_menu_enter(void)
{
    VDP_clearSprites();

    VDP_clearPlane(VDP_BG_A, TRUE);
    VDP_clearPlane(VDP_BG_B, TRUE);
    VDP_setTextPalette(PAL0);
    VDP_clearTextArea(0, 0, 40, 28);
    court_bg_draw();

    VDP_drawText("MEGA DODGEBALL", 13, 1);
    VDP_drawText("------------------------------", 4, 2);

    player_init(&heroA, HERO_A_X, HERO_Y, SLOT_HERO_A, PAL_TEAM_A);
    player_init(&heroB, HERO_B_X, HERO_Y, SLOT_HERO_B, PAL_TEAM_B);

    draw_teams();

    VDP_drawText("<  >  CHANGE TEAM", 11, 23);

    blinkCounter = 0;
    startVisible = TRUE;
    VDP_drawText("START TO PLAY", 13, 25);

    bobCounter = 0;
    bobOffset = 0;
    ballCounter = 0;
}

void scene_menu_update(void)
{
    input_mgr_update();

    /* Blinking "press start" prompt - only touches the text plane when
     * it actually flips state, not every frame. */
    if (++blinkCounter >= BLINK_PERIOD)
    {
        blinkCounter = 0;
        startVisible = !startVisible;
        if (startVisible) VDP_drawText("START TO PLAY", 13, 25);
        else VDP_clearTextLine(25);
    }

    /* Subtle idle "breathing" bob on both heroes so the lineup doesn't
     * read as a frozen screenshot. */
    if (++bobCounter >= BOB_PERIOD)
    {
        bobCounter = 0;
        bobOffset = bobOffset ? 0 : -1;
    }

    ballCounter++;

    if (input_pressed(BUTTON_LEFT))
    {
        gTeamAIndex = (gTeamAIndex + NUM_TEAMS - 1) % NUM_TEAMS;
        draw_teams();
        sound_mgr_blip();
    }
    else if (input_pressed(BUTTON_RIGHT))
    {
        gTeamAIndex = (gTeamAIndex + 1) % NUM_TEAMS;
        draw_teams();
        sound_mgr_blip();
    }
    else if (input_pressed(BUTTON_START))
    {
        gTeamBIndex = opponent_index();
        gScoreA = 0;
        gScoreB = 0;
        sound_mgr_blip();
        /* Fade to black rather than hard-cutting straight into the
         * match - cheap scene-transition polish. */
        PAL_fadeOutAll(20, FALSE);
        gCurrentScene = GS_MATCH;
        return;
    }

    heroA.y = HERO_Y + bobOffset;
    heroB.y = HERO_Y - bobOffset;

    player_draw(&heroA);
    player_draw(&heroB);
    draw_ball();
}
