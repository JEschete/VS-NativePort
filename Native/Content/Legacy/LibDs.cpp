#include "Legacy/LibDs.h"

#include "Core/Unported.h"

// Remaining compatibility entry points (EC-04), not the extracted-file reader.

int DsInit(void) {
  VS_UNPORTED("libds:DsInit");
  return {};
}

int DsGetSector(void* madr, int size) {
  (void)madr;
  (void)size;
  VS_UNPORTED("libds:DsGetSector");
  return {};
}
