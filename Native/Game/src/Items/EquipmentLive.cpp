// Hand-maintained (tools/port/owned.csv).
// Live binding for the equipment statistics, growth, wear and recovery rules
// (EquipmentStats.hpp, EquipmentAffinity.hpp, EquipmentWear.hpp,
// Recovery.hpp): the battle actor records, Ashley's battle actor and the
// inventory (VS::Items::inventory).
#include "Items/EquipmentLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include "Battle/AbilityState.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/FloatingLabelPlacementLive.hpp"
#include "Battle/Hit.hpp"
#include "Battle/StatusEffects.hpp"
#include "Items/EquipmentAffinity.hpp"
#include "Items/EquipmentStats.hpp"
#include "Items/EquipmentWear.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Items/Recovery.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Items {
namespace {
using Live::AshleyStats;
using Live::LiveEquipStatusHost;
using Live::WarnNullRecord;
using Live::WarnRefusal;

// The game's random draws and Ashley's stat-change notices
// (vs_battle_queueStatChangeLabel: 1 raises a stat, 0x80000001 lowers it).
class LiveGrowthHost final : public EquipmentGrowthHost {
 public:
  std::int32_t NextBounded(std::int32_t bound) override {
    return random_.NextBounded(bound);
  }
  std::int32_t NextAveraged(std::int32_t bound) override {
    return random_.NextAveraged(bound);
  }
  void QueueStatChange(std::int32_t label, std::int32_t stat,
                       StatChange change) override {
    constexpr auto kLowered = static_cast<std::int32_t>(0x80000001U);
    Battle::LiveQueueEquipmentStatChange(
        label, stat, change == StatChange::kIncrease ? 1 : kLowered);
  }

 private:
  Live::LiveItemRandom random_;
};

// Each limb's armour DP loss of the current hit.
std::array<std::int16_t, kLimbCount> LimbArmorDpLoss() {
  std::array<std::int16_t, kLimbCount> loss{};
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    loss[i] = VS::Battle::limbArmorDpLoss[i];
  }
  return loss;
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_applyWeaponStats
// (src/BATTLE/BATTLE.PRG/146C.c:968)
void BuildLiveWeaponStatsFromParts(
    VS::Items::WeaponLoadout* target,
    const VS::Items::WeaponSpawnLoadout* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("BuildWeaponStatsFromParts");
    return;
  }
  auto weapon = *target;
  WarnRefusal("BuildWeaponStatsFromParts",
              BuildWeaponStatsFromParts(weapon, *source));
  *target = weapon;
}

// decomp: BATTLE.PRG vs_battle_applyShieldStats
// (src/BATTLE/BATTLE.PRG/146C.c:1028)
void BuildLiveShieldStatsFromParts(
    VS::Items::ShieldLoadout* target,
    const VS::Items::ShieldSpawnLoadout* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("BuildShieldStatsFromParts");
    return;
  }
  auto shield = *target;
  BuildShieldStatsFromParts(shield, *source);
  *target = shield;
}

// decomp: BATTLE.PRG _applyAccessoryStats (src/BATTLE/BATTLE.PRG/146C.c:1080)
void BuildLiveAccessoryStatsFromPart(VS::Items::AccessorySpawnLoadout* target,
                                     const VS::Items::AccessoryInit* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("BuildAccessoryStatsFromPart");
    return;
  }
  auto accessory = *target;
  BuildAccessoryStatsFromPart(accessory, *source);
  *target = accessory;
}

// decomp: BATTLE.PRG _applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:1105)
void BuildLiveArmorStatsFromPart(VS::Items::ArmorLoadout* target,
                                 const VS::Items::ArmorInit* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("BuildArmorStatsFromPart");
    return;
  }
  auto armor = *target;
  BuildArmorStatsFromPart(armor, *source);
  *target = armor;
}

// decomp: BATTLE.PRG vs_battle_copyInventoryBladeStats
// (src/BATTLE/BATTLE.PRG/146C.c:1142)
void ReadLiveInventoryBladeStats(VS::Items::EquipmentPart* target,
                                 const VS::Items::InventoryBlade* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("ReadInventoryBladeStats");
    return;
  }
  ReadInventoryBladeStats(*target, *source);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryGripStats
// (src/BATTLE/BATTLE.PRG/146C.c:1179)
void ReadLiveInventoryGripStats(VS::Items::EquipmentPart* target,
                                const VS::Items::InventoryGrip* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("ReadInventoryGripStats");
    return;
  }
  ReadInventoryGripStats(*target, *source);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryArmorStats
// (src/BATTLE/BATTLE.PRG/146C.c:1198)
void ReadLiveInventoryArmorStats(VS::Items::EquipmentPart* target,
                                 const VS::Items::InventoryArmor* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("ReadInventoryArmorStats");
    return;
  }
  ReadInventoryArmorStats(*target, *source);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryGemStats
