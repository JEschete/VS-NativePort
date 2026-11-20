// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapNavigationLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Audio/SfxLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Log.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/MinimapScene.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.h"
#include "Menu/Map.hpp"
#include "Menu/MapNavigation.hpp"
#include "Menu/MapSceneStorageLive.hpp"

namespace VS::Menu::map {
namespace {
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kVertexBytes = 8;
constexpr std::size_t kVertexYOffset = 2;
constexpr std::size_t kVertexZOffset = 4;
constexpr std::size_t kVisitedOffset = 0;
constexpr std::size_t kDataOffset = 4;
constexpr std::size_t kZoneOffset = 8;
constexpr std::size_t kMapOffset = 10;
constexpr std::size_t kLinkPreviousOffset = 32;
constexpr std::size_t kLinkNextOffset = 34;

static_assert(sizeof(SVECTOR) == kVertexBytes);
static_assert(offsetof(Field::MapRoomRecord, visited) == kVisitedOffset);
static_assert(offsetof(Field::MapRoomRecord, data_offset) == kDataOffset);
static_assert(offsetof(Field::MapRoomRecord, zone_id) == kZoneOffset);
static_assert(offsetof(Field::MapRoomRecord, map_id) == kMapOffset);
static_assert(offsetof(Field::MapRoomName, previous) == kLinkPreviousOffset);
static_assert(offsetof(Field::MapRoomName, next) == kLinkNextOffset);

std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refused_calls)) {
    VS::Core::LogWarn("menu", "map navigation: {} refused: {} ({} refusals)",
                      call, reason, refused_calls);
  }
}

template <typename T>
[[nodiscard]] T Read(std::span<const std::byte> bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.subspan(offset, sizeof(T)).data(), sizeof(T));
  return value;
}

template <typename T>
void Write(std::span<std::byte> bytes, std::size_t offset, T value) {
  std::memcpy(bytes.subspan(offset, sizeof(T)).data(), &value, sizeof(T));
}

[[nodiscard]] std::size_t RecordOffset(std::size_t room) {
  return kWordBytes + (room * Field::kMapSceneRoomRecordBytes);
}

// The buffer `scene` was loaded into: the ARM scene the menu loaded or the
// current room's minimap scene.
[[nodiscard]] std::optional<std::span<std::byte>> SceneBytes(
    const void* scene) {
  if (scene == nullptr) return std::nullopt;
  if (scene == VS::Battle::sceneBuffer) {
    if (const auto loaded = BoundLiveMapScene(scene)) return loaded;
    if (currentScene < 0 || currentScene >= kSceneCount) {
      return std::nullopt;
    }
    const int size = kSceneArmFiles[currentScene].size;
    if (size < 0) return std::nullopt;
    return std::span{static_cast<std::byte*>(VS::Battle::sceneBuffer),
                     static_cast<std::size_t>(size)};
  }
  if (scene == VS::Field::roomData.minimap_scene) {
    return std::span{
        reinterpret_cast<std::byte*>(VS::Field::roomData.minimap_scene),
        static_cast<std::size_t>(
            VS::Field::roomData.header.minimap_scene_length)};
  }
  return std::nullopt;
}

// The scene's room records; a negative count has none, as the original's
// loops run no rooms.
[[nodiscard]] std::optional<std::size_t> RoomCountOf(
    std::span<const std::byte> scene) {
  if (scene.size() < kWordBytes) return std::nullopt;
  const auto count = Read<std::int32_t>(scene, 0);
  if (count <= 0) return std::size_t{0};
  const auto rooms = static_cast<std::size_t>(count);
  if (rooms > (scene.size() - kWordBytes) / Field::kMapSceneRoomRecordBytes) {
    return std::nullopt;
  }
  return rooms;
}

[[nodiscard]] std::vector<MapRoom> RoomsOf(std::span<const std::byte> scene,
                                           std::size_t count) {
  std::vector<MapRoom> rooms(count);
  for (std::size_t room = 0; room < count; ++room) {
    const std::size_t record = RecordOffset(room);
    rooms[room] = {
        .visited = Read<std::int32_t>(scene, record + kVisitedOffset),
        .zone_id = Read<std::int16_t>(scene, record + kZoneOffset),
        .map_id = Read<std::int16_t>(scene, record + kMapOffset)};
  }
  return rooms;
}

// A room's vertex block: where its vertices start and how many there are.
struct VertexBlock {
  std::size_t first = 0;
  std::size_t count = 0;
};

