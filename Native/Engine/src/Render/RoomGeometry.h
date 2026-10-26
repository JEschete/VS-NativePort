// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_ROOMGEOMETRY_H_TYPES
#define VS_RENDER_ROOMGEOMETRY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_RENDER_ROOMGEOMETRY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_ROOMGEOMETRY_H_DECLS)
#define VS_RENDER_ROOMGEOMETRY_H_DECLS
#include "Field/Room.h"

int Render_DrawRoomGeometryPolygons();
int Render_DecodeRoomPolygonVertices();
int Render_ProjectAndDecodeRoomPolygon();
int Render_SubdivideRoomTriangle();
int Render_SubdivideRoomQuad();
int Render_InterpolateRoomVertexMidpoint();
int Render_IsRoomPolygonOnScreen();
int Render_LinkRoomPolygonByDepth();
void Render_TintRoomGeometryPrimitives(void*, void*, Field_RoomTint*, int);
int Render_TintRoomPrimitiveColor();

#endif // VS_RENDER_ROOMGEOMETRY_H_DECLS
