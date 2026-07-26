/* Host audio output. Takes the software SPU's stereo mix and plays it.
 *
 * SDL2 is dlopen'd rather than linked, for the same reason present.c does it:
 * the port has to build and run on a machine with no SDL at all, and a missing
 * sound card is not a reason to fail. Everything here degrades to "no sound"
 * without affecting anything else.
 *
 * This deliberately does NOT share present.c's SDL handle or its
 * initialisation. Audio and video are independent: a headless run
 * (`VS_HEADLESS=1`) should still be able to make noise, which is exactly the
 * configuration a WSL session without a window wants. dlopen is refcounted, so
 * opening libSDL2 a second time costs nothing, and SDL_Init is additive across
 * subsystems.
 *
 * Queue rather than callback. SDL's callback runs on its own thread, and
 * everything else in this port is single-threaded around the game's frame loop;
 * a callback would need a lock around the SPU register block that the game
 * writes at any moment. SDL_QueueAudio pushes from the game's own thread
 * instead, once per vblank, which is 735 frames at 44100Hz -- exactly one
 * vblank's worth, because the frame loop is paced to real time.
 *
 * WSL note: WSLg exports a PulseAudio socket at /mnt/wslg/PulseServer and sets
 * PULSE_SERVER, so SDL's pulse backend finds the Windows audio device with no
 * configuration. The 32-bit libpulse and libSDL2 packages have to be installed,
 * because the native build is -m32.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_INIT_AUDIO      0x00000010u
#define AUDIO_S16LSB        0x8010

typedef struct {
    int freq;
    unsigned short format;
    unsigned char channels;
    unsigned char silence;
    unsigned short samples;
    unsigned short padding;
    unsigned int size;
    void (*callback)(void*, unsigned char*, int);
    void* userdata;
} SDL_AudioSpec;

typedef int (*pfn_Init)(unsigned);
typedef int (*pfn_InitSubSystem)(unsigned);
typedef const char* (*pfn_GetError)(void);
typedef unsigned (*pfn_OpenAudioDevice)(
    const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int);
typedef void (*pfn_PauseAudioDevice)(unsigned, int);
typedef int (*pfn_QueueAudio)(unsigned, const void*, unsigned);
typedef unsigned (*pfn_GetQueuedAudioSize)(unsigned);
typedef void (*pfn_ClearQueuedAudio)(unsigned);
typedef void (*pfn_CloseAudioDevice)(unsigned);
typedef const char* (*pfn_GetCurrentAudioDriver)(void);

static struct {
    void* lib;
    unsigned dev;
    pfn_QueueAudio QueueAudio;
    pfn_GetQueuedAudioSize GetQueuedAudioSize;
    pfn_ClearQueuedAudio ClearQueuedAudio;
    pfn_PauseAudioDevice PauseAudioDevice;
    pfn_CloseAudioDevice CloseAudioDevice;
} snd;

static int sink_state; /* 0 = untried, 1 = open, -1 = unavailable */

/* Bounds on how much audio may sit queued ahead of the speaker.
 *
 * The frame loop is paced to real time, so queueing one vblank per vblank
 * tracks the device on average. It drifts, though: a slow frame queues late and
 * the queue drains, a fast one queues early and it grows. Left alone the queue
 * grows without bound and audio ends up seconds behind the picture.
 *
 * So: above HIGH, skip a push (the mix for that vblank is discarded, which is
 * inaudible at one frame); below LOW after a stall, top up with silence rather
 * than let the device underrun into a click. */
#define SINK_RATE   44100
#define SINK_BYTES_PER_FRAME 4
#define SINK_HIGH   (SINK_RATE * SINK_BYTES_PER_FRAME / 5)  /* 200ms */
#define SINK_LOW    (SINK_RATE * SINK_BYTES_PER_FRAME / 40) /*  25ms */

static void sink_close(void)
{
    if (sink_state == 1 && snd.CloseAudioDevice) {
        snd.PauseAudioDevice(snd.dev, 1);
        snd.CloseAudioDevice(snd.dev);
    }
    sink_state = -1;
}

