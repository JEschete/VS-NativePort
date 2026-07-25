/* libpress: PSYQ's MDEC front end.
 *
 * src/TITLE/TITLE.PRG/libpress/LIBPRESS.c is real, decompiled code, but it is
 * built around inline MIPS assembly that pokes DMA channels 0 and 1 and the
 * MDEC registers directly, so the whole translation unit is excluded from the
 * native build (see NATIVE_EXCLUDE in Makefile.native) and these entry points
 * are ours. The decode itself lives in native/platform/mdec.c.
 *
 * Only DecDCTvlcBuild survives from the retail file: it is ordinary C, it
 * compiles natively, and it produces the decode table this layer hands
 * straight to the VLC decoder -- so the port decodes with the game's own table.
 *
 * THE ONE PIECE OF TIMING THAT MATTERS. On hardware DecDCTout starts a DMA and
 * returns; the transfer completes later and raises an interrupt, which calls
 * the function DecDCToutCallback registered. Here the decode is synchronous, so
 * the callback fires at the end of DecDCTout instead. That is safe for the way
 * the game drives it (_playIntroMovie/_decDCToutCallback in 22C.c): the callback
 * uploads the strip it was handed and issues DecDCTout for the next one, so the
 * chain becomes recursion, bounded by the number of strips in a frame -- twenty
 * for a 320-wide movie. _waitForFrame then finds frameComplete already set and
 * returns immediately.
 */

#include <string.h>
#include <psx_mdec.h>

/* From include/psx/libpress.h; the platform layer builds without PSYQ headers. */
typedef struct {
    unsigned char iq_y[64];
    unsigned char iq_c[64];
    short dct[64];
} DECDCTENV;

static void (*dma_in_callback)(void);
static void (*dma_out_callback)(void);

/* The default inverse-quantisation table and IDCT coefficients the hardware
 * comes up with, as retail's DecDCTGetEnv reports them. */
static const unsigned char default_iq[64] = {
    0x02, 0x10, 0x10, 0x13, 0x10, 0x13, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x1A, 0x18,
    0x1A, 0x1B, 0x1B, 0x1B, 0x1A, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1D, 0x1D, 0x1D,
    0x22, 0x22, 0x22, 0x1D, 0x1D, 0x1D, 0x1B, 0x1B, 0x1D, 0x1D, 0x20, 0x20, 0x22, 0x22,
    0x25, 0x26, 0x25, 0x23, 0x23, 0x22, 0x23, 0x26, 0x26, 0x28, 0x28, 0x28, 0x30, 0x30,
    0x2E, 0x2E, 0x38, 0x38, 0x3A, 0x45, 0x45, 0x53
};

static const short default_dct[64] = {
    0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82, 0x5A82,
    0x7D8A, 0x6A6D, 0x471C, 0x18F8, (short)0xE707, (short)0xB8E3, (short)0x9592, (short)0x8275,
    0x7641, 0x30FB, (short)0xCF04, (short)0x89BE, (short)0x89BE, (short)0xCF04, 0x30FB, 0x7641,
    0x6A6D, (short)0xE707, (short)0x8275, (short)0xB8E3, 0x471C, 0x7D8A, 0x18F8, (short)0x9592,
    0x5A82, (short)0xA57D, (short)0xA57D, 0x5A82, 0x5A82, (short)0xA57D, (short)0xA57D, 0x5A82,
    0x471C, (short)0x8275, 0x18F8, 0x6A6D, (short)0x9592, (short)0xE707, 0x7D8A, (short)0xB8E3,
    0x30FB, (short)0x89BE, 0x7641, (short)0xCF04, (short)0xCF04, 0x7641, (short)0x89BE, 0x30FB,
    0x18F8, (short)0xB8E3, 0x6A6D, (short)0x8275, 0x7D8A, (short)0x9592, 0x471C, (short)0xE707
};

/* Bring the MDEC up. mode 0 also resets the interrupt callbacks, which is why
 * retail calls ResetCallback() on that path. */
void DecDCTReset(int mode)
{
    mdec_reset();
    if (mode == 0) {
        dma_in_callback = 0;
        dma_out_callback = 0;
    }
}

