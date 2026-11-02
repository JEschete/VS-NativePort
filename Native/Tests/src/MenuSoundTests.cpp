#include <algorithm>
#include <array>
#include <cstring>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Menu/Sounds.hpp"

extern "C" {
#include "Content/ModuleData.h"
}

TEST_CASE("Menu sounds keep the original sound-effect ids and channel group",
          "[menu][sounds]") {
  using VS::Menu::MenuSound;
  STATIC_CHECK(static_cast<int>(MenuSound::kChange) == 4);
  STATIC_CHECK(static_cast<int>(MenuSound::kSelect) == 5);
  STATIC_CHECK(static_cast<int>(MenuSound::kLeave) == 6);
  STATIC_CHECK(static_cast<int>(MenuSound::kInvalid) == 7);
  STATIC_CHECK(static_cast<int>(MenuSound::kOpen) == 10);
  STATIC_CHECK(VS::Menu::kMenuSoundChannelGroup == 0x7E);
}

TEST_CASE("Menu sound helpers have no original-address module entries",
          "[menu][sounds][retirement]") {
  // No disc module stores these BATTLE.PRG menu sound helpers as data words;
  // the original reaches them only through jal instructions.
  constexpr std::array<unsigned, 5> kOriginalEntries{
      0x800C02D8U, 0x800C02E0U, 0x800C02E8U, 0x800C02F0U, 0x800C02F8U};
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK_FALSE(symbols.empty());
  for (const auto& symbol : symbols) {
    if (std::strcmp(symbol.module, "BATTLE/BATTLE.PRG") != 0) {
      continue;
    }
    CHECK(std::ranges::find(kOriginalEntries, symbol.address) ==
          kOriginalEntries.end());
  }
}
