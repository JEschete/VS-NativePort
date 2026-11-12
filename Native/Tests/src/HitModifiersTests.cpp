#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/HitModifiers.hpp"
#include "Battle/HitModifiersLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
}

namespace {
namespace battle = VS::Battle;
using Event = std::array<int, 2>;

struct World final : battle::HitModifierWorld {
  std::array<std::optional<battle::HitModifierStats>, 42> stats{};
  std::array<std::optional<std::uint32_t>, 42> drawn{};
  std::optional<battle::HitModifierAction> action = battle::HitModifierAction{};
  std::vector<Event> reads;
  World() {
    stats.at(0) = battle::HitModifierStats{};
    drawn.fill(1);
  }
  std::optional<battle::HitModifierStats> Stats(std::uint8_t slot) override {
    reads.push_back({0, slot});
    return slot < stats.size() ? stats.at(slot) : std::nullopt;
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    reads.push_back({1, slot});
    return slot < drawn.size() ? drawn.at(slot) : std::nullopt;
  }
  std::optional<battle::HitModifierAction> Action() override {
    reads.push_back({2, 0});
    return action;
  }
};

template <typename T>
auto Bytes(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

struct LiveFixture {
  decltype(std::to_array(Battle_actors)) saved = std::to_array(Battle_actors);
  decltype(std::to_array(Battle_hitFunctions)) callbacks =
      std::to_array(Battle_hitFunctions);
  std::uint32_t previous = Battle_previousHitResult;
  std::array<Battle_Actor, 3> actors{};
  std::array<Battle_ActorStats, 2> stats{};
  Battle_ActionDefinition action{};
  Battle_HitResult source{};
  Battle_HitResult target{};
  LiveFixture() {
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
    Battle_actors[0] = &actors.at(0);
    Battle_actors[1] = &actors.at(1);
    Battle_actors[41] = &actors.at(2);
    actors.at(0).stats = &stats.at(0);
    actors.at(1).stats = &stats.at(1);
    actors.at(2).weaponDrawn = 1;
    stats.at(0).actorId = 41;
    stats.at(1).actorId = 41;
    source.target.targetActor = 0;
    target.target.targetActor = 1;
  }
  ~LiveFixture() {
    std::copy(saved.begin(), saved.end(), Battle_actors);
    std::copy(callbacks.begin(), callbacks.end(), Battle_hitFunctions);
    Battle_previousHitResult = previous;
  }
};

std::vector<std::uint8_t> ReadOriginalBattleModule() {
  const std::filesystem::path root = "Native\\data";
  if (!std::filesystem::exists(root / VS::Content::DataStore::ManifestName)) {
    SKIP("Extracted Native/data is unavailable");
  }
  REQUIRE(VS::Content::DataStore::Open(root));
  auto& store = *VS::Content::DataStore::Get();
  const auto* module = store.Find("BATTLE/BATTLE.PRG");
  REQUIRE(module != nullptr);
  auto bytes = store.ReadAll(*module);
  REQUIRE(bytes.has_value());
  return bytes.value();
}

void RelocateHits(std::span<const std::uint8_t> image) {
  const auto tables =
      std::span(Content_modulePointers, Content_modulePointerCount);
  const auto table =
      std::ranges::find(tables, reinterpret_cast<void**>(Battle_hitFunctions),
                        &Content_ModulePointerTable::target);
  REQUIRE(table != tables.end());
  REQUIRE(VS::Content::RelocateModulePointerTable(
      *table, image, {Content_moduleSymbols, Content_moduleSymbolCount}));
}
}  // namespace

TEST_CASE("Attack gems distinguish all action types and gem codes",
          "[hit-modifiers][F00339]") {
  World world;
  battle::HitModifierStats stats;
  auto& action = world.action.emplace();
  for (unsigned type = 0; type < 8; ++type) {
    action.type = static_cast<std::uint8_t>(type);
    for (unsigned gem = 0; gem < 256; ++gem) {
      stats.weapon_gems.fill(static_cast<std::uint8_t>(gem));
      CHECK(battle::AttackGemHitBonus(&stats, world) ==
            (gem == (type == 1 ? 2U : 1U) ? 20 : 0));
    }
  }
}

TEST_CASE("Attack gems inspect all three slots without stacking",
          "[hit-modifiers][F00339]") {
  World world;
  battle::HitModifierStats stats;
  const auto slot = GENERATE(0U, 1U, 2U);
  stats.weapon_gems.at(slot) = 1;
  CHECK(battle::AttackGemHitBonus(&stats, world) == 20);
  stats.weapon_gems.fill(1);
  CHECK(battle::AttackGemHitBonus(&stats, world) == 20);
}

TEST_CASE("Attack gems resolve stats identity before deferred action",
          "[hit-modifiers][F00339]") {
  World world;
  battle::HitModifierStats stats;
  stats.actor_id = 41;
  stats.weapon_gems.fill(1);
  world.drawn.at(0) = 1;
  world.drawn.at(41) = GENERATE(0U, 2U, 0xFFFFFFFEU);
  world.action.reset();
  CHECK(battle::AttackGemHitBonus(&stats, world) == 0);
  CHECK(world.reads == std::vector<Event>{{1, 41}});
  world.drawn.at(41) = 3;
  CHECK(battle::AttackGemHitBonus(&stats, world) == std::nullopt);
}

TEST_CASE("Attack gems refuse only required stats and drawn owners",
          "[hit-modifiers][F00339]") {
  World world;
  battle::HitModifierStats stats;
  CHECK(battle::AttackGemHitBonus(nullptr, world) == std::nullopt);
  CHECK(world.reads.empty());
  stats.actor_id = GENERATE(42, 128, 255);
  CHECK(battle::AttackGemHitBonus(&stats, world) == std::nullopt);
  stats.actor_id = 41;
  world.drawn.at(41).reset();
  CHECK(battle::AttackGemHitBonus(&stats, world) == std::nullopt);
}

TEST_CASE("Defense base gems exhaust all IDs types and both hits",
          "[hit-modifiers][F00340]") {
  World world;
  auto& action = world.action.emplace();
  battle::HitModifierStats stats;
  const auto gem = GENERATE(5, 6, 0, 255);
  stats.shield_gems.fill(static_cast<std::uint8_t>(gem));
  for (unsigned type = 0; type < 8; ++type) {
    action.type = static_cast<std::uint8_t>(type);
    for (unsigned id = 0; id < 256; ++id) {
      action.id = static_cast<std::uint8_t>(id);
      const bool allowed = id < 40 || (id >= 54 && id <= 223);
      const bool applies = type == 1 ? gem == 6 : gem == 5 && allowed;
      CHECK(battle::DefenseGemHitBonus(&stats, 0, world) == (applies ? 20 : 0));
      CHECK(battle::DefenseGemHitBonus(&stats, 1, world) == (applies ? 20 : 0));
    }
  }
}

TEST_CASE("Defense effect gems exhaust all effects and gem codes",
          "[hit-modifiers][F00340]") {
  World world;
  auto& action = world.action.emplace();
  action.id = 40;
  battle::HitModifierStats stats;
  constexpr std::array<unsigned, 8> kEffects{1, 2, 3, 5, 11, 15, 16, 17};
  constexpr std::array<unsigned, 8> kGems{11, 12, 13, 14, 16, 18, 19, 21};
  for (unsigned effect = 0; effect < 128; ++effect) {
    action.effects.fill(static_cast<std::uint8_t>(effect));
    for (unsigned gem = 0; gem < 256; ++gem) {
      stats.shield_gems.fill(static_cast<std::uint8_t>(gem));
      bool applies = false;
      for (std::size_t i = 0; i < kEffects.size(); ++i) {
        applies |= effect == kEffects.at(i) && gem == kGems.at(i);
      }
      CHECK(battle::DefenseGemHitBonus(&stats, 0, world) == (applies ? 20 : 0));
      CHECK(battle::DefenseGemHitBonus(&stats, 1, world) == (applies ? 20 : 0));
    }
  }
}

TEST_CASE(
    "Defense effect presence covers every slot without duplicate stacking",
    "[hit-modifiers][F00340]") {
  World world;
  battle::HitModifierStats stats;
  const auto slot = GENERATE(0U, 1U, 2U);
  world.action.emplace().effects = {1, 2};
  stats.shield_gems.at(slot) = 11;
  CHECK(battle::DefenseGemHitBonus(&stats, 0, world) == 20);
  CHECK(battle::DefenseGemHitBonus(&stats, 1, world) == 0);
  stats.shield_gems.fill(11);
  CHECK(battle::DefenseGemHitBonus(&stats, 0, world) == 20);
  stats.shield_gems.at((slot + 1) % 3) = 5;
  CHECK(battle::DefenseGemHitBonus(&stats, 0, world) == 40);
}

TEST_CASE("Defense gems defer action and hit validation until drawn",
          "[hit-modifiers][F00340]") {
  World world;
  battle::HitModifierStats stats;
  stats.actor_id = 41;
  world.drawn.at(41) = 2;
  world.action.reset();
  const int hit = GENERATE(-1, 2, std::numeric_limits<int>::min(),
                           std::numeric_limits<int>::max());
  CHECK(battle::DefenseGemHitBonus(&stats, hit, world) == 0);
  CHECK(world.reads == std::vector<Event>{{1, 41}});
  world.drawn.at(41) = 1;
  CHECK(battle::DefenseGemHitBonus(&stats, hit, world) == std::nullopt);
  world.action.emplace();
  CHECK(battle::DefenseGemHitBonus(&stats, hit, world) == std::nullopt);
}

TEST_CASE("Defense gems refuse missing stats or drawn actor",
          "[hit-modifiers][F00340]") {
  World world;
  battle::HitModifierStats stats;
  CHECK(battle::DefenseGemHitBonus(nullptr, 0, world) == std::nullopt);
  world.drawn.at(0).reset();
  CHECK(battle::DefenseGemHitBonus(&stats, 0, world) == std::nullopt);
  CHECK(world.reads == std::vector<Event>{{1, 0}});
}

TEST_CASE("Previous hit threshold accepts every nonzero word",
          "[hit-modifiers][F00342]") {
  CHECK(battle::PreviousHitSuccessThreshold(0) == 255);
  for (unsigned bit = 0; bit < 32; ++bit) {
    const auto value = std::bit_cast<std::int32_t>(1U << bit);
    CHECK(battle::PreviousHitSuccessThreshold(value) == 100);
    CHECK(battle::PreviousHitSuccessThreshold(~value) == 100);
  }
  CHECK(battle::PreviousHitSuccessThreshold(-1) == 100);
}

TEST_CASE("Gem chance ignores nullable or position-only hits",
          "[hit-modifiers][F00343]") {
  World world;
  world.action.reset();
  battle::HitModifierTarget target{255, -128, GENERATE(1, -1, 0x10000)};
  CHECK(battle::GemHitChance(nullptr, nullptr, -1, world) == 100);
  CHECK(battle::GemHitChance(&target, &target, -1, world) == 100);
  CHECK(world.reads.empty());
}

TEST_CASE("Gem chance adds source then subtracts target and allows aliasing",
          "[hit-modifiers][F00343]") {
  World world;
  auto& stats = world.stats.at(0).emplace();
  stats.actor_id = 41;
  stats.weapon_gems.fill(1);
  stats.shield_gems = {5, 11, 11};
  world.action.emplace().effects.at(1) = 1;
  battle::HitModifierTarget hit{};
  CHECK(battle::GemHitChance(&hit, &hit, 1, world) == 80);
  CHECK(world.reads ==
        std::vector<Event>{{0, 0}, {1, 41}, {2, 0}, {0, 0}, {1, 41}, {2, 0}});
  CHECK(battle::GemHitChance(&hit, nullptr, -1, world) == 120);
  CHECK(battle::GemHitChance(nullptr, &hit, 1, world) == 60);
}

TEST_CASE("Gem chance refuses required stats action and target index lazily",
          "[hit-modifiers][F00343]") {
  World world;
  battle::HitModifierTarget hit{41, 0, 0};
  CHECK(battle::GemHitChance(&hit, nullptr, 0, world) == std::nullopt);
  CHECK(battle::GemHitChance(nullptr, &hit, 0, world) == std::nullopt);
  hit.actor = 0;
  world.action.reset();
  CHECK(battle::GemHitChance(&hit, nullptr, -1, world) == std::nullopt);
  world.drawn.at(0) = 0;
  CHECK(battle::GemHitChance(&hit, &hit, -1, world) == 100);
  world.action.emplace();
  world.drawn.at(0) = 1;
  CHECK(battle::GemHitChance(&hit, nullptr, -1, world) == 100);
  CHECK(battle::GemHitChance(nullptr, &hit, -1, world) == std::nullopt);
}

TEST_CASE("Risk modifier exhausts unsigned halfwords and corrects only 255",
          "[hit-modifiers][F00347]") {
  World world;
  auto& stats = world.stats.at(0).emplace();
  battle::HitModifierTarget hit{0, -128, -1};
  world.action.reset();
  world.drawn.fill(std::nullopt);
  for (unsigned risk = 0; risk <= 65535; ++risk) {
    stats.risk = static_cast<std::uint16_t>(risk);
    const int expected = static_cast<int>((risk + 150) * 100 / 256);
    CHECK(battle::RiskHitChance(&hit, world) ==
          (expected == 255 ? 254 : expected));
  }
}

TEST_CASE("Risk modifier refuses missing required source and stats",
          "[hit-modifiers][F00347]") {
  World world;
  battle::HitModifierTarget hit{
      static_cast<std::uint8_t>(GENERATE(1, 41, 42, 255)), 0, 1};
  CHECK(battle::RiskHitChance(nullptr, world) == std::nullopt);
  CHECK(world.reads.empty());
  CHECK(battle::RiskHitChance(&hit, world) == std::nullopt);
}

TEST_CASE("Chain modifier exhausts unsigned evasion at every limb",
          "[hit-modifiers][F00348]") {
  World world;
  auto& stats = world.stats.at(0).emplace();
  battle::HitModifierTarget hit{0, 0, -1};
  world.drawn.fill(std::nullopt);
  world.action.reset();
  for (std::int8_t limb = 0; limb < 6; ++limb) {
    hit.limb = limb;
    for (unsigned evasion = 0; evasion < 256; ++evasion) {
      stats.chain_evasion.at(static_cast<std::size_t>(limb)) =
          static_cast<std::uint8_t>(evasion);
      CHECK(battle::ChainHitChance(&hit, world) ==
            static_cast<int>((255 - evasion) * 100 / 255));
    }
  }
}

TEST_CASE("Chain modifier refuses signed invalid limbs and missing target",
          "[hit-modifiers][F00348]") {
  World world;
  battle::HitModifierTarget hit{
      0, static_cast<std::int8_t>(GENERATE(-128, -1, 6, 127)), 1};
  CHECK(battle::ChainHitChance(&hit, world) == std::nullopt);
  CHECK(battle::ChainHitChance(nullptr, world) == std::nullopt);
  hit.limb = 0;
  hit.actor = 255;
  CHECK(battle::ChainHitChance(&hit, world) == std::nullopt);
}

TEST_CASE("Live attack gems use stats owner and ignore absent equipment IDs",
          "[hit-modifiers][F00339][live]") {
  LiveFixture f;
  const auto slot = GENERATE(0, 1, 2);
  f.stats.at(0).weapon.gems[slot].gemEffects = 1;
  CHECK(battle::AttackLiveGemHitBonus(&f.action, &f.stats.at(0)) == 20);
  f.actors.at(2).weaponDrawn = 2;
  CHECK(battle::AttackLiveGemHitBonus(nullptr, &f.stats.at(0)) == 0);
}

TEST_CASE(
    "Live defense gems stop before a fourth candidate and ignore equipment IDs",
    "[hit-modifiers][F00340][live]") {
  LiveFixture f;
  f.action.hits[1].effectId = 17;
  // A fourth 0x30-stride gem probe would read the low byte of baseAgility.
  f.stats.at(1).shield.baseAgility = 21;
  CHECK(battle::DefenseLiveGemHitBonus(&f.action, &f.stats.at(1), 1) == 0);
  f.stats.at(1).shield.gems[2].gemEffects = 21;
  f.stats.at(1).shield.gems[0].gemEffects = 5;
  CHECK(battle::DefenseLiveGemHitBonus(&f.action, &f.stats.at(1), 1) == 40);
  f.actors.at(2).weaponDrawn = 0;
  CHECK(battle::DefenseLiveGemHitBonus(nullptr, &f.stats.at(1), -1) == 0);
}

TEST_CASE("Live previous hit threshold ignores every callback argument",
          "[hit-modifiers][F00342][live]") {
  LiveFixture f;
  Battle_previousHitResult =
      GENERATE(0, 1, -1, std::numeric_limits<int>::min());
  CHECK(battle::PreviousLiveHitSuccessThreshold(nullptr, nullptr, nullptr, -1,
                                                -1) ==
        (Battle_previousHitResult == 0 ? 255 : 100));
}

TEST_CASE("Live gem chance preserves aliased state and ignores inactive actors",
          "[hit-modifiers][F00343][live]") {
  LiveFixture f;
  f.stats.at(0).weapon.gems[0].gemEffects = 1;
  f.stats.at(0).shield.gems[0].gemEffects = 5;
  f.stats.at(0).shield.gems[1].gemEffects = 11;
  f.action.hits[0].effectId = 1;
  const auto before = Bytes(f.stats);
  CHECK(battle::LiveGemHitChance(&f.action, &f.source, &f.source, 0, -1) == 80);
  CHECK(Bytes(f.stats) == before);
  f.target.target.targetActor = 255;
  f.target.targetsPosition = -1;
  CHECK(battle::LiveGemHitChance(nullptr, &f.target, &f.target, -1, 0) == 100);
}

TEST_CASE("Live risk uses hit slot rather than stats actor identity",
          "[hit-modifiers][F00347][live]") {
  LiveFixture f;
  constexpr std::array<unsigned, 7> kRisk{0, 502, 503, 504, 505, 506, 65535};
  constexpr std::array<int, 7> kRates{58, 254, 254, 254, 254, 256, 25658};
  f.source.targetsPosition = 1;
  for (std::size_t i = 0; i < kRisk.size(); ++i) {
    f.stats.at(0).risk = static_cast<std::uint16_t>(kRisk.at(i));
    CHECK(battle::LiveRiskHitChance(nullptr, &f.source, nullptr, -1, -1) ==
          kRates.at(i));
  }
}

TEST_CASE("Live chain reads unsigned byte independently of char signedness",
          "[hit-modifiers][F00348][live]") {
  LiveFixture f;
  f.target.target.targetLimb = 5;
  f.target.targetsPosition = -1;
  for (unsigned evasion = 0; evasion < 256; ++evasion) {
    f.stats.at(1).limbs[5].chainEvasion = static_cast<char>(evasion);
    CHECK(battle::LiveChainHitChance(nullptr, nullptr, &f.target, -1, -1) ==
          static_cast<int>((255 - evasion) * 100 / 255));
  }
}

TEST_CASE("Original hit table dispatches all four typed callbacks",
          "[hit-modifiers][F00342][F00343][F00347][F00348][live][module]") {
  VS::Content::DataStore::ScopedOverride store;
  const auto image = ReadOriginalBattleModule();
  LiveFixture f;
  RelocateHits(image);
  Battle_previousHitResult = -1;
  f.stats.at(0).weapon.gems[0].gemEffects = 1;
  f.stats.at(1).shield.gems[0].gemEffects = 5;
  f.stats.at(0).risk = 506;
  f.stats.at(1).limbs[5].chainEvasion = static_cast<char>(128);
  f.target.target.targetLimb = 5;
  CHECK(Battle_hitFunctions[1] == &battle::PreviousLiveHitSuccessThreshold);
  CHECK(Battle_hitFunctions[2] == &battle::LiveGemHitChance);
  CHECK(Battle_hitFunctions[6] == &battle::LiveRiskHitChance);
  CHECK(Battle_hitFunctions[7] == &battle::LiveChainHitChance);
  CHECK(Battle_hitFunctions[1](nullptr, nullptr, nullptr, -1, -1) == 100);
  CHECK(Battle_hitFunctions[2](&f.action, &f.source, &f.target, 0, 0) == 100);
  CHECK(Battle_hitFunctions[6](nullptr, &f.source, nullptr, -1, -1) == 256);
  CHECK(Battle_hitFunctions[7](nullptr, nullptr, &f.target, -1, -1) == 49);
}

TEST_CASE("Original gem helper symbols dispatch short-return bindings",
          "[hit-modifiers][F00339][F00340][live][module]") {
  LiveFixture f;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto attack = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x8007E5E0);
  const auto defense = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x8007E6A0);
  REQUIRE(attack == reinterpret_cast<void*>(&battle::AttackLiveGemHitBonus));
  REQUIRE(defense == reinterpret_cast<void*>(&battle::DefenseLiveGemHitBonus));
  f.stats.at(0).weapon.gems[2].gemEffects = 2;
  f.stats.at(0).shield.gems[2].gemEffects = 6;
  f.action.type = 1;
  CHECK(reinterpret_cast<decltype(&battle::AttackLiveGemHitBonus)>(attack)(
            &f.action, &f.stats.at(0)) == 20);
  CHECK(reinterpret_cast<decltype(&battle::DefenseLiveGemHitBonus)>(defense)(
            &f.action, &f.stats.at(0), 1) == 20);
}
