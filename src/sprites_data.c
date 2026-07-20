#include "sprites_data.h"
#include "teams.h"

/* 8x8, 4bpp, one u32 per row (8 pixels x 4 bits, MSB = leftmost pixel).
 * Shared color plan for every player tile, only index 1 (kit color)
 * changes per team:
 *   0 = transparent   1 = kit (team color)   2 = skin
 *   3 = shorts/socks (white)   4 = hair / shoes (dark trim)
 *
 * The 16x16 player is built from 4 tiles in hardware sprite order
 * (column-major: top-left, bottom-left, top-right, bottom-right). */

static const u32 tile_player_tl[8] = {
    0x00000044,
    0x00000422,
    0x00000222,
    0x00000022,
    0x00000002,
    0x00001111,
    0x00221111,
    0x00221111
};

static const u32 tile_player_bl[8] = {
    0x00221111,
    0x00000333,
    0x00000333,
    0x00000330,
    0x00000220,
    0x00000220,
    0x00000440,
    0x00004440
};

static const u32 tile_player_tr[8] = {
    0x44000000,
    0x24000000,
    0x22200000,
    0x22000000,
    0x20000000,
    0x11110000,
    0x11112200,
    0x11112200
};

static const u32 tile_player_br[8] = {
    0x11112200,
    0x33300000,
    0x33300000,
    0x03300000,
    0x02200000,
    0x02200000,
    0x04400000,
    0x04440000
};

/* Ball: white with a soft shadow (2) and a dark outline (3) so it reads
 * clearly against the green pitch. */
static const u32 tile_ball[8] = {
    0x00333000,
    0x03111130,
    0x31122213,
    0x31122213,
    0x31222213,
    0x31222213,
    0x03222230,
    0x00333000
};

/* Per-team kit palettes - index 1 is the only thing that changes;
 * skin/shorts/trim (2,3,4) stay identical across teams. Order matches
 * teamNames[] in teams.c: Red Raptors, Blue Hawks, Green Vipers, Gold
 * Tigers. */
#define SKIN   RGB24_TO_VDPCOLOR(0xF0B090)
#define WHITE  RGB24_TO_VDPCOLOR(0xF8F8F8)
#define DARK   RGB24_TO_VDPCOLOR(0x202020)

static const u16 pal_team_red[16] = {
    0x0000, RGB24_TO_VDPCOLOR(0xE81018), SKIN, WHITE, DARK, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 pal_team_blue[16] = {
    0x0000, RGB24_TO_VDPCOLOR(0x2060F0), SKIN, WHITE, DARK, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 pal_team_green[16] = {
    0x0000, RGB24_TO_VDPCOLOR(0x18A040), SKIN, WHITE, DARK, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 pal_team_gold[16] = {
    0x0000, RGB24_TO_VDPCOLOR(0xF0B818), SKIN, WHITE, DARK, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 * const pal_teams[NUM_TEAMS] = {
    pal_team_red, pal_team_blue, pal_team_green, pal_team_gold
};

static const u16 pal_ball[16] = {
    0x0000, WHITE, RGB24_TO_VDPCOLOR(0x9098A0), RGB24_TO_VDPCOLOR(0x101010),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void sprites_data_init(void)
{
    VDP_loadTileData(tile_player_tl, TILE_PLAYER + 0, 1, DMA);
    VDP_loadTileData(tile_player_bl, TILE_PLAYER + 1, 1, DMA);
    VDP_loadTileData(tile_player_tr, TILE_PLAYER + 2, 1, DMA);
    VDP_loadTileData(tile_player_br, TILE_PLAYER + 3, 1, DMA);
    VDP_loadTileData(tile_ball, TILE_BALL, 1, DMA);

    PAL_setPalette(PAL_BALL, pal_ball, DMA);
}

void sprites_data_apply_teams(u8 teamAIndex, u8 teamBIndex)
{
    PAL_setPalette(PAL_TEAM_A, pal_teams[teamAIndex], DMA);
    PAL_setPalette(PAL_TEAM_B, pal_teams[teamBIndex], DMA);
}
