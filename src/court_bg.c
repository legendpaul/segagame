#include "court_bg.h"

/* Reuses PAL0 (the font palette) for background art: font glyphs only
 * use index 0 (transparent) and 1 (white), so grass/line/stand colors
 * ride in the unused index 2-4 slots of the same palette line - no
 * extra palette budget needed on top of the 3 sprite lines. */
#define TILE_GRASS_A   (TILE_USER_INDEX + 5)
#define TILE_GRASS_B   (TILE_USER_INDEX + 6)
#define TILE_LINE_L    (TILE_USER_INDEX + 7)
#define TILE_LINE_R    (TILE_USER_INDEX + 8)
#define TILE_LINE_H    (TILE_USER_INDEX + 9)
#define TILE_STAND     (TILE_USER_INDEX + 10)

#define PITCH_TOP_ROW      3
#define PITCH_BOTTOM_ROW   24
#define HALFWAY_ROW        13

static const u32 tile_grass_a[8] = {
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};

static const u32 tile_grass_b[8] = {
    0x33333333, 0x33333333, 0x33333333, 0x33333333,
    0x33333333, 0x33333333, 0x33333333, 0x33333333
};

static const u32 tile_line_l[8] = {
    0x22222221, 0x22222221, 0x22222221, 0x22222221,
    0x22222221, 0x22222221, 0x22222221, 0x22222221
};

static const u32 tile_line_r[8] = {
    0x12222222, 0x12222222, 0x12222222, 0x12222222,
    0x12222222, 0x12222222, 0x12222222, 0x12222222
};

static const u32 tile_line_h[8] = {
    0x11111111, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};

static const u32 tile_stand[8] = {
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444
};

void court_bg_init(void)
{
    VDP_loadTileData(tile_grass_a, TILE_GRASS_A, 1, DMA);
    VDP_loadTileData(tile_grass_b, TILE_GRASS_B, 1, DMA);
    VDP_loadTileData(tile_line_l,  TILE_LINE_L,  1, DMA);
    VDP_loadTileData(tile_line_r,  TILE_LINE_R,  1, DMA);
    VDP_loadTileData(tile_line_h,  TILE_LINE_H,  1, DMA);
    VDP_loadTileData(tile_stand,   TILE_STAND,   1, DMA);

    /* Light green / dark green / dark navy stand, added into the font's
     * PAL0 line at the unused index 2-4 slots. Index 0/1 (transparent
     * black / white) are left alone so text keeps rendering correctly. */
    PAL_setColor(0 * 16 + 2, RGB24_TO_VDPCOLOR(0x40B050)); /* grass light */
    PAL_setColor(0 * 16 + 3, RGB24_TO_VDPCOLOR(0x309040)); /* grass dark  */
    PAL_setColor(0 * 16 + 4, RGB24_TO_VDPCOLOR(0x182848)); /* stand navy  */
}

void court_bg_draw(void)
{
    u16 row, col;

    for (row = 0; row < 28; row++)
    {
        if (row < PITCH_TOP_ROW || row > PITCH_BOTTOM_ROW)
        {
            for (col = 0; col < 40; col++)
                VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_STAND), col, row);
            continue;
        }

        u16 dt = row - PITCH_TOP_ROW;
        u16 grassTile = (dt & 2) ? TILE_GRASS_B : TILE_GRASS_A;
        u16 leftCol  = 6 - (dt * 4) / (PITCH_BOTTOM_ROW - PITCH_TOP_ROW);
        u16 rightCol = 33 + (dt * 4) / (PITCH_BOTTOM_ROW - PITCH_TOP_ROW);

        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, grassTile), col, row);

        if (row == HALFWAY_ROW)
        {
            for (col = leftCol; col <= rightCol; col++)
                VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_LINE_H), col, row);
        }
        else
        {
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_LINE_L), leftCol, row);
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_LINE_R), rightCol, row);
        }
    }
}
