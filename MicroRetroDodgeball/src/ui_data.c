#include "ui_data.h"

#include "ui_tiles.inc"

#define TILE_UI_SMALL       TILE_UI_BASE
#define TILE_UI_BIG         (TILE_UI_SMALL + UI_SMALL_TILE_COUNT)
#define TILE_UI_FILL        (TILE_UI_BIG + UI_BIG_TILE_COUNT)
#define TILE_UI_H_CYAN      (TILE_UI_FILL + 1)
#define TILE_UI_H_GOLD      (TILE_UI_FILL + 2)
#define TILE_UI_V_CYAN      (TILE_UI_FILL + 3)
#define TILE_UI_V_GOLD      (TILE_UI_FILL + 4)
#define TILE_UI_CORNER_CYAN (TILE_UI_FILL + 5)
#define TILE_UI_CORNER_GOLD (TILE_UI_FILL + 6)
#define TILE_UI_BUTTON      (TILE_UI_FILL + 7)

static const u32 tile_ui_fill[8] = {
    0x44444444,0x44444444,0x44444444,0x44444444,
    0x44444444,0x44444444,0x44444444,0x44444444
};
static const u32 tile_ui_h_cyan[8] = {
    0xcccccccc,0x55555555,0x44444444,0x44444444,
    0x44444444,0x44444444,0x44444444,0x44444444
};
static const u32 tile_ui_h_gold[8] = {
    0x66666666,0x55555555,0x44444444,0x44444444,
    0x44444444,0x44444444,0x44444444,0x44444444
};
static const u32 tile_ui_v_cyan[8] = {
    0xc5444444,0xc5444444,0xc5444444,0xc5444444,
    0xc5444444,0xc5444444,0xc5444444,0xc5444444
};
static const u32 tile_ui_v_gold[8] = {
    0x65444444,0x65444444,0x65444444,0x65444444,
    0x65444444,0x65444444,0x65444444,0x65444444
};
static const u32 tile_ui_corner_cyan[8] = {
    0xcccccccc,0xc5555555,0xc5444444,0xc5444444,
    0xc5444444,0xc5444444,0xc5444444,0xc5444444
};
static const u32 tile_ui_corner_gold[8] = {
    0x66666666,0x65555555,0x65444444,0x65444444,
    0x65444444,0x65444444,0x65444444,0x65444444
};
static const u32 tile_ui_button[8] = {
    0x44444444,0x4cccccc4,0x4c5555c4,0x4c4444c4,
    0x4c4444c4,0x4c5555c4,0x4cccccc4,0x44444444
};

static u8 uiPalette = PAL0;
static bool spritePanelMaskEnabled;
static s16 spritePanelX, spritePanelY;
static u16 spritePanelW, spritePanelH;

void ui_set_sprite_panel_mask(bool enabled, s16 x, s16 y, u16 w, u16 h)
{
    spritePanelMaskEnabled = enabled;
    spritePanelX = x;
    spritePanelY = y;
    spritePanelW = w;
    spritePanelH = h;
}

static bool rectangles_overlap(s16 ax, s16 ay, u16 aw, u16 ah,
                               s16 bx, s16 by, u16 bw, u16 bh)
{
    return ax < bx + (s16)bw && ax + (s16)aw > bx &&
           ay < by + (s16)bh && ay + (s16)ah > by;
}

bool ui_sprite_behind_panel(s16 x, s16 y, u16 w, u16 h)
{
    /* Score/title strips occupy the top three tile rows in both gameplay
     * scenes. The optional rectangle is the live shot-clock panel. */
    if (rectangles_overlap(x, y, w, h, 0, 0, 320, 24)) return TRUE;
    return spritePanelMaskEnabled &&
           rectangles_overlap(x, y, w, h, spritePanelX, spritePanelY,
                              spritePanelW, spritePanelH);
}

static s16 glyph_index(char c)
{
    u16 i;
    for (i = 0; i < UI_GLYPH_COUNT; i++)
        if (ui_glyph_order[i] == c) return (s16)i;
    return -1;
}

