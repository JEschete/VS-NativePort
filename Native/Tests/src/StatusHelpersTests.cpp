#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/StatusHelpers.hpp"
#include "Battle/StatusHelpersLive.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/StatusEffects.h"
#include "Field/Interactions.h"
#include "State/Timers.h"
}

namespace {
namespace battle = VS::Battle;
namespace items = VS::Items;

struct NumberWorld final : battle::StatNumberWorld {
  std::optional<std::uint8_t> spawn = 0;
  mutable std::vector<int> reads;
  std::vector<battle::StatNumberRequest> labels;

  std::optional<std::uint8_t> RoomSpawnIndex(int slot) const override {
    reads.push_back(slot);
    return spawn;
  }
  void QueueValueLabel(const battle::StatNumberRequest& request) override {
    labels.push_back(request);
  }
};

struct RiskWorld final : battle::TargetingRiskWorld {
  std::uint32_t control = 2;
  std::uint32_t flow = 1;
  std::int32_t ticks = 1;
  std::optional<battle::TargetingRiskTarget> target =
      battle::TargetingRiskTarget{0, 8, false};
  std::optional<std::uint16_t> risk = 99;
  mutable int target_reads = 0;
  mutable int risk_reads = 0;
  int writes = 0;

  std::uint32_t ControlMode() const override { return control; }
  std::uint32_t ActionFlowMode() const override { return flow; }
  std::int32_t TickSpeed() const override { return ticks; }
  std::optional<battle::TargetingRiskTarget> SelectedTarget() const override {
    ++target_reads;
    return target;
  }
  std::optional<std::uint16_t> PlayerRisk() const override {
    ++risk_reads;
    return risk;
  }
  void SetPlayerRisk(std::uint16_t value) override {
    ++writes;
    risk = value;
  }
};

template <typename T>
auto Bytes(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

struct LiveRiskFixture {
  Battle_Actor* saved_actor = Battle_actors[0];
  Battle_Actor* saved_player = Battle_characterState;
  Battle_ActionState* saved_action = Battle_actionState;
  std::uint32_t saved_control = Field_controlMode;
  std::uint32_t saved_flow = Battle_actionFlowMode;
  int saved_ticks = State_tickSpeed;
  int saved_timer = Battle_targetingRiskTimer;
  Battle_Actor target_actor{};
  Battle_Actor player_actor{};
  Battle_ActorStats target_stats{};
  Battle_ActorStats player_stats{};
  Battle_ActionState action{};

  LiveRiskFixture() {
    target_actor.stats = &target_stats;
    player_actor.stats = &player_stats;
    target_stats.statuses = 8;
    target_stats.risk = 55;
    player_stats.risk = 99;
    Battle_actors[0] = &target_actor;
    Battle_characterState = &player_actor;
    Battle_actionState = &action;
    Field_controlMode = 2;
    Battle_actionFlowMode = 1;
    State_tickSpeed = 1;
    Battle_targetingRiskTimer = 0;
  }
  ~LiveRiskFixture() {
    Battle_actors[0] = saved_actor;
    Battle_characterState = saved_player;
    Battle_actionState = saved_action;
    Field_controlMode = saved_control;
    Battle_actionFlowMode = saved_flow;
    State_tickSpeed = saved_ticks;
    Battle_targetingRiskTimer = saved_timer;
  }
};
}  // namespace

TEST_CASE("Weapon enchantment wraps halfwords and preserves baseline rows",
          "[status-helpers][F00518]") {
  items::WeaponLoadout weapon{};
  weapon.blade.id = 1;
  weapon.class_affinity.affinities.at(0).at(1) = 32760;
  weapon.class_affinity.affinities.at(0).at(3) = -32760;
  weapon.class_affinity.affinities.at(1).fill(17);
  auto expected = weapon;
  expected.class_affinity.affinities.at(0).at(1) = -32766;
  expected.class_affinity.affinities.at(0).at(3) = 32766;
  expected.affinity_modifiers.at(1) = 10;
  expected.affinity_modifiers.at(3) = -10;
  REQUIRE(battle::ApplyWeaponElementEnchantment(weapon, {1, 3, 10}));
  CHECK(weapon == expected);
}

TEST_CASE("Weapon enchantment same-index writes retain the last modifier",
          "[status-helpers][F00518]") {
  items::WeaponLoadout weapon{};
  weapon.blade.id = 0x100;
  weapon.class_affinity.affinities.at(0).at(7) = 42;
  REQUIRE(battle::ApplyWeaponElementEnchantment(weapon, {7, 7, 65537}));
  CHECK(weapon.class_affinity.affinities.at(0).at(7) == 42);
  CHECK(weapon.affinity_modifiers.at(7) == -1);
  REQUIRE(battle::ApplyWeaponElementEnchantment(
      weapon, {7, 7, std::numeric_limits<std::int32_t>::min()}));
  CHECK(weapon.class_affinity.affinities.at(0).at(7) == 42);
  CHECK(weapon.affinity_modifiers.at(7) == 0);
}

TEST_CASE(
    "Weapon enchantment refuses used invalid indices without touching gear",
    "[status-helpers][F00518]") {
  items::WeaponLoadout weapon{};
  const auto absent = weapon;
  CHECK(battle::ApplyWeaponElementEnchantment(weapon, {-1, 8, 9}));
  CHECK(weapon == absent);
  weapon.blade.id = 1;
  const auto equipped = weapon;
  CHECK_FALSE(battle::ApplyWeaponElementEnchantment(weapon, {-1, 0, 9}));
  CHECK_FALSE(battle::ApplyWeaponElementEnchantment(weapon, {0, 8, 9}));
  CHECK(weapon == equipped);
}

TEST_CASE(
    "Armor enchantment visits shield and sparse limbs including the sixth",
    "[status-helpers][F00519]") {
  items::ShieldLoadout shield{};
  shield.base.id = 1;
  std::array<items::ArmorLoadout, items::kLimbCount> armor{};
  armor.at(0).armor.id = 1;
  armor.at(5).armor.id = 1;
  auto expected_shield = shield;
  auto expected_armor = armor;
  expected_shield.class_affinity.affinities.at(0).at(2) = -7;
  expected_shield.class_affinity.affinities.at(0).at(4) = 7;
  expected_shield.affinity_modifiers.at(2) = -7;
  expected_shield.affinity_modifiers.at(4) = 7;
  for (const int index : {0, 5}) {
    expected_armor.at(index).class_affinity.affinities =
        expected_shield.class_affinity.affinities;
    expected_armor.at(index).affinity_modifiers =
        expected_shield.affinity_modifiers;
  }
  REQUIRE(battle::ApplyArmorElementEnchantment(shield, armor, {2, 4, -7}));
  CHECK(shield == expected_shield);
  CHECK(armor == expected_armor);
}

TEST_CASE("Armor enchantment validates before changing any equipped record",
          "[status-helpers][F00519]") {
  items::ShieldLoadout shield{};
  std::array<items::ArmorLoadout, items::kLimbCount> armor{};
  CHECK(battle::ApplyArmorElementEnchantment(shield, armor, {8, -1, 5}));
  shield.base.id = 1;
  armor.at(5).armor.id = 1;
  const auto before_shield = shield;
  const auto before_armor = armor;
  CHECK_FALSE(battle::ApplyArmorElementEnchantment(shield, armor, {0, 8, 5}));
  CHECK(shield == before_shield);
  CHECK(armor == before_armor);
  REQUIRE(battle::ApplyArmorElementEnchantment(shield, armor, {1, 1, 65535}));
  CHECK(shield.class_affinity.affinities.at(0).at(1) == 0);
  CHECK(armor.at(5).affinity_modifiers.at(1) == 1);
}

TEST_CASE(
    "Stat numbers preserve all request bits and suppress only hidden "
    "nonplayers",
    "[status-helpers][F00521]") {
  NumberWorld world;
  const battle::StatNumberRequest request{41, -7, 0xFEDCBA98, -19};
  world.spawn = 255;
  CHECK(battle::QueueActorStatChangeNumber(world, request) == true);
  REQUIRE(world.labels.size() == 1);
  CHECK(world.labels.at(0).actor_slot == 41);
  CHECK(world.labels.at(0).limb_tag == -7);
  CHECK(world.labels.at(0).encoded_amount == 0xFEDCBA98);
  CHECK(world.labels.at(0).label_flags == -19);
  world.spawn = 128;
  CHECK(battle::QueueActorStatChangeNumber(world, request) == false);
  CHECK(world.labels.size() == 1);
}

TEST_CASE("Stat number player exception still requires the actor lookup",
          "[status-helpers][F00521]") {
  NumberWorld world;
  world.spawn = 128;
  CHECK(battle::QueueActorStatChangeNumber(world, {0, 1, 0x80000000, 2}) ==
        true);
  CHECK(world.reads == std::vector<int>{0});
  CHECK(world.labels.size() == 1);
  world.spawn.reset();
  CHECK_FALSE(battle::QueueActorStatChangeNumber(world, {0, 0, 0, 0}));
  CHECK(world.reads.size() == 2);
  CHECK(world.labels.size() == 1);
}

TEST_CASE(
    "Targeting risk modes are unsigned and inactive modes do not touch records",
    "[status-helpers][F00527]") {
  for (const std::uint32_t mode : {0U, 1U, 2U, 3U, 4U, 0xFFFFFFFFU}) {
    RiskWorld world;
    world.flow = mode;
    world.target.reset();
    std::int32_t timer = 12;
    CHECK(battle::UpdateTargetingRiskPenalty(timer, world));
    CHECK(timer == (mode >= 1 && mode <= 3 ? 13 : 12));
    CHECK(world.target_reads == 0);
    world.control = 0;
    timer = 0;
    CHECK(battle::UpdateTargetingRiskPenalty(timer, world));
    CHECK(timer == 0);
    CHECK(world.target_reads == 0);
  }
}

TEST_CASE(
    "Targeting risk uses the selected status but the separate player risk "
    "owner",
    "[status-helpers][F00527]") {
  RiskWorld world;
  std::int32_t timer = 0;
  REQUIRE(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(world.risk == 100);
  CHECK(timer == 1);
  CHECK(world.target_reads == 1);
  CHECK(world.risk_reads == 1);
  CHECK(world.writes == 1);
  timer = 0;
  REQUIRE(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(world.writes == 1);
}

TEST_CASE(
    "Targeting risk skips player reads for nonrisk, position and other actor "
    "targets",
    "[status-helpers][F00527]") {
  for (const battle::TargetingRiskTarget target :
       {battle::TargetingRiskTarget{0, 0, false},
        {0, 8, true},
        {1, 8, false}}) {
    RiskWorld world;
    world.target = target;
    world.risk.reset();
    std::int32_t timer = 0;
    CHECK(battle::UpdateTargetingRiskPenalty(timer, world));
    CHECK(timer == 1);
    CHECK(world.risk_reads == 0);
    CHECK(world.writes == 0);
  }
}

TEST_CASE("Targeting risk refuses missing required records before advancing",
          "[status-helpers][F00527]") {
  RiskWorld world;
  std::int32_t timer = 0;
  world.target.reset();
  CHECK_FALSE(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(timer == 0);
  CHECK(world.risk_reads == 0);
  world.target = battle::TargetingRiskTarget{0, 8, false};
  world.risk.reset();
  CHECK_FALSE(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(timer == 0);
  CHECK(world.writes == 0);
}

TEST_CASE(
    "Targeting risk clock wraps before signed remainder and permits zero speed",
    "[status-helpers][F00527]") {
  RiskWorld world;
  std::int32_t timer = std::numeric_limits<std::int32_t>::max();
  CHECK(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(timer == -8);
  timer = std::numeric_limits<std::int32_t>::min();
  world.ticks = -1;
  CHECK(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(timer == 7);
  timer = 0;
  world.ticks = 0;
  CHECK(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(timer == 0);
  CHECK(world.risk == 100);
}

TEST_CASE("Targeting risk reset rearms the next targeting tick",
          "[status-helpers][F00528]") {
  RiskWorld world;
  world.risk = 10;
  std::int32_t timer = 29;
  REQUIRE(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(world.risk == 10);
  REQUIRE(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(world.risk == 11);
  battle::ResetTargetingRiskPenaltyTimer(timer);
  REQUIRE(battle::UpdateTargetingRiskPenalty(timer, world));
  CHECK(world.risk == 12);
  CHECK(timer == 1);
}

TEST_CASE("Live enchantments keep all nonaffinity actor statistics unchanged",
          "[status-helpers][live]") {
  Battle_ActorStats stats{};
  stats.weapon.blade.id = 1;
  stats.shield.base.id = 1;
  stats.limbs[5].armor.armor.id = 1;
  stats.statuses = 0x12345678;
  auto expected = stats;
  expected.weapon.classAffinityCurrent.affinities[0][1] = 9;
  expected.weapon.classAffinityCurrent.affinities[0][3] = -9;
  expected.weapon.affinityModifiers[1] = 9;
  expected.weapon.affinityModifiers[3] = -9;
  battle::ApplyLiveWeaponElementEnchantment(&stats, 1, 3, 9);
  CHECK(Bytes(stats) == Bytes(expected));
  expected.shield.classAffinityCurrent.affinities[0][2] = 5;
  expected.shield.classAffinityCurrent.affinities[0][4] = -5;
  expected.shield.affinityModifiers[2] = 5;
  expected.shield.affinityModifiers[4] = -5;
  expected.limbs[5].armor.classAffinityCurrent.affinities[0][2] = 5;
  expected.limbs[5].armor.classAffinityCurrent.affinities[0][4] = -5;
  expected.limbs[5].armor.affinityModifiers[2] = 5;
  expected.limbs[5].armor.affinityModifiers[4] = -5;
  battle::ApplyLiveArmorElementEnchantment(&stats, 2, 4, 5);
  CHECK(Bytes(stats) == Bytes(expected));
}

TEST_CASE("Live targeting risk keeps selection and player ownership separate",
          "[status-helpers][live]") {
  LiveRiskFixture fixture;
  const auto selected_before = Bytes(fixture.target_stats);
  auto player_expected = fixture.player_stats;
  player_expected.risk = 100;
  battle::UpdateLiveTargetingRiskPenalty();
  CHECK(Bytes(fixture.target_stats) == selected_before);
  CHECK(Bytes(fixture.player_stats) == Bytes(player_expected));
  CHECK(Battle_targetingRiskTimer == 1);
  battle::ResetLiveTargetingRiskPenaltyTimer();
  CHECK(Battle_targetingRiskTimer == 0);
}

TEST_CASE(
    "Live targeting risk inactive and nonzero-clock paths need no selection",
    "[status-helpers][live]") {
  LiveRiskFixture fixture;
  Battle_actionState = nullptr;
  Battle_characterState = nullptr;
  Battle_actors[0] = nullptr;
  Battle_actionFlowMode = 0xFFFFFFFF;
  battle::UpdateLiveTargetingRiskPenalty();
  CHECK(Battle_targetingRiskTimer == 0);
  Battle_actionFlowMode = 2;
  Battle_targetingRiskTimer = 29;
  State_tickSpeed = 2;
  battle::UpdateLiveTargetingRiskPenalty();
  CHECK(Battle_targetingRiskTimer == 1);
}

TEST_CASE("Status helper module addresses retain their checked live contracts",
          "[status-helpers][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x80086EA4) ==
        reinterpret_cast<void*>(&battle::ApplyLiveWeaponElementEnchantment));
  CHECK(resolve(0x80086EF4) ==
        reinterpret_cast<void*>(&battle::ApplyLiveArmorElementEnchantment));
  CHECK(resolve(0x80087578) ==
        reinterpret_cast<void*>(&battle::QueueLiveActorStatChangeNumber));
  CHECK(resolve(0x80088B8C) ==
        reinterpret_cast<void*>(&battle::UpdateLiveTargetingRiskPenalty));
  CHECK(resolve(0x80088CA0) ==
        reinterpret_cast<void*>(&battle::ResetLiveTargetingRiskPenaltyTimer));
}
