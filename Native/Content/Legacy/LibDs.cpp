#include "Legacy/LibDs.hpp"

#include "Core/Unported.hpp"

// Remaining compatibility entry points (EC-04), not the extracted-file reader.

namespace VS::Legacy {

int DsInit() {
  VS::Core::RecordUnported("libds:DsInit");
  return {};
}

int DsGetSector(void* madr, int size) {
  (void)madr;
  (void)size;
  VS::Core::RecordUnported("libds:DsGetSector");
  return {};
}

}  // namespace VS::Legacy