// Every room's vertex block, copied out for the rules and back after.
class SceneVertices {
 public:
  [[nodiscard]] static std::optional<SceneVertices> Of(
      std::span<std::byte> scene, std::size_t room_count) {
    SceneVertices vertices{scene};
    std::vector<std::pair<std::size_t, std::size_t>> extents;
    for (std::size_t room = 0; room < room_count; ++room) {
      const std::size_t record = RecordOffset(room);
      // The record's on-disc pointer, resolved to an offset from the record.
      const auto offset = Read<std::uint32_t>(scene, record + kDataOffset);
      const std::size_t block = record + offset;
      if (offset > scene.size() || block > scene.size() - kWordBytes) {
        return std::nullopt;
      }
      const auto stored = Read<std::int32_t>(scene, block);
      const std::size_t count =
          stored > 0 ? static_cast<std::size_t>(stored) : 0;
      const std::size_t first = block + kWordBytes;
      if (count > (scene.size() - first) / kVertexBytes) return std::nullopt;
      extents.emplace_back(first, first + (count * kVertexBytes));
      vertices.blocks_.push_back({first, count});
      vertices.rooms_.emplace_back(count);
      for (std::size_t vertex = 0; vertex < count; ++vertex) {
        const std::size_t at = first + (vertex * kVertexBytes);
        vertices.rooms_.back()[vertex] = {
            .x = Read<std::int16_t>(scene, at),
            .y = Read<std::int16_t>(scene, at + kVertexYOffset),
            .z = Read<std::int16_t>(scene, at + kVertexZOffset)};
      }
    }
    // Shared blocks would be moved once per room by the original but once
    // here.
    std::ranges::sort(extents);
    for (std::size_t block = 1; block < extents.size(); ++block) {
      if (extents[block].first < extents[block - 1].second) {
        return std::nullopt;
      }
    }
    for (std::vector<MapVertex>& room : vertices.rooms_) {
      vertices.views_.emplace_back(room);
    }
    return vertices;
  }

  [[nodiscard]] std::span<const RoomVertices> Rooms() const { return views_; }

  void Store() const {
    for (std::size_t room = 0; room < blocks_.size(); ++room) {
      for (std::size_t vertex = 0; vertex < blocks_[room].count; ++vertex) {
        const std::size_t at = blocks_[room].first + (vertex * kVertexBytes);
        const MapVertex& value = rooms_[room][vertex];
        Write(scene_, at, value.x);
        Write(scene_, at + kVertexYOffset, value.y);
        Write(scene_, at + kVertexZOffset, value.z);
      }
    }
  }

 private:
  explicit SceneVertices(std::span<std::byte> scene) : scene_(scene) {}

  std::span<std::byte> scene_;
  std::vector<VertexBlock> blocks_;
  std::vector<std::vector<MapVertex>> rooms_;
  std::vector<RoomVertices> views_;
};

struct LiveScene {
  std::span<std::byte> bytes;
  std::size_t room_count = 0;
};

[[nodiscard]] std::optional<LiveScene> LiveSceneOf(std::string_view call,
                                                   const void* scene) {
  const auto bytes = SceneBytes(scene);
  if (!bytes.has_value()) {
    Refuse(call, "not the ARM or minimap scene");
    return std::nullopt;
  }
  const auto count = RoomCountOf(*bytes);
  if (!count.has_value()) {
    Refuse(call, "room records outside the scene");
    return std::nullopt;
  }
  return LiveScene{*bytes, *count};
}

[[nodiscard]] std::optional<SceneVertices> LiveVerticesOf(
    std::string_view call, const LiveScene& scene) {
  auto vertices = SceneVertices::Of(scene.bytes, scene.room_count);
  if (!vertices.has_value()) {
    Refuse(call, "room vertices outside the scene or shared");
  }
  return vertices;
}

[[nodiscard]] MapPoint CenterPoint() {
  return {.x = centerPoint.vx, .y = centerPoint.vy, .z = centerPoint.vz};
}

void SetCenterPoint(MapPoint center) {
  centerPoint.vx = center.x;
  centerPoint.vy = center.y;
  centerPoint.vz = center.z;
}

class LiveMapNavigationHost final : public MapNavigationHost {
 public:
  void PlaySound(std::int32_t sound, std::int32_t volume) override {
    Audio::LivePlaySfxCentered(sound, volume);
  }
};
}  // namespace

// decomp: MENU5.PRG func_8010467C (4D8.c:1048).
void LiveMarkAllRoomsVisited(int* scene) {
  constexpr std::string_view kCall = "MarkAllRoomsVisited";
  const auto live = LiveSceneOf(kCall, scene);
  if (!live.has_value()) return;
  std::vector<MapRoom> rooms = RoomsOf(live->bytes, live->room_count);
  MarkAllRoomsVisited(rooms);
  for (std::size_t room = 0; room < rooms.size(); ++room) {
    Write(live->bytes, RecordOffset(room) + kVisitedOffset,
          rooms[room].visited);
  }
}

// decomp: MENU5.PRG _snapMapToRoom (4D8.c:1511).
void LiveCenterOnRoom(Field::MapScene* scene, int room_id) {
  constexpr std::string_view kCall = "CenterOnRoom";
  const auto live = LiveSceneOf(kCall, scene);
  if (!live.has_value()) return;
  if (room_id < 0 || static_cast<std::size_t>(room_id) >= live->room_count) {
    Refuse(kCall, "room outside the scene");
    return;
  }
  const auto vertices = LiveVerticesOf(kCall, *live);
  if (!vertices.has_value()) return;
  MapPoint center = CenterPoint();
  if (!CenterOnRoom(vertices->Rooms(), static_cast<std::size_t>(room_id),
                    center)) {
    Refuse(kCall, "room outside the scene");
    return;
  }
  vertices->Store();
  SetCenterPoint(center);
}

