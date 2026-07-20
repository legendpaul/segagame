#include "court_bg.h"
#include "sprites_data.h"

/* Reuses PAL0 (the font palette) for background art: font glyphs only
 * use index 0 (transparent) and 1 (white), so grass/line/stand/accent
 * colors ride in the unused index 2-6 slots of the same palette line -
 * no extra palette budget needed on top of the 3 sprite lines. */
#define TILE_GRASS_A   (TILE_COURT_BASE + 0)
#define TILE_GRASS_B   (TILE_COURT_BASE + 1)
#define TILE_GRASS_M   (TILE_COURT_BASE + 2)
#define TILE_LINE_L    (TILE_COURT_BASE + 3)
#define TILE_LINE_R    (TILE_COURT_BASE + 4)
#define TILE_LINE_H    (TILE_COURT_BASE + 5)
#define TILE_STAND_A   (TILE_COURT_BASE + 6)
#define TILE_STAND_B   (TILE_COURT_BASE + 7)
#define TILE_ENDZONE   (TILE_COURT_BASE + 8)
#define TILE_CIRCLE    (TILE_COURT_BASE + 9)

#define PITCH_TOP_ROW      3
#define PITCH_BOTTOM_ROW   24
#define HALFWAY_ROW        13
#define ENDZONE_TOP_ROW     4    /* CPU serve line, just under the stand */
#define ENDZONE_BOTTOM_ROW 21    /* player serve line (COURT_BOTTOM_Y / 8) */

static const u32 tile_grass_a[8] = {
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};

static const u32 tile_grass_b[8] = {
    0x33333333, 0x33333333, 0x33333333, 0x33333333,
    0x33333333, 0x33333333, 0x33333333, 0x33333333
};

/* Mid-tone between grass A and B, used as a transition band so the
 * mown-stripe pattern reads as a smooth gradient instead of an abrupt
 * light/dark flip. */
