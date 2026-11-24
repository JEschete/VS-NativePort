#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/AbilityFoundationWorld.hpp"
#include "Menu/BattleAbilities.hpp"

namespace {
namespace abilities = VS::Menu::BattleAbilities;
using VS::Test::ability_foundation::Notifications;
using VS::Test::ability_foundation::Rows;
using VS::Test::ability_foundation::World;
using Phase = abilities::AbilityUnlockPhase;
using Type = abilities::AbilityMenuType;
using Kind = abilities::AbilityKind;
}  // namespace

TEST_CASE(
    "F02614 learns only the next eligible weapon art in notification order",
    "[menu][battle-abilities][F02614]") {
  World world;
  abilities::BreakArtUnlockState state;
  world.weapon = GENERATE(0, 1, 5, 10);
  const int category = world.weapon % 10;
  world.learned[category] = GENERATE(0, 3);
  const int tier = world.learned[category];
  world.kills[category] = 100;
  CHECK(abilities::UpdateBreakArtUnlock(state, world, world, true) == 0);
  CHECK(world.learned[category] == tier + 1);
  CHECK(world.message_action ==
        (category == 0 ? 220 : 184 + (category - 1) * 4) + tier);
  CHECK(world.message_weapon == (world.weapon == 0 ? 10 : world.weapon));
  CHECK(world.actions.at(world.message_action));
  CHECK(state.timeout == 120);
  CHECK(world.events == std::vector<std::string>{"box", "learn-art",
                                                 "art-message",
                                                 "unlock-action"});
}

TEST_CASE("F02614 leaves ineligible or exhausted progress untouched",
          "[menu][battle-abilities][F02614]") {
  World world;
  abilities::BreakArtUnlockState state{31};
  world.learned[1] = GENERATE(0, 4);
  world.kills[1] = 99;
  const auto before = world.learned;
  CHECK(abilities::UpdateBreakArtUnlock(state, world, world, true) == 1);
  CHECK(world.learned == before);
  CHECK(world.events.empty());
  CHECK(state.timeout == 31);
}

TEST_CASE("F02614 refuses invalid categories and tiers before indexing",
          "[menu][battle-abilities][F02614]") {
  World world;
  abilities::BreakArtUnlockState state;
  const auto invalid = GENERATE(-1, 11, 256);
  SECTION("weapon") { world.weapon = invalid; }
  SECTION("tier") { world.learned[1] = invalid; }
  CHECK(abilities::UpdateBreakArtUnlock(state, world, world, true) == 1);
  CHECK(world.events == std::vector<std::string>{"refuse"});
  CHECK(state.timeout == 0);
}

TEST_CASE("F02614 consumes timeout only after text finishes and honors input",
          "[menu][battle-abilities][F02614]") {
  World world;
  abilities::BreakArtUnlockState state{120};
  CHECK(abilities::UpdateBreakArtUnlock(state, world, world, false) == 0);
  CHECK(state.timeout == 120);
  world.done = true;
  for (int tick = 0; tick < 120; ++tick) {
    REQUIRE(abilities::UpdateBreakArtUnlock(state, world, world, false) == 0);
  }
  CHECK(abilities::UpdateBreakArtUnlock(state, world, world, false) == 1);
  state.timeout = 27;
  world.confirm = true;
  CHECK(abilities::UpdateBreakArtUnlock(state, world, world, false) == 1);
  CHECK(state.timeout == 27);
}

TEST_CASE("F02615 resolves distinct chain and defense description entries",
          "[menu][battle-abilities][F02615]") {
  const auto [action, index] = GENERATE(std::pair{24, 8}, std::pair{37, 19},
                                        std::pair{41, 21}, std::pair{53, 33});
  std::array<std::uint16_t, 100> strings{};
  strings[index] = 73;
  const auto text = abilities::UnlockDescription(action, strings, {}, 200);
  CHECK(text.data() == strings.data() + 73);
  CHECK(text.size() == 27);
}

