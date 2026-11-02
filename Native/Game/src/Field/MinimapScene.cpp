// Hand-maintained (tools/port/owned.csv).
#include "Field/MinimapScene.hpp"

#include <climits>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Core/Log.hpp"

namespace VS::Field {
namespace {

constexpr std::size_t kWordBytes = sizeof(std::uint32_t);
constexpr std::size_t kVertexBytes = 8;
constexpr std::size_t kHalfBytes = sizeof(std::int16_t);
constexpr unsigned kByteBits = CHAR_BIT;
constexpr std::uint32_t kByteMask = std::numeric_limits<std::uint8_t>::max();
enum Table : std::size_t {
  kTriangles,
  kQuads,
  kBrightLines,
  kDimLines,
  kDoors,
  kTableCount
};
using IndexCounts = std::array<std::size_t, kTableCount>;
constexpr std::size_t kTri = kMinimapTriangleCorners;
constexpr std::size_t kQuad = kMinimapQuadCorners;
constexpr IndexCounts kIndexCounts{kTri, kQuad, 2, 2, 1};
constexpr std::size_t kQuadLastCorner = kMinimapQuadCorners - 1;
constexpr std::uint32_t kEntryDoorFlag = 0x10;
constexpr std::uint32_t kSpecialDoorFlag = 4;
constexpr std::uint32_t kDoorStateMask = 0x1F;
constexpr unsigned kDoorStateShift = 16;
constexpr unsigned kDoorGateShift = 24;
constexpr std::int16_t kLockedDoorLeft = 3;
constexpr std::int16_t kLockedDoorTop = 4;
constexpr std::uint8_t kHalfIntensity = 127;
constexpr std::uint8_t kFullIntensity = 255;
constexpr MinimapColor kFillColor{0, kHalfIntensity, 63};
constexpr MinimapColor kBrightLineColor{0, kHalfIntensity, kFullIntensity};
constexpr MinimapColor kDimLineColor{0, 95, 47};
constexpr std::uint8_t kGrey = kHalfIntensity;
constexpr MinimapColor kPlainDoorColor{kGrey, kGrey, kGrey};
constexpr MinimapColor kEntryDoorColor{kFullIntensity, 0, kHalfIntensity};
constexpr MinimapColor kSpecialDoorColor{0, kHalfIntensity, kFullIntensity};

using Bytes = std::span<const std::byte>;

std::uint32_t Word(Bytes bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    value |= std::to_integer<std::uint32_t>(bytes[offset + index])
             << (index * kByteBits);
  }
  return value;
}

std::int16_t Half(Bytes bytes, std::size_t offset) {
  const auto low = std::to_integer<std::uint16_t>(bytes[offset]);
  const auto high = std::to_integer<std::uint16_t>(bytes[offset + 1]);
  return static_cast<std::int16_t>(
      static_cast<std::uint16_t>(low | (high << kByteBits)));
}

bool Invalid(const char* reason, std::size_t offset) {
  VS_LOG_ERROR("minimap", "Invalid scene at byte {}: {}", offset, reason);
  return false;
}

struct Room {
  Bytes vertices;
  std::array<Bytes, kTableCount> tables;
};

bool ReadRoom(Bytes bytes, Room& room) {
  if (bytes.size() < kWordBytes) return Invalid("missing vertex count", 0);
  const std::size_t count = Word(bytes, 0);
  if (count > kMinimapMaximumVertices ||
      count > (bytes.size() - kWordBytes) / kVertexBytes) {
    return Invalid("vertex extent", 0);
  }
  room.vertices = bytes.subspan(kWordBytes, count * kVertexBytes);
  std::size_t cursor = kWordBytes + room.vertices.size();
  for (std::size_t table = 0; table < kTableCount; ++table) {
    if (bytes.size() - cursor < kWordBytes) {
      return Invalid("missing primitive count", cursor);
    }
    const std::size_t entries = Word(bytes, cursor);
    cursor += kWordBytes;
    if (entries > (bytes.size() - cursor) / kWordBytes) {
      return Invalid("primitive extent", cursor);
    }
    room.tables[table] = bytes.subspan(cursor, entries * kWordBytes);
    for (std::size_t item = 0; item < entries; ++item) {
      const std::uint32_t indices = Word(room.tables[table], item * kWordBytes);
      for (std::size_t corner = 0; corner < kIndexCounts[table]; ++corner) {
        if (((indices >> (corner * kByteBits)) & kByteMask) >= count) {
          return Invalid("vertex index", cursor + item * kWordBytes);
        }
      }
    }
    cursor += entries * kWordBytes;
  }
  return cursor == bytes.size() || Invalid("room payload length", cursor);
}

std::size_t RecordOffset(std::size_t index) {
  return kWordBytes + index * kMapSceneRoomRecordBytes;
}

bool ReadScene(Bytes bytes, bool resolved, std::vector<Room>& rooms,
               std::size_t& names) {
  if (bytes.size() < kWordBytes) return Invalid("missing room count", 0);
  const std::size_t count = Word(bytes, 0);
  if (count == 0 ||
      count > (bytes.size() - kWordBytes) / kMapSceneRoomRecordBytes) {
    return Invalid("room extent", 0);
  }
  std::size_t cursor = RecordOffset(count);
  rooms.resize(count);
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t record = RecordOffset(index);
    std::size_t length = Word(bytes, record + kWordBytes);
    if (resolved) {
      if (length != cursor - record) {
        return Invalid("resolved room offset", record + kWordBytes);
      }
      const std::size_t end =
          index + 1 < count
              ? RecordOffset(index + 1) +
                    Word(bytes, RecordOffset(index + 1) + kWordBytes)
              : bytes.size();
      if (end < cursor || end > bytes.size()) {
        return Invalid("resolved room extent", record + kWordBytes);
      }
      length = end - cursor;
    }
    if (length > bytes.size() - cursor ||
        !ReadRoom(bytes.subspan(cursor, length), rooms[index])) {
      return Invalid("room payload", cursor);
    }
    cursor += length;
  }
  names = cursor;
  const std::size_t remaining = bytes.size() - cursor;
  if (remaining != 0 && count > remaining / kMapSceneRoomNameBytes) {
    return Invalid("room-name extent", cursor);
  }
  return true;
}

