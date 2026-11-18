// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Content_PFileTimelineTable;
struct Content_PFileTimelineTrackEntry;

#include "Effects/Runtime.h"

// decomp: func_800D55A4_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:46)
struct Content_PFileTimelineTrackEntry {
    Effects_TimelineTrackHeader header;
    u_short dataOffset;
};
static_assert(sizeof(Content_PFileTimelineTrackEntry) == 0x4, "Content_PFileTimelineTrackEntry must keep its PS1 layout");

// decomp: func_800D55A4_t (src/BATTLE/BATTLE.PRG/5BF94.c:51)
struct Content_PFileTimelineTable {
    int trackCount;
    Content_PFileTimelineTrackEntry tracks[0];
};
static_assert(sizeof(Content_PFileTimelineTable) == 0x4, "Content_PFileTimelineTable must keep its PS1 layout");
