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
 * THROW and PICKUP now have their own separate 16-tile blocks below,
 * generated the same way from prompts describing the actual action -
 * see the note above tile_player_throw for why they didn't exist until
 * this pass (a request-encoding bug, not a modeling one). */
#if 0 /* Superseded by the coherent isometric sheet included below. */
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
 * ComfyUI request that finally produced this (and PICKUP below) had been
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

/* PICKUP pose - a diving/leaping reach for the loose ball.
 * The first attempt at this pose came back as a static standing-holding-
 * the-ball shot (too close to STAND to read as a distinct animation) -
 * rejected and regenerated with a stronger prompt ("deep crouch, knees
 * bent wide, both arms stretched forward... off-balance dynamic action
 * pose") before quantizing, rather than shipping the weaker first
 * result. */
static const u32 tile_player_pickup[16][8] = {
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

/* RUN pose - a real mid-stride sprint (previously this slot didn't exist:
 * player_draw() just reused STAND with hflip, which Qwen flagged as the
 * single highest-leverage graphics fix - "reads as placeholder... probably
 * costing 80% of perceived polish"). Same Pixel-Art-XL pipeline as THROW/
 * PICKUP, with one addition: the source render's background wasn't a flat
 * card - it had an outer neutral-gray frame around a white interior, and
 * the enclosed white gaps between the striding legs don't touch the image
 * border, so the existing border-seeded flood-fill couldn't reach them
 * and left ~70% of the quantized palette wasted on near-white background
 * shades. Fixed with a global lightness/saturation background threshold
 * instead of border-flood-fill for this pose. That also surfaced a second
 * issue: the baked-in ground shadow (a desaturated gray-green, coincidentally
 * close in lightness to the skin midtones) kept getting merged into the
 * same quantized slot as the vivid skin orange, rendering as a bright
 * orange smear instead of a shadow. Fixed by darkening the shadow pixels
 * before quantization so they cluster with the dark/neutral tones instead. */
static const u32 tile_player_run[16][8] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000999 },
    { 0x00000003, 0x00000888, 0x00000433, 0x00000018, 0x000000a1, 0x000000a2, 0x000000b1, 0x000000eb },
    { 0x00000ad5, 0x00000422, 0x00000495, 0x031a14bd, 0x0341348e, 0x00831800, 0x000aa000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x99999999 },
    { 0x4aa00000, 0x848a0000, 0x88420000, 0x1aaa0000, 0x14ba0000, 0x11aa0000, 0x81edd000, 0x31954100 },
    { 0x1e2d1143, 0x05583331, 0x555d8311, 0x2ed5ebba, 0xd5555900, 0xdd5d5d00, 0xddd255e0, 0xbd5995b0 },
    { 0x9de25250, 0xe9de552e, 0xe950555d, 0xe983ed2e, 0xee111ede, 0xb41134b0, 0x011388aa, 0x03111a82 },
    { 0x004110be, 0x0083100e, 0x000a3100, 0x00000180, 0x000002b2, 0x00000bda, 0x00000b90, 0x99999999 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x10000000, 0x14000000, 0x31000000, 0x81800000, 0x03800000, 0x0a100000, 0x0a180000 },
    { 0x04130000, 0x00a00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xa0000000 },
    { 0xe0000000, 0xa0000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x90000000 },
};
#endif

#include "player_isometric_tiles.inc"
#include "eliminator_variant_tiles.inc"

#if 0 /* Retired 8x8 ball; retained here only as art-source history. */
static const u32 tile_ball[4][8] = {
    { 0x00333000, 0x03131130, 0x31121213, 0x31121213,
      0x31212113, 0x31212113, 0x03232230, 0x00333000 },
    { 0x00333000, 0x03111130, 0x31122113, 0x31221313,
      0x31213213, 0x31132213, 0x03222230, 0x00333000 },
    { 0x00333000, 0x03111130, 0x31122213, 0x33333333,
      0x31222213, 0x31222213, 0x03222230, 0x00333000 },
    { 0x00333000, 0x03111130, 0x31132213, 0x31213213,
      0x31221313, 0x31122113, 0x03222230, 0x00333000 }
};
#endif

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

static const u32 tile_ball_shadow_air[8] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00033000, 0x00333300, 0x00033000, 0x00000000
};

