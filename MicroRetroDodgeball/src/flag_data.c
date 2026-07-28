#include "flag_data.h"
#include "teams.h"
#include "ui_data.h"

#define TILE_FLAG_PANEL  (TILE_FLAG_BASE + 0)
#define TILE_FLAG_BOX_H  (TILE_FLAG_BASE + 1)
#define TILE_FLAG_BOX_L  (TILE_FLAG_BASE + 2)
#define TILE_FLAG_BOX_R  (TILE_FLAG_BASE + 3)
#define TILE_FLAGS       (TILE_FLAG_BASE + 4)
#define TILE_FLAGS_LARGE (TILE_FLAGS + NUM_TEAMS * 2)
#define TILE_FLAG_SELECT (TILE_FLAGS_LARGE + NUM_TEAMS * 8)
/* Scene-local menu texture; gameplay reloads the centre-net bank before use. */
#define TILE_MENU_PATTERN TILE_COURT_FG_BASE

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

static const u32 tile_flag_select[8] = {
    0x99999999, 0x99999999, 0x99999999, 0x99999999,
    0x99999999, 0x99999999, 0x99999999, 0x99999999
};

/* Subtle embossed diagonal, deliberately using the near-black flag colour
 * over navy. Alternating flips turns the repeated tile into a woven chevron
 * instead of an obvious wallpaper stripe. */
static const u32 tile_menu_pattern[8] = {
    0xb4444444, 0x4b444444, 0x44b44444, 0x444b4444,
    0x4444b444, 0x44444b44, 0x444444b4, 0x4444444b
};

static void apply_flag_palette(void)
{
    /* Restore the selector's own background and text too: the studio/title
     * transition fades every palette entry to black before this screen. */
    PAL_setColor(0 * 16 + 0,  RGB24_TO_VDPCOLOR(0x101C38));
    PAL_setColor(0 * 16 + 1,  RGB24_TO_VDPCOLOR(0xF8F8F8));
    PAL_setColor(0 * 16 + 4,  RGB24_TO_VDPCOLOR(0x101C38));
    PAL_setColor(0 * 16 + 15, RGB24_TO_VDPCOLOR(0xF8F8F8));
    PAL_setColor(PAL3 * 16 + 0,  RGB24_TO_VDPCOLOR(0x2048B0));
    PAL_setColor(PAL3 * 16 + 15, RGB24_TO_VDPCOLOR(0xF8F8F8));
    /* Flags use the remaining seven colors. */
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
    VDP_loadTileData(flag_large_tiles[0], TILE_FLAGS_LARGE, NUM_TEAMS * 8, DMA);
    VDP_loadTileData(tile_flag_select, TILE_FLAG_SELECT, 1, DMA);
}

void flag_data_fill_panel(u16 x, u16 y, u16 w, u16 h)
{
    /* Lay the solid navy panel tile across a BG_B rectangle. Used behind UI
     * boxes so the font glyphs' transparent pixels reveal navy (matching the
     * box fill) instead of the court/crowd bleeding through. The panel tile
     * is index-4 navy, exactly the colour of the UI fill on PAL0. */
    u16 row, col;
    for (row = 0; row < h; row++)
        for (col = 0; col < w; col++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                TILE_FLAG_PANEL), x + col, y + row);
}

static u16 centred_in_region(const char *text, u16 x, u16 width)
{
    u16 length = (u16)strlen(text);
    if (length >= width) return x;
    return x + (width - length) / 2;
}

void flag_data_fill_backdrop(void)
{
    u16 row, col;
    /* Unlike the selector, setup and tournament screens do not call the flag
     * palette loader. Pin the texture ink here so transitions from gameplay
     * cannot leave the weave using an arbitrary court colour. */
    PAL_setColor(PAL0 * 16 + 11, RGB24_TO_VDPCOLOR(0x040C20));
    VDP_loadTileData(tile_menu_pattern, TILE_MENU_PATTERN, 1, CPU);
    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(BG_B,
                TILE_ATTR_FULL(PAL0, 0, (row & 1), (col & 1),
                               TILE_MENU_PATTERN), col, row);
}

void flag_data_draw_small(u8 teamIndex, u16 x, u16 y, u8 palette)
{
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(palette, 1, FALSE, FALSE,
        TILE_FLAGS + teamIndex * 2), x, y);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(palette, 1, FALSE, FALSE,
        TILE_FLAGS + teamIndex * 2 + 1), x + 1, y);
}

