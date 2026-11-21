// Hand-maintained (tools/port/owned.csv).
// Live binding for the MPD room geometry (RoomDraw.cpp).
#include "Render/RoomDrawLive.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Diagnostics.hpp"
#include "Core/Gte.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/Display.hpp"
#include "Render/DrawList.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::VECTOR;

namespace VS::Render {
namespace {
namespace Math = VS::Core::Math;

constexpr const char* kChannel = "render";
// The frame primitive buffers are low-end allocations of this size
// (Field/RoomLoading.cpp, Ending/Credits/StaffRoll.cpp).
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
// One room draw writes at most this much of the frame buffer (2EA3C.s:10).
constexpr std::size_t kRoomBatchBytes = 0x10000;
static_assert(sizeof(VS::Core::Gte::Word) == sizeof(std::int32_t));

[[nodiscard]] MATRIX MatrixFrom(const RoomView& view) {
  MATRIX matrix{};
  Math::StoreMatrix(matrix, view.rotation);
  matrix.t[0] = view.translation.x;
  matrix.t[1] = view.translation.y;
  matrix.t[2] = view.translation.z;
  return matrix;
}

// A transformer over the projection the frame's producers left in the
// shared GTE state; its view and results stay private to the draw.
[[nodiscard]] PerspectiveRoomTransformer FrameTransformer() {
  return PerspectiveRoomTransformer(VS::Core::Gte::SharedProjection());
}

[[nodiscard]] RoomView ViewFrom(const MATRIX& matrix) {
  return {Math::MatrixOf(matrix),
          {static_cast<std::int32_t>(matrix.t[0]),
           static_cast<std::int32_t>(matrix.t[1]),
           static_cast<std::int32_t>(matrix.t[2])}};
}

[[nodiscard]] std::uintptr_t FrameBufferBase() {
  return reinterpret_cast<std::uintptr_t>(legacyPrimitiveBuffers[frameBuf & 1]);
}

// Room polygons submitted to the frame's draw list at the scene table slot
// the frame's ordering pointer names. The packets the original linked from
// the primitive cursor are not built; the cursor still advances by their
// size, at most `budget` bytes and never past the current frame buffer, so
// the producers after the room draw keep the buffer space, and the refusals
// when it runs out, that they had beside the room's packets.
class LiveRoomDrawQueue final : public RoomDrawQueue {
 public:
  explicit LiveRoomDrawQueue(std::size_t budget) {
    const std::uintptr_t base = FrameBufferBase();
    begin_ =
        reinterpret_cast<std::uintptr_t>(FrameScratchpad().primitive_cursor);
    const std::optional<std::size_t> first_slot = GameFrameDrawList().SlotOf(
        static_cast<const std::uint32_t*>(FrameScratchpad().ordering_table));
    if (base == 0 || begin_ < base || begin_ - base > kPrimitiveBufferBytes ||
        !first_slot.has_value()) {
      return;
    }
    first_slot_ = *first_slot;
    limit_ = begin_ + std::min(budget, kPrimitiveBufferBytes - (begin_ - base));
    ready_ = true;
  }

  [[nodiscard]] bool Ready() const { return ready_; }

  bool Submit(const DrawPolygon& polygon, std::uint32_t slot) override {
    const auto cursor =
        reinterpret_cast<std::uintptr_t>(FrameScratchpad().primitive_cursor);
    const std::size_t bytes =
        RoomPacketEquivalentWords(polygon) * sizeof(std::uint32_t);
    if (!ready_ || cursor < begin_ || cursor > limit_ ||
        bytes > limit_ - cursor ||
        !GameFrameDrawList().Submit(polygon, first_slot_ + slot)) {
      return false;
    }
    FrameScratchpad().primitive_cursor =
        static_cast<std::byte*>(FrameScratchpad().primitive_cursor) + bytes;
    return true;
  }

