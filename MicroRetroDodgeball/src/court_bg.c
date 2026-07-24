#include "court_bg.h"
#include "sprites_data.h"

#include "stadium_tiles.inc"

static void restore_colors(void)
{
    /* Court palette pass (2026-07-22). Same 16 slots, same roles - only the
     * colour values are retuned, so no tile data changes and nothing can
     * shift index-wise. Goals: a richer pitch with more mowing-stripe
     * contrast (greens 2/3/7), a deeper stand backdrop so the crowd reads
     * with depth instead of floating (8/14), and slightly less neon,
     * more broadcast-realistic crowd dots (9/10/11). Lines stay pure white
     * (1/15) for crispness. */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x000000));  /* outline / backdrop black */
    PAL_setColor(1,  RGB24_TO_VDPCOLOR(0xF8F8F0));  /* line off-white */
    PAL_setColor(2,  RGB24_TO_VDPCOLOR(0x54C862));  /* grass - light stripe (brighter, cleaner) */
    PAL_setColor(3,  RGB24_TO_VDPCOLOR(0x1E7434));  /* grass - dark stripe (deeper, richer) */
    PAL_setColor(4,  RGB24_TO_VDPCOLOR(0x081830));  /* deep navy (HUD/shadow) */
    PAL_setColor(5,  RGB24_TO_VDPCOLOR(0x405878));  /* slate */
    PAL_setColor(6,  RGB24_TO_VDPCOLOR(0xF0C028));  /* gold accent */
    PAL_setColor(7,  RGB24_TO_VDPCOLOR(0x3AA850));  /* grass - mid stripe */
    PAL_setColor(8,  RGB24_TO_VDPCOLOR(0x0C1220));  /* stand backdrop (darker for crowd depth) */
    PAL_setColor(9,  RGB24_TO_VDPCOLOR(0xC8443C));  /* crowd red (less neon) */
    PAL_setColor(10, RGB24_TO_VDPCOLOR(0xE0BC50));  /* crowd gold (warmer, less neon) */
    PAL_setColor(11, RGB24_TO_VDPCOLOR(0x3C6CB8));  /* crowd blue */
    PAL_setColor(12, RGB24_TO_VDPCOLOR(0x58D8F0));  /* cyan accent */
    PAL_setColor(13, RGB24_TO_VDPCOLOR(0x788898));  /* grey-blue structure */
    PAL_setColor(14, RGB24_TO_VDPCOLOR(0x1E2630));  /* dark structure (slightly deeper) */
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xF8F8F8));  /* pure white */
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

void court_bg_redraw_rect(u16 x, u16 y, u16 w, u16 h)
{
    /* Restore the BG_B court tiles for a sub-rectangle - used to clean up
     * after a transient overlay (e.g. the shot-clock box) is removed. */
    u16 row, col;
    for (row = y; row < y + h && row < 28; row++)
        for (col = x; col < x + w && col < 40; col++)
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
