/* CD-XA ADPCM: the audio track interleaved into the intro movie.
 *
 * On the console the CPU never sees this. With the drive in ADPCM mode it
 * decodes matching audio sectors itself and feeds the result into the SPU's CD
 * input, where it is mixed with the voices under the CD volume registers
 * (0x1F801DB0/DB2). The game's only involvement is setting that volume, which
 * `vs_sound_setCdVol` and `UpdateCdVolume` do. So this sits behind libds, not
 * behind the streaming layer: `psx_cdxa_sector` is handed every raw form 2
 * sector the virtual drive reads, and `psx_cdxa_pull` is drained by spu_core
 * while it mixes.
 *
 * FORMAT, established from the real TITLE.STR rather than from memory.
 *
 * Each audio sector carries 2304 bytes of payload as 18 "sound groups" of 128
 * bytes: a 16-byte header then 112 bytes of packed 4-bit samples. The header
 * holds eight shift/filter parameter bytes, each stored twice for error
 * correction. Measuring which header bytes are always equal across all 25,236
 * groups in TITLE.STR gives the layout unambiguously:
 *
 *     [0]==[4] [1]==[5] [2]==[6] [3]==[7]     blocks 0..3, copy first
 *     [8]==[12] [9]==[13] [10]==[14] [11]==[15]  blocks 4..7, copy first
 *
 * so the live parameters are at 4..7 and 12..15. TITLE.STR's coding byte is
 * 0x01 throughout: stereo, 37800Hz, 4-bit. Only 4-bit stereo is implemented,
 * because that is the only thing the game ships; anything else is skipped
 * rather than mis-decoded.
 *
 * The 112 data bytes are 28 dwords. Within a dword, byte j holds block 2j in
 * its low nibble and block 2j+1 in its high nibble. Blocks alternate channels,
 * so blocks 2k and 2k+1 are the left and right halves of the same 28 frames.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XA_RATE_HI  37800
#define XA_RATE_LO  18900
#define OUT_RATE    44100

/* Same four filter tap pairs as the SPU's ADPCM, in 1/64ths. */
static const int xa_pos[4] = { 0, 60, 115, 98 };
static const int xa_neg[4] = { 0, 0, -52, -55 };

/* Decoded output, resampled to the mixer's rate. About three seconds, far more
 * than the drive ever runs ahead by; it is a bound against a runaway, not a
 * design latency.
 *
 * Must be a power of two -- the head and tail wrap with a mask. It was
 * OUT_RATE*2 (88200) at first, which is not, so every index was mangled and the
 * mixer read frames that had never been written: the decode was perfect and the
 * output was 0.2% non-silent. */
#define RING_FRAMES 131072
static short ring[RING_FRAMES * 2];
static unsigned ring_head, ring_tail; /* frame indices, head == tail is empty */

/* ADPCM filter history, per channel, carried across sectors. */
static int hist1[2], hist2[2];

/* Resampler position, carried across sectors so there is no click at the seam.
 * 16.16 fixed point, stepping the input by in_rate/out_rate per output frame. */
static unsigned rs_pos;
static short rs_prev[2];
static int rs_have_prev;

unsigned long cdxa_stat_sectors;
unsigned long cdxa_stat_frames;

static int clamp16(int v)
{
    if (v < -32768) return -32768;
    if (v > 32767) return 32767;
    return v;
}

static unsigned ring_count(void)
{
    return (ring_head - ring_tail) & (RING_FRAMES - 1);
}

static void ring_put(short l, short r)
{
    unsigned next = (ring_head + 1) & (RING_FRAMES - 1);
    if (next == ring_tail) {
        /* Full: the drive is ahead of the mixer. Drop the oldest frame so the
         * stream stays live rather than stalling a second behind. */
        ring_tail = (ring_tail + 1) & (RING_FRAMES - 1);
    }
    ring[ring_head * 2 + 0] = l;
    ring[ring_head * 2 + 1] = r;
    ring_head = next;
}

/* Push one decoded 37800Hz (or 18900Hz) stereo frame, resampled to OUT_RATE.
 * Linear interpolation: the ratio is 6/7 for the fast rate, so the artefacts of
 * anything cruder would be plainly audible. */
