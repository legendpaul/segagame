/*
 * scene_menu.c - Arcade title and sequential national-team selection.
 *
 * Flow: title -> choose Team 1 -> choose Team 2 -> broadcast matchup ->
 * match. Each team gets its own selector, large flag and kit preview.
 */
#include "genesis.h"
#include "scene_menu.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "sound_mgr.h"
#include "flag_data.h"
#include "title_data.h"
#include "sprites_data.h"
#include "player.h"
#include "ui_data.h"
#include "matchup_art.h"

#define SLOT_PREVIEW_A  0
#define SLOT_PREVIEW_B  1
#define PREVIEW_Y      150

#define BLINK_PERIOD    30
#define BOB_PERIOD      20

typedef enum {
    MENU_TITLE = 0,
    MENU_TEAM_A,
    MENU_TEAM_B,
    MENU_MATCHUP
} MenuPhase;

static MenuPhase phase;
static Player previewA, previewB;
static u16 blinkCounter;
static bool promptVisible;
static u16 bobCounter;
static s16 bobOffset;

static void draw_title(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_clearSprites();
    sprites_data_hide_all_sprites();
    title_data_draw();
    blinkCounter = 0;
    promptVisible = TRUE;
}

static void draw_selector(void)
{
    u8 selected = (phase == MENU_TEAM_A) ? gTeamAIndex : gTeamBIndex;
    flag_data_draw_selector(selected, (phase == MENU_TEAM_A) ? 1 : 2);

    /* Large 32x32 kit previews make the choice visible as a football side,
     * not just a line of text. On Team 2's screen Team 1 remains alongside
     * it as the locked-in opponent. */
    sprites_data_apply_teams(gTeamAIndex, gTeamBIndex);
    player_init(&previewA, 258, PREVIEW_Y, SLOT_PREVIEW_A, PAL_TEAM_A);
    player_init(&previewB, 298, PREVIEW_Y, SLOT_PREVIEW_B, PAL_TEAM_B);
    previewA.farSide = TRUE;
    previewB.farSide = TRUE;
    previewA.facingLeft = FALSE;
    previewB.facingLeft = TRUE;
    if (phase == MENU_TEAM_A)
    {
        previewA.x = 278;
        previewA.facingLeft = TRUE;
        previewB.x = -100;
    }
    ui_draw_text(phase == MENU_TEAM_A ? "P1" : "P1   P2", 31, 16, UI_GOLD);
}

static void enter_selector(MenuPhase next)
{
    phase = next;
    VDP_clearSprites();
    sprites_data_hide_all_sprites();
    draw_selector();
    bobCounter = 0;
    bobOffset = 0;
}

static void enter_matchup(void)
{
    phase = MENU_MATCHUP;
    VDP_clearSprites();
    sprites_data_hide_all_sprites();
    flag_data_draw_matchup(gTeamAIndex, gTeamBIndex);
    /* Big recolourable throwing figures replace the old tiny preview
     * sprites - upload the tile bank, then draw both sides. */
    matchup_art_load();
    matchup_art_draw(gTeamAIndex, gTeamBIndex);
    bobCounter = 0;
    bobOffset = 0;
}

static void move_selection(s8 delta)
{
    u8 *selected = (phase == MENU_TEAM_A) ? &gTeamAIndex : &gTeamBIndex;
    *selected = (u8)((*selected + NUM_TEAMS + delta) % NUM_TEAMS);
    draw_selector();
    sound_mgr_blip();
}

void scene_menu_enter(void)
{
    VDP_setTextPalette(PAL0);
    phase = MENU_TITLE;
    draw_title();
}

void scene_menu_update(void)
{
    input_mgr_update();

    if (phase == MENU_TITLE)
    {
        if (++blinkCounter >= BLINK_PERIOD)
        {
            blinkCounter = 0;
            promptVisible = !promptVisible;
            title_data_set_prompt(promptVisible);
        }

        if (input_pressed(BUTTON_START) || input_pressed(BUTTON_A))
        {
            sound_mgr_confirm();
            /* The full-screen title temporarily occupies the UI font's
             * VRAM region. Restore it before drawing either selector. */
            ui_data_init();
            enter_selector(MENU_TEAM_A);
        }
        return;
    }

    if (phase == MENU_MATCHUP)
    {
        /* Figures are static BG tiles drawn once on entry - just wait on
         * input here. */
        if (input_pressed(BUTTON_B))
        {
            sound_mgr_cancel();
            enter_selector(MENU_TEAM_B);
            return;
        }
        if (input_pressed(BUTTON_A) || input_pressed(BUTTON_START))
        {
            sound_mgr_confirm();
            gScoreA = 0;
            gScoreB = 0;
            PAL_fadeOutAll(20, FALSE);
            gCurrentScene = GS_MATCH;
            return;
        }
        return;
    }

    if (++bobCounter >= BOB_PERIOD)
    {
        bobCounter = 0;
        bobOffset = bobOffset ? 0 : -2;
    }

    if (input_pressed(BUTTON_UP) || input_pressed(BUTTON_LEFT))
        move_selection(-1);
    else if (input_pressed(BUTTON_DOWN) || input_pressed(BUTTON_RIGHT))
        move_selection(1);
    else if (input_pressed(BUTTON_C))
    {
        /* C cancels the whole team-select flow back to the title screen.
         * (title_data_draw reclaims the UI font VRAM; it's restored again
         * by ui_data_init() when START re-enters the selector.) */
        sound_mgr_cancel();
        VDP_clearSprites();
        phase = MENU_TITLE;
        draw_title();
        return;
    }
    else if (input_pressed(BUTTON_B) && phase == MENU_TEAM_B)
    {
        sound_mgr_cancel();
        enter_selector(MENU_TEAM_A);
    }
    else if (input_pressed(BUTTON_A) || input_pressed(BUTTON_START))
    {
        sound_mgr_confirm();
        if (phase == MENU_TEAM_A)
        {
            /* Begin the other selector on a different team by default,
             * while still allowing any country to be chosen deliberately. */
            gTeamBIndex = (gTeamAIndex + 1) % NUM_TEAMS;
            enter_selector(MENU_TEAM_B);
        }
        else
        {
            enter_matchup();
            return;
        }
    }

    previewA.y = PREVIEW_Y + bobOffset;
    previewB.y = PREVIEW_Y - bobOffset;
    player_draw(&previewA);
    player_draw(&previewB);
}
