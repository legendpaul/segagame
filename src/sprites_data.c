#include "sprites_data.h"
#include "teams.h"

/* 8x8, 4bpp, one u32 per row (8 pixels x 4 bits, MSB = leftmost pixel).
 * Color plan for the player sprite - 14 real colors (index 0 stays
 * transparent):
 *   1,3,4,8,9,10,11 = fixed skin/hair/outline tones, identical every team
 *   2,5,6,7,12,13,14 = the jersey "kit ramp" - hue-rotated per team by
 *   sprites_data_apply_teams().
 *
 * tile_player_stand[16][8] is a full 4x4 hardware sprite block (32x32px
 * - the actual max single-sprite size the Genesis VDP supports), tiles
 * in column-major hardware order (col0 top-to-bottom 4 tiles, then
 * col1, col2, col3).
 *
 * Fourth pass at this sprite. The first three passes all fed vanilla
 * Stable Diffusion 1.5 (a general-purpose photoreal-leaning model, not
 * a pixel-art model) through hand-written downsample/classify scripts -
 * real research (not guessing) into what actually produces clean pixel
 * art turned up Pixel-Art-XL (nerijs/pixel-art-xl), a LoRA trained
 * specifically to make SDXL output authentic, pixel-perfect art
 * natively. Downloaded locally (SDXL base + the LoRA + a fixed VAE,
 * ~7.4GB, with explicit go-ahead first) and generated through
 * ComfyUI's API the same way as before. The model card's own tip -
 * "downscale 8x with nearest neighbor to get the pixel-perfect grid" -
 * matters: at native 1024x1024 output the model still draws in fat,
 * aliased blocks; only after that 8x reduction (to 128x128) does the
 * *actual* pixel-art grid the model intended appear, clean edges and
 * all. That 128x128 grid was then cropped to the character's true
 * bounding box (not stretched - the crop was padded to square first so
 * downsampling to the 32x32 hardware grid doesn't squash the
 * proportions), and run through the same flood-fill-background-removal
 * + median-cut 14-color quantization pipeline as the previous pass.
 * Team recoloring still hue-rotates the 7-color jersey ramp while
 * preserving each shade's lightness (same technique real sports games
 * use for team swaps), baked into pal_team_red/blue/green/gold[16]
 * below.
 * Honest limitation, unchanged: all 4 poses (stand/run/throw/catch)
 * still share this one block - see sprites_data_init() and
 * docs/planning.md. */
static const u32 tile_player_stand[16][8] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000b, 0x0000000e },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000005, 0x00000005, 0x00000000 },
    { 0x000000b0, 0x000baabb, 0x000a88a8, 0x00ba8888, 0x00b88888, 0x0ba88888, 0x00b88831, 0x00a88813 },
    { 0x00baa311, 0x00baa411, 0x00eeba94, 0x0dccd414, 0xec7c7d44, 0xbdcd76cc, 0x94edc664, 0x445dcc65 },
    { 0xa319cc76, 0x0934cccc, 0x00a9cccc, 0x000edccc, 0x000eedee, 0x000ec6cd, 0x000ec7c7, 0x0000e7d2 },
    { 0x0000a142, 0x00009345, 0x0000eee2, 0x00000ee2, 0x005599e5, 0x555555e5, 0x55555d55, 0x00555555 },
    { 0x00000000, 0xa0000000, 0x8a900000, 0x888b0000, 0x88ab0000, 0x888b0000, 0x34aa0000, 0x13ab0000 },
    { 0x11900000, 0x11400000, 0x49ee0000, 0xbdccd000, 0xd7ccd119, 0x66dc414b, 0x66ee99b0, 0x6ce00000 },
    { 0x7ce00000, 0xccd00000, 0xccd00000, 0xddcd0000, 0xe765d000, 0xd755d000, 0x7e314000, 0x27411900 },
    { 0x22543ee0, 0x22259eb0, 0x22222e9e, 0x22222d5e, 0x555522de, 0x55555555, 0x55555555, 0x55555500 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x50000000, 0x50000000, 0x00000000 },
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
 * real palette instead of a separate one: 6=kit (jersey ramp), 1=skin
 * (fixed), 2=highlight (kit ramp), 14=dark outline (fixed). Keeps a
 * 1px dark outline on its torso edges (like the ball's) so a
 * light-kit team doesn't disappear against the pitch. */
static const u32 tile_player_small[8] = {
    0x00eeee00,
    0x00e11e00,
    0x0e6666e0,
    0xe666666e,
    0xe666666e,
    0x0e1661e0,
    0x0e1661e0,
    0xee0000ee
};

/* Per-team jersey palettes. Indices 2,5,6,7,12,13,14 are the "kit ramp"
 * - hue-rotated per team while preserving each shade's original
 * lightness (a real color-preserving recolor, not a single flat-color
 * swap), boosted to a minimum 0.45 saturation so the team color always
 * reads clearly even where the source art's shading was almost gray.
 * Indices 1,3,4,8,9,10,11 are fixed skin/hair/outline tones, identical
 * across every team. Order matches teamNames[] in teams.c: Red
 * Raptors, Blue Hawks, Green Vipers, Gold Tigers. */
#define P(rgb24) RGB24_TO_VDPCOLOR(rgb24)

static const u16 pal_team_red[16] = {
    0x0000, P(0xE1A48E), P(0xCF8288), P(0xD2775A), P(0xBA7061), P(0xB8464F),
    P(0xA84048), P(0x9F3C44), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0x95363E), P(0x732C32), P(0x4E1D21)
};

static const u16 pal_team_blue[16] = {
    0x0000, P(0xE1A48E), P(0x82A2CF), P(0xD2775A), P(0xBA7061), P(0x4675B8),
    P(0x406BA8), P(0x3C659F), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0x365E95), P(0x2C4A73), P(0x1D314E)
};

static const u16 pal_team_green[16] = {
    0x0000, P(0xE1A48E), P(0x82CF9C), P(0xD2775A), P(0xBA7061), P(0x46B86C),
    P(0x40A863), P(0x3C9F5D), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0x369556), P(0x2C7344), P(0x1D4E2D)
};

static const u16 pal_team_gold[16] = {
    0x0000, P(0xE1A48E), P(0xCFBC82), P(0xD2775A), P(0xBA7061), P(0xB89C46),
    P(0xA88E40), P(0x9F863C), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0x957D36), P(0x73612C), P(0x4E421D)
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
