#include "music_mgr.h"
#include "fm_synth.h"

#define FM_CH_MELODY   0
#define FM_CH_BASS     1

#define MELODY_NOTE_LEN   24   /* frames per melody note (~0.4s at 60fps) */
#define BASS_NOTE_LEN     (MELODY_NOTE_LEN * 2)

/* Simple I-vi-IV-V-ish loop: melody outlines a C major arpeggio and
 * back down, bass holds root notes underneath at half the rate. Both
 * arrays are timed to complete a full loop together (8 * 24 == 4 * 48). */
static const struct { u8 note; u8 octave; } melody[] = {
    { NOTE_C, 4 }, { NOTE_E, 4 }, { NOTE_G, 4 }, { NOTE_E, 4 },
    { NOTE_F, 4 }, { NOTE_A, 4 }, { NOTE_G, 4 }, { NOTE_E, 4 },
};
#define MELODY_LEN  (sizeof(melody) / sizeof(melody[0]))

static const struct { u8 note; u8 octave; } bass[] = {
    { NOTE_C, 3 }, { NOTE_C, 3 }, { NOTE_F, 3 }, { NOTE_G, 3 },
};
#define BASS_LEN  (sizeof(bass) / sizeof(bass[0]))

static u8  melodyIndex, bassIndex;
static u16 melodyTimer, bassTimer;

void music_mgr_init(void)
{
    fm_synth_initChannel(FM_CH_MELODY);
    fm_synth_initChannel(FM_CH_BASS);

    melodyIndex = 0;
    bassIndex = 0;
    melodyTimer = 0;
    bassTimer = 0;
}

void music_mgr_update(void)
{
    if (melodyTimer == 0)
    {
        fm_synth_noteOn(FM_CH_MELODY, melody[melodyIndex].note, melody[melodyIndex].octave);
        melodyIndex = (melodyIndex + 1) % MELODY_LEN;
        melodyTimer = MELODY_NOTE_LEN;
    }
    else
    {
        melodyTimer--;
    }

    if (bassTimer == 0)
    {
        fm_synth_noteOn(FM_CH_BASS, bass[bassIndex].note, bass[bassIndex].octave);
        bassIndex = (bassIndex + 1) % BASS_LEN;
        bassTimer = BASS_NOTE_LEN;
    }
    else
    {
        bassTimer--;
    }
}
