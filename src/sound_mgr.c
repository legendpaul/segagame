#include "sound_mgr.h"

static u8 envelope[4];
static u8 decay[4];
static u8 active[4];

void sound_mgr_init(void)
{
    PSG_reset();
    u8 i;
    for (i = 0; i < 4; i++)
    {
        envelope[i] = PSG_ENVELOPE_MIN;
        decay[i] = 0;
        active[i] = 0;
    }
}

void sound_mgr_play(u8 channel, u16 freq, u8 decayStep)
{
    PSG_setFrequency(channel, freq);
    PSG_setEnvelope(channel, PSG_ENVELOPE_MAX);
    envelope[channel] = PSG_ENVELOPE_MAX;
    decay[channel] = decayStep;
    active[channel] = 1;
}

static void sound_mgr_noise(u8 noiseFreq, u8 decayStep)
{
    PSG_setNoise(PSG_NOISE_TYPE_WHITE, noiseFreq);
    PSG_setEnvelope(3, PSG_ENVELOPE_MAX);
    envelope[3] = PSG_ENVELOPE_MAX;
    decay[3] = decayStep;
    active[3] = 1;
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
        }
        else
        {
            envelope[i] += decay[i];
        }

        PSG_setEnvelope(i, envelope[i]);
    }
}

void sound_mgr_blip(void)  { sound_mgr_play(SFX_CH_UI, 1800, 3); }
void sound_mgr_confirm(void) { sound_mgr_play(SFX_CH_UI, 1200, 2); }
void sound_mgr_cancel(void)  { sound_mgr_play(SFX_CH_UI, 620, 3); }
void sound_mgr_throw(void) { sound_mgr_play(SFX_CH_ACTION, 780, 2); }
void sound_mgr_catch(void) { sound_mgr_play(SFX_CH_ACTION, 1500, 2); }
void sound_mgr_hit(void)   { sound_mgr_play(SFX_CH_ACTION, 300, 1); sound_mgr_noise(PSG_NOISE_FREQ_CLOCK4, 2); }
void sound_mgr_bounce(void){ sound_mgr_play(SFX_CH_ACTION, 460, 3); sound_mgr_noise(PSG_NOISE_FREQ_CLOCK8, 3); }
void sound_mgr_whistle(void){ sound_mgr_play(SFX_CH_SCORE, 1700, 1); }
void sound_mgr_score(void) { sound_mgr_play(SFX_CH_SCORE, 2200, 1); }
