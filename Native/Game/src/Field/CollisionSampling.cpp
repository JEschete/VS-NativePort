// Hand-maintained (tools/port/owned.csv).
#include "Field/CollisionSampling.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

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

// The MPD collision section header (_mpdRoomCollisionMapSection, 146C.h:12).
struct CollisionHeader {
  std::int16_t width;
  std::int16_t height;
  std::int16_t height_origin;
  std::int16_t cell_count;
};
static_assert(sizeof(CollisionHeader) ==
              offsetof(CollisionHeader, cell_count) + sizeof(std::int16_t));

template <typename T>
T ReadAt(std::span<const std::byte> bytes, std::size_t offset) {
  T value;
  std::memcpy(&value, bytes.subspan(offset, sizeof value).data(), sizeof value);
  return value;
}
}  // namespace

// decomp: BATTLE.PRG func_80099514 (src/BATTLE/BATTLE.PRG/2EA3C.s:2494). The
// original reads the live room map without checks; out-of-map samples and
// short maps report kCollisionHeightInvalid instead.
std::uint8_t SampleCollisionHeightCode(std::span<const std::byte> collision_map,
                                       std::uint32_t sample_x, int sample_z,
                                       int layer) {
  if (collision_map.size() < sizeof(CollisionHeader)) {
    return kCollisionHeightInvalid;
  }
  const auto map = ReadAt<CollisionHeader>(collision_map, 0);
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
  const int signed_sample =
      sample <= INT8_MAX ? sample : static_cast<int>(sample) - kByteRange;
  const int height =
      signed_sample + ((descriptor & kHeightBiasMask) >> kHeightBiasShift);
  return static_cast<std::uint8_t>((height & kCollisionHeightValueMask) |
                                   (sample & kCollisionHeightInvalid));
}
}  // namespace VS::Field
