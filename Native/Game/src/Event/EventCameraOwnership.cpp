// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraOwnership.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {
namespace {
constexpr std::size_t kOriginActorOperand = 1;
constexpr std::uint32_t kLowByteMask = 0xFF;
}  // namespace

// decomp: BATTLE.PRG func_800BBBE8 (F01214, src/BATTLE/BATTLE.PRG/4A0A8.c:3014,
// 0x800BBBE8-0x800BBDDC)
EventCameraStart StartEventCamera(Core::Math::Vec3 field_eye,
                                  Core::Math::Vec3 field_look_at) noexcept {
  EventCameraStart start;
  start.look_at.position = field_look_at;
  start.look_at.partner = field_eye;
  start.eye.position = field_eye;
  start.eye.partner = field_look_at;
  start.screen_scale.value0 = kEventScreenUnitScale;
  start.screen_scale.value1 = kEventScreenUnitScale;
  start.screen_tint.value2 = 0;
  start.screen_translation.value0 = kEventScreenUnitScale;
  start.screen_translation.value1 = kEventScreenUnitScale;
  start.screen_warp.value0 = kEventScreenUnitScale;
  start.projection_distance.value = kEventCameraProjectionDistance;
  start.near_clip.value = kEventCameraNearClip;
  start.far_clip.value = kEventCameraFarClip;
  return start;
}

// decomp: BATTLE.PRG func_800BBE04 (F01216, src/BATTLE/BATTLE.PRG/4A0A8.c:3084,
// 0x800BBE04-0x800BBE10)
std::uint8_t EventFieldCameraOverride(int request) noexcept {
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(request) &
                                   kLowByteMask);
}

// decomp: BATTLE.PRG func_800BBE94 (F01218, src/BATTLE/BATTLE.PRG/4A0A8.c:3101,
// 0x800BBE94-0x800BBF14)
void UpdateEventCameraOrigin(EventCameraOrigin& origin,
                             EventCameraOriginHost& host) {
  if (origin.actor == kEventCameraWorldOrigin) {
    origin.offset = {};
    return;
  }
  const std::optional<Core::Math::Position3> position =
      host.ActorPosition(origin.actor);
  origin.offset = position.has_value()
                      ? Core::Math::ShortVectorToFixed(*position)
                      : Core::Math::Vec3{};
}

// decomp: BATTLE.PRG func_800BBE10 (F01217, src/BATTLE/BATTLE.PRG/4A0A8.c:3086,
// 0x800BBE10-0x800BBE94)
void SetEventCameraOriginActor(EventCameraOrigin& origin, std::uint16_t actor,
                               EventCameraOriginHost& host) {
  if (actor == kEventCameraWorldOrigin && origin.actor != actor) {
    origin.eye_position =
        Core::Math::AddVectors(origin.eye_position, origin.offset);
    origin.look_at_position =
        Core::Math::AddVectors(origin.look_at_position, origin.offset);
  }
  origin.actor = actor;
  UpdateEventCameraOrigin(origin, host);
}

// decomp: BATTLE.PRG func_800BB38C (F01195, src/BATTLE/BATTLE.PRG/4A0A8.c:2774,
// 0x800BB38C-0x800BB3BC)
bool CommandSetCameraOriginActor(ScriptCommand command,
                                 EventCameraOrigin& origin,
                                 EventCameraOriginHost& host) {
  if (command.size() < kSetCameraOriginActorLength) return false;
  const auto reference =
      static_cast<std::uint16_t>(OperandHalf(command, kOriginActorOperand));
  const int actor = host.ResolveActorReference(reference);
  SetEventCameraOriginActor(origin, static_cast<std::uint16_t>(actor), host);
  return true;
}

}  // namespace VS::Event
