#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/HitCalculations.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
}

namespace {
namespace battle = VS::Battle;
using Calculator = decltype(&battle::AgilityHitChance);
struct AttributeAccess final : battle::HitAttributeAccess {
  battle::HitAttributeStats state{};
  std::optional<std::int16_t> attack = 0;
  std::optional<std::int16_t> defense = 0;
  battle::HitAttributeStats Read() override { return state; }
  std::optional<std::int16_t> AttackBonus() override { return attack; }
  std::optional<std::int16_t> DefenseBonus(int hit) override {
    return hit < 0 || hit > 1 ? std::nullopt : defense;
  }
};
struct AttributeWorld final : battle::HitAttributeWorld {
  std::array<AttributeAccess, 2> actors{};
  std::array<std::optional<std::uint32_t>, 42> drawn{};
  battle::HitModifierTarget source{};
  battle::HitModifierTarget target{1, 0, 0};
  std::optional<std::uint8_t> cost = 0;
  std::function<void()> callback;
  int draws{};
  int bound{};
  int draw = 5;
  AttributeWorld() {
    drawn.fill(0);
    actors.at(0).state.actor_id = 40;
    actors.at(1).state.actor_id = 41;
    actors.at(0).state.max_hp = 1;
    actors.at(1).state.max_hp = 1;
  }
  battle::HitAttributeAccess* Stats(std::uint8_t slot) override {
    return slot < actors.size() ? &actors.at(slot) : nullptr;
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    return slot < drawn.size() ? drawn.at(slot) : std::nullopt;
  }
  std::optional<std::uint8_t> Cost() override { return cost; }
  std::int32_t RandomAveraged(int maximum) override {
    bound = maximum;
    ++draws;
    if (callback) callback();
    return draw;
  }
  std::optional<int> Calculate(Calculator function, int hit = 0,
                               int variance = 0) {
    return function(&source, &target, hit, variance, *this);
  }
};
struct DispatchWorld final : battle::ActionCalculationWorld {
  std::optional<battle::CalculationTarget> target =
      battle::CalculationTarget{1, 0, 255};
  std::optional<battle::CalculationTarget> source =
      battle::CalculationTarget{0, 0, 0};
  std::array<std::optional<battle::CalculationActor>, 2> actors{};
  std::optional<std::uint8_t> type = 0;
  std::optional<int> threshold = 50;
  std::optional<std::int16_t> amount = -3;
  std::uint32_t prerequisite = 1;
  std::uint32_t last{};
  int draw = 50;
  int source_reads{};
  std::vector<int> events;
  std::function<void()> calculator;
  std::function<void()> random;
  DispatchWorld() { actors.fill(battle::CalculationActor{}); }
  battle::CalculationTarget& TargetState() {
    if (!target) std::abort();
    return target.value();
  }
  battle::CalculationTarget& SourceState() {
    if (!source) std::abort();
    return source.value();
  }
  battle::CalculationActor& ActorState(std::size_t slot) {
    auto& actor = actors.at(slot);
    if (!actor) std::abort();
    return actor.value();
  }
  std::optional<battle::CalculationTarget> Target() override { return target; }
  std::optional<battle::CalculationTarget> Source() override {
    ++source_reads;
    return source;
  }
  std::optional<battle::CalculationActor> Actor(std::uint8_t slot) override {
    return slot < actors.size() ? actors.at(slot) : std::nullopt;
  }
  std::optional<std::uint8_t> ActionType() override { return type; }
  std::optional<int> HitCalculator(int hit, int execute) override {
    events.insert(events.end(), {1, hit, execute});
    if (calculator) calculator();
    return threshold;
  }
  std::optional<std::int16_t> StatCalculator(int hit, int execute) override {
    events.insert(events.end(), {2, hit, execute});
    if (calculator) calculator();
    return amount;
  }
  std::uint32_t Prerequisite() override { return prerequisite; }
  void StoreThreshold(std::uint8_t value) override {
    events.push_back(3);
    TargetState().threshold = value;
  }
  void StoreLastValue(std::uint32_t value) override {
    events.push_back(4);
    last = value;
  }
  std::int32_t RandomBounded(int bound) override {
    events.insert(events.end(), {5, bound, TargetState().threshold});
    if (random) random();
    return draw;
  }
};
struct LiveFixture {
  decltype(std::to_array(Battle_actors)) saved = std::to_array(Battle_actors);
  decltype(std::to_array(Battle_hitFunctions)) hits =
      std::to_array(Battle_hitFunctions);
  decltype(std::to_array(Battle_statCalculators)) stats =
      std::to_array(Battle_statCalculators);
  decltype(std::to_array(Battle_actionEffectMediators)) effects =
      std::to_array(Battle_actionEffectMediators);
  std::uint32_t last = Battle_lastActionStatValue;
  std::uint32_t prerequisite = Battle_hitPrerequisiteResult;
  std::array<Battle_Actor, 2> actors{};
  std::array<Battle_ActorStats, 2> owners{};
  Battle_ActionDefinition action{};
  Battle_HitResult source{};
  Battle_HitResult target{};
  LiveFixture() {
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
    for (unsigned i = 0; i < actors.size(); ++i) {
      Battle_actors[i] = &actors.at(i);
      actors.at(i).stats = &owners.at(i);
      owners.at(i).actorId = static_cast<std::uint8_t>(i);
      owners.at(i).maxHp = 1;
    }
    target.target.targetActor = 1;
    Battle_hitPrerequisiteResult = 1;
  }
  ~LiveFixture() {
    std::copy(saved.begin(), saved.end(), Battle_actors);
    std::copy(hits.begin(), hits.end(), Battle_hitFunctions);
    std::copy(stats.begin(), stats.end(), Battle_statCalculators);
    std::copy(effects.begin(), effects.end(), Battle_actionEffectMediators);
    Battle_lastActionStatValue = last;
    Battle_hitPrerequisiteResult = prerequisite;
  }
};
std::vector<std::uint8_t> ReadModule(const char* name) {
  const std::filesystem::path root = "Native\\data";
  if (!std::filesystem::exists(root / VS::Content::DataStore::ManifestName)) {
    SKIP("Extracted Native/data is unavailable");
  }
  REQUIRE(VS::Content::DataStore::Open(root));
  auto& store = *VS::Content::DataStore::Get();
  const auto* module = store.Find(name);
  REQUIRE(module != nullptr);
  auto bytes = store.ReadAll(*module);
  REQUIRE(bytes.has_value());
  return bytes.value();
}
void Relocate(void** target, std::span<const std::uint8_t> image) {
  const auto tables =
      std::span(Content_modulePointers, Content_modulePointerCount);
  const auto table =
      std::ranges::find(tables, target, &Content_ModulePointerTable::target);
  REQUIRE(table != tables.end());
  REQUIRE(VS::Content::RelocateModulePointerTable(
      *table, image, {Content_moduleSymbols, Content_moduleSymbolCount}));
}

bool RoutedSelectorsAreActive(
    std::span<const Battle_ActionDefinition> definitions) {
  constexpr std::array kStatEffects{40U, 41U, 42U, 43U, 44U, 45U, 46U, 47U, 48U,
                                    49U, 58U, 59U, 62U, 63U, 64U, 65U, 66U};
  for (const auto& action : definitions) {
    for (const auto& hit : action.hits) {
      const auto prerequisite = hit.prerequisiteFunctionId;
      if (prerequisite == 0) continue;
      if (hit.effectId == 0) return false;
      const bool direct = prerequisite >= 6 && prerequisite <= 8;
      if (!direct && hit.hitFunctionId == 0) return false;
      if (std::ranges::find(kStatEffects, hit.effectId) != kStatEffects.end() &&
          hit.statCalculatorId == 0) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

TEST_CASE(
    "Agility includes signed gear and every armor without equipped guards",
    "[hit-calculations][F00344]") {
  AttributeWorld w;
  auto& a = w.actors.at(0).state.agility;
  a.base = 100;
  a.accessory = -30;
  a.armor.fill(-2);
  CHECK(w.Calculate(battle::AgilityHitChance) == 158);
  w.target.targets_position = 1;
  w.source.targets_position = 1;
  CHECK(w.Calculate(battle::AgilityHitChance) == 158);
  w.actors.at(1).state.defense.at(0) = -5;
  CHECK(w.Calculate(battle::AgilityHitChance) == 163);
}
TEST_CASE("Agility drawn checks use each stats owner not input hit slot",
          "[hit-calculations][F00344]") {
  AttributeWorld w;
  w.actors.at(0).state.agility.weapon = -40;
  w.actors.at(1).state.agility.shield = 20;
  w.drawn.at(0) = 1;
  w.drawn.at(1) = 1;
  CHECK(w.Calculate(battle::AgilityHitChance) == 100);
  w.drawn.at(40) = 1;
  CHECK(w.Calculate(battle::AgilityHitChance) == 60);
  w.drawn.at(41) = 1;
  CHECK(w.Calculate(battle::AgilityHitChance) == 40);
  w.drawn.at(40) = 2;
  CHECK(w.Calculate(battle::AgilityHitChance) == 80);
}
TEST_CASE("Attribute functions wrap products before signed truncating division",
          "[hit-calculations][F00344][F00345][F00346]") {
  const auto function =
      GENERATE(battle::AgilityHitChance, battle::IntelligenceHitChance,
               battle::CostAdjustedAgilityHitChance);
  AttributeWorld w;
  auto& a = w.actors.at(0).state;
  a.agility.base = a.intelligence.base = 65535;
  a.risk = 65535;
  const int offset = function == battle::AgilityHitChance        ? 100
                     : function == battle::IntelligenceHitChance ? 0
                                                                 : 130;
  CHECK(w.Calculate(function) == 66845 + offset);
}
TEST_CASE("Intelligence uses target HP and signed agility limb defense",
          "[hit-calculations][F00345]") {
  AttributeWorld w;
  w.actors.at(0).state.intelligence.base = 200;
  auto& target = w.actors.at(1).state;
  target.intelligence.base = 80;
  target.defense.at(0) = -10;
  target.hp = 3;
  target.max_hp = 2;
  CHECK(w.Calculate(battle::IntelligenceHitChance) == 95);
  target.hp = -3;
  CHECK(w.Calculate(battle::IntelligenceHitChance) == 305);
  target.max_hp = -2;
  CHECK(w.Calculate(battle::IntelligenceHitChance) == 95);
}
TEST_CASE("Intelligence preserves raw R3000 zero divisor semantics",
          "[hit-calculations][F00345]") {
  AttributeWorld w;
  auto& target = w.actors.at(1).state;
  target.intelligence.base = 3;
  target.max_hp = 0;
  target.hp = 1;
  CHECK(w.Calculate(battle::IntelligenceHitChance) == 1);
  target.hp = -1;
  CHECK(w.Calculate(battle::IntelligenceHitChance) == 0);
  target.hp = 0;
  CHECK(w.Calculate(battle::IntelligenceHitChance) == 1);
}
TEST_CASE("Cost adjusted agility reads unsigned action cost before variance",
          "[hit-calculations][F00346]") {
  AttributeWorld w;
  w.cost = 255;
  w.actors.at(0).state.agility.base = 300;
  CHECK(w.Calculate(battle::CostAdjustedAgilityHitChance) == 175);
  w.callback = [&] { w.cost = 0; };
  CHECK(w.Calculate(battle::CostAdjustedAgilityHitChance, 0, -1) == 175);
  CHECK(w.bound == 11);
  CHECK(w.draws == 1);
  w.cost.reset();
  CHECK_FALSE(w.Calculate(battle::CostAdjustedAgilityHitChance));
}
TEST_CASE("Attribute chance floors negatives and corrects only exact 255",
          "[hit-calculations][F00344][F00345][F00346]") {
  const auto function =
      GENERATE(battle::AgilityHitChance, battle::IntelligenceHitChance,
               battle::CostAdjustedAgilityHitChance);
  AttributeWorld w;
  const int offset = function == battle::AgilityHitChance        ? 100
                     : function == battle::IntelligenceHitChance ? 0
                                                                 : 130;
  w.actors.at(0).state.agility.base = w.actors.at(0).state.intelligence.base =
      static_cast<std::uint16_t>(255 - offset);
  CHECK(w.Calculate(function) == 254);
  w.actors.at(0).attack = 1;
  CHECK(w.Calculate(function) == 256);
  w.actors.at(0).attack = -300;
  CHECK(w.Calculate(function) == 0);
}
TEST_CASE("Variance keeps cached math but observes changed owner gem state",
          "[hit-calculations][F00344][F00345][F00346]") {
  const auto function =
      GENERATE(battle::AgilityHitChance, battle::IntelligenceHitChance,
               battle::CostAdjustedAgilityHitChance);
  AttributeWorld w;
  const auto before = w.Calculate(function).value_or(-1);
  w.callback = [&] {
    w.actors.at(0).state.agility.base = 65000;
    w.actors.at(0).state.intelligence.base = 65000;
    w.actors.at(0).attack = 20;
    w.actors.at(1).defense = 5;
    w.source.actor = w.target.actor = 42;
  };
  w.draw = 10;
  CHECK(w.Calculate(function, 1, -1) == before + 20);
  CHECK(w.draws == 1);
}
TEST_CASE("Attributes refuse each used hit stats owner and signed limb index",
          "[hit-calculations][F00344][F00345][F00346]") {
  const auto function =
      GENERATE(battle::AgilityHitChance, battle::IntelligenceHitChance,
               battle::CostAdjustedAgilityHitChance);
  AttributeWorld w;
  CHECK_FALSE(function(nullptr, &w.target, 0, 0, w));
  CHECK_FALSE(function(&w.source, nullptr, 0, 0, w));
  w.source.actor = 255;
  CHECK_FALSE(w.Calculate(function));
  w.source.actor = 0;
  w.target.limb = GENERATE(-128, -1, 6, 127);
  CHECK_FALSE(w.Calculate(function));
  w.target.limb = 0;
  w.actors.at(1).state.actor_id = 255;
  CHECK_FALSE(w.Calculate(function));
}
TEST_CASE("Attributes propagate drawn and gem refusals without RNG",
          "[hit-calculations][F00344][F00345][F00346]") {
  const auto function =
      GENERATE(battle::AgilityHitChance, battle::IntelligenceHitChance,
               battle::CostAdjustedAgilityHitChance);
  AttributeWorld w;
  w.drawn.at(41).reset();
  CHECK_FALSE(w.Calculate(function, 0, 1));
  CHECK(w.draws == 0);
  w.drawn.at(41) = 1;
  w.actors.at(0).attack.reset();
  CHECK_FALSE(w.Calculate(function));
  w.actors.at(0).attack = 0;
  CHECK_FALSE(w.Calculate(function, -1));
  CHECK_FALSE(w.Calculate(function, 2));
}
TEST_CASE(
    "Roll stores threshold before RNG and applies Ashley bonus afterwards",
    "[hit-calculations][F00349]") {
  DispatchWorld w;
  CHECK(battle::RollActionHit(0, -1, w) == 1);
  CHECK(w.TargetState().threshold == 50);
  CHECK(w.events == std::vector<int>{1, 0, -1, 3, 5, 100, 50});
  w.threshold = 100;
  w.draw = 105;
  CHECK(battle::RollActionHit(0, 0, w) == 1);
  CHECK(w.TargetState().threshold == 100);
}
TEST_CASE("Roll wraps doubling then caps signed positives without a floor",
          "[hit-calculations][F00349]") {
  DispatchWorld w;
  w.ActorState(1).statuses = 4;
  w.threshold = 0x40000001;
  w.draw = -1;
  CHECK(battle::RollActionHit(0, 0, w) == 0);
  CHECK(w.TargetState().threshold == 2);
  w.threshold = -1;
  w.SourceState().actor = 1;
  w.draw = -3;
  CHECK(battle::RollActionHit(0, 0, w) == 1);
  CHECK(w.TargetState().threshold == 254);
}
TEST_CASE("Roll failure sentinel still draws but never reads source",
          "[hit-calculations][F00349]") {
  DispatchWorld w;
  w.source.reset();
  w.threshold = 255;
  CHECK(battle::RollActionHit(0, 0, w) == 0);
  CHECK(w.source_reads == 0);
  CHECK(w.events.back() == 255);
  w.threshold = 100;
  w.prerequisite = 0;
  CHECK(battle::RollActionHit(0, 0, w) == 0);
  CHECK(w.source_reads == 0);
}
TEST_CASE("Roll preserves previous threshold unless first hit or sentinel",
          "[hit-calculations][F00349]") {
  DispatchWorld w;
  w.TargetState().threshold = 20;
  CHECK(battle::RollActionHit(1, 0, w) == 1);
  CHECK(w.TargetState().threshold == 20);
  w.TargetState().threshold = 255;
  CHECK(battle::RollActionHit(1, 0, w) == 1);
  CHECK(w.TargetState().threshold == 50);
}
TEST_CASE("Roll rereads spell target and prerequisite after calculator",
          "[hit-calculations][F00349]") {
  DispatchWorld w;
  w.calculator = [&] {
    w.type = 1;
    w.TargetState().actor = 0;
    w.TargetState().position = 1;
    w.ActorState(0).statuses = std::numeric_limits<int>::min();
  };
  w.SourceState().actor = 1;
  CHECK(battle::RollActionHit(0, 0, w) == 0);
  CHECK(w.TargetState().threshold == 0);
  w.calculator = [&] { w.prerequisite = 0; };
  CHECK(battle::RollActionHit(0, 0, w) == 0);
  CHECK(w.TargetState().threshold == 255);
}
TEST_CASE("Roll rereads source after RNG but retains local threshold",
          "[hit-calculations][F00349]") {
  DispatchWorld w;
  w.SourceState().actor = 1;
  w.random = [&] {
    w.SourceState().actor = 0;
    w.threshold = 0;
    w.prerequisite = 0;
    w.TargetState().threshold = 77;
  };
  CHECK(battle::RollActionHit(0, 0, w) == 1);
  CHECK(w.TargetState().threshold == 77);
}
TEST_CASE("Exempt roll bypasses action and hit index reads",
          "[hit-calculations][F00349]") {
  DispatchWorld w;
  w.ActorState(1).id = 128;
  w.type.reset();
  w.threshold.reset();
  CHECK(battle::RollActionHit(-1, -1, w) == 1);
  CHECK(w.events == std::vector<int>{3, 5, 100, 100});
}
TEST_CASE("Roll refuses only required actor and source paths",
          "[hit-calculations][F00349]") {
  DispatchWorld w;
  w.actors.at(1).reset();
  CHECK_FALSE(battle::RollActionHit(0, 0, w));
  w.TargetState().position = 1;
  CHECK(battle::RollActionHit(0, 0, w) == 1);
  w.type = 1;
  CHECK_FALSE(battle::RollActionHit(0, 0, w));
  w.type = 0;
  w.source.reset();
  CHECK_FALSE(battle::RollActionHit(0, 0, w));
}
TEST_CASE("Stat dispatcher sign extends the short and final global write wins",
          "[hit-calculations][F00435]") {
  DispatchWorld w;
  const auto amount = static_cast<std::int16_t>(GENERATE(-32768, -1, 0, 32767));
  w.amount = amount;
  w.calculator = [&] { w.last = 123; };
  CHECK(battle::CalculateActionStatChange(1, -1, w) == w.amount);
  CHECK(w.last == static_cast<std::uint32_t>(amount));
  CHECK(w.events == std::vector<int>{2, 1, -1, 4});
}
TEST_CASE("Exempt stat dispatcher needs no action source callback or index",
          "[hit-calculations][F00435]") {
  DispatchWorld w;
  w.ActorState(1).id = 128;
  w.source.reset();
  w.type.reset();
  w.amount.reset();
  CHECK(battle::CalculateActionStatChange(-1, -1, w) == 999);
  CHECK(w.last == 999);
  CHECK(w.events == std::vector<int>{4});
}
TEST_CASE(
    "Stat dispatcher refuses missing state without overwriting last value",
    "[hit-calculations][F00435]") {
  DispatchWorld w;
  w.last = 123;
  w.amount.reset();
  CHECK_FALSE(battle::CalculateActionStatChange(0, 0, w));
  CHECK(w.last == 123);
  w.target.reset();
  CHECK_FALSE(battle::CalculateActionStatChange(0, 0, w));
  w.target = battle::CalculationTarget{42, 0, 255};
  CHECK_FALSE(battle::CalculateActionStatChange(0, 0, w));
}
TEST_CASE("Live attributes use existing gear fields and real gem helpers",
          "[hit-calculations][F00344][F00345][F00346]") {
  LiveFixture f;
  f.owners.at(0).agility = 180;
  f.owners.at(0).intelligence = 180;
  f.owners.at(0).weapon.gems[2].gemEffects = 1;
  f.actors.at(0).weaponDrawn = 1;
  f.action.cost = 30;
  CHECK(battle::LiveAgilityHitChance(&f.action, &f.source, &f.target, 0, 0) ==
        300);
  CHECK(battle::LiveIntelligenceHitChance(&f.action, &f.source, &f.target, 0,
                                          0) == 200);
  CHECK(battle::LiveCostAdjustedAgilityHitChance(&f.action, &f.source,
                                                 &f.target, 0, 0) == 300);
}
TEST_CASE("Original tables relocate active calculators and distinct sentinels",
          "[hit-calculations][sentinel-retirement]") {
  LiveFixture f;
  const auto image = ReadModule("BATTLE/BATTLE.PRG");
  Relocate(reinterpret_cast<void**>(Battle_hitFunctions), image);
  Relocate(reinterpret_cast<void**>(Battle_statCalculators), image);
  Relocate(reinterpret_cast<void**>(Battle_actionEffectMediators), image);
  CHECK(Battle_hitFunctions[0] == &battle::RejectLiveHitSelector);
  CHECK(Battle_statCalculators[0] == &battle::RejectLiveStatSelector);
  CHECK(Battle_actionEffectMediators[0] == &battle::RejectLiveEffectSelector);
  CHECK(Battle_hitFunctions[3] == &battle::LiveAgilityHitChance);
  CHECK(Battle_hitFunctions[4] == &battle::LiveIntelligenceHitChance);
  CHECK(Battle_hitFunctions[5] == &battle::LiveCostAdjustedAgilityHitChance);
}
TEST_CASE("Live stat dispatch invokes actual short table ABI",
          "[hit-calculations][F00435][battle][action][typed]") {
  LiveFixture f;
  Battle_statCalculators[26] =
      +[](Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*,
          int hit, int execute) -> std::int16_t {
    Battle_lastActionStatValue = 123;
    return static_cast<std::int16_t>(hit + execute);
  };
  f.action.hits[1].statCalculatorId = 26;
  CHECK(battle::LiveCalculateActionStatChange(&f.action, nullptr, &f.target, 1,
                                              -32769) == -32768);
  CHECK(Battle_lastActionStatValue == 0xFFFF8000U);
  f.owners.at(1).actorId = 128;
  CHECK(battle::LiveCalculateActionStatChange(nullptr, nullptr, &f.target, -1,
                                              -1) == 999);
}
TEST_CASE("Live roll invokes actual int table ABI and lazy exemption",
          "[hit-calculations][F00349][battle][action][typed]") {
  LiveFixture f;
  Battle_hitFunctions[7] =
      +[](Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int,
          int) -> int { return 255; };
  f.action.hits[0].hitFunctionId = 7;
  CHECK(battle::LiveRollActionHit(&f.action, nullptr, &f.target, 0, 0) == 0);
  CHECK(f.target.target.hitThreshold == 255);
  f.owners.at(1).actorId = 128;
  CHECK(battle::LiveRollActionHit(nullptr, &f.source, &f.target, -1, -1) == 1);
}

TEST_CASE("Original action table never routes to retired selectors",
          "[hit-calculations][sentinel-retirement]") {
  auto image = ReadModule("SLUS_010.40");
  constexpr std::size_t kTableOffset = 0x3C1DC;
  std::array<Battle_ActionDefinition, 256> definitions{};
  REQUIRE(image.size() >= kTableOffset + sizeof(definitions));
  std::memcpy(definitions.data(), image.data() + kTableOffset,
              sizeof(definitions));
  REQUIRE(RoutedSelectorsAreActive(definitions));
  SECTION("hit sentinel negative control") {
    definitions.at(79).hits[0].prerequisiteFunctionId = 1;
  }
  SECTION("stat sentinel negative control") {
    definitions.at(1).hits[0].statCalculatorId = 0;
  }
  SECTION("effect sentinel negative control") {
    definitions.at(1).hits[0].effectId = 0;
  }
  CHECK_FALSE(RoutedSelectorsAreActive(definitions));
}
