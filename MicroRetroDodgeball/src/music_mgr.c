#include "music_mgr.h"
#include "fm_synth.h"
#include "game_state.h"
#include "sound_mgr.h"

/* YM channel 5 is intentionally unused: it is the DAC channel carrying the
 * four-channel XGM crowd/effect mix. */
#define FM_CH_LEAD        0
#define FM_CH_BASS        1
#define FM_CH_RIFF        2
#define FM_CH_CHORD       3
#define FM_CH_RHYTHM      4
#define FM_CHANNELS_USED  5
#define NOTE_REST         0xFF

typedef struct { u8 note, octave; } MusicNote;
typedef struct { u8 root, octave, minor; } MusicChord;
typedef enum { MUSIC_ROCK = 0, MUSIC_SPORTS } MusicStyle;
typedef struct {
    const MusicNote *melody;
    u8 melodyLen;
    const MusicChord *chords;
    u8 chordLen;
    u8 tempoPAL;
    u8 tempoNTSC;
    MusicStyle style;
} MusicTrack;

#define COUNT_OF(a) ((u8)(sizeof(a) / sizeof((a)[0])))
#define N(note, oct) { note, oct }
#define R            { NOTE_REST, 0 }
#define CH(root, oct, isMinor) { root, oct, isMinor }

/* Original distorted-rock title cue.  The compact E-minor hook, octave
 * answers, power-chord bed and hard half-time accents evoke a loud 1990s
 * alternative-rock intro without reproducing the melody of any recording. */
static const MusicNote titleMelody[] = {
    N(NOTE_E,4), N(NOTE_E,4), N(NOTE_G,4), N(NOTE_A,4),
    R,           N(NOTE_G,4), N(NOTE_E,4), N(NOTE_D,4),
    N(NOTE_E,4), N(NOTE_B,4), N(NOTE_A,4), N(NOTE_G,4),
    N(NOTE_E,4), R,           N(NOTE_D,4), N(NOTE_E,4),
    N(NOTE_E,5), N(NOTE_D,5), N(NOTE_B,4), N(NOTE_A,4),
    N(NOTE_G,4), N(NOTE_A,4), N(NOTE_B,4), R,
    N(NOTE_C,5), N(NOTE_B,4), N(NOTE_G,4), N(NOTE_E,4),
    N(NOTE_D,4), N(NOTE_FS,4),N(NOTE_A,4), R,
    N(NOTE_E,4), N(NOTE_G,4), N(NOTE_B,4), N(NOTE_E,5),
    N(NOTE_D,5), N(NOTE_B,4), N(NOTE_A,4), N(NOTE_G,4),
    N(NOTE_A,4), R,           N(NOTE_C,5), N(NOTE_A,4),
    N(NOTE_B,4), N(NOTE_G,4), N(NOTE_FS,4),N(NOTE_E,4)
};
static const MusicChord titleChords[] = {
    CH(NOTE_E,2,1), CH(NOTE_G,2,0), CH(NOTE_C,3,0), CH(NOTE_D,3,0),
    CH(NOTE_E,2,1), CH(NOTE_C,3,0), CH(NOTE_A,2,1), CH(NOTE_B,2,0),
    CH(NOTE_E,2,1), CH(NOTE_G,2,0), CH(NOTE_D,3,0), CH(NOTE_A,2,1)
};

/* Original Sega-CD-era sports menu theme: syncopated C-minor bass movement,
 * bright brass answers, suspended chord stabs and a busier second phrase.
 * It is used for setup, team select, matchup and every tournament board. */
