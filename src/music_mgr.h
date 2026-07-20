/*
 * music_mgr.h - A small looping PSG melody for the menu screen.
 *
 * Honest limitation: real multi-channel background music on Genesis
 * goes through the YM2612 FM chip via SGDK's XGM driver, authored with
 * a tracker tool - not something hand-written as raw data in a text
 * session. This is a monophonic PSG jingle (one melodic voice, reusing
 * sound_mgr's existing envelope-decay channel so it fades naturally
 * note-to-note) - real music, just not a full soundtrack.
 */
#ifndef _MUSIC_MGR_H_
#define _MUSIC_MGR_H_

#include "genesis.h"

void music_mgr_start(void);   /* call when entering the menu */
void music_mgr_stop(void);    /* call when leaving the menu */
void music_mgr_update(void);  /* call once per frame, harmless if stopped */

#endif /* _MUSIC_MGR_H_ */
