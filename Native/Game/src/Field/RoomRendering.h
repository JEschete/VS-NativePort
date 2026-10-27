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
void Field_SetFieldRenderingEnabled(int enabled);
int Field_EnableOwnedRoomRendering(int owner_room_index);
int Field_DisableOwnedRoomRendering(int owner_room_index);
void Field_DrawRoomScene(void);
void Field_DrawRoomGeometry(void* room_geometry);
void Field_AdvanceRoomAnimationClock(void);
void Field_SetRoomRenderingEnabled(int enabled);
void Field_CopyZonePaletteRow(void* source_colors, int palette_row, int first_color, int color_count);
void Field_UploadRoomPalettes(void);
void Field_OffsetRoomTextureIds(int texture_id_offset, int* geometry_words);
void Field_SetRoomTint(Render_ColorCode* tint_color, int tint_strength);
void Field_ClearRoomTint(void);
void Field_SetRoomAnimationsPaused(int paused);
int Field_ApplyCameraGeometryOcclusion(int query_tile_x, int query_tile_z, int query_floor_layer, int camera_distance_mode);
void Field_ClearGeometryOcclusion(void);
void Field_SampleRoomGroundColor(int packed_world_position, int layer, Render_ColorCode* color);

#endif // VS_FIELD_ROOMRENDERING_H_DECLS
