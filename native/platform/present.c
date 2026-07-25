/* SDL2 window + host input. See native/include/psx_present.h for why SDL is
 * dlopen'd instead of linked.
 *
 * Input sources, merged (OR'd) every frame:
 *   1. keyboard              -- always available under WSLg
 *   2. SDL game controller   -- used when the host exposes a real pad device
 *   3. the Windows pad bridge (native/tools/pad_bridge.py) over TCP
 *
 * (3) exists because WSL2 has no /dev/input at all: USB and Bluetooth devices
 * are not passed through to the VM, so a DualSense connected to Windows is
 * invisible to a Linux binary. The bridge reads the pad on the Windows side and
 * streams its state to us. WSL2 forwards Windows localhost connections to
 * listeners inside the VM, so we are the server and the bridge is the client.
 */
#include <psx_present.h>
#include <psx_gpu.h>

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/* ------------------------------------------------------------------ */
/* PS1 pad bits (mirrors include/psx/libetc.h, which the platform layer
 * does not include -- it is built as ordinary hosted C).               */
/* ------------------------------------------------------------------ */
#define PAD_L2        (1u << 0)
#define PAD_R2        (1u << 1)
#define PAD_L1        (1u << 2)
#define PAD_R1        (1u << 3)
#define PAD_TRIANGLE  (1u << 4)
#define PAD_CIRCLE    (1u << 5)
#define PAD_CROSS     (1u << 6)
#define PAD_SQUARE    (1u << 7)
#define PAD_SELECT    (1u << 8)
#define PAD_L3        (1u << 9)
#define PAD_R3        (1u << 10)
#define PAD_START     (1u << 11)
#define PAD_UP        (1u << 12)
#define PAD_RIGHT     (1u << 13)
#define PAD_DOWN      (1u << 14)
#define PAD_LEFT      (1u << 15)

/* ------------------------------------------------------------------ */
/* SDL2 constants we need (stable ABI values -- no headers involved).   */
/* ------------------------------------------------------------------ */
#define SDL_INIT_VIDEO           0x00000020u
#define SDL_INIT_GAMECONTROLLER  0x00002000u
#define SDL_WINDOWPOS_UNDEFINED  0x1FFF0000
#define SDL_WINDOW_SHOWN         0x00000004u
#define SDL_WINDOW_RESIZABLE     0x00000020u
#define SDL_PIXELFORMAT_ARGB8888 0x16362004u
#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_QUIT_EVENT           0x100u

/* SDL_Scancode values (SDL_scancode.h). */
#define SC_A 4
#define SC_D 7
#define SC_E 8
#define SC_Q 20
#define SC_S 22
#define SC_W 26
#define SC_X 27
#define SC_Z 29
#define SC_1 30
#define SC_2 31
#define SC_3 32
#define SC_RETURN 40
#define SC_ESCAPE 41
#define SC_BACKSPACE 42
#define SC_TAB 43
#define SC_SPACE 44
#define SC_F2 59
#define SC_RIGHT 79
#define SC_LEFT 80
#define SC_DOWN 81
#define SC_UP 82
#define SC_RSHIFT 229

/* SDL_GameControllerButton / Axis. */
#define CB_A 0
#define CB_B 1
#define CB_X 2
#define CB_Y 3
#define CB_BACK 4
#define CB_START 6
#define CB_LEFTSTICK 7
#define CB_RIGHTSTICK 8
#define CB_LEFTSHOULDER 9
#define CB_RIGHTSHOULDER 10
#define CB_DPAD_UP 11
#define CB_DPAD_DOWN 12
#define CB_DPAD_LEFT 13
#define CB_DPAD_RIGHT 14
#define CA_LEFTX 0
#define CA_LEFTY 1
#define CA_RIGHTX 2
#define CA_RIGHTY 3
#define CA_TRIGGERLEFT 4
#define CA_TRIGGERRIGHT 5

