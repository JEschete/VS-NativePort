#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Content/ZndFile.hpp"

namespace VS::test_support {

// The weapon's blade, grip and gems, the shield and its gems, the accessory
// and six limb armours, 0x30 bytes each.
inline constexpr std::array<std::size_t, 16> kTemplatePartOffsets{
    0x34,  0x64,  0x94,  0xC4,  0xF4,  0x140, 0x170, 0x1A0,
    0x1D0, 0x204, 0x258, 0x2B4, 0x310, 0x36C, 0x3C8, 0x424,
};

// Gives every equipment part's damage type, cost type, material and gem effect
// a valid code, so a patterned template still formats to named part columns.
inline void UseValidPartCodes(Content::ZndTemplate& data, std::size_t seed) {
  constexpr std::array<std::uint8_t, 18> kGemEffects{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 16, 18, 19, 21};
  for (const std::size_t part : kTemplatePartOffsets) {
    data.at(part + 0x10) = static_cast<std::byte>((seed + part) % 4);
    data.at(part + 0x11) = static_cast<std::byte>((seed + part) % 5);
    data.at(part + 0x13) = static_cast<std::byte>((seed + part) % 8);
    data.at(part + 0x16) = static_cast<std::byte>(
        kGemEffects.at((seed + part) % kGemEffects.size()));
  }
}

inline constexpr std::size_t kTemplateLimbsAt = 0x238;
inline constexpr std::size_t kTemplateLimbBytes = 0x5C;
inline constexpr std::size_t kLimbActionSlots = 4;

// Gives every limb's four action slots (+0x10, 4 bytes each) an action id
// below 8 that varies with the seed, limb and slot; 0 leaves a slot empty.
inline void UseKnownActions(Content::ZndTemplate& data, std::size_t seed) {
  constexpr std::size_t kKnownActions = 8;
  for (std::size_t limb = 0; limb < 6; ++limb) {
    const std::size_t base = kTemplateLimbsAt + (limb * kTemplateLimbBytes);
    for (std::size_t slot = 0; slot < kLimbActionSlots; ++slot) {
      const std::size_t at = base + 0x10 + (slot * 4);
      data.at(at) = static_cast<std::byte>(
          (seed + (limb * kLimbActionSlots) + slot) % kKnownActions);
      data.at(at + 1) = std::byte{};
    }
  }
}

// Gives the template's reserved bytes the values its tables rebuild:
// padding at 0x33, 0x236, 0x237, bits 0-1 of 0x2C and each limb's
// +0x5A/+0x5B is zero, each limb's +0x52 is 1 exactly when its armour
// material (+0x50) is set, each action slot's index byte (+0x13, 4 bytes
// apart) holds the slot's position, the equipment indices (0x126-0x127,
// 0x202-0x203, 0x235, each limb's +0x53 and each part's +0x17) and the class
// and affinity tails (each limb's +0x0F, each part's +0x26, +0x27 and +0x2F)
// are zero, and the ZUD index (0x460, 4 bytes, little endian) is the
// template's `template_slot`.
inline void UseReservedBytes(Content::ZndTemplate& data,
                             std::size_t template_slot = 0) {
  constexpr std::array<std::size_t, 8> kZeroBytes{0x33,  0x126, 0x127, 0x202,
                                                  0x203, 0x235, 0x236, 0x237};
  for (const std::size_t at : kZeroBytes) data.at(at) = std::byte{};
  data.at(0x2C) &= ~std::byte{0x03};
  for (std::size_t limb = 0; limb < 6; ++limb) {
    const std::size_t base = kTemplateLimbsAt + (limb * kTemplateLimbBytes);
    for (std::size_t slot = 0; slot < kLimbActionSlots; ++slot) {
      data.at(base + 0x13 + (slot * 4)) = static_cast<std::byte>(slot);
    }
    data.at(base + 0x0F) = std::byte{};
    data.at(base + 0x52) =
        data.at(base + 0x50) == std::byte{} ? std::byte{} : std::byte{1};
    data.at(base + 0x53) = std::byte{};
    data.at(base + 0x5A) = std::byte{};
    data.at(base + 0x5B) = std::byte{};
  }
  constexpr std::array<std::size_t, 4> kPartZeroBytes{0x17, 0x26, 0x27, 0x2F};
  for (const std::size_t part : kTemplatePartOffsets) {
    for (const std::size_t at : kPartZeroBytes) {
      data.at(part + at) = std::byte{};
    }
  }
  for (std::size_t index = 0; index < 4; ++index) {
    data.at(0x460 + index) =
        static_cast<std::byte>((template_slot >> (8 * index)) & 0xFF);
  }
}

// Gives the enemy (0x04) and weapon (0x128) name slots two-word names that
// vary with the seed: glyphs (0-9, A-Z, a-z are 0..61), a word gap (FA 06),
// a glyph, the E7 terminator and zero padding, as the disc's names are.
inline void UseTextNames(Content::ZndTemplate& data, std::size_t seed) {
  constexpr std::array<std::size_t, 2> kNameSlots{0x04, 0x128};
  constexpr std::size_t kNameBytes = 24;
  constexpr std::size_t kUpper = 10;
  constexpr std::size_t kLower = 36;
  constexpr std::size_t kLetters = 26;
  for (std::size_t slot = 0; slot < kNameSlots.size(); ++slot) {
    const std::array<std::size_t, 6> glyphs{kUpper + ((seed + slot) % kLetters),
                                            kLower + (seed % kLetters),
                                            0xFA,
                                            0x06,
                                            1 + slot,
                                            0xE7};
    for (std::size_t index = 0; index < kNameBytes; ++index) {
      data.at(kNameSlots[slot] + index) =
          index < glyphs.size() ? static_cast<std::byte>(glyphs[index])
                                : std::byte{};
    }
  }
}

}  // namespace VS::test_support
