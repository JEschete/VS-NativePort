// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScreenDistortion/RadialWarpLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <unordered_set>

#include "Core/Log.hpp"
#include "Core/Math/FixedPoint.hpp"
#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Legacy/Psx.hpp"
#include "Render/DrawList.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Legacy::GpuPointer;
using VS::Legacy::kGpuLinkEnd;

namespace VS::Effects::ScreenDistortion {
namespace {
constexpr std::size_t kLastSceneBucket = 2046;
constexpr unsigned kLinkMask = 0xFFFFFF;
constexpr unsigned kTagLengthShift = 24;
static_assert(sizeof(VS::Core::Math::kArcTangentTable) ==
              kRadialWarpRootTableBytes);

[[noreturn]] void InvalidLiveWarp(const char* reason) {
  VS::Core::LogError("effects", "screen radial warp: {}", reason);
  std::abort();
}

std::uint32_t TagWord(const std::uint32_t* packet) {
  return static_cast<std::uint32_t>(*packet);
}

// Follows the ordering-table link after `packet`. The terminal entry and the
// table base end the walk without being visited, matching 3D8.s.
std::uint32_t* NextPacket(std::uint32_t* packet, const std::uint32_t* base) {
  if (packet == nullptr || packet == base) return nullptr;
  const auto link = TagWord(packet) & kLinkMask;
  if (link == kGpuLinkEnd) return nullptr;
  auto* next = static_cast<std::uint32_t*>(GpuPointer(link));
  if (next == nullptr) {
    InvalidLiveWarp("ordering link has no registered native packet");
  }
  if (next == base || (TagWord(next) & kLinkMask) == kGpuLinkEnd) {
    return nullptr;
  }
  return next;
}

// Walks the frame ordering table from the last scene bucket, checking for a
// cyclic list with a second cursor that advances two links per step. The
// frame's typed draw items (Render/DrawList.hpp) are warped as the walk
// follows the link their slot entry held, where their equivalent packets
// would have been visited; a packet whose link ends the list ends the walk
// unvisited.
class OrderingTablePackets final : public PrimitivePacketCursor {
 public:
  OrderingTablePackets(std::uint32_t* base, const RadialWarpField& field,
                       VS::Render::FrameDrawList& items)
      : base_(base),
        fast_(base + kLastSceneBucket),
        field_(field),
        items_(items) {}

  bool Advance() override {
    if (!started_) {
      started_ = true;
      packet_ = base_ + kLastSceneBucket;
      walked_.insert(packet_);
      return true;
    }
    WarpItemsBefore(TagWord(packet_) & kLinkMask);
    packet_ = NextPacket(packet_, base_);
    fast_ = NextPacket(NextPacket(fast_, base_), base_);
    if (packet_ != nullptr && packet_ == fast_) {
      InvalidLiveWarp("cyclic ordering list");
    }
    if (packet_ == nullptr) return false;
    walked_.insert(packet_);
    return true;
  }

  std::size_t WordCount() const override {
    return static_cast<std::size_t>(TagWord(packet_) >> kTagLengthShift);
  }

  std::uint32_t Word(std::size_t index) const override {
    return static_cast<std::uint32_t>(Payload()[index]);
  }

  void SetWord(std::size_t index, std::uint32_t value) override {
    Payload()[index] = value;
  }

 private:
  [[nodiscard]] std::span<std::uint32_t> Payload() const {
    return {packet_ + 1, WordCount()};
  }

  // Items at slot 0 are anchored at the table base, which the walk never
  // passes through, so they stay unwarped like the packets the original
  // linked there.
  void WarpItemsBefore(std::uint32_t link) {
    items_.ForEachItemBefore(
        link, walked_,
        [this](VS::Render::DrawPolygon& polygon, std::size_t /*slot*/) {
          field_.Warp(polygon);
        });
  }

  std::uint32_t* base_;
  std::uint32_t* packet_{};
  std::uint32_t* fast_;
  const RadialWarpField& field_;
  VS::Render::FrameDrawList& items_;
  std::unordered_set<const void*> walked_;
  bool started_{};
};
}  // namespace

// decomp: SCREFF2.PRG func_800F9BD8 (src/GIM/SCREFF2.PRG/0.c:126).
void WarpLivePolygonVertices() {
  if (screenEffectOverlay == nullptr ||
      VS::Render::FrameScratchpad().ordering_table == nullptr) {
    InvalidLiveWarp("active controller and ordering table are required");
  }
  const RadialWarpSettings settings{
      .scale =
          static_cast<std::uint16_t>(screenEffectOverlay->radial_warp_scale),
      .growth = screenEffectOverlay->radial_warp_growth,
  };
  const RadialWarpField field(settings,
                              std::span(VS::Core::Math::kArcTangentTable));
  OrderingTablePackets packets(
      static_cast<std::uint32_t*>(VS::Render::FrameScratchpad().ordering_table),
      field, VS::Render::GameFrameDrawList());
  WarpPolygonVertices(field, packets);
}
}  // namespace VS::Effects::ScreenDistortion
