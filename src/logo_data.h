/*
 * logo_data.h - The "minnka" studio boot logo, shown briefly before the
 * title screen. Built from a real user-provided source image
 * (assets/minnka_logo.png), not hand-drawn: cropped to the text + a
 * hint of the smoke/flame art, downscaled with a real image filter
 * (LANCZOS) to a 30x11 tile grid, then quantized (Floyd-Steinberg
 * dithered median-cut) to a 15-color palette + reserved black. 177 of
 * the 330 possible 8x8 cells turned out unique - the rest (mostly the
 * flat black margins) collapse onto a shared tile, so this is a real
 * background-tile compression, not a 1:1 grid dump.
 */
#ifndef _LOGO_DATA_H_
#define _LOGO_DATA_H_

#include "genesis.h"
#include "sprites_data.h"

#define TILE_LOGO_BASE   (TILE_COURT_BASE + 9)

#define LOGO_TILES_W     30
#define LOGO_TILES_H     11

void logo_data_draw(void);

#endif /* _LOGO_DATA_H_ */
