/* libetc: frame pacing. Per the port plan the game stays locked to the
 * original 60Hz; a host loop paces frames and counts vblanks.
 *
 * The vblank boundary is also where the host window presents and where input
 * is sampled -- on hardware this is exactly when the framebuffer flips and the
 * pad DMA has landed, so PS1 code that already calls VSync() gets a live window
 * and a live controller with no changes at the call site. */
#include <time.h>
#include <stdint.h>
#include <psx_present.h>

/* Publishes host input into the game's pad buffer (native/platform/libpad.c). */
void psx_pad_update(void);
/* Delivers CD sectors (native/platform/libds.c). On hardware these arrive as
 * drive interrupts; the frame boundary is the natural place for them, and it is
 * where the game already pumps its CD queue from
 * (vs_main_gametimeUpdate -> VSync -> _processCdQueue). */
void psx_cd_pump(void);

static uint64_t vblank_count;
static uint64_t start_ns;
static void (*vsync_cb)(void);

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* NTSC vertical blank period. */
#define FRAME_NS 16683333ull

static void wait_one_vblank(void)
{
    uint64_t target;
    if (!start_ns) start_ns = now_ns();
    vblank_count++;

    /* Show the frame we just drew and refresh the pad, then pace. No-ops when
     * running headless (no window was opened). */
    psx_present_pump();
    psx_pad_update();
    psx_cd_pump();

    target = start_ns + vblank_count * FRAME_NS;
    for (;;) {
        uint64_t t = now_ns();
        if (t >= target) break;
        {
            struct timespec rq;
            uint64_t d = target - t;
            rq.tv_sec = (time_t)(d / 1000000000ull);
            rq.tv_nsec = (long)(d % 1000000000ull);
            nanosleep(&rq, 0);
        }
    }
    if (vsync_cb) vsync_cb();
}

/* mode <0 : return total vblank count
 * mode  0 : wait for the next vblank
 * mode >0 : wait for that many vblanks */
int VSync(int mode)
{
    int i, n;
    if (mode < 0) return (int)vblank_count;
    n = (mode == 0) ? 1 : mode;
    for (i = 0; i < n; i++) wait_one_vblank();
    return (int)vblank_count;
}

int VSyncCallback(void (*f)(void))
{
    vsync_cb = f;
    return 0;
}
