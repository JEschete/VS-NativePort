// Hand-maintained tests for the typed effect texture block parser (#247).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Effects/TextureBindings.hpp"
#include "Legacy/Psx.h"

namespace {
// One table at offset 8 holding one frame record, then two descriptors.
std::array<std::uint32_t, 15> OneTableBlock() {
  std::array<std::uint32_t, 15> words{};
  words[0] = 0x000C0001;  // one table, descriptors at 12
  words[1] = 8;           // table 0 offset
  words[2] = 0x00010001;  // one frame from descriptor 1
  return words;
}
}  // namespace

TEST_CASE("Texture block layouts report validated table offsets",
          "[effects][texture-binding]") {
  const auto words = OneTableBlock();
  const auto layout =
      VS::Effects::BindEffectTextureTables(std::as_bytes(std::span(words)), {});
  CHECK(layout.table_count == 1);
  CHECK(layout.table_offsets[0] == 8);
  CHECK(layout.table_offsets[1] == 0);
  CHECK(layout.descriptor_offset == 12);
}

TEST_CASE("Only the action package's Gravity block shares channel 0",
          "[effects][texture-binding][gravity-material]") {
  const auto words = OneTableBlock();
  const auto bytes = std::as_bytes(std::span(words));
  const auto gravity = VS::Effects::BindEffectTextureTables(
      bytes, {true, VS::Effects::kGravityEffectId});
  CHECK(gravity.table_count == 2);
  CHECK(gravity.table_offsets[1] == gravity.table_offsets[0]);
  const auto secondary = VS::Effects::BindEffectTextureTables(
      bytes, {false, VS::Effects::kGravityEffectId});
  CHECK(secondary.table_count == 1);
  const auto other = VS::Effects::BindEffectTextureTables(bytes, {true, 53});
  CHECK(other.table_count == 1);
}

TEST_CASE("The promoted texture binder has no original module address",
          "[effects][texture-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800D7A14u) == nullptr);
}
