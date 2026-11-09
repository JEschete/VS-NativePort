// Hand-maintained (tools/port/owned.csv).
#include "Field/MinimapSceneLive.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>

#include "Core/Log.hpp"
#include "Field/MinimapScene.hpp"
#include "Field/MinimapSceneLive.h"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Room.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"
}

namespace VS::Field {
namespace {

constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
constexpr std::size_t kGeometrySlot = 5;
constexpr std::size_t kMarkerSlot = 4;
constexpr std::size_t kDoorStateBase = offsetof(State_StateFlags, opaque33F);
constexpr int kMinimapDepth = 256;
constexpr int kDoorPulseMask = 0x20;
constexpr int kMarkerClut = 0x37F8;
constexpr int kMarkerU = 216;
constexpr int kMarkerV = 152;
constexpr int kMarkerIntensity = 128;
constexpr int kDoorCrossLength = 3;
constexpr int kMinimapTextureX = 768;
constexpr unsigned kScreenYShift = 16;
static_assert(sizeof(Field_MapRoom) == kMapSceneRoomRecordBytes);
static_assert(sizeof(Field_MapRoomName) == kMapSceneRoomNameBytes);
static_assert(kDoorStateBase + UINT8_MAX < sizeof(State_StateFlags));

class Packets {
 public:
  explicit Packets(std::size_t required) {
    const auto base = reinterpret_cast<std::uintptr_t>(
        Render_legacyPrimitiveBuffers[Render_frameBuf]);
    const auto cursor =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
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
  std::byte* cursor_ = nullptr;
};

void Link(std::size_t slot, void* packet) {
  AddPrim(static_cast<u_long*>(Render_frameScratch.orderingTable) + slot,
          packet);
}

void SetPoint(std::int16_t& x, std::int16_t& y, MinimapScreenPoint point) {
  x = point.x;
  y = point.y;
}

class LiveMinimapRenderer final : public MinimapRenderer {
 public:
  void Begin(const MinimapPrimitiveCounts& counts) override {
    packets_.emplace(2 * sizeof(DR_MODE) + counts.triangles * sizeof(POLY_F3) +
                     counts.quads * sizeof(POLY_F4) +
                     counts.lines * sizeof(LINE_F2) +
                     counts.doors * 2 * sizeof(TILE));
    const VECTOR translation{0, 0, kMinimapDepth, 0};
    Core_Gte_SetTransVector(&translation);
  }

  MinimapScreenPoint Project(const MinimapVertex& vertex) override {
    SVECTOR point{vertex.x, vertex.y, vertex.z, 0};
    Core_GteWord xy = 0;
    Core_GteWord depth = 0;
    Core_GteWord flags = 0;
    Core_Gte_RotTransPers(&point, &xy, &depth, &flags);
    const auto packed = static_cast<std::uint32_t>(xy);
    return {static_cast<std::int16_t>(packed & UINT16_MAX),
            static_cast<std::int16_t>(packed >> kScreenYShift)};
  }

  bool IsDoorGateLocked(std::uint8_t gate) override {
    const auto* flags =
        reinterpret_cast<const unsigned char*>(&Audio_Seq_unusedStateFlags);
    return flags[kDoorStateBase + gate] == 0;
  }

  void DrawTriangle(const MinimapTriangle& corners,
                    MinimapColor color) override {
    auto* poly = Frame().Next<POLY_F3>();
    setPolyF3(poly);
    setSemiTrans(poly, 1);
    setRGB0(poly, color.red, color.green, color.blue);
    SetPoint(poly->x0, poly->y0, corners[0]);
    SetPoint(poly->x1, poly->y1, corners[1]);
    SetPoint(poly->x2, poly->y2, corners[2]);
    Link(kGeometrySlot, poly);
  }

