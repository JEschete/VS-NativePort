// Hand-maintained (tools/port/owned.csv).
#include "Items/EquipmentAffinity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Items/ItemRecords.hpp"

namespace VS::Items {
namespace {
// A rating grows when 70 - (rating + 100) / 4 reaches a draw below 256 and
// decays when (rating + 100) / 4 + 20 does.
constexpr std::int32_t kGrowthDrawBound = 256;
constexpr std::int32_t kGrowthChanceBase = 70;
constexpr std::int32_t kDecayChanceBase = 20;
constexpr std::int32_t kRatingChanceDivisor = 4;
constexpr std::size_t kPhysicalAffinity = 0;
constexpr auto kAffinityNoticeBase = static_cast<std::int32_t>(kClassCount);

constexpr std::int32_t ChanceFromRating(std::int8_t rating) {
  return (rating + kRatingLimit) / kRatingChanceDivisor;
}

// 1-based ids become 0-based with the R3000's wrapping subtraction.
constexpr std::int32_t ZeroBased(std::int32_t id) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(id) - 1U);
}

struct RatingGrowth {
  std::span<std::int8_t> ratings;
  std::int32_t notice_base{};
  GrowthNotices notices{};
};

// Returns whether rating `index` grew.
bool Grow(const RatingGrowth& growth, std::size_t index,
          EquipmentGrowthHost& host) {
  auto& rating = growth.ratings[index];
  if (rating >= kRatingLimit) return false;
  const auto draw = host.NextBounded(kGrowthDrawBound);
  if (kGrowthChanceBase - ChanceFromRating(rating) < draw) return false;
  rating = static_cast<std::int8_t>(rating + 1);
  if (growth.notices.shown) {
    host.QueueStatChange(growth.notices.label,
                         growth.notice_base + static_cast<std::int32_t>(index),
                         StatChange::kIncrease);
  }
  return true;
}

void Decay(const RatingGrowth& growth, std::size_t index,
           EquipmentGrowthHost& host) {
  auto& rating = growth.ratings[index];
  if (rating <= -kRatingLimit) return;
  const auto draw = host.NextBounded(kGrowthDrawBound);
  if (ChanceFromRating(rating) + kDecayChanceBase < draw) return;
  rating = static_cast<std::int8_t>(rating - 1);
  if (growth.notices.shown) {
    host.QueueStatChange(growth.notices.label,
                         growth.notice_base + static_cast<std::int32_t>(index),
                         StatChange::kDecrease);
  }
}

// The affinity a growth raises and the counterpart that then decays.
struct AffinityPair {
  std::size_t grown;
  std::size_t counterpart;
};
using AffinityPairs = std::array<AffinityPair, kAffinityCount>;

// decomp: _triggerArmorAffinityChange's switch (146C.c:10773).
constexpr AffinityPairs kOpposingAffinities{{
    {0, 0},
    {3, 1},
    {4, 2},
    {1, 3},
    {2, 4},
    {6, 5},
    {5, 6},
}};
// decomp: _triggerWeaponAffinityChange's switch (146C.c:10892).
constexpr AffinityPairs kMatchingAffinities{{
    {0, 0},
    {1, 3},
    {2, 4},
    {3, 1},
    {4, 2},
    {5, 6},
    {6, 5},
}};

ItemsStatus GrowAffinity(const AffinityPairs& pairs, const ItemAction& action,
                         EquipmentPart& equipment, std::int32_t affinity,
                         GrowthNotices notices, EquipmentGrowthHost& host) {
  if (action.first_hit_affinity != 0) {
    affinity = action.first_hit_affinity - 1;
  }
  if (affinity < 0 || static_cast<std::size_t>(affinity) >= kAffinityCount) {
    return ItemsStatus::kAffinityOutOfRange;
  }
  const auto pair = pairs[static_cast<std::size_t>(affinity)];
  const RatingGrowth growth{equipment.affinities, kAffinityNoticeBase, notices};
  if (!Grow(growth, pair.grown, host)) return ItemsStatus::kOk;
  if (pair.grown == kPhysicalAffinity) {
    for (std::size_t i = kPhysicalAffinity + 1; i < kAffinityCount; ++i) {
      Decay(growth, i, host);
    }
    return ItemsStatus::kOk;
  }
  Decay(growth, kPhysicalAffinity, host);
  Decay(growth, pair.counterpart, host);
  return ItemsStatus::kOk;
}

// Writes the current rows from per-index totals: the effective affinity row
// adds the temporary modifiers.
template <typename ClassTotal, typename AffinityTotal>
void SetCurrentRows(ClassAffinityCurrent& rows, const AffinityTotals& modifiers,
                    ClassTotal class_total, AffinityTotal affinity_total) {
  for (std::size_t i = 0; i < kClassCount; ++i) {
    const auto total = class_total(i);
    rows.classes[kEquipmentRow][i] = total;
    rows.classes[kEffectiveRow][i] = total;
  }
  for (std::size_t i = 0; i < kAffinityCount; ++i) {
    const auto total = affinity_total(i);
    rows.affinities[kEquipmentRow][i] = total;
    rows.affinities[kEffectiveRow][i] =
        static_cast<std::int16_t>(total + modifiers[i]);
  }
}

