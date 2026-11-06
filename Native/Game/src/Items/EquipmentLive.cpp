// Hand-maintained (tools/port/owned.csv).
// Live binding for the equipment statistics, growth, wear and recovery rules
// (EquipmentStats.hpp, EquipmentAffinity.hpp, EquipmentWear.hpp,
// Recovery.hpp): the battle actor records, Ashley's battle actor and the
// inventory (Audio_Seq_unusedInventory).
#include "Items/EquipmentLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include "Items/EquipmentAffinity.hpp"
#include "Items/EquipmentStats.hpp"
#include "Items/EquipmentWear.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Items/Recovery.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
}

namespace VS::Items {
namespace {
using Live::AshleyStats;
using Live::Commit;
using Live::LiveEquipStatusHost;
using Live::OrNull;
using Live::Typed;
using Live::TypedOrNone;
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
    Battle_QueueEquipmentStatChange(
        label, stat, change == StatChange::kIncrease ? 1 : kLowered);
  }

 private:
  Live::LiveItemRandom random_;
};

// Each limb's armour DP loss of the current hit.
std::array<std::int16_t, kLimbCount> LimbArmorDpLoss() {
  std::array<std::int16_t, kLimbCount> loss{};
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    loss[i] = Battle_limbArmorDpLoss[i];
  }
  return loss;
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_applyWeaponStats
// (src/BATTLE/BATTLE.PRG/146C.c:968)
void BuildLiveWeaponStatsFromParts(Items_WeaponLoadout* target,
                                   const Items_WeaponSpawnLoadout* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("BuildWeaponStatsFromParts");
    return;
  }
  auto weapon = Typed(*target);
  WarnRefusal("BuildWeaponStatsFromParts",
              BuildWeaponStatsFromParts(weapon, Typed(*source)));
  Commit(*target, weapon);
}

// decomp: BATTLE.PRG vs_battle_applyShieldStats
// (src/BATTLE/BATTLE.PRG/146C.c:1028)
void BuildLiveShieldStatsFromParts(Items_ShieldLoadout* target,
                                   const Items_ShieldSpawnLoadout* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("BuildShieldStatsFromParts");
    return;
  }
  auto shield = Typed(*target);
  BuildShieldStatsFromParts(shield, Typed(*source));
  Commit(*target, shield);
}

// decomp: BATTLE.PRG _applyAccessoryStats (src/BATTLE/BATTLE.PRG/146C.c:1080)
void BuildLiveAccessoryStatsFromPart(Items_AccessorySpawnLoadout* target,
                                     const Items_AccessoryInit* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("BuildAccessoryStatsFromPart");
    return;
  }
  auto accessory = Typed(*target);
  BuildAccessoryStatsFromPart(accessory, Typed(*source));
  Commit(*target, accessory);
}

// decomp: BATTLE.PRG _applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:1105)
void BuildLiveArmorStatsFromPart(Items_ArmorLoadout* target,
                                 const Items_ArmorInit* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("BuildArmorStatsFromPart");
    return;
  }
  auto armor = Typed(*target);
  BuildArmorStatsFromPart(armor, Typed(*source));
  Commit(*target, armor);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryBladeStats
// (src/BATTLE/BATTLE.PRG/146C.c:1142)
void ReadLiveInventoryBladeStats(Items_EquipmentPartStats* target,
                                 const Items_InventoryBlade* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("ReadInventoryBladeStats");
    return;
  }
  auto part = Typed(*target);
  ReadInventoryBladeStats(part, Typed(*source));
  Commit(*target, part);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryGripStats
// (src/BATTLE/BATTLE.PRG/146C.c:1179)
void ReadLiveInventoryGripStats(Items_EquipmentPartStats* target,
                                const Items_InventoryGrip* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("ReadInventoryGripStats");
    return;
  }
  auto part = Typed(*target);
  ReadInventoryGripStats(part, Typed(*source));
  Commit(*target, part);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryArmorStats
// (src/BATTLE/BATTLE.PRG/146C.c:1198)
void ReadLiveInventoryArmorStats(Items_EquipmentPartStats* target,
                                 const Items_InventoryArmor* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("ReadInventoryArmorStats");
    return;
  }
  auto part = Typed(*target);
  ReadInventoryArmorStats(part, Typed(*source));
  Commit(*target, part);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryGemStats
