#include "Field/RoomSwitches.hpp"

#include <limits>

namespace VS::Field {
namespace {

constexpr std::int32_t kOpeningSoundFirst = 0x3B;
constexpr std::int32_t kOpeningSoundSecond = 0x3C;
constexpr std::int32_t kClosingSoundFirst = 0x3D;
constexpr std::int32_t kClosingSoundSecond = 0x3E;
constexpr std::uint8_t kOpenPassability = 5;
constexpr std::uint8_t kSwitchStartFrames = 20;
constexpr std::int16_t kNegativeZCompanionKind = 4;
constexpr std::int16_t kPositiveXCompanionKind = 5;
constexpr std::int16_t kPositiveZCompanionKind = 6;
constexpr std::int16_t kNegativeXCompanionKind = 7;
constexpr std::int32_t kDoorOpeningSoundFirst = 0x1D;
constexpr std::int32_t kDoorOpeningSoundSecond = 0x1E;
constexpr std::int32_t kDoorClosingSoundFirst = 0x16;
constexpr std::int32_t kDoorClosingSoundSecond = 0x17;
constexpr std::uint16_t kFirstDoorSigil = 0x1CA;
constexpr std::uint32_t kDoorSigilCount = 7;
constexpr std::uint8_t kDoorStartDelay = 8;

std::optional<std::size_t> SigilDoorForRoom(const SigilDoorTables& tables,
                                            const SigilDoorRoomQuery& query) {
  if (tables.index_ranges.size() < 2 ||
      query.sigil_index >= tables.index_ranges.size() - 1) {
    return std::nullopt;
  }
  const auto first = tables.index_ranges[query.sigil_index];
  const auto last = tables.index_ranges[query.sigil_index + 1];
  if (first < 0 || last < first ||
      static_cast<std::size_t>(last) > tables.room_pairs.size()) {
    return std::nullopt;
  }
  for (auto index = static_cast<std::size_t>(first);
       index < static_cast<std::size_t>(last); ++index) {
    const auto& pair = tables.room_pairs[index];
    if ((pair.first.zone_id == query.zone_id &&
         pair.first.map_id == query.map_id) ||
        (pair.second.zone_id == query.zone_id &&
         pair.second.map_id == query.map_id)) {
      return index;
    }
  }
  return std::nullopt;
}

std::optional<RoomDoorTileQuery> CompanionDoorTile(
    const RoomDoorPlacement& door) {
  auto tile_x = static_cast<std::int64_t>(door.tile_x);
  auto tile_z = static_cast<std::int64_t>(door.tile_z);
  switch (door.kind) {
    case kNegativeZCompanionKind:
      --tile_z;
      break;
    case kPositiveXCompanionKind:
      ++tile_x;
      break;
    case kPositiveZCompanionKind:
      ++tile_z;
      break;
    case kNegativeXCompanionKind:
      --tile_x;
      break;
    default:
      return std::nullopt;
  }
  if (tile_x < std::numeric_limits<std::int32_t>::min() ||
      tile_x > std::numeric_limits<std::int32_t>::max() ||
      tile_z < std::numeric_limits<std::int32_t>::min() ||
      tile_z > std::numeric_limits<std::int32_t>::max()) {
    return std::nullopt;
  }
  return RoomDoorTileQuery{static_cast<std::int32_t>(tile_x),
                           static_cast<std::int32_t>(tile_z), door.floor_layer};
}

bool DoorCoversAdjacentTile(const RoomDoorPlacement& door,
                            const RoomDoorTileQuery& query) {
  const auto difference_x =
      static_cast<std::int64_t>(query.tile_x) - door.tile_x;
  const auto difference_z =
      static_cast<std::int64_t>(query.tile_z) - door.tile_z;
  switch (door.kind) {
    case kNegativeZCompanionKind:
      return difference_x == 0 && difference_z == -1;
    case kPositiveXCompanionKind:
      return difference_x == 1 && difference_z == 0;
    case kPositiveZCompanionKind:
      return difference_x == 0 && difference_z == 1;
    case kNegativeXCompanionKind:
      return difference_x == -1 && difference_z == 0;
    default:
      return false;
  }
}

}  // namespace

// decomp: BATTLE.PRG func_8008BC04 (146C.c:16215), first-record activation
// guard.
std::optional<std::size_t> InteractiveDoorAtTile(
    std::span<const RoomDoorPlacement> doors, const RoomDoorTileQuery& query) {
  if (doors.empty() || doors.front().activation == 0) return std::nullopt;
  for (std::size_t index = 0; index < doors.size(); ++index) {
    const auto& door = doors[index];
    if (door.floor_layer != query.floor_layer) continue;
    if ((door.tile_x == query.tile_x && door.tile_z == query.tile_z) ||
        DoorCoversAdjacentTile(door, query)) {
      return index;
    }
  }
  return std::nullopt;
}

// decomp: BATTLE.PRG func_8008BD74 (146C.c:16262), original packed XOR5.
bool ToggleDoorCollision(RoomDoorCollisionHost& host) {
  const auto primary = host.State();
  if (!primary) return false;
  host.ToggleCell({primary->tile_x, primary->tile_z, primary->floor_layer},
                  kOpenPassability);
  const auto current = host.State();
  if (!current) return false;
  if (const auto companion = CompanionDoorTile(*current)) {
    host.ToggleCell(*companion, kOpenPassability);
  }
  return true;
}

// decomp: BATTLE.PRG func_8008BEBC (146C.c:16290), original busy/sigil/delay
// order.
bool BeginDoorToggle(RoomDoorAnimationHost& host) {
  const auto initial = host.State(DoorMember::kPrimary);
  if (!initial || initial->busy != 0 || !host.QueueAvailable()) return false;
  if (!host.StoreBusy(DoorMember::kPrimary, 1)) return false;
  const auto sigil =
      static_cast<std::uint32_t>(initial->key_or_sigil) - kFirstDoorSigil;
  if (sigil < kDoorSigilCount) host.MarkSigil(sigil);
  const auto state = host.State(DoorMember::kPrimary);
  if (!state) return false;
  bool accepted = false;
  if (state->open_value == 0) {
    accepted = host.StoreDelay(kDoorStartDelay) &&
               host.Queue(GimmickAnimationKind::kStartDoor);
  } else {
    accepted = StartDoorAnimation(host);
  }
  if (!accepted) {
    static_cast<void>(host.StoreBusy(DoorMember::kPrimary, initial->busy));
    static_cast<void>(host.StoreDelay(initial->delay));
  }
  return accepted;
}

// decomp: BATTLE.PRG func_8008BF48 (146C.c:16309),
// sounds/effects/close/linked/queue.
bool StartDoorAnimation(RoomDoorAnimationHost& host) {
  auto state = host.State(DoorMember::kPrimary);
  if (!state || !host.QueueAvailable()) return false;
  const bool opening = state->open_value == 0;
  host.PlaySound(opening ? kDoorOpeningSoundFirst : kDoorClosingSoundFirst);
  host.PlaySound(opening ? kDoorOpeningSoundSecond : kDoorClosingSoundSecond);
  state = host.State(DoorMember::kPrimary);
  if (!state) return false;
  host.StartEffect(state->effect);
  state = host.State(DoorMember::kPrimary);
  if (!state) return false;
  if (state->open_value != 0 && !host.ToggleCollision(DoorMember::kPrimary)) {
    return false;
  }
  auto linked = host.State(DoorMember::kLinked);
  if (linked && linked->placement.activation != 0) {
    host.StartEffect(linked->effect);
    linked = host.State(DoorMember::kLinked);
    if (!linked) return false;
    if (linked->open_value != 0 && !host.ToggleCollision(DoorMember::kLinked)) {
      return false;
    }
    if (!host.StoreBusy(DoorMember::kLinked, 1)) return false;
  }
  return host.Queue(GimmickAnimationKind::kFinishDoor);
}

// decomp: BATTLE.PRG func_8008C1C8 (146C.c:16389), wait then open/clear both
// records.
bool FinishDoorAnimation(RoomDoorAnimationHost& host) {
  auto state = host.State(DoorMember::kPrimary);
  if (!state || host.EffectActive(state->effect)) return false;
  state = host.State(DoorMember::kPrimary);
  if (!state) return false;
  if (state->open_value == 0 && !host.ToggleCollision(DoorMember::kPrimary)) {
    return false;
  }
  state = host.State(DoorMember::kPrimary);
  if (!state ||
      !host.StoreOpen(DoorMember::kPrimary,
                      static_cast<std::int16_t>(state->open_value == 0), 0)) {
    return false;
  }
  auto linked = host.State(DoorMember::kLinked);
  if (linked && linked->placement.activation != 0) {
    if (linked->open_value == 0 && !host.ToggleCollision(DoorMember::kLinked)) {
      return false;
    }
    linked = host.State(DoorMember::kLinked);
    if (!linked || !host.StoreOpen(
                       DoorMember::kLinked,
                       static_cast<std::int16_t>(linked->open_value == 0), 0)) {
      return false;
    }
  }
  return true;
}

// decomp: BATTLE.PRG func_8008E6DC (146C.c:17399), original first-match SB1.
bool MarkSigilDoorUnlocked(const SigilDoorTables& tables,
                           const SigilDoorRoomQuery& query,
                           std::span<std::byte> flags) {
  const auto index = SigilDoorForRoom(tables, query);
  if (!index || *index >= flags.size()) return false;
  flags[*index] = std::byte{1};
  return true;
}

// decomp: BATTLE.PRG func_8008E7BC (146C.c:17419), original LBU unlock byte.
std::uint8_t RoomSigilDoorUnlockByte(const SigilDoorTables& tables,
                                     const SigilDoorRoomQuery& query,
                                     std::span<const std::byte> flags) {
  const auto index = SigilDoorForRoom(tables, query);
  return index && *index < flags.size()
             ? std::to_integer<std::uint8_t>(flags[*index])
             : 0;
}

// decomp: BATTLE.PRG func_8008D658 (src/BATTLE/BATTLE.PRG/146C.c:16883).
bool ToggleRoomSwitch(RoomSwitchHost& host) {
  auto state = host.State();
  if (!state) {
    return false;
  }
  const bool opening = state->switch_value == 0;
  host.PlaySound(opening ? kOpeningSoundFirst : kClosingSoundFirst);
  host.PlaySound(opening ? kOpeningSoundSecond : kClosingSoundSecond);
  state = host.State();
  if (!state) {
    return false;
  }
  host.StartEffect(state->effect_kind);
  state = host.State();
  if (!state) {
    return false;
  }
  state->marker_fade = 0;
  state->switch_value = static_cast<std::uint16_t>(state->switch_value == 0);
  host.StoreSwitch(state->switch_value, state->marker_fade);
  host.SetCellPassability(*state,
                          state->switch_value != 0 ? kOpenPassability : 0);
  return true;
}

// decomp: BATTLE.PRG func_8008D5FC (src/BATTLE/BATTLE.PRG/146C.c:16870).
// #320: refused queue admission must not leave marker1/activation20 latched.
bool BeginSwitchToggle(RoomSwitchStartHost& host) {
  const auto state = host.State();
  if (!state || state->marker_fade != 0) {
    return false;
  }
  const auto activation =
      state->switch_value == 0 ? kSwitchStartFrames : state->activation;
  if (!host.StoreStartState(1, activation)) {
    return false;
  }
  const bool accepted =
      state->switch_value == 0 ? host.QueueToggle() : host.ToggleImmediately();
  if (!accepted) {
    static_cast<void>(
        host.StoreStartState(state->marker_fade, state->activation));
  }
  return accepted;
}

}  // namespace VS::Field