  void DrawQuad(const MinimapQuad& corners, MinimapColor color) override {
    auto* poly = Frame().Next<POLY_F4>();
    setPolyF4(poly);
    setSemiTrans(poly, 1);
    setRGB0(poly, color.red, color.green, color.blue);
    SetPoint(poly->x0, poly->y0, corners[0]);
    SetPoint(poly->x1, poly->y1, corners[1]);
    SetPoint(poly->x2, poly->y2, corners[2]);
    SetPoint(poly->x3, poly->y3, corners[kMinimapQuadCorners - 1]);
    Link(kGeometrySlot, poly);
  }

  void DrawLine(MinimapScreenPoint from, MinimapScreenPoint to,
                MinimapColor color) override {
    auto* line = Frame().Next<LINE_F2>();
    setLineF2(line);
    setSemiTrans(line, 1);
    setRGB0(line, color.red, color.green, color.blue);
    SetPoint(line->x0, line->y0, from);
    SetPoint(line->x1, line->y1, to);
    Link(kGeometrySlot, line);
  }

  void DrawLockedDoor(MinimapScreenPoint position) override {
    auto* sprite = Frame().Next<SPRT_8>();
    setSprt8(sprite);
    setRGB0(sprite, kMarkerIntensity, kMarkerIntensity, kMarkerIntensity);
    SetPoint(sprite->x0, sprite->y0, position);
    setUV0(sprite, kMarkerU, kMarkerV);
    sprite->clut = kMarkerClut;
    Link(kMarkerSlot, sprite);
  }

  void DrawDoorCross(MinimapScreenPoint position, MinimapColor color) override {
    for (int axis = 0; axis < 2; ++axis) {
      auto* tile = Frame().Next<TILE>();
      setTile(tile);
      setRGB0(tile, color.red, color.green, color.blue);
      SetPoint(tile->x0, tile->y0, position);
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

  void Finish() override {
    for (std::size_t slot : {kGeometrySlot, kMarkerSlot}) {
      auto* mode = Frame().Next<DR_MODE>();
      SetDrawMode(mode, 0, 1, getTPage(0, 0, kMinimapTextureX, 0), nullptr);
      Link(slot, mode);
    }
    Frame().Finish();
  }

 private:
  Packets& Frame() {
    if (!packets_.has_value()) {
      throw std::logic_error("Minimap renderer used before Begin");
    }
    return *packets_;
  }

  std::optional<Packets> packets_;
};

}  // namespace

Field_MapRoomName* ResolveLiveSceneRoomData(void* data, std::size_t size) {
  if (data == nullptr) {
    VS_LOG_ERROR("minimap", "Invalid scene at byte 0: null scene");
    throw std::runtime_error("Missing map scene");
  }
  auto* bytes = static_cast<std::byte*>(data);
  const std::size_t names = ResolveSceneRoomData({bytes, size});
  return reinterpret_cast<Field_MapRoomName*>(bytes + names);
}

void DrawLiveMinimapScene(const void* data, std::size_t size) {
  if (data == nullptr) {
    VS_LOG_ERROR("minimap", "Invalid scene at byte 0: null scene");
    throw std::runtime_error("Missing minimap scene");
  }
  LiveMinimapRenderer renderer;
  DrawMinimapScene({static_cast<const std::byte*>(data), size},
                   (Field_roomAnimationTicks & kDoorPulseMask) != 0, renderer);
}

}  // namespace VS::Field

extern "C" int Field_SceneDoorIdEntry(int door_index) {
  if (Field_roomData.minimapScene == nullptr) return -1;
  const auto* data =
      reinterpret_cast<const std::byte*>(Field_roomData.minimapScene);
  const auto id = VS::Field::SceneDoorId(
      {data, Field_roomData.header.minimapSceneLength}, door_index);
  return id ? static_cast<int>(*id) : -1;
}

extern "C" void Field_MarkEntranceDoorOnSceneMapEntry(int door_index) {
  Field_enteredDoorId =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(door_index));
  if (Field_roomData.minimapScene == nullptr) return;
  auto* data = reinterpret_cast<std::byte*>(Field_roomData.minimapScene);
  static_cast<void>(VS::Field::MarkEntranceDoorOnSceneMap(
      {data, Field_roomData.header.minimapSceneLength}, door_index));
}
