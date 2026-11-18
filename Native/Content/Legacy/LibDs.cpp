#include "Legacy/LibDs.h"

#include "Core/Unported.hpp"

// Remaining compatibility entry points (EC-04), not the extracted-file reader.

int DsInit(void) {
  VS::Core::RecordUnported("libds:DsInit");
  return {};
}

int DsGetSector(void* madr, int size) {
  (void)madr;
  (void)size;
  VS::Core::RecordUnported("libds:DsGetSector");
  return {};
}
