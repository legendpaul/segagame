#include "flag_data.h"
#include "teams.h"

#define TILE_FLAG_PANEL  (TILE_FLAG_BASE + 0)
#define TILE_FLAG_BOX_H  (TILE_FLAG_BASE + 1)
#define TILE_FLAG_BOX_L  (TILE_FLAG_BASE + 2)
#define TILE_FLAG_BOX_R  (TILE_FLAG_BASE + 3)
#define TILE_FLAGS       (TILE_FLAG_BASE + 4)

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

static void apply_flag_palette(void)
{
    /* PAL0 0-7/15 belong to court/font. Flags use the remaining seven
     * colors, restored each time because scene fades blacken all lines. */
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
