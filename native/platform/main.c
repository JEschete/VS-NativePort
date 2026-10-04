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
#include <psx_scratchpad.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdlib.h>

extern void vs_main_exec(void);

static void dump_backtrace(void)
{
    void* frames[24];
    USHORT count = CaptureStackBackTrace(0, 24, frames, NULL);
    USHORT index;
    fprintf(stderr, "\n[trap] Windows stack (%u frames):\n", (unsigned)count);
    for (index = 0; index < count; index++) fprintf(stderr, "  %p\n", frames[index]);
}

/* Snapshot whatever is on screen. Headless runs are the only way to inspect the
 * boot right now, so both a normal exit and the watchdog leave an image behind. */
static void dump_screen(void)
{
    extern int psx_vram_write_ppm(const char* path, int x, int y, int w, int h);
    extern unsigned short* psx_vram(void);
    const char* raw;

    psx_vram_write_ppm("native/build/boot_vram.ppm", 0, 0, 1024, 512);
    fprintf(stderr, "[boot] wrote native/build/boot_vram.ppm\n");

    /* VS_VRAM_RAW=<path> also writes VRAM verbatim. The PPM above reads every
     * halfword as a 15-bit pixel, which is wrong for regions holding 24-bit
     * colour -- the movie framebuffers, notably -- so inspecting those needs the
     * untranslated bytes. */
    raw = getenv("VS_VRAM_RAW");
    if (raw && *raw) {
        FILE* f = fopen(raw, "wb");
        if (f) {
            fwrite(psx_vram(), 2, 1024 * 512, f);
            fclose(f);
            fprintf(stderr, "[boot] wrote %s (raw 1024x512x16bpp)\n", raw);
        }
    }
}

static VOID CALLBACK watchdog_fired(PVOID context, BOOLEAN fired)
{
    HANDLE dump;
    (void)context;
    (void)fired;
    fprintf(stderr, "\n[trap] watchdog: still running after VS_WATCHDOG seconds\n");
    dump_screen();
    dump = CreateFileA("native/build/watchdog.dmp", GENERIC_WRITE, 0, NULL,
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dump != INVALID_HANDLE_VALUE) {
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump,
                         MiniDumpNormal, NULL, NULL, NULL);
        CloseHandle(dump);
    }
    ExitProcess(3);
}

static LONG WINAPI unhandled_exception(EXCEPTION_POINTERS* exception)
{
    fprintf(stderr, "[trap] Windows exception 0x%08lX at %p\n",
            exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress);
    dump_screen();
    dump_backtrace();
    ExitProcess(1);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void install_trap_handlers(void)
{
    static HANDLE watchdog;
    const char* wd = getenv("VS_WATCHDOG");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(unhandled_exception);
    if (wd && atoi(wd) > 0) {
        unsigned long seconds = strtoul(wd, NULL, 10);
        if (seconds > MAXDWORD / 1000) seconds = MAXDWORD / 1000;
        if (!CreateTimerQueueTimer(&watchdog, NULL, watchdog_fired, NULL,
                                  seconds * 1000, 0, WT_EXECUTEONLYONCE)) {
            fprintf(stderr, "[trap] could not install Windows watchdog\n");
            exit(1);
        }
    }
}

int main(void)
{
    /* Unbuffered: the boot sequence is long and any diagnosis of a hang or
     * fault depends on seeing how far it got, which block buffering hides. */
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "[boot] could not configure console output\n");
        return 1;
    }
    install_trap_handlers();
    psx_scratchpad_init();
    printf("[boot] Vagrant Story - native\n");
    psx_present_init("Vagrant Story (native)", 3);
    vs_main_exec();
    psx_present_shutdown();
    printf("[boot] returned from vs_main_exec\n");
    return 0;
}