/* A compact, mathematically symmetric held ball. Keeping it separate from
 * the large flight frames prevents a teammate's torso from clipping the
 * outline into an irregular white blob. */
static const u32 tile_ball_held[8] = {
    0x00033000,
    0x00311300,
    0x03111130,
    0x31121113,
    0x31112113,
    0x03111130,
    0x00311300,
    0x00033000
};

/* Native 16x16 containers holding one consistent compact 10px ball.
 * The thick dark contour survives both the pale court stripe and the white
 * divider; the warm two-pixel highlight gives the eye a stable orientation
 * while the grey seam rotates. Tiles are column-major for SPRITE_SIZE(2,2). */
static const u32 tile_ball16[16][8] = {
    {0x00000000,0x00000000,0x00000000,0x00000333,0x00003344,0x00033111,0x00031111,0x00031122},
    {0x00031211,0x00032111,0x00033111,0x00003311,0x00000333,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x33300000,0x11330000,0x11133000,0x11113000,0x22113000},
    {0x11213000,0x11123000,0x11133000,0x11330000,0x33300000,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x00000333,0x00003344,0x00033111,0x00032222,0x00031111},
    {0x00031111,0x00031111,0x00033111,0x00003311,0x00000333,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x33300000,0x11330000,0x11133000,0x11113000,0x21113000},
    {0x12113000,0x12113000,0x12133000,0x12330000,0x33300000,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x00000333,0x00003344,0x00033112,0x00031111,0x00031111},
    {0x00031111,0x00031111,0x00033112,0x00003321,0x00000333,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x33300000,0x11330000,0x11133000,0x21113000,0x21113000},
    {0x21113000,0x21113000,0x11133000,0x11330000,0x33300000,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x00000333,0x00003344,0x00033111,0x00031111,0x00031111},
    {0x00031111,0x00032222,0x00033111,0x00003311,0x00000333,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x33300000,0x12330000,0x12133000,0x12113000,0x12113000},
    {0x21113000,0x11113000,0x11133000,0x11330000,0x33300000,0x00000000,0x00000000,0x00000000}
};

/* 24x16 rings; unlike the old solid star these leave the feet readable. */
static const u32 tile_ring_yellow[6][8] = {
    {0x00000000,0x00000000,0x00000033,0x00033444,0x00344400,0x03440000,0x34400000,0x34400000},
    {0x34400000,0x34400000,0x03440000,0x00344400,0x00033444,0x00000033,0x00000000,0x00000000},
    {0x00000000,0x00033000,0x44444444,0,0,0,0,0},{0,0,0,0,0,0x44444444,0x00033000,0},
    {0,0,0x33000000,0x44433000,0x00444300,0x00004430,0x00000443,0x00000443},
    {0x00000443,0x00000443,0x00004430,0x00444300,0x44433000,0x33000000,0,0}
};
static const u32 tile_ring_red[6][8] = {
    {0,0,0x00000033,0x00033555,0x00355500,0x03550000,0x35500000,0x35500000},
    {0x35500000,0x35500000,0x03550000,0x00355500,0x00033555,0,0,0},
    {0,0x00033000,0x55555555,0,0,0,0,0},{0,0,0,0,0,0x55555555,0x00033000,0},
    {0,0,0x33000000,0x55533000,0x00555300,0x00005530,0x00000553,0x00000553},
    {0x00000553,0x00000553,0x00005530,0x00555300,0x55533000,0x33000000,0,0}
};

#if 0 /* Replaced by tile_iso_far from the generated isometric sheet. */
/* Far-side player (see TILE_PLAYER_FAR): a 24x24 reduction of the real
 * STAND art, sharing the complete 14-color team palette. The old 8x8
 * single-tile figure made the CPU team functionally unreadable at the
 * emulator's normal display size. Since the VDP cannot scale sprites,
 * this is separately encoded: crop the indexed STAND art, reduce it
 * with nearest-neighbour sampling, bottom-align it in a 24x24 canvas,
 * then store the nine tiles in hardware column-major order. */
