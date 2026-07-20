#include "sprites_data.h"
#include "teams.h"

/* 8x8, 4bpp, one u32 per row (8 pixels x 4 bits, MSB = leftmost pixel).
 * Color plan for the player sprite - 14 real colors now (index 0 stays
 * transparent), not the old flat 4-color plan:
 *   1  = skin (fixed, all teams)          4  = hair highlight (fixed)
 *   5,8,11 = hair/shoe shading (fixed)    13,14 = outline/shadow (fixed)
 *   2,3,6,7,9,10,12 = the jersey "kit ramp" - these 7 indices are what
 *   sprites_data_apply_teams() actually recolors per team.
 *
 * tile_player_stand[16][8] is a full 4x4 hardware sprite block (32x32px
 * - the actual max single-sprite size the Genesis VDP supports), tiles
 * in column-major hardware order (col0 top-to-bottom 4 tiles, then
 * col1, col2, col3).
 *
 * Third pass at this sprite, and the fix that actually mattered: the
 * previous two passes were readable but flat/noisy, because the whole
 * pipeline only ever used 4 flat colors total for the entire player -
 * real 16-bit sports sprites (the "EA Sports on Genesis" bar this was
 * being measured against) use most of their 16-color palette budget for
 * proper light/mid/dark shading, not one flat tone per body part. This
 * pass:
 *   1. Re-processed the same ComfyUI-generated reference through a
 *      cleaner pipeline: flood-fill background removal (instead of a
 *      flat brightness threshold, so light highlights ON the character
 *      don't get eaten as background), average-color downsample to a
 *      true 32x32 grid (instead of nearest/mode-pooled classification,
 *      which produced speckled noise), then an adaptive 14-color
 *      palette extracted directly from the source art via median-cut
 *      quantization - so the actual light/mid/dark jersey tones the AI
 *      generated survive, instead of being crushed into one flat index.
 *   2. Team recoloring now hue-rotates the whole 7-color jersey ramp
 *      (preserving each shade's original lightness, boosting low-
 *      saturation shades to a 0.42 floor so every team's kit reads
 *      clearly) instead of swapping one flat index - this is the same
 *      technique real sports games use for team-color swaps: the
 *      shading structure carries over, only the hue changes.
 * Honest limitation, unchanged from the previous pass: all 4 poses
 * (stand/run/throw/catch) still share this one block - see
 * sprites_data_init() and docs/planning.md. */
