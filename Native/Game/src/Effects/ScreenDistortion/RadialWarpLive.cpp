// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScreenDistortion/RadialWarpLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Core/Log.hpp"
#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Core/Math/FixedPoint.h"
#include "Effects/ScreenEffects.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Effects::ScreenDistortion {
namespace {
constexpr std::size_t kLastSceneBucket = 2046;
constexpr unsigned kLinkMask = 0xFFFFFF;
constexpr unsigned kTagLengthShift = 24;
static_assert(sizeof(Core_Math_kArcTangentTable) == kRadialWarpRootTableBytes);

[[noreturn]] void InvalidLiveWarp(const char* reason) {
  VS_LOG_ERROR("effects", "screen radial warp: {}", reason);
  std::abort();
}

std::uint32_t TagWord(const u_long* packet) {
  return static_cast<std::uint32_t>(*packet);
}

// Follows the ordering-table link after `packet`. The terminal entry and the
// table base end the walk without being visited, matching 3D8.s.
u_long* NextPacket(u_long* packet, const u_long* base) {
  if (packet == nullptr || packet == base) return nullptr;
  const auto link = TagWord(packet) & kLinkMask;
  if (link == LEGACY_GPU_LINK_END) return nullptr;
  auto* next = static_cast<u_long*>(LegacyGpu_Pointer(link));
  if (next == nullptr) {
    InvalidLiveWarp("ordering link has no registered native packet");
  }
  if (next == base || (TagWord(next) & kLinkMask) == LEGACY_GPU_LINK_END) {
    return nullptr;
  }
  return next;
}

// Walks the frame ordering table from the last scene bucket, checking for a
// cyclic list with a second cursor that advances two links per step.
class OrderingTablePackets final : public PrimitivePacketCursor {
 public:
  explicit OrderingTablePackets(u_long* base)
      : base_(base), fast_(base + kLastSceneBucket) {}

  bool Advance() override {
    if (!started_) {
      started_ = true;
      packet_ = base_ + kLastSceneBucket;
      return true;
    }
    packet_ = NextPacket(packet_, base_);
    fast_ = NextPacket(NextPacket(fast_, base_), base_);
    if (packet_ != nullptr && packet_ == fast_) {
      InvalidLiveWarp("cyclic ordering list");
    }
    return packet_ != nullptr;
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
  [[nodiscard]] std::span<u_long> Payload() const {
    return {packet_ + 1, WordCount()};
  }

  u_long* base_;
  u_long* packet_{};
  u_long* fast_;
  bool started_{};
};
}  // namespace

// decomp: SCREFF2.PRG func_800F9BD8 (src/GIM/SCREFF2.PRG/3D8.s:9).
void WarpLivePolygonVertices() {
  if (Effects_screenEffectOverlay == nullptr ||
      Render_frameScratch.orderingTable == nullptr) {
    InvalidLiveWarp("active controller and ordering table are required");
  }
  const RadialWarpSettings settings{
      .scale = static_cast<std::uint16_t>(
          Effects_screenEffectOverlay->radialWarpArg0),
      .growth = Effects_screenEffectOverlay->radialWarpArg1,
  };
  OrderingTablePackets packets(
      static_cast<u_long*>(Render_frameScratch.orderingTable));
  WarpPolygonVertices(settings, std::span(Core_Math_kArcTangentTable), packets);
}
}  // namespace VS::Effects::ScreenDistortion
