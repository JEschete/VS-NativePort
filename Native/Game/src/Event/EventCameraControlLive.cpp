// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraControlLive.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraControl.hpp"
#include "Event/EventCameraTrack.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Event/ScriptCommand.hpp"
#include "Field/CameraLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptVm.h"
}

namespace VS::Event {
namespace {
// The saved field view's script flags (vs_battle_getStateFlag ids).
constexpr int kSavedFieldFacingFlag = 0xAF;
constexpr int kSavedFieldDistanceFlag = 0xB0;

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS_LOG_ERROR("event.camera", "{} refused: {}", operation, reason);
  std::abort();
}

CameraState& RequiredCamera(const char* operation) {
  if (Event_camera == nullptr) Refuse(operation, "missing event camera");
  return *Event_camera;
}

class LiveFieldCameraPlacementHost final : public EventFieldCameraHost {
 public:
  FieldCameraPlacement PlaceFieldCamera(std::int32_t facing,
                                        std::int32_t distance_mode) override {
    VECTOR eye{};
    VECTOR look_at{};
    Field::LiveInitializeSphericalCamera(&eye, &look_at, facing, distance_mode);
    return {.eye = Core::Math::VectorOf(eye),
            .look_at = Core::Math::VectorOf(look_at)};
  }
};
}  // namespace

// decomp: BATTLE.PRG func_800BE36C (F01236, src/BATTLE/BATTLE.PRG/4A0A8.c:4018,
// 0x800BE36C-0x800BE3A0)
void SetEventCameraFollowingEntry(int look_at_follow, int eye_follow) {
  CameraState& camera = RequiredCamera("SetEventCameraFollowing");
  EventCameraTracks tracks = LoadLiveEventCameraTracks(camera);
  SetEventCameraFollowing(tracks, static_cast<std::uint8_t>(look_at_follow),
                          static_cast<std::uint8_t>(eye_follow));
  StoreLiveEventCameraTracks(camera, tracks);
}

// decomp: BATTLE.PRG func_800BE3A0 (F01237, src/BATTLE/BATTLE.PRG/4A0A8.c:4026,
// 0x800BE3A0-0x800BE3D0)
void StopEventCameraMotionEntry() {
  CameraState& camera = RequiredCamera("StopEventCameraMotion");
  EventCameraTracks tracks = LoadLiveEventCameraTracks(camera);
  StopEventCameraMotion(tracks);
  StoreLiveEventCameraTracks(camera, tracks);
}

// decomp: BATTLE.PRG func_800BC1CC (F01220, src/BATTLE/BATTLE.PRG/4A0A8.c:3173,
// 0x800BC1CC-0x800BC2E8)
void RestoreEventFieldCameraEntry(std::int16_t field_facing,
                                  int field_distance_mode) {
  auto skip = static_cast<std::uint8_t>(Event_skipNextFieldCameraRestore);
  if (ConsumeFieldCameraOverride(skip)) {
    Event_skipNextFieldCameraRestore = static_cast<char>(skip);
    return;
  }
  CameraState& camera = RequiredCamera("RestoreEventFieldCamera");
  LiveFieldCameraPlacementHost field_camera;
  LiveEventCameraTrackHost motion;
  EventCameraTracks tracks = LoadLiveEventCameraTracks(camera);
  static_cast<void>(RestoreEventFieldCamera(
      tracks,
      {.facing = field_facing,
       .distance_mode = static_cast<std::int16_t>(field_distance_mode)},
      field_camera, motion));
  StoreLiveEventCameraTracks(camera, tracks);
}

// decomp: BATTLE.PRG func_800BE3D0 (F01238, src/BATTLE/BATTLE.PRG/4A0A8.c:4039,
// 0x800BE3D0-0x800BE53C)
void StartEventCameraOrbitReturnEntry(char orbit_side) {
  constexpr const char* kOperation = "StartEventCameraOrbitReturn";
  const SavedFieldCameraView view{
      .facing = State::LiveScriptFlag(kSavedFieldFacingFlag),
      .distance_index = State::LiveScriptFlag(kSavedFieldDistanceFlag)};
  CameraState& camera = RequiredCamera(kOperation);
  LiveFieldCameraPlacementHost field_camera;
  LiveEventCameraTrackHost motion;
  EventCameraTracks tracks = LoadLiveEventCameraTracks(camera);
  if (!StartEventCameraOrbitReturn(tracks, view,
                                   static_cast<std::uint8_t>(orbit_side),
                                   field_camera, motion)) {
    VS_LOG_WARN("event.camera",
                "{}: distance index {} has no height offset; the start is not "
                "raised",
                kOperation, view.distance_index);
  }
  StoreLiveEventCameraTracks(camera, tracks);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BB33C (F01193, src/BATTLE/BATTLE.PRG/4A0A8.c:2761,
// 0x800BB33C-0x800BB35C), opcode 0xDE in the normal and skip tables. It reads
// no operand.
std::intptr_t Event_CommandStopCameraMotionOpcode(
    std::uint8_t* /*instruction*/, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  VS::Event::CameraState& camera =
      event::RequiredCamera("CommandStopCameraMotion");
  event::EventCameraTracks tracks = event::LoadLiveEventCameraTracks(camera);
  event::CommandStopCameraMotion(tracks);
  event::StoreLiveEventCameraTracks(camera, tracks);
  return 0;
}

// decomp: BATTLE.PRG func_800BB35C (F01194, src/BATTLE/BATTLE.PRG/4A0A8.c:2767,
// 0x800BB35C-0x800BB38C), opcode 0xDF in the normal and skip tables.
std::intptr_t Event_CommandSetCameraFollowingOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandSetCameraFollowing";
  if (instruction == nullptr) event::Refuse(kOperation, "missing instruction");
  VS::Event::CameraState& camera = event::RequiredCamera(kOperation);
  event::EventCameraTracks tracks = event::LoadLiveEventCameraTracks(camera);
  static_cast<void>(event::CommandSetCameraFollowing(
      event::ScriptCommand{instruction, event::kSetCameraFollowingLength},
      tracks));
  event::StoreLiveEventCameraTracks(camera, tracks);
  return 0;
}

// decomp: BATTLE.PRG func_800BB3BC (F01196, src/BATTLE/BATTLE.PRG/4A0A8.c:2780,
// 0x800BB3BC-0x800BB41C), opcode 0xE9 in the normal and skip tables. Marks the
// restore as scripted even when the field camera override skipped it.
std::intptr_t Event_CommandRestoreFieldCameraOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  if (instruction == nullptr) {
    event::Refuse("CommandRestoreFieldCamera", "missing instruction");
  }
  const std::optional<event::FieldCameraReturnRequest> request =
      event::CommandRestoreFieldCamera(
          event::ScriptCommand{instruction, event::kRestoreFieldCameraLength});
  if (request.has_value()) {
    event::RestoreEventFieldCameraEntry(request->facing,
                                        request->distance_mode);
  }
  Event_isFieldCameraRestoreScripted = 1;
  return 0;
}
