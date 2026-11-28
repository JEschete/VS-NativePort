#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The room collision map's height samples. The map is an MPD collision section
// (an 8-byte width/height/origin/cell-count header, a floor and a ceiling
// descriptor per tile, then 4x4 signed sample patterns); the sampler reads it
// as checked bytes, so a short or malformed map reports no height instead of
// reading past it.
namespace VS::Field {
struct CollisionMapHeader {
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::int16_t height_origin = 0;
  std::int16_t cell_count = 0;
};

inline constexpr std::uint8_t kCollisionHeightInvalid = 0x80;
inline constexpr std::uint8_t kCollisionHeightValueMask = 0x7F;
inline constexpr int kCollisionFloorLayer = 0;
inline constexpr int kCollisionCeilingLayer = 1;
inline constexpr std::size_t kCollisionSamplesPerTile = 16;

// The live room's collision map bytes; empty when no room is loaded.
[[nodiscard]] std::span<const std::byte> LiveRoomCollisionMap();

// Returns a sample's height code (func_80099514): the low 7 bits hold the
// height and bit 7 the pattern's no-floor bit. A sample or layer outside the
// map returns kCollisionHeightInvalid.
[[nodiscard]] std::uint8_t SampleCollisionHeightCode(
    std::span<const std::byte> collision_map, std::uint32_t sample_x,
    int sample_z, int layer);

// Owned native samples. Legacy descriptor offsets and height biases are
// resolved at import; queries retain no source storage.
class CollisionHeightMap {
 public:
  [[nodiscard]] static std::expected<CollisionHeightMap, std::string>
  ImportLegacy(std::span<const std::byte> collision_map);

  [[nodiscard]] const std::optional<CollisionMapHeader>& Header() const {
    return header_;
  }
  [[nodiscard]] std::uint8_t Sample(std::uint32_t sample_x, int sample_z,
                                    int layer) const;

 private:
  std::optional<CollisionMapHeader> header_;
  std::vector<std::array<std::uint8_t, kCollisionSamplesPerTile>> tiles_;
};
}  // namespace VS::Field
