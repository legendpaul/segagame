#include "sound_mgr.h"

static u8 envelope[3];
static u8 decay[3];
static u8 active[3];

void sound_mgr_init(void)
{
    PSG_reset();
    u8 i;
    for (i = 0; i < 3; i++)
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

void sound_mgr_update(void)
{
    u8 i;
    for (i = 0; i < 3; i++)
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
void sound_mgr_throw(void) { sound_mgr_play(SFX_CH_ACTION, 900, 2); }
void sound_mgr_catch(void) { sound_mgr_play(SFX_CH_ACTION, 1500, 2); }
void sound_mgr_hit(void)   { sound_mgr_play(SFX_CH_ACTION, 300, 1); }
void sound_mgr_score(void) { sound_mgr_play(SFX_CH_SCORE, 2200, 1); }
