#include "genesis.h"
#include "scene_gameover.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "sound_mgr.h"
#include "court_bg.h"
#include "ui_data.h"
#include "sprites_data.h"
#include "flag_data.h"
#include "player.h"

/* No celebrating figure on this screen - it is a clean results card. */

void scene_gameover_enter(void)
{
    bool aWon = gScoreA > gScoreB;
    const char *winner = aWon ? teamNames[gTeamAIndex] : teamNames[gTeamBIndex];
    char buf[4];

    VDP_clearSprites();
    sprites_data_hide_all_sprites();   /* no stray match sprites bleeding in */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    /* Rectangular tilemap clear, NOT VDP_clearTextArea(): the latter stamps the
     * FONT's space tile, and the UI/title art has reclaimed that font VRAM, so
     * it painted vertical white stripes across the whole screen. */
    VDP_clearTileMapRect(BG_A, 0, 0, 40, 28);
    court_bg_draw();
    sprites_data_apply_teams(gTeamAIndex, gTeamBIndex);

    ui_set_palette(PAL0);
    ui_apply_palette();
    /* Solid navy behind the panel so the text glyphs don't reveal the court. */
    flag_data_fill_panel(3, 5, 34, 19);
    ui_draw_panel(3, 5, 34, 19, TRUE);
    if (gGameMode == MODE_TOURNAMENT)
    {
        if (aWon)
        {
            ui_draw_big_center("CHAMPIONS", 7, UI_GOLD);
            ui_draw_text_center(teamNames[gTeamAIndex], 11, UI_CYAN);
            ui_draw_text_center("WIN THE TOURNAMENT", 13, UI_WHITE);
        }
        else
        {
            ui_draw_big_center("ELIMINATED", 7, UI_GOLD);
            ui_draw_text_center(teamNames[gTeamBIndex], 11, UI_CYAN);
            ui_draw_text_center("KNOCK YOU OUT OF THE CUP", 13, UI_WHITE);
        }
    }
    else
    {
        ui_draw_big_center("GAME OVER", 7, UI_GOLD);
        ui_draw_text_center(winner, 11, UI_CYAN);
        ui_draw_text_center("WORLD CHAMPIONS", 13, UI_WHITE);
    }

    intToStr(gScoreA, buf, 1);
    ui_draw_big_text(buf, 14, 16, UI_GOLD);
    ui_draw_big_text("-", 19, 16, UI_WHITE);
    intToStr(gScoreB, buf, 1);
    ui_draw_big_text(buf, 24, 16, UI_GOLD);

    ui_draw_button("EXIT", 11, 21, 18);

    sound_mgr_score();
    sound_mgr_crowdGameOver();   /* big sustained stadium roar for the win */
}

void scene_gameover_update(void)
{
    input_mgr_update();

    if (input_pressed(BUTTON_START) || input_pressed(BUTTON_A))
    {
        sound_mgr_confirm();
        PAL_fadeOutAll(20, FALSE);
        gCurrentScene = GS_MENU;
    }
}
