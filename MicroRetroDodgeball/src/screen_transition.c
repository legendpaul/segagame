#include "screen_transition.h"

#define TRANSITION_OUT_FRAMES 8
#define TRANSITION_IN_FRAMES  12

static u16 targetPalette[64];
static const u16 blackPalette[64] = { 0 };

void screen_transition_fade_out(void)
{
    PAL_fadeOutAll(TRANSITION_OUT_FRAMES, FALSE);

    /* Once the picture is completely black, stop scanout while the next
     * scene replaces shared VRAM and tilemaps. In particular, matchup player
     * tiles share the court bank: exposing that replacement briefly turned
     * their old map references into tall white court-line fragments. */
    VDP_setEnable(FALSE);
}

void screen_transition_fade_in(void)
{
    /* This also covers the first boot scene, which has no preceding fade-out.
     * Keep scanout disabled until every queued transfer belongs to the new
     * scene, capture its intended palette, then reveal a genuinely black
     * completed frame and fade from there. */
    VDP_setEnable(FALSE);
    SYS_doVBlankProcess();
    PAL_getColors(0, targetPalette, 64);
    PAL_setColors(0, blackPalette, 64, CPU);
    VDP_setEnable(TRUE);
    PAL_fadeIn(0, 63, targetPalette, TRANSITION_IN_FRAMES, FALSE);
}
