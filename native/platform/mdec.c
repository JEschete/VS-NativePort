/* mdec.c -- software MDEC: VLC bitstream in, pixels out.
 *
 * See native/include/psx_mdec.h for how this fits the port. The PlayStation
 * splits video decoding between the CPU and the MDEC, and neither half can run
 * as-is on x86:
 *
 *   - The variable-length stage is PSYQ's DecDCTvlc2, hand-written MIPS
 *     assembly in src/TITLE/TITLE.PRG/libpress/VLC_C.s that never got
 *     decompiled. mdec_vlc_decode() below is a transcription of it.
 *   - The MDEC is hardware, reached over DMA. mdec_in()/mdec_out() model it.
 *
 * THE CODE TABLE IS THE GAME'S OWN
 * --------------------------------
 * No Huffman table is written out here. DecDCTvlc2 is table-driven, and the
 * table is built at runtime by DecDCTvlcBuild (BUILD.c), which is ordinary C
 * and compiles natively -- so the port decodes with the exact table the retail
 * game ships, decompressed by the retail routine. Reconstructing an equivalent
 * table by hand is both unnecessary and a good way to introduce rare, subtle
 * corruption in frames that happen to use an uncommon code.
 *
 * Its layout, read off the assembly's indexing:
 *
 *   +0x00000  1024 B  DC table, blocks 3..6   256 entries x {u16 skip, u16 bits}
 *   +0x00400  1024 B  DC table, blocks 1..2   (same shape)
 *   +0x00800 65536 B  AC primary, indexed by the top 13 bits of the bit
 *                     accumulator: 8192 entries x 8 bytes.
 *                       word 0: bits 0..7  = bits to consume
 *                               bits 16..31 = first output code
 *                       word 1: two further output codes (low, high halves),
 *                               zero when unused
 *                     A zero word 0 means "consume 8 bits and try the
 *                     secondary table".
 *   +0x10800  2048 B  AC secondary, indexed by the next 9 bits: 512 entries x
 *                     4 bytes, same shape as word 0 above.
 *
 * Two code values are special rather than literal: 0xFE00 ends the block, and
 * 0x7C1F means the next 16 raw bits ARE the output code -- which works because
 * the escape's payload (6-bit run, 10-bit level) already has the MDEC code's
 * layout.
 *
 * BITSTREAM
 * ---------
 * A frame begins with an 8-byte header -- u16 word count, u16 0x3800 magic,
 * u16 quantisation scale, u16 version -- then the bitstream, read as 16-bit
 * little-endian units, most significant bit first. Version 2 codes each block's
 * DC as a plain 10-bit value; version 3 codes it as a difference against the
 * previous block of the same component. A DC field of all-ones ends the frame,
 * and the remaining output is padded with end-of-block codes out to the word
 * count the header declared.
 *
 * Blocks come in macroblocks of six -- Cr, Cb, Y0, Y1, Y2, Y3 -- covering a
 * 16x16 pixel area at 4:2:0 chroma, and macroblocks are coded in *column*
 * order, top to bottom then left to right. That is the order the hardware
 * writes them to VRAM: the game uploads each decoded column as a 16-pixel-wide
 * strip (see _decDCToutCallback in src/TITLE/TITLE.PRG/22C.c, which walks
 * renderTarget.x forward by 24 VRAM halfwords = 16 pixels of 24-bit colour).
 * So concatenating one column's macroblocks gives exactly the strip the game
 * LoadImage()s -- for 320x224 that is 14 macroblocks = 2688 words, precisely
 * what 22C.c asks DecDCTout for.
 */

#include <string.h>
#include <psx_mdec.h>

/* ------------------------------------------------------------------ */
/* Variable-length decode (transcription of DecDCTvlc2)                */
/* ------------------------------------------------------------------ */

/* Special code values. */
#define CODE_EOB    0xFE00
#define CODE_ESCAPE 0x7C1F

