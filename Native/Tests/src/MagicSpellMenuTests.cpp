#include <array>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Menu/MagicMenusLive.hpp"
#include "Menu/MagicSpellMenu.hpp"

namespace {

using VS::Menu::Magic::ApplyGlobalSpellRestrictions;
using VS::Menu::Magic::AvailableSpellList;
using VS::Menu::Magic::BuildAvailableSpellList;
using VS::Menu::Magic::SpellCandidate;

constexpr std::size_t kShamanSpellCount = 6;
constexpr std::uint8_t kFirstShamanSpellId = 13;
constexpr int kRestrictionsEnabled = 1;
constexpr int kUnusedRowMarker = 0x40;
constexpr int kUnselectableRowType = 1;

TEST_CASE("F02604 warlock spell rows preserve their source order",
          "[magic-spell-menu][F02604]") {
  const std::array candidates{
      SpellCandidate{0, 7, true}, SpellCandidate{1, 8, false},
      SpellCandidate{2, 9, true}, SpellCandidate{3, 10, true}};
  const auto available = BuildAvailableSpellList(candidates);
  const AvailableSpellList rows = available.value_or(AvailableSpellList{});
  REQUIRE(available);
  REQUIRE(rows.count == 3);
  CHECK(rows.spells[0].source_index == 0);
  CHECK(rows.spells[1].source_index == 2);
  CHECK(rows.spells[2].action_id == 10);
}

TEST_CASE("F02605 shaman spell rows omit locked spells",
          "[magic-spell-menu][F02605]") {
  const std::array candidates{
      SpellCandidate{0, 13, false}, SpellCandidate{1, 14, true},
      SpellCandidate{2, 15, false}, SpellCandidate{3, 16, true},
      SpellCandidate{4, 17, false}, SpellCandidate{5, 18, true}};
  const auto available = BuildAvailableSpellList(candidates);
  const AvailableSpellList rows = available.value_or(AvailableSpellList{});
  REQUIRE(available);
  REQUIRE(rows.count == 3);
  CHECK(rows.spells[0].action_id == 14);
  CHECK(rows.spells[1].source_index == 3);
  CHECK(rows.spells[2].source_index == 5);
}

TEST_CASE("F02606 sorcerer spell rows retain all unlocked entries",
          "[magic-spell-menu][F02606]") {
  std::array<SpellCandidate, 18> candidates{};
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    candidates[i] = {static_cast<std::uint8_t>(i),
                     static_cast<std::uint8_t>(30 + i), true};
  }
  const auto available = BuildAvailableSpellList(candidates);
  const AvailableSpellList rows = available.value_or(AvailableSpellList{});
  REQUIRE(available);
  REQUIRE(rows.count == candidates.size());
  CHECK(rows.spells.front().action_id == 30);
  CHECK(rows.spells.back().source_index == 17);
}

TEST_CASE("F02607 enchanter spell rows handle an empty learned list",
          "[magic-spell-menu][F02607]") {
  const std::array candidates{
      SpellCandidate{0, 48, false}, SpellCandidate{1, 49, false},
      SpellCandidate{2, 50, false}, SpellCandidate{3, 51, false},
      SpellCandidate{4, 52, false}, SpellCandidate{5, 53, false},
      SpellCandidate{6, 54, false}, SpellCandidate{7, 55, false}};
  const auto available = BuildAvailableSpellList(candidates);
  const AvailableSpellList rows = available.value_or(AvailableSpellList{});
  REQUIRE(available);
  CHECK(rows.count == 0);
}

TEST_CASE("F02605 shaman restrictions cover only populated spell rows",
          "[magic-spell-menu][F02605]") {
  const std::array candidates{
      SpellCandidate{0, kFirstShamanSpellId, true},
      SpellCandidate{1, kFirstShamanSpellId + 1, false},
      SpellCandidate{2, kFirstShamanSpellId + 2, false},
      SpellCandidate{3, kFirstShamanSpellId + 3, false},
      SpellCandidate{4, kFirstShamanSpellId + 4, false},
      SpellCandidate{5, kFirstShamanSpellId + 5, false}};
  const auto available = BuildAvailableSpellList(candidates);
  const AvailableSpellList available_rows =
      available.value_or(AvailableSpellList{});
  REQUIRE(available);
  REQUIRE(available_rows.count == 1);

  std::array<int, kShamanSpellCount> row_types;
  row_types.fill(kUnusedRowMarker);
  ApplyGlobalSpellRestrictions(available_rows, std::span<int>(row_types),
                               kRestrictionsEnabled);

  CHECK(row_types[0] == (kUnusedRowMarker | kUnselectableRowType));
  for (std::size_t row = available_rows.count; row < row_types.size(); ++row) {
    CHECK(row_types[row] == kUnusedRowMarker);
  }
}

TEST_CASE("F02604 warlock init enters through the live menu",
          "[magic-spell-menu][F02604]") {
  CHECK(VS::Menu::Magic::UpdateWarlockMenuLive(1) == 0);
}

TEST_CASE("F02605 shaman init enters through the live menu",
          "[magic-spell-menu][F02605]") {
  CHECK(VS::Menu::Magic::UpdateShamanMenuLive(1) == 0);
}

TEST_CASE("F02606 sorcerer init enters through the live menu",
          "[magic-spell-menu][F02606]") {
  CHECK(VS::Menu::Magic::UpdateSorcererMenuLive(1) == 0);
}

TEST_CASE("F02607 enchanter init enters through the live menu",
          "[magic-spell-menu][F02607]") {
  CHECK(VS::Menu::Magic::UpdateEnchanterMenuLive(1) == 0);
}

TEST_CASE("magic spell rows reject unsupported capacity",
          "[magic-spell-menu]") {
  std::array<SpellCandidate, AvailableSpellList::kCapacity + 1> candidates{};
  CHECK_FALSE(BuildAvailableSpellList(candidates).has_value());
}

}  // namespace
