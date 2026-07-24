/* libpad: PSYQ controller API on the host.
 *
 * The game reads its pad through vs_main_processPadState() ->
 * vs_main_updatePadState(), which decodes a RAW PS1 CONTROLLER PACKET out of
 * the buffer registered with PadInitDirect(). So rather than poking
 * vs_main_buttonsPressed / vs_main_buttonRepeat (what the bring-up harness used
 * to do), this layer writes a real packet and lets the game's own untouched
 * code derive held/pressed/released/auto-repeat and the analog->d-pad mapping.
 *
 * Packet layout (SCPH-1200 analog pad, id 0x73), per vs_main_updatePadState:
 *   [0]   0x00 connected, non-zero = nothing plugged in
 *   [1]   pad id (high nibble is what PadInfoMode(InfoModeCurID) reports)
 *   [2:3] button bits, ACTIVE LOW  -- the game applies ~((b[2]<<8) | b[3])
 *   [4:5] right stick X,Y          -- VS calls these lStick*
 *   [6:7] left  stick X,Y          -- VS calls these rStick*, and case 7 of
 *                                     processPadState turns them into the
 *                                     PADLup/down/left/right menu directions,
 *                                     which is why the LEFT stick navigates.
 */
#include <psx_present.h>
#include <string.h>

#define PadStateFindPad  1
#define PadStateStable   6
#define InfoModeCurID    1

/* Buffers handed to us by the game (vs_main_padBuffer[0..1], 34 bytes each). */
static unsigned char* padbuf[2];

void PadInitDirect(unsigned char* b0, unsigned char* b1)
{
    padbuf[0] = b0;
    padbuf[1] = b1;
    if (b0) { memset(b0, 0, 34); b0[0] = 0xFF; }
    if (b1) { memset(b1, 0, 34); b1[0] = 0xFF; }
}

void PadInitMtap(unsigned char* b0, unsigned char* b1) { PadInitDirect(b0, b1); }
void PadStartCom(void) { }
void PadStopCom(void) { }
int  PadChkVsync(void) { return 1; }
unsigned PadEnableCom(unsigned on) { return on; }

/* Port 0 always holds an analog pad; port 1 is empty. */
int PadGetState(int portID)
{
    return ((portID >> 4) == 0) ? PadStateStable : PadStateFindPad;
}

int PadInfoMode(int portID, int term, int offs)
{
    (void)offs;
    if ((portID >> 4) != 0) return 0;
    /* 7 = analog (DualShock). Anything else and updatePadState centres the
     * sticks and the left stick stops steering the menu. */
    if (term == InfoModeCurID) return 7;
    return 0;                       /* no extended/actuator id */
}

int PadInfoAct(int portID, int term, int offs)  { (void)portID; (void)term; (void)offs; return 0; }
int PadInfoComb(int portID, int term, int offs) { (void)portID; (void)term; (void)offs; return 0; }
int PadSetMainMode(int socket, int offs, int lock) { (void)socket; (void)offs; (void)lock; return 1; }

/* Rumble. Accepted and ignored -- see the deviations ledger (B section). */
void PadSetAct(int portID, unsigned char* data, int len) { (void)portID; (void)data; (void)len; }
int  PadSetActAlign(int portID, unsigned char* data) { (void)portID; (void)data; return 1; }

/* Publish the current host input as a PS1 packet. Called once per frame from
 * VSync(), which is where the hardware's pad DMA would have landed. */
void psx_pad_update(void)
{
    unsigned btn;
    unsigned char s[4];

    if (!padbuf[0]) return;

    btn = psx_pad_host_buttons();
    psx_pad_host_sticks(s);

    padbuf[0][0] = 0x00;                              /* connected */
    padbuf[0][1] = 0x73;                              /* analog pad */
    padbuf[0][2] = (unsigned char)(~(btn >> 8));      /* active low */
    padbuf[0][3] = (unsigned char)(~btn);
    padbuf[0][4] = s[2];                              /* right stick X */
    padbuf[0][5] = s[3];                              /* right stick Y */
    padbuf[0][6] = s[0];                              /* left  stick X */
    padbuf[0][7] = s[1];                              /* left  stick Y */

    if (padbuf[1]) padbuf[1][0] = 0xFF;               /* port 2 empty */
}
