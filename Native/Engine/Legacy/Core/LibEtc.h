#pragma once

// libetc/libapi timing and kernel surface (plan §2.2). VSync and the root counters are timing (strategy R,
// backed by Core::FrameClock in LibEtc.cpp); kernel events and IRQ setup have no native meaning (strategy D) and
// are in Legacy/Removed.h.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MODE_NTSC 0

// kernel event descriptors (values only; the event calls themselves are removed)
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

#ifdef __cplusplus
}
#endif
