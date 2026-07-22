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
#include "ui_data.h"

#define BOOT_DURATION   150   /* ~2.5s at 60fps */

static u16 bootTimer;

void scene_boot_enter(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearSprites();
    VDP_clearTextArea(0, 0, 40, 28);

    logo_data_draw();
    ui_set_palette(PAL3);
    ui_apply_palette();
    ui_draw_text_center("PRESENTS", 4, UI_GOLD);
    ui_draw_text_center("ORIGINAL 16 BIT SPORTS", 23, UI_CYAN);

    bootTimer = BOOT_DURATION;
}

void scene_boot_update(void)
{
    input_mgr_update();

    if (bootTimer > 0) bootTimer--;

    if ((bootTimer == 0) || input_pressed(BUTTON_START))
    {
        sound_mgr_confirm();
        PAL_fadeOutAll(20, FALSE);
        gCurrentScene = GS_MENU;
    }
}
