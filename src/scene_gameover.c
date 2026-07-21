#include "genesis.h"
#include "scene_gameover.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "sound_mgr.h"
#include "court_bg.h"
#include "ui_data.h"
#include "sprites_data.h"
#include "player.h"

static Player champion;
static u16 victoryCounter;
static s16 victoryBob;

void scene_gameover_enter(void)
{
    bool aWon = gScoreA > gScoreB;
    const char *winner = aWon ? teamNames[gTeamAIndex] : teamNames[gTeamBIndex];
    char buf[4];

    VDP_clearSprites();
    VDP_clearPlane(VDP_BG_A, TRUE);
    VDP_clearPlane(VDP_BG_B, TRUE);
    VDP_clearTextArea(0, 0, 40, 28);
    court_bg_draw();
    sprites_data_apply_teams(gTeamAIndex, gTeamBIndex);

    ui_set_palette(PAL0);
    ui_apply_palette();
    ui_draw_panel(3, 5, 34, 19, TRUE);
    ui_draw_big_center("GAME OVER", 7, UI_GOLD);
    ui_draw_text_center(winner, 11, UI_CYAN);
    ui_draw_text_center("WORLD CHAMPIONS", 13, UI_WHITE);

    intToStr(gScoreA, buf, 1);
    ui_draw_big_text(buf, 14, 16, UI_GOLD);
    ui_draw_big_text("-", 19, 16, UI_WHITE);
    intToStr(gScoreB, buf, 1);
    ui_draw_big_text(buf, 24, 16, UI_GOLD);

    ui_draw_button("START REMATCH", 11, 21, 18);

    /* Animated winning-kit figure turns the result into a celebration
     * screen instead of a static text card. */
    player_init(&champion, 278, 153, 0, aWon ? PAL_TEAM_A : PAL_TEAM_B);
    player_setPose(&champion, POSE_CATCH, 255);
    victoryCounter = 0;
    victoryBob = 0;
    player_draw(&champion);

    sound_mgr_score();
}

void scene_gameover_update(void)
{
    input_mgr_update();

    if (++victoryCounter >= 18)
    {
        victoryCounter = 0;
        victoryBob = victoryBob ? 0 : -3;
    }
    champion.y = 153 + victoryBob;
    player_draw(&champion);

    if (input_pressed(BUTTON_START))
    {
        sound_mgr_confirm();
        PAL_fadeOutAll(20, FALSE);
        gCurrentScene = GS_MENU;
    }
}
