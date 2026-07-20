/*
 * court_bg.h - Green pitch background for BG_B: striped grass, a
 * tapered perspective sideline (narrower at the top of the screen,
 * wider at the bottom) to suggest an elevated camera angle, a halfway
 * line, and a dark stand band top/bottom. Text (score, menus) stays on
 * BG_A untouched.
 */
#ifndef _COURT_BG_H_
#define _COURT_BG_H_

#include "genesis.h"

void court_bg_init(void);   /* uploads tiles + pitch colors, call once at boot */
void court_bg_draw(void);   /* paints BG_B, call from each scene's _enter() */

#endif /* _COURT_BG_H_ */