// (src/BATTLE/BATTLE.PRG/146C.c:1235)
void ReadLiveInventoryGemStats(VS::Items::EquipmentPart* target,
                               const VS::Items::InventoryGem* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("ReadInventoryGemStats");
    return;
  }
  ReadInventoryGemStats(*target, *source);
}

// decomp: BATTLE.PRG vs_battle_applyWeapon (src/BATTLE/BATTLE.PRG/146C.c:1257)
void LoadLiveInventoryWeaponStats(VS::Items::WeaponLoadout* target,
                                  const VS::Items::InventoryWeapon* source) {
  if (target == nullptr) {
    WarnNullRecord("LoadInventoryWeaponStats");
    return;
  }
  auto weapon = *target;
  WarnRefusal("LoadInventoryWeaponStats",
              LoadInventoryWeaponStats(weapon, source, VS::Items::inventory));
  *target = weapon;
}

// decomp: BATTLE.PRG vs_battle_applyShield (src/BATTLE/BATTLE.PRG/146C.c:1295)
void LoadLiveInventoryShieldStats(VS::Items::ShieldLoadout* target,
                                  const VS::Items::InventoryShield* source) {
  if (target == nullptr) {
    WarnNullRecord("LoadInventoryShieldStats");
    return;
  }
  auto shield = *target;
  WarnRefusal("LoadInventoryShieldStats",
              LoadInventoryShieldStats(shield, source, VS::Items::inventory));
  *target = shield;
}

// decomp: BATTLE.PRG vs_battle_applyArmor (src/BATTLE/BATTLE.PRG/146C.c:1321)
void LoadLiveInventoryArmorStats(VS::Items::ArmorLoadout* target,
                                 const VS::Items::InventoryArmor* source) {
  if (target == nullptr) {
    WarnNullRecord("LoadInventoryArmorStats");
    return;
  }
  auto armor = *target;
  LoadInventoryArmorStats(armor, source);
  *target = armor;
}

// decomp: BATTLE.PRG vs_battle_applyAccessory
// (src/BATTLE/BATTLE.PRG/146C.c:1338)
void LoadLiveInventoryAccessoryStats(VS::Items::AccessorySpawnLoadout* target,
                                     const VS::Items::InventoryArmor* source) {
  if (target == nullptr) {
    WarnNullRecord("LoadInventoryAccessoryStats");
    return;
  }
  auto accessory = *target;
  LoadInventoryAccessoryStats(accessory, source);
  *target = accessory;
}

// decomp: BATTLE.PRG vs_battle_equipWeapon (src/BATTLE/BATTLE.PRG/146C.c:1386)
void EquipLiveAshleyWeapon(const VS::Items::InventoryWeapon* weapon) {
  auto* stats = AshleyStats("EquipAshleyWeapon");
  if (stats == nullptr) return;
  auto& actor = *VS::Battle::characterState;
  auto ashley = Live::ToAshleyWeapon(*stats, actor);
  LiveEquipStatusHost host(
      *stats, [&] { Live::CommitAshleyWeapon(*stats, actor, ashley); });
  const auto status =
      EquipAshleyWeapon(ashley, weapon, VS::Items::inventory, host);
  host.Finish();
  WarnRefusal("EquipAshleyWeapon", status);
}

// decomp: BATTLE.PRG vs_battle_equipShield (src/BATTLE/BATTLE.PRG/146C.c:1411)
void EquipLiveAshleyShield(const VS::Items::InventoryShield* shield) {
  auto* stats = AshleyStats("EquipAshleyShield");
  if (stats == nullptr) return;
  auto loadout = stats->shield;
  LiveEquipStatusHost host(*stats, [&] { stats->shield = loadout; });
  const auto status =
      EquipAshleyShield(loadout, shield, VS::Items::inventory, host);
  host.Finish();
  WarnRefusal("EquipAshleyShield", status);
}

// decomp: BATTLE.PRG vs_battle_equipArmor (src/BATTLE/BATTLE.PRG/146C.c:1419)
void EquipLiveAshleyArmor(int body_part,
                          const VS::Items::InventoryArmor* armor) {
  auto* stats = AshleyStats("EquipAshleyArmor");
  if (stats == nullptr) return;
  std::array<ArmorLoadout, kLimbCount> limb_armor{};
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    limb_armor[i] = stats->limbs[i].armor;
  }
  LiveEquipStatusHost host(*stats, [&] {
    for (std::size_t i = 0; i < kLimbCount; ++i) {
      stats->limbs[i].armor = limb_armor[i];
    }
  });
  const auto status = EquipAshleyArmor(limb_armor, body_part, armor, host);
  host.Finish();
  WarnRefusal("EquipAshleyArmor", status);
}

