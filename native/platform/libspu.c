/* libspu / libsnd: PSYQ's SPU API over the software SPU.
 *
 * The mixing itself lives in native/platform/spu_core.c; this is the PSYQ
 * surface the game and the sound driver call. Most of it stays thin, because
 * the Akao driver does not go through libspu for the interesting work -- it
 * writes the SPU's registers directly (src/include/hw.h) and only uses libspu
 * to move sample data into SPU RAM.
 *
 * Anything here that reports state returns "idle/quiet" rather than 0-as-error,
 * so the game's wait-for-SPU loops terminate instead of spinning forever. */
#include <string.h>
#include <psx_spu.h>
#include <psx_host.h>

/* Where the next SpuWrite lands in SPU RAM. */
static unsigned spu_transfer_addr;

void SpuInit(void) { spu_reset(); }
void SpuQuit(void) { }
int  SpuInitMalloc(int num, char* top) { (void)num; (void)top; return 0; }
long SpuMalloc(long size) { (void)size; return 0; }
void SpuFree(unsigned addr) { (void)addr; }

unsigned SpuSetTransferStartAddr(unsigned addr) { spu_transfer_addr = addr; return addr; }
unsigned SpuGetTransferStartAddr(void) { return spu_transfer_addr; }
int      SpuSetTransferMode(int mode) { (void)mode; return 0; }

/* Transfer-completion callback. On hardware the DMA finishes a moment later and
 * raises an interrupt; here the "transfer" is instantaneous, so the callback
 * fires at the end of the write.
 *
 * This is load-bearing, not bookkeeping: the sound driver does
 *     _isSpuTransfer = 1; SpuSetTransferCallback(_spuWriteComplete);
 * and then _waitTransferAvailable() spins `while (_isSpuTransfer == 1)`. With
 * no callback the boot hangs there forever, before the first frame. */
static void (*spu_transfer_cb)(void);

void* SpuSetTransferCallback(void (*func)(void))
{
    void* old = (void*)spu_transfer_cb;
    spu_transfer_cb = func;
    return old;
}

unsigned SpuWrite(unsigned char* addr, unsigned size)
{
    if (addr) spu_ram_write(spu_transfer_addr, addr, size);
    spu_transfer_addr += size;
    if (spu_transfer_cb) spu_transfer_cb();
    return size;
}

unsigned SpuRead(unsigned char* addr, unsigned size)
{
    if (addr) spu_ram_read(spu_transfer_addr, addr, size);
    spu_transfer_addr += size;
    if (spu_transfer_cb) spu_transfer_cb();
    return size;
}

/* "Transfer finished" -- always, immediately. A never-completing transfer would
 * hang the loaders that poll it. */
int SpuIsTransferCompleted(int mode) { (void)mode; return 1; }

/* Key-on/off through the API rather than the register: same registers, so the
 * mixer picks these up identically. */
void SpuSetKey(int on_off, unsigned voice_bit)
{
    volatile unsigned short* r = (volatile unsigned short*)(on_off ? 0x1F801D88
                                                                   : 0x1F801D8C);
    r[0] |= (unsigned short)(voice_bit & 0xFFFF);
    r[1] |= (unsigned short)(voice_bit >> 16);
}
int  SpuGetKeyStatus(unsigned voice_bit) { (void)voice_bit; return 0; }

void SpuSetCommonMasterVolume(short left, short right)
{
    *(volatile unsigned short*)0x1F801D80 = (unsigned short)left;
    *(volatile unsigned short*)0x1F801D82 = (unsigned short)right;
}
/* CD input volume. The game reaches these registers directly from
 * UpdateCdVolume(), so this exists for completeness rather than because
 * anything calls it -- but it is a register write either way, and spu_core
 * scales the decoded XA track by exactly this. */
void SpuSetCommonCDVolume(short left, short right)
{
    *(volatile unsigned short*)0x1F801DB0 = (unsigned short)left;
    *(volatile unsigned short*)0x1F801DB2 = (unsigned short)right;
}

