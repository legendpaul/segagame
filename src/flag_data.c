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


void flag_data_draw_selector(u8 selected, u8 playerNumber)
{
    u16 row, col;
    u8 i;
    u16 largeBase = TILE_FLAGS_LARGE + selected * 8;
    apply_flag_palette();
    ui_set_palette(PAL0);
    ui_apply_palette();

    /* Dark broadcast-style backing with a bright selected row. */
    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                (row >= 5 && row <= 16 && col < 23 && row == selected + 6)
                    ? TILE_FLAG_SELECT : TILE_FLAG_PANEL), col, row);

    VDP_clearPlane(VDP_BG_A, TRUE);
    ui_draw_panel(0, 0, 40, 4, FALSE);
    ui_draw_big_text("SELECT TEAM", 1, 1, UI_WHITE);
    ui_draw_text(playerNumber == 1 ? "PLAYER 1" : "PLAYER 2", 30, 1, UI_GOLD);
    ui_draw_text("WORLD TOP 10", 28, 2, UI_CYAN);

    for (i = 0; i < NUM_TEAMS; i++)
    {
        u16 y = i + 6;
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
            TILE_FLAGS + i * 2), 2, y);
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
            TILE_FLAGS + i * 2 + 1), 3, y);
        ui_draw_text(teamNames[i], 6, y, (i == selected) ? UI_GOLD : UI_WHITE);
        if (i == selected)
        {
            ui_draw_text(">", 0, y, UI_GOLD);
            ui_draw_text("<", 20, y, UI_GOLD);
        }
    }

    /* Large, nearest-neighbour flag and a proper white box on the right. */
    for (col = 25; col <= 30; col++)
    {
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_H), col, 6);
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, TRUE, FALSE, TILE_FLAG_BOX_H), col, 9);
    }
    for (row = 7; row <= 8; row++)
    {
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_L), 25, row);
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_R), 30, row);
    }
    for (col = 0; col < 4; col++)
        for (row = 0; row < 2; row++)
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                largeBase + col * 2 + row), 26 + col, 7 + row);

    ui_draw_text(teamNames[selected], 25, 11, UI_GOLD);
    ui_draw_text("UP DOWN SELECT", 24, 20, UI_CYAN);
    ui_draw_button("A CONFIRM", 24, 22, 14);
    ui_draw_text(playerNumber == 1 ? "CHOOSE TEAM 1" : "CHOOSE TEAM 2", 24, 25, UI_WHITE);
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
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_PANEL), col, row);

    for (i = 0; i < NUM_TEAMS; i++)
    {
        u16 x = 1 + (i % 5) * 8;
        u16 y = 4 + (i / 5) * 4;
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAGS + i * 2), x + 1, y + 1);
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAGS + i * 2 + 1), x + 2, y + 1);
    }

    {
        u16 x = 1 + (selected % 5) * 8;
        u16 y = 4 + (selected / 5) * 4;
        for (col = x; col < x + 4; col++)
        {
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_H), col, y);
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, TRUE, FALSE, TILE_FLAG_BOX_H), col, y + 2);
        }
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_L), x, y + 1);
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_R), x + 3, y + 1);
    }
}
