/*
 * court_bg.h - Isometric green court on BG_B. Floor shading, closed
 * boundaries and centre line all share the same y=x/4 projection used
 * by player and ball movement. Text/score remain on BG_A.
 */
#ifndef _COURT_BG_H_
#define _COURT_BG_H_

#include "genesis.h"

void court_bg_init(void);   /* uploads tiles + pitch colors, call once at boot */
void court_bg_draw(void);   /* paints BG_B, call from each scene's _enter() */

#endif /* _COURT_BG_H_ */
