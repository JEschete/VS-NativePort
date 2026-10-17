#pragma once

// libcd/libds surface (plan §6.6, strategy T). The game addresses files by disc sector; LibDs.cpp serves those
// reads from the converted content in Native\data (Content maps a sector to its file), never from a disc image.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CdlLOC {
    u_char minute, second, sector, track;
} CdlLOC;

typedef struct CdlATV {
    u_char val0, val1, val2, val3;
} CdlATV;

typedef struct StHEADER {
    u_short id, type, secCount, nSectors;
    u_long frameCount, frameSize;
    u_short width, height;
    u_long dummy1, dummy2;
    CdlLOC loc;
} StHEADER;

typedef struct DslLOC {
    u_char minute, second, sector, track;
} DslLOC;

typedef struct DslATV {
    u_char val0, val1, val2, val3;
} DslATV;

typedef struct DslFILTER {
    u_char file, chan;
    u_short pad;
} DslFILTER;

typedef void (*DslCB)(u_char, u_char*);
typedef void (*DslRCB)(u_char, u_char*, u_long*);

#define DslModeStream2 0x120
#define DslModeSpeed 0x80
#define DslModeRT 0x40
#define DslModeSize1 0x20
#define DslModeSF 0x08
#define DslModeDA 0x01
#define DslSetloc 0x02
#define DslReadN 0x06
#define DslPause 0x09
#define DslSetfilter 0x0D
#define DslSetmode 0x0E
#define DslSeekL 0x15
#define DslReadS 0x1B
#define DslNoIntr 0x00
#define DslDataReady 0x01
#define DslComplete 0x02
#define DslDiskError 0x05
#define DslNoResult 0x06
#define DslReady 1

void StSetRing(u_long* ring, u_long size);
void StUnSetRing(void);
void StSetStream(u_long mode, u_long startFrame, u_long endFrame, void (*func1)(void), void (*func2)(void));
u_long StFreeRing(u_long* base);
u_long StGetNext(u_long** addr, u_long** header);
int StGetBackloc(CdlLOC* loc);
int DsInit(void);
int DsCommand(u_char com, u_char* param, DslCB cbsync, int count);
int DsPacket(u_char mode, DslLOC* pos, u_char com, DslCB func, int count);
int DsGetSector(void* madr, int size);
DslLOC* DsIntToPos(int i, DslLOC* p);
int DsControl(u_char com, u_char* param, u_char* result);
int DsControlB(u_char com, u_char* param, u_char* result);
int DsRead2(DslLOC* pos, int mode);

#ifdef __cplusplus
}
#endif
