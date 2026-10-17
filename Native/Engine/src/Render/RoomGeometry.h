// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_ROOMGEOMETRY_H_TYPES
#define VS_RENDER_ROOMGEOMETRY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_RENDER_ROOMGEOMETRY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_ROOMGEOMETRY_H_DECLS)
#define VS_RENDER_ROOMGEOMETRY_H_DECLS
#include "Field/Room.h"
#include "Render/Render.h"

void Render_DrawRoomGeometry(void*, int);
int Render_DrawRoomGeometryPolygons();
int Render_DecodeRoomPolygonVertices();
int Render_ProjectAndDecodeRoomPolygon();
int Render_SubdivideRoomTriangle();
int Render_SubdivideRoomQuad();
int Render_InterpolateRoomVertexMidpoint();
int Render_IsRoomPolygonOnScreen();
int Render_LinkRoomPolygonByDepth();
void Render_SetRoomPolygonNearClip(int);
int Render_SetRoomPolygonFarClip();
void Render_SetRoomTextureLodDepth(int);
int Render_SetRoomPolygonScreenBounds();
void Render_DrawSkyDome(void*);
void Render_UpdateRoomGroupTransforms(void*);
void Render_BuildRoomGeometryViewMatrix(VECTOR*, VECTOR*, MATRIX*);
void Render_BuildScaleCompensatedRoomView(VECTOR*, VECTOR*, MATRIX*);
void Render_SampleRoomLighting(int, int);
int Render_InterpolateRoomLightTriangle();
void Render_TintRoomGeometryPrimitives(void*, void*, Field_RoomTint*, int);
int Render_TintRoomPrimitiveColor();
void Render_GetProjectionScreenOffset(Render_ScreenOffset*);
void Render_SetProjectionScreenOffset(Render_ScreenOffset*);

#endif // VS_RENDER_ROOMGEOMETRY_H_DECLS
