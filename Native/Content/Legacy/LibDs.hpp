#pragma once

// Remaining EC-04 layouts and compatibility hooks, with the PS1 SDK spellings
// that SDK_API_LEDGER.md and the EC register track. DataStore serves runtime
// sector requests from extracted files; LibDs.cpp does not read data.

#include <cstdint>

namespace VS::Legacy {

// decomp: CdlATV (include/psx/libcd.h:183-188). The CD-to-SPU attenuator
// the sound control's disc mix keeps (bound at 0x800365D0).
struct CdlATV {
  std::uint8_t val0;  // CD (L) -> SPU (L)
  std::uint8_t val1;  // CD (L) -> SPU (R)
  std::uint8_t val2;  // CD (R) -> SPU (L)
  std::uint8_t val3;  // CD (R) -> SPU (R)
};
static_assert(sizeof(CdlATV) == 0x4, "CdlATV must keep its PS1 layout");

// decomp: DslLOC (include/psx/libds.h:103-109). A BCD disc position.
struct DslLOC {
  std::uint8_t minute;
  std::uint8_t second;
  std::uint8_t sector;
  std::uint8_t track;
};
static_assert(sizeof(DslLOC) == 0x4, "DslLOC must keep its PS1 layout");

// decomp: DslFILTER (include/psx/libds.h:114-118). The XA ADPCM filter.
struct DslFILTER {
  std::uint8_t file;
  std::uint8_t chan;
  std::uint16_t pad;
};
static_assert(sizeof(DslFILTER) == 0x4, "DslFILTER must keep its PS1 layout");

// decomp: DsInit (include/psx/libds.h:231), called at boot (main.c:1153).
int DsInit();
// decomp: DsGetSector (include/psx/libds.h:247), called by the sector
// callback (main.c:1925-1927).
int DsGetSector(void* madr, int size);

}  // namespace VS::Legacy
