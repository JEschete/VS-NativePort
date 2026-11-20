// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Actors/Spawning.hpp"

namespace VS::Actors {
namespace {
using VS::Items::EquipmentPart;
using DpSplash = std::array<std::uint8_t, layout::kLimbDpSplashTargets>;

constexpr VS::Items::PartAffinities kStartingArmorAffinities{
    2, 5, 5, -1, -1, -5, -5, 0,
};

// Each limb record lists its first action, then action slots 1 to 3.
constexpr std::array<LimbActionInit, layout::kLimbActionSlots> StartingActions(
    std::uint16_t first_action) noexcept {
  std::array<LimbActionInit, layout::kLimbActionSlots> actions{};
  actions[0].action_id = first_action;
  for (std::size_t slot = 1; slot < actions.size(); ++slot) {
    actions[slot].index = static_cast<std::uint8_t>(slot);
  }
  return actions;
}

// Every armored starting limb has 200 HP and material 2 armor.
constexpr std::uint16_t kStartingLimbHp = 200;
constexpr std::uint8_t kStartingArmorMaterial = 2;

constexpr LimbInit StartingLimb(std::uint16_t first_action,
                                const EquipmentPart& armor,
                                const DpSplash& dp_splash) noexcept {
  return {
      .hp = kStartingLimbHp,
      .actions = StartingActions(first_action),
      .armor = {.equipment = armor,
                .material = kStartingArmorMaterial,
                .opaque32 = 1},
      .dp_splash = dp_splash,
  };
}

// Limbs 0 and 1 wear the same piece.
constexpr EquipmentPart kStartingPairedArmor{
    .id = 191,
    .sub_id = 65,
    .category = 5,
    .strength = 1,
    .intelligence = 13,
    .current_dp = 3350,
    .max_dp = 3350,
    .types = {0, 1, 0, 0},
    .affinities = kStartingArmorAffinities,
};

constexpr EquipmentPart kStartingLimb2Armor{
    .id = 143,
    .sub_id = 17,
    .category = 2,
    .strength = 1,
    .intelligence = 8,
    .current_dp = 3800,
    .max_dp = 3800,
    .affinities = kStartingArmorAffinities,
};

constexpr EquipmentPart kStartingLimb3Armor{
    .id = 159,
    .sub_id = 33,
    .category = 3,
    .strength = 5,
    .intelligence = 10,
    .current_dp = 5500,
    .max_dp = 5500,
    .types = {0, 1, 1, 0},
    .affinities = kStartingArmorAffinities,
};

constexpr EquipmentPart kStartingLimb4Armor{
    .id = 175,
    .sub_id = 49,
    .category = 4,
    .strength = 1,
    .intelligence = 12,
    .current_dp = 4050,
    .max_dp = 4050,
    .affinities = kStartingArmorAffinities,
};

constexpr EquipmentPart kStartingBlade{
    .id = 14,
    .sub_id = 14,
    .weapon_model_id = 16,
    .category = 2,
    .strength = 9,
    .intelligence = 1,
    .agility = -3,
    .current_dp = 12600,
    .max_dp = 12600,
    .max_pp = 136,
    .damage_type = 2,
    .cost_type = 2,
    .cost = 1,
    .range = {.range = 3, .second_range = 2, .third_range = 3, .shape_bits = 1},
    .classes = {-1, -1, -2, -1, -1, -5},
    .affinities = {8, 5, 5, -3, -3, 2, 2, 0},
};

constexpr EquipmentPart kStartingGrip{
    .id = 96,
    .sub_id = 1,
    .category = 1,
    .strength = 1,
    .agility = -1,
    .types = {0, 0, 4, 1},
};

constexpr EquipmentPart kStartingAccessory{
    .id = 223,
    .sub_id = 97,
    .category = 7,
    .intelligence = 1,
    .classes = {0, 0, 5, -5, -5, 5},
    .affinities = {5, 0, 0, 0, 0, 5, -5, 0},
};
}  // namespace

// decomp: D_800FAB18 (D_800FAB18@src/BATTLE/INITBTL.PRG/18.c)
constinit const CharacterInit kAshleyInitialLoadout = {
    .sub_type = 174,
    .reach = 7,
    .name = {"\x0A\x36\x2B\x2F\x28\x3C\xFA\x06\x1B\x2C\x32\x37\xE7"},
    .hp = 250,
    .mp = 50,
    .strength = 100,
    .intelligence = 100,
    .agility = 100,
    .recovery_delay_bonus = 1,
    .material = 2,
    .walk_speed = 10,
    .gait_class = 4,
    .run_speed = 20,
    .mobility_limbs = {4},
    .default_target_limb = 3,
    .weapon = {.blade = kStartingBlade,
               .grip = kStartingGrip,
               .material = 3,
               .name = {"\x0F\x24\x31\x27\x24\x31\x2A\x32\xE7"}},
    .accessory = {.equipment = kStartingAccessory},
    .limbs = {{
        StartingLimb(1, kStartingPairedArmor, {100, 0, 0, 5, 0, 0}),
        StartingLimb(1, kStartingPairedArmor, {0, 100, 0, 5, 0, 0}),
        StartingLimb(0, kStartingLimb2Armor, {0, 0, 100, 10, 0, 0}),
        StartingLimb(0, kStartingLimb3Armor, {5, 5, 5, 100, 5, 0}),
        StartingLimb(0, kStartingLimb4Armor, {0, 0, 0, 5, 100, 0}),
        {.actions = StartingActions(0)},
    }},
};

// decomp: D_8004FDD0 (D_8004FDD0@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::array<char, kPlaceholderActorNameBytes> placeholderActorName;

// decomp: D_8004FE88 (D_8004FE88@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::array<std::uint8_t, kBodyModelIds> usesSecondaryBodyModel;
}  // namespace VS::Actors
