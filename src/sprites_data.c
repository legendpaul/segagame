#include "sprites_data.h"
#include "teams.h"

/* 8x8, 4bpp, one u32 per row (8 pixels x 4 bits, MSB = leftmost pixel).
 * Shared color plan for every player tile, only index 1 (kit color)
 * changes per team:
 *   0 = transparent   1 = kit (team color)   2 = skin
 *   3 = shorts/socks (white)   4 = hair / shoes (dark trim)
 *
 * Each 16x16 pose is 4 tiles in hardware sprite order (column-major:
 * top-left, bottom-left, top-right, bottom-right). Standing quadrants
 * are reused byte-for-byte across poses that don't change that part of
 * the body (see the loadTileData calls in sprites_data_init) - only 3
 * quadrants below are genuinely new art for the run/throw/catch poses. */

static const u32 tile_tl[8] = {
    0x00000044,
    0x00000422,
    0x00000222,
    0x00000022,
    0x00000002,
    0x00001111,
    0x00221111,
    0x00221111
};

static const u32 tile_bl[8] = {
    0x00221111,
    0x00000333,
    0x00000333,
    0x00000330,
    0x00000220,
    0x00000220,
    0x00000440,
    0x00004440
};

static const u32 tile_tr[8] = {
    0x44000000,
    0x24000000,
    0x22200000,
    0x22000000,
    0x20000000,
    0x11110000,
    0x11112200,
    0x11112200
};

static const u32 tile_br[8] = {
    0x11112200,
    0x33300000,
    0x33300000,
    0x03300000,
    0x02200000,
    0x02200000,
    0x04400000,
    0x04440000
};

/* RUN pose: right leg lifted mid-stride (bottom-right only - the
 * mirrored frame reuses this same tile with hflip, showing the left
 * leg lifted instead). */
static const u32 tile_br_run[8] = {
    0x11112200,
    0x33300000,
    0x33300000,
    0x03300000,
    0x02200000,
    0x00000000,
    0x00000000,
    0x00000000
};

/* THROW pose: right arm extended fully outward (wind-up/release). */
static const u32 tile_tr_throw[8] = {
    0x44000000,
    0x24000000,
    0x22200000,
    0x22000000,
    0x20000000,
    0x11110000,
    0x11112222,
    0x11112222
};

/* CATCH pose: left arm extended outward too, mirrors tile_tr_throw on
 * the other side for a symmetrical "arms open" stance. */
static const u32 tile_tl_catch[8] = {
    0x00000044,
    0x00000422,
    0x00000222,
    0x00000022,
    0x00000002,
    0x00001111,
    0x22221111,
    0x22221111
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

/* Ground shadow cast by the ball while it's airborne - a soft dark
 * blob, not a full circle, so it reads as "on the grass" rather than a
 * second ball. */
static const u32 tile_ball_shadow[8] = {
    0x00000000,
    0x00000000,
    0x00033000,
    0x00333300,
    0x03333330,
    0x00333300,
    0x00033000,
    0x00000000
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
    /* STAND: the 4 base quadrants. */
    VDP_loadTileData(tile_tl, TILE_PLAYER_STAND + 0, 1, DMA);
    VDP_loadTileData(tile_bl, TILE_PLAYER_STAND + 1, 1, DMA);
    VDP_loadTileData(tile_tr, TILE_PLAYER_STAND + 2, 1, DMA);
    VDP_loadTileData(tile_br, TILE_PLAYER_STAND + 3, 1, DMA);

    /* RUN: same head/torso/left-leg, new lifted right leg. */
    VDP_loadTileData(tile_tl,     TILE_PLAYER_RUN + 0, 1, DMA);
    VDP_loadTileData(tile_bl,     TILE_PLAYER_RUN + 1, 1, DMA);
    VDP_loadTileData(tile_tr,     TILE_PLAYER_RUN + 2, 1, DMA);
    VDP_loadTileData(tile_br_run, TILE_PLAYER_RUN + 3, 1, DMA);

    /* THROW: same everything, new extended right arm. */
    VDP_loadTileData(tile_tl,        TILE_PLAYER_THROW + 0, 1, DMA);
    VDP_loadTileData(tile_bl,        TILE_PLAYER_THROW + 1, 1, DMA);
    VDP_loadTileData(tile_tr_throw,  TILE_PLAYER_THROW + 2, 1, DMA);
    VDP_loadTileData(tile_br,        TILE_PLAYER_THROW + 3, 1, DMA);

    /* CATCH: new extended left arm + the throw pose's right arm, for a
     * symmetrical arms-open stance. */
    VDP_loadTileData(tile_tl_catch, TILE_PLAYER_CATCH + 0, 1, DMA);
    VDP_loadTileData(tile_bl,       TILE_PLAYER_CATCH + 1, 1, DMA);
    VDP_loadTileData(tile_tr_throw, TILE_PLAYER_CATCH + 2, 1, DMA);
    VDP_loadTileData(tile_br,       TILE_PLAYER_CATCH + 3, 1, DMA);

    VDP_loadTileData(tile_ball,        TILE_BALL,        1, DMA);
    VDP_loadTileData(tile_ball_shadow, TILE_BALL_SHADOW,  1, DMA);

    PAL_setPalette(PAL_BALL, pal_ball, DMA);
}

void sprites_data_apply_teams(u8 teamAIndex, u8 teamBIndex)
{
    PAL_setPalette(PAL_TEAM_A, pal_teams[teamAIndex], DMA);
    PAL_setPalette(PAL_TEAM_B, pal_teams[teamBIndex], DMA);
}