// (src/BATTLE/BATTLE.PRG/146C.c:1235)
void ReadLiveInventoryGemStats(Items_EquipmentPartStats* target,
                               const Items_InventoryGem* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("ReadInventoryGemStats");
    return;
  }
  auto part = Typed(*target);
  ReadInventoryGemStats(part, Typed(*source));
  Commit(*target, part);
}

// decomp: BATTLE.PRG vs_battle_applyWeapon (src/BATTLE/BATTLE.PRG/146C.c:1257)
void LoadLiveInventoryWeaponStats(Items_WeaponLoadout* target,
                                  const Items_InventoryWeapon* source) {
  if (target == nullptr) {
    WarnNullRecord("LoadInventoryWeaponStats");
    return;
  }
  auto weapon = Typed(*target);
  const auto record = TypedOrNone(source);
  WarnRefusal("LoadInventoryWeaponStats",
              LoadInventoryWeaponStats(weapon, OrNull(record),
                                       Typed(Audio_Seq_unusedInventory)));
  Commit(*target, weapon);
}

// decomp: BATTLE.PRG vs_battle_applyShield (src/BATTLE/BATTLE.PRG/146C.c:1295)
void LoadLiveInventoryShieldStats(Items_ShieldLoadout* target,
                                  const Items_InventoryShield* source) {
  if (target == nullptr) {
    WarnNullRecord("LoadInventoryShieldStats");
    return;
  }
  auto shield = Typed(*target);
  const auto record = TypedOrNone(source);
  WarnRefusal("LoadInventoryShieldStats",
              LoadInventoryShieldStats(shield, OrNull(record),
                                       Typed(Audio_Seq_unusedInventory)));
  Commit(*target, shield);
}

// decomp: BATTLE.PRG vs_battle_applyArmor (src/BATTLE/BATTLE.PRG/146C.c:1321)
void LoadLiveInventoryArmorStats(Items_ArmorLoadout* target,
                                 const Items_InventoryArmor* source) {
  if (target == nullptr) {
    WarnNullRecord("LoadInventoryArmorStats");
    return;
  }
  auto armor = Typed(*target);
  const auto record = TypedOrNone(source);
  LoadInventoryArmorStats(armor, OrNull(record));
  Commit(*target, armor);
}

// decomp: BATTLE.PRG vs_battle_applyAccessory
// (src/BATTLE/BATTLE.PRG/146C.c:1338)
void LoadLiveInventoryAccessoryStats(Items_AccessorySpawnLoadout* target,
                                     const Items_InventoryArmor* source) {
  if (target == nullptr) {
    WarnNullRecord("LoadInventoryAccessoryStats");
    return;
  }
  auto accessory = Typed(*target);
  const auto record = TypedOrNone(source);
  LoadInventoryAccessoryStats(accessory, OrNull(record));
  Commit(*target, accessory);
}

// decomp: BATTLE.PRG vs_battle_equipWeapon (src/BATTLE/BATTLE.PRG/146C.c:1386)
void EquipLiveAshleyWeapon(const Items_InventoryWeapon* weapon) {
  auto* stats = AshleyStats("EquipAshleyWeapon");
  if (stats == nullptr) return;
  auto& actor = *Battle_characterState;
  auto ashley = Live::ToAshleyWeapon(*stats, actor);
  LiveEquipStatusHost host(
      *stats, [&] { Live::CommitAshleyWeapon(*stats, actor, ashley); });
  const auto record = TypedOrNone(weapon);
  const auto status = EquipAshleyWeapon(ashley, OrNull(record),
                                        Typed(Audio_Seq_unusedInventory), host);
  host.Finish();
  WarnRefusal("EquipAshleyWeapon", status);
}

// decomp: BATTLE.PRG vs_battle_equipShield (src/BATTLE/BATTLE.PRG/146C.c:1411)
void EquipLiveAshleyShield(const Items_InventoryShield* shield) {
  auto* stats = AshleyStats("EquipAshleyShield");
  if (stats == nullptr) return;
  auto loadout = Typed(stats->shield);
  LiveEquipStatusHost host(*stats, [&] { Commit(stats->shield, loadout); });
  const auto record = TypedOrNone(shield);
  const auto status = EquipAshleyShield(loadout, OrNull(record),
                                        Typed(Audio_Seq_unusedInventory), host);
  host.Finish();
  WarnRefusal("EquipAshleyShield", status);
}

