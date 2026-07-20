/*
 * main.c - MEGA DODGEBALL entry point and scene dispatcher.
 *
 * A Sega Mega Drive dodgeball game built with SGDK. See docs/planning.md
 * for the original design brief this implements: menu with team select,
 * a full match with movement/throw/catch/scoring, and a game-over scene.
 */
#include "genesis.h"
#include "game_state.h"
#include "sprites_data.h"
#include "court_bg.h"
#include "sound_mgr.h"
#include "music_mgr.h"
#include "scene_boot.h"
#include "scene_menu.h"
#include "scene_match.h"
#include "scene_gameover.h"

/* Globals declared extern in game_state.h */
GameScene gCurrentScene;
u8 gTeamAIndex;
u8 gTeamBIndex;
u8 gScoreA;
u8 gScoreB;

int main(bool hardReset)
{
    GameScene lastScene = GS_GAMEOVER + 1; /* force scene_*_enter() on first loop */

    setRandomSeed(GET_HVCOUNTER ^ 0xACE1);

    /* Force all VDP_drawText/VDP_clearText* calls onto a single, known
     * plane so each scene's VDP_clearPlane(VDP_BG_A, ...) reliably wipes
     * whatever the previous scene drew there. */
    VDP_setTextPlane(VDP_BG_A);

    sprites_data_init();
    court_bg_init();
    sound_mgr_init();
    music_mgr_init();

    gCurrentScene = GS_BOOT;
    gTeamAIndex = 0;
    gTeamBIndex = 1;
    gScoreA = 0;
    gScoreB = 0;

    while (TRUE)
    {
        if (gCurrentScene != lastScene)
        {
            setRandomSeed(GET_HVCOUNTER ^ (u16)(gCurrentScene + 1));

            switch (gCurrentScene)
            {
                case GS_BOOT:     scene_boot_enter();     break;
                case GS_MENU:     scene_menu_enter();     break;
                case GS_MATCH:    scene_match_enter();    break;
                case GS_GAMEOVER: scene_gameover_enter(); break;
            }

            lastScene = gCurrentScene;
        }

        switch (gCurrentScene)
        {
            case GS_BOOT:     scene_boot_update();     break;
            case GS_MENU:     scene_menu_update();     break;
            case GS_MATCH:    scene_match_update();    break;
            case GS_GAMEOVER: scene_gameover_update(); break;
        }

        VDP_updateSprites(80, DMA);
        sound_mgr_update();
        music_mgr_update();
        SYS_doVBlankProcess();
    }

    return 0;
}
