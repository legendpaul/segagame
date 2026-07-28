#include "matchup_art.h"
#include "sprites_data.h"
#include "teams.h"

#include "matchup_tiles.inc"

/* Load into the court tile region: unused on the matchup screen and fully
 * reloaded by court_bg_draw() when a match starts. */
#define TILE_MATCHUP_BASE  TILE_COURT_BASE

/* Figure placement in 8px tiles. The figures are 9x16 tiles; the silhouette
 * is centred with transparent margins, so cols land the bodies inboard. */
#define FIG_LX  5
#define FIG_RX  (40 - MATCHUP_FIG_W - 5)   /* 26 */
#define FIG_Y   8
#define SELECT_FIG_X 21
#define SELECT_FIG_Y 7

/* Per-country skin ramps (light, mid, dark) as RGB24. Kept deliberately
 * moderate; assigned per national team below. */
static const u32 SKIN_LIGHT[3] = { 0xF0C8A0, 0xD09A6E, 0x9C6C40 };
static const u32 SKIN_MID[3]   = { 0xD8A878, 0xB07C50, 0x7C542E };
static const u32 SKIN_OLIVE[3] = { 0xC8A070, 0x9C7848, 0x6E4E28 };

/* Team index order (teams.c): SPAIN, ARGENTINA, FRANCE, ENGLAND, BRAZIL,
 * MOROCCO, PORTUGAL, BELGIUM, NETHERLANDS, MEXICO. */
static const u32 * const teamSkin[NUM_TEAMS] = {
    SKIN_MID,   /* Spain       */
    SKIN_LIGHT, /* Argentina   */
    SKIN_MID,   /* France      */
    SKIN_LIGHT, /* England     */
    SKIN_MID,   /* Brazil      */
    SKIN_OLIVE, /* Morocco     */
    SKIN_MID,   /* Portugal    */
    SKIN_LIGHT, /* Belgium     */
    SKIN_LIGHT, /* Netherlands */
    SKIN_MID,   /* Mexico      */
};

void matchup_art_load(void)
{
    VDP_loadTileData(matchup_fig_tiles[0], TILE_MATCHUP_BASE,
                     MATCHUP_FIG_TILE_COUNT, CPU);
}

/* Fixed layout: 2-4 kit, 5-7 skin, everything else from the base palette. */
static void build_palette(u16 palLine, u8 teamIndex)
{
    u16 pal[16];
    u16 kit[3];
    const u32 *sk = teamSkin[teamIndex];
    u16 i;

    for (i = 0; i < 16; i++) pal[i] = matchup_base_palette[i];

    sprites_data_kit_ramp(teamIndex, kit);
    pal[2] = kit[0]; pal[3] = kit[1]; pal[4] = kit[2];
    pal[5] = RGB24_TO_VDPCOLOR(sk[0]);
    pal[6] = RGB24_TO_VDPCOLOR(sk[1]);
    pal[7] = RGB24_TO_VDPCOLOR(sk[2]);

    PAL_setPalette(palLine, pal, CPU);
}

void matchup_art_draw(u8 teamAIndex, u8 teamBIndex)
{
    u16 r, c, t;

    build_palette(PAL1, teamAIndex);
    build_palette(PAL2, teamBIndex);

    for (r = 0; r < MATCHUP_FIG_H; r++)
        for (c = 0; c < MATCHUP_FIG_W; c++)
        {
            /* The source pose faces right: Team 1 uses it normally, while
             * Team 2 reverses the map and every tile to face left. */
            t = matchup_fig2_map[r][c];
            if (t)
                VDP_setTileMapXY(BG_A,
                    TILE_ATTR_FULL(PAL1, 1, FALSE, FALSE, TILE_MATCHUP_BASE + t),
                    FIG_LX + c, FIG_Y + r);

            t = matchup_fig2_map[r][MATCHUP_FIG_W - 1 - c];
            if (t)
                VDP_setTileMapXY(BG_A,
                    TILE_ATTR_FULL(PAL2, 1, FALSE, TRUE, TILE_MATCHUP_BASE + t),
                    FIG_RX + c, FIG_Y + r);
        }
}

void matchup_art_draw_selector(u8 teamIndex)
{
    u16 r, c, t;

    /* PAL1 is scene-local here. build_palette obtains the kit ramp through
     * sprites_data_kit_ramp(), exactly as the matchup and gameplay art do. */
    build_palette(PAL1, teamIndex);
    for (r = 0; r < MATCHUP_FIG_H; r++)
        for (c = 0; c < MATCHUP_FIG_W; c++)
        {
            /* Team Select uses the more compact crouched athlete. Its shorts
             * follow the kit ramp and its right-hand ball uses fixed white. */
            t = matchup_fig2_map[r][c];
            if (t)
                VDP_setTileMapXY(BG_A,
                    TILE_ATTR_FULL(PAL1, 1, FALSE, FALSE,
                                   TILE_MATCHUP_BASE + t),
                    SELECT_FIG_X + c, SELECT_FIG_Y + r);
    }
}

void matchup_art_update_selector_palette(u8 teamIndex)
{
    build_palette(PAL1, teamIndex);
}
