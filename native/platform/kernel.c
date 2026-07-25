/* Odds and ends from the BIOS / libgpu / libapi boot path that have no host
 * equivalent, kept together so the interesting layers stay uncluttered.
 *
 * All of these are called from _sysInit() / _sysReinit() in
 * src/SLUS_010.40/main.c. None of them can do anything meaningful natively:
 * there is no video encoder to configure, no BIOS to hand a card filesystem to.
 * They exist so the retail boot sequence runs unmodified. */
#include <stdio.h>
#include <stdlib.h>

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

/* A counter set to interrupt mode raises an event every time it reaches its
 * target. This is what drives the sound driver: StartSound arms counter 2 at
 * 0x44E8 (4233600 / 17640 = exactly 240Hz, the Akao tick rate) and registers
 * func_80019A58 as an EvMdINTR handler on it. Without the interrupt the
 * sequencer never advances, so no note is ever keyed on and the SPU stays
 * silent no matter how well the mixer works. */
#define RCntMdINTR_BIT 0x1000
#define RCNT_CLASS(c)  (0xF2000000UL | (unsigned long)(c))
#define EvSpINT_SPEC   0x0002UL

extern void DeliverEvent(unsigned long cls, unsigned long spec);

static unsigned short    rcnt_target[4];
static int               rcnt_intr[4];
static unsigned long long rcnt_last_ns[4];

int SetRCnt(int spec, unsigned short target, int mode)
{
    int c = rcnt_of(spec);
    if (!rcnt_start_ns) rcnt_start_ns = now_ns();
    rcnt_target[c] = target;
    rcnt_intr[c] = (mode & RCntMdINTR_BIT) != 0;
    rcnt_last_ns[c] = now_ns();
    rcnt_running[c] = 1;
    if (getenv("VS_RCNT_TRACE"))
        printf("[rcnt] SetRCnt cnt%d target=%u intr=%d\n", c, (unsigned)target, rcnt_intr[c]);
    return 1;
}

/* Starting a counter must not clear the target the game just programmed. */
int StartRCnt(int spec)
{
    int c = rcnt_of(spec);
    if (!rcnt_start_ns) rcnt_start_ns = now_ns();
    rcnt_last_ns[c] = now_ns();
    rcnt_running[c] = 1;
    return 1;
}

/* Raise whatever counter interrupts are due. Called once per frame from
 * VSync(), so a 240Hz counter delivers four ticks per frame. */
void psx_rcnt_pump(void)
{
    unsigned long long now = now_ns();
    int c;

    for (c = 0; c < 4; c++) {
        unsigned long long period_ns, elapsed;
        int n;

        if (!rcnt_running[c] || !rcnt_intr[c] || rcnt_target[c] == 0) continue;

        period_ns = (unsigned long long)(rcnt_target[c] * 1e9 / rcnt_hz(c));
        if (period_ns == 0) continue;

        elapsed = now - rcnt_last_ns[c];
        n = (int)(elapsed / period_ns);
        if (n <= 0) continue;
        /* Cap the catch-up so a long stall cannot fire hundreds of ticks at
         * once and race the sequencer ahead of the music. */
        if (n > 8) n = 8;
        rcnt_last_ns[c] += (unsigned long long)n * period_ns;

        {
            static unsigned long fired;
            if (getenv("VS_RCNT_TRACE") && (fired += n) % 240 < n)
                printf("[rcnt] cnt%d delivered %lu ticks\n", c, fired);
        }
        while (n-- > 0) DeliverEvent(RCNT_CLASS(c), EvSpINT_SPEC);
    }
}

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
