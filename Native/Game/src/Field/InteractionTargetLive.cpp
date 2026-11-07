#include "Field/InteractionTargetLive.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Items/TreasureChests.h"
}

namespace VS::Field {
namespace {

InteractionTargetState target_state;
std::uint64_t room_generation = 1;
constexpr std::size_t kOriginalFloorRecordBytes = 12;
constexpr std::size_t kOriginalChestRecordBytes = 0x234;

std::span<Field_MpdFloorTrigger> TrapRecords() {
  if (Field_roomData.floorTriggers == nullptr) {
    return {};
  }
  return {
      Field_roomData.floorTriggers,
      Field_roomData.header.floorTriggersLength / kOriginalFloorRecordBytes};
}

std::span<Items_TreasureChest> ChestRecords() {
  if (Field_roomData.containers == nullptr) {
    return {};
  }
  return {Field_roomData.containers,
          Field_roomData.header.containersLength / kOriginalChestRecordBytes};
}

template <class Record>
std::optional<std::uint32_t> RecordIndex(std::span<Record> records,
                                         const Record* record) {
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (&records[index] == record) {
      return static_cast<std::uint32_t>(index);
    }
  }
  return std::nullopt;
}

}  // namespace

std::optional<TrapTarget> FindLiveTrapTarget(
    const Field_MpdFloorTrigger* record) {
  const auto index = RecordIndex(TrapRecords(), record);
  return index ? std::optional{TrapTarget{*index, room_generation}}
               : std::nullopt;
}

std::optional<TrapTarget> FindLiveTrapTargetAddress(const void* address) {
  const auto records = TrapRecords();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (static_cast<const void*>(&records[index]) == address) {
      return TrapTarget{static_cast<std::uint32_t>(index), room_generation};
    }
  }
  return std::nullopt;
}

std::optional<ChestTarget> FindLiveChestTarget(
    const Items_TreasureChest* record) {
  const auto index = RecordIndex(ChestRecords(), record);
  return index ? std::optional{ChestTarget{*index, room_generation}}
               : std::nullopt;
}

std::optional<ChestTarget> FindLiveChestTargetAddress(const void* address) {
  const auto records = ChestRecords();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (static_cast<const void*>(&records[index]) == address) {
      return ChestTarget{static_cast<std::uint32_t>(index), room_generation};
    }
  }
  return std::nullopt;
}

Field_MpdFloorTrigger* ResolveLiveTrapTarget(TrapTarget target) {
  const auto records = TrapRecords();
  const auto index = ResolveTrapTarget(target, LiveInteractionTargetDomain());
  return index ? &records[*index] : nullptr;
}

Items_TreasureChest* ResolveLiveChestTarget(ChestTarget target) {
  const auto records = ChestRecords();
  const auto index = ResolveChestTarget(target, LiveInteractionTargetDomain());
  return index ? &records[*index] : nullptr;
}

InteractionTargetState& LiveInteractionTargetState() { return target_state; }

InteractionTargetDomain LiveInteractionTargetDomain() {
  return {room_generation, TrapRecords().size(), ChestRecords().size()};
}

void InvalidateLiveInteractionTargets() {
  target_state.Clear();
  Field_interactionFocus.actor_query = {};
  if (++room_generation == 0) {
    ++room_generation;
  }
}

}  // namespace VS::Field