// decomp: BATTLE.PRG vs_battle_equipAccessory
// (src/BATTLE/BATTLE.PRG/146C.c:1427)
void EquipLiveAshleyAccessory(const VS::Items::InventoryArmor* accessory) {
  auto* stats = AshleyStats("EquipAshleyAccessory");
  if (stats == nullptr) return;
  auto loadout = stats->accessory;
  LiveEquipStatusHost host(*stats, [&] { stats->accessory = loadout; });
  EquipAshleyAccessory(loadout, accessory, host);
  host.Finish();
}

// decomp: BATTLE.PRG vs_battle_copyUiBladeStats
// (src/BATTLE/BATTLE.PRG/146C.c:1435)
void WriteLiveInventoryBladeStats(VS::Items::InventoryBlade* target,
                                  const VS::Items::EquipmentPart* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("WriteInventoryBladeStats");
    return;
  }
  WriteInventoryBladeStats(*target, *source);
}

// decomp: BATTLE.PRG vs_battle_copyUiGripStats
// (src/BATTLE/BATTLE.PRG/146C.c:1471)
void WriteLiveInventoryGripStats(VS::Items::InventoryGrip* target,
                                 const VS::Items::EquipmentPart* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("WriteInventoryGripStats");
    return;
  }
  WriteInventoryGripStats(*target, *source);
}

// decomp: BATTLE.PRG vs_battle_copyUiGemStats
// (src/BATTLE/BATTLE.PRG/146C.c:1489)
void WriteLiveInventoryGemStats(VS::Items::InventoryGem* target,
                                const VS::Items::EquipmentPart* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("WriteInventoryGemStats");
    return;
  }
  WriteInventoryGemStats(*target, *source);
}

// decomp: BATTLE.PRG vs_battle_copyUiEquipmentStats
// (src/BATTLE/BATTLE.PRG/146C.c:1509)
void WriteLiveInventoryArmorStats(VS::Items::InventoryArmor* target,
                                  const VS::Items::EquipmentPart* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("WriteInventoryArmorStats");
    return;
  }
  WriteInventoryArmorStats(*target, *source);
}

// decomp: BATTLE.PRG vs_battle_copyUiWeaponStats
// (src/BATTLE/BATTLE.PRG/146C.c:1545)
// `target` is usually a weapon of the inventory itself: the inventory is
// committed first so the record's new name is not overwritten.
void StoreLiveInventoryWeaponStats(VS::Items::InventoryWeapon* target,
                                   VS::Items::WeaponLoadout* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("StoreInventoryWeaponStats");
    return;
  }
  auto record = *target;
  auto weapon = *source;
  auto inventory = VS::Items::inventory;
  const auto status = StoreInventoryWeaponStats(record, weapon, inventory);
  VS::Items::inventory = inventory;
  *target = record;
  *source = weapon;
  WarnRefusal("StoreInventoryWeaponStats", status);
}

// decomp: BATTLE.PRG vs_battle_copyUiAccessoryStats
// (src/BATTLE/BATTLE.PRG/146C.c:1600)
void StoreLiveInventoryAccessoryStats(
    VS::Items::InventoryArmor* target,
    const VS::Items::AccessorySpawnLoadout* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("StoreInventoryAccessoryStats");
    return;
  }
  StoreInventoryAccessoryStats(*target, *source);
}

// decomp: BATTLE.PRG _calculateArmorClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9582)
void RebuildLiveArmorClassAffinity(VS::Battle::ActorStats* actor_stats,
                                   int limb_index) {
  if (actor_stats == nullptr) {
    WarnNullRecord("RebuildArmorClassAffinity");
    return;
  }
  if (limb_index < 0 || static_cast<std::size_t>(limb_index) >= kLimbCount) {
    WarnRefusal("RebuildArmorClassAffinity", ItemsStatus::kBodyPartOutOfRange);
    return;
  }
  auto& limb_armor = actor_stats->limbs[limb_index].armor;
  auto armor = limb_armor;
  RebuildArmorClassAffinity(armor);
  limb_armor = armor;
}

// decomp: BATTLE.PRG _calculateShieldClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9600)
void RebuildLiveShieldClassAffinity(VS::Battle::ActorStats* actor_stats) {
  if (actor_stats == nullptr) {
    WarnNullRecord("RebuildShieldClassAffinity");
    return;
  }
  auto shield = actor_stats->shield;
  RebuildShieldClassAffinity(shield);
  actor_stats->shield = shield;
}

// decomp: BATTLE.PRG _calculateWeaponClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9622)
void RebuildLiveWeaponClassAffinity(VS::Battle::ActorStats* actor_stats) {
  if (actor_stats == nullptr) {
    WarnNullRecord("RebuildWeaponClassAffinity");
    return;
  }
  auto weapon = actor_stats->weapon;
  RebuildWeaponClassAffinity(weapon);
  actor_stats->weapon = weapon;
}

