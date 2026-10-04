/* libetc: frame pacing. Per the port plan the game stays locked to the
 * original 60Hz; a host loop paces frames and counts vblanks.
 *
 * The vblank boundary is also where the host window presents and where input
 * is sampled -- on hardware this is exactly when the framebuffer flips and the
 * pad DMA has landed, so PS1 code that already calls VSync() gets a live window
 * and a live controller with no changes at the call site. */
#include <stdint.h>
#include <psx_present.h>
#include <psx_host.h>

/* Publishes host input into the game's pad buffer (native/platform/libpad.c). */
void psx_pad_update(void);
/* Delivers CD sectors (native/platform/libds.c). On hardware these arrive as
 * drive interrupts; the frame boundary is the natural place for them, and it is
 * where the game already pumps its CD queue from
 * (vs_main_gametimeUpdate -> VSync -> _processCdQueue). */
void psx_cd_pump(void);
/* Mixes a frame of audio (native/platform/libspu.c). */
void psx_spu_pump(void);
/* Raises root-counter interrupts, which is what ticks the sound driver
 * (native/platform/kernel.c). */
void psx_rcnt_pump(void);

static uint64_t start_ns;
static uint64_t next_vblank_ns;
static void (*vsync_cb)(void);

static uint64_t now_ns(void)
{
    return psx_host_now_ns();
}

/* NTSC vertical blank period. */
#define FRAME_NS 16683333ull

static void start_clock(void)
{
    if (!start_ns) {
        start_ns = now_ns();
        next_vblank_ns = start_ns;
    }
}

/* The vblank counter is FREE-RUNNING, driven by elapsed time rather than by how
 * often the game happens to call VSync -- because on hardware it is incremented
 * by the vblank interrupt whether anyone is looking or not.
 *
 * This is load-bearing, not pedantry. The movie player starts the intro, records
 * VSync(-1), and returns once 0x8F5 (2293) vblanks have passed -- 38 seconds,
 * which is exactly how long TITLE.STR runs. But it only calls VSync once per
 * *movie* frame, about 15 times a second, so a counter that advanced per call
 * would need two and a half minutes to reach the same total and the intro would
 * never end. The same idiom paces _stopMovieIfComplete and the title screen's
 * idle timeout. */
static uint64_t vblanks_elapsed(void)
{
    start_clock();
    return (now_ns() - start_ns) / FRAME_NS;
}

static void wait_one_vblank(void)
{
    start_clock();

    /* Show the frame we just drew and refresh the pad, then pace. No-ops when
     * running headless (no window was opened). */
    psx_present_pump();
    psx_pad_update();
    psx_cd_pump();
    psx_rcnt_pump();
    psx_spu_pump();

    next_vblank_ns += FRAME_NS;
    for (;;) {
        uint64_t t = now_ns();
        if (t >= next_vblank_ns) {
            /* Ran long: resync rather than trying to claw back the lost time,
             * which would spin through several frames with no delay at all. */
            if (t - next_vblank_ns > FRAME_NS) next_vblank_ns = t;
            break;
        }
        psx_host_sleep_ns(next_vblank_ns - t);
    }
    if (vsync_cb) vsync_cb();
}

/* mode <0 : return total vblank count
 * mode  0 : wait for the next vblank
 * mode >0 : wait for that many vblanks */
int VSync(int mode)
{
    int i, n;
    if (mode < 0) return (int)vblanks_elapsed();
    n = (mode == 0) ? 1 : mode;
    for (i = 0; i < n; i++) wait_one_vblank();
    return (int)vblanks_elapsed();
}

int VSyncCallback(void (*f)(void))
{
    vsync_cb = f;
    return 0;
}
