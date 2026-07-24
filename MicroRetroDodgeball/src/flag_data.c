#include "flag_data.h"
#include "teams.h"
#include "ui_data.h"

#define TILE_FLAG_PANEL  (TILE_FLAG_BASE + 0)
#define TILE_FLAG_BOX_H  (TILE_FLAG_BASE + 1)
#define TILE_FLAG_BOX_L  (TILE_FLAG_BASE + 2)
#define TILE_FLAG_BOX_R  (TILE_FLAG_BASE + 3)
#define TILE_FLAGS       (TILE_FLAG_BASE + 4)
#define TILE_FLAGS_LARGE (TILE_FLAGS + NUM_TEAMS * 2)
#define TILE_FLAG_SELECT (TILE_FLAGS_LARGE + NUM_TEAMS * 8)

#include "flag_tiles.inc"

static const u32 tile_flag_panel[8] = {
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444
};

/* White 1px selection frame on the same navy as the flag panel. */
static const u32 tile_flag_box_h[8] = {
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x11111111
};
static const u32 tile_flag_box_l[8] = {
    0x44444441, 0x44444441, 0x44444441, 0x44444441,
    0x44444441, 0x44444441, 0x44444441, 0x44444441
};
static const u32 tile_flag_box_r[8] = {
    0x14444444, 0x14444444, 0x14444444, 0x14444444,
    0x14444444, 0x14444444, 0x14444444, 0x14444444
};

static const u32 tile_flag_select[8] = {
    0x99999999, 0x99999999, 0x99999999, 0x99999999,
    0x99999999, 0x99999999, 0x99999999, 0x99999999
};

static void apply_flag_palette(void)
{
    /* Restore the selector's own background and text too: the studio/title
     * transition fades every palette entry to black before this screen. */
    PAL_setColor(0 * 16 + 0,  RGB24_TO_VDPCOLOR(0x101C38));
    PAL_setColor(0 * 16 + 1,  RGB24_TO_VDPCOLOR(0xF8F8F8));
    PAL_setColor(0 * 16 + 4,  RGB24_TO_VDPCOLOR(0x101C38));
    PAL_setColor(0 * 16 + 15, RGB24_TO_VDPCOLOR(0xF8F8F8));
    PAL_setColor(PAL3 * 16 + 0,  RGB24_TO_VDPCOLOR(0x2048B0));
    PAL_setColor(PAL3 * 16 + 15, RGB24_TO_VDPCOLOR(0xF8F8F8));
    /* Flags use the remaining seven colors. */
    PAL_setColor(0 * 16 + 8,  RGB24_TO_VDPCOLOR(0xD82830)); /* red */
    PAL_setColor(0 * 16 + 9,  RGB24_TO_VDPCOLOR(0x2048B0)); /* blue */
    PAL_setColor(0 * 16 + 10, RGB24_TO_VDPCOLOR(0xF8C820)); /* yellow */
    PAL_setColor(0 * 16 + 11, RGB24_TO_VDPCOLOR(0x101018)); /* black */
    PAL_setColor(0 * 16 + 12, RGB24_TO_VDPCOLOR(0x70C0E8)); /* sky blue */
    PAL_setColor(0 * 16 + 13, RGB24_TO_VDPCOLOR(0x189048)); /* green */
    PAL_setColor(0 * 16 + 14, RGB24_TO_VDPCOLOR(0xE87018)); /* orange */
}

void flag_data_init(void)
{
    VDP_loadTileData(tile_flag_panel, TILE_FLAG_PANEL, 1, DMA);
    VDP_loadTileData(tile_flag_box_h, TILE_FLAG_BOX_H, 1, DMA);
    VDP_loadTileData(tile_flag_box_l, TILE_FLAG_BOX_L, 1, DMA);
    VDP_loadTileData(tile_flag_box_r, TILE_FLAG_BOX_R, 1, DMA);
    VDP_loadTileData(flag_tiles[0], TILE_FLAGS, NUM_TEAMS * 2, DMA);
    VDP_loadTileData(flag_large_tiles[0], TILE_FLAGS_LARGE, NUM_TEAMS * 8, DMA);
    VDP_loadTileData(tile_flag_select, TILE_FLAG_SELECT, 1, DMA);
}

void flag_data_fill_panel(u16 x, u16 y, u16 w, u16 h)
{
    /* Lay the solid navy panel tile across a BG_B rectangle. Used behind UI
     * boxes so the font glyphs' transparent pixels reveal navy (matching the
     * box fill) instead of the court/crowd bleeding through. The panel tile
     * is index-4 navy, exactly the colour of the UI fill on PAL0. */
    u16 row, col;
    for (row = 0; row < h; row++)
        for (col = 0; col < w; col++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                TILE_FLAG_PANEL), x + col, y + row);
}

void flag_data_draw_small(u8 teamIndex, u16 x, u16 y, u8 palette)
{
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(palette, 1, FALSE, FALSE,
        TILE_FLAGS + teamIndex * 2), x, y);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(palette, 1, FALSE, FALSE,
        TILE_FLAGS + teamIndex * 2 + 1), x + 1, y);
}

void flag_data_draw_large(u8 teamIndex, u16 x, u16 y, u8 palette)
{
    u16 row, col;
    u16 base = TILE_FLAGS_LARGE + teamIndex * 8;
    for (col = 0; col < 4; col++)
        for (row = 0; row < 2; row++)
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(palette, 1, FALSE, FALSE,
                base + col * 2 + row), x + col, y + row);
}

