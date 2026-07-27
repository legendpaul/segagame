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
    MENU_MODE,
    MENU_TEAM_A,
    MENU_TEAM_B,
    MENU_CUP,       /* tournament ladder: intro before match 1, progress after each win */
    MENU_MATCHUP
} MenuPhase;

static MenuPhase phase;
static Player previewA, previewB;
static u16 blinkCounter;
static bool promptVisible;
static u16 bobCounter;
static s16 bobOffset;
static u8 modeRow;   /* 0 = mode, 1 = difficulty, on the setup screen */

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

/* Redraw only the values that change on input - no full-plane clear, so the
 * setup screen never blanks/flashes on a keypress. Only the small cells that
 * actually change are cleared (marker columns, the variable-width level name
 * and description); the fixed-width mode name is simply overwritten. */
static void draw_mode_rows(void)
{
    static const char *diffName[3] = { "EASY", "NORMAL", "HARD" };
    VDP_clearTileMapRect(BG_A, 18, 11, 1, 1);
    VDP_clearTileMapRect(BG_A, 31, 11, 1, 1);
    VDP_clearTileMapRect(BG_A, 18, 14, 1, 1);
    VDP_clearTileMapRect(BG_A, 31, 14, 1, 1);
    VDP_clearTileMapRect(BG_A, 20, 14, 8, 1);
    VDP_clearTileMapRect(BG_A, 5, 20, 31, 1);

    ui_draw_text(gGameMode == MODE_EXHIBITION ? "EXHIBITION" : "TOURNAMENT",
                 20, 11, (modeRow == 0) ? UI_GOLD : UI_WHITE);
    if (modeRow == 0) { ui_draw_text(">", 18, 11, UI_GOLD); ui_draw_text("<", 31, 11, UI_GOLD); }

    ui_draw_text(diffName[gDifficulty], 20, 14, (modeRow == 1) ? UI_GOLD : UI_WHITE);
    if (modeRow == 1) { ui_draw_text(">", 18, 14, UI_GOLD); ui_draw_text("<", 31, 14, UI_GOLD); }

    ui_draw_text(gGameMode == MODE_EXHIBITION
                 ? "SINGLE MATCH VS ONE RIVAL"
                 : "BEAT EVERY RIVAL TO WIN THE CUP", 5, 20, UI_CYAN);
}

static void draw_mode(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearSprites();
    sprites_data_hide_all_sprites();
    flag_data_fill_panel(0, 0, 40, 28);   /* solid navy backdrop */
    ui_set_palette(PAL0);
    ui_apply_palette();
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x081830));

    ui_draw_big_center("GAME SETUP", 3, UI_WHITE);
    ui_draw_panel(6, 9, 28, 9, FALSE);
    ui_draw_text("MODE", 9, 11, UI_CYAN);
    ui_draw_text("LEVEL", 9, 14, UI_CYAN);
    ui_draw_text("A START", 9, 24, UI_GOLD);
    ui_draw_text("C BACK", 24, 24, UI_CYAN);
    draw_mode_rows();
}

/* Single-elimination bracket, read left to right: the eight-team quarter-final
 * column, the semi-final column, then the final - with the champion at the
 * far right. Knocked-out teams dim to cyan, teams still in stay white and the
 * player's own team is always gold, so the whole competition is legible at a
 * glance (the same read as a TV tournament roster). */
/* Column x positions and the row of every slot. Pairs sit two rows apart and
 * each round's winner sits on the midpoint row, so the connectors line up. */
/* Mirrored bracket: four teams down each side on plates, the joins converging
 * inward to the cup in the centre - the classic tournament-roster layout. */
/* Columns are spaced so every element is adjacent to the next and NOTHING
 * overlaps: plate | pair-join | half-join | CUP | half-join | pair-join | plate
 *   1..13   14(->15)   16(->17)  18..20  22(->21)  24(->23)   25..37          */
#define PLATE_L_X   1
#define PLATE_R_X  25
#define PLATE_W    13
/* Text rows of the eight plates: left side then right side, in bracket order
 * (pairs are adjacent, so 0-1 and 2-3 meet, then those winners meet). */
static const u16 PLATE_ROW[4] = { 5, 9, 15, 19 };
#define JOIN_L_IN  14   /* left  pair joins,  stub lands on 15 */
#define JOIN_L_MID 16   /* left  half join,   stub lands on 17 */
#define JOIN_R_MID 22   /* right half join,   stub lands on 21 */
#define JOIN_R_IN  24   /* right pair joins,  stub lands on 23 */
#define CUP_X      18   /* gold cup box, 18..20 - clear of both half joins */
#define CHAMP_ROW  23

/* A team plate: a framed box with the name inside, like the roster plates in
 * the reference. Gold frame for the player's own team so it stands out. */
static void bracket_plate(u8 team, u16 x, u16 row, u8 round)
{
    u8 style;
    bool mine = (team == gTeamAIndex);
    ui_draw_panel(x, (u16)(row - 1), PLATE_W, 3, mine);
    if (team == CUP_TBD) return;
    style = mine ? UI_GOLD : cup_is_out(team, round) ? UI_CYAN : UI_WHITE;
    ui_draw_text(teamNames[team], (u16)(x + 1), row, style);
}