static const u32 tile_player_far[9][8] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000b, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x000000ec, 0x000000bd, 0x00000b94, 0x000000a3, 0x00000009, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000555, 0x00000555 },
    { 0x000b0000, 0x0aaba000, 0x088a8a90, 0xb88888ab, 0xa888888b, 0xb88334aa, 0xba311190, 0xba411140 },
    { 0xeba949ee, 0x77d4d7cc, 0xc76c66dc, 0xec6666ee, 0x1cc77ce0, 0x3cccccd0, 0xacccccd0, 0x0edee765 },
    { 0x0c6cd755, 0x0c7c7e31, 0x0a142254, 0x09342225, 0x0eee2222, 0x599e5555, 0x555e5555, 0x55d55555 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x11900000, 0x14b00000, 0x9b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0xee000000, 0xeb000000, 0xe9e00000, 0x2de00000, 0x55500000, 0x55500000 },
};

#endif

#if 0 /* Retired solid markers; the open 24x16 rings replace them. */
static const u32 tile_marker_yellow[2][8] = {
    { 0x00000003, 0x00000334, 0x00333444, 0x03444444,
      0x33444444, 0x03444444, 0x00333444, 0x00000334 },
    { 0x30000000, 0x43300000, 0x44433300, 0x44444430,
      0x44444433, 0x44444430, 0x44433300, 0x43300000 }
};
static const u32 tile_marker_red[2][8] = {
    { 0x00000003, 0x00000335, 0x00333555, 0x03555555,
      0x33555555, 0x03555555, 0x00333555, 0x00000335 },
    { 0x30000000, 0x53300000, 0x55533300, 0x55555530,
      0x55555533, 0x55555530, 0x55533300, 0x53300000 }
};
#endif

/* Per-team jersey palettes. Indices 2,5,6,7,12,13,14 are the "kit ramp".
 * Every country follows one semantic layout: 2/5/12 light, 6 mid, and
 * 7/13/14 dark. Team Select, regular gameplay and compact Eliminator palettes
 * therefore sample the same identity instead of accidentally choosing trim.
 * Indices 1,3,4,8,9,10,11 are fixed skin/hair/outline tones, identical
 * across every team. pal_teams[] maps a deliberately distinct national
 * identity onto every side; no two countries share a kit ramp. */
#define P(rgb24) RGB24_TO_VDPCOLOR(rgb24)

static const u16 pal_team_red[16] = {
    0x0000, P(0xE1A48E), P(0xFF5848), P(0xD2775A), P(0xBA7061), P(0xFF5848),
    P(0xD82028), P(0x781018), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0xFF5848), P(0x781018), P(0x781018)
};

static const u16 pal_team_blue[16] = {
    0x0000, P(0xE1A48E), P(0x7090F8), P(0xD2775A), P(0xBA7061), P(0x7090F8),
    P(0x2850C8), P(0x102868), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0x7090F8), P(0x102868), P(0x102868)
};

static const u16 pal_team_portugal[16] = {
    0x0000, P(0xE1A48E), P(0x58D878), P(0xD2775A), P(0xBA7061), P(0x58D878),
    P(0x20A850), P(0x086030), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0x58D878), P(0x086030), P(0x086030)
};

static const u16 pal_team_gold[16] = {
    0x0000, P(0xE1A48E), P(0xFFE850), P(0xD2775A), P(0xBA7061), P(0xFFE850),
    P(0xE8B818), P(0x806408), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0xFFE850), P(0x806408), P(0x806408)
};

static const u16 pal_team_lightblue[16] = {
    0x0000, P(0xE1A48E), P(0xA8F0FF), P(0xD2775A), P(0xBA7061), P(0xA8F0FF),
    P(0x48B8E8), P(0x185878), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0xA8F0FF), P(0x185878), P(0x185878)
};

static const u16 pal_team_white[16] = {
    0x0000, P(0xE1A48E), P(0xF8F8F8), P(0xD2775A), P(0xBA7061), P(0xF8F8F8),
    P(0xC8D0D8), P(0x687078), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0xF8F8F8), P(0x687078), P(0x687078)
};

static const u16 pal_team_maroon[16] = {
    0x0000, P(0xE1A48E), P(0xC878E8), P(0xD2775A), P(0xBA7061), P(0xC878E8),
    P(0x8840B8), P(0x481868), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0xC878E8), P(0x481868), P(0x481868)
};

static const u16 pal_team_orange[16] = {
    0x0000, P(0xE1A48E), P(0xFFA040), P(0xD2775A), P(0xBA7061), P(0xFFA040),
    P(0xD86818), P(0x783008), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0xFFA040), P(0x783008), P(0x783008)
};

