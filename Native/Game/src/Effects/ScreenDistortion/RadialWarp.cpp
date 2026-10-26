#include "Effects/ScreenDistortion/RadialWarp.h"

#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Math/FixedPoint.h"
#include "Core/Math/ScalarMath.h"
#include "Effects/ScreenEffects.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
constexpr std::size_t kScaleEntries = 444;
constexpr std::size_t kLastSceneBucket = 2046;
constexpr unsigned kLinkMask = 0xFFFFFF;
constexpr unsigned kTagLengthShift = 24;
constexpr unsigned kCommandGroupShift = 26;
constexpr unsigned kFirstDrawGroup = 8;
constexpr unsigned kEndDrawGroup = 56;
constexpr unsigned kPolygonGroups = 8;
constexpr unsigned kCoordinateBits = 16;
constexpr unsigned kWordShiftMask = 31;
constexpr unsigned kRootNormalizeShift = 24;
constexpr std::ptrdiff_t kRootIndexBias = 64;
constexpr std::ptrdiff_t kRootTableOffset = 516;
constexpr std::ptrdiff_t kRootEntryBytes = sizeof(std::uint16_t);
constexpr int kCenterX = 160;
constexpr int kCenterY = 112;
constexpr int kFixedOne = 4096;
constexpr unsigned kFixedFractionBits = 12;
constexpr unsigned kInitialScaleShift = 4;
constexpr unsigned kStoredScaleShift = 3;
constexpr unsigned kVertexScaleShift = 13;
using RadialScales = std::array<std::uint16_t, kScaleEntries>;
static_assert(sizeof(u_long) == sizeof(std::uint32_t));

[[noreturn]] void InvalidWarp(const char* reason) {
  VS_LOG_ERROR("effects", "screen radial warp: {}", reason);
  std::abort();
}

std::int32_t SignedWord(std::uint32_t value) {
  return std::bit_cast<std::int32_t>(value);
}

RadialScales BuildScales(std::uint16_t base, std::int16_t growth) {
  RadialScales scales{};
  std::int32_t amplitude = static_cast<std::int32_t>(base)
                           << kInitialScaleShift;
  const std::int32_t factor = growth + kFixedOne;
  for (auto& scale : scales) {
    scale = static_cast<std::uint16_t>(amplitude >> kStoredScaleShift);
    const auto low_product =
        static_cast<std::uint32_t>(std::int64_t{amplitude} * factor);
    amplitude = SignedWord(low_product) >> kFixedFractionBits;
  }
  return scales;
}

unsigned RadiusIndex(std::int32_t x, std::int32_t y) {
  const std::int32_t ir_x =
      Core_Math_WrapSigned16(static_cast<std::uint32_t>(x));
  const std::int32_t ir_y =
      Core_Math_WrapSigned16(static_cast<std::uint32_t>(y));
  const auto square = static_cast<std::uint32_t>(ir_x * ir_x) +
                      static_cast<std::uint32_t>(ir_y * ir_y);
  if (square == 0) return 0;
  const unsigned leading = SignedWord(square) < 0 ? std::countl_one(square)
                                                  : std::countl_zero(square);
  const unsigned even_leading = leading & ~1u;
  const unsigned shift = (kRootNormalizeShift - even_leading) & kWordShiftMask;
  const auto normalized = SignedWord(square) >> shift;
  const auto offset =
      kRootTableOffset +
      (static_cast<std::ptrdiff_t>(normalized) - kRootIndexBias) *
          kRootEntryBytes;
  const auto resident = std::span(Core_Math_kArcTangentTable);
  if (offset < 0 || static_cast<std::size_t>(offset) >
                        resident.size() - sizeof(std::uint16_t)) {
    InvalidWarp("root lookup is outside its imported resident region");
  }
  // 3D8.s uses a masked right shift even for small radii. The resulting
  // prefix lookup is inside the existing arctangent/root import, not ideal
  // sqrt.
  const auto index = static_cast<std::size_t>(offset);
  const auto sample =
      static_cast<std::uint32_t>(resident[index]) |
      (static_cast<std::uint32_t>(resident[index + 1]) << CHAR_BIT);
  const unsigned exponent =
      ((kWordShiftMask - even_leading) >> 1) & kWordShiftMask;
  const auto radius = SignedWord(sample << exponent) >> kFixedFractionBits;
  return static_cast<unsigned>(radius) >= kScaleEntries
             ? static_cast<unsigned>(kScaleEntries - 1)
             : static_cast<unsigned>(radius);
}

