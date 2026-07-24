/* Native entry point: boot the real game.
 *
 * vs_main_exec() is the game's own top-level entry (src/SLUS_010.40/main.c):
 *
 *     __main(); _sysInit(); vs_main_execTitle();
 *
 * and vs_main_execTitle() is
 *
 *     _sysReinit(); _loadTitlePrg(); vs_title_exec(); ... vs_battle_exec();
 *
 * so running it drives the retail boot sequence -- system init, load TITLE.PRG
 * off the disc, play the intro, and land on the title menu -- through the
 * game's own code. Everything below it (libds / libspu / libcd) is the platform
 * layer's job.
 */
#include <psx_present.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern void vs_main_exec(void);

/* Self-service backtrace. The boot sequence is long, mostly inside decompiled
 * game code, and WSL blocks ptrace-attaching to a running process -- so a hang
 * or fault is otherwise invisible. SIGQUIT prints where we are (send it with
 * `timeout -s QUIT`); SIGSEGV/SIGABRT print where it died.
 * Resolve the addresses with: addr2line -f -e native/build/vagrant-native <a> */
static void dump_backtrace(int sig)
{
    void* frames[24];
    int n = backtrace(frames, 24);
    fprintf(stderr, "\n[trap] signal %d -- backtrace (%d frames):\n", sig, n);
    backtrace_symbols_fd(frames, n, 2);
    if (sig != SIGQUIT) _exit(128 + sig);
}

/* Snapshot whatever is on screen. Headless runs are the only way to inspect the
 * boot right now, so both a normal exit and the watchdog leave an image behind. */
static void dump_screen(void)
{
    extern int psx_vram_write_ppm(const char* path, int x, int y, int w, int h);
    psx_vram_write_ppm("native/build/boot_vram.ppm", 0, 0, 1024, 512);
    fprintf(stderr, "[boot] wrote native/build/boot_vram.ppm\n");
}

static void watchdog_fired(int sig)
{
    fprintf(stderr, "\n[trap] watchdog: still running after VS_WATCHDOG seconds\n");
    dump_screen();
    dump_backtrace(sig);
    _exit(3);
}

static void install_trap_handlers(void)
{
    const char* wd = getenv("VS_WATCHDOG");
    signal(SIGQUIT, dump_backtrace);
    signal(SIGSEGV, dump_backtrace);
    signal(SIGABRT, dump_backtrace);
    /* VS_WATCHDOG=N: bail out after N seconds with a backtrace. The boot spends
     * most of its time in decompiled game code polling for a CD read or an SPU
     * transfer, so "stuck" is the normal failure mode and this is the only
     * practical way to see where -- WSL blocks ptrace-attaching. */
    if (wd && atoi(wd) > 0) {
        signal(SIGALRM, watchdog_fired);
        alarm((unsigned)atoi(wd));
    }
}

int main(void)
{
    /* Line-buffered: the boot sequence is long and any diagnosis of a hang or
     * fault depends on seeing how far it got, which block buffering hides. */
    setvbuf(stdout, 0, _IOLBF, 0);
    install_trap_handlers();
    printf("[boot] Vagrant Story - native\n");
    psx_present_init("Vagrant Story (native)", 3);
    vs_main_exec();
    psx_present_shutdown();
    printf("[boot] returned from vs_main_exec\n");
    return 0;
}