static int vlc_macroblocks;
static int vlc_blocks;

int mdec_vlc_macroblocks(void) { return vlc_macroblocks; }

/* The bit accumulator is kept MSB-aligned in `acc`, with `phase` counting how
 * far into the current 16-bit unit we are. Consuming n bits shifts them out and
 * pulls in a fresh unit whenever the phase crosses a unit boundary -- the same
 * trick the assembly uses (its `and $at, $v1, 0x10` test). */
#define CONSUME(n)                                                    \
    do {                                                              \
        acc <<= (n);                                                  \
        phase += (n);                                                 \
        if (phase & 0x10) {                                           \
            phase &= 0xF;                                             \
            acc |= (unsigned int)(*bs++) << phase;                    \
        }                                                             \
    } while (0)

int mdec_vlc_decode(const void* frame, unsigned long* out, int out_max_words,
    const unsigned short* table)
{
    const unsigned short* hdr = (const unsigned short*)frame;
    const unsigned short* bs;
    const unsigned char* tab = (const unsigned char*)table;
    const unsigned char* ac_primary = tab + 0x800;
    const unsigned char* ac_secondary = tab + 0x800 + 0x10000;

    unsigned short* o = (unsigned short*)out;
    unsigned int acc, phase;
    unsigned int qscale_hi;         /* quantisation scale, pre-shifted */
    unsigned int nwords;
    int version;
    int oi;                         /* output position, in 16-bit units */
    int oend;                       /* padded end of the output, ditto */
    int olimit;
    int block;                      /* 1..6 within the macroblock (version 3) */
    int dc_pred[3];

    if (!frame || !out || !table || out_max_words < 2) return -1;

    nwords = (unsigned int)hdr[0];
    if ((unsigned int)hdr[1] != 0x3800) return -2;
    qscale_hi = (unsigned int)hdr[2] << 10;
    version = (int)hdr[3];
    if (version != 2 && version != 3) return -3;

    olimit = out_max_words * 2;
    oend = 2 + (int)nwords * 2;
    if (oend > olimit) return -4;

    /* buf[0] is the MDEC command word: the header's first word carries both the
     * 0x3800 magic in its top half and the data word count in its bottom, which
     * is exactly the shape the command needs. DecDCTin adjusts the depth bits
     * afterwards. */
    out[0] = (unsigned long)hdr[0] | ((unsigned long)hdr[1] << 16);

    acc = ((unsigned int)hdr[4] << 16) | (unsigned int)hdr[5];
    phase = 0;
    bs = hdr + 6;

    block = (version >= 3) ? 1 : 0;
    dc_pred[0] = dc_pred[1] = dc_pred[2] = 0;
    vlc_macroblocks = 0;
    vlc_blocks = 0;
    oi = 1;

    for (;;) {
        unsigned int dc_field = acc >> 22;      /* top 10 bits */
        unsigned int code;

        /* An all-ones DC field ends the frame. Version 3 uses the full 10 bits;
         * version 2's marker is one bit narrower. */
        if (dc_field == (unsigned int)(version >= 3 ? 0x3FF : 0x1FF)) {
            oi++;
            break;
        }
        oi++;
        if (oi >= olimit) return -5;

        if (version >= 3) {
            /* Differential DC: an 8-bit lookup gives how many bits the prefix
             * occupies and how many follow as a signed difference. Blocks 1 and
             * 2 (Cr, Cb) use the second table, 3..6 (Y) the first. */
            const unsigned char* dctab = (block >= 3) ? tab : tab + 0x400;
            unsigned int idx = acc >> 24;
            unsigned int skip = *(const unsigned short*)(dctab + idx * 4);
            unsigned int bits = *(const unsigned short*)(dctab + idx * 4 + 2);
            int delta = 0;
            int comp;

            if (bits) {
                unsigned int sh = 32 - bits;
                acc <<= skip;
                delta = (int)(acc >> sh);
                if (!(acc & 0x80000000u)) delta -= (int)(~0u >> sh);
                acc <<= bits;
                phase += bits;
            } else {
                acc <<= skip;
            }
            phase += skip;
            if (phase & 0x10) {
                phase &= 0xF;
                acc |= (unsigned int)(*bs++) << phase;
            }

            comp = (block >= 3) ? 0 : block;    /* 0 = Y, 1 = Cr, 2 = Cb */
            dc_pred[comp] += delta;
            code = ((unsigned int)(dc_pred[comp] << 2) & 0x3FF) | qscale_hi;

            if (++block == 7) block = 1;
        } else {
            CONSUME(10);
            code = qscale_hi | dc_field;
        }

        o[oi] = (unsigned short)code;
        oi++;
        vlc_blocks++;
        vlc_macroblocks = vlc_blocks / 6;

        /* AC coefficients, until an end-of-block code. */
        for (;;) {
            unsigned int w0, w1, take;

            if (oi >= olimit) return -5;

            w0 = *(const unsigned int*)(ac_primary + ((acc >> 19) << 3));
            if (w0) {
                w1 = *(const unsigned int*)(ac_primary + ((acc >> 19) << 3) + 4);
            } else {
                CONSUME(8);
                w0 = *(const unsigned int*)(ac_secondary + ((acc >> 23) << 2));
                w1 = 0;
            }
            take = w0 & 0xFF;
            CONSUME(take);

            code = w0 >> 16;
            for (;;) {
                if (code == CODE_ESCAPE) {
                    /* The next 16 raw bits are the code, verbatim. */
                    o[oi] = (unsigned short)(acc >> 16);
                    oi++;
                    acc <<= 16;
                    acc |= (unsigned int)(*bs++) << phase;
                    break;
                }
                if (code == CODE_EOB) {
                    o[oi] = CODE_EOB;
                    goto next_block;
                }
                o[oi] = (unsigned short)code;
                oi++;
                if (oi >= olimit) return -5;

                /* One table hit can carry up to three codes. */
                if (!w1) break;
                code = w1 & 0xFFFF;
                w1 >>= 16;
                if (code == 0) break;
            }
        }
    next_block:
        ;
    }

    /* Pad out to the word count the frame declared, exactly as the assembly
     * does: the MDEC input is DMA'd in whole 32-word blocks, so the encoder
     * rounds the count up and the slack has to be filled with something inert. */
    while (oi < oend) o[oi++] = CODE_EOB;

    return (int)nwords + 1;
}

