#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/StatusLifecycle.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Content/ModuleData.h"
#include "Items/EquipmentAffinity.hpp"
extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/StatusEffects.h"
}

namespace {
namespace battle = VS::Battle;
namespace items = VS::Items;
using State = battle::StatusLifecycleState;
using Event = std::array<std::uint32_t, 4>;
constexpr battle::StatusDurations kDurations{15, 20, 17, 19, 2, 5};
constexpr std::array<std::uint32_t, 4> kLimbStatuses{1, 2, 4, 8};

struct World final : battle::StatusLifecycleWorld {
  std::vector<Event> events;
  std::vector<State> observations;
  std::function<void(State&)> movement;
  std::function<void(State&)> label;
  int draw = 0;

  void Observe(State& state, Event event) {
    events.push_back(event);
    observations.push_back(state);
  }
  std::int32_t RandomBounded(State& state, int bound) override {
    Observe(state, {0, static_cast<std::uint32_t>(bound), 0, 0});
    return draw;
  }
  std::int32_t RandomAveraged(State& state, int bound) override {
    Observe(state, {1, static_cast<std::uint32_t>(bound), 0, 0});
    return draw;
  }
  void UpdateMovement(State& state) override {
    Observe(state, {2, state.actor_id, state.walk, state.run});
    if (movement) movement(state);
  }
  void EquipmentLabel(State& state, int name, int stat,
                      std::uint32_t amount) override {
    Observe(state, {3, static_cast<std::uint32_t>(name),
                    static_cast<std::uint32_t>(stat), amount});
  }
  void ValueLabel(State& state, std::uint32_t amount) override {
    Observe(state, {4, state.actor_id, amount, 0});
  }
  void StatusLabels(State& state, std::uint32_t statuses) override {
    Observe(state, {5, state.actor_id, statuses, 1});
    if (label) label(state);
  }
  void RebuildWeapon(State& state) override {
    Observe(state, {6, 0, 0, 0});
    items::RebuildWeaponClassAffinity(state.weapon);
  }
  void RebuildShield(State& state) override {
    Observe(state, {7, 0, 0, 0});
    items::RebuildShieldClassAffinity(state.shield);
  }
  void RebuildArmor(State& state, std::size_t limb) override {
    Observe(state, {8, static_cast<std::uint32_t>(limb), 0, 0});
    items::RebuildArmorClassAffinity(state.limbs.at(limb).armor);
  }
  void EnchantWeapon(State& state,
                     const battle::ElementEnchantment& change) override {
    Observe(state, {9, static_cast<std::uint32_t>(change.increased_affinity),
                    static_cast<std::uint32_t>(change.decreased_affinity),
                    static_cast<std::uint32_t>(change.amount)});
    REQUIRE(battle::ApplyWeaponElementEnchantment(state.weapon, change));
  }
  void EnchantArmor(State& state,
                    const battle::ElementEnchantment& change) override {
    Observe(state, {10, static_cast<std::uint32_t>(change.increased_affinity),
                    static_cast<std::uint32_t>(change.decreased_affinity),
                    static_cast<std::uint32_t>(change.amount)});
    std::array<items::ArmorLoadout, items::kLimbCount> armor{};
    for (std::size_t index = 0; index < armor.size(); ++index) {
      armor.at(index) = state.limbs.at(index).armor;
    }
    REQUIRE(battle::ApplyArmorElementEnchantment(state.shield, armor, change));
    for (std::size_t index = 0; index < armor.size(); ++index) {
      state.limbs.at(index).armor = armor.at(index);
    }
  }
  void RefreshEquipmentDisplay(State& state) override {
    Observe(state, {11, 0, 0, 0});
  }
};

State Actor() {
  State state{};
  state.hp = 100;
  state.max_hp = 1000;
  state.strength = {11, 101};
  state.intelligence = {111, 111};
  state.agility = {13, 121};
  state.base_walk = 205;
  state.base_run = 255;
  state.mobility_limbs = {4, 5};
  for (auto& limb : state.limbs) limb.max_hp = 100;
  return state;
}

struct LiveFixture {
  Battle_Actor* saved_player = Battle_characterState;
  Battle_Ai_ActorAiState* saved_ai = Battle_Ai_actorAiStates[0];
  Battle_StatusEffectParams saved_attribute = Battle_statusEffectParams[5];
  Battle_StatusEffectParams saved_movement = Battle_statusEffectParams[11];
  Battle_StatusEffectParams saved_poison = Battle_statusEffectParams[14];
  Battle_StatusEffectParams saved_regeneration = Battle_statusEffectParams[17];
  Battle_Actor player{};
  Battle_ActorStats stats{};
  Battle_Ai_ActorAiState ai{};
  LiveFixture() {
    player.stats = &stats;
    Battle_characterState = &player;
    Battle_Ai_actorAiStates[0] = &ai;
    stats.currentHp = -32768;
    stats.currentMp = -123;
    stats.risk = 65535;
    stats.actionRecoveryTimer = 0x1234;
    stats.cancelsHitResults = 1;
    stats.recoveryDelayBonus = 63;
    stats.baseWalkSpeed = std::bit_cast<char>(std::uint8_t{205});
    stats.baseRunSpeed = std::bit_cast<char>(std::uint8_t{255});
    stats.totalStrength = 65535;
    Battle_statusEffectParams[5].duration = 0x12FF;
    Battle_statusEffectParams[11].duration = 0x3401;
    Battle_statusEffectParams[14].tickInterval = 0x5602;
    Battle_statusEffectParams[17].tickInterval = 0x7805;
  }
  ~LiveFixture() {
    Battle_characterState = saved_player;
    Battle_Ai_actorAiStates[0] = saved_ai;
    Battle_statusEffectParams[5] = saved_attribute;
    Battle_statusEffectParams[11] = saved_movement;
    Battle_statusEffectParams[14] = saved_poison;
    Battle_statusEffectParams[17] = saved_regeneration;
  }
};
}  // namespace

