// Hand-maintained (tools/port/owned.csv).
#include "Field/CollisionSampling.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>

namespace VS::Field {
namespace {
constexpr int kSamplesPerAxisBits = 2;
constexpr int kSamplesPerAxis = 1 << kSamplesPerAxisBits;
constexpr auto kPatternBytes = std::size_t{kSamplesPerAxis} * kSamplesPerAxis;
constexpr int kLayerCount = 2;
constexpr std::uint16_t kPatternIndexMask = 0x7FF;
constexpr std::uint16_t kHeightBiasMask = 0xF800;
constexpr int kHeightBiasShift = 9;
constexpr int kByteRange = 0x100;

static_assert(sizeof(CollisionMapHeader) ==
              offsetof(CollisionMapHeader, cell_count) + sizeof(std::int16_t));

template <typename T>
T ReadAt(std::span<const std::byte> bytes, std::size_t offset) {
  T value;
  std::memcpy(&value, bytes.subspan(offset, sizeof value).data(), sizeof value);
  return value;
}

std::uint8_t DecodeSample(std::uint8_t sample, int height_bias) {
  const int signed_sample =
      sample <= INT8_MAX ? sample : static_cast<int>(sample) - kByteRange;
  const int height = signed_sample + height_bias;
  return static_cast<std::uint8_t>((height & kCollisionHeightValueMask) |
                                   (sample & kCollisionHeightInvalid));
}
}  // namespace

// decomp: BATTLE.PRG func_80099514 (src/BATTLE/BATTLE.PRG/2EA3C.s:2363). The
// original reads the live room map without checks; out-of-map samples and
// short maps report kCollisionHeightInvalid instead.
std::uint8_t SampleCollisionHeightCode(std::span<const std::byte> collision_map,
                                       std::uint32_t sample_x, int sample_z,
                                       int layer) {
  if (collision_map.size() < sizeof(CollisionMapHeader)) {
    return kCollisionHeightInvalid;
  }
  const auto map = ReadAt<CollisionMapHeader>(collision_map, 0);
  const auto tile_x = sample_x >> kSamplesPerAxisBits;
  const auto tile_z =
      static_cast<std::uint32_t>(sample_z) >> kSamplesPerAxisBits;
  if (layer < 0 || layer >= kLayerCount || sample_z < 0 || map.width <= 0 ||
      map.height <= 0 || map.cell_count < map.width * map.height ||
      tile_x >= static_cast<std::uint32_t>(map.width) ||
      tile_z >= static_cast<std::uint32_t>(map.height)) {
    return kCollisionHeightInvalid;
  }
  const auto cells = static_cast<std::size_t>(map.cell_count);
  const std::size_t patterns =
      sizeof map + cells * kLayerCount * sizeof(std::uint16_t);
  if (patterns > collision_map.size()) return kCollisionHeightInvalid;
  const std::size_t tile =
      static_cast<std::size_t>(tile_z) * static_cast<std::size_t>(map.width) +
      tile_x;
  const auto descriptor = ReadAt<std::uint16_t>(
      collision_map,
      sizeof map + (static_cast<std::size_t>(layer) * cells + tile) *
                       sizeof(std::uint16_t));
  const std::size_t pattern =
      patterns +
      static_cast<std::size_t>(descriptor & kPatternIndexMask) * kPatternBytes;
  if (pattern > collision_map.size() ||
      collision_map.size() - pattern < kPatternBytes) {
    return kCollisionHeightInvalid;
  }
  const auto sample = std::to_integer<std::uint8_t>(
      collision_map[pattern +
                    (static_cast<std::size_t>(sample_z) &
                     (kSamplesPerAxis - 1)) *
                        kSamplesPerAxis +
                    (sample_x & (kSamplesPerAxis - 1))]);
  return DecodeSample(sample,
                      (descriptor & kHeightBiasMask) >> kHeightBiasShift);
}

std::expected<CollisionHeightMap, std::string> CollisionHeightMap::ImportLegacy(
    std::span<const std::byte> collision_map) {
  CollisionHeightMap result;
  if (collision_map.empty()) return result;
  if (collision_map.size() < sizeof(CollisionMapHeader)) {
    return std::unexpected("collision height header is incomplete");
  }
  const auto header = ReadAt<CollisionMapHeader>(collision_map, 0);
  if (header.width <= 0 || header.height <= 0 ||
      header.cell_count < header.width * header.height) {
    return std::unexpected(
        "collision height dimensions or cell count are invalid");
  }
  const auto cells = static_cast<std::size_t>(header.cell_count);
  const std::size_t patterns =
      sizeof(header) + cells * kLayerCount * sizeof(std::uint16_t);
  if (patterns > collision_map.size()) {
    return std::unexpected("collision height tile descriptors are incomplete");
  }
  result.header_ = header;
  const auto tiles = static_cast<std::size_t>(header.width) * header.height;
  result.tiles_.reserve(tiles * kLayerCount);
  for (int layer = 0; layer < kLayerCount; ++layer) {
    for (std::size_t tile = 0; tile < tiles; ++tile) {
      const auto descriptor = ReadAt<std::uint16_t>(
          collision_map,
          sizeof(header) + (static_cast<std::size_t>(layer) * cells + tile) *
                               sizeof(std::uint16_t));
      const auto pattern =
          patterns + (descriptor & kPatternIndexMask) * kPatternBytes;
      if (pattern > collision_map.size() ||
          collision_map.size() - pattern < kPatternBytes) {
        return std::unexpected("collision height pattern is incomplete");
      }
      std::array<std::uint8_t, kPatternBytes> samples{};
      for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = DecodeSample(
            std::to_integer<std::uint8_t>(collision_map[pattern + index]),
            (descriptor & kHeightBiasMask) >> kHeightBiasShift);
      }
      result.tiles_.push_back(samples);
    }
  }
  return result;
}

std::uint8_t CollisionHeightMap::Sample(std::uint32_t sample_x, int sample_z,
                                        int layer) const {
  if (!header_ || layer < 0 || layer >= kLayerCount || sample_z < 0) {
    return kCollisionHeightInvalid;
  }
  const auto tile_x = sample_x >> kSamplesPerAxisBits;
  const auto tile_z =
      static_cast<std::uint32_t>(sample_z) >> kSamplesPerAxisBits;
  const auto width = static_cast<std::uint32_t>(header_->width);
  const auto height = static_cast<std::uint32_t>(header_->height);
  if (tile_x >= width || tile_z >= height) return kCollisionHeightInvalid;
  const auto tile =
      (static_cast<std::size_t>(layer) * height + tile_z) * width + tile_x;
  const auto sample =
      (static_cast<std::uint32_t>(sample_z) & (kSamplesPerAxis - 1)) *
          kSamplesPerAxis +
      (sample_x & (kSamplesPerAxis - 1));
  return tiles_.at(tile).at(sample);
}
}  // namespace VS::Field