void ui_data_init(void)
{
    spritePanelMaskEnabled = FALSE;
    /* The full authored alphabet is large but still fits VRAM. Upload it
     * synchronously at boot: queuing 660 tiles into the same startup DMA
     * burst as players/logo/flags can overflow the transfer budget before
     * the first VBlank and leave a blank display. */
    VDP_loadTileData(ui_small_tiles[0], TILE_UI_SMALL, UI_SMALL_TILE_COUNT, CPU);
    VDP_loadTileData(ui_big_tiles[0], TILE_UI_BIG, UI_BIG_TILE_COUNT, CPU);
    VDP_loadTileData(tile_ui_fill, TILE_UI_FILL, 1, CPU);
    VDP_loadTileData(tile_ui_h_cyan, TILE_UI_H_CYAN, 1, CPU);
    VDP_loadTileData(tile_ui_h_gold, TILE_UI_H_GOLD, 1, CPU);
    VDP_loadTileData(tile_ui_v_cyan, TILE_UI_V_CYAN, 1, CPU);
    VDP_loadTileData(tile_ui_v_gold, TILE_UI_V_GOLD, 1, CPU);
    VDP_loadTileData(tile_ui_corner_cyan, TILE_UI_CORNER_CYAN, 1, CPU);
    VDP_loadTileData(tile_ui_corner_gold, TILE_UI_CORNER_GOLD, 1, CPU);
    VDP_loadTileData(tile_ui_button, TILE_UI_BUTTON, 1, CPU);
}

void ui_set_palette(u8 palette)
{
    uiPalette = palette;
}

void ui_apply_palette(void)
{
    /* PAL0 slots shared consistently across title/select/court. */
    PAL_setColor(uiPalette * 16 + 4, RGB24_TO_VDPCOLOR(0x081830));
    PAL_setColor(uiPalette * 16 + 5, RGB24_TO_VDPCOLOR(0x405878));
    PAL_setColor(uiPalette * 16 + 6, RGB24_TO_VDPCOLOR(0xF0C028));
    PAL_setColor(uiPalette * 16 + 12, RGB24_TO_VDPCOLOR(0x58D8F0));
    PAL_setColor(uiPalette * 16 + 15, RGB24_TO_VDPCOLOR(0xF8F8F8));
}

void ui_draw_text(const char *text, u16 x, u16 y, u8 style)
{
    u16 i;
    for (i = 0; text[i] && x < 40; i++, x++)
    {
        s16 glyph;
        if (text[i] == ' ') continue;
        glyph = glyph_index(text[i]);
        if (glyph < 0) continue;
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(uiPalette, 1, FALSE, FALSE,
            TILE_UI_SMALL + style * UI_GLYPH_COUNT + (u16)glyph), x, y);
    }
}

void ui_draw_text_center(const char *text, u16 y, u8 style)
{
    u16 len = strlen(text);
    ui_draw_text(text, (40 - len) / 2, y, style);
}

void ui_draw_big_text(const char *text, u16 x, u16 y, u8 style)
{
    u16 i;
    u8 bigPalette = uiPalette;
    if (style == UI_GOLD)
    {
        bigPalette = PAL3;
        PAL_setColor(PAL3 * 16 + 15, RGB24_TO_VDPCOLOR(0xF0C028));
    }
    else if (style == UI_CYAN)
    {
        bigPalette = PAL3;
        PAL_setColor(PAL3 * 16 + 15, RGB24_TO_VDPCOLOR(0x58D8F0));
    }
    for (i = 0; text[i] && x < 39; i++)
    {
        s16 glyph;
        if (text[i] == ' ') { x++; continue; }
        glyph = glyph_index(text[i]);
        if (glyph >= 0)
        {
            u16 base = TILE_UI_BIG + (u16)glyph * 4;
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(bigPalette, 1, FALSE, FALSE, base + 0), x, y);
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(bigPalette, 1, FALSE, FALSE, base + 1), x, y + 1);
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(bigPalette, 1, FALSE, FALSE, base + 2), x + 1, y);
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(bigPalette, 1, FALSE, FALSE, base + 3), x + 1, y + 1);
        }
        x += 2;
    }
}