/* Muting the CD input is not modelled; nothing in the game calls it. Were it
 * to, the mix would keep playing the XA track. */
void SpuSetCommonCDMix(int on_off) { (void)on_off; }

int  SpuSetReverb(int on_off) { (void)on_off; return 0; }
int  SpuSetReverbModeType(int mode) { (void)mode; return 0; }
int  SpuGetReverbModeType(void) { return 0; }
void SpuSetReverbDepth(void* r_attr) { (void)r_attr; }
int  SpuReserveReverbWorkArea(int on_off) { (void)on_off; return 1; }
int  SpuClearReverbWorkArea(int mode) { (void)mode; return 1; }

unsigned SpuSetIRQ(unsigned on_off) { (void)on_off; return 0; }
unsigned SpuSetIRQAddr(unsigned addr) { (void)addr; return addr; }
void*    SpuSetIRQCallback(void (*func)(void)) { (void)func; return 0; }

void SpuStart(void) { }
void SpuSetVoiceAttr(void* attr) { (void)attr; }

/* libsnd (Ss*) -- the sequencer layer sitting on top of the SPU. */
void SsUtReverbOn(void) { }
void SsUtReverbOff(void) { }
int  SsUtSetReverbType(int type) { (void)type; return 0; }
void SsUtSetReverbDepth(short l, short r) { (void)l; (void)r; }
void SsSetMVol(short l, short r) { (void)l; (void)r; }
void SsInit(void) { }
void SsEnd(void) { }
void SsStart(void) { }
void SsQuit(void) { }

/* ---- output ----
 *
 * Audio is produced a frame's worth at a time from VSync(), which is where the
 * driver has just finished its tick, so the mix always reflects the register
 * state the driver most recently wrote.
 *
 * VS_AUDIO_WAV=<path> captures the mix to a WAV file. A headless run has no
 * sound card and no way to listen, and "is the SPU producing anything" is a
 * question a file answers exactly -- so this is how the mixer gets checked
 * before any of it reaches a speaker. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FRAMES_PER_VBLANK (SPU_RATE / 60)   /* 735 */

static FILE* wav;
static unsigned long wav_frames;
static short mixbuf[FRAMES_PER_VBLANK * 2];

