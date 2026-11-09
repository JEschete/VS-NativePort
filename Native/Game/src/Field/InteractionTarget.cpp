#include "Field/InteractionTarget.hpp"

#include <limits>

namespace VS::Field {
namespace {
constexpr std::int16_t kFirstUnsuppressedTriggerKind = 16;
constexpr std::int32_t kTrapTileWorldUnits = 128;
constexpr std::int32_t kDisabledTrapActivation = 1;
}  // namespace

// decomp: BATTLE.PRG func_8008D438 (146C.c:16810), 52 MIPS words.
std::optional<TrapTarget> FindRoomTriggerAtTile(
    std::span<const RoomTriggerInfo> records, const RoomTriggerTile& tile,
    bool suppress_panels) {
  for (const auto& record : records) {
    if (record.tile_x != tile.x || record.tile_z != tile.z ||
        record.floor_layer != tile.layer) {
      continue;
    }
    if (suppress_panels && record.kind < kFirstUnsuppressedTriggerKind) {
      return std::nullopt;
    }
    return record.target;
  }
  return std::nullopt;
}

// decomp: BATTLE.PRG func_8008DEAC (146C.c:17147), 26 MIPS words.
bool SetRoomTriggerEnabled(std::optional<TrapTarget> target,
                           std::int32_t activation,
                           RoomTriggerActivationHost& host) {
  const auto domain = host.Domain();
  const auto byte = static_cast<std::uint8_t>(activation);
  if (target) {
    return ResolveTrapTarget(*target, domain) &&
           host.StoreActivation(*target, byte);
  }
  if (domain.trap_count > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  for (std::size_t index = 0; index < domain.trap_count; ++index) {
    if (!host.StoreActivation(
            {static_cast<std::uint32_t>(index), domain.generation}, byte)) {
      return false;
    }
  }
  return true;
}

// decomp: BATTLE.PRG func_8006CDD8 (146C.c:2151), 30 MIPS words.
bool DisableTrapAtPosition(TrapWorldPosition position, TrapDisableHost& host) {
  const auto target = host.Find(
      {position.x / kTrapTileWorldUnits, position.z / kTrapTileWorldUnits, 0});
  if (!target) return true;
  if (!SetRoomTriggerEnabled(target, kDisabledTrapActivation, host)) {
    return false;
  }
  const auto tile = host.Tile(*target);
  return tile && host.MarkObstacle(*tile);
}
}  // namespace VS::Field
