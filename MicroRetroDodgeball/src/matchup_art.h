/*
 * matchup_art.h - Big recolourable throwing-player figures for the
 * broadcast "MATCH UP" screen. Two distinct AI-authored pixel-art
 * athletes (see tools/build_matchup_full.py); each is recoloured at
 * runtime to its team's kit colours and its country's skin tone.
 */
#ifndef _MATCHUP_ART_H_
#define _MATCHUP_ART_H_

#include "genesis.h"

/* Uploads the shared figure tile bank into VRAM. Reuses the court tile
 * region, which is reloaded on match entry, so it is safe to clobber
 * here on the matchup screen. Call once in enter_matchup before drawing. */
void matchup_art_load(void);

/* Draws the left (team A) and right (team B) figures on BG_A, building
 * their per-team kit + per-country skin palettes into PAL1/PAL2. */
void matchup_art_draw(u8 teamAIndex, u8 teamBIndex);

#endif /* _MATCHUP_ART_H_ */