TEST_CASE("Preserving Ashley calls zero removal without clearing any state",
          "[status-lifecycle][F00254]") {
  auto state = Actor();
  state.statuses = 0xFFFFFFFF;
  state.timers.fill(255);
  state.mobility_limbs = {255, 254};
  state.weapon.affinity_modifiers.fill(123);
  const auto before = state;
  World world;
  REQUIRE(battle::PreserveAshleyStatuses(state, world));
  CHECK(state == before);
  CHECK(world.events.empty());
  REQUIRE(battle::PreserveAshleyStatuses(state, world));
  CHECK(state == before);
}

TEST_CASE("Removal acts on requested masks and only empties affected buckets",
          "[status-lifecycle][F00517]") {
  auto state = Actor();
  state.statuses = 0x100 | 0x4000;
  state.timers.fill(7);
  World world;
  REQUIRE(battle::RemoveActorStatuses(state, 0x60 | 0x80 | 0x20000, world));
  CHECK(state.statuses == (0x100 | 0x4000));
  CHECK(state.strength.current == 101);
  CHECK(state.intelligence.current == 111);
  CHECK(state.timers == std::array<std::uint8_t, 8>{7, 7, 7, 7, 7, 7, 7, 7});
  REQUIRE(battle::RemoveActorStatuses(state, 0xFFFF, world));
  CHECK(state.statuses == 0);
  CHECK(state.timers == std::array<std::uint8_t, 8>{0, 0, 7, 0, 0, 0, 7, 7});
}

TEST_CASE("Removal restores curse gear and independent movement callbacks",
          "[status-lifecycle][F00517]") {
  auto state = Actor();
  state.weapon.blade.id = 1;
  state.weapon.attributes = {5, 65535, 7, 32768, 9, 10};
  state.limbs.at(4).hp = -1;
  state.limbs.at(5).max_hp = 0;
  World world;
  REQUIRE(battle::RemoveActorStatuses(state, 0x188810, world));
  CHECK(state.weapon.attributes.current_strength == -1);
  CHECK(state.weapon.attributes.current_intelligence == -32768);
  CHECK(state.limbs.at(4).hp == 2);
  CHECK(state.limbs.at(5).hp == 0);
  CHECK(world.events == std::vector<Event>{{2, 0, 205, 255}, {2, 0, 205, 255}});
  REQUIRE(battle::RemoveActorStatuses(state, 0x10000, world));
  CHECK(state.agility.current == 121);
}

