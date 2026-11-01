#pragma once

#include <cstdint>
#include <span>

// The room collision map's height samples. The map is an MPD collision section
// (an 8-byte width/height/origin/cell-count header, a floor and a ceiling
// descriptor per tile, then 4x4 signed sample patterns); the sampler reads it
// as checked bytes, so a short or malformed map reports no height instead of
// reading past it.
namespace VS::Field {
inline constexpr std::uint8_t kCollisionHeightInvalid = 0x80;
inline constexpr std::uint8_t kCollisionHeightValueMask = 0x7F;
inline constexpr int kCollisionFloorLayer = 0;
inline constexpr int kCollisionCeilingLayer = 1;

// The live room's collision map bytes; empty when no room is loaded.
[[nodiscard]] std::span<const std::byte> LiveRoomCollisionMap();

// Returns a sample's height code (func_80099514): the low 7 bits hold the
// height and bit 7 the pattern's no-floor bit. A sample or layer outside the
// map returns kCollisionHeightInvalid.
[[nodiscard]] std::uint8_t SampleCollisionHeightCode(
    std::span<const std::byte> collision_map, std::uint32_t sample_x,
    int sample_z, int layer);
}  // namespace VS::Field