/* ------------------------------------------------------------------ */
/* Coefficient order and quantisation                                  */
/* ------------------------------------------------------------------ */

static const unsigned char zigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/* The default inverse-quantisation table, in zig-zag order -- the same bytes
 * the game hands back from DecDCTGetEnv (D_80075B48 in LIBPRESS.c). */
static const unsigned char default_iq[64] = {
    0x02, 0x10, 0x10, 0x13, 0x10, 0x13, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x1A, 0x18,
    0x1A, 0x1B, 0x1B, 0x1B, 0x1A, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1D, 0x1D, 0x1D,
    0x22, 0x22, 0x22, 0x1D, 0x1D, 0x1D, 0x1B, 0x1B, 0x1D, 0x1D, 0x20, 0x20, 0x22, 0x22,
    0x25, 0x26, 0x25, 0x23, 0x23, 0x22, 0x23, 0x26, 0x26, 0x28, 0x28, 0x28, 0x30, 0x30,
    0x2E, 0x2E, 0x38, 0x38, 0x3A, 0x45, 0x45, 0x53
};

static unsigned char iq_y[64];
static unsigned char iq_c[64];
static int iq_loaded;

static void iq_default(void)
{
    if (iq_loaded) return;
    memcpy(iq_y, default_iq, 64);
    memcpy(iq_c, default_iq, 64);
    iq_loaded = 1;
}

