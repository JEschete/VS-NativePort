// Hand-maintained (tools/port/owned.csv).
#include "Menu/Results/ResultsPalette.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;

// The PS1 15-bit color's components and mask bit.
constexpr std::int32_t kRedMask = 0x1F;
constexpr std::int32_t kGreenMask = 0x3E0;
constexpr std::int32_t kBlueMask = 0x7C00;
constexpr std::int32_t kMaskBit = 0x8000;

// One component of the pulse: both rows' component weighted, summed and
// divided by 16 with the R3000's 32-bit wrap and truncating division.
[[nodiscard]] std::int32_t MixComponent(std::int32_t first, std::int32_t second,
                                        std::int32_t blend_weight,
                                        std::int32_t mask) {
  const std::int32_t sum = math::WrappingAdd(
      math::WrappingMultiply(first & mask, blend_weight),
      math::WrappingMultiply(
          second & mask,
          math::WrappingSubtract(kResultsPulseWeights, blend_weight)));
  return (sum / kResultsPulseWeights) & mask;
}
}  // namespace

// decomp: MENUF.PRG pulseTimeAttackEnd (src/MENU/MENUF.PRG/3B8.c:1269; was
// _pulseTimeAttackEnd), 0x80104914
void PulseTimeAttackClut(std::span<std::uint16_t, kResultsClutWords> clut,
                         std::int32_t blend_weight, ResultsClutTarget& target) {
  PulseResultsClut(clut,
                   {.weighted = kTimeAttackPulseFirstRow,
                    .complement = kTimeAttackPulseSecondRow,
                    .target = kTimeAttackPulseTargetRow},
                   blend_weight, target);
}

// decomp: MENUF.PRG pulseCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:1307; was
// _pulseCubePuzzleEnd), 0x80104A50
void PulseCubePuzzleClut(std::span<std::uint16_t, kResultsClutWords> clut,
                         std::int32_t blend_weight, ResultsClutTarget& target) {
  PulseResultsClut(clut, kCubePuzzlePulseRows, blend_weight, target);
}

bool PulseResultsClut(std::span<std::uint16_t, kResultsClutWords> clut,
                      ResultsClutPulseRows rows, std::int32_t blend_weight,
                      ResultsClutTarget& target) {
  constexpr std::size_t kLastRowStart =
      kResultsClutWords - kResultsClutRowColors;
  if (rows.weighted > kLastRowStart || rows.complement > kLastRowStart ||
      rows.target > kLastRowStart) {
    return false;
  }
  for (std::size_t i = 0; i < kResultsClutRowColors; ++i) {
    const std::int32_t first = clut[rows.weighted + i];
    const std::int32_t second = clut[rows.complement + i];
    clut[rows.target + i] = static_cast<std::uint16_t>(
        MixComponent(first, second, blend_weight, kRedMask) |
        MixComponent(first, second, blend_weight, kGreenMask) |
        MixComponent(first, second, blend_weight, kBlueMask) |
        (first & kMaskBit));
  }
  target.Upload(kResultsClutArea, clut);
  return true;
}
}  // namespace VS::Menu::Results