static const u32 tile_grass_m[8] = {
    0x77777777, 0x77777777, 0x77777777, 0x77777777,
    0x77777777, 0x77777777, 0x77777777, 0x77777777
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

/* Stand: two subtly different tiles, alternated, so the crowd band
 * reads as a textured stadium wall instead of a flat color block. */
static const u32 tile_stand_a[8] = {
    0x44444444, 0x44454444, 0x44444444, 0x44444544,
    0x44444444, 0x44454444, 0x44444444, 0x44444544
};

static const u32 tile_stand_b[8] = {
    0x44445444, 0x44444444, 0x45444444, 0x44444444,
    0x44445444, 0x44444444, 0x45444444, 0x44444444
};

/* End-zone accent stripe marking each side's serve line. */
static const u32 tile_endzone[8] = {
    0x66666666, 0x66666666, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};

/* A single filled gold marker, reused at 8 points around the halfway
 * line to approximate a center-court ring (a real octagon, not a true
 * circle - the tile grid is too coarse for a smooth curve, so this is
 * an honest low-res approximation rather than a claim of a real
 * circle). */
static const u32 tile_circle[8] = {
    0x22222222, 0x22666622, 0x26666662, 0x26666662,
    0x26666662, 0x26666662, 0x22666622, 0x22222222
};

static void restore_colors(void)
{
    /* PAL_fadeOutAll() (used for scene transitions - see scene_menu.c /
     * scene_match.c / scene_gameover.c) blackens every color in all 4
     * palette lines, including ones that are otherwise only ever set
     * once. Team colors get reapplied every match via
     * sprites_data_apply_teams(), but PAL0's font white and this
     * court's own colors were only ever set at boot - after the first
     * fade they stayed black forever. Re-applying them on every
     * court_bg_draw() (which every scene already calls on entry) fixes
     * that instead of relying on a one-time init. */
    /* SGDK's built-in font actually renders its "lit" pixels using
     * color index 15 (its glyph tileset was authored that way, with
     * the default boot palette - palette_grey - deliberately filling
     * indices 8-15 with the same white so index 15 reads white
     * regardless of the index 1-7 grey ramp). Index 1 is NOT the font
     * color, despite how that might look from VDP_setTextPalette's
     * name - only index 0 (transparent) and 15 (glyph white) actually
     * matter for text. */
    PAL_setColor(0 * 16 + 0,  0x0000);                       /* transparent/black */
    PAL_setColor(0 * 16 + 15, RGB24_TO_VDPCOLOR(0xF8F8F8));  /* font glyph white */
    PAL_setColor(0 * 16 + 2, RGB24_TO_VDPCOLOR(0x40B050));  /* grass light  */
    PAL_setColor(0 * 16 + 3, RGB24_TO_VDPCOLOR(0x309040));  /* grass dark   */
    PAL_setColor(0 * 16 + 4, RGB24_TO_VDPCOLOR(0x182848));  /* stand navy   */
    PAL_setColor(0 * 16 + 5, RGB24_TO_VDPCOLOR(0x384870));  /* stand fleck  */
    PAL_setColor(0 * 16 + 6, RGB24_TO_VDPCOLOR(0xE8C020));  /* endzone gold */
    PAL_setColor(0 * 16 + 7, RGB24_TO_VDPCOLOR(0x38A048));  /* grass mid (A/B blend) */
}

void court_bg_init(void)
{
    VDP_loadTileData(tile_grass_a, TILE_GRASS_A, 1, DMA);
    VDP_loadTileData(tile_grass_b, TILE_GRASS_B, 1, DMA);
    VDP_loadTileData(tile_grass_m, TILE_GRASS_M, 1, DMA);
    VDP_loadTileData(tile_line_l,  TILE_LINE_L,  1, DMA);
    VDP_loadTileData(tile_line_r,  TILE_LINE_R,  1, DMA);
    VDP_loadTileData(tile_line_h,  TILE_LINE_H,  1, DMA);
    VDP_loadTileData(tile_stand_a, TILE_STAND_A, 1, DMA);
    VDP_loadTileData(tile_stand_b, TILE_STAND_B, 1, DMA);
    VDP_loadTileData(tile_endzone, TILE_ENDZONE, 1, DMA);
    VDP_loadTileData(tile_circle,  TILE_CIRCLE,  1, DMA);

    restore_colors();
}

void court_bg_draw(void)
{
    u16 row, col;

    restore_colors();

    for (row = 0; row < 28; row++)
    {
        if (row < PITCH_TOP_ROW || row > PITCH_BOTTOM_ROW)
        {
            u16 standTile = (row & 1) ? TILE_STAND_B : TILE_STAND_A;
            for (col = 0; col < 40; col++)
                VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, standTile), col, row);
            continue;
        }

        u16 dt = row - PITCH_TOP_ROW;
        /* Doubled from the previous pass for a much more pronounced
         * elevated-camera vanishing-point taper - the sideline gap is
         * now roughly half as wide at the top of the pitch as at the
         * bottom, instead of a subtle ~4-column shift. */
        u16 leftCol  = 8 - (dt * 8) / (PITCH_BOTTOM_ROW - PITCH_TOP_ROW);
        u16 rightCol = 31 + (dt * 8) / (PITCH_BOTTOM_ROW - PITCH_TOP_ROW);

        for (col = 0; col < 40; col++)
        {
            /* Mown-stripe bands now run DIAGONALLY (shifting one band
             * every 6 columns) instead of dead horizontal - real
             * elevated/tilted sports-camera pitches (FIFA Int'l Soccer
             * included) show mowing stripes that rake across the pitch
             * at an angle rather than as flat horizontal rows, which is
             * what actually reads as "looking down at an angle" instead
             * of "looking straight down". Light -> mid -> dark -> mid,
             * still a smooth gradient rather than an abrupt 2-tone flip. */
            u16 band = ((dt + (col / 6)) / 3) % 4;
            u16 grassTile = (band == 0) ? TILE_GRASS_A
                           : (band == 2) ? TILE_GRASS_B
                           : TILE_GRASS_M;
            /* Cheap atmospheric-perspective cue: the two rows closest to
             * the far baseline never use the lightest stripe, so the
             * far end of the pitch reads very slightly darker/hazier
             * than the near end - real elevated-camera sports games
             * fade distant turf the same way. */
            if (dt < 2 && grassTile == TILE_GRASS_A) grassTile = TILE_GRASS_M;
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, grassTile), col, row);
        }

        if (row == HALFWAY_ROW)
        {
            for (col = leftCol; col <= rightCol; col++)
                VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_LINE_H), col, row);
        }
        else if (row == ENDZONE_TOP_ROW || row == ENDZONE_BOTTOM_ROW)
        {
            for (col = leftCol; col <= rightCol; col++)
                VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_ENDZONE), col, row);
        }
        else
        {
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_LINE_L), leftCol, row);
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_LINE_R), rightCol, row);
        }
    }

    /* Center-court ring: 8 marker tiles around the halfway spot, drawn
     * as a final pass so they sit on top of the halfway line instead of
     * being overwritten by it. Octagon points, not a true circle - see
     * the honest note on tile_circle above. */
    {
        u16 cx = 19;   /* pitch centerline column */
        u16 cy = HALFWAY_ROW;
        s8 dx[8] = {  0,  3,  4,  3,  0, -3, -4, -3 };
        s8 dy[8] = { -2, -1,  0,  1,  2,  1,  0, -1 };
        u8 i;
        for (i = 0; i < 8; i++)
            VDP_setTileMapXY(VDP_BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_CIRCLE),
                              cx + dx[i], cy + dy[i]);
    }
}