// decomp: BATTLE.PRG vs_battle_equipArmor (src/BATTLE/BATTLE.PRG/146C.c:1419)
void EquipLiveAshleyArmor(int body_part, const Items_InventoryArmor* armor) {
  auto* stats = AshleyStats("EquipAshleyArmor");
  if (stats == nullptr) return;
  std::array<ArmorLoadout, kLimbCount> limb_armor{};
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    limb_armor[i] = Typed(stats->limbs[i].armor);
  }
  LiveEquipStatusHost host(*stats, [&] {
    for (std::size_t i = 0; i < kLimbCount; ++i) {
      Commit(stats->limbs[i].armor, limb_armor[i]);
    }
  });
  const auto record = TypedOrNone(armor);
  const auto status =
      EquipAshleyArmor(limb_armor, body_part, OrNull(record), host);
  host.Finish();
  WarnRefusal("EquipAshleyArmor", status);
}

// decomp: BATTLE.PRG vs_battle_equipAccessory
// (src/BATTLE/BATTLE.PRG/146C.c:1427)
void EquipLiveAshleyAccessory(const Items_InventoryArmor* accessory) {
  auto* stats = AshleyStats("EquipAshleyAccessory");
  if (stats == nullptr) return;
  auto loadout = Typed(stats->accessory);
  LiveEquipStatusHost host(*stats, [&] { Commit(stats->accessory, loadout); });
  const auto record = TypedOrNone(accessory);
  EquipAshleyAccessory(loadout, OrNull(record), host);
  host.Finish();
}

// decomp: BATTLE.PRG vs_battle_copyUiBladeStats
// (src/BATTLE/BATTLE.PRG/146C.c:1435)
void WriteLiveInventoryBladeStats(Items_InventoryBlade* target,
                                  const Items_EquipmentPartStats* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("WriteInventoryBladeStats");
    return;
  }
  auto blade = Typed(*target);
  WriteInventoryBladeStats(blade, Typed(*source));
  Commit(*target, blade);
}

// decomp: BATTLE.PRG vs_battle_copyUiGripStats
// (src/BATTLE/BATTLE.PRG/146C.c:1471)
void WriteLiveInventoryGripStats(Items_InventoryGrip* target,
                                 const Items_EquipmentPartStats* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("WriteInventoryGripStats");
    return;
  }
  auto grip = Typed(*target);
  WriteInventoryGripStats(grip, Typed(*source));
  Commit(*target, grip);
}

// decomp: BATTLE.PRG vs_battle_copyUiGemStats
// (src/BATTLE/BATTLE.PRG/146C.c:1489)
void WriteLiveInventoryGemStats(Items_InventoryGem* target,
                                const Items_EquipmentPartStats* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("WriteInventoryGemStats");
    return;
  }
  auto gem = Typed(*target);
  WriteInventoryGemStats(gem, Typed(*source));
  Commit(*target, gem);
}

// decomp: BATTLE.PRG vs_battle_copyUiEquipmentStats
// (src/BATTLE/BATTLE.PRG/146C.c:1509)
void WriteLiveInventoryArmorStats(Items_InventoryArmor* target,
                                  const Items_EquipmentPartStats* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("WriteInventoryArmorStats");
    return;
  }
  auto armor = Typed(*target);
  WriteInventoryArmorStats(armor, Typed(*source));
  Commit(*target, armor);
}

// decomp: BATTLE.PRG vs_battle_copyUiWeaponStats
// (src/BATTLE/BATTLE.PRG/146C.c:1545)
// `target` is usually a weapon of the inventory itself: the inventory is
// committed first so the record's new name is not overwritten.
void StoreLiveInventoryWeaponStats(Items_InventoryWeapon* target,
                                   Items_WeaponLoadout* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("StoreInventoryWeaponStats");
    return;
  }
  auto record = Typed(*target);
  auto weapon = Typed(*source);
  auto inventory = Typed(Audio_Seq_unusedInventory);
  const auto status = StoreInventoryWeaponStats(record, weapon, inventory);
  Commit(Audio_Seq_unusedInventory, inventory);
  Commit(*target, record);
  Commit(*source, weapon);
  WarnRefusal("StoreInventoryWeaponStats", status);
}

// decomp: BATTLE.PRG vs_battle_copyUiAccessoryStats
// (src/BATTLE/BATTLE.PRG/146C.c:1600)
void StoreLiveInventoryAccessoryStats(
    Items_InventoryArmor* target, const Items_AccessorySpawnLoadout* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("StoreInventoryAccessoryStats");
    return;
  }
  auto record = Typed(*target);
  StoreInventoryAccessoryStats(record, Typed(*source));
  Commit(*target, record);
}

