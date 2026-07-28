/* Full-screen authored isometric stadium on BG_B. */
#ifndef _COURT_BG_H_
#define _COURT_BG_H_

#include "genesis.h"

/* Static reservation. The match-local court bank may extend into the boot
 * logo's VRAM because those scenes are mutually exclusive. */
#define COURT_TILE_COUNT 549
/* Keep the foreground at the historical +592 scene-local boundary. The
 * standard court occupies the first <500 tiles; any Eliminator-only tail may
 * overlap this match-only foreground bank because those two layers are never
 * loaded in the same scene. */
#define TILE_COURT_FG_BASE (TILE_COURT_BASE + 592)

void court_bg_init(void);   /* uploads tiles + pitch colors, call once at boot */
void court_bg_draw(void);   /* paints BG_B, call from each scene's _enter() */
/* Dedicated open Global Eliminator arena: identical projection and bounds,
 * but no centre barrier and mode-specific championship floor markings. */
void court_bg_drawEliminator(void);
/* Transparent priority roof plus the standard-match centre net on BG_A. */
void court_bg_drawForeground(void);
/* Roof only: Eliminator has no centre net. */
void court_bg_drawEliminatorForeground(void);
/* True when any part of a sprite rectangle enters the near/right roof mask.
 * Such sprites must use low priority so the high-priority BG_A canopy wins. */
bool court_bg_spriteBehindRoof(s16 x, s16 y, u16 w, u16 h);
void court_bg_redraw_rect(u16 x, u16 y, u16 w, u16 h);

#endif /* _COURT_BG_H_ */
