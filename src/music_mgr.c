#include "music_mgr.h"
#include "fm_synth.h"

#define FM_CH_MELODY  0
#define FM_CH_BASS    1
#define NOTE_REST     0xFF

/* A longer arcade phrase with deliberate breathing space. Bass outlines
 * C-Am-F-G over the first half and adds Dm-G movement in the turnaround. */
static const struct { u8 note; u8 octave; } melody[] = {
    {NOTE_C,4}, {NOTE_E,4}, {NOTE_G,4}, {NOTE_REST,0},
    {NOTE_E,4}, {NOTE_G,4}, {NOTE_C,5}, {NOTE_B,4},
    {NOTE_A,4}, {NOTE_F,4}, {NOTE_A,4}, {NOTE_C,5},
    {NOTE_G,4}, {NOTE_E,4}, {NOTE_D,4}, {NOTE_REST,0},
    {NOTE_C,4}, {NOTE_E,4}, {NOTE_G,4}, {NOTE_A,4},
    {NOTE_C,5}, {NOTE_B,4}, {NOTE_G,4}, {NOTE_E,4},
    {NOTE_F,4}, {NOTE_A,4}, {NOTE_D,5}, {NOTE_C,5},
    {NOTE_B,4}, {NOTE_G,4}, {NOTE_D,4}, {NOTE_REST,0},
};
#define MELODY_LEN (sizeof(melody) / sizeof(melody[0]))

static const struct { u8 note; u8 octave; } bass[] = {
    {NOTE_C,3}, {NOTE_A,2}, {NOTE_F,2}, {NOTE_G,2},
    {NOTE_C,3}, {NOTE_F,2}, {NOTE_D,3}, {NOTE_G,2},
};
#define BASS_LEN (sizeof(bass) / sizeof(bass[0]))

static u8 melodyIndex, bassIndex;
static u16 melodyTimer, bassTimer;
static u16 melodyStep, bassStep;

void music_mgr_init(void)
{
    fm_synth_initChannel(FM_CH_MELODY);
    fm_synth_initBassChannel(FM_CH_BASS);
    melodyIndex = bassIndex = 0;
    melodyTimer = bassTimer = 0;
    /* Same real tempo on 50Hz PAL and 60Hz NTSC hardware. */
    melodyStep = SYS_isPAL() ? 10 : 12;
    bassStep = melodyStep * 4;
}

void music_mgr_update(void)
{
    if (melodyTimer == 0)
    {
        fm_synth_noteOff(FM_CH_MELODY);
        if (melody[melodyIndex].note != NOTE_REST)
            fm_synth_noteOn(FM_CH_MELODY, melody[melodyIndex].note, melody[melodyIndex].octave);
        melodyIndex = (melodyIndex + 1) % MELODY_LEN;
        melodyTimer = melodyStep;
    }
    else
    {
        melodyTimer--;
        if (melodyTimer == 3) fm_synth_noteOff(FM_CH_MELODY);
    }

    if (bassTimer == 0)
    {
        fm_synth_noteOff(FM_CH_BASS);
        fm_synth_noteOn(FM_CH_BASS, bass[bassIndex].note, bass[bassIndex].octave);
        bassIndex = (bassIndex + 1) % BASS_LEN;
        bassTimer = bassStep;
    }
    else
    {
        bassTimer--;
        if (bassTimer == 6) fm_synth_noteOff(FM_CH_BASS);
    }
}
