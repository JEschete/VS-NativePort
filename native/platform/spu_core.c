/* spu_core.c -- software SPU: ADPCM voices, ADSR envelopes, stereo mix.
 *
 * See native/include/psx_spu.h for why this is all that native audio needs:
 * the Akao sound driver is decompiled C and already runs, writing the register
 * block that scratchpad.c backs with real memory. This reads that block.
 *
 * WHAT IS MODELLED
 *   - 24 voices, each decoding 4-bit ADPCM out of 512 KB of sample RAM, with
 *     the loop/repeat flags that let a sample sustain.
 *   - Pitch, as a 12.12 step through the sample (0x1000 = 44100 Hz).
 *   - The full ADSR envelope: linear and exponential attack, decay to the
 *     sustain level, sustain in either direction, and release.
 *   - Per-voice left/right volume and the main volume.
 *
 * WHAT IS NOT, yet: reverb, noise voices, and pitch/amplitude modulation, all
 * of which the driver can request through registers this ignores. They are in
 * the deviations ledger. Interpolation is linear rather than the hardware's
 * four-tap Gaussian, which is audible only as a slightly brighter top end.
 */

#include <string.h>
#include <psx_spu.h>

/* ---- the register block, as the game sees it ---- */

#define SPU_REGS       ((volatile unsigned short*)0x1F801C00)
#define REG(off)       (*(volatile unsigned short*)(0x1F801C00 + (off)))
#define VOICE_REG(v, off) (*(volatile unsigned short*)(0x1F801C00 + (v) * 0x10 + (off)))

/* Per-voice register offsets. */
#define V_VOL_L        0x0
#define V_VOL_R        0x2
#define V_PITCH        0x4
#define V_START        0x6      /* sample address >> 3 */
#define V_ADSR_LO      0x8
#define V_ADSR_HI      0xA
#define V_ADSR_VOL     0xC      /* current envelope level, readable */
#define V_REPEAT       0xE      /* loop address >> 3 */

/* Control registers, as offsets from 0x1F801C00. */
#define R_MVOL_L       0x180
#define R_MVOL_R       0x182
#define R_KON_LO       0x188
#define R_KON_HI       0x18A
#define R_KOFF_LO      0x18C
#define R_KOFF_HI      0x18E

/* ---- ADPCM ---- */

/* The four filter tap pairs, in 1/64ths. */
static const int adpcm_pos[5] = { 0, 60, 115, 98, 122 };
static const int adpcm_neg[5] = { 0,  0, -52, -55, -60 };

/* ADSR phases. */
enum { PH_OFF = 0, PH_ATTACK, PH_DECAY, PH_SUSTAIN, PH_RELEASE };

typedef struct {
    int  on;                 /* voice is sounding */
    unsigned addr;           /* current 16-byte block in SPU RAM */
    unsigned repeat;         /* block to jump to at loop end */
    int  pos;                /* fractional sample position, 12 bits */
    int  cur;                /* index of the next sample in `decoded` */
    short decoded[28];
    int  have;               /* decoded[] holds a valid block */
    int  hist1, hist2;       /* ADPCM filter history */

    int  phase;
    int  level;              /* envelope, 0..0x7FFF */
    int  counter;            /* cycles until the next envelope step */
} voice_t;

static voice_t voices[SPU_VOICES];
static unsigned char spu_ram[SPU_RAM_SIZE];

/* Counters, for working out where a silent mix stops: no uploads means the
 * instrument bank never arrived, no key-ons means the driver is not playing. */
unsigned long spu_stat_keyons;
unsigned long spu_stat_upload_bytes;

void spu_reset(void)
{
    memset(voices, 0, sizeof voices);
    memset(spu_ram, 0, sizeof spu_ram);
}

void spu_ram_write(unsigned addr, const void* data, unsigned len)
{
    if (addr >= SPU_RAM_SIZE) return;
    if (addr + len > SPU_RAM_SIZE) len = SPU_RAM_SIZE - addr;
    memcpy(spu_ram + addr, data, len);
    spu_stat_upload_bytes += len;
}

void spu_ram_read(unsigned addr, void* data, unsigned len)
{
    if (addr >= SPU_RAM_SIZE) return;
    if (addr + len > SPU_RAM_SIZE) len = SPU_RAM_SIZE - addr;
    memcpy(data, spu_ram + addr, len);
}

