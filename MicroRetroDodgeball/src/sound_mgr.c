#include "sound_mgr.h"
#include "audio.h"

#define PCM_CH_MUSIC     SOUND_PCM_CH1
#define PCM_CH_AMBIENCE  SOUND_PCM_CH2
#define PCM_CH_EVENT     SOUND_PCM_CH3
#define PCM_CH_ACTION    SOUND_PCM_CH4

static u8 envelope[4];
static u8 decay[4];
static u8 active[4];
static u8 priority[4];
static u16 toneFrequency[4];
static s16 pitchSweep[4];

static bool matchCrowd;
static u8 ambienceState;
static u16 ambienceTimer;

static const s16 pitchVariation[4] = { 0, 28, -20, 13 };

static u16 real_frames(u16 ntscFrames)
{
    return SYS_isPAL() ? (u16)((ntscFrames * 5) / 6) : ntscFrames;
}

static u16 varied_frequency(u16 base)
{
    s16 varied = (s16)base + pitchVariation[random() & 3];
    return (u16)((varied < 32) ? 32 : varied);
}

static void sound_mgr_play_swept(u8 channel, u16 freq, u8 decayStep,
                                 u8 newPriority, s16 sweep)
{
    if (active[channel] && priority[channel] > newPriority) return;

    PSG_setFrequency(channel, freq);
    PSG_setEnvelope(channel, PSG_ENVELOPE_MAX);
    envelope[channel] = PSG_ENVELOPE_MAX;
    decay[channel] = decayStep;
    active[channel] = 1;
    priority[channel] = newPriority;
    toneFrequency[channel] = freq;
    pitchSweep[channel] = sweep;
}

void sound_mgr_init(void)
{
    u8 i;

    PSG_reset();
    SND_PCM4_loadDriver(TRUE);
    SND_PCM4_setVolume(SOUND_PCM_CH1, 12); /* music */
    SND_PCM4_setVolume(SOUND_PCM_CH2, 7);  /* ambience: deliberately quiet */
    SND_PCM4_setVolume(SOUND_PCM_CH3, 14); /* goal/win crowd */
    SND_PCM4_setVolume(SOUND_PCM_CH4, 15); /* whistle and action SFX */

    for (i = 0; i < 4; i++)
    {
        envelope[i] = PSG_ENVELOPE_MIN;
        decay[i] = 0;
        active[i] = 0;
        priority[i] = 0;
        toneFrequency[i] = 0;
        pitchSweep[i] = 0;
    }
    matchCrowd = FALSE;
    ambienceState = 0;
    ambienceTimer = 0;
}

static void play_ambience(void)
{
    const u8 *data = CROWD_NORMAL_01;
    u32 len = sizeof(CROWD_NORMAL_01);

    if (ambienceState == 1)
    {
        data = CROWD_NORMAL_02;
        len = sizeof(CROWD_NORMAL_02);
    }
    else if (ambienceState == 2)
    {
        data = CROWD_CHANT;
        len = sizeof(CROWD_CHANT);
    }
    else if (ambienceState == 3)
    {
        data = CROWD_NORMAL_03;
        len = sizeof(CROWD_NORMAL_03);
    }

    SND_PCM4_startPlay(data, len, PCM_CH_AMBIENCE, TRUE);
}

void sound_mgr_setMatchCrowd(bool enabled)
{
    if (matchCrowd == enabled) return;
    matchCrowd = enabled;

    if (enabled)
    {
        ambienceState = 0;
        play_ambience();
        ambienceTimer = real_frames(900); /* 15 seconds */
    }
    else
    {
        SND_PCM4_stopPlay(PCM_CH_AMBIENCE);
        ambienceState = 0;
        ambienceTimer = 0;
    }
}

void sound_mgr_play(u8 channel, u16 freq, u8 decayStep)
{
    sound_mgr_play_swept(channel, freq, decayStep, 0, 0);
}

static void update_stadium(void)
{
    if (!matchCrowd) return;

    if (ambienceTimer == 0)
    {
        ambienceState = (ambienceState + 1) % 4;
        play_ambience();
        ambienceTimer = real_frames(900); /* 15 seconds */
    }
    else
    {
        ambienceTimer--;
    }
}

