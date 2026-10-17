// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_MINIMAP_H_TYPES
#define VS_FIELD_MINIMAP_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_MINIMAP_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_MINIMAP_H_DECLS)
#define VS_FIELD_MINIMAP_H_DECLS
#include "Field/Map.h"

extern int Field_minimapBorderPulsePhase;
extern short Field_minimapSlideFrame;
extern char Field_isMinimapVisible;
void Field_DrawMinimapScene(void*);
Field_MapRoomName* Field_ResolveSceneRoomData(void*);

#endif // VS_FIELD_MINIMAP_H_DECLS
