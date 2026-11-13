#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Items/ItemRecords.hpp"

namespace container_detail_helpers_tests::samples {
inline constexpr std::uint8_t kTitle = 7;
inline constexpr std::uint8_t kIcons = 8;
inline constexpr std::uint8_t kControls = 9;
inline constexpr std::uint8_t kOldPhase = 99;
inline constexpr int kOldTriangle = 0x12345678;
inline constexpr std::uint8_t kOldWeaponIndex = 91;
inline constexpr std::uint8_t kOldShieldIndex = 87;
inline constexpr std::uint8_t kReservedByte = 0xA5;
inline constexpr int kReservedHeader = 98;
inline constexpr int kReservedHeaderTail = 71;
inline constexpr std::int16_t kRetainedClassTotal = 1901;
inline constexpr std::uint8_t kUpdatedIcons = 77;
inline constexpr std::uint8_t kFlagOnlyGem = 128;
inline constexpr std::uint8_t kByteMax = 255;
inline constexpr std::uint16_t kHalfwordToken = 65535;
inline constexpr std::uint16_t kHighToken = 0x8001;
inline constexpr std::uint16_t kByteOverflowToken = 256;
inline constexpr std::size_t kMaxOrderRows = 256;
inline constexpr std::size_t kFlaggedGemIndex = 126;
inline constexpr std::uint8_t kInvalidGrip = 65;
inline constexpr std::size_t kMiscCategory = 6;
inline constexpr std::size_t kCursorBase = 0xA2;
inline constexpr int kClassAffinityPages = 3;
inline constexpr int kTypePage = 4;
inline constexpr int kBarsOut = 4;
inline constexpr std::uint8_t kHeaderWidth = 164;
inline constexpr int kOtherActiveRow = 3;
inline constexpr std::uint32_t kVariableDrawMode = 0xE100002D;
inline constexpr std::uint32_t kVariableColor = 0x66808080;
inline constexpr std::uint32_t kVariableSize = 12 | (12 << 16);
inline constexpr std::uint32_t kGlyphCell = 12;
inline constexpr std::uint32_t kClutBase = 0x3780;
inline constexpr std::uint32_t kClutBias = 0x34;
inline constexpr std::uint32_t kClutMask = 0x3F;
inline constexpr int kTabX = 0x22;
inline constexpr int kTabY = 0x30;
inline constexpr char kFinalSlideStep = 9;
inline constexpr std::size_t kCompleteClassPageSprites = 59;
inline constexpr auto kTabGlyphs = [] {
  constexpr std::array<std::uint8_t, 19> kGlyphs{12,  21, 10, 28, 28, 161, 10,
                                                 15,  15, 18, 23, 18, 29,  34,
                                                 161, 29, 34, 25, 14};
  return kGlyphs;
}();
inline constexpr auto kTabPalettes = [] {
  constexpr std::array<std::uint8_t, kTabGlyphs.size()> kPalettes{
      5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 7, 7, 7, 7};
  return kPalettes;
}();
inline constexpr auto kBlade = [] {
  constexpr auto kSample = VS::Items::InventoryBlade{
      .id = 41, .strength = -8, .intelligence = 12, .material = 0x1234};
  return kSample;
}();
inline constexpr auto kGrip = [] {
  constexpr auto kSample = VS::Items::InventoryGrip{
      .id = 300, .strength = 5, .types = {-7, 0, 0, 0}};
  return kSample;
}();
inline constexpr VS::Items::InventoryGem kWeaponGem{.id = 511, .strength = 11};
inline constexpr VS::Items::InventoryGem kShieldGem{.id = 500, .strength = -9};
inline constexpr auto kWeapon = [] {
  constexpr auto kParts = VS::Items::InventoryWeapon{
      .index = 0xFE, .blade = 64, .grip = 64, .gems = {192, 0, 192, 255}};
  auto weapon = kParts;
  for (std::size_t i = 0; i < weapon.name.size(); ++i) {
    weapon.name[i] = static_cast<char>(i + 1);
  }
  return weapon;
}();
inline constexpr auto kShield = [] {
  constexpr auto kSample =
      VS::Items::InventoryShield{.index = 0xFF,
                                 .base = {.id = 31,
                                          .strength = 12,
                                          .types = {-1, 2, -3, 4},
                                          .material = 0x1234},
                                 .gems = {255, 0, 127, 128}};
  return kSample;
}();
}  // namespace container_detail_helpers_tests::samples