static const MusicNote sportsMelody[] = {
    N(NOTE_G,4), R,           N(NOTE_C,5), N(NOTE_DS,5),
    N(NOTE_D,5), N(NOTE_C,5), R,           N(NOTE_G,4),
    N(NOTE_GS,4),N(NOTE_C,5), N(NOTE_DS,5),R,
    N(NOTE_C,5), N(NOTE_AS,4),N(NOTE_G,4), R,
    N(NOTE_DS,5),N(NOTE_F,5), N(NOTE_G,5), N(NOTE_DS,5),
    R,           N(NOTE_C,5), N(NOTE_AS,4),N(NOTE_G,4),
    N(NOTE_F,4), N(NOTE_G,4), N(NOTE_AS,4),N(NOTE_C,5),
    N(NOTE_D,5), R,           N(NOTE_G,4), R,
    N(NOTE_C,5), N(NOTE_DS,5),N(NOTE_G,5), R,
    N(NOTE_F,5), N(NOTE_DS,5),N(NOTE_C,5), N(NOTE_G,4),
    N(NOTE_GS,4),R,           N(NOTE_C,5), N(NOTE_F,5),
    N(NOTE_DS,5),N(NOTE_C,5), N(NOTE_AS,4),R,
    N(NOTE_G,4), N(NOTE_AS,4),N(NOTE_D,5), N(NOTE_F,5),
    N(NOTE_DS,5),N(NOTE_D,5), N(NOTE_C,5), R,
    N(NOTE_GS,4),N(NOTE_C,5), N(NOTE_DS,5),N(NOTE_G,5),
    N(NOTE_F,5), N(NOTE_D,5), N(NOTE_G,4), R
};
static const MusicChord sportsChords[] = {
    CH(NOTE_C,3,1), CH(NOTE_GS,2,0), CH(NOTE_DS,3,0), CH(NOTE_AS,2,0),
    CH(NOTE_F,2,1), CH(NOTE_G,2,0),  CH(NOTE_GS,2,0), CH(NOTE_AS,2,0),
    CH(NOTE_C,3,1), CH(NOTE_DS,3,0), CH(NOTE_F,2,1),  CH(NOTE_G,2,0),
    CH(NOTE_GS,2,0),CH(NOTE_AS,2,0), CH(NOTE_G,2,0),  CH(NOTE_C,3,1)
};

static const MusicTrack trackTitle = {
    titleMelody, COUNT_OF(titleMelody), titleChords, COUNT_OF(titleChords),
    5, 6, MUSIC_ROCK
};
static const MusicTrack trackSports = {
    sportsMelody, COUNT_OF(sportsMelody), sportsChords,
    COUNT_OF(sportsChords), 6, 7, MUSIC_SPORTS
};

static const MusicTrack *track;
static bool titleContext = TRUE;
static u8 lastScene;
static bool lastTitleContext;
static u8 melodyIndex, chordIndex, riffIndex, rhythmIndex;
static u16 melodyTimer, chordTimer, riffTimer, rhythmTimer;
static u16 melodyStep, chordStep, riffStep;

static MusicNote chord_tone(const MusicChord *chord, u8 interval)
{
    MusicNote result;
    u8 value = chord->root + interval;
    result.note = value % 12;
    result.octave = chord->octave + value / 12;
    return result;
}

static const MusicTrack *track_for_scene(u8 scene)
{
    if (scene == GS_MENU) return titleContext ? &trackTitle : &trackSports;
    /* Matches and results deliberately have no score: crowd, chants and
     * physical effects own the full mix and carry the drama. */
    return NULL;
}

static void stop_all_notes(void)
{
    u8 ch;
    for (ch = 0; ch < FM_CHANNELS_USED; ch++) fm_synth_noteOff(ch);
}

static void configure_band(MusicStyle style)
{
    fm_synth_initBassChannel(FM_CH_BASS);
    fm_synth_initPercussionChannel(FM_CH_RHYTHM);

    if (style == MUSIC_ROCK)
    {
        fm_synth_initRockChannel(FM_CH_LEAD, FALSE);
        fm_synth_initRockChannel(FM_CH_RIFF, TRUE);
        fm_synth_initRockChannel(FM_CH_CHORD, TRUE);
    }
    else
    {
        fm_synth_initSportsBrassChannel(FM_CH_LEAD, FALSE);
        fm_synth_initSportsBrassChannel(FM_CH_RIFF, TRUE);
        fm_synth_initPadChannel(FM_CH_CHORD, TRUE);
    }
}

static void begin_scene_score(u8 scene)
{
    stop_all_notes();
    sound_mgr_setMatchCrowd(scene == GS_MATCH || scene == GS_ELIMINATOR);
    track = track_for_scene(scene);
    melodyIndex = chordIndex = riffIndex = rhythmIndex = 0;
    melodyTimer = chordTimer = riffTimer = rhythmTimer = 0;

    if (track != NULL)
    {
        configure_band(track->style);
        melodyStep = SYS_isPAL() ? track->tempoPAL : track->tempoNTSC;
        chordStep = melodyStep * 4;
        riffStep = melodyStep / 2;
        if (riffStep == 0) riffStep = 1;
    }

    lastScene = scene;
    lastTitleContext = titleContext;
}

void music_mgr_init(void)
{
    track = NULL;
    lastScene = 0xFF;
    lastTitleContext = !titleContext;
}

