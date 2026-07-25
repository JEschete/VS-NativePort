/* Host window + input for the native port (Phase 2.1).
 *
 * Presents the PS1 VRAM display area in a real window and collects host input,
 * normalised into the PS1 controller's own bit layout so the game's untouched
 * pad code (vs_main_processPadState) consumes it exactly as it does on hardware.
 *
 * SDL2 is loaded with dlopen at RUNTIME, deliberately:
 *   - the native binary is -m32, and Ubuntu's SDL headers are a multiarch shim
 *     that needs libsdl2-dev:i386; pulling i386 dev packages risks displacing
 *     the mipsel cross-toolchain the matching build depends on;
 *   - it keeps the window OPTIONAL -- with no SDL (or no display) the binary
 *     still runs headless and dumps PPMs, which is what batch/regression use.
 */
#ifndef NATIVE_PSX_PRESENT_H
#define NATIVE_PSX_PRESENT_H

/* Open the window. scale = integer magnification of the 320x240 display area.
 * Returns 0 on success, <0 if unavailable (caller should continue headless). */
int  psx_present_init(const char* title, int scale);
int  psx_present_active(void);

/* Called once per frame from VSync(): present VRAM and refresh input. */
void psx_present_pump(void);

/* Window close / ESC. By default a quit request ends the process from inside
 * psx_present_pump(), because the game's own loop never returns and never polls
 * anything. Callers that run their own loop should call
 * psx_present_cooperative_quit(1) and poll psx_present_quit_requested()
 * instead, so they can shut down cleanly. */
int  psx_present_quit_requested(void);
void psx_present_cooperative_quit(int on);
void psx_present_shutdown(void);

/* Returns 1 exactly once per F2 press, for dumping a PPM to diff against a
 * DuckStation capture of the same screen. */
int  psx_present_screenshot_requested(void);

/* Which VRAM rect to display (defaults to 0,0,320,240). */
void psx_present_set_source(int x, int y, int w, int h);

/* --- host pad state, already in PS1 bit layout (PADLup etc., active high) --- */
unsigned psx_pad_host_buttons(void);
/* Physical sticks, 0..255 with 128 centred: [0]=left X, [1]=left Y,
 * [2]=right X, [3]=right Y. */
void     psx_pad_host_sticks(unsigned char out[4]);
/* Non-zero once any real pad (SDL gamepad or the Windows bridge) has reported. */
int      psx_pad_host_connected(void);
/* Human-readable description of the active input source, for logging. */
const char* psx_pad_host_source(void);

#endif