static const u16 pal_team_belgium[16] = {
    0x0000, P(0xE1A48E), P(0x707078), P(0xD2775A), P(0xBA7061), P(0x707078),
    P(0x383840), P(0x101018), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0x707078), P(0x101018), P(0x101018)
};

static const u16 pal_team_mexico[16] = {
    0x0000, P(0xE1A48E), P(0xFF88C8), P(0xD2775A), P(0xBA7061), P(0xFF88C8),
    P(0xD83888), P(0x781048), P(0xCA5B18), P(0x875051), P(0x953C20),
    P(0x6A2822), P(0xFF88C8), P(0x781048), P(0x781048)
};

static const u16 * const pal_teams[NUM_TEAMS] = {
    pal_team_red,       /* Spain */
    pal_team_lightblue, /* Argentina */
    pal_team_blue,      /* France */
    pal_team_white,     /* England */
    pal_team_gold,      /* Brazil */
    pal_team_maroon,    /* Morocco */
    pal_team_portugal,  /* Portugal */
    pal_team_belgium,   /* Belgium */
    pal_team_orange,    /* Netherlands */
    pal_team_mexico     /* Mexico */
};

/* GLOBAL ELIMINATOR palette layout. Four two-shade kits fit beside seven
 * shared Exhibition-derived skin/hair/outline shades. The deepest brown and
 * black source inks share the final near-black slot, preserving clean hair,
 * boots and silhouettes for all ten simultaneous countries. */
static u16 pal_elim_a[16];
static u16 pal_elim_b[16];
static u16 pal_elim_c[16];
static const u16 pal_ball[16];

void sprites_data_load_eliminator_player(u8 teamIndex, u16 sourceBase,
                                         u16 destination)
{
    u8 pose;
    u8 variant;

    if (sourceBase == TILE_PLAYER_FRONT_STAND) pose = 0;
    else if (sourceBase == TILE_PLAYER_FRONT_RUN) pose = 1;
    else if (sourceBase == TILE_PLAYER_FRONT_RUN_PASS) pose = 2;
    else if (sourceBase == TILE_PLAYER_FRONT_RUN_ALT) pose = 3;
    else if (sourceBase == TILE_PLAYER_FRONT_THROW) pose = 4;
    else if (sourceBase == TILE_PLAYER_FRONT_PICKUP) pose = 5;
    else if (sourceBase == TILE_PLAYER_FRONT_HIT) pose = 6;
    else if (sourceBase == TILE_PLAYER_FRONT_FALL) pose = 7;
    else if (sourceBase == TILE_PLAYER_FRONT_CELEBRATE) pose = 8;
    else if (sourceBase == TILE_PLAYER_BACK_RUN) pose = 10;
    else if (sourceBase == TILE_PLAYER_BACK_RUN_PASS) pose = 11;
    else if (sourceBase == TILE_PLAYER_BACK_RUN_ALT) pose = 12;
    else if (sourceBase == TILE_PLAYER_BACK_THROW) pose = 13;
    else if (sourceBase == TILE_PLAYER_BACK_HIT) pose = 14;
    else if (sourceBase == TILE_PLAYER_BACK_FALL) pose = 15;
    else if (sourceBase == TILE_PLAYER_BACK_CELEBRATE) pose = 16;
    else pose = 9;

    /* These frames are already recoloured in ROM. Live gameplay now performs
     * one queued 512-byte copy only—no per-pixel conversion and no RAM race. */
    variant = teamIndex < 4 ? teamIndex
            : teamIndex < 7 ? (u8)(teamIndex - 4)
                            : (u8)(teamIndex - 7);
    VDP_loadTileData(tile_elim_variants[variant][pose][0],
                     destination, 16, DMA);
}