typedef int            (*pfn_Init)(unsigned);
typedef void           (*pfn_Quit)(void);
typedef const char*    (*pfn_GetError)(void);
typedef void*          (*pfn_CreateWindow)(const char*, int, int, int, int, unsigned);
typedef void           (*pfn_DestroyWindow)(void*);
typedef void*          (*pfn_CreateRenderer)(void*, int, unsigned);
typedef void*          (*pfn_CreateTexture)(void*, unsigned, int, int, int);
typedef int            (*pfn_UpdateTexture)(void*, const void*, const void*, int);
typedef int            (*pfn_RenderClear)(void*);
typedef int            (*pfn_RenderCopy)(void*, void*, const void*, const void*);
typedef void           (*pfn_RenderPresent)(void*);
typedef int            (*pfn_RenderSetLogicalSize)(void*, int, int);
typedef int            (*pfn_PollEvent)(void*);
typedef const unsigned char* (*pfn_GetKeyboardState)(int*);
typedef int            (*pfn_SetHint)(const char*, const char*);
typedef int            (*pfn_NumJoysticks)(void);
typedef int            (*pfn_IsGameController)(int);
typedef void*          (*pfn_GameControllerOpen)(int);
typedef unsigned char  (*pfn_GameControllerGetButton)(void*, int);
typedef short          (*pfn_GameControllerGetAxis)(void*, int);
typedef const char*    (*pfn_GameControllerName)(void*);
typedef void           (*pfn_GameControllerUpdate)(void);

static struct {
    void* lib;
    pfn_Init Init;
    pfn_Quit Quit;
    pfn_GetError GetError;
    pfn_CreateWindow CreateWindow;
    pfn_DestroyWindow DestroyWindow;
    pfn_CreateRenderer CreateRenderer;
    pfn_CreateTexture CreateTexture;
    pfn_UpdateTexture UpdateTexture;
    pfn_RenderClear RenderClear;
    pfn_RenderCopy RenderCopy;
    pfn_RenderPresent RenderPresent;
    pfn_RenderSetLogicalSize RenderSetLogicalSize;
    pfn_PollEvent PollEvent;
    pfn_GetKeyboardState GetKeyboardState;
    pfn_SetHint SetHint;
    pfn_NumJoysticks NumJoysticks;
    pfn_IsGameController IsGameController;
    pfn_GameControllerOpen GameControllerOpen;
    pfn_GameControllerGetButton GameControllerGetButton;
    pfn_GameControllerGetAxis GameControllerGetAxis;
    pfn_GameControllerName GameControllerName;
    pfn_GameControllerUpdate GameControllerUpdate;
} sdl;

static void*  win;
static void*  ren;
static void*  tex;
static void*  gamepad;
static int    active;
static int    quit_req;
/* Set by callers that run their own loop and poll psx_present_quit_requested()
 * themselves, so they can shut down in their own time. Everything else is
 * driven by the game's loop, which never returns, so a quit request there has
 * to end the process from inside the pump. */
static int    coop_quit;
static int    src_x, src_y, src_w = 320, src_h = 240;
static int    src_rgb24;            /* display region holds 24-bit colour */
static unsigned* argb;              /* conversion scratch, src_w * src_h */
static int    argb_cap;
static unsigned frame_no;

static int    shot_req, shot_held;

static unsigned      host_buttons;
static unsigned char host_sticks[4] = { 128, 128, 128, 128 };
static int           host_connected;
static const char*   host_source = "keyboard";

/* ------------------------------------------------------------------ */
/* Windows pad bridge (TCP server on 127.0.0.1).                        */
/* ------------------------------------------------------------------ */
#define BRIDGE_PORT_DEFAULT 27015
#define BRIDGE_MAGIC 0x44505356u        /* 'V','S','P','D' little-endian */
#define BRIDGE_PKT   12

static int bridge_listen = -1;
static int bridge_conn   = -1;
static unsigned char bridge_buf[BRIDGE_PKT * 8];
static int bridge_len;
static unsigned      bridge_buttons;
static unsigned char bridge_sticks[4] = { 128, 128, 128, 128 };
static int           bridge_live;
static int           bridge_idle;       /* frames since last packet */

