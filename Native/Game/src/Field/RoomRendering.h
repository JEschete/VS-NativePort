// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ROOMRENDERING_H_TYPES
#define VS_FIELD_ROOMRENDERING_H_TYPES
#include "Legacy/Psx.h"


// decomp: D_800F1BB6 (D_800F1BB6@BATTLE/BATTLE.PRG): a view into another global
#define Field_isRoomTintActive (*(short*)(&Field_roomTint.isActive))

#endif // VS_FIELD_ROOMRENDERING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_ROOMRENDERING_H_DECLS)
#define VS_FIELD_ROOMRENDERING_H_DECLS
#include "Field/Room.h"
#include "Render/Palette.h"

extern int Field_isFieldRenderingEnabled;
extern u_short Field_areRoomUpdatesPaused;
extern u_short* Field_roomPalettes;
extern Field_RoomTint Field_roomTint;
void Field_SetFieldRenderingEnabled(int arg0);
int Field_EnableOwnedRoomRendering(int arg0);
int Field_DisableOwnedRoomRendering(int arg0);
void Field_DrawRoomScene(void);
void Field_DrawRoomGeometry(void* arg0);
void Field_AdvanceRoomAnimationClock(void);
void Field_SetRoomRenderingEnabled(int arg0);
void Field_CopyZonePaletteRow(void* arg0, int arg1, int arg2, int arg3);
void Field_UploadRoomPalettes(void);
void Field_OffsetRoomTextureIds(int arg0, int* arg1);
void Field_SetRoomTint(Render_ColorCode* arg0, int arg1);
void Field_ClearRoomTint(void);
void Field_SetRoomAnimationsPaused(int arg0);
int Field_ApplyCameraGeometryOcclusion(int arg0, int arg1, int arg2, int arg3);
void Field_ClearGeometryOcclusion(void);
void Field_SampleRoomGroundColor(int arg0, int layer, Render_ColorCode* color);

#endif // VS_FIELD_ROOMRENDERING_H_DECLS
