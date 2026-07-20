#include "scene_menu.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "sound_mgr.h"
#include "court_bg.h"

static void draw_teams(void)
{
    VDP_clearTextLine(14);
    VDP_clearTextLine(16);
    VDP_drawText("YOUR TEAM:", 9, 14);
    VDP_drawTextFill(teamNames[gTeamAIndex], 20, 14, 14);

    VDP_drawText("OPPONENT :", 9, 16);
    VDP_drawTextFill(teamNames[(gTeamAIndex + 2) % NUM_TEAMS], 20, 16, 14);
}

void scene_menu_enter(void)
{
    VDP_clearSprites();

    VDP_clearPlane(VDP_BG_A, TRUE);
    VDP_clearPlane(VDP_BG_B, TRUE);
    VDP_setTextPalette(PAL0);
    VDP_clearTextArea(0, 0, 40, 28);
    court_bg_draw();

    VDP_drawText("MEGA DODGEBALL", 13, 6);
    VDP_drawText("------------------------------", 4, 8);

    draw_teams();

    VDP_drawText("<  >  CHANGE TEAM", 11, 19);
    VDP_drawText("START TO PLAY", 13, 21);
}

void scene_menu_update(void)
{
    input_mgr_update();

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
        gTeamBIndex = (gTeamAIndex + 2) % NUM_TEAMS;
        gScoreA = 0;
        gScoreB = 0;
        sound_mgr_blip();
        gCurrentScene = GS_MATCH;
    }
}
