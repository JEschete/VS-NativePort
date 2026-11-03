// Hand-maintained (tools/port/owned.csv).
#include "Save/Snapshot.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Save {
namespace {

constexpr unsigned kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;
// State_AshleyStatsSnapshot.isPending, a little-endian int.
constexpr std::size_t kPendingFlagBytes = 4;

// A bit field of a Field_MpdDoorRecord halfword.
struct DoorField {
  std::size_t offset = 0;
  unsigned shift = 0;
  unsigned width = 0;
};

constexpr DoorField kEntryDirection = {.offset = 2, .shift = 13, .width = 3};
constexpr DoorField kEntryTileX = {.offset = 4, .shift = 8, .width = 5};
constexpr DoorField kEntryElevation = {.offset = 4, .shift = 13, .width = 2};
constexpr DoorField kRequiredFlagId = {.offset = 6, .shift = 0, .width = 10};
constexpr DoorField kEntryTileZ = {.offset = 6, .shift = 10, .width = 5};
// Actors_TileTarget.facing keeps the elevation in its top two bits.
constexpr unsigned kFacingElevationShift = 6;

void SetDoorField(EntryDoor& door, DoorField field, unsigned value) {
  const unsigned low = std::to_integer<unsigned>(door.at(field.offset));
  const unsigned high = std::to_integer<unsigned>(door.at(field.offset + 1));
  const unsigned mask = ((1U << field.width) - 1U) << field.shift;
  const unsigned half = ((low | (high << kBitsPerByte)) & ~mask) |
                        ((value << field.shift) & mask);
  door.at(field.offset) = static_cast<std::byte>(half & kByteMask);
  door.at(field.offset + 1) =
      static_cast<std::byte>((half >> kBitsPerByte) & kByteMask);
}

}  // namespace

// decomp: SLUS_010.40 func_80042CB0 (src/SLUS_010.40/main.c:8375), the
// door fields
EntryDoor PlaceEntryDoor(const EntryDoor& pending, TilePlacement placement,
                         std::uint16_t flag_id) {
  EntryDoor door = pending;
  SetDoorField(door, kEntryTileX, placement.tile_x);
  SetDoorField(door, kEntryTileZ, placement.tile_z);
  SetDoorField(door, kEntryDirection, placement.tile_layer);
  SetDoorField(
      door, kEntryElevation,
      static_cast<unsigned>(placement.facing) >> kFacingElevationShift);
  SetDoorField(door, kRequiredFlagId, flag_id);
  return door;
}

// decomp: SLUS_010.40 func_80042CB0 (src/SLUS_010.40/main.c:8375), the
// learned-action loop. The original shifts a char held in a register; the
// copied C read four bytes through `*(int*)&actionsLearned`.
LearnedActions PackLearnedActions(SnapshotHost& host) {
  LearnedActions learned{};
  for (std::size_t index = 0; index < kLearnedActionBytes; ++index) {
    unsigned bits = 0;
    for (std::size_t bit = 0; bit < kActionsPerLearnedByte; ++bit) {
      const bool unlocked =
          host.ActionUnlocked(index * kActionsPerLearnedByte + bit);
      bits = (bits << 1U) | (unlocked ? 1U : 0U);
    }
    learned.at(index) = static_cast<std::uint8_t>(bits & kByteMask);
  }
  return learned;
}

// decomp: SLUS_010.40 func_80042CA0 (src/SLUS_010.40/main.c:8373)
void MarkAshleySnapshotValid(std::span<std::byte, kAshleySnapshotBytes> bytes) {
  const auto is_pending = bytes.first<kPendingFlagBytes>();
  std::ranges::fill(is_pending, std::byte{0});
  is_pending[0] = std::byte{1};
}

// decomp: SLUS_010.40 func_80042CB0 (src/SLUS_010.40/main.c:8375)
void CaptureAshleySnapshot(SnapshotHost& host) {
  host.StoreEntryDoor(PlaceEntryDoor(host.PendingDestinationDoor(),
                                     host.AshleyPlacement(),
                                     host.SnapshotDoorFlagId()));
  host.StoreLearnedActions(PackLearnedActions(host));
  if (const auto stats = host.AshleyStats(); stats.has_value()) {
    host.StoreSnapshot({.stats = *stats, .camera = host.Camera()});
  }
  for (std::int32_t actor = 0; actor < kSnapshotRoomActorCount; ++actor) {
    host.StoreRoomActorState(actor);
  }
  host.StoreEntranceDoorForReload();
}
}  // namespace VS::Save
