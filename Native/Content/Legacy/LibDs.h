#pragma once

// Remaining EC-04 layouts and compatibility hooks. DataStore serves runtime
// sector requests from extracted files; LibDs.cpp does not read data.

#include "Legacy/Core/PsxTypes.h"

struct CdlATV {
  u_char val0, val1, val2, val3;
};

struct DslLOC {
  u_char minute, second, sector, track;
};

struct DslFILTER {
  u_char file, chan;
  u_short pad;
};

int DsInit();
int DsGetSector(void* madr, int size);
