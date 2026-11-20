#include "Field/InteractionTargetLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Interactions.hpp"
#include "Field/Mpd.hpp"
#include "Items/ItemRecords.hpp"
#include "State/StateFlags.hpp"

using VS::Core::Gte::SVECTOR;

namespace VS::Field {
namespace {

InteractionTargetState targetState;
std::uint64_t roomGeneration = 1;
constexpr std::size_t kOriginalFloorRecordBytes = 12;
constexpr std::size_t kOriginalChestRecordBytes = 0x234;

std::span<VS::Field::MpdFloorTrigger> TrapRecords() {
  if (VS::Field::roomData.floor_triggers == nullptr ||
      VS::Field::roomData.header.floor_triggers_length %
              kOriginalFloorRecordBytes !=
          0 ||
      reinterpret_cast<std::uintptr_t>(VS::Field::roomData.floor_triggers) %
              alignof(VS::Field::MpdFloorTrigger) !=
          0) {
    return {};
  }
  return {VS::Field::roomData.floor_triggers,
          VS::Field::roomData.header.floor_triggers_length /
              kOriginalFloorRecordBytes};
}

std::span<VS::Items::TreasureChest> ChestRecords() {
  if (VS::Field::roomData.containers == nullptr) {
    return {};
  }
  return {
      VS::Field::roomData.containers,
      VS::Field::roomData.header.containers_length / kOriginalChestRecordBytes};
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

class LiveRoomTriggerActivationHost final : public TrapDisableHost {
 public:
  LiveRoomTriggerActivationHost()
      : records_(TrapRecords()),
        generation_(roomGeneration),
        pointer_(VS::Field::roomData.floor_triggers),
        length_(VS::Field::roomData.header.floor_triggers_length) {}
  InteractionTargetDomain Domain() override {
    return {generation_, records_.size(), 0};
  }
  bool StoreActivation(TrapTarget target, std::uint8_t activation) override {
    if (generation_ != roomGeneration ||
        pointer_ != VS::Field::roomData.floor_triggers ||
        length_ != VS::Field::roomData.header.floor_triggers_length) {
      return false;
    }
    const auto index = ResolveTrapTarget(target, Domain());
    if (!index) return false;
    records_[*index].activation = std::bit_cast<char>(activation);
    return true;
  }
  std::optional<TrapTarget> Find(const RoomTriggerTile& tile) override {
    if (!Valid()) return std::nullopt;
    return FindLiveRoomTriggerAtTile(tile);
  }
  std::optional<RoomTriggerTile> Tile(TrapTarget target) override {
    if (!Valid()) return std::nullopt;
    const auto index = ResolveTrapTarget(target, Domain());
    if (!index) return std::nullopt;
    const auto& record = records_[*index];
    return RoomTriggerTile{record.tile_x, record.tile_z, record.floor_layer};
  }
  bool MarkObstacle(const RoomTriggerTile& tile) override {
    constexpr auto kGridSide = VS::Battle::Ai::kNavigationGridSide;
    if (!Valid() || (VS::Battle::Ai::tileFlagMap != nullptr &&
                     (tile.x < 0 || tile.z < 0 || tile.x >= kGridSide ||
                      tile.z >= kGridSide))) {
      return false;
    }
    VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(tile.x, tile.z);
    return Valid();
  }

 private:
  bool Valid() const {
    return generation_ == roomGeneration &&
           pointer_ == VS::Field::roomData.floor_triggers &&
           length_ == VS::Field::roomData.header.floor_triggers_length;
  }
  std::span<VS::Field::MpdFloorTrigger> records_;
  std::uint64_t generation_;
  VS::Field::MpdFloorTrigger* pointer_;
  std::uint32_t length_;
};

}  // namespace

std::vector<RoomTriggerInfo> LiveActiveRoomTriggerInfo() {
  if (VS::State::stateFlags.floor_traps_disabled_20 != 0 ||
      VS::State::stateFlags.floor_traps_disabled_40 != 0) {
    return {};
  }
  const auto records = TrapRecords();
  std::vector<RoomTriggerInfo> result;
  result.reserve(records.size());
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto& record = records[index];
    result.push_back(
        {.target = {static_cast<std::uint32_t>(index), roomGeneration},
         .kind = record.kind,
         .tile_x = record.tile_x,
         .tile_z = record.tile_z,
         .floor_layer = record.floor_layer});
  }
  return result;
}

std::optional<TrapTarget> FindLiveRoomTriggerAtTile(
    const RoomTriggerTile& tile) {
  const auto records = TrapRecords();
  std::vector<RoomTriggerInfo> values;
  values.reserve(records.size());
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto& record = records[index];
    values.push_back({{static_cast<std::uint32_t>(index), roomGeneration},
                      record.kind,
                      record.tile_x,
                      record.tile_z,
                      record.floor_layer});
  }
  return FindRoomTriggerAtTile(
      values, tile,
      VS::State::stateFlags.floor_traps_disabled_20 != 0 ||
          VS::State::stateFlags.floor_traps_disabled_40 != 0);
}

