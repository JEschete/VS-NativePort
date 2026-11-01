// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScreenDistortion/RadialWarp.hpp"

#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Effects::ScreenDistortion {
namespace {
constexpr std::size_t kScaleEntries = 444;
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

[[noreturn]] void InvalidWarp(const char* reason) {
  VS_LOG_ERROR("effects", "screen radial warp: {}", reason);
  std::abort();
}

std::int32_t SignedWord(std::uint32_t value) {
  return std::bit_cast<std::int32_t>(value);
}

RadialScales BuildScales(RadialWarpSettings settings) {
  RadialScales scales{};
  std::int32_t amplitude = static_cast<std::int32_t>(settings.scale)
                           << kInitialScaleShift;
  const std::int32_t factor = settings.growth + kFixedOne;
  for (auto& scale : scales) {
    scale = static_cast<std::uint16_t>(amplitude >> kStoredScaleShift);
    const auto low_product =
        static_cast<std::uint32_t>(std::int64_t{amplitude} * factor);
    amplitude = SignedWord(low_product) >> kFixedFractionBits;
  }
  return scales;
}

unsigned RadiusIndex(std::span<const std::uint8_t> root_table, std::int32_t x,
                     std::int32_t y) {
  const std::int32_t ir_x =
      Core::Math::WrapSigned16(static_cast<std::uint32_t>(x));
  const std::int32_t ir_y =
      Core::Math::WrapSigned16(static_cast<std::uint32_t>(y));
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
  if (root_table.size() < sizeof(std::uint16_t) || offset < 0 ||
      static_cast<std::size_t>(offset) >
          root_table.size() - sizeof(std::uint16_t)) {
    InvalidWarp("root lookup is outside its imported resident region");
  }
  // 3D8.s uses a masked right shift even for small radii. The resulting
  // prefix lookup is inside the existing arctangent/root import, not ideal
  // sqrt.
  const auto index = static_cast<std::size_t>(offset);
  const auto sample =
      static_cast<std::uint32_t>(root_table[index]) |
      (static_cast<std::uint32_t>(root_table[index + 1]) << CHAR_BIT);
  const unsigned exponent =
      ((kWordShiftMask - even_leading) >> 1) & kWordShiftMask;
  const auto radius = SignedWord(sample << exponent) >> kFixedFractionBits;
  return static_cast<unsigned>(radius) >= kScaleEntries
             ? static_cast<unsigned>(kScaleEntries - 1)
             : static_cast<unsigned>(radius);
}

std::uint32_t WarpPoint(std::uint32_t packed, const RadialScales& scales,
                        std::span<const std::uint8_t> root_table) {
  const std::int32_t x = Core::Math::WrapSigned16(packed) - kCenterX;
  const std::int32_t y =
      Core::Math::WrapSigned16(packed >> kCoordinateBits) - kCenterY;
  const auto scale = scales[RadiusIndex(root_table, x, y)];
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

void WarpPacket(PrimitivePacketCursor& packet, const RadialScales& scales,
                std::span<const std::uint8_t> root_table) {
  const std::size_t words = packet.WordCount();
  std::size_t command = 0;
  unsigned group = 0;
  for (; command < words; ++command) {
    group = packet.Word(command) >> kCommandGroupShift;
    if (group >= kFirstDrawGroup && group < kEndDrawGroup) break;
  }
  if (command == words || group - kFirstDrawGroup >= kPolygonGroups) {
    return;
  }
  const unsigned kind = group - kFirstDrawGroup;
  const unsigned vertices = 3 + ((kind >> 1) & 1);
  const std::size_t stride = 1 + (kind & 1) + ((kind >> 2) & 1);
  const std::size_t first = command + 1;
  if (first >= words || (vertices - 1) * stride >= words - first) {
    InvalidWarp("polygon coordinates exceed the declared packet length");
  }
  for (unsigned vertex = 0; vertex < vertices; ++vertex) {
    const std::size_t index = first + std::size_t{vertex} * stride;
    packet.SetWord(index, WarpPoint(packet.Word(index), scales, root_table));
  }
}
}  // namespace

// decomp: SCREFF2.PRG func_800F9BD8 (src/GIM/SCREFF2.PRG/3D8.s:9).
void WarpPolygonVertices(RadialWarpSettings settings,
                         std::span<const std::uint8_t> root_table,
                         PrimitivePacketCursor& packets) {
  const auto scales = BuildScales(settings);
  while (packets.Advance()) WarpPacket(packets, scales, root_table);
}
}  // namespace VS::Effects::ScreenDistortion