DECDCTENV* DecDCTGetEnv(DECDCTENV* env)
{
    if (!env) return env;
    memcpy(env->iq_y, default_iq, sizeof default_iq);
    memcpy(env->iq_c, default_iq, sizeof default_iq);
    memcpy(env->dct, default_dct, sizeof default_dct);
    return env;
}

DECDCTENV* DecDCTPutEnv(DECDCTENV* env)
{
    if (!env) return env;
    /* The IDCT coefficient table is fixed in hardware -- retail's DecDCTPutEnv
     * uploads only the two quantisation tables, and so do we. */
    mdec_set_iq(env->iq_y, env->iq_c);
    return env;
}

/* Feed the MDEC. buf[0] is the command word the VLC stage built; mode picks the
 * output format, exactly as retail does by rewriting bits of that word:
 * bit 0 selects 24-bit rather than 15-bit colour, bit 1 sets the 15-bit mask
 * bit. The transfer length is the command's low half. */
void DecDCTin(unsigned long* buf, int mode)
{
    if (!buf) return;

    if (mode & 1) *buf &= 0xF7FFFFFFUL;
    else          *buf |= 0x08000000UL;

    if (mode & 2) *buf |= 0x02000000UL;
    else          *buf &= 0xFDFFFFFFUL;

    mdec_in(buf, (int)(*buf & 0xFFFFUL));

    if (dma_in_callback) dma_in_callback();
}

/* Pull `size` words of decoded pixels out, then raise the completion callback.
 *
 * The callback issues the NEXT DecDCTout before it uploads the strip it was
 * handed, so calling it directly would recurse one level per strip and, worse,
 * run every transfer before any of the uploads -- leaving the game's two
 * decode buffers holding only the last two strips, which then get uploaded
 * across the whole frame. Queueing instead of recursing reproduces the hardware
 * ordering exactly: one transfer completes, its callback runs and starts the
 * next, and control returns before that one is serviced. */
static unsigned long* pending_buf;
static int pending_size;
static int pending_valid;
static int dispatching;

void DecDCTout(unsigned long* buf, int size)
{
    pending_buf = buf;
    pending_size = size;
    pending_valid = 1;

    if (dispatching) return;        /* re-entered from the callback: just queue */

    dispatching = 1;
    while (pending_valid) {
        unsigned long* b = pending_buf;
        int n = pending_size;
        pending_valid = 0;
        if (b && n > 0) mdec_out(b, n);
        if (dma_out_callback) dma_out_callback();
    }
    dispatching = 0;
}

/* Both transfers have already finished by the time anything can ask. */
int DecDCTinSync(int mode) { (void)mode; return 0; }
int DecDCToutSync(int mode) { (void)mode; return 0; }

int DecDCTinCallback(void (*func)(void))
{
    void (*old)(void) = dma_in_callback;
    dma_in_callback = func;
    return (int)(long)old;
}

int DecDCToutCallback(void (*func)(void))
{
    void (*old)(void) = dma_out_callback;
    dma_out_callback = func;
    return (int)(long)old;
}

/* The variable-length stage. `table` is what DecDCTvlcBuild produced. */
int DecDCTvlc2(unsigned long* bs, unsigned long* buf, unsigned short* table)
{
    /* The output buffer is the game's own encoded-data allocation (0x23000
     * bytes in _initIntroMovie); the decoder needs its size only as a bound. */
    int rc = mdec_vlc_decode(bs, buf, 0x23000 / 4, table);
    return rc < 0 ? rc : 0;
}

int DecDCTvlc(unsigned long* bs, unsigned long* buf)
{
    (void)bs; (void)buf;
    /* The single-table variant is never called: the movie player builds its own
     * table and uses DecDCTvlc2. */
    return -1;
}

int DecDCTBufSize(unsigned long* bs)
{
    /* Frame word count from the header, plus the command word. */
    return bs ? (int)((*bs & 0xFFFFUL) + 1) : 0;
}

int DecDCTvlcSize(int size) { (void)size; return 0; }
int DecDCTvlcSize2(int size) { (void)size; return 0; }

/* DMA channel callbacks, as libapi exposes them. Only channels 0 and 1 (MDEC
 * in and out) reach us. */
int DMACallback(int channel, void (*func)(void))
{
    if (channel == 0) return DecDCTinCallback(func);
    if (channel == 1) return DecDCToutCallback(func);
    return 0;
}
