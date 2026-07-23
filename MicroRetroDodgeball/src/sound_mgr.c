#include "sound_mgr.h"

static u8 envelope[4];
static u8 decay[4];
static u8 active[4];
static u8 priority[4];

/* Sustained crowd-roar state (2026-07-22). The old crowd cues were a
 * single one-frame PSG noise blip that was over before you registered it.
 * A real crowd is a swell: it rises, holds with a churning texture, then
 * falls over roughly a second. crowdTimer counts DOWN the frames left;
 * crowdLen is the full length so the update can tell where in the
 * attack/sustain/release shape it is; crowdPeak is the loudest PSG
 * attenuation reached (LOWER = LOUDER, since PSG env 0 = full, 15 =
 * silent). It owns the shared noise channel (3) for its whole duration. */
static u16 crowdTimer;
static u16 crowdLen;
static u8  crowdPeak;
static u16 crowdSeed = 0x1234;

/* Four deterministic-sized offsets avoid identical machine-gun retriggers
 * without adding a large random range that would make the ball sound comedic. */
static const s16 pitchVariation[4] = { 0, 32, -24, 16 };

static u16 varied_frequency(u16 base)
{
    s16 varied = (s16)base + pitchVariation[random() & 3];
    return (u16)((varied < 32) ? 32 : varied);
}

static void sound_mgr_play_priority(u8 channel, u16 freq, u8 decayStep, u8 newPriority)
{
    if (active[channel] && priority[channel] > newPriority) return;

    PSG_setFrequency(channel, freq);
    PSG_setEnvelope(channel, PSG_ENVELOPE_MAX);
    envelope[channel] = PSG_ENVELOPE_MAX;
    decay[channel] = decayStep;
    active[channel] = 1;
    priority[channel] = newPriority;
}

void sound_mgr_init(void)
{
    PSG_reset();
    u8 i;
    for (i = 0; i < 4; i++)
    {
        envelope[i] = PSG_ENVELOPE_MIN;
        decay[i] = 0;
        active[i] = 0;
        priority[i] = 0;
    }
}

void sound_mgr_play(u8 channel, u16 freq, u8 decayStep)
{
    sound_mgr_play_priority(channel, freq, decayStep, 0);
}

static void sound_mgr_noise(u8 noiseFreq, u8 decayStep, u8 newPriority)
{
    /* A crowd swell owns the noise channel for its whole length; a stray
     * hit/bounce blip mid-cheer would just chop a hole in the roar. These
     * cheers only fire at elimination/round-end/game-over moments when
     * little else is happening, so nothing gameplay-critical is lost. */
    if (crowdTimer > 0) return;
    if (active[3] && priority[3] > newPriority) return;
    PSG_setNoise(PSG_NOISE_TYPE_WHITE, noiseFreq);
    PSG_setEnvelope(3, PSG_ENVELOPE_MAX);
    envelope[3] = PSG_ENVELOPE_MAX;
    decay[3] = decayStep;
    active[3] = 1;
    priority[3] = newPriority;
}

/* Start (or upgrade) a crowd roar. A quieter cheer cannot stomp a louder
 * one already ringing - so a single knockout during a round-win swell
 * won't cut the bigger celebration short. */
static void sound_mgr_crowd_start(u16 len, u8 peak)
{
    if (crowdTimer > 0 && crowdPeak < peak) return;  /* keep the louder cheer */
    crowdTimer = len;
    crowdLen = len;
    crowdPeak = peak;
}

/* Drive the crowd channel one frame along its attack/sustain/release
 * envelope, with a churning texture so it reads as a stadium roar rather
 * than a flat hiss. Called once per frame from sound_mgr_update(). */