static void draw_cup_bracket(void)
{
    static const char *roundName[CUP_ROUNDS] = {
        "QUARTER FINAL", "SEMI FINAL", "FINAL"
    };
    u8 i;

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearSprites();
    sprites_data_hide_all_sprites();
    flag_data_fill_panel(0, 0, 40, 28);   /* solid navy backdrop */
    ui_set_palette(PAL0);
    ui_apply_palette();
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x081830));

    ui_draw_text_center("TOURNAMENT", 0, UI_WHITE);
    ui_draw_text_center(cupChampion != CUP_TBD ? "COMPLETE"
                        : roundName[gCupStage < CUP_ROUNDS ? gCupStage : 2],
                        2, UI_GOLD);

    /* Eight plates: the first four slots down the left, the last four down the
     * right, so both halves converge on the cup in the middle. */
    for (i = 0; i < 4; i++)
        bracket_plate(cupQF[i], PLATE_L_X, PLATE_ROW[i], gCupStage);
    for (i = 0; i < 4; i++)
        bracket_plate(cupQF[4 + i], PLATE_R_X, PLATE_ROW[i], gCupStage);

    /* Pair joins, then each half's semi-final join heading inward. */
    ui_draw_bracket(JOIN_L_IN,  PLATE_ROW[0], PLATE_ROW[1], FALSE);
    ui_draw_bracket(JOIN_L_IN,  PLATE_ROW[2], PLATE_ROW[3], FALSE);
    ui_draw_bracket(JOIN_L_MID, 7, 17, FALSE);
    ui_draw_bracket(JOIN_R_IN,  PLATE_ROW[0], PLATE_ROW[1], TRUE);
    ui_draw_bracket(JOIN_R_IN,  PLATE_ROW[2], PLATE_ROW[3], TRUE);
    ui_draw_bracket(JOIN_R_MID, 7, 17, TRUE);

    /* The cup itself in the centre, where both halves meet. Sized and placed
     * so the converging lines stop at its edges rather than crossing it. */
    ui_draw_panel(CUP_X, 11, 3, 3, TRUE);
    ui_draw_text("CUP", CUP_X, 12, UI_GOLD);

    ui_draw_text_center(cupChampion != CUP_TBD ? teamNames[cupChampion]
                                               : "WINNER", CHAMP_ROW,
                        cupChampion != CUP_TBD ? UI_GOLD : UI_CYAN);

    /* Once the cup is won there is nothing left to continue into. */
    if (cupChampion == CUP_TBD)
        ui_draw_text("A CONTINUE", 2, 26, UI_GOLD);
    ui_draw_text("C EXIT", 31, 26, UI_CYAN);
}

static void enter_cup_ladder(void)
{
    phase = MENU_CUP;
    draw_cup_bracket();
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

    /* A mid-cup match win returns here to show the updated ladder rather than
     * the title. Consume the request so a later title-return works normally. */
    if (gMenuEntry == MENU_ENTRY_CUP_LADDER)
    {
        gMenuEntry = MENU_ENTRY_TITLE;
        ui_data_init();          /* the match may have reclaimed UI font VRAM */
        enter_cup_ladder();
        return;
    }

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
             * VRAM region. Restore it before drawing any menu text. */
            ui_data_init();
            phase = MENU_MODE;
            modeRow = 0;
            draw_mode();
        }
        return;
    }

    if (phase == MENU_MODE)
    {
        if (input_pressed(BUTTON_UP) || input_pressed(BUTTON_DOWN))
        {
            modeRow ^= 1;
            sound_mgr_blip();
            draw_mode_rows();   /* in-place value update, no plane clear/flash */
        }
        else if (input_pressed(BUTTON_LEFT) || input_pressed(BUTTON_RIGHT))
        {
            if (modeRow == 0)
                gGameMode = (gGameMode == MODE_EXHIBITION) ? MODE_TOURNAMENT
                                                           : MODE_EXHIBITION;
            else
            {
                s8 d = input_pressed(BUTTON_LEFT) ? 2 : 1;   /* -1 == +2 mod 3 */
                gDifficulty = (u8)((gDifficulty + d) % 3);
            }
            sound_mgr_blip();
            draw_mode_rows();   /* in-place value update, no plane clear/flash */
        }
        else if (input_pressed(BUTTON_C) || input_pressed(BUTTON_B))
        {
            sound_mgr_cancel();
            phase = MENU_TITLE;
            draw_title();
        }
        else if (input_pressed(BUTTON_A) || input_pressed(BUTTON_START))
        {
            sound_mgr_confirm();
            enter_selector(MENU_TEAM_A);
        }
        return;
    }

    if (phase == MENU_CUP)
    {
        /* Cup already won: this is the final standings, so only EXIT applies. */
        if (cupChampion == CUP_TBD &&
            (input_pressed(BUTTON_A) || input_pressed(BUTTON_START)))
        {
            sound_mgr_confirm();
            enter_matchup();
            return;
        }
        if (input_pressed(BUTTON_C) || input_pressed(BUTTON_B))
        {
            sound_mgr_cancel();
            phase = MENU_TITLE;
            draw_title();
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
        if (phase == MENU_TEAM_A && gGameMode == MODE_TOURNAMENT)
        {
            /* Tournament: you only pick your own team, then face the whole
             * gauntlet. Seed the cup and jump straight to the first matchup. */
            gCupStage = 0;
            gScoreA = 0;
            gScoreB = 0;
            cup_build(gTeamAIndex);   /* fresh 8-team knockout draw */
            gTeamBIndex = cup_opponent_now(gTeamAIndex, 0);
            enter_cup_ladder();   /* show the bracket, then A -> matchup */
            return;
        }
        if (phase == MENU_TEAM_A)
        {
            /* Exhibition: pick the opponent too. Default to a different team. */
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