int spu_active_voices(void)
{
    int i, n = 0;
    for (i = 0; i < SPU_VOICES; i++) if (voices[i].on) n++;
    return n;
}

static int clamp16(int v)
{
    if (v < -32768) return -32768;
    if (v >  32767) return  32767;
    return v;
}

/* Decode the 16-byte ADPCM block at v->addr into v->decoded, and follow the
 * loop flags. Returns 0 when the voice has run off the end of its sample. */
static int decode_block(voice_t* v)
{
    const unsigned char* b;
    int shift, filter, flags, i;

    if (v->addr + 16 > SPU_RAM_SIZE) return 0;
    b = spu_ram + v->addr;

    shift  = b[0] & 0x0F;
    filter = (b[0] >> 4) & 0x07;
    flags  = b[1];
    if (filter > 4) filter = 4;
    /* Shift 13..15 behave as 9 on hardware. */
    if (shift > 12) shift = 9;

    for (i = 0; i < 28; i++) {
        int nib = (b[2 + (i >> 1)] >> ((i & 1) ? 4 : 0)) & 0x0F;
        int s = (int)((short)(nib << 12)) >> shift;
        s += (v->hist1 * adpcm_pos[filter] + v->hist2 * adpcm_neg[filter]) >> 6;
        s = clamp16(s);
        v->decoded[i] = (short)s;
        v->hist2 = v->hist1;
        v->hist1 = s;
    }

    /* bit 2 marks a loop start, which is where the repeat address comes from
     * when the driver has not set one explicitly. */
    if (flags & 0x04) v->repeat = v->addr;

    if (flags & 0x01) {                 /* end of sample */
        if (flags & 0x02) {
            v->addr = v->repeat;        /* ... and loop back */
        } else {
            return 0;                   /* ... and stop */
        }
    } else {
        v->addr += 16;
    }
    v->have = 1;
    v->cur = 0;
    return 1;
}

/* ---- ADSR ----
 *
 * Each phase is a shift/step pair; the shift sets how many samples pass between
 * steps and the step how far the level moves. Exponential rise slows above
 * three quarters, and exponential fall scales the step by the current level --
 * which is what makes a decaying note sound natural rather than linear. */
static void envelope_params(voice_t* v, int* cycles, int* step, int* expo, int* target)
{
    unsigned lo = VOICE_REG(v - voices, V_ADSR_LO);
    unsigned hi = VOICE_REG(v - voices, V_ADSR_HI);
    int shift, st, mode, dir;

    switch (v->phase) {
    case PH_ATTACK:
        shift = (lo >> 10) & 0x1F;
        st    = 7 - ((lo >> 8) & 3);
        mode  = (lo >> 15) & 1;
        *target = 0x7FFF;
        break;
    case PH_DECAY: {
        int sl = (lo & 0x0F);
        shift = (lo >> 4) & 0x0F;
        st    = -8;
        mode  = 1;                       /* decay is always exponential */
        *target = (sl + 1) * 0x800;
        if (*target > 0x7FFF) *target = 0x7FFF;
        break;
    }
    case PH_SUSTAIN:
        shift = (hi >> 8) & 0x1F;
        dir   = (hi >> 14) & 1;
        st    = dir ? -8 + ((hi >> 6) & 3) : 7 - ((hi >> 6) & 3);
        mode  = (hi >> 15) & 1;
        *target = -1;                    /* no target: holds until key-off */
        break;
    default: /* PH_RELEASE */
        shift = hi & 0x1F;
        st    = -8;
        mode  = (hi >> 5) & 1;
        *target = 0;
        break;
    }

    *cycles = 1 << (shift > 11 ? shift - 11 : 0);
    *step   = st << (shift < 11 ? 11 - shift : 0);
    *expo   = mode;
}

static void envelope_tick(voice_t* v)
{
    int cycles, step, expo, target;

    if (v->phase == PH_OFF) return;

    envelope_params(v, &cycles, &step, &expo, &target);

    if (expo) {
        if (step > 0) {
            /* Exponential rise flattens out in the last quarter. */
            if (v->level > 0x6000) cycles *= 4;
        } else {
            step = (int)((long long)step * v->level / 0x8000);
            if (step == 0) step = -1;
        }
    }

    if (--v->counter > 0) return;
    v->counter = cycles;

    v->level += step;
    if (v->level > 0x7FFF) v->level = 0x7FFF;
    if (v->level < 0) v->level = 0;

    switch (v->phase) {
    case PH_ATTACK:
        if (v->level >= 0x7FFF) { v->phase = PH_DECAY; v->counter = 1; }
        break;
    case PH_DECAY:
        if (v->level <= target) { v->phase = PH_SUSTAIN; v->counter = 1; }
        break;
    case PH_RELEASE:
        if (v->level <= 0) { v->phase = PH_OFF; v->on = 0; }
        break;
    default:
        break;
    }
}

