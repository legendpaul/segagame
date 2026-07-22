#include "court_bg.h"
#include "sprites_data.h"

#include "stadium_tiles.inc"

static void restore_colors(void)
{
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(1,  RGB24_TO_VDPCOLOR(0xF8F8F0));
    PAL_setColor(2,  RGB24_TO_VDPCOLOR(0x42B850));
    PAL_setColor(3,  RGB24_TO_VDPCOLOR(0x27843C));
    PAL_setColor(4,  RGB24_TO_VDPCOLOR(0x081830));
    PAL_setColor(5,  RGB24_TO_VDPCOLOR(0x405878));
    PAL_setColor(6,  RGB24_TO_VDPCOLOR(0xF0C028));
    PAL_setColor(7,  RGB24_TO_VDPCOLOR(0x35A048));
    PAL_setColor(8,  RGB24_TO_VDPCOLOR(0x101828));
    PAL_setColor(9,  RGB24_TO_VDPCOLOR(0xD83838));
    PAL_setColor(10, RGB24_TO_VDPCOLOR(0xF0C838));
    PAL_setColor(11, RGB24_TO_VDPCOLOR(0x3870C8));
    PAL_setColor(12, RGB24_TO_VDPCOLOR(0x58D8F0));
    PAL_setColor(13, RGB24_TO_VDPCOLOR(0x788898));
    PAL_setColor(14, RGB24_TO_VDPCOLOR(0x283038));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xF8F8F8));
}

void court_bg_init(void)
{
    VDP_loadTileData(stadium_tiles[0], TILE_COURT_BASE, STADIUM_TILE_COUNT, CPU);
    restore_colors();
}

void court_bg_draw(void)
{
    u16 row, col;
    /* Restore the scene-local tail that deliberately overlaps unused boot
     * logo tiles in VRAM. */
    VDP_loadTileData(stadium_tiles[0], TILE_COURT_BASE, STADIUM_TILE_COUNT, CPU);
    restore_colors();
    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(BG_B,
                TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                    TILE_COURT_BASE + stadium_tilemap[row][col]),
                col, row);
}

void court_bg_drawForeground(void)
{
    u16 row, col;
    VDP_loadTileData(stadium_foreground_tiles[0], TILE_COURT_FG_BASE,
                     STADIUM_FOREGROUND_TILE_COUNT, CPU);
    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
        {
            u16 tile = stadium_foreground_tilemap[row][col];
            if (tile)
                VDP_setTileMapXY(BG_A,
                    TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
                        TILE_COURT_FG_BASE + tile), col, row);
        }
}
