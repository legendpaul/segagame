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
 * RUN still reuses this block with hflip for a cheap side-to-side sway.
 * THROW and CATCH now have their own separate 16-tile blocks below,
 * generated the same way from prompts describing the actual action -
 * see the note above tile_player_throw for why they didn't exist until
 * this pass (a request-encoding bug, not a modeling one). */
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

/* THROW pose - a real separate Pixel-Art-XL generation ("arm raised
 * overhead about to release the ball, leaning forward, dynamic athletic
 * pose"), run through the same pipeline as the stand pose above. The
 * ComfyUI request that finally produced this (and CATCH below) had been
 * failing with a bare 500 for every prompt, including trivial unrelated
 * ones - not a model/GPU problem as first suspected, but PowerShell's
 * `Out-File -Encoding utf8` silently prepending a UTF-8 BOM to the JSON
 * body, which the server's JSON parser rejected outright. Writing the
 * request with a real no-BOM UTF-8 encoder fixed it immediately.
 * Same 14-color plan as the stand pose (kit ramp at 2,5,6,7,12,13,14),
 * hand-classified by inspecting this specific generation's quantized
 * palette - an automatic hue-window and an automatic saturation-rank
 * classifier were both tried first and both misjudged which colors were
 * jersey vs skin for at least one of the two new poses (badly enough
 * that team recoloring visibly tinted the skin too), so the real fix was
 * looking at the actual 14 colors and deciding by eye, same as a human
 * pixel artist would when hand-indexing a sprite. */
static const u32 tile_player_throw[16][8] = {
    { 0x000000b9, 0x00000043, 0x00000094, 0x00000039, 0x000000ab, 0x00000004, 0x00000004, 0x00000008 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000d, 0x0000003b, 0x00000003 },
    { 0x40000000, 0x33000000, 0x39000000, 0x90000000, 0x90000000, 0x80000bbb, 0xa000a898, 0x300baa44 },
    { 0x300bb344, 0x4300b339, 0xa43eb933, 0x0425544a, 0x0edcc233, 0x00edc555, 0x000dc555, 0x000ddd66 },
    { 0x000edc56, 0x000eeb2e, 0x000dcc56, 0x000c6c5e, 0x000c65ee, 0x000666d0, 0x000222c0, 0x000a8ae0 },
    { 0x00043a00, 0x0033a000, 0x0a3a0000, 0x09a00000, 0xa2000000, 0x23000000, 0x2e333333, 0x33333333 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x80000000, 0xa0000000 },
    { 0xb0000000, 0xb0000000, 0xb0000000, 0xc6e00000, 0x66600000, 0x2e529000, 0x5be93400, 0xd0008480 },
    { 0xe0000a40, 0xeeee0040, 0x65e33a00, 0x65e44300, 0xede04300, 0x0000a3a0, 0x00000b80, 0x000000d2 },
    { 0x000000ed, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x33333330, 0x33333333 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x3d000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x30000000 },
};

/* CATCH pose - a diving/leaping reach for the ball, same pipeline again.
 * The first attempt at this pose came back as a static standing-holding-
 * the-ball shot (too close to STAND to read as a distinct animation) -
 * rejected and regenerated with a stronger prompt ("deep crouch, knees
 * bent wide, both arms stretched forward... off-balance dynamic action
 * pose") before quantizing, rather than shipping the weaker first
 * result. */
static const u32 tile_player_catch[16][8] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000000b2, 0x000000d1, 0x0000003b, 0x000000e0 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00aa4800, 0x00055000, 0x000a4600 },
    { 0x0000a580, 0x0000085a, 0x00000075, 0x000000a9, 0x000000a9, 0x0000000a, 0x00000000, 0x00000000 },
    { 0x00000eea, 0x0000ed2b, 0x000edddd, 0x889d2ddd, 0x445b2222, 0x4648beee, 0x00b000ed, 0x000000e8 },
    { 0x00000009, 0x0000009a, 0x00000454, 0x0b294564, 0x0ad1a000, 0x00de0000, 0x003e0000, 0x00000000 },
    { 0x000000e1, 0x00000021, 0x00000e22, 0x00000031, 0x00000082, 0x000000a3, 0x00000080, 0x00aaa040 },
    { 0xa9999050, 0xaa495a50, 0xaa365570, 0x5446b3c0, 0x75988460, 0xc7c44540, 0x9cc57500, 0xcc5551a0 },
    { 0xcccc5500, 0xc7cc5500, 0x9cccc000, 0xebcc0000, 0x2de00000, 0xdde00000, 0x22e00000, 0x55e00000 },
    { 0x65000000, 0x55000000, 0x64000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x69000000, 0x11000000, 0x11000000, 0x1c000000, 0x49000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
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
    /* RUN reuses STAND's tiles (hflip only); THROW and CATCH are now
     * genuinely separate 16-tile blocks, each uploaded on its own. */
    VDP_loadTileData(tile_player_stand[0], TILE_PLAYER_STAND, 16, DMA);
    VDP_loadTileData(tile_player_throw[0], TILE_PLAYER_THROW, 16, DMA);
    VDP_loadTileData(tile_player_catch[0], TILE_PLAYER_CATCH, 16, DMA);

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

void sprites_data_flash_team(u8 palLine)
{
    /* Whites-out every non-transparent index on the line so the whole
     * sprite reads as a bright flash for a couple of frames - cheap
     * (one DMA palette write, no tile re-upload) and instantly readable
     * impact feedback for a catch/hit. */
    u16 i;
    for (i = 1; i < 16; i++)
        PAL_setColor(palLine * 16 + i, RGB24_TO_VDPCOLOR(0xF8F8F8));
}