static void key_on(int i)
{
    voice_t* v = &voices[i];
    spu_stat_keyons++;
    v->on = 1;
    v->addr = (unsigned)VOICE_REG(i, V_START) << 3;
    v->repeat = (unsigned)VOICE_REG(i, V_REPEAT) << 3;
    v->pos = 0;
    v->cur = 0;
    v->have = 0;
    v->hist1 = v->hist2 = 0;
    v->phase = PH_ATTACK;
    v->level = 0;
    v->counter = 1;
}

static void key_off(int i)
{
    voice_t* v = &voices[i];
    if (!v->on) return;
    v->phase = PH_RELEASE;
    v->counter = 1;
}

/* Key-on and key-off are events, not state. Reading the register cannot tell a
 * fresh write from a stale one, so they are consumed: acted on, then cleared.
 * The driver only ever writes them (see 25AC.c), so this is invisible to it. */
static void consume_keys(void)
{
    unsigned kon  = (unsigned)REG(R_KON_LO)  | ((unsigned)REG(R_KON_HI)  << 16);
    unsigned koff = (unsigned)REG(R_KOFF_LO) | ((unsigned)REG(R_KOFF_HI) << 16);
    int i;

    if (koff) {
        for (i = 0; i < SPU_VOICES; i++) if (koff & (1u << i)) key_off(i);
        REG(R_KOFF_LO) = 0;
        REG(R_KOFF_HI) = 0;
    }
    if (kon) {
        for (i = 0; i < SPU_VOICES; i++) if (kon & (1u << i)) key_on(i);
        REG(R_KON_LO) = 0;
        REG(R_KON_HI) = 0;
    }
}

/* Volume registers are either a plain level (bit 15 clear, 15-bit signed,
 * doubled) or a sweep. Sweeps are not modelled; their current level is taken as
 * the sweep's target so a swept voice is at least audible at a sane volume. */
static int voice_volume(unsigned reg)
{
    if (reg & 0x8000) return 0x3FFF;
    return (int)(short)(reg << 1) >> 1;
}

void spu_render(short* out, int frames)
{
    int f, i;
    int mvol_l = voice_volume(REG(R_MVOL_L));
    int mvol_r = voice_volume(REG(R_MVOL_R));

    consume_keys();

    for (f = 0; f < frames; f++) {
        int accum_l = 0, accum_r = 0;

        for (i = 0; i < SPU_VOICES; i++) {
            voice_t* v = &voices[i];
            int pitch, s, vl, vr;

            if (!v->on) continue;

            if (!v->have && !decode_block(v)) { v->on = 0; continue; }

            s = v->decoded[v->cur];

            /* Advance by the pitch, in 12.12 steps, decoding blocks as needed.
             * Hardware caps the rate at 4x. */
            pitch = VOICE_REG(i, V_PITCH) & 0x3FFF;
            if (pitch > 0x4000) pitch = 0x4000;
            v->pos += pitch;
            while (v->pos >= 0x1000) {
                v->pos -= 0x1000;
                if (++v->cur >= 28) {
                    if (!decode_block(v)) { v->on = 0; break; }
                }
            }
            if (!v->on) continue;

            envelope_tick(v);

            s = (int)((long long)s * v->level >> 15);
            vl = voice_volume(VOICE_REG(i, V_VOL_L));
            vr = voice_volume(VOICE_REG(i, V_VOL_R));
            accum_l += (int)((long long)s * vl >> 15);
            accum_r += (int)((long long)s * vr >> 15);

            /* Report the envelope back, as the hardware does. */
            VOICE_REG(i, V_ADSR_VOL) = (unsigned short)v->level;
        }

        out[f * 2 + 0] = (short)clamp16((int)((long long)accum_l * mvol_l >> 15));
        out[f * 2 + 1] = (short)clamp16((int)((long long)accum_r * mvol_r >> 15));
    }
}