TEST_CASE("F02615 falls back for unmapped actions without indexing their table",
          "[menu][battle-abilities][F02615]") {
  const int action =
      GENERATE(-1, 0, 23, 28, 54, std::numeric_limits<int>::max());
  std::array<std::uint16_t, 8> menu{};
  const auto text = abilities::UnlockDescription(action, {}, menu, 3);
  CHECK(text.data() == menu.data() + 3);
  CHECK(text.size() == 5);
}

TEST_CASE("F02615 refuses truncated index tables offsets and missing fallbacks",
          "[menu][battle-abilities][F02615]") {
  std::array<std::uint16_t, 40> strings{};
  strings[8] = 40;
  CHECK(abilities::UnlockDescription(24, std::span(strings).first(8), {}, 0)
            .empty());
  CHECK(abilities::UnlockDescription(24, strings, {}, 0).empty());
  CHECK(abilities::UnlockDescription(0, strings, {}, 0).empty());
}

TEST_CASE("F02616 collects locked actions in canonical reward order and resets",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  state.remaining_chain.fill(0xCC);
  state.phase = Phase::kShowMessage;
  world.total_kills = 100;
  world.actions[24] = true;
  world.actions[0x31] = true;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, true) == 0);
  CHECK(world.unlocked == 1);
  CHECK(state.chain_count == 10);
  CHECK(state.defense_count == 10);
  CHECK(std::ranges::equal(std::span(state.remaining_chain).first(4),
                           std::array<std::uint8_t, 4>{25, 34, 37, 35}));
  CHECK(state.remaining_chain[10] == 0xCC);
  CHECK(state.phase == Phase::kInit);
  CHECK(state.first_visit == 3);
  CHECK(world.events ==
        std::vector<std::string>{"earn-ability", "reset-memory"});
}

TEST_CASE("F02616 refuses unearned exhausted and invalid reward tiers",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  world.unlocked = GENERATE(0, 22, 23, -1);
  world.total_kills = world.unlocked == 0 ? 99 : 65535;
  const int before = world.unlocked;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, true) == 1);
  CHECK(world.unlocked == before);
  CHECK(state.timeout == 0);
  CHECK(std::ranges::none_of(world.actions, [](bool value) { return value; }));
}

TEST_CASE("F02616 chooses both categories or the completed message",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  const bool both = GENERATE(false, true);
  state.chain_count = both ? 1 : 0;
  state.defense_count = both ? 1 : 0;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(state.phase == (both ? Phase::kBothRemaining : Phase::kShowMessage));
  CHECK(world.events ==
        std::vector<std::string>{both ? "both" : "ability-message"});
  CHECK(world.all_unlocked == !both);
}

