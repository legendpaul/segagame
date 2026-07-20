#include "sprites_data.h"
#include "teams.h"

/* 8x8, 4bpp, one u32 per row (8 pixels x 4 bits, MSB = leftmost pixel).
 * Shared color plan for every player tile, only index 1 (kit color)
 * changes per team:
 *   0 = transparent   1 = kit (team color)   2 = skin/hair
 *   3 = white highlight   4 = dark outline / trim
 *
 * tile_player_stand[16][8] is a full 4x4 hardware sprite block (32x32px
 * - the actual max single-sprite size the Genesis VDP supports), tiles
 * in column-major hardware order (col0 top-to-bottom 4 tiles, then
 * col1, col2, col3).
 *
 * This is a second, corrected pass at using AI generation for this
 * sprite. The first pass generated real art via a local ComfyUI
 * (Stable Diffusion 1.5) run but then quantized it down into the old
 * 16x16 (2x2-tile) sprite size - at that resolution almost all of the
 * generated detail was destroyed and it read as a small blob in-game,
 * which was a fair criticism, not a matter of taste. The fix wasn't
 * more prompting, it was recognizing the actual bottleneck was sprite
 * resolution: the same AI reference (full body, wind-up throw, ball
 * masked out of frame), quantized into this same 5-color plan by
 * hue/brightness and block-mode-pooled down to a true 32x32 grid
 * instead of 16x16, keeps the head/hair, extended arm, jersey shading
 * and lunging legs actually visible on screen.
 * Honest limitation: all 4 poses (stand/run/throw/catch) currently
 * share this single block - see sprites_data_init() and
 * docs/planning.md. Per-pose art at this resolution (16 new tiles per
 * pose) needs either separately-generated AI images that stay aligned
 * to this same character/silhouette, or hand-editing at this larger
 * scale - both bigger jobs than this pass, called out explicitly
 * rather than left unstated. */
static const u32 tile_player_stand[16][8] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000004, 0x00000041, 0x00000411 },
    { 0x00000111, 0x00000111, 0x00000444, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000004, 0x00000004, 0x00000444, 0x00000441, 0x00004444, 0x00044444, 0x04444444 },
    { 0x00000004, 0x00000044, 0x00000044, 0x00000044, 0x00000000, 0x00000002, 0x00000002, 0x00000002 },
    { 0x00000044, 0x00000044, 0x00000011, 0x00000011, 0x41111111, 0x11122221, 0x11422221, 0x11422221 },
    { 0x34444411, 0x34444144, 0x32222111, 0x02222111, 0x00041111, 0x00441114, 0x04111140, 0x41111440 },
    { 0x11111400, 0x11111400, 0x11111000, 0x11111000, 0x11140000, 0x24400000, 0x44000000, 0x00000000 },
    { 0x44400044, 0x44444004, 0x44444404, 0x44444444, 0x44144224, 0x44144442, 0x44444244, 0x44444444 },
    { 0x44444444, 0x44444244, 0x44444413, 0x14444411, 0x11114110, 0x11111001, 0x11111111, 0x11001111 },
    { 0x11100111, 0x41111111, 0x11144444, 0x11111400, 0x11111000, 0x44111140, 0x04411114, 0x00441111 },
    { 0x00041111, 0x00001111, 0x00000111, 0x00004411, 0x00000041, 0x00000004, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x40000000, 0x44000000, 0x22000000, 0x44400000, 0x44400000, 0x22400000 },
    { 0x00000000, 0x00000000, 0x10000000, 0x14400000, 0x11400000, 0x11400000, 0x11400000, 0x11400000 },
    { 0x14000000, 0x40000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x40000000, 0x40000000 },
    { 0x14000000, 0x11400000, 0x11100000, 0x11140000, 0x11124000, 0x44424400, 0x00022444, 0x00000000 },
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

/* Small single-tile player (see TILE_PLAYER_SMALL) - a compact 8x8
 * humanoid using the same color plan (1=kit, 2=skin, 3=white, 4=dark)
 * so it can share a team's normal palette. The torso keeps a 1px dark
 * outline on its left/right edges (like the ball's outline) so a
 * green-kit team doesn't disappear against the green pitch - kit color
 * alone isn't reliable contrast against a fixed-color background. */
static const u32 tile_player_small[8] = {
    0x00444400,
    0x00422400,
    0x04111140,
    0x41111114,
    0x41111114,
    0x04211240,
    0x04211240,
    0x44000044
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
    /* STAND/RUN/THROW/CATCH all currently alias the same 16-tile block
     * (see the comment above tile_player_stand) - one upload covers
     * all 4 pose constants since they're the same tile index. */
    VDP_loadTileData(tile_player_stand[0], TILE_PLAYER_STAND, 16, DMA);

    VDP_loadTileData(tile_ball,        TILE_BALL,        1, DMA);
    VDP_loadTileData(tile_ball_shadow, TILE_BALL_SHADOW,  1, DMA);
    VDP_loadTileData(tile_player_small, TILE_PLAYER_SMALL, 1, DMA);

    PAL_setPalette(PAL_BALL, pal_ball, DMA);
}

void sprites_data_apply_teams(u8 teamAIndex, u8 teamBIndex)
{
    /* Also re-applied here (not just at boot): PAL_fadeOutAll() during
     * scene transitions blackens every palette line including the
     * ball's, and this is the one place reliably called every time we
     * enter a match. */
    PAL_setPalette(PAL_BALL, pal_ball, DMA);
    PAL_setPalette(PAL_TEAM_A, pal_teams[teamAIndex], DMA);
    PAL_setPalette(PAL_TEAM_B, pal_teams[teamBIndex], DMA);
}