void ui_draw_big_center(const char *text, u16 y, u8 style)
{
    u16 i, width = 0;
    for (i = 0; text[i]; i++) width += (text[i] == ' ') ? 1 : 2;
    ui_draw_big_text(text, (40 - width) / 2, y, style);
}

void ui_draw_panel(u16 x, u16 y, u16 w, u16 h, bool gold)
{
    u16 row, col;
    u16 hTile = gold ? TILE_UI_H_GOLD : TILE_UI_H_CYAN;
    u16 vTile = gold ? TILE_UI_V_GOLD : TILE_UI_V_CYAN;
    u16 corner = gold ? TILE_UI_CORNER_GOLD : TILE_UI_CORNER_CYAN;
    for (row = 0; row < h; row++)
        for (col = 0; col < w; col++)
        {
            u16 tile = TILE_UI_FILL;
            bool hf = FALSE, vf = FALSE;
            /* Text and frame both live on BG_A. Without a matching solid tile
             * on BG_B, transparent glyph pixels reveal crowd/grass instead of
             * the panel, producing noisy, broken-looking lettering. Make
             * every panel self-contained so callers cannot forget its backer. */
            VDP_setTileMapXY(BG_B,
                TILE_ATTR_FULL(uiPalette, 1, FALSE, FALSE, TILE_UI_FILL),
                x + col, y + row);
            if ((row == 0 || row == h - 1) && (col == 0 || col == w - 1))
            {
                tile = corner; hf = (col == w - 1); vf = (row == h - 1);
            }
            else if (row == 0 || row == h - 1)
            {
                tile = hTile; vf = (row == h - 1);
            }
            else if (col == 0 || col == w - 1)
            {
                tile = vTile; hf = (col == w - 1);
            }
            /* UI panels are foreground surfaces. Their opaque fill and frame
             * must mask gameplay sprites, including the permanent HUD strips
             * at the top of Match and Eliminator scenes. */
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(uiPalette, 1, vf, hf, tile), x + col, y + row);
    }
}

void ui_clear_panel_interior(u16 x, u16 y, u16 w, u16 h)
{
    u16 row, col;

    if (w < 3 || h < 3) return;
    for (row = 1; row < h - 1; row++)
        for (col = 1; col < w - 1; col++)
            VDP_setTileMapXY(BG_A,
                TILE_ATTR_FULL(uiPalette, 1, FALSE, FALSE, TILE_UI_FILL),
                x + col, y + row);
}

void ui_draw_hrule(u16 x, u16 y, u16 w)
{
    u16 i;
    for (i = 0; i < w; i++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(uiPalette, 0, FALSE, FALSE,
                         TILE_UI_H_CYAN), (u16)(x + i), y);
}

void ui_draw_bracket(u16 x, u16 yTop, u16 yBot, bool mirrored)
{
    /* A tournament-bracket join: a vertical run down column x linking the two
     * paired rows, closed with corners, and a horizontal stub leaving the
     * midpoint toward the next round. Built from the existing panel line
     * tiles, whose navy fill matches the screen backdrop. "mirrored" flips it
     * for the right-hand half of a centre-converging bracket, so its lines run
     * down the left edge and the stub exits leftward. */
    bool hf = !mirrored;   /* vertical rule on the right for the left half */
    u16 y;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(uiPalette, 0, FALSE, hf,
                     TILE_UI_CORNER_CYAN), x, yTop);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(uiPalette, 0, TRUE, hf,
                     TILE_UI_CORNER_CYAN), x, yBot);
    for (y = (u16)(yTop + 1); y < yBot; y++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(uiPalette, 0, FALSE, hf,
                         TILE_UI_V_CYAN), x, y);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(uiPalette, 0, FALSE, FALSE,
                     TILE_UI_H_CYAN),
                     mirrored ? (u16)(x - 1) : (u16)(x + 1),
                     (u16)((yTop + yBot) >> 1));
}

void ui_draw_button(const char *label, u16 x, u16 y, u16 w)
{
    ui_draw_panel(x, y - 1, w, 3, TRUE);
    ui_draw_text(label, x + (w - strlen(label)) / 2, y, UI_GOLD);
}
