#include "title_data.h"

#include "title_tiles.inc"

#define TILE_TITLE_BG       (TILE_TITLE_BASE + TITLE_ART_TILE_COUNT)
#define TILE_TITLE_LIGHT_A  (TILE_TITLE_BG + 1)
#define TILE_TITLE_LIGHT_B  (TILE_TITLE_BG + 2)
#define TILE_TITLE_RAIL     (TILE_TITLE_BG + 3)

static const u32 tile_title_bg[8] = {
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};
static const u32 tile_title_light_a[8] = {
    0x11111111, 0x11171111, 0x11117111, 0x11111711,
    0x11111171, 0x11111117, 0x11111111, 0x11111111
};
static const u32 tile_title_light_b[8] = {
    0x11111111, 0x11111111, 0x71111111, 0x17111111,
    0x11711111, 0x11171111, 0x11117111, 0x11111111
};
static const u32 tile_title_rail[8] = {
    0x22222222, 0x77777777, 0x33333333, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};

static const u16 title_palette[16] = {
    RGB24_TO_VDPCOLOR(0x040818), RGB24_TO_VDPCOLOR(0x081030),
    RGB24_TO_VDPCOLOR(0x143068), RGB24_TO_VDPCOLOR(0xF8F0D0),
    RGB24_TO_VDPCOLOR(0xF8D828), RGB24_TO_VDPCOLOR(0xF07018),
    RGB24_TO_VDPCOLOR(0xD02028), RGB24_TO_VDPCOLOR(0x40B8E8),
    0, 0, 0, 0, 0, 0, 0,
    RGB24_TO_VDPCOLOR(0xF8F8F8)
};

static s16 glyph_index(char c)
{
    u16 i;
    for (i = 0; i < TITLE_GLYPH_COUNT; i++)
        if (title_glyph_order[i] == c) return (s16)i;
    return -1;
}

static void draw_word(const char *word, u16 y)
{
    u16 len = strlen(word);
    u16 width = len * 2 + (len - 1);
    u16 x = (40 - width) / 2;
    u16 i;
    for (i = 0; i < len; i++, x += 3)
    {
        s16 glyph = glyph_index(word[i]);
        u16 base;
        if (glyph < 0) continue;
        base = TILE_TITLE_BASE + (u16)glyph * 4;
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, base + 0), x, y);
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, base + 1), x, y + 1);
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, base + 2), x + 1, y);
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, base + 3), x + 1, y + 1);
    }
}

void title_data_init(void)
{
    VDP_loadTileData(title_art_tiles[0], TILE_TITLE_BASE, TITLE_ART_TILE_COUNT, DMA);
    VDP_loadTileData(tile_title_bg, TILE_TITLE_BG, 1, DMA);
    VDP_loadTileData(tile_title_light_a, TILE_TITLE_LIGHT_A, 1, DMA);
    VDP_loadTileData(tile_title_light_b, TILE_TITLE_LIGHT_B, 1, DMA);
    VDP_loadTileData(tile_title_rail, TILE_TITLE_RAIL, 1, DMA);
}

void title_data_draw(void)
{
    u16 row, col;
    u16 ballBase = TILE_TITLE_BASE + TITLE_GLYPH_COUNT * 4;
    PAL_setPalette(PAL0, title_palette, DMA);

    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(VDP_BG_B,
                TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                    ((row + col) % 17 == 0) ? TILE_TITLE_LIGHT_A :
                    ((row * 3 + col) % 23 == 0) ? TILE_TITLE_LIGHT_B : TILE_TITLE_BG),
                col, row);

    for (col = 0; col < 40; col++)
        VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_TITLE_RAIL), col, 23);

    draw_word("MICRO", 3);
    draw_word("RETRO", 6);
    draw_word("DODGEBALL", 10);

    /* 32x32 football tucked under the logo, echoing the reference's
     * large emblem without copying its branding. */
    for (col = 0; col < 4; col++)
        for (row = 0; row < 4; row++)
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
                ballBase + col * 4 + row), 31 + col, 14 + row);

    VDP_drawText("WORLD CHAMPIONSHIP", 10, 14);
    VDP_drawText("10 NATIONAL TEAMS", 11, 17);
    VDP_drawText("PRESS START", 14, 21);
    VDP_drawText("MINNKA GAMES  2026", 10, 25);
}
