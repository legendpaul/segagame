#include "court_bg.h"
#include "sprites_data.h"
#include "game_state.h"

/* Fifteen compact reusable tiles create the whole venue. The court remains
 * cheap enough for the Mega Drive, but now sits inside stands, crowd, an
 * advertising wall and a dark perimeter instead of floating in endless turf. */
#define TILE_GRASS_A    (TILE_COURT_BASE + 0)
#define TILE_GRASS_B    (TILE_COURT_BASE + 1)
#define TILE_GRASS_M    (TILE_COURT_BASE + 2)
#define TILE_EDGE_L     (TILE_COURT_BASE + 3)
#define TILE_EDGE_R     (TILE_COURT_BASE + 4)
#define TILE_ISO_0      (TILE_COURT_BASE + 5)
#define TILE_ISO_2      (TILE_COURT_BASE + 6)
#define TILE_ISO_4      (TILE_COURT_BASE + 7)
#define TILE_ISO_6      (TILE_COURT_BASE + 8)
#define TILE_HUD        (TILE_COURT_BASE + 9)
#define TILE_CROWD_A    (TILE_COURT_BASE + 10)
#define TILE_CROWD_B    (TILE_COURT_BASE + 11)
#define TILE_WALL       (TILE_COURT_BASE + 12)
#define TILE_TRACK      (TILE_COURT_BASE + 13)
#define TILE_LIGHTS     (TILE_COURT_BASE + 14)

static const u32 tile_grass_a[8] = {
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};
static const u32 tile_grass_b[8] = {
    0x33333333, 0x33333333, 0x33333333, 0x33333333,
    0x33333333, 0x33333333, 0x33333333, 0x33333333
};
static const u32 tile_grass_m[8] = {
    0x77777777, 0x77777777, 0x77777777, 0x77777777,
    0x77777777, 0x77777777, 0x77777777, 0x77777777
};
static const u32 tile_edge_l[8] = {
    0x11133333, 0x11133333, 0x11133333, 0x11133333,
    0x11133333, 0x11133333, 0x11133333, 0x11133333
};
static const u32 tile_edge_r[8] = {
    0x33333111, 0x33333111, 0x33333111, 0x33333111,
    0x33333111, 0x33333111, 0x33333111, 0x33333111
};

/* Four sub-tile phases of y=x/4. This is the actual white 1px projected
 * court line; the previous build accidentally reused crowd-pattern tiles
 * for three of these phases, which made the geometry look broken. */
static const u32 tile_iso_0[8] = {
    0x11112222, 0x22221111, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};
static const u32 tile_iso_2[8] = {
    0x22222222, 0x22222222, 0x11112222, 0x22221111,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};
static const u32 tile_iso_4[8] = {
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x11112222, 0x22221111, 0x22222222, 0x22222222
};
static const u32 tile_iso_6[8] = {
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x11112222, 0x22221111
};
static const u32 tile_hud[8] = {
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444
};
static const u32 tile_crowd_a[8] = {
    0x898b8a89, 0x88888888, 0x8a889888, 0x888b8888,
    0x898888a8, 0x88888888, 0x8b888998, 0x888a8888
};
static const u32 tile_crowd_b[8] = {
    0x888c8888, 0x9a888b88, 0x88888888, 0x88b88898,
    0x888a8888, 0x898888c8, 0x88888888, 0xa888b888
};
static const u32 tile_wall[8] = {
    0xdddddddd, 0xd666666d, 0xd666666d, 0xdddddddd,
    0x44444444, 0x44444444, 0xeeeeeeee, 0x44444444
};
static const u32 tile_track[8] = {
    0xeeeeeeee, 0xeeeeeeee, 0xeeeeeeee, 0xeeeeeeee,
    0xeeeeeeee, 0xeeeeeeee, 0xeeeeeeee, 0xeeeeeeee
};
static const u32 tile_lights[8] = {
    0x44444444, 0x4f4f4f44, 0x44444444, 0x44444444,
    0x4f4f4f44, 0x44444444, 0x44444444, 0x44444444
};

