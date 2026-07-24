/* Odds and ends from the BIOS / libgpu / libapi boot path that have no host
 * equivalent, kept together so the interesting layers stay uncluttered.
 *
 * All of these are called from _sysInit() / _sysReinit() in
 * src/SLUS_010.40/main.c. None of them can do anything meaningful natively:
 * there is no video encoder to configure, no BIOS to hand a card filesystem to.
 * They exist so the retail boot sequence runs unmodified. */
#include <stdio.h>

/* Restore the BIOS's default interrupt callbacks. Retail's decompiled version
 * (libetc/INTR.c) is `D_800320D4->unkC()` -- a jump through the kernel callback
 * vector table, which does not exist here, so that TU is excluded from the
 * native build and this takes its place. There is nothing to restore: the
 * platform layer owns vblank (libetc) and CD (libds) directly. */
void ResetCallback(void) { }

/* Overlay load addresses, verbatim from src/SLUS_010.40/overlay.c. That TU is
 * excluded from the native build (its body is inline MIPS assembly), so the
 * table is restated here.
 *
 * These are real PS1 RAM addresses and they are used as real addresses: the CD
 * loader writes TITLE.PRG's 555KB to slot 0, so it has to be backed by that much
 * contiguous memory at exactly 0x80068800. It is -- scratchpad.c maps 2MB of PS1
 * main RAM at 0x80000000. (The loaded image is redundant natively, since every
 * overlay is statically linked in, but letting the load happen for real keeps
 * the boot path unmodified and costs nothing.) */
void* const vs_overlay_slots[5] = {
    (void*)0x80068800, (void*)0x800F9800,
    (void*)0x80102800, (void*)0x800F9800, (void*)0x800F9800
};

/* Video standard. The port is locked to NTSC timing by libetc's 60Hz pacer. */
int SetVideoMode(int mode)
{
    static int cur = 0;
    int old = cur;
    cur = mode;
    return old;
}
int GetVideoMode(void) { return 0; }

/* GPU debug level -- retail passes 0. */
int SetGraphDebug(int level) { (void)level; return 0; }
int GetGraphDebug(void) { return 0; }

/* Memory card. The card itself is implemented in libapi.c over a real .mcd
 * image; these are the BIOS-side init calls, which have nothing to set up. */
void InitCARD(int pad_enable) { (void)pad_enable; }
void StartCARD(void) { }
void StopCARD(void) { }
void _bu_init(void) { }

/* ---- root counters (libapi RCnt) ----
 *
 * Hardware has four: dotclock, hblank, system-clock/8, and vblank. The game
 * uses them for timing, so rather than returning a constant (which stalls any
 * loop waiting for the count to advance) these are driven from the host
 * monotonic clock at the counters' real rates. */
#include <time.h>

#define RCNT_MASK 0xFFFF

static unsigned long long rcnt_start_ns;
static int rcnt_running[4];

static unsigned long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

/* spec encodes the counter in its low bits (RCntCNT0..3). */
static int rcnt_of(int spec) { return spec & 3; }

/* Ticks per second: CNT0 dotclock ~3.4MHz, CNT1 hblank 15.734kHz,
 * CNT2 system clock/8 ~4.23MHz, CNT3 vblank 60Hz. */
static double rcnt_hz(int c)
{
    switch (c) {
    case 0:  return 3406000.0;
    case 1:  return 15734.0;
    case 2:  return 33868800.0 / 8.0;
    default: return 60.0;
    }
}

int SetRCnt(int spec, unsigned short target, int mode)
{
    (void)target; (void)mode;
    if (!rcnt_start_ns) rcnt_start_ns = now_ns();
    rcnt_running[rcnt_of(spec)] = 1;
    return 1;
}

int StartRCnt(int spec) { return SetRCnt(spec, 0, 0); }
int StopRCnt(int spec) { rcnt_running[rcnt_of(spec)] = 0; return 1; }
int ResetRCnt(int spec) { (void)spec; rcnt_start_ns = now_ns(); return 1; }

unsigned short GetRCnt(int spec)
{
    int c = rcnt_of(spec);
    unsigned long long dt;
    if (!rcnt_start_ns) rcnt_start_ns = now_ns();
    dt = now_ns() - rcnt_start_ns;
    return (unsigned short)((unsigned long long)(dt * rcnt_hz(c) / 1e9) & RCNT_MASK);
}
