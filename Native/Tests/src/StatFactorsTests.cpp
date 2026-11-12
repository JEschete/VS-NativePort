#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/HitCalculationsLive.hpp"
#include "Battle/StatFactors.hpp"
#include "Battle/StatFactorsLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
}

namespace {
namespace battle = VS::Battle;
struct World final : battle::StatFactorWorld {
  std::optional<std::uint16_t> queue = 1;
  std::array<std::optional<std::uint8_t>, 2> factors{3, 31};
  std::optional<battle::StatFactorHit> previous;
  std::optional<battle::StatFactorVital> hp;
  std::optional<battle::StatFactorVital> mp;
  std::optional<std::int16_t> target = -101;
  std::optional<std::uint16_t> pp = 65535;
  std::optional<std::uint8_t> type = 1;
  std::optional<std::uint8_t> effect = 1;
  std::optional<battle::StatFactorHit> primary;
  std::vector<int> reads;
  World() {
    previous.emplace(-101, 50);
    primary.emplace(50, -101);
    hp.emplace(201, 100);
    mp.emplace(301, 200);
  }
  std::optional<std::uint16_t> QueueIndex() override {
    reads.push_back(0);
    return queue;
  }
  std::optional<std::uint8_t> Factor(int hit) override {
    reads.push_back(1);
    return hit >= 0 && hit < 2 ? factors.at(hit) : std::nullopt;
  }
  std::optional<battle::StatFactorHit> PreviousTarget() override {
    reads.push_back(2);
    return previous;
  }
  std::optional<battle::StatFactorVital> SourceVital(
      battle::StatFactorResource resource) override {
    reads.push_back(3);
    return resource == battle::StatFactorResource::kHp ? hp : mp;
  }
  std::optional<std::int16_t> TargetHp() override {
    reads.push_back(4);
    return target;
  }
  std::optional<std::uint16_t> WeaponPp() override {
    reads.push_back(5);
    return pp;
  }
  std::optional<std::uint8_t> SelectedActionType() override {
    reads.push_back(6);
    return type;
  }
  std::optional<std::uint8_t> PrimaryHpEffect() override {
    reads.push_back(7);
    return effect;
  }
  std::optional<battle::StatFactorHit> PrimaryHit() override {
    reads.push_back(8);
    return primary;
  }
};
struct LiveFixture {
  decltype(std::to_array(Battle_actors)) actors_saved =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_statCalculators)) callbacks_saved =
      std::to_array(Battle_statCalculators);
  Battle_ActionState* saved_state = Battle_actionState;
  Battle_ActionDefinition saved_selected = Battle_actionDefinitions[255];
  std::uint32_t saved_last = Battle_lastActionStatValue;
  std::unique_ptr<Battle_ActionState> state =
      std::make_unique<Battle_ActionState>();
  Battle_ActionDefinition action{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  Battle_HitResult source{};
  Battle_HitResult target{};
  LiveFixture() {
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
    Battle_actionState = state.get();
    Battle_actors[40] = &actor;
    actor.stats = &stats;
    source.target.targetActor = 40;
    target.target.targetActor = 40;
    stats.actorId = 255;
    state->queueIndex = 0xABCD0001;
    state->selection.actionId = 255;
    state->selection.primaryTarget.hpEffect = 1;
    state->selection.primaryTarget.target.hp = -101;
    state->selection.primaryTarget.target.mp = 50;
    state->selection.targets[0].target.hp = -101;
    state->selection.targets[0].target.mp = 50;
    action.hits[0].statFactor = 3;
    action.hits[1].statFactor = 31;
    Battle_actionDefinitions[255].type = 1;
  }
  ~LiveFixture() {
    std::copy(actors_saved.begin(), actors_saved.end(), Battle_actors);
    std::copy(callbacks_saved.begin(), callbacks_saved.end(),
              Battle_statCalculators);
    Battle_actionState = saved_state;
    Battle_actionDefinitions[255] = saved_selected;
    Battle_lastActionStatValue = saved_last;
  }
};
void CheckPrevious() {
  World world;
  CHECK(battle::ScalePreviousTargetHit(0, world) == -15);
  CHECK(battle::ScalePreviousTargetHit(1, world) == -158);
  world.previous.emplace(-32768, -32768);
  CHECK(battle::ScalePreviousTargetHit(1, world) == -6553);
  world.previous.emplace(32767, 32767);
  CHECK(battle::ScalePreviousTargetHit(1, world) == 6547);
  world.factors.at(0) = 0;
  const auto queue = GENERATE(0, 1, 32768, 65535);
  world.queue = static_cast<std::uint16_t>(queue);
  CHECK(battle::ScalePreviousTargetHit(0, world) ==
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(queue - 1)));
}
void CheckPreviousRefusals() {
  World world;
  world.factors.at(0) = 0;
  world.previous.reset();
  CHECK_FALSE(battle::ScalePreviousTargetHit(0, world));
  world.previous.emplace();
  world.queue.reset();
  CHECK_FALSE(battle::ScalePreviousTargetHit(0, world));
  CHECK_FALSE(battle::ScalePreviousTargetHit(-1, world));
  CHECK_FALSE(battle::ScalePreviousTargetHit(2, world));
  world.factors.at(1).reset();
  CHECK_FALSE(battle::ScalePreviousTargetHit(1, world));
}
void CheckAlias(std::uint32_t address) {
  LiveFixture fixture;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto entry =
      std::ranges::find(symbols, address, &Content_ModuleSymbol::address);
  REQUIRE(entry != symbols.end());
  CHECK(entry->target ==
        reinterpret_cast<void*>(&battle::LiveScalePreviousTargetHit));
  fixture.state->selection.targetCount = 0;
  fixture.state->selection.actionId = 65535;
  const auto hit = GENERATE(0, 1);
  CHECK(battle::LiveScalePreviousTargetHit(&fixture.action, nullptr, nullptr,
                                           hit, -1) == (hit == 0 ? -15 : -158));
  CHECK(fixture.state->queueIndex == 0xABCD0001);
  CHECK(fixture.state->selection.targets[0].target.hp == -101);
}
using Calculator = decltype(&battle::ScaleSourceMissingHp);
void CheckMissing(Calculator calculator, battle::StatFactorResource resource) {
  World world;
  auto& vital =
      resource == battle::StatFactorResource::kHp ? world.hp : world.mp;
  CHECK(calculator(0, world) == -30);
  CHECK(calculator(1, world) == -313);
  vital.emplace(-32768, 32767);
  CHECK(calculator(1, world) == 6550);
  vital.emplace(32767, -32768);
  CHECK(calculator(1, world) == -6550);
  world.factors.at(0) = 0;
  world.queue = 0;
  CHECK(calculator(0, world) == -1);
  vital.reset();
  CHECK_FALSE(calculator(0, world));
}
void CheckMissingRefusals(Calculator calculator) {
  World world;
  CHECK_FALSE(calculator(-1, world));
  CHECK_FALSE(calculator(2, world));
  world.factors.at(0).reset();
  CHECK_FALSE(calculator(0, world));
  world.queue.reset();
  CHECK_FALSE(calculator(1, world));
  world.queue = 1;
  world.type.reset();
  world.effect.reset();
  world.primary.reset();
  CHECK(calculator(1, world) == -313);
}
}  // namespace

