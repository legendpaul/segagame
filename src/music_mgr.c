#include "music_mgr.h"
#include "sound_mgr.h"

/* Reuses the SCORE sfx channel - it's silent while browsing the menu
 * (score sfx only ever plays on the game-over screen), so the melody
 * doesn't fight with the menu's own move/select blips on SFX_CH_UI. */
#define MUSIC_CH    SFX_CH_SCORE
#define NOTE_LEN    20   /* frames per note (~0.33s at 60fps) */

/* A short major arpeggio loop: C4 E4 G4 C5 G4 E4 D4 G3 */
static const u16 notes[] = { 262, 330, 392, 523, 392, 330, 294, 196 };
#define NUM_NOTES   (sizeof(notes) / sizeof(notes[0]))

static u8   noteIndex;
static u16  noteTimer;
static bool playing;

void music_mgr_start(void)
{
    noteIndex = 0;
    noteTimer = 0;
    playing = TRUE;
}

void music_mgr_stop(void)
{
    playing = FALSE;
}

void music_mgr_update(void)
{
    if (!playing) return;

    if (noteTimer == 0)
    {
        /* decayStep 1 = a slow, soft fade (versus the punchier SFX
         * decay steps), so each note rings out gently like a pluck
         * instead of a sharp beep. */
        sound_mgr_play(MUSIC_CH, notes[noteIndex], 1);
        noteIndex = (noteIndex + 1) % NUM_NOTES;
        noteTimer = NOTE_LEN;
    }
    else
    {
        noteTimer--;
    }
}
