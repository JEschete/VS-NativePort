// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Title/SaveCard.h"

// decomp: _dirEntBuf (_dirEntBuf@src/TITLE/TITLE.PRG/22C.c)
struct DIRENTRY* Title_cardDirectoryEntryBuffer;

// decomp: _loadSaveDataErrorOffset (_loadSaveDataErrorOffset@src/TITLE/TITLE.PRG/22C.c)
char Title_saveTransferErrorOffset;

// decomp: _memcardEventDescriptors (_memcardEventDescriptors@src/TITLE/TITLE.PRG/22C.c)
long Title_cardEventDescriptors[8];

// decomp: _memcardFiles (_memcardFiles@src/TITLE/TITLE.PRG/22C.c)
struct DIRENTRY* Title_cardDirectoryEntries[15];

}  // extern "C"
