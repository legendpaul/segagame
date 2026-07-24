/* Full-screen authored isometric stadium on BG_B. */
#ifndef _COURT_BG_H_
#define _COURT_BG_H_

#include "genesis.h"

/* Static reservation. The match-local court bank may extend into the boot
 * logo's VRAM because those scenes are mutually exclusive. */
#define COURT_TILE_COUNT 549
/* Match-local background currently contains 592 generated tiles; place the
 * 35 foreground tiles immediately after it, still inside the unused logo bank. */
#define TILE_COURT_FG_BASE (TILE_COURT_BASE + 592)

void court_bg_init(void);   /* uploads tiles + pitch colors, call once at boot */
void court_bg_draw(void);   /* paints BG_B, call from each scene's _enter() */
void court_bg_drawForeground(void); /* transparent priority net on BG_A */
void court_bg_redraw_rect(u16 x, u16 y, u16 w, u16 h);

#endif /* _COURT_BG_H_ */
