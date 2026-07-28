/*
 * sound_mgr.h - Tiny PSG sound-effect manager.
 *
 * PSG supplies crisp interface accents while four XGM PCM channels carry a
 * continuous stadium bed, chants, scalable reactions and physical action.
 */
#ifndef _SOUND_MGR_H_
#define _SOUND_MGR_H_

#include "genesis.h"

#define SFX_CH_UI       0
#define SFX_CH_ACTION   1
#define SFX_CH_SCORE    2

void sound_mgr_init(void);
void sound_mgr_update(void);
void sound_mgr_setMatchCrowd(bool enabled);
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
void sound_mgr_crowdGameOver(void);

#endif /* _SOUND_MGR_H_ */