// decomp: BATTLE.PRG _calculateArmorClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9582)
void RebuildLiveArmorClassAffinity(Battle_ActorStats* actor_stats,
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
  auto armor = Typed(limb_armor);
  RebuildArmorClassAffinity(armor);
  Commit(limb_armor, armor);
}

// decomp: BATTLE.PRG _calculateShieldClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9600)
void RebuildLiveShieldClassAffinity(Battle_ActorStats* actor_stats) {
  if (actor_stats == nullptr) {
    WarnNullRecord("RebuildShieldClassAffinity");
    return;
  }
  auto shield = Typed(actor_stats->shield);
  RebuildShieldClassAffinity(shield);
  Commit(actor_stats->shield, shield);
}

// decomp: BATTLE.PRG _calculateWeaponClassAffinity
// (src/BATTLE/BATTLE.PRG/146C.c:9622)
void RebuildLiveWeaponClassAffinity(Battle_ActorStats* actor_stats) {
  if (actor_stats == nullptr) {
    WarnNullRecord("RebuildWeaponClassAffinity");
    return;
  }
  auto weapon = Typed(actor_stats->weapon);
  RebuildWeaponClassAffinity(weapon);
  Commit(actor_stats->weapon, weapon);
}

// decomp: BATTLE.PRG _adjustDpPp (src/BATTLE/BATTLE.PRG/146C.c:10689)
// The attacker's weapon and the defender's shield and armour are disjoint, so
// an actor hitting itself commits both sides.
void ConsumeLiveHitEquipmentDp(const Battle_ActionDefinition* action,
                               Battle_ActorStats* source,
                               Battle_ActorStats* target, int amount) {
  if (action == nullptr || source == nullptr || target == nullptr) {
    WarnNullRecord("ConsumeHitEquipmentDp");
    return;
  }
  auto weapon = Typed(source->weapon);
  auto defender = Live::ToDefender(*target);
  const auto loss = LimbArmorDpLoss();
  const auto status = ConsumeHitEquipmentDp(Live::ToItemAction(*action), weapon,
                                            defender, loss, amount);
  Commit(source->weapon, weapon);
  Live::CommitDefender(*target, defender);
  WarnRefusal("ConsumeHitEquipmentDp", status);
}

// decomp: BATTLE.PRG func_800803A4 (src/BATTLE/BATTLE.PRG/146C.c:10721)
void AccumulateLiveHitEquipmentPp(const Battle_ActionDefinition* action,
                                  Battle_ActorStats* source_stats,
                                  Battle_ActorStats* target_stats,
                                  int dp_amount) {
  if (action == nullptr || source_stats == nullptr || target_stats == nullptr) {
    WarnNullRecord("AccumulateHitEquipmentPp");
    return;
  }
  auto weapon = Typed(source_stats->weapon);
  auto defender = Live::ToDefender(*target_stats);
  const auto status = AccumulateHitEquipmentPp(Live::ToItemAction(*action),
                                               weapon, defender, dp_amount);
  Commit(source_stats->weapon, weapon);
  Live::CommitDefender(*target_stats, defender);
  WarnRefusal("AccumulateHitEquipmentPp", status);
}

// decomp: BATTLE.PRG func_80080C9C (src/BATTLE/BATTLE.PRG/146C.c:11090)
void GrowLiveDefenderEquipmentRatings(const Battle_ActionDefinition* action,
                                      Battle_ActorStats* defender_stats,
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
void GrowLiveAttackerWeaponRatings(const Battle_ActionDefinition* action,
                                   Battle_ActorStats* attacker_stats,
                                   int defender_class_id,
                                   int defender_affinity_id) {
  if (action == nullptr || attacker_stats == nullptr) {
    WarnNullRecord("GrowAttackerWeaponRatings");
    return;
  }
  auto weapon = Typed(attacker_stats->weapon);
  LiveGrowthHost host;
  const auto status = GrowAttackerWeaponRatings(
      Live::ToItemAction(*action), weapon, attacker_stats->actorId == 0,
      HitRatings{.class_id = defender_class_id,
                 .affinity_id = defender_affinity_id},
      host);
  Commit(attacker_stats->weapon, weapon);
  WarnRefusal("GrowAttackerWeaponRatings", status);
}

// decomp: BATTLE.PRG _itemRecovery (src/BATTLE/BATTLE.PRG/146C.c:12109)
std::int16_t CalculateLiveItemRecovery(Battle_ActionDefinition* action,
                                       Battle_HitResult* /*attacker_result*/,
                                       Battle_HitResult* /*target_result*/,
                                       int hit_index,
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