void sprites_data_load_eliminator_ball_art(void)
{
    static u32 converted[30][8];
    u16 tile, row;
    u8 pixel;

    for (tile = 0; tile < 30; tile++)
    {
        const u32 *source;
        if (tile == 0) source = tile_ball_shadow;
        else if (tile == 1) source = tile_ball_shadow_air;
        else if (tile < 18) source = tile_ball16[tile - 2];
        else if (tile < 24) source = tile_ring_yellow[tile - 18];
        else source = tile_ring_red[tile - 24];

        for (row = 0; row < 8; row++)
        {
            u32 in = source[row];
            u32 out = 0;
            for (pixel = 0; pixel < 8; pixel++)
            {
                u8 shift = (u8)(28 - pixel * 4);
                u8 colour = (u8)((in >> shift) & 15);
                u8 mapped;
                if (colour == 0) mapped = 0;
                else if (tile < 2) mapped = 15;       /* shadow: dark */
                else if (tile < 18)
                    mapped = (colour == 1 || colour == 4) ? 14 : 15;
                else if (tile < 24)
                    mapped = colour == 4 ? 6 : 14;   /* PAL0 gold + outline */
                else
                    mapped = colour == 5 ? 9 : 14;   /* PAL0 red + outline */
                out |= (u32)mapped << shift;
            }
            converted[tile][row] = out;
        }
    }
    VDP_loadTileData(converted[0], TILE_ELIM_BALL_SHADOW, 30, CPU);
}

/* Add ordered, low-density light inside an existing open ring. The ellipse is
 * smaller than the outline, so no glow leaks outside the coloured circle; the
 * player's later sprite slot naturally draws feet and body over the light. */
static void build_pulsed_ring(const u32 source[6][8], u32 output[6][8],
                              u8 fillColour, u8 phase)
{
    static const u8 bayer[16] = {
         0,  8,  2, 10,
        12,  4, 14,  6,
         3, 11,  1,  9,
        15,  7, 13,  5
    };
    static const u8 density[4] = { 1, 2, 3, 2 };
    u8 tile, row, x, y;
    u8 limit = density[phase & 3];

    for (tile = 0; tile < 6; tile++)
        for (row = 0; row < 8; row++)
            output[tile][row] = source[tile][row];

    for (y = 0; y < 16; y++)
    {
        for (x = 0; x < 24; x++)
        {
            s16 dx = (s16)(x * 2) - 23;
            s16 dy = (s16)(y * 2) - 15;
            u8 sourceTile = (u8)((x >> 3) * 2 + (y >> 3));
            u8 sourceRow = y & 7;
            u8 shift = (u8)(28 - (x & 7) * 4);
            u32 nibble = (output[sourceTile][sourceRow] >> shift) & 15;

            /* 19x9 doubled-coordinate ellipse, inset from the 24x16 ring. */
            if (nibble == 0 &&
                (s32)dx * dx * 81 + (s32)dy * dy * 361 <= 29241 &&
                bayer[(y & 3) * 4 + (x & 3)] < limit)
            {
                output[sourceTile][sourceRow] |= (u32)fillColour << shift;
            }
        }
    }
}

void sprites_data_load_alt_rings(u16 base)
{
    /* Player 2 needs rings that can never be mistaken for player 1's. The ring
     * artwork is identical in both banks - only the fill index differs (4 gold,
     * 5 red) - so two extra banks are produced by remapping that one nibble to
     * unused PAL_BALL entries: 9 (blue) when free, 12 (pale blue) when
     * carrying. Four rings, four clearly different colours. */
    u32 blue[6][8], pale[6][8];
    u8 t, r, n;
    for (t = 0; t < 6; t++)
        for (r = 0; r < 8; r++)
        {
            u32 sy = tile_ring_yellow[t][r], sr = tile_ring_red[t][r];
            u32 ob = 0, op = 0;
            for (n = 0; n < 8; n++)
            {
                u8 shift = (u8)(28 - 4 * n);
                u8 vy = (u8)((sy >> shift) & 0xF);
                u8 vr = (u8)((sr >> shift) & 0xF);
                if (vy == 4) vy = 9;
                if (vr == 5) vr = 12;
                ob |= (u32)vy << shift;
                op |= (u32)vr << shift;
            }
            blue[t][r] = ob;
            pale[t][r] = op;
        }
    VDP_loadTileData(blue[0], base, 6, DMA);
    VDP_loadTileData(pale[0], (u16)(base + 6), 6, DMA);
}