static const u32 tile_player_stand[16][8] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000000dd, 0x00000bca, 0x0000bc76 },
    { 0x0000da63, 0x0000da33, 0x00000ccc, 0x00000090, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000000c, 0x000000cc, 0x000000cc, 0x00000eda, 0x00000b96, 0x0000e844, 0x009db844, 0x0dee4458 },
    { 0x0000000d, 0x000000ee, 0x000000ee, 0x000000ee, 0x00000000, 0x00000004, 0x00000001, 0x000000b1 },
    { 0x000000e5, 0x000000ee, 0x00000022, 0x00000022, 0xea633321, 0xa6711112, 0x63911112, 0x33911112 },
    { 0x29dbbb73, 0x28bb93cc, 0x21252777, 0x21112a7a, 0x00b93776, 0x00bc666e, 0x0ec737dd, 0xda7669d0 },
    { 0xc663ae00, 0x7333ae00, 0x73337000, 0x33337000, 0x767eb000, 0x58be0000, 0x8be00000, 0xd0000000 },
    { 0xeebb00ed, 0x848ed00e, 0x84458de8, 0x84448bb4, 0x45984114, 0x459844e5, 0x4448b1e8, 0x44448bed },
    { 0x44444be5, 0x884445db, 0xee845e92, 0x19deed21, 0x126aa321, 0x33333113, 0x33223333, 0x33111333 },
    { 0x33311333, 0xccc63667, 0x377ceede, 0x6777cd00, 0x63667c00, 0xea636ad0, 0xdd9767cc, 0x00dc7677 },
    { 0x000da636, 0x000b7333, 0x00007733, 0x0000bc73, 0x000000e7, 0x0000000d, 0x00000000, 0x00000000 },
    { 0x00000000, 0xb0000000, 0x8b000000, 0x8b000000, 0x41000000, 0x8ee00000, 0x8be00000, 0x55d00000 },
    { 0x40000000, 0x90000000, 0x7b000000, 0x6ab00000, 0x3ad00000, 0x3ad00000, 0x3ad00000, 0x3ad00000 },
    { 0x7d000000, 0x9d000000, 0xb0000000, 0x00000000, 0x00000000, 0x00000000, 0xd0000000, 0xcb000000 },
    { 0x7cb00000, 0x6ac00000, 0x367b0000, 0x767db000, 0xa754e000, 0xed84edb0, 0xbb444bed, 0x00000000 },
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
 * humanoid, remapped onto the new 14-color plan so it shares a team's
 * real palette instead of the old separate 4-color one: 3=kit (jersey
 * ramp), 1=skin (fixed), 2=highlight (kit ramp), 14=dark outline
 * (fixed). Keeps a 1px dark outline on its torso edges (like the
 * ball's) so a light-kit team doesn't disappear against the pitch. */
static const u32 tile_player_small[8] = {
    0x00eeee00,
    0x00e11e00,
    0x0e333340,
    0xe3333334,
    0xe3333334,
    0x0e322340,
    0x0e322340,
    0xee0000ee
};

/* Per-team jersey palettes. Indices 2,3,6,7,9,10,12 are the "kit ramp"
 * - hue-rotated per team while preserving each shade's original
 * lightness (a real color-preserving recolor, not a single flat-color
 * swap), boosted to a minimum 0.42 saturation so the team color always
 * reads clearly even where the source art's shading was almost gray.
 * Indices 1,4,5,8,11,13,14 are fixed skin/hair/outline tones, identical
 * across every team. Order matches teamNames[] in teams.c: Red
 * Raptors, Blue Hawks, Green Vipers, Gold Tigers. */
#define P(rgb24) RGB24_TO_VDPCOLOR(rgb24)

static const u16 pal_team_red[16] = {
    0x0000, P(0xE3BEA0), P(0xCE878D), P(0xD1616A), P(0xEC6907), P(0xAA642F),
    P(0xC04B55), P(0x96343C), P(0x7D3D0C), P(0x582428), P(0x772229),
    P(0x201C19), P(0x480F14), P(0x0E0F0F), P(0x030303)
};

static const u16 pal_team_blue[16] = {
    0x0000, P(0xE3BEA0), P(0x87A5CE), P(0x6190D1), P(0xEC6907), P(0xAA642F),
    P(0x4B7CC0), P(0x345D96), P(0x7D3D0C), P(0x243A58), P(0x224577),
    P(0x201C19), P(0x0F2748), P(0x0E0F0F), P(0x030303)
};

static const u16 pal_team_green[16] = {
    0x0000, P(0xE3BEA0), P(0x87CE9F), P(0x61D186), P(0xEC6907), P(0xAA642F),
    P(0x4BC072), P(0x349655), P(0x7D3D0C), P(0x245835), P(0x22773E),
    P(0x201C19), P(0x0F4822), P(0x0E0F0F), P(0x030303)
};

static const u16 pal_team_gold[16] = {
    0x0000, P(0xE3BEA0), P(0xCEBC87), P(0xD1B561), P(0xEC6907), P(0xAA642F),
    P(0xC0A34B), P(0x967E34), P(0x7D3D0C), P(0x584B24), P(0x776222),
    P(0x201C19), P(0x483A0F), P(0x0E0F0F), P(0x030303)
};

static const u16 * const pal_teams[NUM_TEAMS] = {
    pal_team_red, pal_team_blue, pal_team_green, pal_team_gold
};

static const u16 pal_ball[16] = {
    0x0000, P(0xF8F8F8), P(0x9098A0), P(0x101010),
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
