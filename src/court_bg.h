/* Full-screen authored isometric stadium on BG_B. */
#ifndef _COURT_BG_H_
#define _COURT_BG_H_

#include "genesis.h"

/* 481 generated tiles plus a small guard band for stable downstream bases. */
#define COURT_TILE_COUNT 488

void court_bg_init(void);   /* uploads tiles + pitch colors, call once at boot */
void court_bg_draw(void);   /* paints BG_B, call from each scene's _enter() */

#endif /* _COURT_BG_H_ */