TEST_CASE("F02616 single category carries the original hoisted row geometry",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  const bool chain = GENERATE(false, true);
  state.chain_count = chain ? 1 : 0;
  state.defense_count = chain ? 0 : 1;
  REQUIRE(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  REQUIRE(world.category.has_value());
  if (!world.category) return;
  const auto& row = *world.category;
  CHECK(row.kind == (chain ? Kind::kChain : Kind::kDefense));
  const std::array placement{row.x, row.y, row.width, row.background_width,
                             row.target_x};
  const std::array expected{320, 18, 140, 8, 180};
  CHECK(placement == expected);
  CHECK(row.state == VS::Menu::Framework::RowState::kSlideX);
  CHECK(row.selected);
  CHECK(state.phase == (chain ? Phase::kChainOnly : Phase::kDefenseOnly));
}

TEST_CASE(
    "F02616 category selection distinguishes waiting cancellation and rows",
    "[menu][battle-abilities][F02616]") {
  const auto [row, next] = GENERATE(
      std::pair{-1, Phase::kBothRemaining}, std::pair{-2, Phase::kReinit},
      std::pair{0, Phase::kChainOnly}, std::pair{1, Phase::kDefenseOnly});
  World world;
  abilities::AbilityUnlockState state;
  state.phase = Phase::kBothRemaining;
  world.selected = row;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(state.phase == next);
  CHECK(world.events.size() == (row == -1 ? 0 : 1));
}

TEST_CASE("F02616 list pagination consumes each category first-visit bit once",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  const bool chain = GENERATE(false, true);
  const int count = GENERATE(1, 2, 4, 5, 11);
  state.phase = chain ? Phase::kChainOnly : Phase::kDefenseOnly;
  state.chain_count = static_cast<std::uint8_t>(count);
  state.defense_count = static_cast<std::uint8_t>(count);
  state.first_visit = 3;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(world.visible_rows == (count == 1 ? 1 : std::min(count - 1, 4)));
  CHECK(world.listed.size() == count);
  CHECK(world.forced_cursor);
  CHECK(state.first_visit == (chain ? 2 : 1));
  state.phase = chain ? Phase::kChainOnly : Phase::kDefenseOnly;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK_FALSE(world.forced_cursor);
}

TEST_CASE("F02616 waits for menu completion and retries refused presentation",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  state.chain_count = 1;
  state.phase = Phase::kChainOnly;
  state.first_visit = 3;
  world.ready = false;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(world.events.empty());
  world.ready = true;
  world.list_available = false;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(state.phase == Phase::kChainOnly);
  CHECK(state.first_visit == 3);
}

TEST_CASE(
    "F02616 confirmation uses the selected reward and preserves other flags",
    "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  const bool chain = GENERATE(false, true);
  state.phase = chain ? Phase::kUnlockChain : Phase::kUnlockDefense;
  state.remaining_chain[1] = 37;
  state.remaining_defense[1] = 53;
  state.chain_count = state.defense_count = 2;
  world.selected = 1;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(world.message_action == (chain ? 37 : 53));
  CHECK(world.actions.at(world.message_action));
  CHECK(std::ranges::count(world.actions, true) == 1);
  CHECK(state.phase == Phase::kShowMessage);
  CHECK(world.events == std::vector<std::string>{"clear", "box",
                                                 "ability-message",
                                                 "unlock-action"});
}

TEST_CASE("F02616 cancellation reenters only after rows finish flying out",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  state.phase = GENERATE(Phase::kUnlockChain, Phase::kUnlockDefense);
  world.selected = -2;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(state.phase == Phase::kReinit);
  world.ready = false;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(state.phase == Phase::kReinit);
  world.ready = true;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(state.phase == Phase::kInit);
  CHECK(world.unlocked == 0);
}

TEST_CASE("F02616 message timeout and acknowledgement wait for text completion",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  state.phase = Phase::kShowMessage;
  state.timeout = 1;
  world.confirm = true;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(state.timeout == 1);
  world.done = true;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 1);
  world.confirm = false;
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(state.timeout == 0);
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 1);
}

TEST_CASE("F02616 invalid state counts and selection never index beyond arrays",
          "[menu][battle-abilities][F02616]") {
  World world;
  abilities::AbilityUnlockState state;
  state.phase = Phase::kUnlockChain;
  SECTION("count") { state.chain_count = 17; }
  SECTION("selection") { world.selected = std::numeric_limits<int>::max(); }
  CHECK(abilities::UpdateAbilityUnlock(state, world, world, false) == 0);
  CHECK(world.events.back() == "refuse");
  CHECK(std::ranges::count(world.actions, true) == 0);
}

TEST_CASE("F02617 absent rewards initialize once then dismiss and reset",
          "[menu][battle-abilities][F02617]") {
  Notifications host;
  std::uint8_t state = 0;
  CHECK(abilities::UpdateUnlockNotifications(state, host) == 0);
  CHECK(state == 3);
  CHECK(abilities::UpdateUnlockNotifications(state, host) == 0);
  CHECK(state == 5);
  CHECK(abilities::UpdateUnlockNotifications(state, host) == 1);
  CHECK(state == 0);
  CHECK(host.calls == std::vector<int>{0, 5});
}

