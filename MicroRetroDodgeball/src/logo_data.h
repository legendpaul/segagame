/*
 * logo_data.h - The "minnka" studio boot logo, shown briefly before the
 * title screen. Built from a real user-provided source image
 * (assets/minnka_logo.png), not hand-drawn. The complete composition is
 * downscaled with LANCZOS to a 30x13 tile grid, then mapped onto an authored
 * 15-colour logo palette + reserved black. That preserves the cream-to-white
 * wordmark, indigo flame and orange core instead of spending the palette on
 * near-black shades. Only 166 of the 390 cells are unique; black margins
 * collapse onto a shared tile rather than becoming a 1:1 grid dump.
 */
#ifndef _LOGO_DATA_H_
#define _LOGO_DATA_H_

#include "genesis.h"
#include "sprites_data.h"
#include "court_bg.h"

#define TILE_LOGO_BASE   (TILE_COURT_BASE + COURT_TILE_COUNT)
#define LOGO_TILE_COUNT  166

#define LOGO_TILES_W     30
#define LOGO_TILES_H     13

void logo_data_draw(void);

#endif /* _LOGO_DATA_H_ */