static void bridge_open(void)
{
    struct sockaddr_in a;
    int port = BRIDGE_PORT_DEFAULT;
    const char* e = getenv("VS_PAD_PORT");
    int yes = 1;

    if (e && atoi(e) > 0) port = atoi(e);
    if (getenv("VS_NO_PAD_BRIDGE")) return;

    bridge_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (bridge_listen < 0) return;
    setsockopt(bridge_listen, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    fcntl(bridge_listen, F_SETFL, O_NONBLOCK);

    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((unsigned short)port);
    if (bind(bridge_listen, (struct sockaddr*)&a, sizeof a) < 0 ||
        listen(bridge_listen, 1) < 0) {
        close(bridge_listen);
        bridge_listen = -1;
        return;
    }
    printf("[present] pad bridge listening on 127.0.0.1:%d "
           "(run native/tools/pad_bridge.py on Windows)\n", port);
}

/* Decode every complete packet available; keep the most recent. */
static void bridge_poll(void)
{
    int n;

    if (bridge_listen < 0) return;

    if (bridge_conn < 0) {
        int c = accept(bridge_listen, 0, 0);
        if (c >= 0) {
            int yes = 1;
            fcntl(c, F_SETFL, O_NONBLOCK);
            setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
            bridge_conn = c;
            bridge_len = 0;
            printf("[present] pad bridge connected\n");
        }
        return;
    }

    for (;;) {
        n = (int)recv(bridge_conn, bridge_buf + bridge_len,
                      (size_t)(sizeof bridge_buf - bridge_len), 0);
        if (n > 0) {
            bridge_len += n;
        } else if (n == 0) {
            printf("[present] pad bridge disconnected\n");
            close(bridge_conn);
            bridge_conn = -1;
            bridge_live = 0;
            return;
        } else {
            if (errno == EINTR) continue;
            break;                              /* EAGAIN: drained */
        }

        /* Consume whole packets; resync on a bad magic. */
        while (bridge_len >= BRIDGE_PKT) {
            unsigned magic;
            memcpy(&magic, bridge_buf, 4);
            if (magic != BRIDGE_MAGIC) {
                memmove(bridge_buf, bridge_buf + 1, (size_t)(bridge_len - 1));
                bridge_len--;
                continue;
            }
            bridge_buttons = (unsigned)(bridge_buf[4] | (bridge_buf[5] << 8));
            bridge_sticks[0] = bridge_buf[6];
            bridge_sticks[1] = bridge_buf[7];
            bridge_sticks[2] = bridge_buf[8];
            bridge_sticks[3] = bridge_buf[9];
            bridge_live = (bridge_buf[10] & 1);
            bridge_idle = 0;
            memmove(bridge_buf, bridge_buf + BRIDGE_PKT,
                    (size_t)(bridge_len - BRIDGE_PKT));
            bridge_len -= BRIDGE_PKT;
        }
        if (bridge_len == (int)sizeof bridge_buf) bridge_len = 0;  /* garbage guard */
    }

    if (++bridge_idle > 120) bridge_live = 0;   /* bridge went quiet */
}

/* ------------------------------------------------------------------ */
/* Input collection                                                     */
/* ------------------------------------------------------------------ */
static unsigned char axis_to_pad(short v)
{
    int u = (v + 32768) >> 8;           /* -32768..32767 -> 0..255 */
    if (u < 0) u = 0;
    if (u > 255) u = 255;
    return (unsigned char)u;
}

static void poll_keyboard(unsigned* btn)
{
    const unsigned char* k = sdl.GetKeyboardState(0);
    if (!k) return;

    if (k[SC_UP])    *btn |= PAD_UP;
    if (k[SC_DOWN])  *btn |= PAD_DOWN;
    if (k[SC_LEFT])  *btn |= PAD_LEFT;
    if (k[SC_RIGHT]) *btn |= PAD_RIGHT;

    if (k[SC_Z]) *btn |= PAD_CIRCLE;    /* confirm */
    if (k[SC_X]) *btn |= PAD_CROSS;     /* cancel  */
    if (k[SC_A]) *btn |= PAD_SQUARE;
    if (k[SC_S]) *btn |= PAD_TRIANGLE;

    if (k[SC_Q]) *btn |= PAD_L1;
    if (k[SC_E]) *btn |= PAD_R1;
    if (k[SC_1]) *btn |= PAD_L2;
    if (k[SC_3]) *btn |= PAD_R2;

    if (k[SC_RETURN])    *btn |= PAD_START;
    if (k[SC_RSHIFT] || k[SC_BACKSPACE]) *btn |= PAD_SELECT;
    if (k[SC_W]) *btn |= PAD_L3;
    if (k[SC_D]) *btn |= PAD_R3;

    if (k[SC_ESCAPE]) quit_req = 1;

    /* F2: edge-triggered screenshot request. */
    if (k[SC_F2]) {
        if (!shot_held) { shot_held = 1; shot_req = 1; }
    } else {
        shot_held = 0;
    }
}

int psx_present_screenshot_requested(void)
{
    int r = shot_req;
    shot_req = 0;
    return r;
}

static void poll_gamepad(unsigned* btn, unsigned char sticks[4])
{
    if (!gamepad) {
        /* Re-scan occasionally; a pad may be plugged in mid-run. */
        if ((frame_no % 60) == 0 && sdl.NumJoysticks) {
            int i, n = sdl.NumJoysticks();
            for (i = 0; i < n; i++) {
                if (sdl.IsGameController(i)) {
                    gamepad = sdl.GameControllerOpen(i);
                    if (gamepad) {
                        const char* nm = sdl.GameControllerName(gamepad);
                        printf("[present] SDL gamepad: %s\n", nm ? nm : "(unnamed)");
                        host_source = "SDL gamepad";
                        break;
                    }
                }
            }
        }
        return;
    }

    sdl.GameControllerUpdate();
    if (sdl.GameControllerGetButton(gamepad, CB_DPAD_UP))    *btn |= PAD_UP;
    if (sdl.GameControllerGetButton(gamepad, CB_DPAD_DOWN))  *btn |= PAD_DOWN;
    if (sdl.GameControllerGetButton(gamepad, CB_DPAD_LEFT))  *btn |= PAD_LEFT;
    if (sdl.GameControllerGetButton(gamepad, CB_DPAD_RIGHT)) *btn |= PAD_RIGHT;
    /* SDL's A/B/X/Y are positional: A=south=cross, B=east=circle,
     * X=west=square, Y=north=triangle. */
    if (sdl.GameControllerGetButton(gamepad, CB_A)) *btn |= PAD_CROSS;
    if (sdl.GameControllerGetButton(gamepad, CB_B)) *btn |= PAD_CIRCLE;
    if (sdl.GameControllerGetButton(gamepad, CB_X)) *btn |= PAD_SQUARE;
    if (sdl.GameControllerGetButton(gamepad, CB_Y)) *btn |= PAD_TRIANGLE;
    if (sdl.GameControllerGetButton(gamepad, CB_LEFTSHOULDER))  *btn |= PAD_L1;
    if (sdl.GameControllerGetButton(gamepad, CB_RIGHTSHOULDER)) *btn |= PAD_R1;
    if (sdl.GameControllerGetButton(gamepad, CB_BACK))       *btn |= PAD_SELECT;
    if (sdl.GameControllerGetButton(gamepad, CB_START))      *btn |= PAD_START;
    if (sdl.GameControllerGetButton(gamepad, CB_LEFTSTICK))  *btn |= PAD_L3;
    if (sdl.GameControllerGetButton(gamepad, CB_RIGHTSTICK)) *btn |= PAD_R3;
    if (sdl.GameControllerGetAxis(gamepad, CA_TRIGGERLEFT)  > 8000) *btn |= PAD_L2;
    if (sdl.GameControllerGetAxis(gamepad, CA_TRIGGERRIGHT) > 8000) *btn |= PAD_R2;

    sticks[0] = axis_to_pad(sdl.GameControllerGetAxis(gamepad, CA_LEFTX));
    sticks[1] = axis_to_pad(sdl.GameControllerGetAxis(gamepad, CA_LEFTY));
    sticks[2] = axis_to_pad(sdl.GameControllerGetAxis(gamepad, CA_RIGHTX));
    sticks[3] = axis_to_pad(sdl.GameControllerGetAxis(gamepad, CA_RIGHTY));
    host_connected = 1;
}

/* ------------------------------------------------------------------ */
/* Presentation                                                         */
/* ------------------------------------------------------------------ */
int psx_gpu_display_enabled(void);

static void blit_vram(void)
{
    const uint16_t* vram = psx_vram();
    int x, y;

    /* Display off. The game blanks the screen across transitions and expects
     * whatever it draws meanwhile to stay hidden, so show black rather than
     * the contents of VRAM. */
    if (!psx_gpu_display_enabled()) {
        memset(argb, 0, (size_t)src_w * src_h * sizeof *argb);
        return;
    }

    /* 24-bit mode. VRAM is addressed in halfwords whatever the display depth,
     * so a row of `src_w` pixels occupies src_w*3 bytes starting at the same
     * halfword; there is no separate 24-bit framebuffer. The game turns this on
     * for the intro movie (disp.isrgb24 = 1 in _playIntroMovie), having already
     * scaled the width by 2/3 -- 480 halfwords of MDEC output is 320 pixels.
     * Reading those bytes as 15-bit pixels is what made the movie come out as
     * coloured noise with the shapes faintly visible through it. */
    if (src_rgb24) {
        for (y = 0; y < src_h; y++) {
            const unsigned char* row =
                (const unsigned char*)(vram + (size_t)(src_y + y) * PSX_VRAM_W + src_x);
            unsigned* out = argb + (size_t)y * src_w;
            for (x = 0; x < src_w; x++) {
                out[x] = 0xFF000000u | ((unsigned)row[0] << 16)
                       | ((unsigned)row[1] << 8) | (unsigned)row[2];
                row += 3;
            }
        }
        return;
    }

    for (y = 0; y < src_h; y++) {
        const uint16_t* row = vram + (size_t)(src_y + y) * PSX_VRAM_W + src_x;
        unsigned* out = argb + (size_t)y * src_w;
        for (x = 0; x < src_w; x++) {
            unsigned p = row[x];
            unsigned r = (p >>  0) & 0x1F;
            unsigned g = (p >>  5) & 0x1F;
            unsigned b = (p >> 10) & 0x1F;
            /* 5 -> 8 bits, replicating the high bits into the low ones. */
            r = (r << 3) | (r >> 2);
            g = (g << 3) | (g >> 2);
            b = (b << 3) | (b >> 2);
            out[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

int psx_present_init(const char* title, int scale)
{
    const char* err;

    if (active) return 0;
    if (getenv("VS_HEADLESS")) {
        printf("[present] VS_HEADLESS set -- running without a window\n");
        return -1;
    }

    sdl.lib = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!sdl.lib) sdl.lib = dlopen("libSDL2.so", RTLD_NOW | RTLD_LOCAL);
    if (!sdl.lib) {
        printf("[present] SDL2 not available (%s) -- headless\n", dlerror());
        return -1;
    }

#define SYM(field, type, name)                                                 \
    sdl.field = (type)dlsym(sdl.lib, name);                                    \
    if (!sdl.field) { printf("[present] missing %s -- headless\n", name);      \
                      dlclose(sdl.lib); sdl.lib = 0; return -1; }
    SYM(Init, pfn_Init, "SDL_Init")
    SYM(Quit, pfn_Quit, "SDL_Quit")
    SYM(GetError, pfn_GetError, "SDL_GetError")
    SYM(CreateWindow, pfn_CreateWindow, "SDL_CreateWindow")
    SYM(DestroyWindow, pfn_DestroyWindow, "SDL_DestroyWindow")
    SYM(CreateRenderer, pfn_CreateRenderer, "SDL_CreateRenderer")
    SYM(CreateTexture, pfn_CreateTexture, "SDL_CreateTexture")
    SYM(UpdateTexture, pfn_UpdateTexture, "SDL_UpdateTexture")
    SYM(RenderClear, pfn_RenderClear, "SDL_RenderClear")
    SYM(RenderCopy, pfn_RenderCopy, "SDL_RenderCopy")
    SYM(RenderPresent, pfn_RenderPresent, "SDL_RenderPresent")
    SYM(RenderSetLogicalSize, pfn_RenderSetLogicalSize, "SDL_RenderSetLogicalSize")
    SYM(PollEvent, pfn_PollEvent, "SDL_PollEvent")
    SYM(GetKeyboardState, pfn_GetKeyboardState, "SDL_GetKeyboardState")
    SYM(SetHint, pfn_SetHint, "SDL_SetHint")
    SYM(NumJoysticks, pfn_NumJoysticks, "SDL_NumJoysticks")
    SYM(IsGameController, pfn_IsGameController, "SDL_IsGameController")
    SYM(GameControllerOpen, pfn_GameControllerOpen, "SDL_GameControllerOpen")
    SYM(GameControllerGetButton, pfn_GameControllerGetButton, "SDL_GameControllerGetButton")
    SYM(GameControllerGetAxis, pfn_GameControllerGetAxis, "SDL_GameControllerGetAxis")
    SYM(GameControllerName, pfn_GameControllerName, "SDL_GameControllerName")
    SYM(GameControllerUpdate, pfn_GameControllerUpdate, "SDL_GameControllerUpdate")
#undef SYM

    /* Game controller init is allowed to fail (WSL has no /dev/input). */
    if (sdl.Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        if (sdl.Init(SDL_INIT_VIDEO) != 0) {
            err = sdl.GetError();
            printf("[present] SDL_Init failed (%s) -- headless\n", err ? err : "?");
            return -1;
        }
    }

    sdl.SetHint("SDL_RENDER_SCALE_QUALITY", "0");   /* nearest: crisp pixels */

    if (scale < 1) scale = 1;
    win = sdl.CreateWindow(title ? title : "Vagrant Story (native)",
                           SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                           src_w * scale, src_h * scale,
                           SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) {
        printf("[present] CreateWindow failed (%s) -- headless\n", sdl.GetError());
        return -1;
    }
    ren = sdl.CreateRenderer(win, -1, 0);
    if (!ren) {
        printf("[present] CreateRenderer failed (%s) -- headless\n", sdl.GetError());
        return -1;
    }
    sdl.RenderSetLogicalSize(ren, src_w, src_h);
    tex = sdl.CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, PSX_VRAM_W, PSX_VRAM_H);
    if (!tex) {
        printf("[present] CreateTexture failed (%s) -- headless\n", sdl.GetError());
        return -1;
    }

    /* Full-VRAM texture so any display rect the game selects fits without
     * reallocating (see psx_present_set_display). */
    argb_cap = PSX_VRAM_W * PSX_VRAM_H;
    argb = (unsigned*)malloc((size_t)argb_cap * sizeof *argb);
    if (!argb) return -1;

    active = 1;
    bridge_open();
    printf("[present] window open: %dx%d (x%d)\n", src_w, src_h, scale);
    return 0;
}

int psx_present_active(void) { return active; }
int psx_present_quit_requested(void) { return quit_req; }

void psx_present_cooperative_quit(int on) { coop_quit = on; }

void psx_present_set_source(int x, int y, int w, int h)
{
    if (active) return;                 /* size is fixed once the texture exists */
    src_x = x; src_y = y; src_w = w; src_h = h;
}

/* Follow the game's display environment. A real boot moves and resizes the
 * displayed region -- the menu runs 320x240 at the VRAM origin, the title screen
 * a 512x480 area at x=512 -- so the window has to track PutDispEnv rather than
 * assume one layout. The texture is allocated at full VRAM size, so any display
 * rect fits without reallocating. */
void psx_present_set_display(int x, int y, int w, int h, int rgb24)
{
    if (w <= 0 || h <= 0) return;
    if (x == src_x && y == src_y && w == src_w && h == src_h && rgb24 == src_rgb24)
        return;
    if (w > PSX_VRAM_W) w = PSX_VRAM_W;
    if (h > PSX_VRAM_H) h = PSX_VRAM_H;
    /* A 24-bit row spans w*3 bytes of a 2048-byte VRAM line. */
    if (rgb24 && x * 2 + w * 3 > PSX_VRAM_W * 2) w = (PSX_VRAM_W * 2 - x * 2) / 3;
    src_x = x; src_y = y; src_w = w; src_h = h; src_rgb24 = rgb24;
    if (active) {
        int need = src_w * src_h;
        if (need > argb_cap) {
            unsigned* p = (unsigned*)realloc(argb, (size_t)need * sizeof *argb);
            if (!p) return;
            argb = p;
            argb_cap = need;
        }
        if (sdl.RenderSetLogicalSize) sdl.RenderSetLogicalSize(ren, src_w, src_h);
    }
}

void psx_present_pump(void)
{
    unsigned char evbuf[256];
    unsigned btn = 0;
    unsigned char sticks[4];

    if (!active) return;
    frame_no++;

    while (sdl.PollEvent(evbuf)) {
        unsigned type;
        memcpy(&type, evbuf, 4);
        if (type == SDL_QUIT_EVENT) quit_req = 1;
    }

    sticks[0] = sticks[1] = sticks[2] = sticks[3] = 128;

    poll_keyboard(&btn);
    poll_gamepad(&btn, sticks);
    bridge_poll();

    /* Closing the window has to actually close the program. The boot path is
     * the game's own loop, which never returns and never polls anything, so
     * without this the close button and ESC set a flag that nobody reads, and
     * the only way out is to kill the process.
     *
     * Checked after the keyboard poll so ESC takes effect on the same frame it
     * is pressed. exit() rather than a longjmp or a flag, because it runs the
     * atexit handlers, so an in-progress audio capture still gets its WAV
     * header fixed up. */
    if (quit_req && !coop_quit) {
        printf("[present] quit requested, exiting\n");
        fflush(stdout);
        psx_present_shutdown();
        exit(0);
    }

    if (bridge_live) {
        btn |= bridge_buttons;
        memcpy(sticks, bridge_sticks, 4);
        host_connected = 1;
        host_source = "DualSense (Windows bridge)";
    }

    host_buttons = btn;
    memcpy(host_sticks, sticks, 4);

    {
        /* Update and draw only the display rect, from the top-left of the
         * full-VRAM texture. */
        int dst[4] = { 0, 0, src_w, src_h };
        blit_vram();
        sdl.UpdateTexture(tex, dst, argb, src_w * 4);
        sdl.RenderClear(ren);
        sdl.RenderCopy(ren, tex, dst, 0);
        sdl.RenderPresent(ren);
    }
}

void psx_present_shutdown(void)
{
    if (bridge_conn >= 0) { close(bridge_conn); bridge_conn = -1; }
    if (bridge_listen >= 0) { close(bridge_listen); bridge_listen = -1; }
    if (!active) return;
    active = 0;
    if (win) sdl.DestroyWindow(win);
    sdl.Quit();
    free(argb);
    argb = 0;
}

unsigned psx_pad_host_buttons(void) { return host_buttons; }
void psx_pad_host_sticks(unsigned char out[4]) { memcpy(out, host_sticks, 4); }
int  psx_pad_host_connected(void) { return host_connected; }
const char* psx_pad_host_source(void) { return host_source; }