// decomp: BATTLE.PRG _adjustDpPp (src/BATTLE/BATTLE.PRG/146C.c:10689)
// The attacker's weapon and the defender's shield and armour are disjoint, so
// an actor hitting itself commits both sides.
void ConsumeLiveHitEquipmentDp(const ActionDefinition* action,
                               VS::Battle::ActorStats* source,
                               VS::Battle::ActorStats* target, int amount) {
  if (action == nullptr || source == nullptr || target == nullptr) {
    WarnNullRecord("ConsumeHitEquipmentDp");
    return;
  }
  auto weapon = source->weapon;
  auto defender = Live::ToDefender(*target);
  const auto loss = LimbArmorDpLoss();
  const auto status = ConsumeHitEquipmentDp(Live::ToItemAction(*action), weapon,
                                            defender, loss, amount);
  source->weapon = weapon;
  Live::CommitDefender(*target, defender);
  WarnRefusal("ConsumeHitEquipmentDp", status);
}

// decomp: BATTLE.PRG func_800803A4 (src/BATTLE/BATTLE.PRG/146C.c:10721)
void AccumulateLiveHitEquipmentPp(const ActionDefinition* action,
                                  VS::Battle::ActorStats* source_stats,
                                  VS::Battle::ActorStats* target_stats,
                                  int dp_amount) {
  if (action == nullptr || source_stats == nullptr || target_stats == nullptr) {
    WarnNullRecord("AccumulateHitEquipmentPp");
    return;
  }
  auto weapon = source_stats->weapon;
  auto defender = Live::ToDefender(*target_stats);
  const auto status = AccumulateHitEquipmentPp(Live::ToItemAction(*action),
                                               weapon, defender, dp_amount);
  source_stats->weapon = weapon;
  Live::CommitDefender(*target_stats, defender);
  WarnRefusal("AccumulateHitEquipmentPp", status);
}

// decomp: BATTLE.PRG func_80080C9C (src/BATTLE/BATTLE.PRG/146C.c:11090)
void GrowLiveDefenderEquipmentRatings(const ActionDefinition* action,
                                      VS::Battle::ActorStats* defender_stats,
                                      int attacker_class_id,
                                      int attacker_affinity_id,
                                      int hit_limb_index) {
  if (action == nullptr || defender_stats == nullptr) {
    WarnNullRecord("GrowDefenderEquipmentRatings");
    return;
  }
  auto defender = Live::ToDefender(*defender_stats);
  LiveGrowthHost host;
  const auto status = GrowDefenderEquipmentRatings(
      Live::ToItemAction(*action), defender,
      HitRatings{.class_id = attacker_class_id,
                 .affinity_id = attacker_affinity_id},
      hit_limb_index, host);
  Live::CommitDefender(*defender_stats, defender);
  WarnRefusal("GrowDefenderEquipmentRatings", status);
}

// decomp: BATTLE.PRG func_80080F78 (src/BATTLE/BATTLE.PRG/146C.c:11178)
void GrowLiveAttackerWeaponRatings(const ActionDefinition* action,
                                   VS::Battle::ActorStats* attacker_stats,
                                   int defender_class_id,
                                   int defender_affinity_id) {
  if (action == nullptr || attacker_stats == nullptr) {
    WarnNullRecord("GrowAttackerWeaponRatings");
    return;
  }
  auto weapon = attacker_stats->weapon;
  LiveGrowthHost host;
  const auto status = GrowAttackerWeaponRatings(
      Live::ToItemAction(*action), weapon, attacker_stats->actor_id == 0,
      HitRatings{.class_id = defender_class_id,
                 .affinity_id = defender_affinity_id},
      host);
  attacker_stats->weapon = weapon;
  WarnRefusal("GrowAttackerWeaponRatings", status);
}

// decomp: BATTLE.PRG _itemRecovery (src/BATTLE/BATTLE.PRG/146C.c:12109)
std::int16_t CalculateLiveItemRecovery(
    ActionDefinition* action, VS::Battle::HitResult* /*attacker_result*/,
    VS::Battle::HitResult* /*target_result*/, int hit_index,
    int add_random_variation) {
  if (action == nullptr) {
    WarnNullRecord("CalculateItemRecovery");
    return 0;
  }
  Live::LiveItemRandom random;
  const auto recovery =
      CalculateItemRecovery(Live::ToItemAction(*action), hit_index,
                            add_random_variation != 0, random);
  if (!recovery.has_value()) {
    WarnRefusal("CalculateItemRecovery", ItemsStatus::kHitIndexOutOfRange);
    return 0;
  }
  return *recovery;
}
}  // namespace VS::Items
