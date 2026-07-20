/*
 * music_mgr.h - A short looping FM melody + bass line, playing
 * continuously through the menu, match, and game-over screens (real
 * 1990s Genesis games score through the YM2612 FM chip and keep PSG
 * free for SFX - this follows the same split).
 *
 * Honest limitation: this is one hand-built FM instrument voice
 * playing a short hand-written phrase, not a composed multi-
 * instrument soundtrack - authoring that normally means a tracker
 * tool (DefleMask/Furnace) exporting to SGDK's XGM driver, which
 * isn't available in this environment.
 */
#ifndef _MUSIC_MGR_H_
#define _MUSIC_MGR_H_

#include "genesis.h"

void music_mgr_init(void);    /* call once at boot */
void music_mgr_update(void);  /* call once per frame */

#endif /* _MUSIC_MGR_H_ */