void sound_mgr_update(void)
{
    u8 i;

    update_stadium();
    for (i = 0; i < 4; i++)
    {
        if (!active[i]) continue;

        if (envelope[i] + decay[i] >= PSG_ENVELOPE_MIN)
        {
            envelope[i] = PSG_ENVELOPE_MIN;
            active[i] = 0;
            priority[i] = 0;
            pitchSweep[i] = 0;
        }
        else envelope[i] += decay[i];

        if (i < 3 && active[i] && pitchSweep[i] != 0)
        {
            s32 next = (s32)toneFrequency[i] + pitchSweep[i];
            if (next < 50) next = 50;
            if (next > 4000) next = 4000;
            toneFrequency[i] = (u16)next;
            PSG_setFrequency(i, toneFrequency[i]);
        }
        PSG_setEnvelope(i, envelope[i]);
    }
}

void sound_mgr_blip(void)
{
    /* Tight broadcast-console tick + new move swipe */
    SND_PCM4_startPlay(SFX_MENU_MOVE, sizeof(SFX_MENU_MOVE), PCM_CH_ACTION, FALSE);
    sound_mgr_play_swept(SFX_CH_UI, 1320, 5, 1, 110);
}

void sound_mgr_confirm(void)
{
    SND_PCM4_startPlay(SFX_MENU_SELECT, sizeof(SFX_MENU_SELECT), PCM_CH_ACTION, FALSE);
    sound_mgr_play_swept(SFX_CH_UI, 988, 2, 2, 34);
    sound_mgr_play_swept(SFX_CH_SCORE, 1480, 3, 2, 22);
}

void sound_mgr_cancel(void)
{
    sound_mgr_play_swept(SFX_CH_UI, 820, 3, 2, -92);
}

void sound_mgr_throw(void)
{
    SND_PCM4_startPlay(SFX_THROW, sizeof(SFX_THROW), PCM_CH_ACTION, FALSE);
    sound_mgr_play_swept(SFX_CH_ACTION, varied_frequency(430), 3, 4, 65);
}

void sound_mgr_pickup(void)
{
    SND_PCM4_startPlay(SFX_PICKUP, sizeof(SFX_PICKUP), PCM_CH_ACTION, FALSE);
    sound_mgr_play_swept(SFX_CH_ACTION, 860, 4, 3, 74);
}

void sound_mgr_hit(void)
{
    SND_PCM4_startPlay(SFX_HIT, sizeof(SFX_HIT), PCM_CH_ACTION, FALSE);
    sound_mgr_play_swept(SFX_CH_ACTION, varied_frequency(205), 2, 7, -13);
}

void sound_mgr_bounce(void)
{
    SND_PCM4_startPlay(SFX_BOUNCE, sizeof(SFX_BOUNCE), PCM_CH_ACTION, FALSE);
    sound_mgr_play_swept(SFX_CH_ACTION, varied_frequency(345), 5, 3, -31);
}

void sound_mgr_whistle(void)
{
    SND_PCM4_startPlay(SFX_WHISTLE, sizeof(SFX_WHISTLE), PCM_CH_ACTION, FALSE);
}

void sound_mgr_score(void)
{
    sound_mgr_play_swept(SFX_CH_SCORE, 1568, 1, 7, 31);
    sound_mgr_play_swept(SFX_CH_UI, 2093, 2, 7, 19);
}

void sound_mgr_crowdKnockout(void)
{
    SND_PCM4_startPlay(CROWD_GOAL, sizeof(CROWD_GOAL), PCM_CH_EVENT, FALSE);
}

void sound_mgr_crowdVictory(void)
{
    SND_PCM4_startPlay(CROWD_MATCH_WIN, sizeof(CROWD_MATCH_WIN), PCM_CH_EVENT, FALSE);
}

void sound_mgr_crowdGameOver(void)
{
    /* The final roar is authored longest and has absolute reaction priority.
     * The quiet bed is stopped so the climax retains headroom. */
    SND_PCM4_stopPlay(PCM_CH_AMBIENCE);
    SND_PCM4_startPlay(CROWD_WORLD_CUP_WIN, sizeof(CROWD_WORLD_CUP_WIN), PCM_CH_EVENT, FALSE);
}