void flag_data_draw_large(u8 teamIndex, u16 x, u16 y, u8 palette)
{
    u16 row, col;
    u16 base = TILE_FLAGS_LARGE + teamIndex * 8;
    for (col = 0; col < 4; col++)
        for (row = 0; row < 2; row++)
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(palette, 1, FALSE, FALSE,
                base + col * 2 + row), x + col, y + row);
}

void flag_data_draw_matchup(u8 teamAIndex, u8 teamBIndex)
{
    u16 row, col;
    const u16 leftRegionX = 2;
    const u16 rightRegionX = 22;
    const u16 regionWidth = 16;
    const u16 leftFlagX = leftRegionX + (regionWidth - 4) / 2;
    const u16 rightFlagX = rightRegionX + (regionWidth - 4) / 2;
    u16 leftBase = TILE_FLAGS_LARGE + teamAIndex * 8;
    u16 rightBase = TILE_FLAGS_LARGE + teamBIndex * 8;
    apply_flag_palette();
    ui_set_palette(PAL0);
    ui_apply_palette();

    flag_data_fill_backdrop();

    VDP_clearPlane(BG_A, TRUE);
    ui_draw_panel(1, 1, 38, 26, FALSE);
    ui_draw_big_center("MATCH UP", 2, UI_WHITE);

    /* Header row: each side's flag with its country name beneath, sitting
     * above the big player figures (drawn separately by matchup_art). */
    ui_draw_text("TEAM 1", 7, 4, UI_CYAN);
    ui_draw_text("TEAM 2", 27, 4, UI_CYAN);
    for (col = 0; col < 4; col++)
        for (row = 0; row < 2; row++)
        {
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
                leftBase + col * 2 + row), leftFlagX + col, 5 + row);
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
                rightBase + col * 2 + row), rightFlagX + col, 5 + row);
        }
    ui_draw_text(teamNames[teamAIndex],
                 centred_in_region(teamNames[teamAIndex],
                                   leftRegionX, regionWidth), 7, UI_GOLD);
    ui_draw_text(teamNames[teamBIndex],
                 centred_in_region(teamNames[teamBIndex],
                                   rightRegionX, regionWidth), 7, UI_GOLD);
    ui_draw_hrule(leftRegionX, 8, regionWidth);
    ui_draw_hrule(rightRegionX, 8, regionWidth);

    /* VS sits between the two figures, vertically centred on them. */
    ui_draw_big_text("VS", 18, 14, UI_GOLD);

    ui_draw_button("START MATCH", 13, 24, 14);
}


void flag_data_draw_selector(u8 selected, u8 playerNumber)
{
    /* Redesigned team picker (2026-07-22), layout guided by an outside AI
     * consult loosely after Virtua-Striker's cup select: bold title band,
     * a left flag+name list with the current row highlighted, a boxed big
     * flag + name on the right, and one clean control legend at the bottom.
     * Removed the old "WORLD TOP 10" tag, the "UP DOWN SELECT" line, and the
     * redundant PLAYER/CHOOSE double label - one "CHOOSE TEAM n" now. */
    u16 row, col;
    u8 i;
    u16 largeBase = TILE_FLAGS_LARGE + selected * 8;
    apply_flag_palette();
    ui_set_palette(PAL0);
    ui_apply_palette();
    flag_data_fill_backdrop();

    /* Solid broadcast backing; a bright bar sits behind the selected row
     * (list column band cols 1-17 only). */
    for (row = 0; row < 28; row++)
        for (col = 0; col < 40; col++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, row & 1, col & 1,
                (col >= 1 && col <= 17 && row == selected + 6)
                    ? TILE_FLAG_SELECT : TILE_MENU_PATTERN), col, row);

    VDP_clearPlane(BG_A, TRUE);

    /* Header band + big title, with a single right-aligned action label. */
    ui_draw_panel(0, 0, 40, 4, FALSE);
    ui_draw_big_text("TEAM SELECT", 2, 1, UI_WHITE);
    ui_draw_text(playerNumber == 1 ? "CHOOSE TEAM 1" : "CHOOSE TEAM 2",
                 25, 2, UI_GOLD);

    /* Vertical divider between the list and the preview panel. */
    for (row = 5; row <= 23; row++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
            TILE_FLAG_BOX_L), 19, row);

    /* Left: one flag + name per row, single pointer on the current row. */
    for (i = 0; i < NUM_TEAMS; i++)
    {
        u16 y = i + 6;
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
            TILE_FLAGS + i * 2), 2, y);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
            TILE_FLAGS + i * 2 + 1), 3, y);
        ui_draw_text(teamNames[i], 6, y, (i == selected) ? UI_GOLD : UI_WHITE);
        if (i == selected)
            ui_draw_text(">", 0, y, UI_GOLD);
    }

    /* Right preview panel: the large athlete occupies cols 21..29. Keep the
     * selected flag in a compact box beside it, with the name above both. */
    for (col = 33; col <= 38; col++)
    {
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_H), col, 7);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, TRUE,  FALSE, TILE_FLAG_BOX_H), col, 10);
    }
    for (row = 8; row <= 9; row++)
    {
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_L), 33, row);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_R), 38, row);
    }
    for (col = 0; col < 4; col++)
        for (row = 0; row < 2; row++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                largeBase + col * 2 + row), 34 + col, 8 + row);

    ui_draw_text(teamNames[selected],
                 29 - (u16)strlen(teamNames[selected]) / 2, 5, UI_GOLD);

    /* Bottom legend: divider rule + the two controls. */
    for (col = 1; col <= 38; col++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE,
            TILE_FLAG_BOX_H), col, 25);
    ui_draw_text("A CONFIRM", 9, 26, UI_GOLD);
    ui_draw_text("C CANCEL", 23, 26, UI_CYAN);
}

