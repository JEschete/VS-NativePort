#pragma once
// TITLE.PRG's memory-card driver state: the card events and the directory
// the last scan found.
#include <cstddef>
#include <cstdint>

struct DIRENTRY;

namespace VS::Title {
inline constexpr std::size_t kCardEventDescriptors = 8;
inline constexpr std::size_t kCardDirectoryCapacity = 15;

// decomp: _dirEntBuf (_dirEntBuf@src/TITLE/TITLE.PRG/22C.c)
extern DIRENTRY* cardDirectoryEntryBuffer;
// decomp: _loadSaveDataErrorOffset
// (_loadSaveDataErrorOffset@src/TITLE/TITLE.PRG/22C.c)
extern std::uint8_t saveTransferErrorOffset;
// decomp: _memcardEventDescriptors
// (_memcardEventDescriptors@src/TITLE/TITLE.PRG/22C.c)
extern std::int32_t cardEventDescriptors[kCardEventDescriptors];
// decomp: _memcardFiles (_memcardFiles@src/TITLE/TITLE.PRG/22C.c)
extern DIRENTRY* cardDirectoryEntries[kCardDirectoryCapacity];
}  // namespace VS::Title
