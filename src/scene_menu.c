/*
 * scene_menu.c - Team-select / title screen.
 *
 * National-team selector: ten ranked countries appear as a 5x2 flag
 * grid with a hardware-tile selection frame. The actual player sprite
 * stands below in the selected national kit against the next-ranked
 * opponent, with a bouncing ball between them.
 */
#include "genesis.h"
#include "scene_menu.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "sound_mgr.h"
#include "court_bg.h"
#include "flag_data.h"
#include "sprites_data.h"
#include "player.h"

#define SLOT_HERO_A   0
#define SLOT_HERO_B   1
#define SLOT_BALL     2

#define HERO_Y        150
#define HERO_A_X       80
#define HERO_B_X      240

#define BALL_BASE_X   160
#define BALL_BASE_Y   142
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
    return (gTeamAIndex + 1) % NUM_TEAMS;
}

static void draw_teams(void)
{
    u8 oppIndex = opponent_index();

    flag_data_draw_grid(gTeamAIndex);

    VDP_drawText("TEAM", 1, 13);
    VDP_drawTextFill(teamNames[gTeamAIndex], 6, 13, 11);
    VDP_drawText("VS", 19, 13);
    VDP_drawTextFill(teamNames[oppIndex], 23, 13, 11);

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

    VDP_setSpriteFull(SLOT_BALL, BALL_BASE_X, y, SPRITE_SIZE(1, 1),
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

    VDP_drawText("D-PAD SELECT   START PLAY", 7, 23);

    blinkCounter = 0;
    startVisible = TRUE;
    VDP_drawText("TOP 10 NATIONAL TEAMS", 9, 25);

    bobCounter = 0;
    bobOffset = 0;
    ballCounter = 0;
}

void scene_menu_update(void)
{
    input_mgr_update();

    /* Keep the prompt persistent. Clearing a text row writes opaque
     * font-space tiles on this SGDK setup and used to flash a thick
     * black bar across the isometric court. */
    if (++blinkCounter >= BLINK_PERIOD)
    {
        blinkCounter = 0;
        startVisible = TRUE;
        VDP_drawText("TOP 10 NATIONAL TEAMS", 9, 25);
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
        u8 rowBase = (gTeamAIndex / 5) * 5;
        gTeamAIndex = rowBase + ((gTeamAIndex + 4) % 5);
        draw_teams();
        sound_mgr_blip();
    }
    else if (input_pressed(BUTTON_RIGHT))
    {
        u8 rowBase = (gTeamAIndex / 5) * 5;
        gTeamAIndex = rowBase + ((gTeamAIndex + 1) % 5);
        draw_teams();
        sound_mgr_blip();
    }
    else if (input_pressed(BUTTON_UP) || input_pressed(BUTTON_DOWN))
    {
        gTeamAIndex = (gTeamAIndex + 5) % NUM_TEAMS;
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
