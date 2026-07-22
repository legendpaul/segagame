/* Full-screen authored isometric stadium on BG_B. */
#ifndef _COURT_BG_H_
#define _COURT_BG_H_

#include "genesis.h"

/* Exact generated bank size. Downstream scene banks are compile-time based. */
#define COURT_TILE_COUNT 549

void court_bg_init(void);   /* uploads tiles + pitch colors, call once at boot */
void court_bg_draw(void);   /* paints BG_B, call from each scene's _enter() */

#endif /* _COURT_BG_H_ */
