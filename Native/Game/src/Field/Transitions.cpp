// Hand-maintained (tools/port/owned.csv).
#include "Field/Transitions.hpp"

#include <cstdint>
#include <optional>

namespace VS::Field {
namespace {
// TeleportToLocation's facing quadrant: the facing is rounded by an eighth
// turn, wrapped to one turn and its quarter kept in the entry elevation bits
// (146C.c:8853, `ONE` and `>> 10`).
constexpr int kFacingRounding = 0x200;
constexpr int kFullTurn = 0x1000;
constexpr int kNegativeWrapBias = 0x3FF;
constexpr int kQuadrantShift = 10;

std::uint32_t StepValue(TransitionStep step) {
  return static_cast<std::uint32_t>(step);
}

void StartExit(Render::ScreenTransition& screen, TransitionStep step,
               int effect, int speed) {
  screen.step = StepValue(step);
  screen.effect = effect;
  screen.speed = speed;
}

void LeaveRoom(TransitionHost& host) {
  host.RestoreAshleyIdlePose();
  host.BypassAshleyCollision();
  host.ClearEntranceDoor();
}

void TakeDoorUnderAshley(TransitionHost& host) {
  const Actors::ActorPlacementView placement =
      host.AshleyPlacement().value_or(Actors::ActorPlacementView{});
  host.FindEntranceDoorNearTile(placement.tile.x, placement.tile.z,
                                placement.tile.layer);
}

std::uint8_t FacingQuadrant(std::int16_t facing) {
  const int rounded = facing + kFacingRounding;
  int wrapped = rounded % kFullTurn;
  if (wrapped < 0) {
    wrapped += kNegativeWrapBias;
  }
  return static_cast<std::uint8_t>((wrapped >> kQuadrantShift) &
                                   kDoorEntryElevationMask);
}
}  // namespace

TransitionPhase ScreenTransitionPhase(TransitionStep step) {
  switch (step) {
    case TransitionStep::kRoomExit:
    case TransitionStep::kZoneExit:
      return TransitionPhase::kExiting;
    case TransitionStep::kRoomEntryLoading:
      return TransitionPhase::kEntryLoading;
    case TransitionStep::kRoomEntryReveal:
      return TransitionPhase::kEntryReveal;
    case TransitionStep::kFadeOut:
      return TransitionPhase::kFadingOut;
    case TransitionStep::kFadedOut:
      return TransitionPhase::kFadedOut;
    case TransitionStep::kFadeIn:
      return TransitionPhase::kFadingIn;
    case TransitionStep::kIdle:
    default:
      return TransitionPhase::kIdle;
  }
}

void ConfigureScreenTransition(Render::ScreenTransition& screen,
                               const ScreenTransitionStyle& style) {
  screen.effect = style.effect;
  screen.speed = style.speed;
  screen.wipe_angle = style.wipe_angle;
}

void SetTransitionWipeAngle(Render::ScreenTransition& screen, int wipe_angle) {
  screen.wipe_angle = wipe_angle;
}

void BeginScreenFadeOut(Render::ScreenTransition& screen,
                        const ScreenTransitionStyle& style) {
  if (screen.step == StepValue(TransitionStep::kRoomExit) ||
      screen.step == StepValue(TransitionStep::kZoneExit)) {
    return;
  }
  screen.step = StepValue(TransitionStep::kFadeOut);
  ConfigureScreenTransition(screen, style);
}

void BeginScreenFadeIn(Render::ScreenTransition& screen,
                       const ScreenTransitionStyle& style) {
  screen.step = StepValue(TransitionStep::kFadeIn);
  ConfigureScreenTransition(screen, style);
}

void BeginRoomTransition(RoomTransitionState& state, TransitionHost& host,
                         const DoorRecord& door, int effect, int speed) {
  state.pending_door = door;
  StartExit(state.screen, TransitionStep::kRoomExit, effect, speed);
  // The door leads to a room of the zone Ashley is in, whatever zone it names.
  state.pending_door.destination_zone_id =
      static_cast<std::uint8_t>(host.CurrentZoneId());
  host.QueueRoomLoad(door.destination_room_id);
  LeaveRoom(host);
}

void BeginScriptedRoomTransition(RoomTransitionState& state,
                                 TransitionHost& host, const DoorRecord& door,
                                 int effect, int speed) {
  BeginRoomTransition(state, host, door, effect, speed);
  TakeDoorUnderAshley(host);
}

void BeginZoneTransition(RoomTransitionState& state, TransitionHost& host,
                         const DoorRecord& door, int effect, int speed) {
  state.pending_door = door;
  StartExit(state.screen, TransitionStep::kZoneExit, effect, speed);
  host.QueueZoneLoad(door.destination_zone_id);
  LeaveRoom(host);
}

void BeginScriptedZoneTransition(RoomTransitionState& state,
                                 TransitionHost& host, const DoorRecord& door,
                                 int effect, int speed) {
  BeginZoneTransition(state, host, door, effect, speed);
  TakeDoorUnderAshley(host);
}

bool TeleportToLocation(RoomTransitionState& state, TransitionHost& host,
                        const TeleportRequest& request) {
  const std::optional<LocationDestination> location =
      host.Location(request.location_id);
  if (!location.has_value()) {
    host.ReportRefusedLocation(request.location_id);
    return false;
  }
  DoorRecord& door = state.pending_door;
  door.entry_tile_x = static_cast<std::uint8_t>(request.tile_x & kDoorTileMask);
  door.entry_tile_z = static_cast<std::uint8_t>(request.tile_z & kDoorTileMask);
  door.entry_direction = 0;
  const Actors::ActorPlacementView placement =
      host.AshleyPlacement().value_or(Actors::ActorPlacementView{});
  StartExit(state.screen, TransitionStep::kRoomExit, request.effect,
            request.speed);
  // The original keeps Ashley's facing quadrant in the entry elevation bits.
  door.entry_elevation = FacingQuadrant(placement.position.w);
  door.reserved_entry_bit = false;
  if (location->zone_id != door.destination_zone_id) {
    state.screen.step = StepValue(TransitionStep::kZoneExit);
    host.QueueZoneLoad(location->zone_id);
  } else if (location->room_id != door.destination_room_id) {
    state.screen.step = StepValue(TransitionStep::kRoomExit);
    host.QueueRoomLoad(location->room_id);
  }
  door.destination_zone_id = location->zone_id;
  door.destination_room_id = location->room_id;
  host.FreezeActorAnimations();
  host.BypassAshleyCollision();
  host.ClearEntranceDoor();
  return true;
}

void InitializeRoomTransitionPrimitives(TransitionPrimitiveTarget& target,
                                        const TransitionPrimitiveStyle& style) {
  target.InitializeFade(style);
  target.InitializeWipe(style);
  target.InitializeEdge(style);
  target.InitializeDrawMode(style);
  target.SetTransitionAlpha(style.initial_alpha);
}
}  // namespace VS::Field
