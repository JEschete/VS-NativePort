// Hand-maintained (tools/port/owned.csv).
// Live binding for the MPD room geometry (RoomDraw.cpp).
#include "Render/RoomDrawLive.hpp"

#include <algorithm>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Core/Gte.h"
#include "Field/Room.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Render {
namespace {
namespace Math = VS::Core::Math;

constexpr const char* kChannel = "render";
// The frame primitive buffers are low-end allocations of this size
// (Field/RoomLoading.cpp, Ending/Credits/StaffRoll.cpp).
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
// One room draw writes at most this much of the frame buffer (2EA3C.s:10).
constexpr std::size_t kRoomBatchBytes = 0x10000;
static_assert(sizeof(u_long) == sizeof(std::uint32_t));
static_assert(sizeof(Core_GteWord) == sizeof(std::int32_t));

[[nodiscard]] MATRIX MatrixFrom(const RoomView& view) {
  MATRIX matrix{};
  Math::StoreMatrix(matrix, view.rotation);
  matrix.t[0] = view.translation.x;
  matrix.t[1] = view.translation.y;
  matrix.t[2] = view.translation.z;
  return matrix;
}

[[nodiscard]] SVECTOR VectorFrom(const RoomPosition& position) {
  return {position.x, position.y, position.z, 0};
}

class LiveRoomTransformer final : public RoomTransformer {
 public:
  void LoadView(const RoomView& view) override {
    const MATRIX matrix = MatrixFrom(view);
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
  }
  RoomProjectedPoint Project(const RoomPosition& position) override {
    SVECTOR input = VectorFrom(position);
    Core_GteWord screen = 0;
    Core_GteWord fog = 0;
    Core_GteWord flags = 0;
    // Native projection returns the quarter depth (SZ3 >> 2) directly.
    const Core_GteWord depth =
        Core_Gte_RotTransPers(&input, &screen, &fog, &flags);
    return {static_cast<std::int32_t>(screen),
            static_cast<std::int32_t>(depth)};
  }
  Math::Vec3 Transform(const RoomPosition& position) override {
    SVECTOR input = VectorFrom(position);
    VECTOR output{};
    Core_GteWord flags = 0;
    Core_Gte_RotTrans(&input, &output, &flags);
    return Math::VectorOf(output);
  }
  [[nodiscard]] RoomScreenOffset ScreenOffset() const override {
    Core_GteWord x = 0;
    Core_GteWord y = 0;
    Core_Gte_GetGeomOffset(&x, &y);
    return {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
  }
  void SetScreenOffset(const RoomScreenOffset& offset) override {
    Core_Gte_SetGeomOffset(offset.x, offset.y);
  }
};

[[nodiscard]] std::uintptr_t FrameBufferBase() {
  return reinterpret_cast<std::uintptr_t>(
      Render_legacyPrimitiveBuffers[Render_frameBuf & 1]);
}

// Packets linked into the frame's ordering table from the primitive cursor,
// at most `budget` bytes and never past the current frame buffer.
class LiveRoomPacketQueue final : public RoomPacketQueue {
 public:
  explicit LiveRoomPacketQueue(std::size_t budget) {
    const std::uintptr_t base = FrameBufferBase();
    begin_ =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
    if (base == 0 || begin_ < base || begin_ - base > kPrimitiveBufferBytes ||
        Render_frameScratch.orderingTable == nullptr) {
      return;
    }
    limit_ = begin_ + std::min(budget, kPrimitiveBufferBytes - (begin_ - base));
    ready_ = true;
  }

  [[nodiscard]] bool Ready() const { return ready_; }
  [[nodiscard]] std::uintptr_t Begin() const { return begin_; }
  [[nodiscard]] std::uintptr_t Limit() const { return limit_; }

  bool Emit(std::span<const std::uint32_t> packet,
            std::uint32_t depth) override {
    const auto cursor =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
    if (!ready_ || packet.empty() || cursor < begin_ || cursor > limit_ ||
        packet.size_bytes() > limit_ - cursor) {
      return false;
    }
    auto* slot = static_cast<u_long*>(Render_frameScratch.primitiveCursor);
    std::ranges::copy(packet, slot);
    setlen(slot, packet.size() - 1);
    AddPrim(static_cast<u_long*>(Render_frameScratch.orderingTable) + depth,
            slot);
    Render_frameScratch.primitiveCursor = slot + packet.size();
    return true;
  }