bool SetLiveRoomTriggerEnabled(std::optional<TrapTarget> target,
                               std::int32_t activation) {
  if (VS::Field::roomData.floor_triggers != nullptr && TrapRecords().empty() &&
      VS::Field::roomData.header.floor_triggers_length != 0) {
    return false;
  }
  LiveRoomTriggerActivationHost host;
  return SetRoomTriggerEnabled(target, activation, host);
}

bool DisableLiveTrapAtPosition(TrapWorldPosition position) {
  LiveRoomTriggerActivationHost host;
  return DisableTrapAtPosition(position, host);
}

std::optional<TrapTarget> FindLiveTrapTarget(
    const VS::Field::MpdFloorTrigger* record) {
  const auto index = RecordIndex(TrapRecords(), record);
  return index ? std::optional{TrapTarget{*index, roomGeneration}}
               : std::nullopt;
}

std::optional<TrapTarget> FindLiveTrapTargetAddress(const void* address) {
  const auto records = TrapRecords();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (static_cast<const void*>(&records[index]) == address) {
      return TrapTarget{static_cast<std::uint32_t>(index), roomGeneration};
    }
  }
  return std::nullopt;
}

std::optional<ChestTarget> FindLiveChestTarget(
    const VS::Items::TreasureChest* record) {
  const auto index = RecordIndex(ChestRecords(), record);
  return index ? std::optional{ChestTarget{*index, roomGeneration}}
               : std::nullopt;
}

std::optional<ChestTarget> FindLiveChestTargetAddress(const void* address) {
  const auto records = ChestRecords();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (static_cast<const void*>(&records[index]) == address) {
      return ChestTarget{static_cast<std::uint32_t>(index), roomGeneration};
    }
  }
  return std::nullopt;
}

VS::Field::MpdFloorTrigger* ResolveLiveTrapTarget(TrapTarget target) {
  const auto records = TrapRecords();
  const auto index = ResolveTrapTarget(target, LiveInteractionTargetDomain());
  return index ? &records[*index] : nullptr;
}

VS::Items::TreasureChest* ResolveLiveChestTarget(ChestTarget target) {
  const auto records = ChestRecords();
  const auto index = ResolveChestTarget(target, LiveInteractionTargetDomain());
  return index ? &records[*index] : nullptr;
}

InteractionTargetState& LiveInteractionTargetState() { return targetState; }

InteractionTargetDomain LiveInteractionTargetDomain() {
  return {roomGeneration, TrapRecords().size(), ChestRecords().size()};
}

void InvalidateLiveInteractionTargets() {
  targetState.Clear();
  interactionFocus.actor_query = {};
  if (++roomGeneration == 0) {
    ++roomGeneration;
  }
}

}  // namespace VS::Field

VS::Field::MpdFloorTrigger* VS::Field::RoomTriggerAtTileEntry(int x, int z,
                                                              int layer) {
  const auto target = VS::Field::FindLiveRoomTriggerAtTile({x, z, layer});
  return target ? VS::Field::ResolveLiveTrapTarget(*target) : nullptr;
}

void VS::Field::SetRoomTriggerEnabledEntry(VS::Field::MpdFloorTrigger* selected,
                                           int activation) {
  if (selected == nullptr) {
    static_cast<void>(
        VS::Field::SetLiveRoomTriggerEnabled(std::nullopt, activation));
    return;
  }
  const auto target = VS::Field::FindLiveTrapTarget(selected);
  if (target) {
    static_cast<void>(VS::Field::SetLiveRoomTriggerEnabled(target, activation));
  }
}

void VS::Field::DisableTrapAtPositionEntry(SVECTOR* position) {
  if (position == nullptr) return;
  static_cast<void>(
      VS::Field::DisableLiveTrapAtPosition({position->vx, position->vz}));
}