/* ------------------------------------------------------------------ */
/* IDCT                                                                */
/* ------------------------------------------------------------------ */

/* cos(m * pi / 16) for m = 0..16; the rest of the circle mirrors onto these.
 *
 * Spelled out rather than computed, so this file needs no libm. That is worth
 * more than it looks: the stub generator turns any undefined symbol it does not
 * recognise into a zeroed data blob, and libm is not among the libraries it
 * checks -- so a call to cos() links happily and then jumps into an array of
 * zeroes at run time. */
static const double cos_table[17] = {
    1.00000000000000000000,  0.98078528040323044913,  0.92387953251128673848,
    0.83146961230254523708,  0.70710678118654752440,  0.55557023301960222474,
    0.38268343236508977173,  0.19509032201612826785,  0.00000000000000000000,
   -0.19509032201612826785, -0.38268343236508977173, -0.55557023301960222474,
   -0.70710678118654752440, -0.83146961230254523708, -0.92387953251128673848,
   -0.98078528040323044913, -1.00000000000000000000
};

static double idct_cos[8][8];
static int idct_ready;

static void idct_init(void)
{
    int x, u;
    if (idct_ready) return;
    for (x = 0; x < 8; x++) {
        for (u = 0; u < 8; u++) {
            double c = (u == 0) ? 0.353553390593273762   /* 1 / (2 * sqrt 2) */
                                : 0.5;
            int m = ((2 * x + 1) * u) & 31;             /* cos has period 32 here */
            idct_cos[x][u] = c * (m <= 16 ? cos_table[m] : cos_table[32 - m]);
        }
    }
    idct_ready = 1;
}

/* floor(v + 0.5), without libm. */
static int round_half_up(double s)
{
    int v = (int)s;             /* truncates toward zero */
    double frac = s - v;
    if (frac >= 0.5) v += 1;
    else if (frac < -0.5) v -= 1;
    return v;
}

/* Separable 8x8 inverse DCT. Output is clamped to signed 8 bits, which is what
 * the hardware feeds its colour conversion stage. */
static void idct_8x8(const int in[64], signed char out[64])
{
    double tmp[64];
    int x, y, u;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            double s = 0.0;
            for (u = 0; u < 8; u++) {
                int c = in[y * 8 + u];
                if (c) s += c * idct_cos[x][u];
            }
            tmp[y * 8 + x] = s;
        }
    }
    for (x = 0; x < 8; x++) {
        for (y = 0; y < 8; y++) {
            double s = 0.0;
            int v;
            for (u = 0; u < 8; u++) {
                double c = tmp[u * 8 + x];
                if (c != 0.0) s += c * idct_cos[y][u];
            }
            v = round_half_up(s);
            if (v < -128) v = -128;
            if (v > 127) v = 127;
            out[y * 8 + x] = (signed char)v;
        }
    }
}

/* ------------------------------------------------------------------ */
/* The MDEC itself                                                     */
/* ------------------------------------------------------------------ */

#define MAX_MACROBLOCKS 1200
#define MB_BYTES_24 (16 * 16 * 3)
#define MB_BYTES_15 (16 * 16 * 2)

static unsigned char out_fifo[MAX_MACROBLOCKS * MB_BYTES_24];
static int out_len;   /* bytes valid */
static int out_pos;   /* bytes already read */

void mdec_reset(void)
{
    out_len = 0;
    out_pos = 0;
    iq_loaded = 0;
    iq_default();
}

void mdec_set_iq(const unsigned char* y, const unsigned char* c)
{
    iq_default();
    if (y) memcpy(iq_y, y, 64);
    if (c) memcpy(iq_c, c, 64);
}

int mdec_out_avail(void) { return (out_len - out_pos) / 4; }

static int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static int sext10(unsigned int v)
{
    return (int)((v ^ 0x200u) - 0x200u);
}