 private:
  std::uintptr_t begin_ = 0;
  std::uintptr_t limit_ = 0;
  bool ready_ = false;
};

[[nodiscard]] std::span<const std::byte> BytesOf(const void* data,
                                                 std::size_t bytes) {
  return data == nullptr
             ? std::span<const std::byte>{}
             : std::span(static_cast<const std::byte*>(data), bytes);
}

[[nodiscard]] Math::Angles3 AnglesFrom(const VECTOR& angles) {
  return Math::AnglesOf(angles);
}

[[nodiscard]] RoomPosition PositionFrom(const VECTOR& position) {
  return {static_cast<std::int16_t>(position.vx),
          static_cast<std::int16_t>(position.vy),
          static_cast<std::int16_t>(position.vz)};
}
}  // namespace

RoomClipState& LiveRoomClipState() {
  static RoomClipState clip;
  return clip;
}

RoomCamera LiveRoomCamera() {
  const MATRIX& view = Render_frameScratch.viewMatrix;
  RoomCamera camera;
  camera.view.rotation = Math::MatrixOf(view);
  camera.view.translation = {static_cast<std::int32_t>(view.t[0]),
                             static_cast<std::int32_t>(view.t[1]),
                             static_cast<std::int32_t>(view.t[2])};
  camera.scale = Render_frameScratch.camera.farClip;
  camera.roll = static_cast<std::int32_t>(Render_frameScratch.camera.angles.vz);
  return camera;
}

bool ValidateLiveRoomGeometry(const void* geometry, std::size_t bytes) {
  return geometry != nullptr && ValidateRoomGeometry(BytesOf(geometry, bytes));
}

bool UpdateLiveRoomGroupTransforms(void* geometry, std::size_t bytes) {
  if (geometry == nullptr ||
      !UpdateRoomGroupTransforms(
          std::span(static_cast<std::byte*>(geometry), bytes),
          LiveRoomCamera())) {
    Core_Debugf(kChannel, "Room group transforms refused (%zu bytes)", bytes);
    return false;
  }
  return true;
}

RoomDrawStatus DrawLiveRoomGeometry(const void* geometry, std::size_t bytes,
                                    int mode) {
  LiveRoomPacketQueue queue(kRoomBatchBytes);
  if (geometry == nullptr || !queue.Ready()) {
    return RoomDrawStatus::kDrawn;
  }
  const auto start = reinterpret_cast<std::uintptr_t>(geometry);
  if (start < queue.Limit() &&
      (start >= queue.Begin() || queue.Begin() - start < bytes)) {
    Core_Debugf(kChannel, "Room geometry overlaps the primitive buffer");
    return RoomDrawStatus::kInvalidGeometry;
  }
  LiveRoomTransformer transformer;
  const RoomDrawPass pass{mode, Render_frameScratch.firstPersonViewFacing};
  const RoomDrawStatus status = DrawRoomGeometry(
      BytesOf(geometry, bytes), pass, LiveRoomClipState(), transformer, queue);
  if (status == RoomDrawStatus::kInvalidGeometry) {
    Core_Debugf(kChannel, "Room geometry refused (%zu bytes)", bytes);
  }
  return status;
}

void BuildLiveRoomGeometryViewMatrix(const VECTOR& position,
                                     const VECTOR& angles, MATRIX& output) {
  output = MatrixFrom(BuildRoomGeometryViewMatrix(
      LiveRoomCamera(), PositionFrom(position), AnglesFrom(angles)));
}

void BuildLiveScaleCompensatedRoomView(const VECTOR& position,
                                       const VECTOR& angles, MATRIX& output) {
  output = MatrixFrom(BuildScaleCompensatedRoomView(
      LiveRoomCamera(), PositionFrom(position), AnglesFrom(angles)));
}

RoomDrawStatus DrawLiveSkyDome(const void* sky, std::size_t bytes) {
  LiveRoomPacketQueue queue(kPrimitiveBufferBytes);
  if (sky == nullptr || !queue.Ready()) {
    return RoomDrawStatus::kDrawn;
  }
  LiveRoomTransformer transformer;
  const RoomDrawStatus status =
      DrawSkyDome(BytesOf(sky, bytes), transformer, queue);
  if (status == RoomDrawStatus::kInvalidGeometry) {
    Core_Debugf(kChannel, "Sky dome refused (%zu bytes)", bytes);
  }
  return status;
}

bool TintLiveRoomGeometryPrimitives(void* start, void* end,
                                    const Field_RoomTint& tint, int flags) {
  if (start == end) {
    return true;
  }
  const auto first = reinterpret_cast<std::uintptr_t>(start);
  const auto last = reinterpret_cast<std::uintptr_t>(end);
  const std::uintptr_t base = FrameBufferBase();
  if (start == nullptr || end == nullptr || base == 0 || first > last ||
      (last - first) % sizeof(std::uint32_t) != 0 ||
      first % alignof(std::uint32_t) != 0 || first < base ||
      last - base > kPrimitiveBufferBytes) {
    Core_Debugf(kChannel, "Room tint range is outside the frame buffer");
    return false;
  }
  const std::span packets(static_cast<std::uint32_t*>(start),
                          (last - first) / sizeof(std::uint32_t));
  const RoomTint room_tint{tint.red, tint.green, tint.blue, tint.strength};
  if (TintRoomGeometryPrimitives(packets, room_tint,
                                 static_cast<std::uint32_t>(flags)) ==
      RoomTintStatus::kTruncated) {
    Core_Debugf(kChannel, "Room tint range ends inside a packet");
    return false;
  }
  return true;
}

Render_ScreenOffset GetLiveProjectionScreenOffset() {
  const LiveRoomTransformer transformer;
  const RoomScreenOffset offset = GetProjectionScreenOffset(transformer);
  return {offset.x, offset.y};
}

void SetLiveProjectionScreenOffset(const Render_ScreenOffset& offset) {
  LiveRoomTransformer transformer;
  SetProjectionScreenOffset(transformer, {offset.x, offset.y});
}
}  // namespace VS::Render
