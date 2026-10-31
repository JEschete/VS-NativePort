// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"

// decomp: _cdReadBuffer (_cdReadBuffer@SLUS_010.40)
Core_HeapBlockHeader* Content_legacyDiscReadBuffer;

// decomp: cdReadLoc (cdReadLoc@SLUS_010.40)
DslLOC Content_legacyDiscReadLocation;

// decomp: D_800501CC (D_800501CC@SLUS_010.40)
int Content_legacyXaReadyBcdField;

// decomp: D_800501D0 (D_800501D0@SLUS_010.40)
int Content_legacyXaReadyByteThree;

// decomp: D_800501D4 (D_800501D4@SLUS_010.40)
int Content_legacyXaReadyByteFour;

// decomp: D_800501D8 (D_800501D8@SLUS_010.40)
int Content_xaReadyMetadataWord;

// decomp: _cdQueue (_cdQueue@SLUS_010.40)
Content_DiscQueueSlot Content_discRequestSlots[32];

// decomp: _cdQueueCount (_cdQueueCount@SLUS_010.40)
struct Content_DiscRequestCounts Content_discRequestCounts;

// decomp: vs_main_disk (vs_main_disk@SLUS_010.40)
Content_DiscDrive Content_discDriveState;

// decomp: _dsControlBuf (_dsControlBuf@SLUS_010.40)
char Content_discControlResponseBytes[11];

}  // extern "C"