/* Read the run/level codes of one block and produce its 8x8 pixels. */
static const unsigned short* decode_block(const unsigned short* code, const unsigned short* end,
    const unsigned char* iq, signed char pixels[64])
{
    int coef[64];
    int qscale, dc, k;

    memset(coef, 0, sizeof coef);
    if (code >= end) {
        memset(pixels, 0, 64);
        return code;
    }

    qscale = (*code >> 10) & 0x3F;
    dc = sext10(*code & 0x3FF);
    code++;

    /* The DC term is scaled by the table entry alone -- the frame's
     * quantisation scale applies only to the AC coefficients. */
    coef[0] = dc * iq[0];

    k = 0;
    while (code < end) {
        int run, level;
        if (*code == CODE_EOB) { code++; break; }
        run = (*code >> 10) & 0x3F;
        level = sext10(*code & 0x3FF);
        code++;
        k += run + 1;
        if (k > 63) break;
        coef[zigzag[k]] = (level * iq[k] * qscale + 4) >> 3;
    }

    idct_8x8(coef, pixels);
    return code;
}

void mdec_in(const unsigned long* buf, int nwords)
{
    unsigned long cmd;
    const unsigned short* code;
    const unsigned short* end;
    int depth, bit15;
    int mb;

    idct_init();
    iq_default();
    out_len = 0;
    out_pos = 0;

    if (!buf || nwords <= 0) return;

    cmd = buf[0];
    if (((cmd >> 29) & 7) != 1) return;   /* only decode-macroblock lands here */
    depth = (int)((cmd >> 27) & 3);
    bit15 = (int)((cmd >> 25) & 1);

    code = (const unsigned short*)&buf[1];
    end = code + (unsigned)nwords * 2;

    for (mb = 0; mb < MAX_MACROBLOCKS && code < end; mb++) {
        signed char cr[64], cb[64], y[4][64];
        unsigned char* dst;
        int i, px, py;

        code = decode_block(code, end, iq_c, cr);
        code = decode_block(code, end, iq_c, cb);
        for (i = 0; i < 4; i++) code = decode_block(code, end, iq_y, y[i]);

        if (out_len + MB_BYTES_24 > (int)sizeof out_fifo) break;
        dst = out_fifo + out_len;

        for (py = 0; py < 16; py++) {
            for (px = 0; px < 16; px++) {
                int yb = ((py >> 3) << 1) | (px >> 3);
                int yv = y[yb][(py & 7) * 8 + (px & 7)];
                int cidx = (py >> 1) * 8 + (px >> 1);
                int crv = cr[cidx];
                int cbv = cb[cidx];
                int r = clamp255(yv + 128 + (int)(1.402 * crv));
                int g = clamp255(yv + 128 - (int)(0.3437 * cbv + 0.7143 * crv));
                int b = clamp255(yv + 128 + (int)(1.772 * cbv));

                if (depth == MDEC_DEPTH_15BIT) {
                    unsigned int v = ((unsigned)(r >> 3))
                                   | ((unsigned)(g >> 3) << 5)
                                   | ((unsigned)(b >> 3) << 10)
                                   | ((unsigned)bit15 << 15);
                    dst[0] = (unsigned char)(v & 0xFF);
                    dst[1] = (unsigned char)(v >> 8);
                    dst += 2;
                } else {
                    dst[0] = (unsigned char)r;
                    dst[1] = (unsigned char)g;
                    dst[2] = (unsigned char)b;
                    dst += 3;
                }
            }
        }
        out_len += (depth == MDEC_DEPTH_15BIT) ? MB_BYTES_15 : MB_BYTES_24;
    }
}

int mdec_out(void* dst, int nwords)
{
    int avail = out_len - out_pos;
    int want = nwords * 4;
    int n = want < avail ? want : avail;

    if (n > 0) memcpy(dst, out_fifo + out_pos, (size_t)n);
    if (n < want) memset((unsigned char*)dst + n, 0, (size_t)(want - n));
    out_pos += n;
    return n / 4;
}