bool Visited(Bytes bytes, std::size_t index) {
  return Word(bytes, RecordOffset(index)) != 0;
}

std::size_t Entries(Bytes table) { return table.size() / kWordBytes; }

MinimapPrimitiveCounts CountPrimitives(Bytes bytes,
                                       const std::vector<Room>& rooms) {
  MinimapPrimitiveCounts counts;
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    if (!Visited(bytes, index)) continue;
    const auto& tables = rooms[index].tables;
    if (index == 0) {
      counts.triangles += Entries(tables[kTriangles]);
      counts.quads += Entries(tables[kQuads]);
      counts.lines += Entries(tables[kDimLines]);
      counts.doors += Entries(tables[kDoors]);
    }
    counts.lines += Entries(tables[kBrightLines]);
  }
  return counts;
}

void DrawDoor(std::uint32_t entry, MinimapScreenPoint position,
              bool entry_door_pulse_visible, MinimapRenderer& renderer) {
  const auto gate = static_cast<std::uint8_t>(entry >> kDoorGateShift);
  if (gate != 0 && renderer.IsDoorGateLocked(gate)) {
    renderer.DrawLockedDoor(
        {static_cast<std::int16_t>(position.x - kLockedDoorLeft),
         static_cast<std::int16_t>(position.y - kLockedDoorTop)});
    return;
  }
  const std::uint32_t state = (entry >> kDoorStateShift) & kByteMask;
  MinimapColor color = kPlainDoorColor;
  if ((state & kEntryDoorFlag) != 0) {
    if (!entry_door_pulse_visible) return;
    color = kEntryDoorColor;
  } else if ((state & kSpecialDoorFlag) != 0) {
    color = kSpecialDoorColor;
  } else if ((state & kDoorStateMask) != 0) {
    return;
  }
  renderer.DrawDoorCross(position, color);
}

