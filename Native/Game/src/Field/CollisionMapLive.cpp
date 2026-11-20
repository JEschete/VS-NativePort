// Hand-maintained (tools/port/owned.csv).
#include "Field/CollisionMapLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "Actors/ActorRecord.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/Movement.hpp"
#include "Core/Log.hpp"
#include "Field/Collision.hpp"
#include "Field/CollisionMap.hpp"
#include "Field/CollisionSampling.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.hpp"

using VS::Core::Gte::SVECTOR;

namespace {
using VS::Field::CollisionCellRef;
using VS::Field::CollisionCells;
using VS::Field::CollisionCellSource;
using VS::Field::GimmickSupportState;
using VS::Field::kGimmickSlotCount;
using VS::Field::kMovingPlatformCapacity;
using VS::Field::PlatformContact;

using PlatformWords = std::array<std::uint32_t, kMovingPlatformCapacity>;
using GimmickState = std::optional<GimmickSupportState>;
using GimmickStates = std::array<GimmickState, kGimmickSlotCount>;
using ActorRecordTable = decltype(VS::Actors::actorRecords);

constexpr std::size_t kPlatformRecordBytes = 0xB8;
constexpr std::size_t kPlatformCellOffset = 0xB4;
constexpr std::size_t kActorSlotCount = std::tuple_size_v<ActorRecordTable>;
constexpr std::size_t kFirstRoomActor = 2;

static_assert(sizeof(VS::Field::MpdTileCell) == sizeof(std::uint32_t));
static_assert(sizeof(VS::Field::MpdMovingPlatform) == kPlatformRecordBytes);
static_assert(offsetof(VS::Field::MpdMovingPlatform, tile_cell) ==
              kPlatformCellOffset);
static_assert(std::tuple_size_v<decltype(VS::Field::gimmickInstances)> ==
              kGimmickSlotCount);

// The authored platform records, or none when the section is malformed.
std::span<VS::Field::MpdMovingPlatform> LivePlatformRecords() {
  const std::size_t bytes = VS::Field::roomData.header.moving_platforms_length;
  if (VS::Field::roomData.moving_platforms == nullptr) return {};
  if (bytes % kPlatformRecordBytes != 0 ||
      bytes / kPlatformRecordBytes > kMovingPlatformCapacity) {
    VS::Core::LogWarn("field.collision",
                      "Moving platform section of {} bytes refused", bytes);
    return {};
  }
  return {VS::Field::roomData.moving_platforms, bytes / kPlatformRecordBytes};
}

// The tile cell words are read in place; VS::Field::MpdTileCell is one word.
std::span<VS::Field::MpdTileCell> LiveRoomCells() {
  if (VS::Field::roomData.tile_cells == nullptr) return {};
  return {VS::Field::roomData.tile_cells,
          VS::Field::roomData.header.tile_cells_length /
              sizeof(VS::Field::MpdTileCell)};
}

CollisionCells LiveCells(PlatformWords& platform_words) {
  const auto room = LiveRoomCells();
  const auto platforms = LivePlatformRecords();
  for (std::size_t index = 0; index < platforms.size(); ++index) {
    std::memcpy(&platform_words.at(index), &platforms[index].tile_cell,
                sizeof(std::uint32_t));
  }
  return CollisionCells{
      .map =
          VS::Field::ReadCollisionMapHeader(VS::Field::LiveRoomCollisionMap()),
      .room = {reinterpret_cast<const std::uint32_t*>(room.data()),
               room.size()},
      .platforms = std::span<const std::uint32_t>(platform_words)
                       .first(platforms.size())};
}

VS::Field::MpdTileCell* CellPointer(std::optional<CollisionCellRef> cell) {
  if (!cell) return nullptr;
  if (cell->source == CollisionCellSource::kRoom) {
    return &LiveRoomCells()[cell->index];
  }
  return &LivePlatformRecords()[cell->index].tile_cell;
}

std::vector<PlatformContact> LiveVisiblePlatforms() {
  std::vector<PlatformContact> contacts;
  const auto records = LivePlatformRecords();
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto contact = VS::Field::FindLiveMovingPlatformById(
        static_cast<int>(index) + VS::Field::kPlatformCollisionLayer);
    if (contact) {
      contacts.push_back(*contact);
    }
  }
  return contacts;
}