TEST_CASE("Used mobility indices refuse before removal or timer writes",
          "[status-lifecycle][F00517][F00523]") {
  auto state = Actor();
  state.statuses = 0x10;
  state.mobility_limbs.at(1) = 6;
  state.timers.fill(1);
  const auto before = state;
  World world;
  CHECK_FALSE(battle::RemoveActorStatuses(state, 0x10, world));
  CHECK(state == before);
  CHECK_FALSE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(state == before);
  CHECK(world.events.empty());
  REQUIRE(battle::RemoveActorStatuses(state, 0x60, world));
  CHECK(state.strength.current == 101);
}

TEST_CASE(
    "Element removal respects growth boundaries and exact draw label order",
    "[status-lifecycle][F00517]") {
  auto state = Actor();
  state.weapon.blade.id = 1;
  state.weapon.blade.affinities.at(1) = 49;
  state.weapon.blade.affinities.at(3) = -99;
  state.weapon.affinity_modifiers.fill(42);
  World world;
  REQUIRE(battle::RemoveActorStatuses(state, 0x200000, world));
  CHECK(state.weapon.blade.affinities.at(1) == 50);
  CHECK(state.weapon.blade.affinities.at(3) == -100);
  CHECK(world.events == std::vector<Event>{{0, 8, 0, 0},
                                           {3, 240, 7, 1},
                                           {0, 16, 0, 0},
                                           {3, 240, 9, 0x80000001},
                                           {6, 0, 0, 0}});
  CHECK(state.weapon.affinity_modifiers.at(1) == 0);
  CHECK(state.weapon.class_affinity.affinities.at(0).at(3) == -100);
  world.events.clear();
  REQUIRE(battle::RemoveActorStatuses(state, 0x200000, world));
  CHECK(world.events == std::vector<Event>{{6, 0, 0, 0}});
}

TEST_CASE(
    "Armor element removal rebuilds absent slots and multi masks only flush",
    "[status-lifecycle][F00517]") {
  auto state = Actor();
  state.actor_id = 3;
  state.limbs.at(5).armor.armor.id = 1;
  state.limbs.at(5).name_index = 99;
  for (auto& limb : state.limbs) limb.armor.affinity_modifiers.fill(33);
  World world;
  REQUIRE(battle::RemoveActorStatuses(state, 0x4000000, world));
  CHECK(world.events.front() == Event{7, 0, 0, 0});
  CHECK(world.events.back() == Event{11, 0, 0, 0});
  CHECK(state.limbs.at(5).armor.armor.affinities.at(2) == 1);
  CHECK(state.limbs.at(0).armor.affinity_modifiers.at(4) == 0);
  world.events.clear();
  const auto before = state;
  REQUIRE(battle::RemoveActorStatuses(state, 0x6600000, world));
  CHECK(world.events == std::vector<Event>{{11, 0, 0, 0}});
  CHECK(state == before);
}

TEST_CASE(
    "Infliction keeps source attribute order curse override and timer "
    "precedence",
    "[status-lifecycle][F00520]") {
  auto state = Actor();
  state.statuses = 0x4000;
  state.strength.total = 65535;
  World world;
  battle::InflictActorStatuses(state, 0x7E0 | 0x20000, kDurations, world);
  CHECK(state.strength.current == 6552);
  CHECK(state.intelligence.current == 122);
  CHECK(state.agility.current == 133);
  CHECK(state.timers.at(5) == 5);
  battle::InflictActorStatuses(state, 0x10000 | 0x24000, kDurations, world);
  CHECK(state.strength.current == 58982);
  CHECK(state.intelligence.current == 100);
  CHECK(state.agility.current == 109);
  CHECK(state.timers.at(5) == 2);
  CHECK(state.statuses == (0x7E0 | 0x34000));
}

TEST_CASE(
    "Infliction uses current speed statuses byte wrap and callback mutations",
    "[status-lifecycle][F00520]") {
  auto state = Actor();
  state.weapon.blade.id = 1;
  state.weapon.attributes.current_strength = 100;
  World world;
  world.movement = [](auto& current) { current.intelligence.current = 200; };
  battle::InflictActorStatuses(state, 0x100800, kDurations, world);
  CHECK(world.events.at(0) == Event{2, 0, 0, 62});
  CHECK(world.observations.at(0).statuses == 0x100800);
  CHECK(state.weapon.attributes.current_strength == 114);
  state.statuses |= 0x10;
  battle::InflictActorStatuses(state, 0x800, kDurations, world);
  CHECK(state.walk == 102);
  CHECK(state.run == 127);
  CHECK(state.intelligence.current == 200);
}

