/*
 * scene_boot.c - The "minnka" studio splash, shown once before the
 * title screen. Skippable with Start so it never gets in the way of
 * someone who just wants to play.
 */
#include "genesis.h"
#include "scene_boot.h"
#include "game_state.h"
#include "logo_data.h"
#include "input_mgr.h"
#include "sound_mgr.h"
#include "screen_transition.h"

#define BOOT_DURATION   150   /* ~2.5s at 60fps */

static u16 bootTimer;

void scene_boot_enter(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearSprites();

    logo_data_draw();

    bootTimer = BOOT_DURATION;
    screen_transition_fade_in();
}

void scene_boot_update(void)
{
    input_mgr_update();

    if (bootTimer > 0) bootTimer--;

    if ((bootTimer == 0) || input_pressed(BUTTON_START))
    {
        sound_mgr_confirm();
        screen_transition_fade_out();
        gCurrentScene = GS_MENU;
    }
}