// decomp: MENU5.PRG _setCenterpoint (4D8.c:1578).
void LiveSetRoomCenterTarget(Field::MapScene* scene, int room_id) {
  constexpr std::string_view kCall = "SetRoomCenterTarget";
  const auto live = LiveSceneOf(kCall, scene);
  if (!live.has_value()) return;
  if (room_id < 0 || static_cast<std::size_t>(room_id) >= live->room_count) {
    Refuse(kCall, "room outside the scene");
    return;
  }
  const auto vertices = LiveVerticesOf(kCall, *live);
  if (!vertices.has_value()) return;
  SetCenterPoint(
      RoomCenterOf(vertices->Rooms()[static_cast<std::size_t>(room_id)]));
}

// decomp: MENU5.PRG _smoothMapToCenterpoint (4D8.c:1629).
void LiveAnimateCentering(Field::MapScene* scene) {
  constexpr std::string_view kCall = "AnimateCentering";
  MapPoint center = CenterPoint();
  if (center == MapPoint{}) return;
  const auto live = LiveSceneOf(kCall, scene);
  if (!live.has_value()) return;
  const auto vertices = LiveVerticesOf(kCall, *live);
  if (!vertices.has_value()) return;
  AnimateCentering(vertices->Rooms(), center);
  vertices->Store();
  SetCenterPoint(center);
}

// decomp: MENU5.PRG _scaleRoomVertices (4D8.c:1728).
void LiveScaleRoomVertices(Field::MapScene* scene, int factor) {
  constexpr std::string_view kCall = "ScaleRoomVertices";
  const auto live = LiveSceneOf(kCall, scene);
  if (!live.has_value()) return;
  const auto vertices = LiveVerticesOf(kCall, *live);
  if (!vertices.has_value()) return;
  ScaleRoomVertices(vertices->Rooms(), factor);
  vertices->Store();
}

// decomp: MENU5.PRG _getCurrentRoomIndex (4D8.c:1834).
int LiveInitialRoomIndex(Field::MapScene* scene) {
  constexpr std::string_view kCall = "InitialRoomIndex";
  const auto live = LiveSceneOf(kCall, scene);
  if (!live.has_value()) return 0;
  std::optional<Field::SceneRoomAddress> current;
  if (VS::Field::roomData.minimap_scene == nullptr) {
    Refuse(kCall, "no minimap scene");
  } else {
    current = Field::CurrentSceneRoomAddress(
        {reinterpret_cast<const std::byte*>(VS::Field::roomData.minimap_scene),
         VS::Field::roomData.header.minimap_scene_length});
    if (!current.has_value()) Refuse(kCall, "empty minimap scene");
  }
  const InitialRoom initial =
      InitialRoomOf(RoomsOf(live->bytes, live->room_count), current);
  isCurrentScene = initial.current;
  return initial.selected;
}

// decomp: MENU5.PRG _updateRoomIndex (4D8.c:1862).
void LiveSelectAdjacentVisitedRoom(int search_forward) {
  constexpr std::string_view kCall = "SelectAdjacentVisitedRoom";
  const auto live = LiveSceneOf(kCall, VS::Battle::sceneBuffer);
  if (!live.has_value()) return;
  const auto* names = reinterpret_cast<const std::byte*>(roomNamesTable);
  const std::byte* scene_begin = live->bytes.data();
  const std::byte* scene_end = scene_begin + live->bytes.size();
  if (names == nullptr || names < scene_begin || names > scene_end ||
      live->room_count > static_cast<std::size_t>(scene_end - names) /
                             Field::kMapSceneRoomNameBytes) {
    Refuse(kCall, "room names outside the scene");
    return;
  }
  const std::span<const std::byte> name_bytes{
      names, live->room_count * Field::kMapSceneRoomNameBytes};
  std::vector<RoomLink> links(live->room_count);
  for (std::size_t room = 0; room < links.size(); ++room) {
    const std::size_t name = room * Field::kMapSceneRoomNameBytes;
    links[room] = {
        .previous = Read<std::int16_t>(name_bytes, name + kLinkPreviousOffset),
        .next = Read<std::int16_t>(name_bytes, name + kLinkNextOffset)};
  }
  LiveMapNavigationHost host;
  const auto selected = SelectAdjacentVisitedRoom(
      host, links, RoomsOf(live->bytes, live->room_count), currentRoomIndex,
      search_forward);
  if (!selected.has_value()) {
    Refuse(kCall, "a link leaves the rooms or no room is visited");
    return;
  }
  currentRoomIndex = *selected;
}
}  // namespace VS::Menu::map
