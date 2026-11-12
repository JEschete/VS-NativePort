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
  std::optional<battle::StatFactorVital> target_hp;
  std::optional<battle::StatFactorVital> target_mp;
  std::optional<std::uint16_t> shield_pp = 65535;
  std::optional<std::uint8_t> cost = 255;
  std::vector<int> reads;
  World() {
    previous.emplace(-101, 50);
    primary.emplace(50, -101);
    hp.emplace(201, 100);
    mp.emplace(301, 200);
    target_hp.emplace(201, 100);
    target_mp.emplace(302, 200);
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
  std::optional<battle::StatFactorVital> TargetVital(
      battle::StatFactorResource resource) override {
    reads.push_back(9);
    return resource == battle::StatFactorResource::kHp ? target_hp : target_mp;
  }
  std::optional<std::uint16_t> ShieldPp() override {
    reads.push_back(10);
    return shield_pp;
  }
  std::optional<std::uint8_t> SelectedActionCost() override {
    reads.push_back(11);
    return cost;
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

namespace {
enum class IncomingGate { kPhysical, kSpell, kAny };
bool Accepts(IncomingGate gate, int type) {
  return gate == IncomingGate::kAny ||
         ((type == 1) == (gate == IncomingGate::kSpell));
}
void CheckIncomingGates(Calculator calculator, IncomingGate gate) {
  World world;
  const auto type = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto effect = GENERATE(0, 1, 2, 3);
  const auto hit = GENERATE(0, 1);
  const auto factor = GENERATE(0, 31);
  world.type = static_cast<std::uint8_t>(type);
  world.effect = static_cast<std::uint8_t>(effect);
  world.factors.at(hit) = static_cast<std::uint8_t>(factor);
  world.queue.reset();
  const bool active = Accepts(gate, type) && (effect == 1 || effect == 3);
  CHECK(calculator(hit, world) == (active ? (-51 * factor) / 10 : 0));
  CHECK(std::ranges::find(world.reads, 0) == world.reads.end());
  if (gate == IncomingGate::kAny) {
    CHECK(std::ranges::find(world.reads, 6) == world.reads.end());
  }
  if (!active) {
    world.factors = {};
    world.primary.reset();
    CHECK(calculator(-1, world) == 0);
    CHECK(calculator(2, world) == 0);
  }
}
void CheckIncomingArithmetic(Calculator calculator, IncomingGate gate) {
  World world;
  world.type = gate == IncomingGate::kSpell ? 1 : 7;
  CHECK(calculator(0, world) == -15);
  world.primary.emplace(-32768, -32768);
  CHECK(calculator(1, world) == -6553);
  world.primary.emplace(32767, 32767);
  CHECK(calculator(1, world) == 6547);
  world.factors.at(0) = 0;
  CHECK(calculator(0, world) == 0);
  world.primary.reset();
  CHECK_FALSE(calculator(0, world));
}
void CheckIncomingRefusals(Calculator calculator, IncomingGate gate) {
  World world;
  world.type = gate == IncomingGate::kSpell ? 1 : 7;
  CHECK_FALSE(calculator(-1, world));
  CHECK_FALSE(calculator(2, world));
  world.factors.at(0).reset();
  CHECK_FALSE(calculator(0, world));
  world.effect.reset();
  CHECK_FALSE(calculator(1, world));
  world.effect = 3;
  world.type.reset();
  if (gate == IncomingGate::kAny) {
    CHECK(calculator(1, world) == -158);
  } else {
    CHECK_FALSE(calculator(1, world));
  }
}
using LiveCalculator = decltype(&battle::LiveScaleIncomingDamage);
void CheckIncomingLive(LiveCalculator calculator, IncomingGate gate,
                       std::uint32_t address) {
  LiveFixture fixture;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto entry =
      std::ranges::find(symbols, address, &Content_ModuleSymbol::address);
  REQUIRE(entry != symbols.end());
  CHECK(entry->target == reinterpret_cast<void*>(calculator));
  Battle_actionDefinitions[255].type = gate == IncomingGate::kSpell ? 1 : 7;
  fixture.action.type = gate == IncomingGate::kSpell ? 7 : 1;
  fixture.state->queueIndex = 65535;
  if (gate == IncomingGate::kAny) fixture.state->selection.actionId = 65535;
  CHECK(calculator(&fixture.action, nullptr, nullptr, 0, -1) == -15);
  CHECK(calculator(&fixture.action, nullptr, nullptr, 1, 0) == -158);
  fixture.state->selection.primaryTarget.hpEffect = 2;
  CHECK(calculator(nullptr, nullptr, nullptr, -1, -1) == 0);
  CHECK(calculator(nullptr, nullptr, nullptr, 2, 0) == 0);
}
using VitalCalculator = decltype(&battle::TargetMissingHp);
void CheckTargetMissing(VitalCalculator calculator,
                        battle::StatFactorResource resource) {
  World world;
  auto& vital = resource == battle::StatFactorResource::kHp ? world.target_hp
                                                            : world.target_mp;
  world.queue.reset();
  world.hp.reset();
  world.mp.reset();
  world.factors = {};
  vital.emplace(201, 100);
  CHECK(calculator(world) == -101);
  vital.emplace(-32768, 32767);
  CHECK(calculator(world) == -1);
  vital.emplace(32767, -32768);
  CHECK(calculator(world) == 1);
  vital.emplace(0, 0);
  CHECK(calculator(world) == 0);
  CHECK(world.reads == std::vector<int>{9, 9, 9, 9});
  vital.reset();
  CHECK_FALSE(calculator(world));
}
void CheckTargetLive(LiveCalculator calculator) {
  LiveFixture fixture;
  fixture.stats.currentHp = 201;
  fixture.stats.maxHp = 100;
  fixture.stats.currentMp = 302;
  fixture.stats.maxMp = 200;
  fixture.target.targetsPosition = GENERATE(0, 1);
  fixture.state->selection.actionId = 65535;
  Battle_actionState = nullptr;
  const auto hit = GENERATE(-1, 0, 1, 2);
  CHECK(calculator(nullptr, nullptr, &fixture.target, hit, -1) ==
        (calculator == battle::LiveTargetMissingHp ? -101 : -102));
}
}  // namespace

TEST_CASE("F00418 physical type effect hit and factor lattice",
          "[stat-factors][stat-factors-b8][F00418]") {
  CheckIncomingGates(battle::ScaleIncomingPhysicalDamage,
                     IncomingGate::kPhysical);
}
TEST_CASE("F00428 physical type effect hit and factor lattice",
          "[stat-factors][stat-factors-b8][F00428]") {
  CheckIncomingGates(battle::ScaleIncomingPhysicalDamage,
                     IncomingGate::kPhysical);
}
TEST_CASE("F00427 spell alias type effect hit and factor lattice",
          "[stat-factors][stat-factors-b8][F00427]") {
  CheckIncomingGates(battle::ScaleIncomingSpellDamage, IncomingGate::kSpell);
}
TEST_CASE("F00430 only primary effect gates the hit and factor lattice",
          "[stat-factors][stat-factors-b8][F00430]") {
  CheckIncomingGates(battle::ScaleIncomingDamage, IncomingGate::kAny);
}
TEST_CASE("F00418 signed sum truncation and narrowing",
          "[stat-factors][stat-factors-b8][F00418]") {
  CheckIncomingArithmetic(battle::ScaleIncomingPhysicalDamage,
                          IncomingGate::kPhysical);
}
TEST_CASE("F00428 signed sum truncation and narrowing",
          "[stat-factors][stat-factors-b8][F00428]") {
  CheckIncomingArithmetic(battle::ScaleIncomingPhysicalDamage,
                          IncomingGate::kPhysical);
}
TEST_CASE("F00427 signed sum truncation and narrowing",
          "[stat-factors][stat-factors-b8][F00427]") {
  CheckIncomingArithmetic(battle::ScaleIncomingSpellDamage,
                          IncomingGate::kSpell);
}
TEST_CASE("F00430 signed sum truncation and narrowing",
          "[stat-factors][stat-factors-b8][F00430]") {
  CheckIncomingArithmetic(battle::ScaleIncomingDamage, IncomingGate::kAny);
}
TEST_CASE("F00418 required inputs fail only after their gates",
          "[stat-factors][stat-factors-b8][F00418]") {
  CheckIncomingRefusals(battle::ScaleIncomingPhysicalDamage,
                        IncomingGate::kPhysical);
}
TEST_CASE("F00428 required inputs fail only after their gates",
          "[stat-factors][stat-factors-b8][F00428]") {
  CheckIncomingRefusals(battle::ScaleIncomingPhysicalDamage,
                        IncomingGate::kPhysical);
}
TEST_CASE("F00427 required inputs fail only after their gates",
          "[stat-factors][stat-factors-b8][F00427]") {
  CheckIncomingRefusals(battle::ScaleIncomingSpellDamage, IncomingGate::kSpell);
}
TEST_CASE("F00430 never requires a selected action",
          "[stat-factors][stat-factors-b8][F00430]") {
  CheckIncomingRefusals(battle::ScaleIncomingDamage, IncomingGate::kAny);
}
TEST_CASE("F00418 live physical original entry and independent action owners",
          "[stat-factors][stat-factors-b8][F00418]") {
  CheckIncomingLive(battle::LiveScaleIncomingPhysicalDamage,
                    IncomingGate::kPhysical, 0x8008241C);
}
TEST_CASE(
    "F00428 live physical alias original entry and independent action owners",
    "[stat-factors][stat-factors-b8][F00428]") {
  CheckIncomingLive(battle::LiveScaleIncomingPhysicalDamage,
                    IncomingGate::kPhysical, 0x8008364C);
}
TEST_CASE("F00427 live spell alias reuses F00417 binding",
          "[stat-factors][stat-factors-b8][F00427]") {
  CheckIncomingLive(battle::LiveScaleIncomingSpellDamage, IncomingGate::kSpell,
                    0x80083590);
}
TEST_CASE("F00430 live damage ignores invalid selection ID",
          "[stat-factors][stat-factors-b8][F00430]") {
  CheckIncomingLive(battle::LiveScaleIncomingDamage, IncomingGate::kAny,
                    0x8008379C);
}
TEST_CASE(
    "F00424 target missing HP preserves low16 subtraction without scaling",
    "[stat-factors][stat-factors-b8][F00424]") {
  CheckTargetMissing(battle::TargetMissingHp, battle::StatFactorResource::kHp);
}
TEST_CASE(
    "F00425 target missing MP preserves low16 subtraction without scaling",
    "[stat-factors][stat-factors-b8][F00425]") {
  CheckTargetMissing(battle::TargetMissingMp, battle::StatFactorResource::kMp);
}
TEST_CASE("F00424 live HP still reads the target actor for position hits",
          "[stat-factors][stat-factors-b8][F00424]") {
  CheckTargetLive(battle::LiveTargetMissingHp);
}
TEST_CASE("F00425 live MP still reads the target actor for position hits",
          "[stat-factors][stat-factors-b8][F00425]") {
  CheckTargetLive(battle::LiveTargetMissingMp);
}
TEST_CASE("F00429 selected spell cost has no HP effect or queue dependencies",
          "[stat-factors][stat-factors-b8][F00429]") {
  World world;
  const auto type = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto hit = GENERATE(0, 1);
  const auto factor = GENERATE(0, 31);
  const auto cost = GENERATE(0, 255);
  world.type = static_cast<std::uint8_t>(type);
  world.factors.at(hit) = static_cast<std::uint8_t>(factor);
  world.cost = static_cast<std::uint8_t>(cost);
  world.effect.reset();
  world.primary.reset();
  world.queue.reset();
  CHECK(battle::ScaleIncomingSpellCost(hit, world) ==
        (type == 1 ? (cost * factor) / 10 : 0));
  CHECK(world.reads ==
        (type == 1 ? std::vector<int>{6, 11, 1} : std::vector<int>{6}));
}
TEST_CASE("F00429 refuses reached inputs even when cost is zero",
          "[stat-factors][stat-factors-b8][F00429]") {
  World world;
  world.cost = 0;
  CHECK_FALSE(battle::ScaleIncomingSpellCost(-1, world));
  CHECK_FALSE(battle::ScaleIncomingSpellCost(2, world));
  world.factors.at(0).reset();
  CHECK_FALSE(battle::ScaleIncomingSpellCost(0, world));
  world.cost.reset();
  CHECK_FALSE(battle::ScaleIncomingSpellCost(1, world));
  world.type.reset();
  CHECK_FALSE(battle::ScaleIncomingSpellCost(1, world));
  world.type = 7;
  CHECK(battle::ScaleIncomingSpellCost(-1, world) == 0);
}
TEST_CASE("F00429 live cost and factor have different action owners",
          "[stat-factors][stat-factors-b8][F00429]") {
  LiveFixture fixture;
  Battle_actionDefinitions[255].cost = 255;
  Battle_actionDefinitions[255].hits[1].statFactor = 0;
  fixture.action.type = 7;
  fixture.action.cost = 0;
  fixture.state->selection.primaryTarget.hpEffect = GENERATE(0, 1, 2, 3);
  CHECK(battle::LiveScaleIncomingSpellCost(&fixture.action, nullptr, nullptr, 0,
                                           -1) == 76);
  CHECK(battle::LiveScaleIncomingSpellCost(&fixture.action, nullptr, nullptr, 1,
                                           0) == 790);
  Battle_actionDefinitions[255].type = 7;
  CHECK(battle::LiveScaleIncomingSpellCost(nullptr, nullptr, nullptr, -1, -1) ==
        0);
}
TEST_CASE("F00431 shield PP preserves signed return and reads nothing else",
          "[stat-factors][stat-factors-b8][F00431]") {
  World world;
  const auto pp = GENERATE(0, 32767, 32768, 65535);
  world.shield_pp = static_cast<std::uint16_t>(pp);
  world.queue.reset();
  CHECK(battle::SourceShieldPp(world) ==
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(pp)));
  CHECK(world.reads == std::vector<int>{10});
  world.shield_pp.reset();
  CHECK_FALSE(battle::SourceShieldPp(world));
}
TEST_CASE("F00431 live shield PP ignores equipment and position flags",
          "[stat-factors][stat-factors-b8][F00431]") {
  LiveFixture fixture;
  Battle_actionState = nullptr;
  fixture.source.targetsPosition = GENERATE(0, 1);
  const auto pp = GENERATE(0, 32767, 32768, 65535);
  fixture.stats.shield.currentPp = static_cast<std::uint16_t>(pp);
  fixture.stats.weapon.currentPp = 123;
  CHECK(battle::LiveSourceShieldPp(nullptr, &fixture.source, nullptr, -1, -1) ==
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(pp)));
}
TEST_CASE("Original SHORT table relocates and dispatches all B8 factors",
          "[stat-factors][stat-factors-b8][battle][action][typed]") {
  LiveFixture fixture;
  RelocateStatTable();
  const auto selector = GENERATE(10, 16, 17, 19, 20, 21, 22, 23);
  fixture.action.hits[1].statCalculatorId = selector;
  Battle_actionDefinitions[255].type = selector == 10 || selector == 20 ? 7 : 1;
  Battle_actionDefinitions[255].cost = 255;
  fixture.stats.currentHp = 201;
  fixture.stats.maxHp = 100;
  fixture.stats.currentMp = 302;
  fixture.stats.maxMp = 200;
  fixture.stats.shield.currentPp = 65535;
  const int expected = selector == 16   ? -101
                       : selector == 17 ? -102
                       : selector == 21 ? 790
                       : selector == 23 ? -1
                                        : -158;
  CHECK(battle::LiveCalculateActionStatChange(&fixture.action, &fixture.source,
                                              &fixture.target, 1,
                                              -1) == expected);
  CHECK(Battle_lastActionStatValue == static_cast<std::uint32_t>(expected));
}