void music_mgr_setMenuContext(bool titleScreen)
{
    titleContext = titleScreen;
}

static void update_melody(void)
{
    if (melodyTimer == 0)
    {
        const MusicNote *note = &track->melody[melodyIndex];
        fm_synth_noteOff(FM_CH_LEAD);
        if (note->note != NOTE_REST)
            fm_synth_noteOn(FM_CH_LEAD, note->note, note->octave);
        melodyIndex = (melodyIndex + 1) % track->melodyLen;
        melodyTimer = melodyStep;
    }
    else
    {
        melodyTimer--;
        if (melodyTimer == 2) fm_synth_noteOff(FM_CH_LEAD);
    }
}

static void update_chord(void)
{
    if (chordTimer == 0)
    {
        const MusicChord *chord = &track->chords[chordIndex];
        MusicNote root = chord_tone(chord, 0);
        MusicNote colour = chord_tone(chord,
            track->style == MUSIC_ROCK ? 7 : (chord->minor ? 3 : 4));

        fm_synth_noteOff(FM_CH_BASS);
        fm_synth_noteOff(FM_CH_CHORD);
        fm_synth_noteOn(FM_CH_BASS, root.note, root.octave);
        fm_synth_noteOn(FM_CH_CHORD, colour.note, colour.octave + 1);
        chordIndex = (chordIndex + 1) % track->chordLen;
        chordTimer = chordStep;
    }
    else
    {
        chordTimer--;
        if (chordTimer == (track->style == MUSIC_ROCK ? 4 : 7))
        {
            fm_synth_noteOff(FM_CH_BASS);
            fm_synth_noteOff(FM_CH_CHORD);
        }
    }
}

static void update_riff(void)
{
    static const u8 rockShape[8] = { 0, 0, 7, 0, 12, 7, 0, 7 };
    static const u8 sportShape[8] = { 0, 7, 3, 7, 0, 10, 7, 3 };
    const MusicChord *chord;
    MusicNote note;
    u8 interval;

    if (riffTimer > 0)
    {
        riffTimer--;
        if (riffTimer == 1) fm_synth_noteOff(FM_CH_RIFF);
        return;
    }

    chord = &track->chords[(riffIndex / 8) % track->chordLen];
    interval = track->style == MUSIC_ROCK
             ? rockShape[riffIndex & 7]
             : sportShape[riffIndex & 7];
    if (track->style == MUSIC_SPORTS && interval == 3 && !chord->minor)
        interval = 4;
    note = chord_tone(chord, interval);
    fm_synth_noteOff(FM_CH_RIFF);
    fm_synth_noteOn(FM_CH_RIFF, note.note,
                    note.octave + (track->style == MUSIC_ROCK ? 1 : 2));
    riffIndex = (riffIndex + 1) % (track->chordLen * 8);
    riffTimer = riffStep;
}

static void update_rhythm(void)
{
    u8 beat = rhythmIndex & 15;
    bool play;
    u8 note;
    u8 octave;

    if (rhythmTimer > 0)
    {
        rhythmTimer--;
        if (rhythmTimer == 1) fm_synth_noteOff(FM_CH_RHYTHM);
        return;
    }

    fm_synth_noteOff(FM_CH_RHYTHM);
    if (track->style == MUSIC_ROCK)
    {
        play = (beat == 0 || beat == 4 || beat == 8 || beat == 10 ||
                beat == 12 || beat == 14);
        note = (beat == 0 || beat == 8) ? NOTE_E : NOTE_B;
        octave = (beat == 0 || beat == 8) ? 1 : 3;
    }
    else
    {
        play = (beat == 0 || beat == 3 || beat == 6 || beat == 8 ||
                beat == 11 || beat == 14);
        note = (beat == 0 || beat == 8) ? NOTE_C
             : (beat == 6 || beat == 14) ? NOTE_G : NOTE_DS;
        octave = (beat == 0 || beat == 8) ? 2 : 3;
    }

    if (play) fm_synth_noteOn(FM_CH_RHYTHM, note, octave);
    rhythmIndex++;
    rhythmTimer = riffStep;
}

void music_mgr_update(void)
{
    if (lastScene != (u8)gCurrentScene || lastTitleContext != titleContext)
        begin_scene_score((u8)gCurrentScene);
    if (track == NULL) return;

    update_melody();
    update_chord();
    update_riff();
    update_rhythm();
}
