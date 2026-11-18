#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the event camera workspace (Event_camera, Event/Camera.hpp), the
// tick speed and the actor, bone, easing and floor services to the typed
// track motion (EventCameraTrack.hpp). Each entry keeps the original
// signature so the opcode table, the module bindings and the callers can use
// it in place of the copy. A missing workspace or track is refused where the
// original wrote or read through a null pointer.
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Event/EventCameraTrack.hpp"

struct MATRIX;
struct VECTOR;

namespace VS::Event {
struct CameraState;
struct CameraTrack;

// The workspace's tracks, origin and last terrain position, and back. Storing
// writes the tracks and the last terrain position, which the motion may
// change, and keeps the VECTOR pads.
[[nodiscard]] EventCameraTracks LoadLiveEventCameraTracks(
    const CameraState& camera);
void StoreLiveEventCameraTracks(CameraState& camera,
                                const EventCameraTracks& tracks);

// The motion services over the live floor, easing curves, script actor
// references, actor placements and bones.
class LiveEventCameraTrackHost final : public EventCameraTrackHost {
 public:
  [[nodiscard]] std::int16_t FloorHeight(std::int32_t world_x,
                                         std::int32_t world_z) override;
  [[nodiscard]] std::int32_t Ease(std::uint8_t curve, std::uint8_t frame,
                                  std::uint8_t duration) override;
  [[nodiscard]] int ResolveActorReference(std::uint16_t reference) override;
  [[nodiscard]] std::optional<std::int16_t> ActorFacing(int actor) override;
  [[nodiscard]] std::optional<Core::Math::Position3> TaggedBonePosition(
      int actor, int tag) override;
};

// decomp: BATTLE.PRG func_800BC2E8 (F01221, 4A0A8.c:3200), opcodes 0xC0-0xDF
// Operation 6 has no case: it warns and returns -1 where the original
// returned its epilogue address.
std::intptr_t CommandCameraMotionEntry(std::uint8_t* instruction,
                                       std::int16_t thread_index);
// decomp: BATTLE.PRG func_800BCA8C (F01223, 4A0A8.c:3389)
// `other` is the original's unread second argument.
void UpdateEventCameraTrackEntry(CameraTrack* track, CameraTrack* other);
// decomp: BATTLE.PRG func_800BCFB4 (F01224, 4A0A8.c:3502)
void AdvanceTerrainCameraTrackEntry(CameraTrack* track);
// decomp: BATTLE.PRG func_800BD2B8 (F01225, 4A0A8.c:3556)
// Reads the probe frame from the record; the GTE registers are untouched.
int EventCameraGroundClearanceEntry(MATRIX* probe);
// decomp: BATTLE.PRG func_800BE53C (F01239, 4A0A8.c:4074)
void ScaleEventCameraTrackDurationEntry(CameraTrack* track);
// decomp: BATTLE.PRG func_800BE5A4 (F01240, 4A0A8.c:4082)
void ScaleEventCameraDurationEntry(std::int16_t selector);
// decomp: BATTLE.PRG _setVectorImm (F01246, 4A0A8.c:4148)
VECTOR* ReadScriptFixedVectorEntry(VECTOR* destination, std::uint8_t* operands);
}  // namespace VS::Event