GimmickStates LiveGimmicks() {
  GimmickStates states{};
  for (std::size_t slot = 0; slot < kGimmickSlotCount; ++slot) {
    const VS::Field::GimmickInstance* gimmick =
        VS::Field::gimmickInstances[slot];
    if (gimmick == nullptr) continue;
    states.at(slot) = GimmickSupportState{
        .skipped = gimmick->is_skipped != 0,
        .falling = gimmick->object_info.is_falling != 0,
        .motion_state = gimmick->motion_state,
        .tile_x = static_cast<std::uint8_t>(gimmick->tile_x),
        .tile_z = static_cast<std::uint8_t>(gimmick->tile_z),
        .mode = gimmick->mode,
        .x = gimmick->position_x,
        .y = gimmick->position_y,
        .z = gimmick->position_z};
  }
  return states;
}

VS::Actors::ActorRecord* LiveActor(int actor_index, const char* caller) {
  if (actor_index < 0 ||
      static_cast<std::size_t>(actor_index) >= kActorSlotCount) {
    VS::Core::LogWarn("field.collision", "{} refused actor slot {}", caller,
                      actor_index);
    return nullptr;
  }
  return VS::Actors::actorRecords[actor_index];
}

class LiveActorCollisionFlags final : public VS::Field::ActorCollisionFlags {
 public:
  explicit LiveActorCollisionFlags(VS::Actors::ActorRecord* actor)
      : actor_(actor) {}

  [[nodiscard]] std::optional<VS::Field::CollisionBypass> Bypass(
      std::int32_t /*actor_index*/) const override {
    if (actor_ == nullptr) return std::nullopt;
    return VS::Field::CollisionBypass{
        .actor_collision = actor_->bypass_actor_collision != 0,
        .ignored = actor_->ignored_by_collision != 0};
  }

  void SetBypass(std::int32_t /*actor_index*/,
                 VS::Field::CollisionBypass bypass) override {
    actor_->bypass_actor_collision = bypass.actor_collision ? 1 : 0;
    actor_->ignored_by_collision = bypass.ignored ? 1 : 0;
  }

 private:
  VS::Actors::ActorRecord* actor_;
};

// Cross-slice calls: retarget when those rows are promoted.
// The platform and gimmick an actor stands on (Actors movement state). The
// original stores the layer as a word over the halfword and its padding.
void RecordSupportingPlatformLayer(std::int32_t layer) {
  VS::Actors::supportingPlatformLayer = static_cast<std::uint16_t>(layer);
}

void RecordSupportingGimmick(std::uint8_t slot) {
  VS::Actors::supportingGimmickIndex = slot;
}
// End of cross-slice calls.
}  // namespace

