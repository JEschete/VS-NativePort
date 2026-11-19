// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Title/SaveCard.hpp"

// decomp: _dirEntBuf (_dirEntBuf@src/TITLE/TITLE.PRG/22C.c)
DIRENTRY* VS::Title::cardDirectoryEntryBuffer;

// decomp: _loadSaveDataErrorOffset (_loadSaveDataErrorOffset@src/TITLE/TITLE.PRG/22C.c)
std::uint8_t VS::Title::saveTransferErrorOffset;

// decomp: _memcardEventDescriptors (_memcardEventDescriptors@src/TITLE/TITLE.PRG/22C.c)
std::int32_t VS::Title::cardEventDescriptors[VS::Title::kCardEventDescriptors];

// decomp: _memcardFiles (_memcardFiles@src/TITLE/TITLE.PRG/22C.c)
DIRENTRY* VS::Title::cardDirectoryEntries[VS::Title::kCardDirectoryCapacity];

