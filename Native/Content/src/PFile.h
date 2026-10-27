// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_PFILE_H_TYPES
#define VS_PFILE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_PFileTimelineTable Content_PFileTimelineTable;
typedef struct Content_PFileTimelineTrackEntry Content_PFileTimelineTrackEntry;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_PFILE_H_SET_TYPES_ONLY
#endif
#include "Effects/Runtime.h"
#ifdef VS_PFILE_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_PFILE_H_SET_TYPES_ONLY
#endif

// decomp: func_800D55A4_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:46)
typedef struct Content_PFileTimelineTrackEntry {
    Effects_TimelineTrackHeader header;
    u_short dataOffset;
} Content_PFileTimelineTrackEntry;
_Static_assert(sizeof(Content_PFileTimelineTrackEntry) == 0x4, "Content_PFileTimelineTrackEntry must keep its PS1 layout");

// decomp: func_800D55A4_t (src/BATTLE/BATTLE.PRG/5BF94.c:51)
typedef struct Content_PFileTimelineTable {
    int trackCount;
    Content_PFileTimelineTrackEntry tracks[0];
} Content_PFileTimelineTable;
_Static_assert(sizeof(Content_PFileTimelineTable) == 0x4, "Content_PFileTimelineTable must keep its PS1 layout");

#endif // VS_PFILE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_PFILE_H_DECLS)
#define VS_PFILE_H_DECLS
#include "Effects/Package.h"

void* Content_PFileTimelineSection(Effects_PackageBlock3* timeline_block, int section_index);
void* Content_PFileTimelineTrackData(Effects_PackageBlock3* timeline_block, int section_index, int track_index);

#endif // VS_PFILE_H_DECLS
