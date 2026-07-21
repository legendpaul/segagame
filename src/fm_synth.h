/*
 * fm_synth.h - Minimal direct YM2612 FM synth driver.
 *
 * Real 1990s Genesis games score their music through the YM2612 FM
 * chip (PSG is reserved for SFX) - normally authored with a tracker
 * and played back through SGDK's XGM driver. There's no tracker
 * available in this environment, so this hand-programs the chip's
 * registers directly (algorithm/feedback, per-operator envelope, and
 * note frequency) for a simple sustained instrument voice, using
 * SGDK's raw YM2612_writeReg() access. It's a genuine FM voice, not a
 * PSG beep - just a hand-built one-instrument patch instead of a
 * composed multi-instrument score.
 */
#ifndef _FM_SYNTH_H_
#define _FM_SYNTH_H_

#include "genesis.h"

/* Note names as 0-11 semitone offsets from C, for readability at
 * call sites. */
#define NOTE_C   0
#define NOTE_CS  1
#define NOTE_D   2
#define NOTE_DS  3
#define NOTE_E   4
#define NOTE_F   5
#define NOTE_FS  6
#define NOTE_G   7
#define NOTE_GS  8
#define NOTE_A   9
#define NOTE_AS  10
#define NOTE_B   11

/* ch: 0-5, one of the YM2612's 6 FM channels. */
void fm_synth_initChannel(u8 ch);
void fm_synth_initBassChannel(u8 ch);
void fm_synth_noteOn(u8 ch, u8 note, u8 octave);
void fm_synth_noteOff(u8 ch);

#endif /* _FM_SYNTH_H_ */