void flag_data_draw_matchup(u8 teamAIndex, u8 teamBIndex)
{
    u16 row, col;
    u16 leftBase = TILE_FLAGS_LARGE + teamAIndex * 8;
    u16 rightBase = TILE_FLAGS_LARGE + teamBIndex * 8;
    apply_flag_palette();
    ui_set_palette(PAL0);
    ui_apply_palette();

    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                TILE_FLAG_PANEL), col, row);

    VDP_clearPlane(BG_A, TRUE);
    ui_draw_panel(1, 1, 38, 26, FALSE);
    ui_draw_big_center("MATCH UP", 2, UI_WHITE);

    /* Header row: each side's flag with its country name beneath, sitting
     * above the big player figures (drawn separately by matchup_art). */
    for (col = 0; col < 4; col++)
        for (row = 0; row < 2; row++)
        {
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
                leftBase + col * 2 + row), 4 + col, 5 + row);
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
                rightBase + col * 2 + row), 32 + col, 5 + row);
        }
    ui_draw_text(teamNames[teamAIndex], 6 - strlen(teamNames[teamAIndex]) / 2,
                 7, UI_GOLD);
    ui_draw_text(teamNames[teamBIndex], 34 - strlen(teamNames[teamBIndex]) / 2,
                 7, UI_GOLD);

    /* VS sits between the two figures, vertically centred on them. */
    ui_draw_big_text("VS", 18, 14, UI_GOLD);

    ui_draw_button("START MATCH", 13, 24, 14);
}


void flag_data_draw_selector(u8 selected, u8 playerNumber)
{
    /* Redesigned team picker (2026-07-22), layout guided by an outside AI
     * consult loosely after Virtua-Striker's cup select: bold title band,
     * a left flag+name list with the current row highlighted, a boxed big
     * flag + name on the right, and one clean control legend at the bottom.
     * Removed the old "WORLD TOP 10" tag, the "UP DOWN SELECT" line, and the
     * redundant PLAYER/CHOOSE double label - one "CHOOSE TEAM n" now. */
    u16 row, col;
    u8 i;
    u16 largeBase = TILE_FLAGS_LARGE + selected * 8;
    apply_flag_palette();
    ui_set_palette(PAL0);
    ui_apply_palette();

    /* Solid broadcast backing; a bright bar sits behind the selected row
     * (list column band cols 1-17 only). */
    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                (col >= 1 && col <= 17 && row == selected + 6)
                    ? TILE_FLAG_SELECT : TILE_FLAG_PANEL), col, row);

    VDP_clearPlane(BG_A, TRUE);

    /* Header band + big title, with a single right-aligned action label. */
    ui_draw_panel(0, 0, 40, 4, FALSE);
    ui_draw_big_text("TEAM SELECT", 2, 1, UI_WHITE);
    ui_draw_text(playerNumber == 1 ? "CHOOSE TEAM 1" : "CHOOSE TEAM 2",
                 25, 2, UI_GOLD);

    /* Vertical divider between the list and the preview panel. */
    for (row = 5; row <= 23; row++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
            TILE_FLAG_BOX_L), 19, row);

    /* Left: one flag + name per row, single pointer on the current row. */
    for (i = 0; i < NUM_TEAMS; i++)
    {
        u16 y = i + 6;
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
            TILE_FLAGS + i * 2), 2, y);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
            TILE_FLAGS + i * 2 + 1), 3, y);
        ui_draw_text(teamNames[i], 6, y, (i == selected) ? UI_GOLD : UI_WHITE);
        if (i == selected)
            ui_draw_text(">", 0, y, UI_GOLD);
    }

    /* Right preview panel (cols 20-38): boxed big flag, name centred below. */
    for (col = 26; col <= 31; col++)
    {
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_H), col, 7);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, TRUE,  FALSE, TILE_FLAG_BOX_H), col, 10);
    }
    for (row = 8; row <= 9; row++)
    {
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_L), 26, row);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_R), 31, row);
    }
    for (col = 0; col < 4; col++)
        for (row = 0; row < 2; row++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                largeBase + col * 2 + row), 27 + col, 8 + row);

    ui_draw_text(teamNames[selected],
                 29 - (u16)strlen(teamNames[selected]) / 2, 12, UI_GOLD);

    /* Bottom legend: divider rule + the two controls. */
    for (col = 1; col <= 38; col++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
            TILE_FLAG_BOX_H), col, 25);
    ui_draw_text("A CONFIRM", 9, 26, UI_GOLD);
    ui_draw_text("C CANCEL", 23, 26, UI_CYAN);
}

void flag_data_draw_grid(u8 selected)
{
    u8 i;
    u16 row, col;
    apply_flag_palette();

    /* Solid panel lets selection borders be clean and reversible without
     * leaving font-space artifacts over the isometric court. */
    for (row = 3; row <= 11; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_PANEL), col, row);

    for (i = 0; i < NUM_TEAMS; i++)
    {
        u16 x = 1 + (i % 5) * 8;
        u16 y = 4 + (i / 5) * 4;
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAGS + i * 2), x + 1, y + 1);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAGS + i * 2 + 1), x + 2, y + 1);
    }

    {
        u16 x = 1 + (selected % 5) * 8;
        u16 y = 4 + (selected / 5) * 4;
        for (col = x; col < x + 4; col++)
        {
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_H), col, y);
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, TRUE, FALSE, TILE_FLAG_BOX_H), col, y + 2);
        }
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_L), x, y + 1);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_R), x + 3, y + 1);
    }
}
