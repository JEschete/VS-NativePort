// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Runtime.h"
#include "PFile.h"

// decomp: BATTLE.PRG _getPFileBlock3section (src/BATTLE/BATTLE.PRG/5BF94.c:7874)
void* Content_PFileTimelineSection(Effects_PackageBlock3* timeline_block, int section_index)
{
    return (char*)timeline_block + timeline_block->offsets[section_index];
}

// decomp: BATTLE.PRG func_800D5564 (src/BATTLE/BATTLE.PRG/5BF94.c:7879)
void* Content_PFileTimelineTrackData(Effects_PackageBlock3* timeline_block, int section_index, int track_index)
{
    Content_PFileTimelineTable* entry = (Content_PFileTimelineTable *)(Content_PFileTimelineSection(timeline_block, section_index));
    return (char*)timeline_block + entry->tracks[track_index].dataOffset;
}

}  // extern "C"