u_long WarpPoint(u_long packed, const RadialScales& scales) {
  const std::int32_t x = Core_Math_WrapSigned16(packed) - kCenterX;
  const std::int32_t y =
      Core_Math_WrapSigned16(packed >> kCoordinateBits) - kCenterY;
  const auto scale = scales[RadiusIndex(x, y)];
  const auto scaled_x =
      SignedWord(static_cast<std::uint32_t>(std::int64_t{x} * scale)) >>
      kVertexScaleShift;
  const auto scaled_y =
      SignedWord(static_cast<std::uint32_t>(std::int64_t{y} * scale)) >>
      kVertexScaleShift;
  return static_cast<std::uint16_t>(scaled_x + kCenterX) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint16_t>(scaled_y + kCenterY))
          << kCoordinateBits);
}

void WarpPacket(std::span<u_long> payload, const RadialScales& scales) {
  std::size_t command = 0;
  unsigned group = 0;
  for (; command < payload.size(); ++command) {
    group = static_cast<unsigned>(payload[command] >> kCommandGroupShift);
    if (group >= kFirstDrawGroup && group < kEndDrawGroup) break;
  }
  if (command == payload.size() || group - kFirstDrawGroup >= kPolygonGroups) {
    return;
  }
  const unsigned kind = group - kFirstDrawGroup;
  const unsigned vertices = 3 + ((kind >> 1) & 1);
  const std::size_t stride = 1 + (kind & 1) + ((kind >> 2) & 1);
  const std::size_t first = command + 1;
  if (first >= payload.size() ||
      (vertices - 1) * stride >= payload.size() - first) {
    InvalidWarp("polygon coordinates exceed the declared packet length");
  }
  for (unsigned vertex = 0; vertex < vertices; ++vertex) {
    auto& point = payload[first + std::size_t{vertex} * stride];
    point = WarpPoint(point, scales);
  }
}

u_long* NextPacket(u_long* packet, const u_long* base) {
  if (packet == nullptr || packet == base) return nullptr;
  const auto link = static_cast<unsigned>(*packet & kLinkMask);
  if (link == LEGACY_GPU_LINK_END) return nullptr;
  auto* next = static_cast<u_long*>(LegacyGpu_Pointer(link));
  if (next == nullptr) {
    InvalidWarp("ordering link has no registered native packet");
  }
  if (next == base || (*next & kLinkMask) == LEGACY_GPU_LINK_END) {
    return nullptr;
  }
  return next;
}
}  // namespace

// decomp: SCREFF2.PRG func_800F9BD8 (src/GIM/SCREFF2.PRG/3D8.s:9).
extern "C" void Effects_ScreenDistortion_WarpPolygonVertices(void) {
  if (Effects_screenEffectOverlay == nullptr ||
      Render_frameScratch.orderingTable == nullptr) {
    InvalidWarp("active controller and ordering table are required");
  }
  const auto scales = BuildScales(
      static_cast<std::uint16_t>(Effects_screenEffectOverlay->radialWarpArg0),
      Effects_screenEffectOverlay->radialWarpArg1);
  auto* base = static_cast<u_long*>(Render_frameScratch.orderingTable);
  auto* packet = base + kLastSceneBucket;
  auto* fast = packet;
  while (packet != nullptr) {
    const auto words = static_cast<std::size_t>(*packet >> kTagLengthShift);
    WarpPacket(std::span(packet + 1, words), scales);
    packet = NextPacket(packet, base);
    fast = NextPacket(NextPacket(fast, base), base);
    if (packet != nullptr && packet == fast) {
      InvalidWarp("cyclic ordering list");
    }
  }
}
