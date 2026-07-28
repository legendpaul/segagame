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
    PAL_setColor(9,  RGB24_TO_VDPCOLOR(0xB83840));  /* crowd red, rich not neon */
    PAL_setColor(10, RGB24_TO_VDPCOLOR(0xD8A840));  /* warm stadium gold */
    PAL_setColor(11, RGB24_TO_VDPCOLOR(0x345890));  /* restrained crowd blue */
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

static void draw_tilemap(const u16 tilemap[28][40])
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
                    TILE_COURT_BASE + tilemap[row][col]),
                col, row);
}

static void restore_eliminator_colors(void)
{
    /* Title-screen atmosphere on the open arena: midnight blue floor ramps,
     * cyan floodlight edges and hot orange championship accents. */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x000008));
    PAL_setColor(1,  RGB24_TO_VDPCOLOR(0xE8F8F8));
    PAL_setColor(2,  RGB24_TO_VDPCOLOR(0x102858));
    PAL_setColor(3,  RGB24_TO_VDPCOLOR(0x050C24));
    PAL_setColor(4,  RGB24_TO_VDPCOLOR(0x040820));
    PAL_setColor(5,  RGB24_TO_VDPCOLOR(0x102848));
    PAL_setColor(6,  RGB24_TO_VDPCOLOR(0xF09818));
    PAL_setColor(7,  RGB24_TO_VDPCOLOR(0x0A1840));
    PAL_setColor(8,  RGB24_TO_VDPCOLOR(0x020318));
    PAL_setColor(9,  RGB24_TO_VDPCOLOR(0xD84018));
    PAL_setColor(10, RGB24_TO_VDPCOLOR(0xF0B030));
    PAL_setColor(11, RGB24_TO_VDPCOLOR(0x183888));
    PAL_setColor(12, RGB24_TO_VDPCOLOR(0x70D8F0));
    PAL_setColor(13, RGB24_TO_VDPCOLOR(0x607898));
    PAL_setColor(14, RGB24_TO_VDPCOLOR(0x101830));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xF8FFFF));
}

void court_bg_draw(void)
{
    draw_tilemap(stadium_tilemap);
}

void court_bg_drawEliminator(void)
{
    draw_tilemap(eliminator_stadium_tilemap);
    restore_eliminator_colors();
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

static void draw_foreground(const u16 tilemap[28][40])
{
    u16 row, col;
    VDP_loadTileData(stadium_foreground_tiles[0], TILE_COURT_FG_BASE,
                     STADIUM_FOREGROUND_TILE_COUNT, CPU);
    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
        {
            u16 tile = tilemap[row][col];
            if (tile)
                VDP_setTileMapXY(BG_A,
                    TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
                        TILE_COURT_FG_BASE + tile), col, row);
        }
}

void court_bg_drawForeground(void)
{
    draw_foreground(stadium_foreground_tilemap);
}

void court_bg_drawEliminatorForeground(void)
{
    draw_foreground(eliminator_stadium_foreground_tilemap);
}

bool court_bg_spriteBehindRoof(s16 x, s16 y, u16 w, u16 h)
{
    /* Conservative rectangle-vs-canopy test matching the generated geometry.
     * Lowering the complete sprite is safe because transparent BG_A pixels do
     * not cover it; only the authored roof pixels win the priority contest. */
    s16 right = x + (s16)w - 1;
    s16 bottom = y + (s16)h - 1;
    s16 nearEdge = 163 + (x >> 2);

    if (bottom >= nearEdge) return TRUE;
    /* Right canopy inner edge: y ~= 684 - 1.75x. */
    return ((s32)7 * right + (s32)4 * bottom) >= 2736;
}
