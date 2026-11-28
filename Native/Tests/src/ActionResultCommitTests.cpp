#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorInstances.hpp"
#include "Battle/ActionPreview.hpp"
#include "Battle/ActionResults.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/CombatImpact.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hit.hpp"
#include "Content/ModuleData.hpp"
#include "Items/ItemRecords.hpp"
#include "TestSupport/StatusRoot.hpp"

namespace {
namespace battle = VS::Battle;
using Field = battle::ActionResultField;
using Event = std::array<std::uint32_t, 5>;
struct Stats final : battle::ActorVitalsAccess {
  battle::ActorVitalsState state{};
  battle::ActorVitalsState Read() override { return state; }
  void Write(const battle::ActorVitalsState& value) override { state = value; }
};
struct World final : battle::ActorActionResultWorld {
  Stats stats;
  Stats other;
  battle::ActorActionResult hit{};
  std::vector<Event> events;
  std::vector<battle::ActorVitalsState> snapshots;
  std::function<void()> on_commit;
  std::function<void()> on_number;
  std::function<void(bool)> on_label;
  std::function<void(bool)> on_status;
  std::uint8_t bound = 255;
  bool missing = false;
  bool replaced = false;
  void CommitEquipmentStats() override {
    events.push_back({0, 0, 0, 0, 0});
    if (on_commit) on_commit();
  }
  battle::ActorActionResult ReadHit() override { return hit; }
  battle::ActorVitalsAccess* TargetStats(std::uint8_t slot) override {
    bound = slot;
    return missing ? nullptr : (replaced ? &other : &stats);
  }
  void Number(int slot, int limb, std::uint32_t amount, int label) override {
    events.push_back({1, static_cast<std::uint32_t>(slot),
                      static_cast<std::uint32_t>(limb), amount,
                      static_cast<std::uint32_t>(label)});
    snapshots.push_back(stats.state);
    if (on_number) on_number();
  }
  void StatusLabel(int slot, std::uint32_t mask, bool removing) override {
    events.push_back({2, static_cast<std::uint32_t>(slot), 0, mask,
                      static_cast<std::uint32_t>(removing)});
    if (on_label) on_label(removing);
  }
  void RemoveStatuses(battle::ActorVitalsAccess& owner,
                      std::uint32_t mask) override {
    events.push_back({3, mask, 0, 0, 0});
    auto state = owner.Read();
    state.status.statuses &= ~mask;
    owner.Write(state);
    if (on_status) on_status(true);
  }
  void InflictStatuses(battle::ActorVitalsAccess& owner,
                       std::uint32_t mask) override {
    events.push_back({4, mask, 0, 0, 0});
    auto state = owner.Read();
    state.status.statuses |= mask;
    owner.Write(state);
    if (on_status) on_status(false);
  }
  void Change(Field field, std::int16_t amount, std::uint8_t mode) {
    hit.changes.at(static_cast<std::size_t>(field)) = {amount, mode};
  }
};

struct ImpactRandom final : VS::Items::ItemRandom {
  std::vector<int> bounds;
  std::int32_t NextBounded(std::int32_t bound) override {
    bounds.push_back(bound);
    return 0;
  }
  std::int32_t NextAveraged(std::int32_t bound) override {
    bounds.push_back(-bound);
    return 0;
  }
};

struct ImpactFixture {
  VS::Actors::ActorInstanceTable identities;
  VS::Actors::SpawnedActor actor = identities.Begin(0, 1, {});
  ImpactRandom random;
  battle::ActorVitalsState state{};
  battle::StatusDurations durations{};
  battle::CombatHitResult hit{};

  ImpactFixture() {
    if (!identities.Publish(0, actor)) FAIL("publish impact fixture");
    hit.target = battle::CombatActorTarget{actor, 2};
    hit.changes.limb = 2;
    state.status.hp = 80;
    state.status.max_hp = 100;
    state.status.limbs.at(2).hp = 5;
    state.status.limbs.at(2).max_hp = 10;
    state.status.limbs.at(2).armor.current_dp = 9;
  }
  void DamageAndInjury() {
    state.status.statuses = 0x8000;
    state.status.base_walk = 8;
    state.status.base_run = 16;
    state.status.walk = 4;
    state.status.run = 8;
    state.status.timers.at(4) = 99;
    durations.movement = 45;
    hit.changes.changes.at(static_cast<std::size_t>(Field::kHp)) = {20, 1};
    hit.changes.limbs.at(2) = {5, 2};
    hit.changes.inflict_statuses = 0x8000;
    hit.changes.remove_statuses = 0x10;
  }
  std::expected<battle::CombatImpactResult, std::string> Apply() {
    return battle::ApplyCombatImpact(hit, {actor, 0, state, durations, random});
  }
  battle::CombatImpactResult Run() {
    auto result = Apply();
    if (!result) FAIL(result.error());
    return *result;
  }
};
}  // namespace