void sprites_data_set_ring_pulse(u8 phase, bool eliminator)
{
    static u32 yellow[6][8];
    static u32 red[6][8];
    static u32 convertedYellow[6][8];
    static u32 convertedRed[6][8];
    u8 tile, row, pixel;

    build_pulsed_ring(tile_ring_yellow, yellow, 4, phase);
    build_pulsed_ring(tile_ring_red, red, 5, phase);

    if (!eliminator)
    {
        VDP_loadTileData(yellow[0], TILE_RING_YELLOW, 6, DMA);
        VDP_loadTileData(red[0], TILE_RING_RED, 6, DMA);
        return;
    }

    /* Eliminator markers live on PAL0, so translate the normal ball-palette
     * yellow/red and outline slots into the court's gold/red/charcoal slots. */
    for (tile = 0; tile < 6; tile++)
    {
        for (row = 0; row < 8; row++)
        {
            u32 inYellow = yellow[tile][row];
            u32 inRed = red[tile][row];
            u32 outYellow = 0;
            u32 outRed = 0;
            for (pixel = 0; pixel < 8; pixel++)
            {
                u8 shift = (u8)(28 - pixel * 4);
                u8 yellowColour = (u8)((inYellow >> shift) & 15);
                u8 redColour = (u8)((inRed >> shift) & 15);
                u8 mappedYellow = yellowColour == 0 ? 0
                                : yellowColour == 4 ? 6 : 14;
                u8 mappedRed = redColour == 0 ? 0
                             : redColour == 5 ? 9 : 14;
                outYellow |= (u32)mappedYellow << shift;
                outRed |= (u32)mappedRed << shift;
            }
            convertedYellow[tile][row] = outYellow;
            convertedRed[tile][row] = outRed;
        }
    }
    VDP_loadTileData(convertedYellow[0], TILE_ELIM_RING_YELLOW, 6, CPU);
    VDP_loadTileData(convertedRed[0], TILE_ELIM_RING_RED, 6, CPU);
}

void sprites_data_apply_eliminator_teams(void)
{
    u8 i;
    /* Every full team palette now uses the same semantic layout: 5 is the
     * primary light and 13 the primary dark. This guarantees Eliminator's
     * compact pair exactly matches Team Select and regular gameplay. */
    static const u8 kitLight[NUM_TEAMS] = {
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5
    };
    static const u8 kitDark[NUM_TEAMS] = {
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13
    };
    static const u16 fixed[8] = {
        0x0000, P(0xE1A48E), P(0xD2775A), P(0xBA7061),
        P(0xCA5B18), P(0x875051), P(0x953C20), P(0x181820)
    };

    for (i = 0; i < 8; i++)
        pal_elim_a[i] = pal_elim_b[i] = pal_elim_c[i] = fixed[i];
    for (i = 0; i < 4; i++)
    {
        pal_elim_a[8 + i * 2] = pal_teams[i][kitLight[i]];
        pal_elim_a[9 + i * 2] = pal_teams[i][kitDark[i]];
    }
    for (i = 0; i < 3; i++)
    {
        u8 teamB = (u8)(i + 4);
        u8 teamC = (u8)(i + 7);
        pal_elim_b[8 + i * 2] = pal_teams[teamB][kitLight[teamB]];
        pal_elim_b[9 + i * 2] = pal_teams[teamB][kitDark[teamB]];
        pal_elim_c[8 + i * 2] = pal_teams[teamC][kitLight[teamC]];
        pal_elim_c[9 + i * 2] = pal_teams[teamC][kitDark[teamC]];
    }
    /* PAL3 also carries the simplified two-colour Eliminator ball. */
    pal_elim_c[14] = P(0xF8F8F8);
    pal_elim_c[15] = P(0x181820);
    PAL_setPalette(PAL_TEAM_A, pal_elim_a, DMA);
    PAL_setPalette(PAL_TEAM_B, pal_elim_b, DMA);
    PAL_setPalette(PAL_BALL, pal_elim_c, DMA);
}

static const u16 pal_ball[16] = {
    0x0000, P(0xF8F8F8), P(0x9098A0), P(0x101010),
    P(0xF8D020), P(0xE82828), P(0xF08020), 0,
    P(0xD82830), P(0x2048B0), P(0xF8C820), P(0x101018),
    P(0x70C0E8), P(0x189048), P(0xE87018), 0
};

/* A 10x10 visible star centred in a 16x16 hardware container: approximately
 * 60% of the former silhouette, without changing its carefully aligned anchor.
 * Gold marks player hits; slate grey marks court and wall hits. */
