// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/TileHeightSampling.hpp"

#include <array>
#include <cstddef>
#include <span>

#include "Core/Math/Distance.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr int kTileUnits = 128;
constexpr int kSamplesPerAxis = 4;
constexpr int kSampleStep = kTileUnits / kSamplesPerAxis;
constexpr unsigned kSampleRowWidth = 4;
constexpr unsigned kFallbackSample = 10;
constexpr int kHeightSignBit = 0x4000;
constexpr int kHeightMagnitudeMask = kHeightSignBit - 1;
constexpr unsigned kSolidPatchMask = 0x33;

int SignedHeight(int sample) {
  return (sample & kHeightMagnitudeMask) - (sample & kHeightSignBit);
}
}  // namespace

int SelectTileHeight(std::span<const int, kTileSampleCount> samples,
                     int center) noexcept {
  int highest = center;
  unsigned invalid = 0;
  for (std::size_t index = 0; index < samples.size(); ++index) {
    const int sample = samples[index];
    if (sample < 0) invalid |= 1u << index;
    if (SignedHeight(sample) > SignedHeight(highest)) highest = sample;
  }
  if (invalid == 0) return highest;
  for (unsigned index = 0; index < kFallbackSample; ++index) {
    if (index % kSampleRowWidth == kSampleRowWidth - 1) continue;
    const unsigned patch = kSolidPatchMask << index;
    if ((~invalid & patch) == patch) return highest;
  }
  return samples[kFallbackSample];
}

// decomp: BATTLE.PRG func_800E71DC (src/BATTLE/BATTLE.PRG/7E05C.s:738).
int SampleAiTileGroundHeight(const FloorHeightQuery& floor_height,
                             Core::Math::TileCoordinate tile) {
  const int origin_x = tile.x * kTileUnits;
  const int origin_z = tile.z * kTileUnits;
  const int center =
      floor_height(origin_x + kTileUnits / 2, origin_z + kTileUnits / 2);
  std::array<int, kTileSampleCount> samples{};
  for (int index = 0; index < kTileSampleCount; ++index) {
    samples[static_cast<std::size_t>(index)] =
        floor_height(origin_x + (index / kSamplesPerAxis) * kSampleStep,
                     origin_z + (index % kSamplesPerAxis) * kSampleStep);
  }
  return SelectTileHeight(samples, center);
}
}  // namespace VS::Battle::Ai