TEST_CASE("Detached impact owns damage, wear and complete injury status state",
          "[combat-impact][issue-56]") {
  ImpactFixture f;
  f.DamageAndInjury();
  const auto before = f.state;
  auto expected = before;
  expected.status.hp = 60;
  expected.status.limbs.at(2).hp = 0;
  expected.status.limbs.at(2).armor.current_dp = 7;
  expected.status.statuses = 0x10;
  expected.status.timers.at(1) = 45;
  expected.status.timers.at(4) = 0;
  const auto result = f.Run();
  CHECK(result.state == expected);
  CHECK(f.state == before);
  CHECK(f.random.bounds.empty());
}

TEST_CASE("Detached impact reports ordered owner-qualified presentation events",
          "[combat-impact][issue-56]") {
  ImpactFixture f;
  f.DamageAndInjury();
  const auto result = f.Run();
  const std::vector<battle::CombatImpactEvent> expected{
      battle::CombatEquipmentFlush{},
      battle::CombatNumberEvent{f.actor, 2, 0x80000014, 0},
      battle::CombatStatusEvent{f.actor, 0x8000, true},
      battle::CombatMovementChange{f.actor, 8, 16},
      battle::CombatStatusEvent{f.actor, 0x10, false},
      battle::CombatMovementChange{f.actor, 4, 8}};
  CHECK(result.events == expected);
}

TEST_CASE("Exempt stats keep their role without replacing the impact owner",
          "[combat-impact][issue-56]") {
  ImpactFixture f;
  f.state.status.actor_id = 128;
  f.hit.changes.changes.at(static_cast<std::size_t>(Field::kHp)) = {20, 1};
  const auto result = f.Run();
  CHECK(result.state.status.actor_id == 128);
  CHECK(result.state.status.hp == 60);
  CHECK(result.events.at(1) ==
        battle::CombatImpactEvent{
            battle::CombatNumberEvent{f.actor, 2, 0x80000014, 0}});
}

TEST_CASE("Detached impact preserves elemental removal draws and growth",
          "[combat-impact][issue-56]") {
  ImpactFixture f;
  f.state.status.statuses = f.hit.changes.inflict_statuses = 0x200000;
  auto& weapon = f.state.status.weapon;
  weapon.blade.id = 1;
  weapon.blade.affinities.at(1) = 4;
  weapon.blade.affinities.at(3) = 9;
  weapon.affinity_modifiers.at(1) = 25;
  weapon.affinity_modifiers.at(3) = -25;
  const auto result = f.Run();
  CHECK(f.random.bounds == std::vector<int>{8, 16});
  CHECK(result.state.status.statuses == 0);
  CHECK(result.state.status.weapon.blade.affinities.at(1) == 5);
  CHECK(result.state.status.weapon.blade.affinities.at(3) == 8);
  CHECK(result.state.status.weapon.affinity_modifiers.at(1) == 0);
  CHECK(result.state.status.weapon.affinity_modifiers.at(3) == 0);
  CHECK(result.events.at(2) ==
        battle::CombatImpactEvent{
            battle::CombatEquipmentEvent{f.actor, 0xF0, 7, 1}});
  CHECK(result.events.at(3) ==
        battle::CombatImpactEvent{
            battle::CombatEquipmentEvent{f.actor, 0xF0, 9, 0x80000001}});
}

TEST_CASE("Detached impact refuses stale identity and invalid used state",
          "[combat-impact][issue-56]") {
  ImpactFixture f;
  f.DamageAndInjury();
  const int invalid = GENERATE(0, 1, 2);
  if (invalid == 0) {
    REQUIRE(f.identities.Remove(0, f.actor));
    f.actor = f.identities.Begin(0, 1, {});
    REQUIRE(f.identities.Publish(0, f.actor));
  } else if (invalid == 1) {
    f.state.status.statuses = 0x10;
    f.hit.changes.inflict_statuses = 0x10;
    f.state.status.mobility_limbs.front() = 6;
  } else {
    f.hit.changes.limb = -1;
    f.hit.changes.changes.at(static_cast<std::size_t>(Field::kArmorDp)) = {1,
                                                                           1};
  }
  const auto before = f.state;
  const auto result = f.Apply();
  REQUIRE_FALSE(result.has_value());
  CHECK_FALSE(result.error().empty());
  CHECK(f.state == before);
  CHECK(f.random.bounds.empty());
}

