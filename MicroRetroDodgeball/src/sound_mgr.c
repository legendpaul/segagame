#include "sound_mgr.h"
#include "audio.h"

/* PCM channels are mixed independently by SGDK's XGM driver.  Keeping the
 * stadium bed, chants, reactions and physical action on separate channels is
 * what lets the match sound like a place rather than a sequence of beeps. */
#define PCM_CH_BED       SOUND_PCM_CH1
#define PCM_CH_REACTION  SOUND_PCM_CH2
#define PCM_CH_CHANT     SOUND_PCM_CH3
#define PCM_CH_ACTION    SOUND_PCM_CH4

#define PCM_ID_CROWD_BED          64
#define PCM_ID_CROWD_CHANT        65
#define PCM_ID_CROWD_THROW        66
#define PCM_ID_CROWD_ELIMINATION  67
#define PCM_ID_CROWD_ROUND        68
#define PCM_ID_CROWD_GAMEOVER     69
#define PCM_ID_THROW              70
#define PCM_ID_PICKUP             71
#define PCM_ID_HIT                72
#define PCM_ID_BOUNCE             73
#define PCM_ID_WHISTLE            74

static u8 envelope[4];
static u8 decay[4];
static u8 active[4];
static u8 priority[4];
static u16 toneFrequency[4];
static s16 pitchSweep[4];

static bool matchCrowd;
static u16 crowdBedTimer;
static u16 chantTimer;

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

static void play_pcm(u8 id, u8 priorityValue, SoundPCMChannel channel)
{
    XGM_startPlayPCM(id, priorityValue, channel);
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

static void register_pcm(void)
{
    XGM_setPCM(PCM_ID_CROWD_BED, crowd_bed, sizeof(crowd_bed));
    XGM_setPCM(PCM_ID_CROWD_CHANT, crowd_chant, sizeof(crowd_chant));
    XGM_setPCM(PCM_ID_CROWD_THROW, crowd_throw, sizeof(crowd_throw));
    XGM_setPCM(PCM_ID_CROWD_ELIMINATION, crowd_elimination,
               sizeof(crowd_elimination));
    XGM_setPCM(PCM_ID_CROWD_ROUND, crowd_round, sizeof(crowd_round));
    XGM_setPCM(PCM_ID_CROWD_GAMEOVER, crowd_gameover,
               sizeof(crowd_gameover));
    XGM_setPCM(PCM_ID_THROW, sfx_throw, sizeof(sfx_throw));
    XGM_setPCM(PCM_ID_PICKUP, sfx_pickup, sizeof(sfx_pickup));
    XGM_setPCM(PCM_ID_HIT, sfx_hit, sizeof(sfx_hit));
    XGM_setPCM(PCM_ID_BOUNCE, sfx_bounce, sizeof(sfx_bounce));
    XGM_setPCM(PCM_ID_WHISTLE, sfx_whistle, sizeof(sfx_whistle));
}

void sound_mgr_init(void)
{
    u8 i;

    PSG_reset();
    Z80_loadDriver(Z80_DRIVER_XGM, TRUE);
    register_pcm();

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
    crowdBedTimer = 0;
    chantTimer = 0;
}

void sound_mgr_setMatchCrowd(bool enabled)
{
    if (matchCrowd == enabled) return;
    matchCrowd = enabled;

    if (enabled)
    {
        crowdBedTimer = 0;
        chantTimer = real_frames((u16)(390 + (random() & 255)));
    }
    else
    {
        XGM_stopPlayPCM(PCM_CH_BED);
        XGM_stopPlayPCM(PCM_CH_CHANT);
        crowdBedTimer = 0;
        chantTimer = 0;
    }
}

void sound_mgr_play(u8 channel, u16 freq, u8 decayStep)
{
    sound_mgr_play_swept(channel, freq, decayStep, 0, 0);
}

static void update_stadium(void)
{
    if (!matchCrowd) return;

    /* Retrigger just before the authored bed ends, giving a continuous broad
     * wash with no silent seam.  Replacing the same low-priority channel is
     * safe even if PAL/NTSC frame timing differs slightly. */
    if (crowdBedTimer == 0)
    {
        play_pcm(PCM_ID_CROWD_BED, 1, PCM_CH_BED);
        crowdBedTimer = real_frames(64);
    }
    else crowdBedTimer--;

    if (chantTimer == 0)
    {
        play_pcm(PCM_ID_CROWD_CHANT, 3, PCM_CH_CHANT);
        chantTimer = real_frames((u16)(420 + (random() & 255)));
    }
    else chantTimer--;
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
    /* Tight broadcast-console tick: quick enough not to smear while cycling
     * rapidly through teams. */
    sound_mgr_play_swept(SFX_CH_UI, 1320, 5, 1, 110);
}

void sound_mgr_confirm(void)
{
    sound_mgr_play_swept(SFX_CH_UI, 988, 2, 2, 34);
    sound_mgr_play_swept(SFX_CH_SCORE, 1480, 3, 2, 22);
}

void sound_mgr_cancel(void)
{
    sound_mgr_play_swept(SFX_CH_UI, 820, 3, 2, -92);
}

void sound_mgr_throw(void)
{
    play_pcm(PCM_ID_THROW, 5, PCM_CH_ACTION);
    play_pcm(PCM_ID_CROWD_THROW, 4, PCM_CH_REACTION);
    sound_mgr_play_swept(SFX_CH_ACTION, varied_frequency(430), 3, 4, 65);
}

void sound_mgr_pickup(void)
{
    play_pcm(PCM_ID_PICKUP, 4, PCM_CH_ACTION);
    sound_mgr_play_swept(SFX_CH_ACTION, 860, 4, 3, 74);
}

void sound_mgr_hit(void)
{
    play_pcm(PCM_ID_HIT, 10, PCM_CH_ACTION);
    sound_mgr_play_swept(SFX_CH_ACTION, varied_frequency(205), 2, 7, -13);
}

void sound_mgr_bounce(void)
{
    play_pcm(PCM_ID_BOUNCE, 3, PCM_CH_ACTION);
    sound_mgr_play_swept(SFX_CH_ACTION, varied_frequency(345), 5, 3, -31);
}

void sound_mgr_whistle(void)
{
    play_pcm(PCM_ID_WHISTLE, 12, PCM_CH_ACTION);
}

void sound_mgr_score(void)
{
    sound_mgr_play_swept(SFX_CH_SCORE, 1568, 1, 7, 31);
    sound_mgr_play_swept(SFX_CH_UI, 2093, 2, 7, 19);
}

void sound_mgr_crowdKnockout(void)
{
    play_pcm(PCM_ID_CROWD_ELIMINATION, 8, PCM_CH_REACTION);
}

void sound_mgr_crowdVictory(void)
{
    play_pcm(PCM_ID_CROWD_ROUND, 12, PCM_CH_REACTION);
}

void sound_mgr_crowdGameOver(void)
{
    /* The final roar is authored longest and has absolute reaction priority.
     * The quiet bed is stopped so the climax retains headroom. */
    XGM_stopPlayPCM(PCM_CH_BED);
    XGM_stopPlayPCM(PCM_CH_CHANT);
    play_pcm(PCM_ID_CROWD_GAMEOVER, 15, PCM_CH_REACTION);
}
