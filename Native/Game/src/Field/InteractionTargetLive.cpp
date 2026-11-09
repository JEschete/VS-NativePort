#include "Field/InteractionTargetLive.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Field/InteractionTargetLive.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Ai/Navigation.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Items/TreasureChests.h"
#include "State/StateFlags.h"
}

namespace VS::Field {
namespace {

InteractionTargetState target_state;
std::uint64_t room_generation = 1;
constexpr std::size_t kOriginalFloorRecordBytes = 12;
constexpr std::size_t kOriginalChestRecordBytes = 0x234;

std::span<Field_MpdFloorTrigger> TrapRecords() {
  if (Field_roomData.floorTriggers == nullptr ||
      Field_roomData.header.floorTriggersLength % kOriginalFloorRecordBytes !=
          0 ||
      reinterpret_cast<std::uintptr_t>(Field_roomData.floorTriggers) %
              alignof(Field_MpdFloorTrigger) !=
          0) {
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

class LiveRoomTriggerActivationHost final : public TrapDisableHost {
 public:
  LiveRoomTriggerActivationHost()
      : records_(TrapRecords()),
        generation_(room_generation),
        pointer_(Field_roomData.floorTriggers),
        length_(Field_roomData.header.floorTriggersLength) {}
  InteractionTargetDomain Domain() override {
    return {generation_, records_.size(), 0};
  }
  bool StoreActivation(TrapTarget target, std::uint8_t activation) override {
    if (generation_ != room_generation ||
        pointer_ != Field_roomData.floorTriggers ||
        length_ != Field_roomData.header.floorTriggersLength) {
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
    return RoomTriggerTile{record.tileX, record.tileZ, record.floorLayer};
  }
  bool MarkObstacle(const RoomTriggerTile& tile) override {
    constexpr auto kGridSide = VS::Battle::Ai::kNavigationGridSide;
    if (!Valid() || (Battle_Ai_tileFlagMap != nullptr &&
                     (tile.x < 0 || tile.z < 0 || tile.x >= kGridSide ||
                      tile.z >= kGridSide))) {
      return false;
    }
    VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(tile.x, tile.z);
    return Valid();
  }

 private:
  bool Valid() const {
    return generation_ == room_generation &&
           pointer_ == Field_roomData.floorTriggers &&
           length_ == Field_roomData.header.floorTriggersLength;
  }
  std::span<Field_MpdFloorTrigger> records_;
  std::uint64_t generation_;
  Field_MpdFloorTrigger* pointer_;
  std::uint32_t length_;
};

}  // namespace

std::vector<RoomTriggerInfo> LiveActiveRoomTriggerInfo() {
  if (Audio_Seq_unusedStateFlags.opaque20 != 0 ||
      Audio_Seq_unusedStateFlags.opaque40 != 0) {
    return {};
  }
  const auto records = TrapRecords();
  std::vector<RoomTriggerInfo> result;
  result.reserve(records.size());
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto& record = records[index];
    result.push_back(
        {.target = {static_cast<std::uint32_t>(index), room_generation},
         .kind = record.kind,
         .tile_x = record.tileX,
         .tile_z = record.tileZ,
         .floor_layer = record.floorLayer});
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
    values.push_back({{static_cast<std::uint32_t>(index), room_generation},
                      record.kind,
                      record.tileX,
                      record.tileZ,
                      record.floorLayer});
  }
  return FindRoomTriggerAtTile(values, tile,
                               Audio_Seq_unusedStateFlags.opaque20 != 0 ||
                                   Audio_Seq_unusedStateFlags.opaque40 != 0);
}

bool SetLiveRoomTriggerEnabled(std::optional<TrapTarget> target,
                               std::int32_t activation) {
  if (Field_roomData.floorTriggers != nullptr && TrapRecords().empty() &&
      Field_roomData.header.floorTriggersLength != 0) {
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

Field_MpdFloorTrigger* Field_RoomTriggerAtTileEntry(int x, int z, int layer) {
  const auto target = VS::Field::FindLiveRoomTriggerAtTile({x, z, layer});
  return target ? VS::Field::ResolveLiveTrapTarget(*target) : nullptr;
}

void Field_SetRoomTriggerEnabledEntry(Field_MpdFloorTrigger* selected,
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

void Field_DisableTrapAtPositionEntry(SVECTOR* position) {
  if (position == nullptr) return;
  static_cast<void>(
      VS::Field::DisableLiveTrapAtPosition({position->vx, position->vz}));
}
