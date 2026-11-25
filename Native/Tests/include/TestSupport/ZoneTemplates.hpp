#pragma once

#include <array>
#include <cstddef>

#include "Content/ZndFile.hpp"

namespace VS::test_support {

// The weapon's blade, grip and gems, the shield and its gems, the accessory
// and six limb armours, 0x30 bytes each.
inline constexpr std::array<std::size_t, 16> kTemplatePartOffsets{
    0x34,  0x64,  0x94,  0xC4,  0xF4,  0x140, 0x170, 0x1A0,
    0x1D0, 0x204, 0x258, 0x2B4, 0x310, 0x36C, 0x3C8, 0x424,
};

// Gives every equipment part's damage type, cost type and material a valid
// code, so a patterned template still formats to named part columns.
inline void UseValidPartCodes(Content::ZndTemplate& data, std::size_t seed) {
  for (const std::size_t part : kTemplatePartOffsets) {
    data.at(part + 0x10) = static_cast<std::byte>((seed + part) % 4);
    data.at(part + 0x11) = static_cast<std::byte>((seed + part) % 5);
    data.at(part + 0x13) = static_cast<std::byte>((seed + part) % 8);
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
// padding at 0x33, 0x236, 0x237 and each limb's +0x5A/+0x5B is zero, each
// limb's +0x52 is 1 exactly when its armour material (+0x50) is set, each
// action slot's index byte (+0x13, 4 bytes apart) holds the slot's position,
// the equipment indices (0x126-0x127, 0x202-0x203, 0x235, each limb's +0x53
// and each part's +0x17) are zero, and the ZUD index (0x460, 4 bytes, little
// endian) is the template's `template_slot`.
inline void UseReservedBytes(Content::ZndTemplate& data,
                             std::size_t template_slot = 0) {
  constexpr std::array<std::size_t, 8> kZeroBytes{0x33,  0x126, 0x127, 0x202,
                                                  0x203, 0x235, 0x236, 0x237};
  for (const std::size_t at : kZeroBytes) data.at(at) = std::byte{};
  for (std::size_t limb = 0; limb < 6; ++limb) {
    const std::size_t base = kTemplateLimbsAt + (limb * kTemplateLimbBytes);
    for (std::size_t slot = 0; slot < kLimbActionSlots; ++slot) {
      data.at(base + 0x13 + (slot * 4)) = static_cast<std::byte>(slot);
    }
    data.at(base + 0x52) =
        data.at(base + 0x50) == std::byte{} ? std::byte{} : std::byte{1};
    data.at(base + 0x53) = std::byte{};
    data.at(base + 0x5A) = std::byte{};
    data.at(base + 0x5B) = std::byte{};
  }
  for (const std::size_t part : kTemplatePartOffsets) {
    data.at(part + 0x17) = std::byte{};
  }
  for (std::size_t index = 0; index < 4; ++index) {
    data.at(0x460 + index) =
        static_cast<std::byte>((template_slot >> (8 * index)) & 0xFF);
  }
}

}  // namespace VS::test_support
