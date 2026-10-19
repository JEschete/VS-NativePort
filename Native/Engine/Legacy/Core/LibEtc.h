#pragma once

// libetc/libapi timing and kernel surface (plan §2.2). VSync and the root
// counters are timing (strategy R, backed by logical frames in LibEtc.cpp).
// Counter 2 dispatches its registered sound callback; other removed kernel and
// IRQ operations remain in Legacy/Removed.h.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MODE_NTSC 0

// kernel event descriptors
#define DescHW 0xF0000000
#define DescSW 0xF4000000
#define DescRC 0xF2000000
#define HwCARD (DescHW | 0x11)
#define SwCARD (DescSW | 0x01)
#define RCntCNT2 (DescRC | 0x02)
#define RCntMdINTR 0x1000
#define EvSpINT 0x0002
#define EvSpIOE 0x0004
#define EvSpTIMOUT 0x0100
#define EvSpNEW 0x2000
#define EvSpERROR 0x8000
#define EvMdINTR 0x1000
#define EvMdNOINTR 0x2000

int VSync(int mode);
int VSyncCallback(void (*f)(void));
long SetVideoMode(long mode);
long SetRCnt(unsigned long spec, unsigned short target, long mode);
long GetRCnt(unsigned long spec);
long StartRCnt(unsigned long spec);
long StopRCnt(unsigned long spec);
long OpenEvent(unsigned long desc, long spec, long mode,
               long (*callback)(void));
long CloseEvent(long event);
long EnableEvent(long event);
long DisableEvent(long event);
void Legacy_AdvanceCounter2(unsigned elapsedClocks);

#ifdef __cplusplus
}
#endif
