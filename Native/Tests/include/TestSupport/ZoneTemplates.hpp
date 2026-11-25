#pragma once

#include <array>
#include <cstddef>

#include "Content/ZndFile.hpp"

namespace VS::test_support {

// Gives every equipment part's damage type, cost type and material a valid
// code, so a patterned template still formats to named part columns.
inline void UseValidPartCodes(Content::ZndTemplate& data, std::size_t seed) {
  // The weapon's blade, grip and gems, the shield and its gems, the accessory
  // and six limb armours, 0x30 bytes each.
  constexpr std::array<std::size_t, 16> kPartOffsets{
      0x34,  0x64,  0x94,  0xC4,  0xF4,  0x140, 0x170, 0x1A0,
      0x1D0, 0x204, 0x258, 0x2B4, 0x310, 0x36C, 0x3C8, 0x424};
  for (const std::size_t part : kPartOffsets) {
    data.at(part + 0x10) = static_cast<std::byte>((seed + part) % 4);
    data.at(part + 0x11) = static_cast<std::byte>((seed + part) % 5);
    data.at(part + 0x13) = static_cast<std::byte>((seed + part) % 8);
  }
}

// Gives the template's reserved bytes the values its tables rebuild:
// padding at 0x33, 0x236, 0x237 and each limb's +0x5A/+0x5B is zero, and each
// limb's +0x52 is 1 exactly when its armour material (+0x50) is set.
inline void UseReservedBytes(Content::ZndTemplate& data) {
  constexpr std::size_t kLimbsAt = 0x238;
  constexpr std::size_t kLimbBytes = 0x5C;
  data.at(0x33) = std::byte{};
  data.at(0x236) = std::byte{};
  data.at(0x237) = std::byte{};
  for (std::size_t limb = 0; limb < 6; ++limb) {
    const std::size_t base = kLimbsAt + (limb * kLimbBytes);
    data.at(base + 0x52) =
        data.at(base + 0x50) == std::byte{} ? std::byte{} : std::byte{1};
    data.at(base + 0x5A) = std::byte{};
    data.at(base + 0x5B) = std::byte{};
  }
}

}  // namespace VS::test_support
