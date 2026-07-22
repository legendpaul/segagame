#include "sound_mgr.h"

static u8 envelope[4];
static u8 decay[4];
static u8 active[4];
static u8 priority[4];

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
    if (active[3] && priority[3] > newPriority) return;
    PSG_setNoise(PSG_NOISE_TYPE_WHITE, noiseFreq);
    PSG_setEnvelope(3, PSG_ENVELOPE_MAX);
    envelope[3] = PSG_ENVELOPE_MAX;
    decay[3] = decayStep;
    active[3] = 1;
    priority[3] = newPriority;
}

void sound_mgr_update(void)
{
    u8 i;
    for (i = 0; i < 4; i++)
    {
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

/* Short, deliberately lo-fi crowd layers. They stay below contact and
 * whistle priority, so atmosphere never masks a gameplay-critical cue. */
void sound_mgr_crowdKnockout(void)
{
    sound_mgr_noise(PSG_NOISE_FREQ_CLOCK8, 1, 2);
}

void sound_mgr_crowdVictory(void)
{
    sound_mgr_noise(PSG_NOISE_FREQ_CLOCK4, 1, 5);
}