TEST_CASE("F00412 previous hit signed scaling", "[stat-factors][F00412]") {
  CheckPrevious();
}
TEST_CASE("F00414 previous hit signed scaling", "[stat-factors][F00414]") {
  CheckPrevious();
}
TEST_CASE("F00426 previous hit signed scaling", "[stat-factors][F00426]") {
  CheckPrevious();
}
TEST_CASE("F00412 required reads survive zero factor",
          "[stat-factors][F00412]") {
  CheckPreviousRefusals();
}
TEST_CASE("F00414 required reads survive zero factor",
          "[stat-factors][F00414]") {
  CheckPreviousRefusals();
}
TEST_CASE("F00426 required reads survive zero factor",
          "[stat-factors][F00426]") {
  CheckPreviousRefusals();
}
TEST_CASE("F00412 original address binds shared kernel",
          "[stat-factors][F00412]") {
  CheckAlias(0x80082144);
}
TEST_CASE("F00414 original address binds shared kernel",
          "[stat-factors][F00414]") {
  CheckAlias(0x80082234);
}
TEST_CASE("F00426 original address binds shared kernel",
          "[stat-factors][F00426]") {
  CheckAlias(0x80083524);
}
TEST_CASE("Missing source HP preserves overfull and signed extrema",
          "[stat-factors][F00413]") {
  CheckMissing(battle::ScaleSourceMissingHp, battle::StatFactorResource::kHp);
}
TEST_CASE("Missing source MP preserves overfull and signed extrema",
          "[stat-factors][F00419]") {
  CheckMissing(battle::ScaleSourceMissingMp, battle::StatFactorResource::kMp);
}
TEST_CASE("Missing HP refuses required inputs but not selected action",
          "[stat-factors][F00413]") {
  CheckMissingRefusals(battle::ScaleSourceMissingHp);
}
TEST_CASE("Missing MP refuses required inputs but not selected action",
          "[stat-factors][F00419]") {
  CheckMissingRefusals(battle::ScaleSourceMissingMp);
}
TEST_CASE("Target HP uses percentage signed truncation and queue wrap",
          "[stat-factors][F00415]") {
  World world;
  CHECK(battle::ScaleTargetCurrentHp(0, world) == -3);
  CHECK(battle::ScaleTargetCurrentHp(1, world) == -31);
  world.target = -32768;
  CHECK(battle::ScaleTargetCurrentHp(1, world) == -10158);
  world.target = 32767;
  world.queue = 65535;
  CHECK(battle::ScaleTargetCurrentHp(1, world) == 10155);
  world.factors.at(0) = 0;
  CHECK(battle::ScaleTargetCurrentHp(0, world) == -2);
  world.target.reset();
  CHECK_FALSE(battle::ScaleTargetCurrentHp(0, world));
}
TEST_CASE("Target HP refuses only its used inputs", "[stat-factors][F00415]") {
  World world;
  CHECK_FALSE(battle::ScaleTargetCurrentHp(-1, world));
  CHECK_FALSE(battle::ScaleTargetCurrentHp(2, world));
  world.queue.reset();
  CHECK_FALSE(battle::ScaleTargetCurrentHp(0, world));
  world.queue = 1;
  world.type.reset();
  world.hp.reset();
  world.mp.reset();
  CHECK(battle::ScaleTargetCurrentHp(0, world) == -3);
}
TEST_CASE("Weapon PP keeps unsigned value and only queue offset",
          "[stat-factors][F00416]") {
  World world;
  CHECK(battle::WeaponPpWithChainOffset(world) == -1);
  world.queue = 0;
  CHECK(battle::WeaponPpWithChainOffset(world) == -2);
  world.queue = 32768;
  CHECK(battle::WeaponPpWithChainOffset(world) == 32766);
  world.queue = 65535;
  CHECK(battle::WeaponPpWithChainOffset(world) == -3);
  CHECK(world.reads == std::vector<int>{5, 0, 5, 0, 5, 0, 5, 0});
}
TEST_CASE("Weapon PP refuses missing stats value and state only",
          "[stat-factors][F00416]") {
  World world;
  world.pp.reset();
  CHECK_FALSE(battle::WeaponPpWithChainOffset(world));
  world.pp = 0;
  world.queue.reset();
  CHECK_FALSE(battle::WeaponPpWithChainOffset(world));
}
TEST_CASE("Incoming spell gates every action type and HP effect lazily",
          "[stat-factors][F00417]") {
  World world;
  const auto type = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto effect = GENERATE(0, 1, 2, 3);
  world.type = static_cast<std::uint8_t>(type);
  world.effect = static_cast<std::uint8_t>(effect);
  world.queue.reset();
  const bool active = type == 1 && (effect == 1 || effect == 3);
  CHECK(battle::ScaleIncomingSpellDamage(active ? 1 : -1, world) ==
        (active ? -158 : 0));
  const std::vector<int> expected =
      type != 1
          ? std::vector<int>{6}
          : (active ? std::vector<int>{6, 7, 8, 1} : std::vector<int>{6, 7});
  CHECK(world.reads == expected);
}
TEST_CASE("Incoming spell narrows only after signed sum and division",
          "[stat-factors][F00417]") {
  World world;
  CHECK(battle::ScaleIncomingSpellDamage(0, world) == -15);
  world.primary.emplace(-32768, -32768);
  CHECK(battle::ScaleIncomingSpellDamage(1, world) == -6553);
  world.primary.emplace(32767, 32767);
  CHECK(battle::ScaleIncomingSpellDamage(1, world) == 6547);
  world.factors.at(0) = 0;
  CHECK(battle::ScaleIncomingSpellDamage(0, world) == 0);
  world.primary.reset();
  CHECK_FALSE(battle::ScaleIncomingSpellDamage(0, world));
}
TEST_CASE("Incoming spell refuses only values past reached gates",
          "[stat-factors][F00417]") {
  World world;
  world.type.reset();
  CHECK_FALSE(battle::ScaleIncomingSpellDamage(0, world));
  world.type = 1;
  world.effect.reset();
  CHECK_FALSE(battle::ScaleIncomingSpellDamage(0, world));
  world.effect = 3;
  CHECK_FALSE(battle::ScaleIncomingSpellDamage(-1, world));
  CHECK_FALSE(battle::ScaleIncomingSpellDamage(2, world));
  world.factors.at(0).reset();
  CHECK_FALSE(battle::ScaleIncomingSpellDamage(0, world));
  world.type = 7;
  CHECK(battle::ScaleIncomingSpellDamage(-1, world) == 0);
}
TEST_CASE("Live missing HP uses hit slot not stats identity",
          "[stat-factors][F00413]") {
  LiveFixture fixture;
  fixture.stats.currentHp = 201;
  fixture.stats.maxHp = 100;
  fixture.state->selection.actionId = 65535;
  CHECK(battle::LiveScaleSourceMissingHp(&fixture.action, &fixture.source,
                                         nullptr, 0, -1) == -30);
  CHECK(battle::LiveScaleSourceMissingHp(&fixture.action, &fixture.source,
                                         nullptr, 1, 0) == -313);
}
TEST_CASE("Live missing MP uses independent signed MP fields",
          "[stat-factors][F00419]") {
  LiveFixture fixture;
  fixture.stats.currentMp = 301;
  fixture.stats.maxMp = 200;
  fixture.state->selection.actionId = 65535;
  CHECK(battle::LiveScaleSourceMissingMp(&fixture.action, &fixture.source,
                                         nullptr, 0, -1) == -30);
  CHECK(battle::LiveScaleSourceMissingMp(&fixture.action, &fixture.source,
                                         nullptr, 1, 0) == -313);
}
TEST_CASE("Live target HP needs no source or selected definition",
          "[stat-factors][F00415]") {
  LiveFixture fixture;
  fixture.stats.currentHp = -101;
  fixture.state->selection.actionId = 65535;
  CHECK(battle::LiveScaleTargetCurrentHp(&fixture.action, nullptr,
                                         &fixture.target, 0, 0) == -3);
  CHECK(battle::LiveScaleTargetCurrentHp(&fixture.action, nullptr,
                                         &fixture.target, 1, -1) == -31);
}
TEST_CASE("Live weapon PP ignores equipment presence action and hit index",
          "[stat-factors][F00416]") {
  LiveFixture fixture;
  fixture.stats.weapon.currentPp = 65535;
  fixture.state->selection.actionId = 65535;
  const auto index = GENERATE(-1, 0, 1, 2);
  CHECK(battle::LiveWeaponPpWithChainOffset(nullptr, &fixture.source, nullptr,
                                            index, -1) == -1);
  fixture.state->queueIndex = 0xFFFF0000;
  CHECK(battle::LiveWeaponPpWithChainOffset(nullptr, &fixture.source, nullptr,
                                            index, 0) == -2);
}
TEST_CASE("Live spell selected and input actions are independent owners",
          "[stat-factors][F00417]") {
  LiveFixture fixture;
  fixture.action.type = 7;
  fixture.state->queueIndex = 0xFFFFFFFF;
  CHECK(battle::LiveScaleIncomingSpellDamage(&fixture.action, nullptr, nullptr,
                                             0, -1) == -15);
  CHECK(battle::LiveScaleIncomingSpellDamage(&fixture.action, nullptr, nullptr,
                                             1, 0) == -158);
  Battle_actionDefinitions[255].type = 7;
  CHECK(battle::LiveScaleIncomingSpellDamage(nullptr, nullptr, nullptr, -1,
                                             -1) == 0);
  Battle_actionDefinitions[255].type = 1;
  fixture.state->selection.primaryTarget.hpEffect = 2;
  CHECK(battle::LiveScaleIncomingSpellDamage(nullptr, nullptr, nullptr, 2,
                                             -1) == 0);
}
namespace {
void RelocateStatTable() {
  const std::filesystem::path root = "Native\\data";
  if (!std::filesystem::exists(root / VS::Content::DataStore::ManifestName)) {
    SKIP("Extracted Native/data is unavailable");
  }
  REQUIRE(VS::Content::DataStore::Open(root));
  auto& store = *VS::Content::DataStore::Get();
  const auto* module = store.Find("BATTLE/BATTLE.PRG");
  REQUIRE(module != nullptr);
  const auto image = store.ReadAll(*module);
  REQUIRE(image.has_value());
  const auto tables =
      std::span(Content_modulePointers, Content_modulePointerCount);
  const auto table = std::ranges::find(
      tables, reinterpret_cast<void**>(Battle_statCalculators),
      &Content_ModulePointerTable::target);
  REQUIRE(table != tables.end());
  REQUIRE(VS::Content::RelocateModulePointerTable(
      *table, image.value(),
      {Content_moduleSymbols, Content_moduleSymbolCount}));
}
}  // namespace
TEST_CASE("Original SHORT table relocates and dispatches all eight factors",
          "[stat-factors][battle][action][typed]") {
  LiveFixture fixture;
  RelocateStatTable();
  const auto selector = GENERATE(4, 5, 7, 6, 8, 9, 11, 18);
  fixture.action.hits[1].statCalculatorId = selector;
  fixture.stats.currentHp = 201;
  fixture.stats.maxHp = 100;
  fixture.stats.currentMp = 301;
  fixture.stats.maxMp = 200;
  fixture.stats.weapon.currentPp = 65535;
  constexpr std::array<int, 19> kExpected{0,  0,  0,    0, -158, -313, -158,
                                          62, -1, -158, 0, -313, 0,    0,
                                          0,  0,  0,    0, -158};
  CHECK(battle::LiveCalculateActionStatChange(&fixture.action, &fixture.source,
                                              &fixture.target, 1,
                                              -1) == kExpected.at(selector));
  CHECK(Battle_lastActionStatValue ==
        static_cast<std::uint32_t>(kExpected.at(selector)));
}
