#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Items/ItemRecords.hpp"

// Equipment growth: the class and affinity ratings a weapon, shield or armour
// gains (and the opposing ones it loses) when it strikes or is struck, and
// the per-loadout totals rebuilt from the part ratings afterwards.
namespace VS::Items {
// Ratings never grow past this value or decay below its negation.
inline constexpr std::int8_t kRatingLimit = 100;

// Labels of the stat-change notices for Ashley's weapon and shield; armour is
// labelled with its limb's name index.
inline constexpr std::int32_t kWeaponNoticeLabel = 0xF0;
inline constexpr std::int32_t kShieldNoticeLabel = 0xF1;

enum class StatChange : std::uint8_t { kIncrease, kDecrease };

// The draws growth consumes and the notices it shows.
class EquipmentGrowthHost : public ItemRandom {
 public:
  // Shows equipment `label`'s rating `stat` changing: classes are stats 0-5
  // and affinities follow them (stat 6 is physical).
  virtual void QueueStatChange(std::int32_t label, std::int32_t stat,
                               StatChange change) = 0;
};

// Who sees a growth's notices.
struct GrowthNotices {
  std::int32_t label{};
  bool shown{};
};

// A hit's class and affinity, 1-based; 0 for none.
struct HitRatings {
  std::int32_t class_id{};
  std::int32_t affinity_id{};
};

// Rebuild a loadout's current class and affinity rows from its part ratings
// (and, for the effective affinity row, the temporary modifiers).
void RebuildArmorClassAffinity(ArmorLoadout& armor);
void RebuildShieldClassAffinity(ShieldLoadout& shield);
void RebuildWeaponClassAffinity(WeaponLoadout& weapon);

// Grow the affinity of `equipment` that opposes (or matches) `affinity`
// (0-based; the action's first-hit affinity replaces it when it has one),
// then let physical and the counterpart affinity decay. Growing physical
// lets every other affinity decay instead. An affinity outside
// physical..dark changes nothing and returns kAffinityOutOfRange.
[[nodiscard]] ItemsStatus GrowOpposingEquipmentAffinity(
    const ItemAction& action, EquipmentPart& equipment, std::int32_t affinity,
    GrowthNotices notices, EquipmentGrowthHost& host);
[[nodiscard]] ItemsStatus GrowMatchingEquipmentAffinity(
    const ItemAction& action, EquipmentPart& equipment, std::int32_t affinity,
    GrowthNotices notices, EquipmentGrowthHost& host);

// Grow class `class_index` (0-based) of `equipment` and let the two classes
// after it decay. A class outside human..evil changes nothing and returns
// kClassOutOfRange.
[[nodiscard]] ItemsStatus GrowEquipmentClass(EquipmentPart& equipment,
                                             std::int32_t class_index,
                                             GrowthNotices notices,
                                             EquipmentGrowthHost& host);

// Grow the ratings of one piece of the defender's equipment the hit could
// land on, picked at random (the raised shield, or armour by action
// category), against the attacker's class and affinity. A hit limb outside
// the limb table for an action that strikes one limb, or an unknown
// weapon-drawn state the category needs, changes nothing and returns
// kHitLimbOutOfRange or kActorOutOfRange.
[[nodiscard]] ItemsStatus GrowDefenderEquipmentRatings(
    const ItemAction& action, DefenderEquipment& defender, HitRatings attacker,
    std::int32_t hit_limb_index, EquipmentGrowthHost& host);

// Grow the attacker's blade against the defender's class and affinity unless
// the action is a spell.
[[nodiscard]] ItemsStatus GrowAttackerWeaponRatings(const ItemAction& action,
                                                    WeaponLoadout& weapon,
                                                    bool shows_notices,
                                                    HitRatings defender,
                                                    EquipmentGrowthHost& host);
}  // namespace VS::Items