TEST_CASE("Action result conversion preserves signed limbs and effect lanes",
          "[action-result-commit][combat-rules]") {
  battle::HitResult original{};
  original.target.target_actor = 3;
  original.target.target_limb = -1;
  original.hp_effect = 3;
  original.weapon_dp_effect = 1;
  original.shield_pp_effect = 2;
  original.stat_status_effect = 2;
  original.target.hp = 32767;
  original.weapon_dp_change = -32768;
  original.shield_pp_change = -19;
  original.limbs[4].effects.hp_change = -32768;
  original.limbs[4].effects.armor_dp_change = 32767;
  auto hit = std::bit_cast<battle::ActionHit>(original);
  const auto result = battle::ReadActionHitResult(hit);
  CHECK(result.limb == -1);
  CHECK(result.changes.at(static_cast<std::size_t>(Field::kHp)) ==
        battle::ActionResultChange{32767, 3});
  CHECK(result.changes.at(static_cast<std::size_t>(Field::kWeaponDp)) ==
        battle::ActionResultChange{-32768, 1});
  CHECK(result.changes.at(static_cast<std::size_t>(Field::kShieldPp)) ==
        battle::ActionResultChange{-19, 2});
  CHECK(result.limbs.at(4) == battle::ActionResultLimb{-32768, 32767});
  battle::WriteActionHitResult(result, hit);
  CHECK(hit == std::bit_cast<battle::ActionHit>(original));
}

TEST_CASE("Result commit precedes reads then retains target and stats owner",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  world.on_commit = [&] {
    world.hit.target = 4;
    world.hit.impact = 4;
    world.hit.limb = -1;
    world.stats.state.status.hp = 10;
    world.stats.state.status.max_hp = 100;
    world.Change(Field::kHp, 1, 2);
  };
  world.on_number = [&] {
    world.hit.target = 5;
    world.replaced = true;
    world.Change(Field::kHp, 7, 3);
    world.stats.state.status.hp = 40;
  };
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(world.bound == 4);
  CHECK(world.events.at(1) == Event{1, 4, 0xFFFFFFFF, 0, 2});
  CHECK(world.events.at(2) == Event{1, 4, 0xFFFFFFFF, 0x80000007, 0});
  CHECK(world.stats.state.status.hp == 33);
  CHECK(world.other.state.status.hp == 0);
}

TEST_CASE("HP result modes affect all six limbs with signed subtraction",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  const auto mode = GENERATE(0, 1, 2, 3);
  world.stats.state.status.hp = 10;
  world.stats.state.status.max_hp = 12;
  for (std::size_t index = 0; index < 6; ++index) {
    world.stats.state.status.limbs.at(index).hp = 10;
    world.stats.state.status.limbs.at(index).max_hp = 12;
    world.stats.state.status.limbs.at(index).armor.current_dp = 1;
    world.hit.limbs.at(index) = {5, -2};
  }
  world.Change(Field::kHp, 5, static_cast<std::uint8_t>(mode));
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(world.stats.state.status.hp == (mode == 0 ? 10 : mode == 2 ? 12 : 5));
  for (const auto& limb : world.stats.state.status.limbs) {
    CHECK(limb.hp == (mode == 0 ? 10 : mode == 2 ? 12 : 5));
    CHECK(limb.armor.current_dp == (mode == 1 || mode == 3 ? 3 : 1));
  }
}

TEST_CASE("Signed HP MP and maxima wrap halfwords before their upper clamps",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  world.stats.state.status.hp = 32767;
  world.stats.state.status.max_hp = 32767;
  world.stats.state.mp = 32767;
  world.stats.state.max_mp = 32767;
  world.Change(Field::kHp, 1, 2);
  world.Change(Field::kMp, 1, 2);
  world.Change(Field::kMaxHp, 1, 2);
  world.Change(Field::kMaxMp, 1, 2);
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(world.stats.state.status.hp == -32768);
  CHECK(world.stats.state.mp == -32768);
  CHECK(world.stats.state.status.max_hp == -32768);
  CHECK(world.stats.state.max_mp == -32768);
  CHECK(world.snapshots.at(1).status.hp == -32768);
}