/* Paint one list row without disturbing anything above, below, or beside it.
 * The flags live on BG_B over the textured/highlight band; labels and pointer
 * live on transparent BG_A. Keeping this operation local prevents the VDP
 * from visibly working through a full selector redraw after every keypress. */
static void update_selector_row(u8 teamIndex, bool selected)
{
    u16 col;
    u16 y = (u16)teamIndex + 6;
    u16 backing = selected ? TILE_FLAG_SELECT : TILE_MENU_PATTERN;

    for (col = 1; col <= 17; col++)
        VDP_setTileMapXY(BG_B,
            TILE_ATTR_FULL(PAL0, 0, y & 1, col & 1, backing), col, y);

    /* Reapply the two flag cells overwritten by the row backing. */
    VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
        TILE_FLAGS + teamIndex * 2), 2, y);
    VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
        TILE_FLAGS + teamIndex * 2 + 1), 3, y);

    VDP_clearTileMapRect(BG_A, 0, y, 1, 1);
    VDP_clearTileMapRect(BG_A, 6, y, 12, 1);
    ui_draw_text(teamNames[teamIndex], 6, y,
                 selected ? UI_GOLD : UI_WHITE);
    if (selected)
        ui_draw_text(">", 0, y, UI_GOLD);
}

void flag_data_update_selector(u8 previous, u8 selected)
{
    u16 row, col;
    u16 largeBase = TILE_FLAGS_LARGE + selected * 8;

    if (previous == selected) return;

    update_selector_row(previous, FALSE);
    update_selector_row(selected, TRUE);

    /* The preview name is centred and variable-width, so erase just its own
     * line before drawing the replacement. The panel, divider and athlete
     * remain untouched. */
    VDP_clearTileMapRect(BG_A, 21, 5, 18, 1);
    ui_draw_text(teamNames[selected],
                 29 - (u16)strlen(teamNames[selected]) / 2, 5, UI_GOLD);

    /* Swap only the eight flag tiles inside the already-drawn frame. */
    for (col = 0; col < 4; col++)
        for (row = 0; row < 2; row++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE,
                largeBase + col * 2 + row), 34 + col, 8 + row);
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
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_PANEL), col, row);

    for (i = 0; i < NUM_TEAMS; i++)
    {
        u16 x = 1 + (i % 5) * 8;
        u16 y = 4 + (i / 5) * 4;
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAGS + i * 2), x + 1, y + 1);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAGS + i * 2 + 1), x + 2, y + 1);
    }

    {
        u16 x = 1 + (selected % 5) * 8;
        u16 y = 4 + (selected / 5) * 4;
        for (col = x; col < x + 4; col++)
        {
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_H), col, y);
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, TRUE, FALSE, TILE_FLAG_BOX_H), col, y + 2);
        }
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_L), x, y + 1);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_FLAG_BOX_R), x + 3, y + 1);
    }
}