 private:
  std::uintptr_t begin_ = 0;
  std::uintptr_t limit_ = 0;
  std::size_t first_slot_ = 0;
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
  const MATRIX& view = FrameScratchpad().view_matrix;
  RoomCamera camera;
  camera.view.rotation = Math::MatrixOf(view);
  camera.view.translation = {static_cast<std::int32_t>(view.t[0]),
                             static_cast<std::int32_t>(view.t[1]),
                             static_cast<std::int32_t>(view.t[2])};
  camera.scale = FrameScratchpad().camera.far_clip;
  camera.roll = static_cast<std::int32_t>(FrameScratchpad().camera.angles.vz);
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
    VS::Core::Debugf(kChannel, "Room group transforms refused (%zu bytes)",
                     bytes);
    return false;
  }
  return true;
}

RoomDrawStatus DrawLiveRoomGeometry(const void* geometry, std::size_t bytes,
                                    int mode) {
  LiveRoomDrawQueue queue(kRoomBatchBytes);
  if (geometry == nullptr) {
    return RoomDrawStatus::kDrawn;
  }
  if (!queue.Ready()) {
    VS::Core::Debugf(kChannel,
                     "Room geometry skipped: no frame buffer or open scene "
                     "draw list");
    return RoomDrawStatus::kDrawn;
  }
  PerspectiveRoomTransformer transformer = FrameTransformer();
  const RoomDrawPass pass{mode, FrameScratchpad().first_person_view_facing};
  const RoomDrawStatus status = DrawRoomGeometry(
      BytesOf(geometry, bytes), pass, LiveRoomClipState(), transformer, queue);
  if (status == RoomDrawStatus::kInvalidGeometry) {
    VS::Core::Debugf(kChannel, "Room geometry refused (%zu bytes)", bytes);
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

RoomDrawStatus DrawLiveSkyDome(const void* sky, std::size_t bytes,
                               const MATRIX& view) {
  LiveRoomDrawQueue queue(kPrimitiveBufferBytes);
  if (sky == nullptr) {
    return RoomDrawStatus::kDrawn;
  }
  if (!queue.Ready()) {
    VS::Core::Debugf(kChannel,
                     "Sky dome skipped: no frame buffer or open scene draw "
                     "list");
    return RoomDrawStatus::kDrawn;
  }
  PerspectiveRoomTransformer transformer = FrameTransformer();
  const RoomDrawStatus status =
      DrawSkyDome(BytesOf(sky, bytes), ViewFrom(view), transformer, queue);
  if (status == RoomDrawStatus::kInvalidGeometry) {
    VS::Core::Debugf(kChannel, "Sky dome refused (%zu bytes)", bytes);
  }
  return status;
}

// decomp: BATTLE.PRG func_80098D6C (src/BATTLE/BATTLE.PRG/2EA3C.s:1837),
// which walked the packets between two primitive-cursor marks; the room's
// items are the draw-list indices [first, end) instead.
bool TintLiveRoomGeometry(std::size_t first, std::size_t end,
                          const RoomTint& tint) {
  FrameDrawList& items = GameFrameDrawList();
  if (first == end) {
    return true;
  }
  if (!items.IsOpen() || first > end || end > items.ItemCount()) {
    VS::Core::Debugf(kChannel,
                     "Room tint range [%zu, %zu) is outside the frame's %zu "
                     "draw items",
                     first, end, items.ItemCount());
    return false;
  }
  const RoomColorAdjustment adjustment = RoomTintAdjustment(tint);
  items.ForEachItem(first, end,
                    [&adjustment](DrawPolygon& polygon, std::size_t /*slot*/) {
                      TintRoomPolygon(polygon, adjustment);
                    });
  return true;
}

VS::Render::ScreenOffset GetLiveProjectionScreenOffset() {
  const PerspectiveRoomTransformer transformer = FrameTransformer();
  const RoomScreenOffset offset = GetProjectionScreenOffset(transformer);
  return {offset.x, offset.y};
}

// The shared offset stays the producer every other projection reads.
void SetLiveProjectionScreenOffset(const VS::Render::ScreenOffset& offset) {
  PerspectiveRoomTransformer transformer = FrameTransformer();
  SetProjectionScreenOffset(transformer, {offset.x, offset.y});
  VS::Core::Gte::SetSharedProjection(transformer.Projection());
}
}  // namespace VS::Render
