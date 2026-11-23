#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The MAP\*.ZUD enemy bundles, decoded strictly for the asset pipeline
// (ASSET_PIPELINE.md §6.2.5, #200). A room spawn reads the zone's ZUD for an
// enemy and loads each section like the OBJ file it holds (BATTLE.PRG
// func_800775C0); the encyclopedia preview also reads the identity bytes.
//
// A file is a 0x30-byte header, then the sections back to back in header
// order with nothing between or after them. The header holds 8 identity bytes
// (ZudData, MENU9.PRG 592C.c:8: the character, weapon, battle-sequence model,
// weapon material, shield, shield material and default material, then a pad
// byte), then each section's offset and size as little-endian words. An empty
// section has size 0 at the offset where the next one starts.
namespace VS::Content {

inline constexpr std::string_view kZudExtension = ".ZUD";
inline constexpr std::size_t kZudHeaderBytes = 0x30;
inline constexpr std::size_t kZudIdentityBytes = 8;

// The sections in header and file order.
enum class ZudSection : std::uint8_t {
  kBody,             // the character model (an SHP)
  kWeapon,           // a WEP
  kShield,           // a WEP
  kCommonAnimation,  // a SEQ bank
  kBattleAnimation,  // a SEQ bank
};
inline constexpr std::size_t kZudSectionCount = 5;

struct ZudFile {
  std::array<std::uint8_t, kZudIdentityBytes> identity{};
  std::array<std::vector<std::byte>, kZudSectionCount> sections;

  [[nodiscard]] const std::vector<std::byte>& Section(
      ZudSection section) const noexcept {
    return sections.at(static_cast<std::size_t>(section));
  }
  bool operator==(const ZudFile&) const = default;
};

// Decodes a whole ZUD file: a full header whose sections start where the
// previous one ends (the first at kZudHeaderBytes) and whose last one ends the
// file. Errors name the section at fault.
[[nodiscard]] std::expected<ZudFile, std::string> DecodeZudFile(
    std::span<const std::byte> data);
// The file `zud` is, its offsets packed from the header on. A file
// DecodeZudFile returned rebuilds byte for byte.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeZudFile(
    const ZudFile& zud);

// The section's name as errors and documents write it, e.g. "battle
// animation".
[[nodiscard]] std::string_view ZudSectionName(ZudSection section) noexcept;

}  // namespace VS::Content