template <typename Gems>
void RebuildGemmedClassAffinity(const EquipmentPart& part, const Gems& gems,
                                ClassAffinityCurrent& rows,
                                const AffinityTotals& modifiers) {
  SetCurrentRows(
      rows, modifiers,
      [&](std::size_t i) {
        return static_cast<std::int16_t>(part.classes[i] + gems[0].classes[i] +
                                         gems[1].classes[i] +
                                         gems[2].classes[i]);
      },
      [&](std::size_t i) {
        return static_cast<std::int16_t>(
            part.affinities[i] + gems[0].affinities[i] + gems[1].affinities[i] +
            gems[2].affinities[i]);
      });
}
}  // namespace

// decomp: BATTLE.PRG _calculateArmorClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9582)
void RebuildArmorClassAffinity(ArmorLoadout& armor) {
  const auto& part = armor.armor;
  SetCurrentRows(
      armor.class_affinity, armor.affinity_modifiers,
      [&](std::size_t i) { return std::int16_t{part.classes[i]}; },
      [&](std::size_t i) { return std::int16_t{part.affinities[i]}; });
}

// decomp: BATTLE.PRG _calculateShieldClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9600)
void RebuildShieldClassAffinity(ShieldLoadout& shield) {
  RebuildGemmedClassAffinity(shield.base, shield.gems, shield.class_affinity,
                             shield.affinity_modifiers);
}

// decomp: BATTLE.PRG _calculateWeaponClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9622)
void RebuildWeaponClassAffinity(WeaponLoadout& weapon) {
  RebuildGemmedClassAffinity(weapon.blade, weapon.gems, weapon.class_affinity,
                             weapon.affinity_modifiers);
}

// decomp: BATTLE.PRG _triggerArmorAffinityChange
// (src/BATTLE/BATTLE.PRG/146C.c:10773)
ItemsStatus GrowOpposingEquipmentAffinity(const ItemAction& action,
                                          EquipmentPart& equipment,
                                          std::int32_t affinity,
                                          GrowthNotices notices,
                                          EquipmentGrowthHost& host) {
  return GrowAffinity(kOpposingAffinities, action, equipment, affinity, notices,
                      host);
}

// decomp: BATTLE.PRG _triggerWeaponAffinityChange
// (src/BATTLE/BATTLE.PRG/146C.c:10892)
ItemsStatus GrowMatchingEquipmentAffinity(const ItemAction& action,
                                          EquipmentPart& equipment,
                                          std::int32_t affinity,
                                          GrowthNotices notices,
                                          EquipmentGrowthHost& host) {
  return GrowAffinity(kMatchingAffinities, action, equipment, affinity, notices,
                      host);
}

// decomp: BATTLE.PRG _triggerClassChange (src/BATTLE/BATTLE.PRG/146C.c:11011)
// The original also took the action, which it never read.
ItemsStatus GrowEquipmentClass(EquipmentPart& equipment,
                               std::int32_t class_index, GrowthNotices notices,
                               EquipmentGrowthHost& host) {
  if (class_index < 0 || static_cast<std::size_t>(class_index) >= kClassCount) {
    return ItemsStatus::kClassOutOfRange;
  }
  const auto grown = static_cast<std::size_t>(class_index);
  const RatingGrowth growth{equipment.classes, 0, notices};
  if (Grow(growth, grown, host)) {
    Decay(growth, (grown + 1) % kClassCount, host);
    Decay(growth, (grown + 2) % kClassCount, host);
  }
  return ItemsStatus::kOk;
}