/* Returns 1 once the device is open and accepting frames. */
int psx_audio_sink_open(void)
{
    SDL_AudioSpec want, have;
    pfn_Init Init;
    pfn_InitSubSystem InitSubSystem;
    pfn_GetError GetError;
    pfn_OpenAudioDevice OpenAudioDevice;
    pfn_GetCurrentAudioDriver GetCurrentAudioDriver;

    if (sink_state) return sink_state == 1;
    sink_state = -1;

    if (getenv("VS_NO_AUDIO")) {
        printf("[audio] VS_NO_AUDIO set -- no sound output\n");
        return 0;
    }

    snd.lib = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!snd.lib) snd.lib = dlopen("libSDL2.so", RTLD_NOW | RTLD_LOCAL);
    if (!snd.lib) {
        printf("[audio] SDL2 not available (%s) -- no sound output\n", dlerror());
        return 0;
    }

#define SYM(v, t, n)                                                                     \
    v = (t)dlsym(snd.lib, n);                                                            \
    if (!v) {                                                                            \
        printf("[audio] missing %s -- no sound output\n", n);                            \
        return 0;                                                                        \
    }
    SYM(Init, pfn_Init, "SDL_Init")
    SYM(InitSubSystem, pfn_InitSubSystem, "SDL_InitSubSystem")
    SYM(GetError, pfn_GetError, "SDL_GetError")
    SYM(OpenAudioDevice, pfn_OpenAudioDevice, "SDL_OpenAudioDevice")
    SYM(GetCurrentAudioDriver, pfn_GetCurrentAudioDriver, "SDL_GetCurrentAudioDriver")
    SYM(snd.QueueAudio, pfn_QueueAudio, "SDL_QueueAudio")
    SYM(snd.GetQueuedAudioSize, pfn_GetQueuedAudioSize, "SDL_GetQueuedAudioSize")
    SYM(snd.ClearQueuedAudio, pfn_ClearQueuedAudio, "SDL_ClearQueuedAudio")
    SYM(snd.PauseAudioDevice, pfn_PauseAudioDevice, "SDL_PauseAudioDevice")
    SYM(snd.CloseAudioDevice, pfn_CloseAudioDevice, "SDL_CloseAudioDevice")
#undef SYM

    /* InitSubSystem, not Init: present.c may already have brought SDL up for
     * video, and Init on an initialised library is fine but this is the honest
     * call. It also works standalone -- SDL initialises the core lazily. */
    if (InitSubSystem(SDL_INIT_AUDIO) != 0 && Init(SDL_INIT_AUDIO) != 0) {
        const char* err = GetError();
        printf("[audio] SDL audio init failed (%s) -- no sound output\n",
               err && *err ? err : "?");
        return 0;
    }

    memset(&want, 0, sizeof want);
    want.freq = SINK_RATE;
    want.format = AUDIO_S16LSB;
    want.channels = 2;
    want.samples = 1024;

    /* No format conversion allowed: the SPU mixes native 44.1kHz S16 stereo,
     * and taking exactly that keeps the queue arithmetic above honest. */
    snd.dev = OpenAudioDevice(0, 0, &want, &have, 0);
    if (snd.dev == 0) {
        const char* err = GetError();
        printf("[audio] no output device (%s) -- no sound output\n",
               err && *err ? err : "?");
        return 0;
    }

    snd.PauseAudioDevice(snd.dev, 0);
    sink_state = 1;
    atexit(sink_close);
    {
        const char* drv = GetCurrentAudioDriver();
        printf("[audio] %dHz stereo via SDL \"%s\"\n", have.freq, drv ? drv : "?");
    }
    return 1;
}

/* One vblank of mixed stereo frames, straight from the SPU. */
void psx_audio_sink_push(const short* frames, int nframes)
{
    unsigned queued;

    if (sink_state != 1) return;

    queued = snd.GetQueuedAudioSize(snd.dev);
    if (queued > (unsigned)SINK_HIGH) {
        return; /* running ahead: drop this vblank rather than add latency */
    }
    if (queued < (unsigned)SINK_LOW && queued > 0) {
        /* Recovering from a stall. Pad so the device does not run dry between
         * this push and the next one. */
        static const short quiet[512 * 2];
        snd.QueueAudio(snd.dev, quiet, sizeof quiet);
    }
    snd.QueueAudio(snd.dev, frames, (unsigned)(nframes * SINK_BYTES_PER_FRAME));
}

/* Drop anything still queued. Used when the game jumps (end of the FMV, a
 * scene change) so stale audio does not play over the new scene. */
void psx_audio_sink_flush(void)
{
    if (sink_state == 1) snd.ClearQueuedAudio(snd.dev);
}
