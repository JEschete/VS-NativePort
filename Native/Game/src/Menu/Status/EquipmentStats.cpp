#include "Menu/Status/EquipmentStats.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming): the namespace matches the
// catalog's Menu::Status component.
namespace VS::Menu::Status {
namespace {
namespace framework = Framework;

constexpr std::size_t kClassStats = 0;
constexpr std::size_t kAffinityStats = 16;
constexpr std::size_t kTypeStats = 32;
constexpr std::size_t kDamageTypeStat = 36;
constexpr std::size_t kBaselineClassStats = 48;
constexpr std::size_t kBaselineAffinityStats = 56;
constexpr std::size_t kEquippedStat = 63;
constexpr std::size_t kClassesPerPart = Items::kPartClassSlots;
constexpr std::size_t kAffinitiesPerPart = Items::kPartAffinitySlots;
constexpr std::size_t kEquipmentClassStats = Items::kPartClassSlots * 2;
constexpr std::size_t kKnownStatIndex = Framework::kPrimaryStats;
constexpr std::size_t kCurrentStrengthIndex = 0;
constexpr std::size_t kCurrentIntelligenceIndex = 1;
constexpr std::size_t kCurrentAgilityIndex = 2;
constexpr std::size_t kBaselineStrengthIndex = kKnownStatIndex + 1;
constexpr std::size_t kBaselineIntelligenceIndex = kKnownStatIndex + 2;
constexpr std::size_t kBaselineAgilityIndex = kKnownStatIndex + 3;
constexpr std::size_t kBaselineKnownIndex = Framework::kPrimaryStatCount - 1;
constexpr std::size_t kDpIndex = 0;
constexpr std::size_t kMaxDpIndex = 1;
constexpr std::size_t kPpIndex = 2;
constexpr std::size_t kMaxPpIndex = 3;
constexpr std::size_t kRangeIndex = 0;
constexpr std::size_t kRiskIndex = 1;
constexpr std::size_t kUnusedRangeRiskIndex = 2;
constexpr std::size_t kShowRangeRiskIndex = 3;
constexpr int kKnownPrimaryStats = 1;
constexpr int kAccessoryKnownPrimaryStats = 2;
constexpr int kRangeRiskShown = 1;
constexpr int kAllStatPages = Framework::stat_page::kAllEnabled;
constexpr int kClassStatPage = Framework::stat_page::kClassEnabled;
constexpr int kAffinityStatPage = Framework::stat_page::kAffinityEnabled;
constexpr int kClassAffinityPages = kClassStatPage | kAffinityStatPage;
constexpr int kTypeStatPages = Framework::stat_page::kTypeEnabled;
constexpr int kStaticDpPpBars = Framework::dp_pp_bars::kStatic;
constexpr int kAllStaticBars = Framework::dp_pp_bars::kBoth | kStaticDpPpBars;

[[nodiscard]] std::int16_t SignedWord(std::uint16_t value) {
  return std::bit_cast<std::int16_t>(value);
}

[[nodiscard]] std::int16_t SignedByte(std::int8_t value) {
  return static_cast<std::int16_t>(static_cast<int>(value));
}

void Reset(Framework::EquipmentDetailsState& state) {
  state.stats.fill(0);
  state.current_dp = 0;
  state.max_dp = 0;
  state.current_pp = 0;
  state.max_pp = 0;
  state.primary.fill(0);
  state.primary[kKnownStatIndex] = kKnownPrimaryStats;
  state.primary[kBaselineKnownIndex] = kKnownPrimaryStats;
  state.range_risk[kRangeIndex] = 0;
  state.range_risk[kRiskIndex] = 0;
  state.range_risk[kUnusedRangeRiskIndex] = 0;
  state.range_risk[kShowRangeRiskIndex] = 0;
}

void AssignDpPp(Framework::EquipmentDetailsState& state,
                Framework::DpPpValues values) {
  constexpr int kDpPercent = 100;
  constexpr int kDpRounding = 99;
  state.current_dp =
      static_cast<std::uint16_t>((values[kDpIndex] + kDpRounding) / kDpPercent);
  state.max_dp = static_cast<std::uint16_t>(
      (values[kMaxDpIndex] + kDpRounding) / kDpPercent);
  state.current_pp = static_cast<std::uint16_t>(values[kPpIndex]);
  state.max_pp = static_cast<std::uint16_t>(values[kMaxPpIndex]);
}

void SetPrimary(Framework::EquipmentDetailsState& state,
                const Items::LoadoutAttributes& attributes, int known) {
  state.primary[kCurrentStrengthIndex] = attributes.current_strength;
  state.primary[kCurrentIntelligenceIndex] = attributes.current_intelligence;
  state.primary[kCurrentAgilityIndex] = attributes.current_agility;
  state.primary[kKnownStatIndex] = static_cast<std::int16_t>(known);
  state.range_risk[kRangeIndex] = 0;
  state.range_risk[kRiskIndex] = 0;
  state.range_risk[kUnusedRangeRiskIndex] = 0;
  state.range_risk[kShowRangeRiskIndex] = 0;
  state.primary[kBaselineStrengthIndex] = state.primary[kCurrentStrengthIndex];
  state.primary[kBaselineIntelligenceIndex] =
      state.primary[kCurrentIntelligenceIndex];
  state.primary[kBaselineAgilityIndex] = state.primary[kCurrentAgilityIndex];
  state.primary[kBaselineKnownIndex] = state.primary[kKnownStatIndex];
}

void SetBasePrimary(Framework::EquipmentDetailsState& state,
                    const Items::LoadoutAttributes& attributes) {
  state.primary[kBaselineStrengthIndex] = SignedWord(attributes.base_strength);
  state.primary[kBaselineIntelligenceIndex] =
      SignedWord(attributes.base_intelligence);
  state.primary[kBaselineAgilityIndex] = SignedWord(attributes.base_agility);
}

void SetRangeRisk(Framework::EquipmentDetailsState& state,
                  Framework::RangeRiskValues values) {
  state.range_risk[kRangeIndex] =
      static_cast<std::uint8_t>(values[kRangeIndex]);
  state.range_risk[kRiskIndex] = static_cast<std::uint8_t>(values[kRiskIndex]);
  state.range_risk[kUnusedRangeRiskIndex] =
      static_cast<std::uint8_t>(values[kUnusedRangeRiskIndex]);
  state.range_risk[kShowRangeRiskIndex] =
      static_cast<std::uint8_t>(values[kShowRangeRiskIndex]);
}

void CopyCurrentClassAffinity(Framework::EquipmentDetailsState& state,
                              const Items::ClassAffinityCurrent& current) {
  for (std::size_t group = 0; group < current.classes.size(); ++group) {
    for (std::size_t value = 0; value < current.classes[group].size();
         ++value) {
      state.stats[kClassStats + group * kClassesPerPart + value] =
          current.classes[group][value];
      state.stats[kAffinityStats + group * kAffinitiesPerPart + value] =
          current.affinities[group][value];
    }
  }
}

void CopyBaselineClassAffinity(Framework::EquipmentDetailsState& state,
                               const Items::ClassAffinityBaseline& baseline) {
  for (std::size_t value = 0; value < baseline.classes.size(); ++value) {
    state.stats[kBaselineClassStats + value] = baseline.classes[value];
    state.stats[kBaselineAffinityStats + value] =
        SignedWord(baseline.affinities[value]);
  }
}

void CopyPartClassAffinity(Framework::EquipmentDetailsState& state,
                           const Items::EquipmentPart& part) {
  for (std::size_t value = 0; value < kEquipmentClassStats; ++value) {
    const std::size_t source = value % kClassesPerPart;
    state.stats[kClassStats + value] = SignedByte(part.classes[source]);
    state.stats[kAffinityStats + value] = SignedByte(part.affinities[source]);
  }
}

void SetPartPrimary(Framework::EquipmentDetailsState& state,
                    const Items::EquipmentPart& part) {
  const Items::LoadoutAttributes attributes{
      .current_strength = static_cast<std::int16_t>(part.strength),
      .base_strength = 0,
      .current_intelligence = static_cast<std::int16_t>(part.intelligence),
      .base_intelligence = 0,
      .current_agility = static_cast<std::int16_t>(part.agility),
      .base_agility = 0,
  };
  SetPrimary(state, attributes, kKnownPrimaryStats);
}
}  // namespace

EquipmentStatsEffects PopulateWeaponStats(
    Framework::EquipmentDetailsState& state, const Items::WeaponLoadout& weapon,
    bool equipped) {
  state.subtype = framework::equipment_subtype::kWeapon;
  Reset(state);
  SetRangeRisk(state, {0, 0, 0, kRangeRiskShown});

  if (weapon.blade.id != 0) {
    CopyCurrentClassAffinity(state, weapon.class_affinity);
    CopyBaselineClassAffinity(state, weapon.baseline);
    state.stats[kEquippedStat] = static_cast<std::int16_t>(equipped);
    for (std::size_t i = 0; i < weapon.grip.types.size(); ++i) {
      state.stats[kTypeStats + i] = SignedByte(weapon.grip.types[i]);
    }
    state.stats[kDamageTypeStat] =
        static_cast<std::int16_t>(weapon.damage_type);
    AssignDpPp(state, {weapon.current_dp, weapon.max_dp, weapon.current_pp,
                       weapon.max_pp});
    SetPrimary(state, weapon.attributes, kKnownPrimaryStats);
    SetBasePrimary(state, weapon.attributes);
    SetRangeRisk(state, {weapon.range.range, weapon.risk, 0, kRangeRiskShown});
  }

  return {.enabled_pages = kAllStatPages};
}

EquipmentStatsEffects PopulateShieldStats(
    Framework::EquipmentDetailsState& state, const Items::ShieldLoadout& shield,
    bool equipped) {
  state.subtype = framework::equipment_subtype::kShield;
  Reset(state);

  if (shield.base.id != 0) {
    CopyCurrentClassAffinity(state, shield.class_affinity);
    CopyBaselineClassAffinity(state, shield.baseline);
    state.stats[kEquippedStat] = static_cast<std::int16_t>(equipped);
    for (std::size_t i = 0; i < shield.types.size(); ++i) {
      state.stats[kTypeStats + i] = shield.types[i];
    }
    AssignDpPp(state, {shield.current_dp, shield.max_dp, shield.current_pp,
                       shield.max_pp});
    SetPrimary(state, shield.attributes, kKnownPrimaryStats);
    SetBasePrimary(state, shield.attributes);
  }

  return {.enabled_pages = kAllStatPages};
}

EquipmentStatsEffects PopulateArmorStats(
    Framework::EquipmentDetailsState& state, const Items::ArmorLoadout& armor,
    bool equipped) {
  state.subtype = framework::equipment_subtype::kArmor;
  Reset(state);

  if (armor.armor.id != 0) {
    CopyCurrentClassAffinity(state, armor.class_affinity);
    CopyBaselineClassAffinity(state, armor.baseline);
    state.stats[kEquippedStat] = static_cast<std::int16_t>(equipped);
    for (std::size_t i = 0; i < armor.types.size(); ++i) {
      state.stats[kTypeStats + i] = SignedWord(armor.types[i]);
    }
    AssignDpPp(state, {armor.current_dp, armor.max_dp, 0, 0});
    SetPrimary(state, armor.attributes, kKnownPrimaryStats);
    SetBasePrimary(state, armor.attributes);
  }

  return {.enabled_pages = kAllStatPages};
}

EquipmentStatsEffects PopulateAccessoryStats(
    Framework::EquipmentDetailsState& state,
    const Items::AccessorySpawnLoadout& accessory) {
  state.subtype = framework::equipment_subtype::kAccessory;
  Reset(state);

  if (accessory.accessory.id != 0) {
    for (std::size_t i = 0; i < kEquipmentClassStats; ++i) {
      const std::size_t source = i % kClassesPerPart;
      state.stats[kClassStats + i] = SignedWord(accessory.classes[source]);
      state.stats[kAffinityStats + i] =
          SignedWord(accessory.affinities[source]);
    }

    // The original reads types[4..7] from the adjacent classes member. Keep
    // the observed values while avoiding an out-of-bounds typed-array access.
    for (std::size_t i = 0; i < kEquipmentClassStats; ++i) {
      const std::size_t source = i % kClassesPerPart;
      const std::uint16_t value =
          source < accessory.types.size()
              ? accessory.types[source]
              : accessory.classes[source - accessory.types.size()];
      state.stats[kTypeStats + i] = SignedWord(value);
    }
    const Items::LoadoutAttributes attributes{
        .current_strength = accessory.current_strength,
        .base_strength = 0,
        .current_intelligence = accessory.current_intelligence,
        .base_intelligence = 0,
        .current_agility = accessory.current_agility,
        .base_agility = 0,
    };
    SetPrimary(state, attributes, kAccessoryKnownPrimaryStats);
  }

  return {.enabled_pages = kAllStatPages};
}

EquipmentStatsEffects PopulateBladeStats(
    Framework::EquipmentDetailsState& state, const Items::EquipmentPart& blade,
    std::uint16_t weapon_current_pp, std::uint16_t weapon_max_pp) {
  state.subtype = framework::equipment_subtype::kBlade;
  CopyPartClassAffinity(state, blade);
  AssignDpPp(state, {blade.current_dp, blade.max_dp, weapon_current_pp,
                     weapon_max_pp});
  SetPartPrimary(state, blade);
  SetRangeRisk(state, {blade.range.range, blade.cost, 0, kRangeRiskShown});
  return {.enabled_pages = kClassAffinityPages, .dp_pp_bars = kAllStaticBars};
}

EquipmentStatsEffects PopulateGripStats(Framework::EquipmentDetailsState& state,
                                        const Items::EquipmentPart& grip) {
  state.subtype = framework::equipment_subtype::kGrip;
  for (std::size_t i = 0; i < grip.types.size(); ++i) {
    state.stats[kTypeStats + i] = SignedByte(grip.types[i]);
  }
  SetPartPrimary(state, grip);
  return {.enabled_pages = kTypeStatPages, .dp_pp_bars = kStaticDpPpBars};
}

EquipmentStatsEffects PopulateGemStats(Framework::EquipmentDetailsState& state,
                                       const Items::EquipmentPart& gem) {
  state.subtype = framework::equipment_subtype::kGem;
  CopyPartClassAffinity(state, gem);
  SetPartPrimary(state, gem);
  return {.enabled_pages = kClassAffinityPages, .dp_pp_bars = kStaticDpPpBars};
}
}  // namespace VS::Menu::Status