static void push_resampled(int in_rate, short l, short r)
{
    unsigned step = (unsigned)(((unsigned long long)in_rate << 16) / OUT_RATE);

    if (!rs_have_prev) {
        rs_prev[0] = l;
        rs_prev[1] = r;
        rs_have_prev = 1;
        return;
    }
    /* Emit every output frame whose position falls inside this input step. */
    while (rs_pos < 0x10000) {
        int t = (int)rs_pos;
        int ol = rs_prev[0] + (((l - rs_prev[0]) * t) >> 16);
        int orr = rs_prev[1] + (((r - rs_prev[1]) * t) >> 16);
        ring_put((short)clamp16(ol), (short)clamp16(orr));
        cdxa_stat_frames++;
        rs_pos += step;
    }
    rs_pos -= 0x10000;
    rs_prev[0] = l;
    rs_prev[1] = r;
}

/* One raw 2336-byte MODE2/FORM2 sector, subheader first. Returns 1 if it was an
 * audio sector (and has been consumed), 0 if the caller should treat it as
 * data. */
int psx_cdxa_sector(const unsigned char* raw)
{
    const unsigned char* body;
    int coding, stereo, rate, bits, g;

    if (!(raw[2] & 0x04)) return 0; /* submode bit 2: not audio */

    coding = raw[3];
    stereo = (coding & 0x01) != 0;
    rate = (coding & 0x04) ? XA_RATE_LO : XA_RATE_HI;
    bits = (coding & 0x10) ? 8 : 4;

    if (!stereo || bits != 4) {
        /* Not what this game ships. Consume it rather than let the streaming
         * layer see ADPCM as a video chunk, but do not guess at the format. */
        static int warned;
        if (!warned++) {
            fprintf(stderr, "[cdxa] coding %02X (%s, %d-bit) not implemented\n",
                coding, stereo ? "stereo" : "mono", bits);
        }
        return 1;
    }

    body = raw + 8;
    for (g = 0; g < 18; g++) {
        const unsigned char* grp = body + g * 128;
        int k;

        for (k = 0; k < 4; k++) {
            short samp[2][28];
            int blk, d;

            for (blk = k * 2; blk < k * 2 + 2; blk++) {
                int param = grp[(blk < 4) ? (4 + blk) : (8 + blk)];
                int shift = param & 0x0F;
                int filter = (param >> 4) & 0x03;
                int ch = blk & 1;

                for (d = 0; d < 28; d++) {
                    int nib = (grp[16 + d * 4 + (blk >> 1)] >> ((blk & 1) * 4)) & 0x0F;
                    int s = (int)(short)(nib << 12) >> shift;

                    s += (hist1[ch] * xa_pos[filter] + hist2[ch] * xa_neg[filter] + 32)
                        >> 6;
                    s = clamp16(s);
                    hist2[ch] = hist1[ch];
                    hist1[ch] = s;
                    samp[ch][d] = (short)s;
                }
            }
            for (d = 0; d < 28; d++) {
                push_resampled(rate, samp[0][d], samp[1][d]);
            }
        }
    }
    cdxa_stat_sectors++;

    /* Under VS_SND_TRACE, report throughput and buffer level once a second's
     * worth of sectors. "The movie is quiet" has several possible causes -- no
     * sectors arriving, sectors arriving too slowly to keep the mixer fed, or a
     * decode producing silence -- and these two numbers separate them. */
    {
        extern int _sndTrace;
        if (_sndTrace > 0 && (cdxa_stat_sectors % 19) == 0) {
            fprintf(stderr, "[cdxa] %lu sectors, %lu frames, ring %u\n",
                cdxa_stat_sectors, cdxa_stat_frames, ring_count());
        }
    }
    return 1;
}

/* Drain up to `frames` stereo frames. Returns how many were available; the
 * caller zeroes the rest. */
int psx_cdxa_pull(short* out, int frames)
{
    int n = 0;

    while (n < frames && ring_count() > 0) {
        out[n * 2 + 0] = ring[ring_tail * 2 + 0];
        out[n * 2 + 1] = ring[ring_tail * 2 + 1];
        ring_tail = (ring_tail + 1) & (RING_FRAMES - 1);
        n++;
    }
    return n;
}

/* Drop everything buffered. The movie ending, or restarting, should not trail
 * a second of stale audio into whatever comes next. */
void psx_cdxa_reset(void)
{
    ring_head = ring_tail = 0;
    hist1[0] = hist1[1] = hist2[0] = hist2[1] = 0;
    rs_pos = 0;
    rs_have_prev = 0;
}
