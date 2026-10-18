#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_ROOMDRAW_H
#define VS_OWNED_RENDER_ROOMDRAW_H
#include "Legacy/Psx.h"
#include "Render/Render.h"

// decomp: _mpdGeometryHeader / 146C.h:31; transform access in 2EA3C.s:1265.
typedef struct Render_RoomGeometryGroup {
	unsigned flags;
	short bounds[3][2];
	int angles[3];
	unsigned polygonOffset;
	MATRIX view;
} Render_RoomGeometryGroup;

#ifdef __cplusplus
static_assert(sizeof(Render_RoomGeometryGroup) == 64);
extern "C" {
#else
_Static_assert(sizeof(Render_RoomGeometryGroup) == 64, "MPD group layout");
#endif

int Render_ValidateRoomGeometry(const void* data, size_t size);
void Render_UpdateRoomGroupTransforms(void* data);
void Render_BuildRoomGeometryViewMatrix(VECTOR* position, VECTOR* angles, MATRIX* output);
void Render_BuildScaleCompensatedRoomView(VECTOR* position, VECTOR* angles, MATRIX* output);
void Render_DrawRoomGeometry(void* data, int mode);
void Render_SetRoomPolygonNearClip(int depth);
void Render_SetRoomTextureLodDepth(int depth);
void Render_DrawSkyDome(void* data);

#ifdef __cplusplus
}
#endif
#include "Field/Room.h"


#endif // VS_OWNED_RENDER_ROOMDRAW_H