void DrawRoom(const Room& room, bool current, bool entry_door_pulse_visible,
              MinimapRenderer& renderer) {
  std::array<MinimapScreenPoint, kMinimapMaximumVertices> points{};
  const std::size_t count = room.vertices.size() / kVertexBytes;
  for (std::size_t vertex = 0; vertex < count; ++vertex) {
    const std::size_t offset = vertex * kVertexBytes;
    points[vertex] = renderer.Project(
        {Half(room.vertices, offset), Half(room.vertices, offset + kHalfBytes),
         Half(room.vertices, offset + 2 * kHalfBytes)});
  }
  for (std::size_t table = 0; table < kTableCount; ++table) {
    if (!current && table != kBrightLines) continue;
    for (std::size_t item = 0; item < room.tables[table].size();
         item += kWordBytes) {
      const std::uint32_t entry = Word(room.tables[table], item);
      MinimapQuad xy{};
      for (std::size_t corner = 0; corner < kIndexCounts[table]; ++corner) {
        xy[corner] = points[(entry >> (corner * kByteBits)) & kByteMask];
      }
      switch (table) {
        case kTriangles:
          renderer.DrawTriangle({xy[0], xy[1], xy[2]}, kFillColor);
          break;
        case kQuads:
          renderer.DrawQuad({xy[0], xy[1], xy[kQuadLastCorner], xy[2]},
                            kFillColor);
          break;
        case kBrightLines:
          renderer.DrawLine(xy[0], xy[1], kBrightLineColor);
          break;
        case kDimLines:
          renderer.DrawLine(xy[0], xy[1], kDimLineColor);
          break;
        default:
          DrawDoor(entry, xy[0], entry_door_pulse_visible, renderer);
          break;
      }
    }
  }
}

}  // namespace

bool IsValidMapScene(std::span<const std::byte> scene) {
  std::vector<Room> rooms;
  std::size_t names = 0;
  return ReadScene(scene, false, rooms, names);
}

// decomp: BATTLE.PRG vs_battle_initSceneAndGetRoomNames
// (src/BATTLE/BATTLE.PRG/2EA3C.s:2085-2103).
std::size_t ResolveSceneRoomData(std::span<std::byte> scene) {
  std::vector<Room> rooms;
  std::size_t names = 0;
  if (!ReadScene(scene, false, rooms, names)) {
    throw std::runtime_error("Invalid map scene");
  }
  const Bytes read_only = scene;
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    const std::size_t record = RecordOffset(index);
    const auto payload = static_cast<std::size_t>(
        rooms[index].vertices.data() - kWordBytes - read_only.data());
    const auto offset = static_cast<std::uint32_t>(payload - record);
    for (std::size_t byte = 0; byte < kWordBytes; ++byte) {
      scene[record + kWordBytes + byte] =
          static_cast<std::byte>(offset >> (byte * kByteBits));
    }
  }
  return names;
}

// decomp: BATTLE.PRG func_80098F8C (src/BATTLE/BATTLE.PRG/2EA3C.s:2109-2467).
void DrawMinimapScene(std::span<const std::byte> scene,
                      bool entry_door_pulse_visible,
                      MinimapRenderer& renderer) {
  std::vector<Room> rooms;
  std::size_t names = 0;
  if (!ReadScene(scene, true, rooms, names)) {
    throw std::runtime_error("Invalid resolved minimap scene");
  }
  renderer.Begin(CountPrimitives(scene, rooms));
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    if (!Visited(scene, index)) continue;
    DrawRoom(rooms[index], index == 0, entry_door_pulse_visible, renderer);
  }
  renderer.Finish();
}

}  // namespace VS::Field
