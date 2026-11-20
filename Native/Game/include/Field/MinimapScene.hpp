#pragma once
// Hand-maintained (tools/port/owned.csv).
// The ARM/MPD map scene: a room count, 12-byte room records, per-room vertex
// and primitive tables and a trailing room-name table. Resolving rewrites
// each record's payload length as its offset from that record, the form the
// minimap and the map menu read.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Field {

inline constexpr std::size_t kMapSceneHeaderBytes = 4;
inline constexpr std::size_t kMapSceneRoomRecordBytes = 12;
inline constexpr std::size_t kMapSceneRoomNameBytes = 36;
inline constexpr std::size_t kMapRoomNameChars = 32;
inline constexpr std::size_t kMinimapMaximumVertices = 256;
inline constexpr std::size_t kMinimapTriangleCorners = 3;
inline constexpr std::size_t kMinimapQuadCorners = 4;

struct MinimapVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;

  friend bool operator==(const MinimapVertex&, const MinimapVertex&) = default;
};

struct MinimapScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const MinimapScreenPoint&,
                         const MinimapScreenPoint&) = default;
};

using MinimapTriangle = std::array<MinimapScreenPoint, kMinimapTriangleCorners>;
using MinimapQuad = std::array<MinimapScreenPoint, kMinimapQuadCorners>;

struct MinimapColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const MinimapColor&, const MinimapColor&) = default;
};

// Primitive counts reserved before drawing: room 0's fills, dim lines and
// door markers plus every visited room's bright lines.
struct MinimapPrimitiveCounts {
  std::size_t triangles = 0;
  std::size_t quads = 0;
  std::size_t lines = 0;
  std::size_t doors = 0;

  friend bool operator==(const MinimapPrimitiveCounts&,
                         const MinimapPrimitiveCounts&) = default;
};

class MinimapRenderer {
 public:
  MinimapRenderer() = default;
  MinimapRenderer(const MinimapRenderer&) = delete;
  MinimapRenderer& operator=(const MinimapRenderer&) = delete;
  MinimapRenderer(MinimapRenderer&&) = delete;
  MinimapRenderer& operator=(MinimapRenderer&&) = delete;
  virtual ~MinimapRenderer() = default;

  // Throws when the frame cannot hold the primitives.
  virtual void Begin(const MinimapPrimitiveCounts& counts) = 0;
  [[nodiscard]] virtual MinimapScreenPoint Project(
      const MinimapVertex& vertex) = 0;
  [[nodiscard]] virtual bool IsDoorGateLocked(std::uint8_t gate) = 0;
  virtual void DrawTriangle(const MinimapTriangle& corners,
                            MinimapColor color) = 0;
  // Corners in strip order: the original's first, second, fourth and third.
  virtual void DrawQuad(const MinimapQuad& corners, MinimapColor color) = 0;
  virtual void DrawLine(MinimapScreenPoint from, MinimapScreenPoint to,
                        MinimapColor color) = 0;
  virtual void DrawLockedDoor(MinimapScreenPoint position) = 0;
  virtual void DrawDoorCross(MinimapScreenPoint position,
                             MinimapColor color) = 0;
  virtual void Finish() = 0;
};

// The scene's on-disc records. Readers copy them out of the scene's bytes;
// these name the fields and fix the layout.
// decomp: vs_Scene (src/BATTLE/BATTLE.PRG/mpd.h:114, formerly vs_battle_scene)
// without its zero-length rooms[]: the room records follow the header.
struct MapScene {
  std::int32_t room_count;
};
// decomp: vs_Room (src/BATTLE/BATTLE.PRG/mpd.h:109, formerly vs_battle_room)
struct MapRoomRecord {
  std::int32_t visited;
  // The room payload's byte length on disc; ResolveSceneRoomData rewrites it
  // as the payload's offset from this record.
  std::uint32_t data_offset;
  std::int16_t zone_id;
  std::int16_t map_id;
};
// decomp: vs_RoomName (src/BATTLE/BATTLE.PRG/mpd.h:238, formerly
// vs_battle_roomName)
struct MapRoomName {
  std::array<char, kMapRoomNameChars> name;
  std::int16_t previous;
  std::int16_t next;
};
static_assert(sizeof(MapScene) == kMapSceneHeaderBytes);
static_assert(sizeof(MapRoomRecord) == kMapSceneRoomRecordBytes);
static_assert(sizeof(MapRoomName) == kMapSceneRoomNameBytes);

[[nodiscard]] bool IsValidMapScene(std::span<const std::byte> scene);

struct SceneRoomAddress {
  std::int16_t zone_id = 0;
  std::int16_t map_id = 0;
  friend bool operator==(const SceneRoomAddress&,
                         const SceneRoomAddress&) = default;
};

[[nodiscard]] std::optional<SceneRoomAddress> CurrentSceneRoomAddress(
    std::span<const std::byte> scene);

[[nodiscard]] std::optional<std::uint8_t> SceneDoorId(
    std::span<const std::byte> scene, std::int32_t door_index);
bool MarkEntranceDoorOnSceneMap(std::span<std::byte> scene,
                                std::int32_t door_index);

// Validates an unresolved scene, rewrites its room payload lengths as
// record-relative offsets and returns the byte offset of the room names.
// Throws std::runtime_error for a malformed scene, leaving it unchanged.
std::size_t ResolveSceneRoomData(std::span<std::byte> scene);

// Draws a resolved scene; throws std::runtime_error when it is malformed.
void DrawMinimapScene(std::span<const std::byte> scene,
                      bool entry_door_pulse_visible, MinimapRenderer& renderer);

}  // namespace VS::Field
