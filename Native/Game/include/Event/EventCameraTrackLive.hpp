#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (Event_camera, Event/Camera.h), the
// tick speed and the actor, bone, easing and floor services to the typed
// track motion (EventCameraTrack.hpp). Each entry keeps the original
// signature so the opcode table, the module bindings and the callers can use
// it in place of the copy. A missing workspace or track is refused where the
// original wrote or read through a null pointer.
#include <cstdint>

struct Event_CameraTrack;
struct MATRIX;
struct VECTOR;

namespace VS::Event {
// decomp: BATTLE.PRG func_800BC2E8 (F01221, 4A0A8.c:3200), opcodes 0xC0-0xDF
// Operation 6 has no case: it warns and returns -1 where the original
// returned its epilogue address.
std::intptr_t CommandCameraMotionEntry(std::uint8_t* instruction,
                                       std::int16_t thread_index);
// decomp: BATTLE.PRG func_800BCA8C (F01223, 4A0A8.c:3389)
// `other` is the original's unread second argument.
void UpdateEventCameraTrackEntry(Event_CameraTrack* track,
                                 Event_CameraTrack* other);
// decomp: BATTLE.PRG func_800BCFB4 (F01224, 4A0A8.c:3502)
void AdvanceTerrainCameraTrackEntry(Event_CameraTrack* track);
// decomp: BATTLE.PRG func_800BD2B8 (F01225, 4A0A8.c:3556)
// Reads the probe frame from the record; the GTE registers are untouched.
int EventCameraGroundClearanceEntry(MATRIX* probe);
// decomp: BATTLE.PRG func_800BE53C (F01239, 4A0A8.c:4074)
void ScaleEventCameraTrackDurationEntry(Event_CameraTrack* track);
// decomp: BATTLE.PRG func_800BE5A4 (F01240, 4A0A8.c:4082)
void ScaleEventCameraDurationEntry(std::int16_t selector);
// decomp: BATTLE.PRG _setVectorImm (F01246, 4A0A8.c:4148)
VECTOR* ReadScriptFixedVectorEntry(VECTOR* destination, std::uint8_t* operands);
}  // namespace VS::Event
