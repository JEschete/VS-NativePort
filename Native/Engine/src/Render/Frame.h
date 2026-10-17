// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_FRAME_H_TYPES
#define VS_RENDER_FRAME_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_RENDER_FRAME_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_FRAME_H_DECLS)
#define VS_RENDER_FRAME_H_DECLS

extern void* Render_legacyCurrentOrderingTable;
extern DR_STP Render_legacyMaskBitOffPackets[8];
extern DR_STP Render_legacyMaskBitOnPackets[6];
extern void* Render_legacyOrderingTables[2];
extern void* Render_legacyPrimitiveBuffers[2];

#endif // VS_RENDER_FRAME_H_DECLS