static void restore_colors(void)
{
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(1,  RGB24_TO_VDPCOLOR(0xF8F8F0));
    PAL_setColor(2,  RGB24_TO_VDPCOLOR(0x42B850));
    PAL_setColor(3,  RGB24_TO_VDPCOLOR(0x27843C));
    PAL_setColor(4,  RGB24_TO_VDPCOLOR(0x081830));
    PAL_setColor(5,  RGB24_TO_VDPCOLOR(0x304868));
    PAL_setColor(6,  RGB24_TO_VDPCOLOR(0xE8B820));
    PAL_setColor(7,  RGB24_TO_VDPCOLOR(0x35A048));
    PAL_setColor(8,  RGB24_TO_VDPCOLOR(0x101828));
    PAL_setColor(9,  RGB24_TO_VDPCOLOR(0xD83838));
    PAL_setColor(10, RGB24_TO_VDPCOLOR(0xF0C838));
    PAL_setColor(11, RGB24_TO_VDPCOLOR(0x3870C8));
    PAL_setColor(12, RGB24_TO_VDPCOLOR(0x58C0D8));
    PAL_setColor(13, RGB24_TO_VDPCOLOR(0x788898));
    PAL_setColor(14, RGB24_TO_VDPCOLOR(0x283038));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xF8F8F8));
}

void court_bg_init(void)
{
    VDP_loadTileData(tile_grass_a, TILE_GRASS_A, 1, DMA);
    VDP_loadTileData(tile_grass_b, TILE_GRASS_B, 1, DMA);
    VDP_loadTileData(tile_grass_m, TILE_GRASS_M, 1, DMA);
    VDP_loadTileData(tile_edge_l, TILE_EDGE_L, 1, DMA);
    VDP_loadTileData(tile_edge_r, TILE_EDGE_R, 1, DMA);
    VDP_loadTileData(tile_iso_0, TILE_ISO_0, 1, DMA);
    VDP_loadTileData(tile_iso_2, TILE_ISO_2, 1, DMA);
    VDP_loadTileData(tile_iso_4, TILE_ISO_4, 1, DMA);
    VDP_loadTileData(tile_iso_6, TILE_ISO_6, 1, DMA);
    VDP_loadTileData(tile_hud, TILE_HUD, 1, DMA);
    VDP_loadTileData(tile_crowd_a, TILE_CROWD_A, 1, DMA);
    VDP_loadTileData(tile_crowd_b, TILE_CROWD_B, 1, DMA);
    VDP_loadTileData(tile_wall, TILE_WALL, 1, DMA);
    VDP_loadTileData(tile_track, TILE_TRACK, 1, DMA);
    VDP_loadTileData(tile_lights, TILE_LIGHTS, 1, DMA);
    restore_colors();
}

static void draw_iso_line(s16 intercept)
{
    u16 col;
    for (col = 0; col < 40; col++)
    {
        s16 y = (s16)(col * 2) + intercept;
        u16 tile;
        if (y < 56 || y >= 224) continue;
        switch (y & 7)
        {
            case 0: tile = TILE_ISO_0; break;
            case 2: tile = TILE_ISO_2; break;
            case 4: tile = TILE_ISO_4; break;
            default: tile = TILE_ISO_6; break;
        }
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, tile), col, (u16)y >> 3);
    }
}

void court_bg_draw(void)
{
    u16 row, col;
    restore_colors();

    /* Clean broadcast framing: compact grandstand across the far edge,
     * one advertising/lighting rail, then uninterrupted full-width turf.
     * The previous side crowd columns chopped the pitch into a jagged
     * green island and looked more like walls than a stadium. */
    for (row = 0; row < 28; row++)
    {
        for (col = 0; col < 40; col++)
        {
            u16 tile;
            if (row < 2)
                tile = TILE_HUD;
            else if (row < 5)
                tile = ((row + col) & 1) ? TILE_CROWD_A : TILE_CROWD_B;
            else if (row == 5)
                tile = (col % 7 == 0) ? TILE_LIGHTS : TILE_WALL;
            else if (row == 6)
                tile = TILE_TRACK;
            else
            {
                s16 depth = (s16)(row * 8 + 4) - (s16)(col * 2 + 1);
                u16 band = (u16)((depth + 128) / 24) % 3;
                tile = (band == 0) ? TILE_GRASS_M : (band == 1) ? TILE_GRASS_A : TILE_GRASS_B;
            }

            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, tile), col, row);
        }
    }

    draw_iso_line(COURT_FAR_DEPTH);
    draw_iso_line(COURT_CENTER_DEPTH);
    draw_iso_line(COURT_NEAR_DEPTH);
}
