// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Runtime.h"
#include "PFile.h"

// decomp: BATTLE.PRG _getPFileBlock3section (src/BATTLE/BATTLE.PRG/5BF94.c:7874)
void* Content_PFileTimelineSection(Effects_PackageBlock3* arg0, int arg1)
{
    return (char*)arg0 + arg0->offsets[arg1];
}

// decomp: BATTLE.PRG func_800D5564 (src/BATTLE/BATTLE.PRG/5BF94.c:7879)
void* Content_PFileTimelineTrackData(Effects_PackageBlock3* arg0, int arg1, int arg2)
{
    Content_PFileTimelineTable* entry = Content_PFileTimelineSection(arg0, arg1);
    return (char*)arg0 + entry->tracks[arg2].dataOffset;
}