namespace {
// Which equipment an action category can strike.
enum class StrikeReach : std::uint8_t {
  kNothing,
  kShieldOrAnyLimb,
  kShieldOrHitLimb,
  kHitLimb,
};

// decomp: func_80080C9C's switch on the 4-bit action category.
constexpr std::size_t kActionCategoryCount = 16;
constexpr std::array<StrikeReach, kActionCategoryCount> kStrikeReach{
    StrikeReach::kNothing,         StrikeReach::kShieldOrAnyLimb,
    StrikeReach::kShieldOrAnyLimb, StrikeReach::kShieldOrAnyLimb,
    StrikeReach::kShieldOrAnyLimb, StrikeReach::kShieldOrHitLimb,
    StrikeReach::kHitLimb,         StrikeReach::kShieldOrHitLimb,
    StrikeReach::kHitLimb,         StrikeReach::kNothing,
    StrikeReach::kNothing,         StrikeReach::kHitLimb,
    StrikeReach::kHitLimb,         StrikeReach::kNothing,
    StrikeReach::kShieldOrHitLimb, StrikeReach::kNothing,
};

// The shield's entry among the limb candidates.
constexpr std::size_t kShieldCandidate = kLimbCount;

struct StrikeCandidates {
  std::array<std::size_t, kLimbCount + 1> entries{};
  std::size_t count{};
  void Add(std::size_t entry) { entries[count++] = entry; }
};

bool HasStrikableArmor(const DefenderLimb& limb) {
  return limb.max_hp != 0 && limb.armor.armor.id != 0;
}

ItemsStatus CollectStrikeCandidates(StrikeReach reach,
                                    const DefenderEquipment& defender,
                                    std::int32_t hit_limb_index,
                                    StrikeCandidates& candidates) {
  if (reach != StrikeReach::kHitLimb) {
    if (!defender.weapon_drawn) return ItemsStatus::kActorOutOfRange;
    if (*defender.weapon_drawn && defender.shield.base.id != 0) {
      candidates.Add(kShieldCandidate);
    }
  }
  if (reach == StrikeReach::kShieldOrAnyLimb) {
    for (std::size_t i = 0; i < kLimbCount; ++i) {
      if (HasStrikableArmor(defender.limbs[i])) candidates.Add(i);
    }
    return ItemsStatus::kOk;
  }
  if (hit_limb_index < 0 ||
      static_cast<std::size_t>(hit_limb_index) >= kLimbCount) {
    return ItemsStatus::kHitLimbOutOfRange;
  }
  const auto hit = static_cast<std::size_t>(hit_limb_index);
  if (HasStrikableArmor(defender.limbs[hit])) candidates.Add(hit);
  return ItemsStatus::kOk;
}

ItemsStatus GrowStruckPart(const ItemAction& action, EquipmentPart& part,
                           HitRatings attacker, GrowthNotices notices,
                           EquipmentGrowthHost& host) {
  auto status = ItemsStatus::kOk;
  if (part.id == 0) return status;
  if (attacker.class_id != 0) {
    KeepFirstRefusal(
        status,
        GrowEquipmentClass(part, ZeroBased(attacker.class_id), notices, host));
  }
  if (attacker.affinity_id != 0) {
    KeepFirstRefusal(status, GrowMatchingEquipmentAffinity(
                                 action, part, ZeroBased(attacker.affinity_id),
                                 notices, host));
  }
  return status;
}
}  // namespace

// decomp: BATTLE.PRG func_80080C9C (src/BATTLE/BATTLE.PRG/146C.c:11090)
ItemsStatus GrowDefenderEquipmentRatings(const ItemAction& action,
                                         DefenderEquipment& defender,
                                         HitRatings attacker,
                                         std::int32_t hit_limb_index,
                                         EquipmentGrowthHost& host) {
  const auto reach = action.category < kActionCategoryCount
                         ? kStrikeReach[action.category]
                         : StrikeReach::kNothing;
  if (reach == StrikeReach::kNothing) return ItemsStatus::kOk;
  StrikeCandidates candidates;
  const auto collected =
      CollectStrikeCandidates(reach, defender, hit_limb_index, candidates);
  if (collected != ItemsStatus::kOk || candidates.count == 0) {
    return collected;
  }
  // A host draw outside [0, count) wraps into the candidates.
  const auto draw = static_cast<std::uint32_t>(
      host.NextBounded(static_cast<std::int32_t>(candidates.count)));
  const auto selected = candidates.entries[draw % candidates.count];
  if (selected == kShieldCandidate) {
    const auto status =
        GrowStruckPart(action, defender.shield.base, attacker,
                       {kShieldNoticeLabel, defender.shows_notices}, host);
    RebuildShieldClassAffinity(defender.shield);
    return status;
  }
  auto& limb = defender.limbs[selected];
  const auto status =
      GrowStruckPart(action, limb.armor.armor, attacker,
                     {limb.name_index, defender.shows_notices}, host);
  RebuildArmorClassAffinity(limb.armor);
  return status;
}

// decomp: BATTLE.PRG func_80080F78 (src/BATTLE/BATTLE.PRG/146C.c:11178)
ItemsStatus GrowAttackerWeaponRatings(const ItemAction& action,
                                      WeaponLoadout& weapon, bool shows_notices,
                                      HitRatings defender,
                                      EquipmentGrowthHost& host) {
  if (action.IsSpell()) return ItemsStatus::kOk;
  auto status = ItemsStatus::kOk;
  auto& blade = weapon.blade;
  if (blade.id != 0) {
    const GrowthNotices notices{kWeaponNoticeLabel, shows_notices};
    if (defender.class_id != 0) {
      KeepFirstRefusal(status,
                       GrowEquipmentClass(blade, ZeroBased(defender.class_id),
                                          notices, host));
    }
    if (defender.affinity_id != 0) {
      KeepFirstRefusal(
          status,
          GrowOpposingEquipmentAffinity(
              action, blade, ZeroBased(defender.affinity_id), notices, host));
    }
  }
  RebuildWeaponClassAffinity(weapon);
  return status;
}
}  // namespace VS::Items