static void sound_mgr_crowd_update(void)
{
    u16 pos, atk, rel;
    u8 env, wob;

    if (crowdTimer == 0) return;

    pos = crowdLen - crowdTimer;   /* frames elapsed so far */
    atk = crowdLen / 5;            /* ~20% fade-in */
    rel = crowdLen / 2;            /* last ~50% fades back out */
    if (atk == 0) atk = 1;
    if (rel == 0) rel = 1;

    if (pos < atk)
        env = 15 - (u8)(((15 - crowdPeak) * pos) / atk);       /* rise */
    else if (crowdTimer < rel)
        env = crowdPeak + (u8)(((15 - crowdPeak) * (rel - crowdTimer)) / rel); /* fall */
    else
        env = crowdPeak;                                        /* hold */

    /* Cheap LCG for a per-frame loudness flutter + churning noise pitch. */
    crowdSeed = (u16)(crowdSeed * 41 + 13);
    wob = (crowdSeed >> 9) & 1;
    if (env + wob <= 15) env += wob;

    if ((crowdTimer & 7) == 0)
        PSG_setNoise(PSG_NOISE_TYPE_WHITE,
                     (crowdSeed & 1) ? PSG_NOISE_FREQ_CLOCK4 : PSG_NOISE_FREQ_CLOCK8);
    PSG_setEnvelope(3, env);

    crowdTimer--;
    if (crowdTimer == 0)
    {
        PSG_setEnvelope(3, PSG_ENVELOPE_MIN);
        active[3] = 0;
        priority[3] = 0;
    }
}

void sound_mgr_update(void)
{
    u8 i;
    sound_mgr_crowd_update();
    for (i = 0; i < 4; i++)
    {
        /* The crowd swell drives the noise channel itself while it runs. */
        if (i == 3 && crowdTimer > 0) continue;
        if (!active[i]) continue;

        if (envelope[i] + decay[i] >= PSG_ENVELOPE_MIN)
        {
            envelope[i] = PSG_ENVELOPE_MIN;
            active[i] = 0;
            priority[i] = 0;
        }
        else
        {
            envelope[i] += decay[i];
        }

        PSG_setEnvelope(i, envelope[i]);
    }
}

void sound_mgr_blip(void)   { sound_mgr_play_priority(SFX_CH_UI, 1800, 3, 1); }
void sound_mgr_confirm(void){ sound_mgr_play_priority(SFX_CH_UI, 1200, 2, 2); }
void sound_mgr_cancel(void) { sound_mgr_play_priority(SFX_CH_UI, 620, 3, 2); }
void sound_mgr_throw(void)  { sound_mgr_play_priority(SFX_CH_ACTION, varied_frequency(780), 2, 4); }
void sound_mgr_pickup(void) { sound_mgr_play_priority(SFX_CH_ACTION, 1320, 2, 4); }
void sound_mgr_hit(void)
{
    sound_mgr_play_priority(SFX_CH_ACTION, varied_frequency(300), 1, 6);
    sound_mgr_noise(PSG_NOISE_FREQ_CLOCK4, 2, 6);
}
void sound_mgr_bounce(void)
{
    sound_mgr_play_priority(SFX_CH_ACTION, varied_frequency(460), 3, 3);
    sound_mgr_noise(PSG_NOISE_FREQ_CLOCK8, 3, 3);
}
void sound_mgr_whistle(void){ sound_mgr_play_priority(SFX_CH_SCORE, 1700, 1, 7); }
void sound_mgr_score(void)  { sound_mgr_play_priority(SFX_CH_SCORE, 2200, 1, 6); }

/* Sustained crowd roars (2026-07-22), shaped by sound_mgr_crowd_update().
 * Longer + louder = bigger moment. peak is PSG attenuation at the crest,
 * LOWER = LOUDER (0 full .. 15 silent). Tuned so a single knockout is an
 * appreciative swell (~0.9s), a round win is a real celebration (~1.5s),
 * and the match win is the biggest roar of all (~2.5s). */
void sound_mgr_crowdKnockout(void)
{
    sound_mgr_crowd_start(54, 6);
}

void sound_mgr_crowdVictory(void)
{
    sound_mgr_crowd_start(90, 2);
}

void sound_mgr_crowdGameOver(void)
{
    sound_mgr_crowd_start(150, 0);
}
