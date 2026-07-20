#include "fm_synth.h"

/* F-Num values for a block/octave of 4 (C..B), the standard reference
 * table used across Genesis homebrew FM code. Shifted left/right via
 * the "octave" (block) parameter to reach other registers. */
static const u16 noteFnum[12] = {
    644, 681, 722, 765, 810, 858, 910, 964, 1022, 1083, 1148, 1216
};

static void writeOp(u8 part, u8 chOff, u8 opOff,
                     u8 mul, u8 dt, u8 tl, u8 ar, u8 d1r, u8 d2r, u8 sl, u8 rr)
{
    u8 base = chOff + opOff;

    YM2612_writeReg(part, 0x30 + base, (dt << 4) | (mul & 0x0F));
    YM2612_writeReg(part, 0x40 + base, tl & 0x7F);
    YM2612_writeReg(part, 0x50 + base, ar & 0x1F);       /* rate scaling = 0 */
    YM2612_writeReg(part, 0x60 + base, d1r & 0x1F);      /* AM = off */
    YM2612_writeReg(part, 0x70 + base, d2r & 0x1F);
    YM2612_writeReg(part, 0x80 + base, ((sl & 0x0F) << 4) | (rr & 0x0F));
    YM2612_writeReg(part, 0x90 + base, 0);                /* SSG-EG off */
}

void fm_synth_initChannel(u8 ch)
{
    u8 part   = (ch >= 3) ? 1 : 0;
    u8 chOff  = ch % 3;
    /* Operator offsets within a channel: op1=0x00, op2=0x08, op3=0x04,
     * op4=0x0C - the YM2612's non-obvious operator ordering. */

    /* Algorithm 7: all 4 operators are carriers (summed, no FM
     * modulation chain). Deliberately the safest algorithm to hand-
     * program - every operator is directly audible, so a mistake in
     * one operator's envelope can't silence the whole voice the way a
     * broken modulator would in algorithms 0-6. Feedback = 0. */
    YM2612_writeReg(part, 0xB0 + chOff, 0x07);
    /* Pan both L+R (center), no LFO sensitivity. */
    YM2612_writeReg(part, 0xB4 + chOff, 0xC0);

    /* A simple layered/organ-ish tone: op1 the loudest fundamental,
     * op2 an octave-up harmonic (MUL=2) for brightness, op3/op4
     * quieter supporting layers. Fast attack, slow decay into a
     * sustained level so held notes ring instead of plucking out. */
    writeOp(part, chOff, 0x00, /*mul*/1, /*dt*/0, /*tl*/ 4, /*ar*/31, /*d1r*/5, /*d2r*/2, /*sl*/1, /*rr*/7);
    writeOp(part, chOff, 0x08, /*mul*/2, /*dt*/0, /*tl*/24, /*ar*/31, /*d1r*/6, /*d2r*/2, /*sl*/2, /*rr*/7);
    writeOp(part, chOff, 0x04, /*mul*/1, /*dt*/0, /*tl*/30, /*ar*/31, /*d1r*/6, /*d2r*/2, /*sl*/2, /*rr*/7);
    writeOp(part, chOff, 0x0C, /*mul*/1, /*dt*/0, /*tl*/16, /*ar*/31, /*d1r*/5, /*d2r*/2, /*sl*/1, /*rr*/7);
}

void fm_synth_noteOn(u8 ch, u8 note, u8 octave)
{
    u8  part  = (ch >= 3) ? 1 : 0;
    u8  chOff = ch % 3;
    u16 fnum  = noteFnum[note % 12];
    /* Key on/off is always addressed via part 0; the target channel
     * (including which part it belongs to) is encoded in the data
     * byte's low bits: bit2 selects part 1-3 vs 4-6, bits0-1 select
     * the channel within that part. */
    u8  keyCh = chOff | (part << 2);

    YM2612_writeReg(part, 0xA4 + chOff, (octave << 3) | (fnum >> 8));
    YM2612_writeReg(part, 0xA0 + chOff, fnum & 0xFF);
    YM2612_writeReg(0, 0x28, 0xF0 | keyCh);
}

void fm_synth_noteOff(u8 ch)
{
    u8 part  = (ch >= 3) ? 1 : 0;
    u8 chOff = ch % 3;
    u8 keyCh = chOff | (part << 2);

    YM2612_writeReg(0, 0x28, keyCh);
}
