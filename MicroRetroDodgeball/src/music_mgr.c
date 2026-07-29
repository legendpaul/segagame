#include "music_mgr.h"
#include "game_state.h"
#include "sound_mgr.h"
#include "audio.h"
#include "scene_menu.h"

static u8 lastScene = 0xFF;
static bool lastTitleContext = FALSE;
static u8 lastMenuPhase = 0xFF;
static bool titleContext = TRUE;

void music_mgr_init(void)
{
    lastScene = 0xFF;
    lastTitleContext = !titleContext;
    lastMenuPhase = 0xFF;
}

void music_mgr_setMenuContext(bool titleScreen)
{
    titleContext = titleScreen;
}

void music_mgr_update(void)
{
    u8 currentScene = (u8)gCurrentScene;
    u8 menuPhase = 0;

    if (currentScene == GS_MENU)
    {
        menuPhase = scene_menu_get_phase();
    }

    if (lastScene != currentScene || lastTitleContext != titleContext || lastMenuPhase != menuPhase)
    {
        // Scene, context, or menu phase transitioned! Update music track.
        lastScene = currentScene;
        lastTitleContext = titleContext;
        lastMenuPhase = menuPhase;

        // Stop existing music play
        SND_PCM4_stopPlay(SOUND_PCM_CH1);

        if (currentScene == GS_MENU)
        {
            if (titleContext)
            {
                SND_PCM4_startPlay(MUSIC_TITLE_LOOP, sizeof(MUSIC_TITLE_LOOP), SOUND_PCM_CH1, TRUE);
            }
            else if (menuPhase == 4) // MENU_CUP (cup ladder bracket)
            {
                SND_PCM4_startPlay(MUSIC_TOURNAMENT_LOOP, sizeof(MUSIC_TOURNAMENT_LOOP), SOUND_PCM_CH1, TRUE);
            }
            else
            {
                SND_PCM4_startPlay(MUSIC_MENU_LOOP, sizeof(MUSIC_MENU_LOOP), SOUND_PCM_CH1, TRUE);
            }
        }
        else if (currentScene == GS_GAMEOVER)
        {
            if (gGameMode == MODE_TOURNAMENT && gScoreA > gScoreB)
            {
                SND_PCM4_startPlay(MUSIC_TOURNAMENT_WIN, sizeof(MUSIC_TOURNAMENT_WIN), SOUND_PCM_CH1, FALSE);
            }
        }
    }
}