TEST_CASE(
    "Infliction pairs single elements after signed equipment calculations",
    "[status-lifecycle][F00520]") {
  auto state = Actor();
  state.weapon.blade.id = state.shield.base.id = 1;
  state.weapon.attributes.current_intelligence = -113;
  World world;
  battle::InflictActorStatuses(state, 0x30680000, kDurations, world);
  CHECK(state.analyzed);
  CHECK(world.events == std::vector<Event>{{10, 4, 2, 0}});
  CHECK(state.weapon.attributes.current_strength == 0);
  world.events.clear();
  state.weapon.attributes.current_intelligence = 9;
  battle::InflictActorStatuses(state, 0x4400000, kDurations, world);
  CHECK(world.events == std::vector<Event>{{9, 2, 4, 60}, {10, 2, 4, 60}});
  CHECK(state.weapon.affinity_modifiers.at(4) == -60);
  CHECK(state.shield.affinity_modifiers.at(2) == 60);
}

TEST_CASE("Poison wins regeneration resets interval and floors signed HP",
          "[status-lifecycle][F00523]") {
  auto state = Actor();
  state.statuses = 0x24000;
  state.timers.at(5) = 1;
  state.hp = -20;
  World world;
  REQUIRE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(state.hp == 0);
  CHECK(state.timers.at(5) == 2);
  CHECK(world.events ==
        std::vector<Event>{{0, 6, 0, 0}, {4, 0, 0x80000001, 0}});
  CHECK(world.observations.at(0).timers.at(5) == 2);
  REQUIRE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(state.timers.at(5) == 1);
  CHECK(world.events.size() == 2);
}

TEST_CASE("Regeneration stores signed halfwords before upper clamps",
          "[status-lifecycle][F00523]") {
  auto state = Actor();
  state.statuses = 0x20000;
  state.timers.at(5) = 1;
  state.hp = state.max_hp = 32767;
  state.limbs.at(0).hp = state.limbs.at(0).max_hp = 32767;
  World world;
  REQUIRE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(state.hp == -31133);
  CHECK(state.limbs.at(0).hp == -32565);
  CHECK(world.events.back() == Event{4, 0, 1636, 0});
  state.max_hp = 0;
  state.timers.at(5) = 1;
  const auto limb_hp = state.limbs.at(0).hp;
  REQUIRE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(state.limbs.at(0).hp == limb_hp);
  CHECK(world.events.back() == Event{4, 0, 0xFFFFFFFE, 0});
}

TEST_CASE("Timer expiry labels precede removal and preserve callback changes",
          "[status-lifecycle][F00523]") {
  auto state = Actor();
  state.statuses = 0x60 | 0x2000;
  state.timers = {1, 0, 2, 1, 0, 0, 255, 255};
  World world;
  world.label = [](auto& current) { current.strength.total = 777; };
  REQUIRE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(world.events == std::vector<Event>{{5, 0, 0x2060, 1}});
  CHECK(world.observations.at(0).statuses == 0x2060);
  CHECK(state.strength.current == 777);
  CHECK(state.statuses == 0);
  CHECK(state.timers ==
        std::array<std::uint8_t, 8>{0, 0, 1, 0, 0, 0, 255, 255});
}

TEST_CASE("Limb recovery uses either mobility limb but player flags use limb i",
          "[status-lifecycle][F00523]") {
  auto state = Actor();
  state.statuses = 0x1F;
  state.limbs.at(1).hp = 2;
  state.limbs.at(5).hp = 2;
  World world;
  REQUIRE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(world.events.front() == Event{5, 0, 0x12, 1});
  CHECK(state.statuses == 0xD);
  state.mobility_limbs.at(0) = 0;
  state.limbs.at(0).hp = 2;
  REQUIRE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(state.statuses == 0xD);
}