TEST_CASE(
    "Negative amounts retain sign extension in labels and increase reductions",
    "[actor-vitals][action-result-commit][F00522]") {
  World world;
  world.stats.state.status.hp = -1;
  world.stats.state.mp = 10;
  world.stats.state.risk = 65535;
  world.Change(Field::kHp, -2, 1);
  world.Change(Field::kMp, -3, 2);
  world.Change(Field::kRisk, -2, 2);
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(world.stats.state.status.hp == 1);
  CHECK(world.stats.state.mp == 0);
  CHECK(world.stats.state.risk == 1);
  CHECK(world.events.at(1) == Event{1, 0, 0, 0xFFFFFFFE, 0});
  CHECK(world.events.at(2) == Event{1, 0, 0, 0xFFFFFFFD, 1});
  CHECK(world.events.at(3) == Event{1, 0, 0, 0xFFFFFFFE, 8});
}

TEST_CASE("MP risk and maximum modes preserve their asymmetric no-op branches",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  const auto mode = GENERATE(0, 1, 2, 3);
  world.stats.state.mp = 10;
  world.stats.state.max_mp = 20;
  world.stats.state.status.max_hp = 20;
  world.stats.state.risk = 10;
  for (const auto field :
       {Field::kMp, Field::kRisk, Field::kMaxHp, Field::kMaxMp}) {
    world.Change(field, 3, static_cast<std::uint8_t>(mode));
  }
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(world.stats.state.mp == (mode == 1 ? 7 : mode == 2 ? 13 : 10));
  CHECK(world.stats.state.risk == (mode == 2 ? 7 : 10));
  CHECK(world.stats.state.status.max_hp == (mode == 2 ? 23 : 20));
  CHECK(world.stats.state.max_mp == (mode == 2 ? 23 : 20));
}

TEST_CASE("All attributes use unsigned halfword wrap and signed delta floors",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  const auto mode = GENERATE(0, 1, 2, 3);
  world.stats.state.status.strength = {65535, 998};
  world.stats.state.status.intelligence = {65535, 998};
  world.stats.state.status.agility = {65535, 998};
  for (const auto field :
       {Field::kStrength, Field::kIntelligence, Field::kAgility}) {
    world.Change(field, 2, static_cast<std::uint8_t>(mode));
  }
  REQUIRE(battle::ApplyActorActionResult(world));
  const battle::StatusAttributes expected{
      static_cast<std::uint16_t>(mode == 1   ? 65533
                                 : mode == 2 ? 1
                                             : 65535),
      static_cast<std::uint16_t>(mode == 1   ? 996
                                 : mode == 2 ? 999
                                             : 998)};
  CHECK(world.stats.state.status.strength == expected);
  CHECK(world.stats.state.status.intelligence == expected);
  CHECK(world.stats.state.status.agility == expected);
}

TEST_CASE("Equipment ignores equipped ids and applies each mode asymmetrically",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  const auto mode = GENERATE(0, 1, 2, 3);
  auto& state = world.stats.state.status;
  state.weapon.current_dp = 65535;
  state.weapon.max_dp = 100;
  state.weapon.current_pp = 1;
  state.weapon.max_pp = 2;
  state.shield.current_dp = 1;
  state.shield.current_pp = 1;
  for (auto& limb : state.limbs) limb.armor.current_dp = 1;
  world.hit.limb = GENERATE(0, 1, 2, 3, 4, 5);
  for (const auto field : {Field::kWeaponDp, Field::kWeaponPp, Field::kArmorDp,
                           Field::kShieldPp}) {
    world.Change(field, 3, static_cast<std::uint8_t>(mode));
  }
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(state.weapon.current_dp == (mode == 2 ? 2 : 65535));
  CHECK(state.weapon.current_pp == (mode == 1 ? 0 : mode == 2 ? 2 : 1));
  CHECK(state.shield.current_dp == (mode == 1 ? 0 : 1));
  CHECK(state.shield.current_pp == (mode == 1 ? 0 : 1));
  CHECK(state.limbs.at(static_cast<std::size_t>(world.hit.limb))
            .armor.current_dp == (mode == 1 ? 0 : 1));
}