static const u32 tile_impact_burst[4][8] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x00000004,
      0x00000046, 0x00046666, 0x00004666, 0x00000466 },
    { 0x00000466, 0x00004640, 0x00046400, 0x00044000,
      0x00040000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x40000000,
      0x64000000, 0x66640000, 0x66400000, 0x64000000 },
    { 0x64000000, 0x06400000, 0x00640000, 0x00440000,
      0x00040000, 0x00000000, 0x00000000, 0x00000000 }
};

static const u32 tile_impact_grey[4][8] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x00000004,
      0x0000004d, 0x0004dddd, 0x00004ddd, 0x000004dd },
    { 0x000004dd, 0x00004d40, 0x0004d400, 0x00044000,
      0x00040000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x40000000,
      0xd4000000, 0xddd40000, 0xdd400000, 0xd4000000 },
    { 0xd4000000, 0x0d400000, 0x00d40000, 0x00440000,
      0x00040000, 0x00000000, 0x00000000, 0x00000000 }
};

void sprites_data_load_impact_art(void)
{
    VDP_loadTileData(tile_impact_burst[0], TILE_IMPACT_BURST, 4, DMA);
    VDP_loadTileData(tile_impact_grey[0], TILE_IMPACT_GREY, 4, DMA);
}

void sprites_data_load_referee_art(u16 frontStandDestination,
                                   u16 frontRunDestination,
                                   u16 backStandDestination,
                                   u16 backRunDestination)
{
    const u32 (*frames[8])[8] = {
        tile_iso_front_stand,
        tile_iso_front_run,
        tile_iso_front_run_pass,
        tile_iso_front_run_alt,
        tile_iso_back_stand,
        tile_iso_back_run,
        tile_iso_back_run_pass,
        tile_iso_back_run_alt
    };
    static u32 converted[16][8];
    u8 frame, tile, row, pixel;

    for (frame = 0; frame < 8; frame++)
    {
        for (tile = 0; tile < 16; tile++)
        {
            for (row = 0; row < 8; row++)
            {
                u32 source = frames[frame][tile][row];
                u32 output = 0;
                for (pixel = 0; pixel < 8; pixel++)
                {
                    u8 shift = (u8)(28 - pixel * 4);
                    u8 colour = (u8)((source >> shift) & 15);
                    u8 globalX = (u8)((tile / 4) * 8 + pixel);
                    u8 globalY = (u8)((tile & 3) * 8 + row);
                    u8 mapped;

                    if (colour == 0)
                        mapped = 0;                       /* transparent */
                    else if (colour == 1 || colour == 3)
                        mapped = 7;                       /* light/mid skin */
                    else if (colour == 4)
                        mapped = 15;                      /* skin shade */
                    else if (colour >= 8 && colour <= 11)
                        mapped = 3;                       /* black hair/outline/shoes */
                    else if (globalY >= 6 && globalY <= 19)
                    {
                        /* All seven team-kit shades become crisp vertical
                         * referee stripes on the shirt. Three-pixel bands are
                         * broad enough to survive the 32px sprite cleanly. */
                        mapped = ((globalX / 3) & 1) ? 3 : 1;
                    }
                    else
                    {
                        /* Below the shirt every former kit pixel belongs to
                         * shorts, socks or footwear: keep the whole lower
                         * uniform solid black as a referee should read. */
                        mapped = 3;
                    }
                    output |= (u32)mapped << shift;
                }
                converted[tile][row] = output;
            }
        }

        /* CPU upload is deliberate: the one-frame scratch buffer is reused
         * immediately, so queued DMA would race the next conversion. This is
         * a scene-entry-only 512-byte transfer hidden by the fade. */
        {
            u16 destination;
            if (frame == 0) destination = frontStandDestination;
            else if (frame < 4)
                destination = (u16)(frontRunDestination + (frame - 1) * 16);
            else if (frame == 4) destination = backStandDestination;
            else destination = (u16)(backRunDestination + (frame - 5) * 16);
            VDP_loadTileData(converted[0], destination, 16, CPU);
        }
    }
}

