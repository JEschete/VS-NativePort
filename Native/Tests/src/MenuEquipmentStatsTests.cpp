#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Menu/Status/EquipmentStats.hpp"

namespace {
namespace status = VS::Menu::Status;
namespace framework = VS::Menu::Framework;
namespace items = VS::Items;

template <std::size_t Count>
bool MatchesAt(
    const std::array<std::int16_t, framework::kEquipmentStatCount>& actual,
    std::size_t offset, const std::array<std::int16_t, Count>& expected) {
  for (std::size_t index = 0; index < Count; ++index) {
    if (actual[offset + index] != expected[index]) return false;
  }
  return true;
}

TEST_CASE("weapon status details populate loadout and comparison fields",
          "[equipment-stat-details]") {
  items::WeaponLoadout weapon{};
  weapon.blade.id = 1;
  weapon.class_affinity.classes[0] = {1, 2, 3, 4, 5, 6, 7, 8};
  weapon.class_affinity.classes[1] = {11, 12, 13, 14, 15, 16, 17, 18};
  weapon.class_affinity.affinities[0] = {21, 22, 23, 24, 25, 26, 27, 28};
  weapon.class_affinity.affinities[1] = {31, 32, 33, 34, 35, 36, 37, 38};
  weapon.baseline.classes = {41, 42, 43, 44, 45, 46, 47, 48};
  weapon.baseline.affinities = {51, 52, 53, 54, 55, 56, 57, 58};
  weapon.grip.types = {-1, 2, 3, 4};
  weapon.damage_type = 5;
  weapon.current_dp = 250;
  weapon.max_dp = 451;
  weapon.current_pp = 17;
  weapon.max_pp = 29;
  weapon.attributes = {.current_strength = 61,
                       .base_strength = 71,
                       .current_intelligence = 62,
                       .base_intelligence = 72,
                       .current_agility = 63,
                       .base_agility = 73};
  weapon.range.range = 6;
  weapon.risk = 7;

  framework::EquipmentDetailsState state{};
  const auto effects = status::PopulateWeaponStats(state, weapon, true);

  CHECK(effects.enabled_pages == 7);
  CHECK(effects.dp_pp_bars == 0);
  CHECK(state.subtype == framework::equipment_subtype::kWeapon);
  CHECK((state.stats[0] == 1 && state.stats[8] == 11 && state.stats[16] == 21 &&
         state.stats[24] == 31 && state.stats[32] == -1 &&
         state.stats[36] == 5 && state.stats[63] == 1));
  CHECK((state.current_dp == 3 && state.max_dp == 5 && state.current_pp == 17));
  CHECK((state.primary[0] == 61 && state.primary[4] == 71));
  CHECK((state.range_risk[0] == 6 && state.range_risk[1] == 7));
}

TEST_CASE("empty weapon resets stats but keeps its range panel enabled",
          "[equipment-stat-details]") {
  items::WeaponLoadout weapon{};
  framework::EquipmentDetailsState state{};
  state.stats.fill(99);

  const auto effects = status::PopulateWeaponStats(state, weapon, false);

  CHECK(effects.enabled_pages == 7);
  CHECK(state.subtype == framework::equipment_subtype::kWeapon);
  CHECK((state.stats[0] == 0 && state.stats[63] == 0));
  CHECK(state.current_dp == 0);
  CHECK(state.primary[3] == 1);
  CHECK(state.range_risk[3] == 1);
}

TEST_CASE("shield details preserve byte type values and equipped status",
          "[equipment-stat-details]") {
  items::ShieldLoadout shield{};
  shield.base.id = 1;
  shield.types = {0xFF, 2, 3, 4};
  shield.current_dp = 100;
  shield.max_dp = 200;
  shield.current_pp = 8;
  shield.max_pp = 9;
  shield.attributes = {.current_strength = 10,
                       .base_strength = 11,
                       .current_intelligence = 12,
                       .base_intelligence = 13,
                       .current_agility = 14,
                       .base_agility = 15};

  framework::EquipmentDetailsState state{};
  const auto effects = status::PopulateShieldStats(state, shield, true);

  CHECK(effects.enabled_pages == 7);
  CHECK(state.subtype == framework::equipment_subtype::kShield);
  CHECK(state.stats[32] == 255);
  CHECK(state.stats[63] == 1);
  CHECK(state.current_dp == 1);
  CHECK(state.max_pp == 9);
  CHECK(state.primary[2] == 14);
  CHECK(state.primary[6] == 15);
}

TEST_CASE("armor detail marks the equipped limb and keeps four type values",
          "[equipment-stat-details]") {
  items::ArmorLoadout armor{};
  armor.armor.id = 1;
  armor.types = {0x1001, 0x1002, 0x1003, 0x1004};
  armor.current_dp = 199;
  armor.max_dp = 201;
  armor.attributes = {.current_strength = 5,
                      .base_strength = 15,
                      .current_intelligence = 6,
                      .base_intelligence = 16,
                      .current_agility = 7,
                      .base_agility = 17};

  framework::EquipmentDetailsState state{};
  const auto effects = status::PopulateArmorStats(state, armor, true);

  CHECK(effects.enabled_pages == 7);
  CHECK(state.subtype == framework::equipment_subtype::kArmor);
  CHECK((state.stats[32] == 0x1001 && state.stats[35] == 0x1004));
  CHECK(state.stats[63] == 1);
  CHECK(state.current_dp == 2);
  CHECK(state.current_pp == 0);
  CHECK(state.primary[4] == 15);
}

TEST_CASE("accessory status preserves the original adjacent-member values",
          "[equipment-stat-details]") {
  items::AccessorySpawnLoadout accessory{};
  accessory.accessory.id = 1;
  accessory.types = {1, 2, 3, 4};
  accessory.classes = {10, 11, 12, 13, 14, 15, 16, 17};
  accessory.affinities = {20, 21, 22, 23, 24, 25, 26, 27};
  accessory.current_strength = 31;
  accessory.current_intelligence = 32;
  accessory.current_agility = 33;

  framework::EquipmentDetailsState state{};
  const auto effects = status::PopulateAccessoryStats(state, accessory);
  const std::array<std::int16_t, 16> expected_types{1, 2, 3, 4, 10, 11, 12, 13,
                                                    1, 2, 3, 4, 10, 11, 12, 13};

  CHECK(effects.enabled_pages == 7);
  CHECK(state.subtype == framework::equipment_subtype::kAccessory);
  CHECK((state.stats[0] == 10 && state.stats[8] == 10 &&
         state.stats[16] == 20 && state.stats[24] == 20));
  CHECK(MatchesAt(state.stats, 32, expected_types));
  CHECK((state.primary[0] == 31 && state.primary[3] == 2));
}

TEST_CASE("blade details set range, Risk, DP, PP, and bars",
          "[equipment-stat-details]") {
  items::EquipmentPart blade{};
  blade.classes = {-1, 2, 3, 4, 5, 6, 7, 8};
  blade.affinities = {-11, 12, 13, 14, 15, 16, 17, 18};
  blade.current_dp = 99;
  blade.max_dp = 101;
  blade.strength = 21;
  blade.intelligence = 22;
  blade.agility = 23;
  blade.range.range = 24;
  blade.cost = 25;

  framework::EquipmentDetailsState state{};
  state.stats.fill(77);
  const auto effects = status::PopulateBladeStats(state, blade, 26, 27);

  CHECK((effects.enabled_pages == 3 && effects.dp_pp_bars == 11));
  CHECK(state.subtype == framework::equipment_subtype::kBlade);
  CHECK(
      (state.stats[0] == -1 && state.stats[8] == -1 && state.stats[16] == -11));
  CHECK(state.stats[32] == 77);
  CHECK((state.current_dp == 1 && state.max_dp == 2 && state.current_pp == 26 &&
         state.max_pp == 27));
  CHECK(state.primary[0] == 21);
  CHECK((state.range_risk[0] == 24 && state.range_risk[1] == 25));
}

TEST_CASE("grip details leave unrelated values untouched",
          "[equipment-stat-details]") {
  items::EquipmentPart grip{};
  grip.types = {-1, -2, 3, 4};
  grip.strength = 5;
  grip.intelligence = 6;
  grip.agility = 7;
  framework::EquipmentDetailsState grip_state{};
  grip_state.stats.fill(77);
  const auto grip_effects = status::PopulateGripStats(grip_state, grip);

  CHECK(grip_effects.enabled_pages == 4);
  CHECK(grip_effects.dp_pp_bars == 8);
  CHECK(grip_state.subtype == framework::equipment_subtype::kGrip);
  CHECK((grip_state.stats[32] == -1 && grip_state.stats[35] == 4 &&
         grip_state.stats[0] == 77));
}

TEST_CASE("gem details leave unrelated values untouched",
          "[equipment-stat-details]") {
  items::EquipmentPart gem{};
  gem.classes = {-8, 2, 3, 4, 5, 6, 7, 8};
  gem.affinities = {-18, 12, 13, 14, 15, 16, 17, 18};
  gem.strength = 25;
  gem.intelligence = 26;
  gem.agility = 27;

  framework::EquipmentDetailsState gem_state{};
  gem_state.stats.fill(88);
  const auto gem_effects = status::PopulateGemStats(gem_state, gem);

  CHECK(gem_effects.enabled_pages == 3);
  CHECK(gem_effects.dp_pp_bars == 8);
  CHECK(gem_state.subtype == framework::equipment_subtype::kGem);
  CHECK((gem_state.stats[0] == -8 && gem_state.stats[8] == -8 &&
         gem_state.stats[16] == -18 && gem_state.stats[32] == 88 &&
         gem_state.primary[2] == 27));
}
}  // namespace
