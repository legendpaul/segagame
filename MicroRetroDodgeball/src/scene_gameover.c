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
#include "screen_transition.h"

/* No celebrating figure on this screen - it is a clean broadcast result. */

static u16 resultTimer;

static u16 result_name_x(const char *name, u16 regionX, u16 regionWidth)
{
    u16 length = (u16)strlen(name);
    if (length >= regionWidth) return regionX;
    return regionX + (regionWidth - length) / 2;
}

void scene_gameover_enter(void)
{
    bool aWon = gScoreA > gScoreB;
    const char *winner = aWon ? teamNames[gTeamAIndex] : teamNames[gTeamBIndex];
    char buf[4];

    resultTimer = (u16)((SYS_isPAL() ? 50 : 60) *
                        RESULT_SCREEN_TIMEOUT_SECONDS);

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
    ui_draw_panel(2, 4, 36, 22, TRUE);
    if (gGameMode == MODE_TOURNAMENT)
    {
        if (aWon)
            ui_draw_big_center("CHAMPIONS", 6, UI_GOLD);
        else
            ui_draw_big_center("ELIMINATED", 6, UI_GOLD);
    }
    else
        ui_draw_big_center("FULL TIME", 6, UI_GOLD);

    ui_draw_text_center("FINAL SCORE", 9, UI_CYAN);
    flag_data_draw_large(gTeamAIndex, 8, 11, PAL3);
    flag_data_draw_large(gTeamBIndex, 28, 11, PAL3);
    ui_draw_text(teamNames[gTeamAIndex],
                 result_name_x(teamNames[gTeamAIndex], 3, 15), 14, UI_WHITE);
    ui_draw_text(teamNames[gTeamBIndex],
                 result_name_x(teamNames[gTeamBIndex], 22, 15), 14, UI_WHITE);

    intToStr(gScoreA, buf, 1);
    ui_draw_big_text(buf, 15, 11, UI_GOLD);
    ui_draw_big_text("-", 19, 11, UI_WHITE);
    intToStr(gScoreB, buf, 1);
    ui_draw_big_text(buf, 23, 11, UI_GOLD);

    ui_draw_text_center(winner, 17, UI_GOLD);
    ui_draw_text_center(gGameMode == MODE_TOURNAMENT && aWon
                        ? "TOURNAMENT CHAMPIONS"
                        : gGameMode == MODE_TOURNAMENT
                        ? "TOURNAMENT OVER"
                        : "MATCH WINNERS", 19, UI_WHITE);

    ui_draw_button("CONTINUE", 13, 23, 14);

    screen_transition_fade_in();
    sound_mgr_score();
    sound_mgr_crowdGameOver();   /* big sustained stadium roar for the win */
}

void scene_gameover_update(void)
{
    input_mgr_update();

    if (resultTimer > 0) resultTimer--;

    if (resultTimer == 0 || input_pressed_any(BUTTON_BTN))
    {
        sound_mgr_confirm();
        screen_transition_fade_out();
        gMenuEntry = (gGameMode == MODE_TOURNAMENT)
            ? MENU_ENTRY_CUP_LADDER : MENU_ENTRY_TITLE;
        gCurrentScene = GS_MENU;
    }
}
