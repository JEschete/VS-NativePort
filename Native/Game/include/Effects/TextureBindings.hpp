#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

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

// Why a texture descriptor's palette reference was refused.
enum class PaletteReferenceFault : std::uint8_t {
  kNone,
  // Its 16 or 256 palette entries lie outside the effect CLUT's x 768-1023.
  kColumn,
  // It names a row outside the effect CLUT rows 240-244.
  kRow,
  // It names an effect CLUT row nothing loaded for its effect fills.
  kUnbound,
};

// The first descriptor whose palette reference was refused, and its CLUT
// word.
struct PaletteReferenceCheck {
  PaletteReferenceFault fault = PaletteReferenceFault::kNone;
  std::size_t descriptor = 0;
  std::uint16_t clut = 0;

  friend bool operator==(const PaletteReferenceCheck&,
                         const PaletteReferenceCheck&) = default;
};

// Validates a texture block (_parsePfileBlock1, 6F014.c:512): every table and
// frame record lies inside the block and references available descriptors.
// A malformed block aborts. In the action package of E084, channel 1 shares
// table 0. The block's bytes are not changed.
[[nodiscard]] TextureBlockLayout BindEffectTextureTables(
    std::span<const std::byte> block, const EffectTextureOwner& owner);

// Checks that each indexed descriptor of a validated block samples its
// palette inside the effect CLUT, on one of `bound_rows` (bit n: row 240 + n;
// EffectPaletteRows). A descriptor's CLUT word holds the palette's x / 16 in
// bits 0-5 and its row in bits 6-14; its page's bits 7-8 select 4-bit (16
// entries), 8-bit (256) or direct colour, which samples no palette.
[[nodiscard]] PaletteReferenceCheck CheckEffectPaletteReferences(
    std::span<const std::byte> block, const TextureBlockLayout& layout,
    std::uint8_t bound_rows);

// What a palette fault means, for diagnostics.
[[nodiscard]] std::string_view DescribePaletteFault(
    PaletteReferenceFault fault) noexcept;

// Validates a block and publishes it into the current effect package: tables
// past the layout's count are cleared. A missing package, a misaligned block
// or a descriptor whose palette is outside `bound_palette_rows`
// (CheckEffectPaletteReferences) aborts.
void BindLiveEffectTextureTables(std::span<std::byte> block,
                                 std::uint8_t bound_palette_rows);
}  // namespace VS::Effects