namespace VS::Field {
VS::Field::MpdTileCell* LiveRoomCollisionCells() {
  PlatformWords words{};
  if (RoomCollisionCells(LiveCells(words)).empty()) return nullptr;
  return VS::Field::roomData.tile_cells;
}

int LiveCollisionMapDimensions(int /*unused*/) {
  return CollisionMapDimensions(ReadCollisionMapHeader(LiveRoomCollisionMap()));
}

std::optional<CameraBounds> LiveRoomCameraBounds() {
  if (VS::Field::roomData.zone_bounds == nullptr) return std::nullopt;
  const auto decoded = RoomCameraBounds(
      {reinterpret_cast<const std::byte*>(VS::Field::roomData.zone_bounds),
       VS::Field::roomData.header.zone_bounds_length});
  if (!decoded) {
    VS::Core::LogWarn("field.collision",
                      "Camera bounds section of {} bytes refused",
                      VS::Field::roomData.header.zone_bounds_length);
  }
  return decoded;
}

VS::Field::MpdTileCell* LiveCollisionCellAt(unsigned int tile_x,
                                            unsigned int tile_z, int layer) {
  PlatformWords words{};
  return CellPointer(CollisionCellAt(LiveCells(words), tile_x, tile_z, layer));
}

const VS::Field::MpdTileCell* LiveCollisionCellOrAbsent(unsigned int tile_x,
                                                        unsigned int tile_z,
                                                        int layer) {
  static const VS::Field::MpdTileCell absent = [] {
    VS::Field::MpdTileCell cell{};
    std::memcpy(&cell, &kAbsentCellWord, sizeof cell);
    return cell;
  }();
  const VS::Field::MpdTileCell* cell =
      LiveCollisionCellAt(tile_x, tile_z, layer);
  return cell != nullptr ? cell : &absent;
}

int SetLiveCollisionCellBlocked(int tile_x, int tile_z, int layer,
                                int blocked) {
  PlatformWords words{};
  const CollisionCells cells = LiveCells(words);
  if (cells.room.empty()) return 0;
  const auto target = SetCollisionCellBlocked(cells, tile_x, tile_z, layer);
  if (!target) {
    VS::Core::LogWarn(
        "field.collision",
        "Blocking cell x={} z={} layer={} outside the tables refused", tile_x,
        tile_z, layer);
    return 0;
  }
  VS::Field::MpdTileCell* cell = CellPointer(target);
  std::uint32_t word = 0;
  std::memcpy(&word, cell, sizeof word);
  word = BlockedCellWord(word, blocked);
  std::memcpy(cell, &word, sizeof word);
  return 1;
}

VS::Field::MpdTileCell* FindLiveCollisionCellAtPosition(const SVECTOR* position,
                                                        int layer) {
  if (position == nullptr) {
    VS::Core::LogWarn("field.collision",
                      "Cell lookup without a position refused");
    return nullptr;
  }
  PlatformWords words{};
  return CellPointer(FindCollisionCellAtPosition(LiveCells(words), position->vx,
                                                 position->vz, layer));
}

std::int16_t LiveInterpolatedFloorHeight(int world_x, int world_z) {
  return InterpolatedFloorHeight(LiveRoomCollisionMap(), world_x, world_z);
}

std::int16_t LiveFloorHeightAt(int world_x, int world_z) {
  return FloorHeightAt(LiveRoomCollisionMap(), world_x, world_z);
}

std::int16_t LiveCeilingHeightAt(int world_x, int world_z) {
  return CeilingHeightAt(LiveRoomCollisionMap(), world_x, world_z);
}

int LiveFloorAndCeilingHeightsAt(int world_x, int world_z) {
  const FloorAndCeiling heights =
      FloorAndCeilingHeightsAt(LiveRoomCollisionMap(), world_x, world_z);
  const auto floor = static_cast<std::uint16_t>(heights.floor);
  const auto ceiling =
      static_cast<std::uint32_t>(static_cast<std::uint16_t>(heights.ceiling));
  constexpr int kCeilingShift = 16;
  return static_cast<int>((ceiling << kCeilingShift) | floor);
}

int LiveRoomCeilingHeight(int world_x, int world_z) {
  return RoomCeilingHeight(LiveRoomCollisionMap(), world_x, world_z);
}

int LiveActorSupportHeight(int world_x, int world_z) {
  const GimmickStates gimmicks = LiveGimmicks();
  return ActorSupportHeight(LiveRoomCollisionMap(), gimmicks, world_x, world_z);
}

void SnapLiveRoomActorsToSupport() {
  std::array<std::optional<RoomActorSupport>, kActorSlotCount - kFirstRoomActor>
      actors{};
  for (std::size_t slot = kFirstRoomActor; slot < kActorSlotCount; ++slot) {
    const VS::Actors::ActorRecord* actor = VS::Actors::actorRecords[slot];
    if (actor == nullptr) continue;
    actors.at(slot - kFirstRoomActor) =
        RoomActorSupport{.motion_locked = actor->instance.motion_locked != 0,
                         .tile_layer = actor->instance.current_tile_layer,
                         .movement_mode = actor->instance.movement_mode,
                         .x = actor->instance.position.vx,
                         .y = actor->instance.position.vy,
                         .z = actor->instance.position.vz};
  }
  const GimmickStates gimmicks = LiveGimmicks();
  SnapRoomActorsToSupport(LiveRoomCollisionMap(), gimmicks, actors);
  for (std::size_t slot = kFirstRoomActor; slot < kActorSlotCount; ++slot) {
    const auto& placed = actors.at(slot - kFirstRoomActor);
    if (placed) {
      VS::Actors::actorRecords[slot]->instance.position.vy = placed->y;
    }
  }
}

int LiveGimmickSupportHeight(int tile_x, int tile_z) {
  return LiveGimmickSupport(tile_x, tile_z).height;
}

int LiveGimmickSupportHeightBelow(int tile_x, int tile_z,
                                  int reference_height) {
  const GimmickStates gimmicks = LiveGimmicks();
  return GimmickSupportHeightBelow(gimmicks, tile_x, tile_z, reference_height);
}

int LiveIsPointOnMovingPlatform(int platform_layer, const SVECTOR* position) {
  if (position == nullptr) {
    VS::Core::LogWarn("field.collision",
                      "Platform test without a position refused");
    return 0;
  }
  const auto platforms = LiveVisiblePlatforms();
  if (std::ranges::none_of(platforms, [&](const PlatformContact& platform) {
        return platform.platform_index ==
               platform_layer - kPlatformCollisionLayer;
      })) {
    VS::Core::LogWarn("field.collision", "No visible platform on layer {}",
                      platform_layer);
  }
  return IsPointOnMovingPlatform(platforms, platform_layer, position->vx,
                                 position->vz)
             ? 1
             : 0;
}

int LiveMovingPlatformCount() {
  return static_cast<int>(LivePlatformRecords().size());
}

int LiveMovingPlatformContactHeight(int world_x, int world_y, int world_z,
                                    int search_mode) {
  return LiveMovingPlatformContact({.x = world_x, .y = world_y, .z = world_z},
                                   search_mode)
      .height;
}

void SetLiveActorCollisionBypass(int actor_index, int flags) {
  LiveActorCollisionFlags actors(
      LiveActor(actor_index, "SetLiveActorCollisionBypass"));
  SetActorCollisionBypass(actors, actor_index, flags);
}

int LiveActorCollisionBypass(int actor_index) {
  const LiveActorCollisionFlags actors(
      LiveActor(actor_index, "LiveActorCollisionBypass"));
  return ActorCollisionBypass(actors, actor_index);
}

std::optional<CollisionCellRef> LiveCollisionCellRefAtPosition(
    std::int16_t world_x, std::int16_t world_z, int layer) {
  PlatformWords words{};
  return FindCollisionCellAtPosition(LiveCells(words), world_x, world_z, layer);
}

std::optional<std::uint32_t> LiveCollisionCellWord(CollisionCellRef cell) {
  const VS::Field::MpdTileCell* source = nullptr;
  if (cell.source == CollisionCellSource::kRoom) {
    const auto room = LiveRoomCells();
    if (cell.index < room.size()) source = &room[cell.index];
  } else {
    const auto platforms = LivePlatformRecords();
    if (cell.index < platforms.size()) {
      source = &platforms[cell.index].tile_cell;
    }
  }
  if (source == nullptr) return std::nullopt;
  std::uint32_t word = 0;
  std::memcpy(&word, source, sizeof word);
  return word;
}

GimmickSupport LiveGimmickSupport(int tile_x, int tile_z) {
  const GimmickStates gimmicks = LiveGimmicks();
  const GimmickSupport support = GimmickSupportHeight(gimmicks, tile_x, tile_z);
  RecordSupportingGimmick(support.gimmick_index);
  return support;
}

PlatformContactHeight LiveMovingPlatformContact(PlatformProbe probe,
                                                int search_mode) {
  const auto platforms = LiveVisiblePlatforms();
  const PlatformContactHeight contact = MovingPlatformContactHeight(
      platforms, probe, search_mode, platformContactSearchDepth);
  RecordSupportingPlatformLayer(contact.supporting_layer);
  return contact;
}

std::optional<PlatformContact> LiveVisiblePlatform(int support_layer) {
  return FindLiveMovingPlatformById(support_layer);
}
}  // namespace VS::Field
