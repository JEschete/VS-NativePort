// Hand-maintained tests for the typed effect texture block parser (#247).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/TextureBindings.hpp"
#include "Legacy/Psx.hpp"

namespace {
// One table at offset 8 holding one frame record, then two descriptors.
std::array<std::uint32_t, 15> OneTableBlock() {
  std::array<std::uint32_t, 15> words{};
  words[0] = 0x000C0001;  // one table, descriptors at 12
  words[1] = 8;           // table 0 offset
  words[2] = 0x00010001;  // one frame from descriptor 1
  return words;
}

using VS::Effects::PaletteReferenceCheck;
using VS::Effects::PaletteReferenceFault;
constexpr std::uint16_t kFourBitPage = 0;
constexpr std::uint16_t kEightBitPage = 1U << 7;
constexpr std::uint16_t kDirectPage = 2U << 7;

constexpr std::uint16_t Clut(std::uint32_t x, std::uint32_t row) {
  return static_cast<std::uint16_t>((row << 6) | (x / 16));
}

// OneTableBlock's palette check, with descriptor 0 sampling x 768 on row 240
// and descriptor 1 given `clut` and `page`.
PaletteReferenceCheck CheckDescriptor(
    std::uint16_t clut, std::uint16_t page,
    std::uint8_t rows = VS::Effects::kAllEffectPaletteRows) {
  auto words = OneTableBlock();
  words[3] = Clut(768, 240);
  words[9] = clut | (static_cast<std::uint32_t>(page) << 16);
  const auto bytes = std::as_bytes(std::span(words));
  return VS::Effects::CheckEffectPaletteReferences(
      bytes, VS::Effects::BindEffectTextureTables(bytes, {}), rows);
}

PaletteReferenceCheck Fault(PaletteReferenceFault fault, std::size_t descriptor,
                            std::uint16_t clut) {
  return {.fault = fault, .descriptor = descriptor, .clut = clut};
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

TEST_CASE("Effect texture descriptors sample loaded effect CLUT rows",
          "[effects][texture-binding][effect-palette]") {
  CHECK(CheckDescriptor(Clut(1008, 244), kFourBitPage) ==
        PaletteReferenceCheck{});
  CHECK(CheckDescriptor(Clut(768, 241), kEightBitPage) ==
        PaletteReferenceCheck{});
  // Bit 15 is the semi-transparency hint, not part of the row.
  CHECK(CheckDescriptor(static_cast<std::uint16_t>(Clut(768, 241) | 0x8000U),
                        kFourBitPage) == PaletteReferenceCheck{});
  // Direct colour samples no palette.
  CHECK(CheckDescriptor(0, kDirectPage) == PaletteReferenceCheck{});
  CHECK(CheckDescriptor(Clut(768, 240), kFourBitPage,
                        VS::Effects::kSystemPaletteRows) ==
        PaletteReferenceCheck{});
}

TEST_CASE("Effect texture descriptors outside their loaded palette are refused",
          "[effects][texture-binding][effect-palette]") {
  using enum PaletteReferenceFault;
  CHECK(CheckDescriptor(Clut(752, 241), kFourBitPage) ==
        Fault(kColumn, 1, Clut(752, 241)));
  CHECK(CheckDescriptor(Clut(784, 241), kEightBitPage) ==
        Fault(kColumn, 1, Clut(784, 241)));
  CHECK(CheckDescriptor(Clut(768, 239), kFourBitPage) ==
        Fault(kRow, 1, Clut(768, 239)));
  CHECK(CheckDescriptor(Clut(768, 245), kEightBitPage) ==
        Fault(kRow, 1, Clut(768, 245)));
  CHECK(CheckDescriptor(Clut(768, 241), kFourBitPage,
                        VS::Effects::kSystemPaletteRows) ==
        Fault(kUnbound, 1, Clut(768, 241)));
  // The first refused descriptor is reported.
  CHECK(CheckDescriptor(Clut(768, 245), kFourBitPage, 0) ==
        Fault(kUnbound, 0, Clut(768, 240)));
  CHECK(VS::Effects::DescribePaletteFault(kUnbound).find("row") !=
        std::string_view::npos);
}

TEST_CASE("The promoted texture binder has no original module address",
          "[effects][texture-binding]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800D7A14u) == nullptr);
}
