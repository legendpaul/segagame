#include "sprites_data.h"

/* 8x8, 4bpp, one u32 per row (8 pixels x 4 bits, MSB = leftmost pixel).
 * Color index 0 = transparent, index 1 = the team/ball color. */

static const u32 tile_player_top[8] = {
    0x00011000,
    0x00111100,
    0x00111100,
    0x00011000,
    0x01111110,
    0x11111111,
    0x11111111,
    0x11111111
};

static const u32 tile_player_bottom[8] = {
    0x11111111,
    0x11111111,
    0x01111110,
    0x01111110,
    0x01100110,
    0x01100110,
    0x11000011,
    0x11000011
};

static const u32 tile_ball[8] = {
    0x00111100,
    0x01111110,
    0x11111111,
    0x11111111,
    0x11111111,
    0x11111111,
    0x01111110,
    0x00111100
};

static const u16 pal_team_a[16] = {
    0x0000, RGB24_TO_VDPCOLOR(0xF02020), 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 pal_team_b[16] = {
    0x0000, RGB24_TO_VDPCOLOR(0x3070F8), 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 pal_ball[16] = {
    0x0000, RGB24_TO_VDPCOLOR(0xF8F830), 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

void sprites_data_init(void)
{
    VDP_loadTileData(tile_player_top, TILE_PLAYER_TOP, 1, DMA);
    VDP_loadTileData(tile_player_bottom, TILE_PLAYER_BOTTOM, 1, DMA);
    VDP_loadTileData(tile_ball, TILE_BALL, 1, DMA);

    PAL_setPalette(PAL_TEAM_A, pal_team_a, DMA);
    PAL_setPalette(PAL_TEAM_B, pal_team_b, DMA);
    PAL_setPalette(PAL_BALL, pal_ball, DMA);
}
