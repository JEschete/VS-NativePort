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

}  // namespace VS::test_support
