/*
 * sound_mgr.h - Tiny PSG sound-effect manager.
 *
 * Three PSG tone channels are used for simple beeps/blips (menu move,
 * throw, pickup, hit, score). Each call starts a note at full volume
 * that fades out over a handful of frames; sound_mgr_update() must be
 * called once per frame (from SYS_doVBlankProcess loop).
 */
#ifndef _SOUND_MGR_H_
#define _SOUND_MGR_H_

#include "genesis.h"

#define SFX_CH_UI       0
#define SFX_CH_ACTION   1
#define SFX_CH_SCORE    2

void sound_mgr_init(void);
void sound_mgr_update(void);
void sound_mgr_play(u8 channel, u16 freq, u8 decayStep);

/* Convenience shortcuts used across scenes */
void sound_mgr_blip(void);     /* menu move / select */
void sound_mgr_confirm(void);
void sound_mgr_cancel(void);
void sound_mgr_throw(void);
void sound_mgr_pickup(void);
void sound_mgr_hit(void);
void sound_mgr_bounce(void);
void sound_mgr_whistle(void);
void sound_mgr_score(void);
void sound_mgr_crowdKnockout(void);
void sound_mgr_crowdVictory(void);

#endif /* _SOUND_MGR_H_ */
