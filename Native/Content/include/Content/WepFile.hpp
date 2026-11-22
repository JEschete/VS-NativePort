#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ShpFile.hpp"

// The OBJ\*.WEP weapon model file, decoded strictly for the asset pipeline
// (ASSET_PIPELINE.md §6.2.4, #204). A file is the SHP magic and counts
// (ShpFile.hpp), the model's size and the model section SHP files use (flat
// polygons, no sound), then the texture and one rest rotation per bone:
//
//   size field   4 + width x 32 + palette_words (the game skips it)
//   0x01         the flags byte (the game skips it)
//   width / 2    one byte
//   16           the height byte (the game draws 32 rows whatever it says)
//   palette_words  48, or 96 for the double palette
//   handle       palette_words / 3 colour words
//   7 materials  2 x palette_words / 3 colour words each: iron, bronze,
//                hagane, silver, damascus, wood, leather (the blade's
//                material picks the row the game uploads after the handle)
//   pixels       width x 32 bytes, 8-bit indices
//   rotations    x, y, z and a reserved halfword per bone
//
// The decoder keeps every byte it cannot derive, so EncodeWep rebuilds the
// exact file; it rejects anything else, and what the game cannot load, with
// a message naming the offset. The game's own parse (ActorAssets.cpp
// ParseWeaponFile) keeps the original's lenient rules.
namespace VS::Content {

inline constexpr std::string_view kWepExtension = ".WEP";
inline constexpr std::size_t kWepMaterialCount = 7;
using WepMaterialNames = std::array<std::string_view, kWepMaterialCount>;
// The blade materials in palette row order. BATTLE.PRG's
// weaponMaterialPaletteMap (Items/Materials.hpp) gives each material id its
// row: wood 5, leather 6, bronze 1, iron 0, hagane 2, silver 3, damascus 4.
inline constexpr WepMaterialNames kWepMaterialNames{
    "iron", "bronze", "hagane", "silver", "damascus", "wood", "leather",
};
inline constexpr std::uint32_t kWepTextureRows = 32;
inline constexpr std::uint8_t kWepSinglePaletteWords = 48;
inline constexpr std::uint8_t kWepDoublePaletteWords = 96;
// The handle's colours are a third of the palette words; each material's are
// the other two thirds.
inline constexpr std::size_t kWepPaletteThirds = 3;
// The widest texture: two 64-pixel weapon texture slots.
inline constexpr std::uint8_t kWepMaxWidthByte = 64;
// The game refuses more bones (ActorAssets.hpp kWeaponModelBones).
inline constexpr std::size_t kWepMaxBones = 4;
// The game copies a weapon's model section into a buffer this size.
inline constexpr std::size_t kWepMaxModelBytes = 0x800;

// decomp: SVECTOR (include/psx/libgte.h:148-151): a bone's rest angles in
// 4096ths of a turn (the game uses the low 12 bits) and the unused pad
// halfword.
struct WepBoneRotation {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::int16_t reserved6 = 0;

  bool operator==(const WepBoneRotation&) const = default;
};

struct WepTexture {
  std::uint8_t width_byte = 0;  // width / 2
  std::uint8_t palette_words = kWepSinglePaletteWords;
  std::vector<std::uint16_t> handle;  // HandleWords() colour words
  // MaterialWords() colour words each, in material order.
  std::array<std::vector<std::uint16_t>, kWepMaterialCount> materials;
  std::vector<std::uint8_t> pixels;  // Width() x kWepTextureRows, row-major

  [[nodiscard]] std::uint32_t Width() const noexcept {
    return std::uint32_t{width_byte} * 2U;
  }
  [[nodiscard]] std::size_t HandleWords() const noexcept {
    return palette_words / kWepPaletteThirds;
  }
  [[nodiscard]] std::size_t MaterialWords() const noexcept {
    return palette_words - HandleWords();
  }
  bool operator==(const WepTexture&) const = default;
};

struct WepFile : ShpModel {
  WepTexture texture;
  std::vector<WepBoneRotation> rotations;  // one per bone

  bool operator==(const WepFile&) const = default;
};

[[nodiscard]] std::expected<WepFile, std::string> DecodeWep(
    std::span<const std::byte> data);
// The file `wep` is; DecodeWep reads it back to `wep`. Errors name what the
// file or the game cannot hold.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeWep(
    const WepFile& wep);

}  // namespace VS::Content