TEST_CASE("Result refuses only missing stats and a selected used armor index",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  world.missing = true;
  CHECK_FALSE(battle::ApplyActorActionResult(world));
  world.missing = false;
  world.hit.limb = GENERATE(-128, -1, 6, 127);
  CHECK(battle::ApplyActorActionResult(world));
  world.Change(Field::kArmorDp, 1, 2);
  CHECK(battle::ApplyActorActionResult(world));
  world.Change(Field::kArmorDp, 1, 1);
  CHECK_FALSE(battle::ApplyActorActionResult(world));
}

TEST_CASE("Dead enemy status labels hide but all 32 status bits still mutate",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  world.hit.target = GENERATE(0, 3);
  world.stats.state.status.statuses = 0x80000001;
  world.hit.inflict_statuses = 0x80000001;
  world.hit.remove_statuses = 0x80000001;
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(world.stats.state.status.statuses == 0x80000001);
  CHECK(world.events.size() == (world.hit.target == 0 ? 9 : 5));
  if (world.hit.target == 3) {
    CHECK(world.events == std::vector<Event>{{0, 0, 0, 0, 0},
                                             {3, 1, 0, 0, 0},
                                             {3, 0x80000000, 0, 0, 0},
                                             {4, 1, 0, 0, 0},
                                             {4, 0x80000000, 0, 0, 0}});
  }
}

TEST_CASE(
    "Status labels can change masks before lifecycle and HP before next bit",
    "[actor-vitals][action-result-commit][F00522]") {
  World world;
  world.hit.target = 2;
  world.stats.state.status.hp = 5;
  world.stats.state.status.statuses = 3;
  world.hit.inflict_statuses = 3;
  world.on_label = [&](bool removing) {
    if (removing) {
      world.hit.inflict_statuses = 2;
      world.stats.state.status.hp = 0;
      world.hit.target = 0;
      world.replaced = true;
    }
  };
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(world.events == std::vector<Event>{{0, 0, 0, 0, 0},
                                           {2, 2, 0, 1, 1},
                                           {3, 0, 0, 0, 0},
                                           {3, 2, 0, 0, 0}});
  CHECK(world.stats.state.status.statuses == 1);
  CHECK(world.stats.state.status.hp == 0);
  CHECK(world.other.state.status.statuses == 0);
}

TEST_CASE("Lifecycle callback mutations are visible to later bits and add pass",
          "[actor-vitals][action-result-commit][F00522]") {
  World world;
  world.hit.target = 2;
  world.stats.state.status.statuses = 3;
  world.hit.inflict_statuses = 1;
  world.on_status = [&](bool removing) {
    if (removing) {
      world.stats.state.status.hp = 4;
      world.hit.inflict_statuses = 3;
      world.hit.remove_statuses = 0x80000000;
    }
  };
  REQUIRE(battle::ApplyActorActionResult(world));
  CHECK(world.stats.state.status.statuses == 0x80000000);
  CHECK(world.events.at(2) == Event{2, 2, 0, 2, 1});
  CHECK(world.events.back() == Event{4, 0x80000000, 0, 0, 0});
}

TEST_CASE(
    "Live action result module binding preserves HP wrap and unused fields",
    "[actor-vitals][action-result-commit][F00522][live]") {
  VS::test_support::StatusRoot status_catalog;
  const auto symbols = VS::Content::ModuleSymbols();
  const auto apply = reinterpret_cast<void (*)(VS::Battle::HitResult*)>(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800875C8));
  REQUIRE(apply == battle::ApplyLiveActorActionResult);
  VS::Battle::ActorStats stats{};
  VS::Battle::Actor actor{};
  VS::Battle::HitResult hit{};
  auto* saved = VS::Battle::actors[41];
  actor.stats = &stats;
  VS::Battle::actors[41] = &actor;
  stats.current_hp = 0;
  stats.current_mp = -123;
  stats.action_recovery_timer = 65535;
  stats.is_analyzed = 1;
  stats.weapon.current_dp = 65535;
  stats.weapon.max_dp = 100;
  hit.target.target_actor = 41;
  hit.target.target_limb = -1;
  hit.weapon_dp_effect = 2;
  hit.weapon_dp_change = 2;
  hit.remove_statuses = std::bit_cast<int>(0x80000000U);
  apply(&hit);
  VS::Battle::actors[41] = saved;
  CHECK(stats.weapon.current_dp == 1);
  CHECK(stats.current_mp == -123);
  CHECK(stats.action_recovery_timer == 65535);
  CHECK(stats.is_analyzed == 1);
  CHECK(std::bit_cast<std::uint32_t>(stats.statuses) == 0x80000000);
}