TEST_CASE(
    "F02617 repeats each reward family until its initializer is exhausted",
    "[menu][battle-abilities][F02617]") {
  Notifications host;
  const bool art = GENERATE(false, true);
  std::uint8_t state = art ? 1 : 3;
  host.art_pending = host.ability_pending = true;
  CHECK(abilities::UpdateUnlockNotifications(state, host) == 0);
  CHECK(state == (art ? 2 : 4));
  CHECK(abilities::UpdateUnlockNotifications(state, host) == 0);
  CHECK(state == (art ? 2 : 4));
  host.art_result = host.ability_result = 1;
  CHECK(abilities::UpdateUnlockNotifications(state, host) == 0);
  CHECK(state == (art ? 1 : 3));
  CHECK(abilities::UpdateUnlockNotifications(state, host) == 0);
  CHECK(state == (art ? 3 : 5));
}

TEST_CASE("F02617 busy dismissal and unknown state preserve the notification",
          "[menu][battle-abilities][F02617]") {
  Notifications host;
  auto state = static_cast<std::uint8_t>(GENERATE(5, 6, 255));
  const auto before = state;
  host.dismiss_result = 1;
  CHECK(abilities::UpdateUnlockNotifications(state, host) == 0);
  CHECK(state == before);
  CHECK(host.calls.size() == (state == 5 ? 1 : 0));
}

TEST_CASE("F02618 formats the entire unsigned-byte cost domain including zero",
          "[menu][battle-abilities][F02618]") {
  World world;
  abilities::RiskCostState state;
  for (int cost = 0; cost <= 255; ++cost) {
    world.costs.clear();
    abilities::SetRiskCost(state, static_cast<std::uint8_t>(cost), world);
    REQUIRE(world.costs.size() == 2);
    CHECK(world.costs[0].text == "RISK");
    CHECK(world.costs[0].index == 1);
    CHECK(world.costs[0].x == 8);
    CHECK(world.costs[1].text == "#" + std::to_string(cost));
    CHECK(world.costs[1].index == 0);
    CHECK(world.costs[1].x == 72);
    CHECK_FALSE(world.costs[1].disabled);
  }
}

TEST_CASE(
    "F02618 repeated shorter costs terminate without clearing unused bytes",
    "[menu][battle-abilities][F02618]") {
  World world;
  abilities::RiskCostState state;
  state.text.fill('X');
  abilities::SetRiskCost(state, 255, world);
  abilities::SetRiskCost(state, 0, world);
  CHECK(world.costs.back().text == "#0");
  CHECK(state.text[0] == 'X');
  CHECK(state.text[11] == '#');
  CHECK(state.text[12] == '2');
  CHECK(state.text[13] == '#');
  CHECK(state.text[14] == '0');
  CHECK(state.text.back() == '\0');
}

TEST_CASE("F02619 restores paging without selecting the scroll-arrow rows",
          "[menu][battle-abilities][F02619]") {
  const auto sample =
      GENERATE(std::array{7, 5, 1, 6, 0}, std::array{7, 5, 2, 0, 0},
               std::array{8, 7, 0, 7, 0}, std::array{8, 7, 1, 0, 0},
               std::array{9, 0, 1, 1, 0}, std::array{9, 7, 0, 6, 1},
               std::array{10, 2, 3, 3, 2}, std::array{16, 0, 8, 1, 7},
               std::array{5, 0, 255, 0, 0}, std::array{16, 255, 255, 0, 0});
  const auto type = GENERATE(Type::kChain, Type::kDefense);
  World world;
  Rows rows;
  abilities::AbilityRowsState state;
  world.remember_cursor = true;
  world.cursor = {sample[1], sample[2]};
  CHECK(abilities::CreateAbilityRows(state, world, sample[0], type,
                                     rows.text) == 1);
  CHECK(state.cursor.highlighted == sample[3]);
  CHECK(state.cursor.first == sample[4]);
  CHECK(world.risk_action ==
        (type == Type::kChain ? 24 : 40) + sample[3] + sample[4]);
  CHECK(world.allocation_words ==
        static_cast<std::size_t>(sample[0]) * abilities::kAbilityRowWords);
  CHECK(world.events ==
        std::vector<std::string>{"risk", "allocate", "publish"});
}

