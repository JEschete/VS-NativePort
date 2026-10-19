#pragma once

// libpad surface (plan §6.5). The copied code registers two receive buffers
// with PadInitDirect and reads them every frame; LibPad.cpp fills them from
// Engine\Input (SDL3 gamepad and keyboard) in the original pad format, so the
// game's own pad decoding runs unchanged until it is rewritten against
// Input::PadState.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PADLup (1 << 12)
#define PADLdown (1 << 14)
#define PADLleft (1 << 15)
#define PADLright (1 << 13)
#define PADRup (1 << 4)
#define PADRdown (1 << 6)
#define PADRleft (1 << 7)
#define PADRright (1 << 5)
#define PADi (1 << 9)
#define PADj (1 << 10)
#define PADk (1 << 8)
#define PADl (1 << 3)
#define PADm (1 << 1)
#define PADn (1 << 2)
#define PADo (1 << 0)
#define PADh (1 << 11)
#define PADL1 PADn
#define PADL2 PADo
#define PADR1 PADl
#define PADR2 PADm
#define PADstart PADh
#define PADselect PADk

#define PadStateFindPad 1
#define PadStateFindCTP1 2
#define PadStateStable 6
#define InfoModeCurID 1
#define InfoModeCurExID 2
#define InfoModeCurExOffs 3

void PadInitDirect(unsigned char* port1, unsigned char* port2);
void PadStartCom(void);
int PadGetState(int port);
int PadInfoMode(int port, int term, int offs);
int PadSetActAlign(int port, unsigned char* table);
int PadSetMainMode(int port, int offs, int lock);
void PadSetAct(int port, unsigned char* table, int len);

#ifdef __cplusplus
}
#endif