static void wav_put32(FILE* f, unsigned long v)
{
    fputc((int)(v & 0xFF), f);       fputc((int)((v >> 8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f); fputc((int)((v >> 24) & 0xFF), f);
}
static void wav_put16(FILE* f, unsigned v)
{
    fputc((int)(v & 0xFF), f); fputc((int)((v >> 8) & 0xFF), f);
}

static void wav_finish(void)
{
    unsigned long bytes;
    if (!wav) return;
    bytes = wav_frames * 4;
    fseek(wav, 4, SEEK_SET);  wav_put32(wav, 36 + bytes);
    fseek(wav, 40, SEEK_SET); wav_put32(wav, bytes);
    fclose(wav);
    wav = 0;
    {
        extern unsigned long spu_stat_keyons, spu_stat_upload_bytes;
        fprintf(stderr, "[spu] %lu key-ons, %lu bytes uploaded to SPU RAM\n",
                spu_stat_keyons, spu_stat_upload_bytes);
    }
    fprintf(stderr, "[spu] wrote %lu frames (%.1fs) of audio\n",
            wav_frames, (double)wav_frames / SPU_RATE);
}

static void wav_open(const char* path)
{
    wav = fopen(path, "wb");
    if (!wav) return;
    fwrite("RIFF", 1, 4, wav); wav_put32(wav, 0);
    fwrite("WAVEfmt ", 1, 8, wav); wav_put32(wav, 16);
    wav_put16(wav, 1); wav_put16(wav, 2);
    wav_put32(wav, SPU_RATE); wav_put32(wav, SPU_RATE * 4);
    wav_put16(wav, 4); wav_put16(wav, 16);
    fwrite("data", 1, 4, wav); wav_put32(wav, 0);
    atexit(wav_finish);
}

/* The sound driver polls this to find out whether a voice has finished: when
 * the envelope reaches zero it hands the voice back (func_80015080 ->
 * UnassignVoicesFromChannels). spu_core keeps the current envelope level in the
 * voice's ADSR_VOL register as it mixes, so this is a plain register read --
 * and it has to be real, because a stub returning 0 would have every voice
 * reclaimed on the tick after it started. */
void SpuGetVoiceEnvelope(int vNum, short* envx)
{
    if (envx) {
        *envx = (short)(*(volatile unsigned short*)(0x1F801C00 + vNum * 0x10 + 0xC));
    }
}

/* Reverb is not modelled by the software SPU (see native/include/psx_spu.h), so
 * depth is accepted and dropped. Noise is not modelled either. Both are
 * recorded in docs/NATIVE_DEVIATIONS.md. */
void SpuSetReverbModeDepth(short depth_left, short depth_right)
{
    (void)depth_left;
    (void)depth_right;
}

long SpuSetNoiseClock(long n_clock) { return n_clock; }

extern int psx_audio_sink_open(void);
extern void psx_audio_sink_push(const short* frames, int nframes);

static unsigned long long spu_now_ns(void)
{
    return psx_host_now_ns();
}

/* Called from VSync().
 *
 * The mix runs unconditionally. It used to be gated on VS_AUDIO_WAV, which made
 * an ordinary run not merely silent but inert: `spu_render` is what consumes
 * the key-on register, so without it no voice ever started and the counters
 * stayed at zero, which reads as "the driver is broken" rather than "nothing is
 * listening".
 *
 * How much to render is decided by ELAPSED TIME, not by one vblank per call --
 * the same correction the vblank counter needed, and for the same reason. The
 * game does not call VSync sixty times a second: during the intro movie it
 * calls it once per movie frame, about fifteen times a second. Rendering a
 * fixed 735 frames per call therefore produced audio at a quarter of real time,
 * which starved the movie's soundtrack into short bursts separated by silence
 * and left a 50-second run holding 24 seconds of audio. */
void psx_spu_pump(void)
{
    static int checked;
    static unsigned long long last_ns;
    unsigned long long now = spu_now_ns();
    long need;

    if (!checked) {
        const char* path = getenv("VS_AUDIO_WAV");
        checked = 1;
        if (path && *path) wav_open(path);
        psx_audio_sink_open();
        last_ns = now;
        return;
    }

    need = (long)((now - last_ns) * SPU_RATE / 1000000000ull);
    if (need <= 0) return;

    /* After a long stall (a load, a breakpoint) do not try to catch up by
     * emitting a second of audio in one go: drop the backlog and resync. */
    if (need > SPU_RATE / 4) {
        need = FRAMES_PER_VBLANK;
        last_ns = now;
    } else {
        /* Advance by exactly what was rendered, keeping the sub-frame
         * remainder, so the render rate tracks the clock instead of drifting. */
        last_ns += (unsigned long long)need * 1000000000ull / SPU_RATE;
    }

    while (need > 0) {
        int n = need > FRAMES_PER_VBLANK ? FRAMES_PER_VBLANK : (int)need;

        spu_render(mixbuf, n);
        psx_audio_sink_push(mixbuf, n);

        if (wav) {
            fwrite(mixbuf, sizeof(short) * 2, (size_t)n, wav);
            wav_frames += (unsigned long)n;
        }
        need -= n;
    }

    /* Periodic, because the watchdog kills the process with a signal and
     * atexit never runs -- and a silent capture is exactly the case where these
     * numbers are wanted. */
    {
        extern unsigned long spu_stat_keyons, spu_stat_upload_bytes;
        static unsigned long ticks;
        if (++ticks % 600 == 0)
            fprintf(stderr, "[spu] %lus: %lu key-ons, %lu bytes in SPU RAM, %d voices\n",
                    ticks / 60, spu_stat_keyons, spu_stat_upload_bytes,
                    spu_active_voices());
    }
}