TEST_CASE("Player limb recovery uses a nonzero mobility byte only as a guard",
          "[status-lifecycle][F00523]") {
  auto state = Actor();
  state.statuses = 1;
  state.mobility_limbs = {255, 254};
  state.limbs.at(0).hp = 2;
  World world;
  REQUIRE(
      battle::UpdateActorStatusTimers(state, kDurations, kLimbStatuses, world));
  CHECK(state.statuses == 0);
  CHECK(world.events == std::vector<Event>{{5, 0, 1, 1}});
}

TEST_CASE("Live preservation resolves Ashley rather than the actor table",
          "[status-lifecycle][F00254]") {
  auto* saved_player = Battle_characterState;
  Battle_ActorStats stats{};
  Battle_Actor player{};
  player.stats = &stats;
  stats.statuses = -1;
  stats.currentHp = -32768;
  stats.currentMp = -123;
  stats.risk = 65535;
  stats.isAnalyzed = 1;
  stats.statusTimers[7] = 255;
  const auto before =
      std::bit_cast<std::array<std::byte, sizeof(stats)>>(stats);
  Battle_characterState = &player;
  battle::PreserveLiveAshleyStatuses();
  Battle_characterState = saved_player;
  CHECK(std::bit_cast<std::array<std::byte, sizeof(stats)>>(stats) == before);
}

TEST_CASE("Live infliction maps unsigned speed bytes and low-byte durations",
          "[status-lifecycle][F00520][live]") {
  LiveFixture fixture;
  battle::InflictLiveActorStatuses(0x20024840, &fixture.stats);
  CHECK(fixture.stats.statusTimers[0] == 255);
  CHECK(fixture.stats.statusTimers[1] == 1);
  CHECK(fixture.stats.statusTimers[5] == 2);
  CHECK(static_cast<std::uint8_t>(fixture.stats.walkSpeed) == 0);
  CHECK(static_cast<std::uint8_t>(fixture.stats.runSpeed) == 62);
  CHECK(fixture.ai.runStep == 62);
  CHECK(fixture.stats.strength == 6552);
  CHECK(fixture.stats.isAnalyzed == 1);
}

TEST_CASE("Live status writes preserve signed HP MP risk and neighboring flags",
          "[status-lifecycle][F00517][F00520][live]") {
  LiveFixture fixture;
  battle::InflictLiveActorStatuses(0x20000000, &fixture.stats);
  battle::RemoveLiveActorStatuses(0x20000000, &fixture.stats);
  CHECK(fixture.stats.currentHp == -32768);
  CHECK(fixture.stats.currentMp == -123);
  CHECK(fixture.stats.risk == 65535);
  CHECK(fixture.stats.actionRecoveryTimer == 0x1234);
  CHECK(fixture.stats.cancelsHitResults == 1);
  CHECK(fixture.stats.recoveryDelayBonus == 63);
  CHECK(fixture.stats.isAnalyzed == 1);
  CHECK(fixture.stats.statuses == 0);
}

TEST_CASE("Lifecycle module bindings execute their typed signatures",
          "[status-lifecycle][F00254][F00517][F00520][F00523][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  LiveFixture fixture;
  using Change = void (*)(int, Battle_ActorStats*);
  using Tick = void (*)(Battle_ActorStats*);
  using Preserve = void (*)();
  const auto inflict = reinterpret_cast<Change>(resolve(0x80086FA8));
  const auto remove = reinterpret_cast<Change>(resolve(0x80086754));
  const auto tick = reinterpret_cast<Tick>(resolve(0x80087EF4));
  const auto preserve = reinterpret_cast<Preserve>(resolve(0x8007C31C));
  REQUIRE(inflict == battle::InflictLiveActorStatuses);
  REQUIRE(remove == battle::RemoveLiveActorStatuses);
  REQUIRE(tick == battle::UpdateLiveActorStatusTimers);
  REQUIRE(preserve == battle::PreserveLiveAshleyStatuses);
  inflict(0x40, &fixture.stats);
  preserve();
  CHECK(fixture.stats.statuses == 0x40);
  tick(&fixture.stats);
  CHECK(fixture.stats.statusTimers[0] == 254);
  remove(0x40, &fixture.stats);
  CHECK(fixture.stats.statuses == 0);
  CHECK(fixture.stats.strength == 65535);
}
