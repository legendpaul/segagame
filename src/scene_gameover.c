#include "genesis.h"
#include "scene_gameover.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "sound_mgr.h"

void scene_gameover_enter(void)
{
    VDP_clearSprites();
    VDP_clearPlane(VDP_BG_A, TRUE);
    VDP_clearPlane(VDP_BG_B, TRUE);
    VDP_setTextPalette(PAL0);
    VDP_clearTextArea(0, 0, 40, 28);

    bool aWon = gScoreA > gScoreB;
    const char *winner = aWon ? teamNames[gTeamAIndex] : teamNames[gTeamBIndex];

    VDP_drawText("GAME OVER", 15, 8);
    VDP_drawTextFill(winner, (40 - strlen(winner)) / 2, 12, strlen(winner));
    VDP_drawText("WINS THE MATCH!", 12, 14);

    char buf[16];
    intToStr(gScoreA, buf, 1);
    VDP_drawTextFill(buf, 17, 16, 2);
    VDP_drawText("-", 20, 16);
    intToStr(gScoreB, buf, 1);
    VDP_drawTextFill(buf, 22, 16, 2);

    VDP_drawText("PRESS START", 14, 20);

    sound_mgr_score();
}

void scene_gameover_update(void)
{
    input_mgr_update();

    if (input_pressed(BUTTON_START))
    {
        sound_mgr_blip();
        gCurrentScene = GS_MENU;
    }
}
