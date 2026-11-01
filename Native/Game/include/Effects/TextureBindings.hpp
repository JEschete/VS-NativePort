#pragma once

#include <array>
#include <cstddef>
#include <span>

// An effect package's texture block (block 1): a table count, the descriptor
// offset, one frame table offset per material channel, the frame records and
// the texture descriptors. The parser returns validated byte offsets only; the
// live binding forms the package's pointers from them.
namespace VS::Effects {
inline constexpr std::size_t kTextureTableCapacity = 4;
inline constexpr std::size_t kTextureFrameRecordBytes = 4;
inline constexpr std::size_t kTextureFrameRecordAlignment = 2;
inline constexpr std::size_t kTextureDescriptorBytes = 0x18;
// E084 (Gravity) authors material channel 1 but supplies only table 0.
inline constexpr int kGravityEffectId = 84;

// The validated layout of one texture block, in offsets from its first byte.
struct TextureBlockLayout {
  std::array<std::size_t, kTextureTableCapacity> table_offsets{};
  std::size_t table_count = 0;
  std::size_t descriptor_offset = 0;
};

// The package a block is bound into.
struct EffectTextureOwner {
  bool is_action_package = false;
  int action_effect_id = 0;
};

// Validates a texture block (_parsePfileBlock1, 6F014.c:512): every table and
// frame record lies inside the block and references available descriptors.
// A malformed block aborts. In the action package of E084, channel 1 shares
// table 0. The block's bytes are not changed.
[[nodiscard]] TextureBlockLayout BindEffectTextureTables(
    std::span<const std::byte> block, const EffectTextureOwner& owner);

// Validates a block and publishes it into the current effect package: tables
// past the layout's count are cleared. A missing package or a misaligned block
// aborts.
void BindLiveEffectTextureTables(std::span<std::byte> block);
}  // namespace VS::Effects
