#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENUF.PRG results palette (clutBuf) and the palette pulse of the time-attack
// and cube-puzzle end screens (F03023, F03024). Both pulses blend two of the
// palette's rows into a third and hand all five rows to a ResultsClutTarget,
// so the cores reach no global state.

#include <cstddef>
#include <cstdint>
#include <span>

#include "Render/DisplayControl.hpp"

namespace VS::Menu::Results {
// clutBuf: five 32-color rows uploaded to (768, 511).
inline constexpr std::size_t kResultsClutRowColors = 32;
inline constexpr std::size_t kResultsClutWords = 160;
inline constexpr Render::ScreenRect kResultsClutArea{768, 511, 160, 1};
// A pulse's blend weight is in sixteenths.
inline constexpr std::int32_t kResultsPulseWeights = 16;

// The time-attack pulse mixes the second row (weighted by the blend weight)
// with the fourth (weighted by 16 minus it) into the fifth.
inline constexpr std::size_t kTimeAttackPulseFirstRow = 32;
inline constexpr std::size_t kTimeAttackPulseSecondRow = 96;
inline constexpr std::size_t kTimeAttackPulseTargetRow = 128;

// Where the pulsed palette goes (LoadImage).
class ResultsClutTarget {
 public:
  ResultsClutTarget() = default;
  ResultsClutTarget(const ResultsClutTarget&) = delete;
  ResultsClutTarget& operator=(const ResultsClutTarget&) = delete;
  ResultsClutTarget(ResultsClutTarget&&) = delete;
  ResultsClutTarget& operator=(ResultsClutTarget&&) = delete;
  virtual ~ResultsClutTarget() = default;
  virtual void Upload(const Render::ScreenRect& area,
                      std::span<const std::uint16_t> colors) = 0;
};

// The first color of each 32-color row a palette pulse reads and writes: the
// row weighted by the blend weight (which also supplies the mask bits), the
// row weighted by 16 minus it, and the row the mix replaces.
struct ResultsClutPulseRows {
  std::size_t weighted = 0;
  std::size_t complement = 0;
  std::size_t target = 0;
};

// The fourth palette row weighted by `blend_weight` sixteenths (its mask bits
// kept) and the third by the rest, into the fifth; then all five rows upload.
inline constexpr ResultsClutPulseRows kCubePuzzlePulseRows{
    .weighted = 96,
    .complement = 64,
    .target = 128,
};

// Mixes the second and fourth palette rows by `blend_weight` sixteenths into
// the fifth, keeping the second row's mask bits, then uploads all five rows.
// Each component's weighted sum wraps in 32 bits as on the R3000.
void PulseTimeAttackClut(std::span<std::uint16_t, kResultsClutWords> clut,
                         std::int32_t blend_weight, ResultsClutTarget& target);

// The cube-puzzle pulse: kCubePuzzlePulseRows mixed as PulseTimeAttackClut.
void PulseCubePuzzleClut(std::span<std::uint16_t, kResultsClutWords> clut,
                         std::int32_t blend_weight, ResultsClutTarget& target);

// The mix both pulses share, then the upload of all five rows. False, with the
// palette untouched and nothing uploaded, when a row does not lie inside the
// palette.
bool PulseResultsClut(std::span<std::uint16_t, kResultsClutWords> clut,
                      ResultsClutPulseRows rows, std::int32_t blend_weight,
                      ResultsClutTarget& target);
}  // namespace VS::Menu::Results
