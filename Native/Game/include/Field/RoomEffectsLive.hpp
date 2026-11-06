#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/RoomEffects.hpp"

// Live binding for RoomEffects.hpp: the generated room loading, rendering,
// gimmick, door, event script and cutscene callers keep the original call
// shapes (2842C.c:550-1456). Each adapter runs the typed room effects over
// the one RoomEffectState below and binds RoomEffectHost to the loaded
// room's sections (Field_roomData and its header lengths), its sound bank,
// MoveImage, the low-end game heap, the collision floor query and the skip
// state's fast-forward flag.
namespace VS::Field {
// D_800F1DD8, D_800F1D00 and D_800F2258..D_800F225B.
[[nodiscard]] RoomEffectState& LiveRoomEffectState();

// func_8009195C: the group record in the loaded room's geometry, or null.
[[nodiscard]] int* LiveFindRoomGeometryGroup(int group_index);
int LiveShowRoomGeometryGroup(int group_index);
int LiveHideRoomGeometryGroup(int group_index);
int LiveSwapRoomGeometryGroupFlags(int first_group_index,
                                   int second_group_index);
int LiveAddRoomGeometryDepthBias(int group_index, int depth_bias);
int LiveDisableRoomGeometryGroup(int group_index);
void LiveSwapRoomGeometryRedGreen(int group_index);

void LiveUpdateRoomEffects();
void LiveInitializeRoomEffects();
int LiveStartRoomEffect(int effect_index);
int LiveReleaseRoomEffects();
int LiveToggleRoomEffectUpdates(int effect_index);
[[nodiscard]] int LiveIsRoomEffectActive(int effect_index);
void LiveFinishRoomDoorRotation(int effect_index);
void LiveFinishRoomTiltRotation(int effect_index);

void LiveDuckRoomEffectSound(int duck_enabled);
void LiveRestoreRoomEffectSoundVolume();
void LiveRestartRoomEffectSound();

// 2EA3C.s:1770 (func_80098B1C): no generated code calls it. The section E
// code runs only through a binding recognized by its bytes; none is bound,
// so every room's entry point is refused.
int LiveCallRoomEntryPoint();
}  // namespace VS::Field
