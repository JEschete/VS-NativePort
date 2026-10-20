// Hand-maintained (tools/port/owned.csv).
#include "Field/MinimapScene.h"

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

#include "Core/Log.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Gimmicks.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"
}

namespace {
constexpr size_t kWordBytes = sizeof(uint32_t);
constexpr size_t kRoomBytes = 12;
enum Table : size_t {
  kTriangles,
  kQuads,
  kBrightLines,
  kDimLines,
  kDoors,
  kTableCount
};
constexpr size_t kMaximumVertices = 256;
constexpr size_t kPrimitiveBufferBytes = 0x20000;
constexpr size_t kGeometrySlot = 5;
constexpr size_t kMarkerSlot = 4;
constexpr uint32_t kEntryDoorFlag = 0x10;
constexpr uint32_t kSpecialDoorFlag = 4;
constexpr uint32_t kDoorStateMask = 0x1F;
constexpr unsigned kDoorStateShift = 16;
constexpr unsigned kDoorGateShift = 24;
constexpr size_t kDoorStateBase = offsetof(State_StateFlags, opaque33F);
constexpr int kMinimapDepth = 256;
constexpr int kDoorPulseMask = 0x20;
constexpr int kMarkerClut = 0x37F8;
constexpr int kMarkerU = 216;
constexpr int kMarkerV = 152;
constexpr int kMarkerLeft = 3;
constexpr int kMarkerTop = 4;
constexpr int kDoorCrossLength = 3;
constexpr int kHalfIntensity = 127;
constexpr int kFullIntensity = 255;
constexpr int kDimGreen = 95;
constexpr int kDimBlue = 47;
constexpr int kFillBlue = 63;
constexpr int kMinimapTextureX = 768;
constexpr size_t kQuadCorners = 4;
constexpr std::array<size_t, kTableCount> kIndexCounts{3, 4, 2, 2, 1};
static_assert(sizeof(Field_MapRoom) == kRoomBytes);
static_assert(kDoorStateBase + UINT8_MAX < sizeof(State_StateFlags));

using Bytes = std::span<const std::byte>;

uint32_t Word(Bytes bytes, size_t offset) {
  uint32_t value;
  std::memcpy(&value, bytes.data() + offset, sizeof value);
  return value;
}

bool Invalid(const char* reason, size_t offset) {
  VS_LOG_ERROR("minimap", "Invalid scene at byte {}: {}", offset, reason);
  return false;
}

struct Room {
  Bytes vertices;
  std::array<Bytes, kTableCount> tables;
};

bool ReadRoom(Bytes bytes, Room& room) {
  if (bytes.size() < kWordBytes) return Invalid("missing vertex count", 0);
  const size_t count = Word(bytes, 0);
  if (count > kMaximumVertices ||
      count > (bytes.size() - kWordBytes) / sizeof(SVECTOR)) {
    return Invalid("vertex extent", 0);
  }
  const size_t vertex_bytes = count * sizeof(SVECTOR);
  room.vertices = bytes.subspan(kWordBytes, vertex_bytes);
  size_t cursor = kWordBytes + vertex_bytes;
  for (size_t table = 0; table < kTableCount; ++table) {
    if (bytes.size() - cursor < kWordBytes) {
      return Invalid("missing primitive count", cursor);
    }
    const size_t entries = Word(bytes, cursor);
    cursor += kWordBytes;
    if (entries > (bytes.size() - cursor) / kWordBytes) {
      return Invalid("primitive extent", cursor);
    }
    room.tables[table] = bytes.subspan(cursor, entries * kWordBytes);
    for (size_t item = 0; item < entries; ++item) {
      const uint32_t indices = Word(room.tables[table], item * kWordBytes);
      for (size_t corner = 0; corner < kIndexCounts[table]; ++corner) {
        if (((indices >> (corner * CHAR_BIT)) & UINT8_MAX) >= count) {
          return Invalid("vertex index", cursor + item * kWordBytes);
        }
      }
    }
    cursor += entries * kWordBytes;
  }
  return cursor == bytes.size() || Invalid("room payload length", cursor);
}

bool ReadScene(Bytes bytes, bool resolved, std::vector<Room>& rooms,
               size_t& names) {
  if (bytes.size() < kWordBytes) return Invalid("missing room count", 0);
  const size_t count = Word(bytes, 0);
  if (count == 0 || count > (bytes.size() - kWordBytes) / kRoomBytes) {
    return Invalid("room extent", 0);
  }
  size_t cursor = kWordBytes + count * kRoomBytes;
  rooms.resize(count);
  for (size_t index = 0; index < count; ++index) {
    const size_t record = kWordBytes + index * kRoomBytes;
    size_t length = Word(bytes, record + kWordBytes);
    if (resolved) {
      if (length != cursor - record) {
        return Invalid("resolved room offset", record + kWordBytes);
      }
      const size_t end = index + 1 < count
                             ? kWordBytes + (index + 1) * kRoomBytes +
                                   Word(bytes, record + kRoomBytes + kWordBytes)
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
  const size_t remaining = bytes.size() - cursor;
  if (remaining != 0 && count > remaining / sizeof(Field_MapRoomName)) {
    return Invalid("room-name extent", cursor);
  }
  return true;
}

class Packets {
 public:
  explicit Packets(size_t required) {
    const auto base = reinterpret_cast<uintptr_t>(
        Render_legacyPrimitiveBuffers[Render_frameBuf]);
    const auto cursor =
        reinterpret_cast<uintptr_t>(Render_frameScratch.primitiveCursor);
    if (Render_frameScratch.orderingTable == nullptr || base == 0 ||
        cursor < base || cursor - base > kPrimitiveBufferBytes ||
        required > kPrimitiveBufferBytes - (cursor - base)) {
      VS_LOG_ERROR("minimap", "Primitive buffer cannot hold {} bytes",
                   required);
      throw std::runtime_error("Minimap primitive capacity exceeded");
    }
    cursor_ = reinterpret_cast<std::byte*>(cursor);
  }

  template <typename T>
  T* Next() {
    auto* packet = reinterpret_cast<T*>(cursor_);
    std::memset(packet, 0, sizeof(T));
    cursor_ += sizeof(T);
    return packet;
  }

  void Finish() { Render_frameScratch.primitiveCursor = cursor_; }

 private:
  std::byte* cursor_;
};

void Link(size_t slot, void* packet) {
  AddPrim(static_cast<u_long*>(Render_frameScratch.orderingTable) + slot,
          packet);
}

void SetPoint(int16_t& x, int16_t& y, Core_GteWord packed) {
  x = static_cast<int16_t>(packed & UINT16_MAX);
  y = static_cast<int16_t>(static_cast<uint32_t>(packed) >> kDoorStateShift);
}

void DoorMarker(uint32_t entry, Core_GteWord xy, Packets& packets) {
  const unsigned gate = entry >> kDoorGateShift;
  const auto* flags =
      reinterpret_cast<const unsigned char*>(&Audio_Seq_unusedStateFlags);
  if (gate != 0 && flags[kDoorStateBase + gate] == 0) {
    auto* sprite = packets.Next<SPRT_8>();
    setSprt8(sprite);
    setRGB0(sprite, 128, 128, 128);
    SetPoint(sprite->x0, sprite->y0, xy);
    sprite->x0 -= kMarkerLeft;
    sprite->y0 -= kMarkerTop;
    setUV0(sprite, kMarkerU, kMarkerV);
    sprite->clut = kMarkerClut;
    Link(kMarkerSlot, sprite);
    return;
  }
  const uint32_t state = (entry >> kDoorStateShift) & UINT8_MAX;
  int red = kHalfIntensity;
  int green = kHalfIntensity;
  int blue = kHalfIntensity;
  if (state & kEntryDoorFlag) {
    if (!(Field_roomAnimationTicks & kDoorPulseMask)) return;
    red = kFullIntensity;
    green = 0;
  } else if (state & kSpecialDoorFlag) {
    red = 0;
    blue = kFullIntensity;
  } else if (state & kDoorStateMask) {
    return;
  }
  for (int axis = 0; axis < 2; ++axis) {
    auto* tile = packets.Next<TILE>();
    setTile(tile);
    setRGB0(tile, red, green, blue);
    SetPoint(tile->x0, tile->y0, xy);
    if (axis == 0) {
      --tile->y0;
      setWH(tile, 1, kDoorCrossLength);
    } else {
      --tile->x0;
      setWH(tile, kDoorCrossLength, 1);
    }
    Link(kMarkerSlot, tile);
  }
}

size_t RequiredBytes(Bytes bytes, const std::vector<Room>& rooms) {
  size_t required = 2 * sizeof(DR_MODE);
  for (size_t index = 0; index < rooms.size(); ++index) {
    if (Word(bytes, kWordBytes + index * kRoomBytes) == 0) continue;
    const auto& tables = rooms[index].tables;
    if (index == 0) {
      required += tables[kTriangles].size() / kWordBytes * sizeof(POLY_F3) +
                  tables[kQuads].size() / kWordBytes * sizeof(POLY_F4) +
                  tables[kDimLines].size() / kWordBytes * sizeof(LINE_F2) +
                  tables[kDoors].size() / kWordBytes * 2 * sizeof(TILE);
    }
    required += tables[kBrightLines].size() / kWordBytes * sizeof(LINE_F2);
  }
  return required;
}

void Shape(size_t table, const std::array<Core_GteWord, kQuadCorners>& xy,
           Packets& packets) {
  if (table == kTriangles) {
    auto* poly = packets.Next<POLY_F3>();
    setPolyF3(poly);
    setSemiTrans(poly, 1);
    setRGB0(poly, 0, kHalfIntensity, kFillBlue);
    SetPoint(poly->x0, poly->y0, xy[0]);
    SetPoint(poly->x1, poly->y1, xy[1]);
    SetPoint(poly->x2, poly->y2, xy[2]);
    Link(kGeometrySlot, poly);
  } else if (table == kQuads) {
    auto* poly = packets.Next<POLY_F4>();
    setPolyF4(poly);
    setSemiTrans(poly, 1);
    setRGB0(poly, 0, kHalfIntensity, kFillBlue);
    SetPoint(poly->x0, poly->y0, xy[0]);
    SetPoint(poly->x1, poly->y1, xy[1]);
    SetPoint(poly->x2, poly->y2, xy[kQuadCorners - 1]);
    SetPoint(poly->x3, poly->y3, xy[2]);
    Link(kGeometrySlot, poly);
  } else {
    auto* line = packets.Next<LINE_F2>();
    setLineF2(line);
    setSemiTrans(line, 1);
    setRGB0(line, 0, table == kBrightLines ? kHalfIntensity : kDimGreen,
            table == kBrightLines ? kFullIntensity : kDimBlue);
    SetPoint(line->x0, line->y0, xy[0]);
    SetPoint(line->x1, line->y1, xy[1]);
    Link(kGeometrySlot, line);
  }
}

void DrawRoom(const Room& room, bool current, Packets& packets) {
  std::array<Core_GteWord, kMaximumVertices> points{};
  const size_t count = room.vertices.size() / sizeof(SVECTOR);
  for (size_t vertex = 0; vertex < count; ++vertex) {
    SVECTOR point;
    std::memcpy(&point, room.vertices.data() + vertex * sizeof(point),
                sizeof point);
    Core_GteWord depth;
    Core_GteWord flags;
    Core_Gte_RotTransPers(&point, &points[vertex], &depth, &flags);
  }
  for (size_t table = 0; table < kTableCount; ++table) {
    if (!current && table != kBrightLines) continue;
    for (size_t item = 0; item < room.tables[table].size();
         item += kWordBytes) {
      const uint32_t entry = Word(room.tables[table], item);
      std::array<Core_GteWord, kQuadCorners> xy{};
      for (size_t corner = 0; corner < kIndexCounts[table]; ++corner) {
        xy[corner] = points[(entry >> (corner * CHAR_BIT)) & UINT8_MAX];
      }
      if (table == kDoors) {
        DoorMarker(entry, xy[0], packets);
      } else {
        Shape(table, xy, packets);
      }
    }
  }
}
}  // namespace

int Field_ValidateMapScene(const void* data, size_t size) {
  if (data == nullptr) return Invalid("null scene", 0);
  std::vector<Room> rooms;
  size_t names = 0;
  return ReadScene({static_cast<const std::byte*>(data), size}, false, rooms,
                   names);
}

// decomp: vs_battle_initSceneAndGetRoomNames, 2EA3C.s:2085-2103.
Field_MapRoomName* Field_ResolveSceneRoomData(void* data,
                                              size_t size) noexcept(false) {
  if (data == nullptr) {
    Invalid("null scene", 0);
    throw std::runtime_error("Missing map scene");
  }
  auto* bytes = static_cast<std::byte*>(data);
  std::vector<Room> rooms;
  size_t names = 0;
  if (!ReadScene({bytes, size}, false, rooms, names)) {
    throw std::runtime_error("Invalid map scene");
  }
  for (size_t index = 0; index < rooms.size(); ++index) {
    const size_t record = kWordBytes + index * kRoomBytes;
    const auto offset = static_cast<uint32_t>(rooms[index].vertices.data() -
                                              kWordBytes - (bytes + record));
    std::memcpy(bytes + record + kWordBytes, &offset, sizeof offset);
  }
  return reinterpret_cast<Field_MapRoomName*>(bytes + names);
}

// decomp: BATTLE.PRG func_80098F8C, 2EA3C.s:2109-2467.
void Field_DrawMinimapScene(void* data, size_t size) noexcept(false) {
  if (data == nullptr) {
    Invalid("null scene", 0);
    throw std::runtime_error("Missing minimap scene");
  }
  const Bytes bytes{static_cast<const std::byte*>(data), size};
  std::vector<Room> rooms;
  size_t names = 0;
  if (!ReadScene(bytes, true, rooms, names)) {
    throw std::runtime_error("Invalid resolved minimap scene");
  }
  Packets packets(RequiredBytes(bytes, rooms));
  const VECTOR translation{0, 0, kMinimapDepth, 0};
  Core_Gte_SetTransVector(&translation);
  for (size_t index = 0; index < rooms.size(); ++index) {
    if (Word(bytes, kWordBytes + index * kRoomBytes) == 0) continue;
    DrawRoom(rooms[index], index == 0, packets);
  }
  for (size_t slot : {kGeometrySlot, kMarkerSlot}) {
    auto* mode = packets.Next<DR_MODE>();
    SetDrawMode(mode, 0, 1, getTPage(0, 0, kMinimapTextureX, 0), nullptr);
    Link(slot, mode);
  }
  packets.Finish();
}