void sprites_data_init(void)
{
    VDP_loadTileData(tile_iso_front_stand[0], TILE_PLAYER_FRONT_STAND, 16, DMA);
    VDP_loadTileData(tile_iso_front_run[0], TILE_PLAYER_FRONT_RUN, 16, DMA);
    VDP_loadTileData(tile_iso_front_run_alt[0], TILE_PLAYER_FRONT_RUN_ALT, 16, DMA);
    VDP_loadTileData(tile_iso_front_throw[0], TILE_PLAYER_FRONT_THROW, 16, DMA);
    VDP_loadTileData(tile_iso_front_pickup[0], TILE_PLAYER_FRONT_PICKUP, 16, DMA);
    VDP_loadTileData(tile_iso_back_stand[0], TILE_PLAYER_BACK_STAND, 16, DMA);
    VDP_loadTileData(tile_iso_back_run[0], TILE_PLAYER_BACK_RUN, 16, DMA);
    VDP_loadTileData(tile_iso_back_run_alt[0], TILE_PLAYER_BACK_RUN_ALT, 16, DMA);
    VDP_loadTileData(tile_iso_back_throw[0], TILE_PLAYER_BACK_THROW, 16, DMA);
    VDP_loadTileData(tile_iso_front_hit[0], TILE_PLAYER_FRONT_HIT, 16, DMA);
    VDP_loadTileData(tile_iso_front_fall[0], TILE_PLAYER_FRONT_FALL, 16, DMA);
    VDP_loadTileData(tile_iso_front_celebrate[0], TILE_PLAYER_FRONT_CELEBRATE, 16, DMA);
    VDP_loadTileData(tile_iso_back_hit[0], TILE_PLAYER_BACK_HIT, 16, DMA);
    VDP_loadTileData(tile_iso_back_fall[0], TILE_PLAYER_BACK_FALL, 16, DMA);
    VDP_loadTileData(tile_iso_back_celebrate[0], TILE_PLAYER_BACK_CELEBRATE, 16, DMA);
    VDP_loadTileData(tile_iso_front_run_pass[0], TILE_PLAYER_FRONT_RUN_PASS, 16, DMA);
    VDP_loadTileData(tile_iso_back_run_pass[0], TILE_PLAYER_BACK_RUN_PASS, 16, DMA);
    VDP_loadTileData(tile_ball_held, TILE_BALL_HELD, 1, DMA);
    /* Pickup still shares the established low silhouette; impact, grounded
     * fall and celebration now use their own front/rear authored blocks. */
    VDP_loadTileData(tile_ball_shadow, TILE_BALL_SHADOW,  1, DMA);
    VDP_loadTileData(tile_ball_shadow_air, TILE_BALL_SHADOW_AIR, 1, DMA);
    VDP_loadTileData(tile_ball16[0], TILE_BALL16_FRAME_0, 16, DMA);
    VDP_loadTileData(tile_ring_yellow[0], TILE_RING_YELLOW, 6, DMA);
    VDP_loadTileData(tile_ring_red[0], TILE_RING_RED, 6, DMA);

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

void sprites_data_hide_all_sprites(void)
{
    /* Park every gameplay sprite slot off-screen with a self-terminating link
     * chain. VDP_clearSprites() leaves the match's manually-set slots in the
     * cache, so on a non-gameplay screen the old link chain (players, ball,
     * shadows) still uploads and "bleeds" through. Rewriting slots 0..31 to a
     * hidden 1x1 that ends the chain guarantees a clean slate; the scene then
     * re-draws only the sprites it actually wants. */
    u16 i;
    for (i = 0; i < 32; i++)
        VDP_setSpriteFull((u8)i, 0, -32, SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, 0),
                          (u8)((i + 1) < 32 ? (i + 1) : 0));
}

void sprites_data_kit_ramp(u8 teamIndex, u16 *out)
{
    /* The three most representative kit shades (light/mid/dark) from a
     * team's full player palette, for reuse by the matchup screen's big
     * figures. Indices 2/6/13 are the jersey light/mid/dark across every
     * team palette (skin/shorts indices are shared and sit elsewhere). */
    const u16 *p = pal_teams[teamIndex];
    out[0] = p[2];
    out[1] = p[6];
    out[2] = p[13];
}

void sprites_data_flash_team(u8 palLine)
{
    /* Whites-out every non-transparent index on the line so the whole
     * sprite reads as a bright flash for a couple of frames - cheap
     * (one DMA palette write, no tile re-upload) and instantly readable
     * impact feedback for a hit. */
    u16 i;
    for (i = 1; i < 16; i++)
        PAL_setColor(palLine * 16 + i, RGB24_TO_VDPCOLOR(0xF8F8F8));
}
