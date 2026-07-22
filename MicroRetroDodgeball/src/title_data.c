#include "title_data.h"

#include "title_tiles.inc"

#define TILE_TITLE_PROMPT (TILE_TITLE_BASE + TITLE_ART_TILE_COUNT)

static s16 prompt_index(char c)
{
    u16 i;
    for (i = 0; i < TITLE_PROMPT_TILE_COUNT; i++)
        if (title_prompt_order[i] == c) return (s16)i;
    return -1;
}

void title_data_init(void)
{
    /* The illustration shares VRAM with the large UI font. Loading its
     * clustered scene-local bank at title entry keeps the detailed source
     * art clear without crossing the VDP plane-map region. */
}

void title_data_set_prompt(bool visible)
{
    static const char text[] = "> PRESS START <";
    u16 i;
    VDP_clearTileMapRect(BG_A, 12, 25, 16, 1);
    if (!visible) return;

    for (i = 0; text[i]; i++)
    {
        s16 glyph;
        if (text[i] == ' ') continue;
        glyph = prompt_index(text[i]);
        if (glyph >= 0)
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
                TILE_TITLE_PROMPT + glyph), 13 + i, 25);
    }
}

void title_data_draw(void)
{
    u16 row, col;
    PAL_setPalette(PAL0, title_art_palette, CPU);
    VDP_loadTileData(title_art_tiles[0], TILE_TITLE_BASE,
                     TITLE_ART_TILE_COUNT, CPU);
    VDP_loadTileData(title_prompt_tiles[0], TILE_TITLE_PROMPT,
                     TITLE_PROMPT_TILE_COUNT, CPU);

    VDP_clearPlane(BG_A, TRUE);
    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(BG_B,
                TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                    TILE_TITLE_BASE + title_art_tilemap[row][col]),
                col, row);
    title_data_set_prompt(TRUE);
}
