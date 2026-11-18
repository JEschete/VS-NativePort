#pragma once

// Remaining EC-04 layouts and compatibility hooks. DataStore serves runtime
// sector requests from extracted files; LibDs.cpp does not read data.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CdlATV {
  u_char val0, val1, val2, val3;
} CdlATV;

typedef struct DslLOC {
  u_char minute, second, sector, track;
} DslLOC;

typedef struct DslFILTER {
  u_char file, chan;
  u_short pad;
} DslFILTER;

int DsInit(void);
int DsGetSector(void* madr, int size);

#ifdef __cplusplus
}
#endif
