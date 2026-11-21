#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/RoomRendering.hpp"

namespace VS::Render {
struct ColorCode;
}  // namespace VS::Render

// Binding from the session's room rendering globals
// (RoomRenderingLiveState.hpp), the loaded room (VS::Field::roomData), the
// frame scratch and the frame's primitive buffer to the typed room rendering
// rules (RoomRendering.hpp). Each adapter keeps the original call shape for the
// generated callers (copy_fixups.py OWNED_CALL_BINDINGS).
namespace VS::Field {
// The switches and tint as the globals hold them now.
[[nodiscard]] RoomRenderState LiveRoomRenderState();

// decomp: BATTLE.PRG func_8008A4DC (src/BATTLE/BATTLE.PRG/146C.c:15391)
void LiveSetFieldRenderingEnabled(int enabled);
// decomp: BATTLE.PRG func_8008AC78 (src/BATTLE/BATTLE.PRG/146C.c:15791)
void LiveDrawRoomScene();
// decomp: BATTLE.PRG func_8008B1FC (src/BATTLE/BATTLE.PRG/146C.c:15796)
// Draws `geometry` over the loaded room's geometry extent.
void LiveDrawRoomGeometry(void* geometry);
// decomp: BATTLE.PRG func_8008B28C (src/BATTLE/BATTLE.PRG/146C.c:15809)
void LiveAdvanceRoomAnimationClock();
// decomp: BATTLE.PRG func_8008B2C0 (src/BATTLE/BATTLE.PRG/146C.c:15816)
void LiveSetRoomRenderingEnabled(int enabled);
// decomp: BATTLE.PRG func_8008B2E0 (src/BATTLE/BATTLE.PRG/146C.c:15823)
void LiveCopyZonePaletteRow(const void* colors, int row, int first, int count);
// decomp: BATTLE.PRG func_8008B320 (src/BATTLE/BATTLE.PRG/146C.c:15828)
void LiveUploadRoomPalettes();
// decomp: BATTLE.PRG func_8008B390 (src/BATTLE/BATTLE.PRG/146C.c:15836)
// Only the loaded room's geometry is rewritten; any other is refused.
void LiveOffsetRoomTextureIds(int offset, int* geometry);
// decomp: BATTLE.PRG func_8008B430 (src/BATTLE/BATTLE.PRG/146C.c:15865)
void LiveSetRoomTint(const VS::Render::ColorCode* color, int strength);
// decomp: BATTLE.PRG func_8008B4B0 (src/BATTLE/BATTLE.PRG/146C.c:15878)
void LiveClearRoomTint();
// decomp: BATTLE.PRG func_8008B4BC (src/BATTLE/BATTLE.PRG/146C.c:15880)
void LiveSetRoomAnimationsPaused(int paused);
// decomp: BATTLE.PRG func_8008C2C0 (src/BATTLE/BATTLE.PRG/146C.c:16283)
int LiveApplyCameraGeometryOcclusion(int tile_x, int tile_z, int floor_layer,
                                     int distance_mode);
// decomp: BATTLE.PRG func_8008C40C (src/BATTLE/BATTLE.PRG/146C.c:16320)
void LiveClearGeometryOcclusion();
}  // namespace VS::Field