TEST_CASE(
    "F02619 copies fixed text windows and leaves following storage untouched",
    "[menu][battle-abilities][F02619]") {
  World world;
  Rows rows;
  abilities::AbilityRowsState state;
  REQUIRE(abilities::CreateAbilityRows(state, world, 2, Type::kChain,
                                       rows.text) == 1);
  CHECK(world.text[0] == 0x10FA);
  CHECK(
      std::ranges::equal(std::span(world.text).subspan(1, 14), rows.names[0]));
  CHECK(world.text[15] == 0xE7E7);
  CHECK(world.text[16] == 0xF8);
  CHECK(std::ranges::equal(std::span(world.text).subspan(17, 46),
                           rows.descriptions[0]));
  CHECK(world.text[63] == 0xE7E7);
  CHECK(world.text[128] == 0xCAFE);
}

TEST_CASE("F02619 absent name and description write only their terminators",
          "[menu][battle-abilities][F02619]") {
  World world;
  Rows rows;
  rows.text[0] = {};
  abilities::AbilityRowsState state;
  REQUIRE(abilities::CreateAbilityRows(state, world, 1, Type::kChain,
                                       rows.text) == 1);
  CHECK(world.text[0] == 0xE7E7);
  CHECK(world.text[16] == 0xE7E7);
  CHECK(world.text[1] == 0xCAFE);
  CHECK(world.text[15] == 0xCAFE);
  CHECK(world.text[17] == 0xCAFE);
  CHECK(world.text[63] == 0xCAFE);
}

TEST_CASE("F02619 disabled cursor memory and repeat entry ignore stale inputs",
          "[menu][battle-abilities][F02619]") {
  World world;
  Rows rows;
  abilities::AbilityRowsState state;
  world.cursor = {255, 255};
  CHECK(abilities::CreateAbilityRows(state, world, 1, Type::kChain,
                                     rows.text) == 1);
  CHECK(state.cursor.highlighted == 0);
  CHECK(state.cursor.first == 0);
  world.events.clear();
  CHECK(abilities::CreateAbilityRows(state, world, -1, Type::kChain, {}) == 0);
  CHECK(world.events.empty());
}

TEST_CASE(
    "F02619 refuses incomplete row inputs before allocating or publishing",
    "[menu][battle-abilities][F02619]") {
  World world;
  Rows rows;
  abilities::AbilityRowsState state;
  int count = 1;
  Type type = Type::kChain;
  SECTION("zero count") { count = 0; }
  SECTION("negative count") { count = -1; }
  SECTION("excess count") { count = 17; }
  SECTION("unselected type") { type = Type::kNone; }
  SECTION("short name") {
    rows.text[0].name = std::span(rows.names[0]).first(13);
  }
  SECTION("short description") {
    rows.text[0].description = std::span(rows.descriptions[0]).first(45);
  }
  CHECK(abilities::CreateAbilityRows(state, world, count, type, rows.text) ==
        0);
  CHECK(world.events == std::vector<std::string>{"refuse"});
  CHECK_FALSE(world.has_text);
}

TEST_CASE(
    "F02619 missing actions and allocation failure preserve previous state",
    "[menu][battle-abilities][F02619]") {
  World world;
  Rows rows;
  abilities::AbilityRowsState state;
  state.count = 9;
  SECTION("missing action") { world.missing_action = true; }
  SECTION("failed allocation") { world.fail_allocation = true; }
  SECTION("short allocation") { world.short_allocation = true; }
  CHECK(abilities::CreateAbilityRows(state, world, 1, Type::kDefense,
                                     rows.text) == 0);
  CHECK(state.count == 9);
  CHECK_FALSE(world.has_text);
  CHECK(world.events.back() == "refuse");
  CHECK(world.text[0] == 0xCAFE);
}
