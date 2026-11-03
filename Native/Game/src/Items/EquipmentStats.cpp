// Hand-maintained (tools/port/owned.csv).
#include "Items/EquipmentStats.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"

namespace VS::Items {
namespace {
// A damage type indexes the grip's types, then runs on into its classes and
// affinities, which follow the types in the part record.
constexpr std::size_t kGripTypesEnd = kPartTypeSlots;
constexpr std::size_t kGripClassesEnd = kGripTypesEnd + kPartClassSlots;
constexpr std::size_t kGripAffinitiesEnd = kGripClassesEnd + kPartAffinitySlots;

std::optional<std::uint8_t> GripTypeValue(const EquipmentPart& grip,
                                          std::size_t damage_type) {
  if (damage_type < kGripTypesEnd) {
    return static_cast<std::uint8_t>(grip.types[damage_type]);
  }
  if (damage_type < kGripClassesEnd) {
    return static_cast<std::uint8_t>(grip.classes[damage_type - kGripTypesEnd]);
  }
  if (damage_type < kGripAffinitiesEnd) {
    return static_cast<std::uint8_t>(
        grip.affinities[damage_type - kGripClassesEnd]);
  }
  return std::nullopt;
}

// Sums signed part statistics into the 16 bits a loadout stores.
template <typename... Values>
constexpr std::int16_t Total(Values... values) {
  return static_cast<std::int16_t>((static_cast<std::int32_t>(values) + ...));
}

// A signed part statistic widened as the original loads it (lb).
template <typename Wide>
constexpr Wide SignExtend(std::int8_t value) {
  return static_cast<Wide>(value);
}

struct AttributeTotals {
  std::int16_t strength{};
  std::int16_t intelligence{};
  std::int16_t agility{};
};

// The parts' attribute totals, in the 16 bits a loadout stores.
template <typename... Parts>
constexpr AttributeTotals SumAttributes(const Parts&... parts) {
  return {.strength = Total(parts.strength...),
          .intelligence = Total(parts.intelligence...),
          .agility = Total(parts.agility...)};
}

constexpr void SetAttributes(LoadoutAttributes& attributes,
                             const AttributeTotals& totals) {
  attributes.base_strength = static_cast<std::uint16_t>(totals.strength);
  attributes.current_strength = totals.strength;
  attributes.base_intelligence =
      static_cast<std::uint16_t>(totals.intelligence);
  attributes.current_intelligence = totals.intelligence;
  attributes.base_agility = static_cast<std::uint16_t>(totals.agility);
  attributes.current_agility = totals.agility;
}

// Writes one class total to the baseline and both current rows.
constexpr void SetClass(ClassAffinityCurrent& current,
                        ClassAffinityBaseline& baseline, std::size_t index,
                        std::int16_t total) {
  current.classes[kEquipmentRow][index] = total;
  current.classes[kEffectiveRow][index] = total;
  baseline.classes[index] = total;
}

// Writes one affinity total to the baseline and both current rows and clears
// its modifier.
constexpr void SetAffinity(ClassAffinityCurrent& current,
                           ClassAffinityBaseline& baseline,
                           AffinityTotals& modifiers, std::size_t index,
                           std::int16_t total) {
  current.affinities[kEquipmentRow][index] = total;
  current.affinities[kEffectiveRow][index] = total;
  baseline.affinities[index] = static_cast<std::uint16_t>(total);
  modifiers[index] = 0;
}

// Class and affinity totals of a part and its three gems.
void SetGemmedClassAffinity(
    const EquipmentPart& part,
    const std::array<EquipmentPart, kWeaponGemCount>& gems,
    ClassAffinityCurrent& current, ClassAffinityBaseline& baseline,
    AffinityTotals& modifiers) {
  for (std::size_t i = 0; i < kClassCount; ++i) {
    SetClass(current, baseline, i,
             Total(part.classes[i], gems[0].classes[i], gems[1].classes[i],
                   gems[2].classes[i]));
  }
  for (std::size_t i = 0; i < kAffinityCount; ++i) {
    SetAffinity(current, baseline, modifiers, i,
                Total(part.affinities[i], gems[0].affinities[i],
                      gems[1].affinities[i], gems[2].affinities[i]));
  }
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_applyWeaponStats
// (src/BATTLE/BATTLE.PRG/146C.c:968)
ItemsStatus BuildWeaponStatsFromParts(WeaponLoadout& target,
                                      const WeaponSpawnLoadout& source) {
  auto status = ItemsStatus::kOk;
  target.blade = source.blade;
  target.grip = source.grip;
  target.gems = source.gems;
  target.weapon_model_id =
      static_cast<std::uint8_t>(source.blade.weapon_model_id);
  target.drop_rate = source.drop_rate;
  target.index = static_cast<std::uint8_t>(source.index);
  target.blade.material = source.material;
  target.damage_type = source.blade.damage_type;
  target.action_type = source.blade.cost_type;
  if (const auto value = GripTypeValue(source.grip, target.damage_type)) {
    target.damage_type_value = *value;
  } else {
    target.damage_type_value = 0;
    status = ItemsStatus::kDamageTypeOutOfRange;
  }
  target.risk = source.blade.cost;
  target.recovery_delay = source.blade.opaque14;
  target.range = source.blade.range;
  target.current_dp = source.blade.current_dp;
  target.max_dp = source.blade.max_dp;
  target.current_pp = source.blade.current_pp;
  target.max_pp = source.blade.max_pp;
  const auto& gems = source.gems;
  SetAttributes(target.attributes, SumAttributes(source.blade, source.grip,
                                                 gems[0], gems[1], gems[2]));
  SetGemmedClassAffinity(source.blade, gems, target.class_affinity,
                         target.baseline, target.affinity_modifiers);
  target.name = source.name;
  return status;
}

// decomp: BATTLE.PRG vs_battle_applyShieldStats
// (src/BATTLE/BATTLE.PRG/146C.c:1028)
void BuildShieldStatsFromParts(ShieldLoadout& target,
                               const ShieldSpawnLoadout& source) {
  target.base = source.base;
  target.gems = source.gems;
  target.weapon_model_id =
      static_cast<std::uint8_t>(source.base.weapon_model_id);
  target.drop_rate = source.drop_rate;
  target.index = static_cast<std::uint8_t>(source.index);
  target.base.material = source.material;
  for (std::size_t i = 0; i < kPartTypeSlots; ++i) {
    target.types[i] = static_cast<std::uint8_t>(source.base.types[i]);
  }
  target.current_dp = source.base.current_dp;
  target.max_dp = source.base.max_dp;
  target.current_pp = source.base.current_pp;
  target.max_pp = source.base.max_pp;
  const auto& gems = source.gems;
  SetAttributes(target.attributes,
                SumAttributes(source.base, gems[0], gems[1], gems[2]));
  SetGemmedClassAffinity(source.base, gems, target.class_affinity,
                         target.baseline, target.affinity_modifiers);
}

// decomp: BATTLE.PRG _applyAccessoryStats (src/BATTLE/BATTLE.PRG/146C.c:1080)
void BuildAccessoryStatsFromPart(AccessorySpawnLoadout& target,
                                 const AccessoryInit& source) {
  const auto& part = source.equipment;
  target.accessory = part;
  target.current_strength = SignExtend<std::int16_t>(part.strength);
  target.current_intelligence = SignExtend<std::int16_t>(part.intelligence);
  target.current_agility = SignExtend<std::int16_t>(part.agility);
  target.drop_rate = source.drop_rate;
  target.index = source.index;
  for (std::size_t i = 0; i < kPartTypeSlots; ++i) {
    target.types[i] = SignExtend<std::uint16_t>(part.types[i]);
  }
  for (std::size_t i = 0; i < kClassCount; ++i) {
    target.classes[i] = SignExtend<std::uint16_t>(part.classes[i]);
  }
  for (std::size_t i = 0; i < kAffinityCount; ++i) {
    target.affinities[i] = SignExtend<std::uint16_t>(part.affinities[i]);
  }
}

// decomp: BATTLE.PRG _applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:1105)
void BuildArmorStatsFromPart(ArmorLoadout& target, const ArmorInit& source) {
  const auto& part = source.equipment;
  target.armor = part;
  target.armor.material = source.material;
  target.current_dp = part.current_dp;
  target.max_dp = part.max_dp;
  SetAttributes(target.attributes, SumAttributes(part));
  target.index = source.index;
  for (std::size_t i = 0; i < kPartTypeSlots; ++i) {
    target.types[i] = SignExtend<std::uint16_t>(part.types[i]);
  }
  for (std::size_t i = 0; i < kClassCount; ++i) {
    SetClass(target.class_affinity, target.baseline, i, part.classes[i]);
  }
  for (std::size_t i = 0; i < kAffinityCount; ++i) {
    SetAffinity(target.class_affinity, target.baseline,
                target.affinity_modifiers, i, part.affinities[i]);
  }
  target.drop_rate = source.drop_rate;
  target.opaque99 = source.opaque32;
}

namespace {
// The durability, points and attributes every part record carries.
template <typename Record>
void ReadWornStats(EquipmentPart& target, const Record& source) {
  target.current_dp = source.current_dp;
  target.max_dp = source.max_dp;
  target.current_pp = source.current_pp;
  target.max_pp = source.max_pp;
}

template <typename Record>
void ReadAttributes(EquipmentPart& target, const Record& source) {
  target.strength = source.strength;
  target.intelligence = source.intelligence;
  target.agility = source.agility;
}

template <typename Record>
void ReadClassAffinity(EquipmentPart& target, const Record& source) {
  for (std::size_t i = 0; i < kClassCount; ++i) {
    target.classes[i] = source.classes[i];
  }
  for (std::size_t i = 0; i < kAffinityCount; ++i) {
    target.affinities[i] = source.affinities[i];
  }
}

template <typename Record>
void WriteWornStats(Record& target, const EquipmentPart& source) {
  target.current_dp = source.current_dp;
  target.max_dp = source.max_dp;
  target.current_pp = source.current_pp;
  target.max_pp = source.max_pp;
}

template <typename Record>
void WriteAttributes(Record& target, const EquipmentPart& source) {
  target.strength = source.strength;
  target.intelligence = source.intelligence;
  target.agility = source.agility;
}

template <typename Record>
void WriteClassAffinity(Record& target, const EquipmentPart& source) {
  for (std::size_t i = 0; i < kClassCount; ++i) {
    target.classes[i] = source.classes[i];
  }
  for (std::size_t i = 0; i < kAffinityCount; ++i) {
    target.affinities[i] = source.affinities[i];
  }
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_copyInventoryBladeStats
// (src/BATTLE/BATTLE.PRG/146C.c:1142)
void ReadInventoryBladeStats(EquipmentPart& target,
                             const InventoryBlade& source) {
  target.id = source.id;
  target.sub_id = static_cast<std::int8_t>(source.sub_id);
  target.weapon_model_id = static_cast<std::int8_t>(source.weapon_model_id);
  target.category = source.category;
  ReadWornStats(target, source);
  ReadAttributes(target, source);
  target.cost = source.cost;
  target.damage_type = source.DamageType();
  target.cost_type = source.CostType();
  target.opaque14 = static_cast<std::uint8_t>(source.opaque12);
  target.range = source.range;
  target.index = source.index;
  ReadClassAffinity(target, source);
  target.material = static_cast<std::uint8_t>(source.material);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryGripStats
// (src/BATTLE/BATTLE.PRG/146C.c:1179)
void ReadInventoryGripStats(EquipmentPart& target,
                            const InventoryGrip& source) {
  target.id = source.id;
  target.sub_id = static_cast<std::int8_t>(source.sub_id);
  target.category = source.category;
  target.gem_slots = source.gem_slots;
  ReadAttributes(target, source);
  target.index = static_cast<std::uint8_t>(source.index);
  target.types = source.types;
}

// decomp: BATTLE.PRG vs_battle_copyInventoryArmorStats
// (src/BATTLE/BATTLE.PRG/146C.c:1198)
void ReadInventoryArmorStats(EquipmentPart& target,
                             const InventoryArmor& source) {
  target.id = source.id;
  target.sub_id = static_cast<std::int8_t>(source.sub_id);
  target.weapon_model_id = static_cast<std::int8_t>(source.weapon_model_id);
  target.category = source.category;
  ReadWornStats(target, source);
  target.gem_slots = source.gem_slots;
  ReadAttributes(target, source);
  target.index = source.index;
  target.types = source.types;
  ReadClassAffinity(target, source);
  target.material = static_cast<std::uint8_t>(source.material);
}

// decomp: BATTLE.PRG vs_battle_copyInventoryGemStats
// (src/BATTLE/BATTLE.PRG/146C.c:1235)
void ReadInventoryGemStats(EquipmentPart& target, const InventoryGem& source) {
  target.id = source.id;
  target.sub_id = static_cast<std::int8_t>(source.sub_id);
  target.gem_effects = source.gem_effects;
  ReadAttributes(target, source);
  target.index = static_cast<std::uint8_t>(source.index);
  ReadClassAffinity(target, source);
}

// decomp: BATTLE.PRG vs_battle_applyWeapon (src/BATTLE/BATTLE.PRG/146C.c:1257)
// The original assembled the parts in a high-end heap block; the parts here
// are a local record.
ItemsStatus LoadInventoryWeaponStats(WeaponLoadout& target,
                                     const InventoryWeapon* source,
                                     const InventoryContents& inventory) {
  auto status = ItemsStatus::kOk;
  WeaponSpawnLoadout parts{};
  if (source != nullptr) {
    parts.index = source->index;
    if (source->blade != 0) {
      if (const auto* blade = ReferencedSlot(inventory.blades, source->blade)) {
        ReadInventoryBladeStats(parts.blade, *blade);
        parts.material = static_cast<std::uint8_t>(blade->material);
      } else {
        KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
      }
    }
    if (source->grip != 0) {
      if (const auto* grip = ReferencedSlot(inventory.grips, source->grip)) {
        ReadInventoryGripStats(parts.grip, *grip);
      } else {
        KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
      }
    }
    for (std::size_t i = 0; i < kWeaponGemCount; ++i) {
      if (source->gems[i] == 0) continue;
      if (const auto* gem = ReferencedSlot(inventory.gems, source->gems[i])) {
        ReadInventoryGemStats(parts.gems[i], *gem);
      } else {
        KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
      }
    }
    parts.name = source->name;
  }
  KeepFirstRefusal(status, BuildWeaponStatsFromParts(target, parts));
  return status;
}

// decomp: BATTLE.PRG vs_battle_applyShield (src/BATTLE/BATTLE.PRG/146C.c:1295)
ItemsStatus LoadInventoryShieldStats(ShieldLoadout& target,
                                     const InventoryShield* source,
                                     const InventoryContents& inventory) {
  auto status = ItemsStatus::kOk;
  ShieldSpawnLoadout parts{};
  if (source != nullptr) {
    parts.index = source->index;
    ReadInventoryArmorStats(parts.base, source->base);
    parts.material = static_cast<std::uint8_t>(source->base.material);
    for (std::size_t i = 0; i < kWeaponGemCount; ++i) {
      if (source->gems[i] == 0) continue;
      const auto slot = source->gems[i] & kShieldGemSlotMask;
      if (const auto* gem = ReferencedSlot(inventory.gems, slot)) {
        ReadInventoryGemStats(parts.gems[i], *gem);
      } else {
        KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
      }
    }
  }
  BuildShieldStatsFromParts(target, parts);
  return status;
}

// decomp: BATTLE.PRG vs_battle_applyArmor (src/BATTLE/BATTLE.PRG/146C.c:1321)
void LoadInventoryArmorStats(ArmorLoadout& target,
                             const InventoryArmor* source) {
  ArmorInit part{};
  if (source != nullptr) {
    part.index = source->index;
    ReadInventoryArmorStats(part.equipment, *source);
    part.material = static_cast<std::uint8_t>(source->material);
  }
  BuildArmorStatsFromPart(target, part);
}

// decomp: BATTLE.PRG vs_battle_applyAccessory
// (src/BATTLE/BATTLE.PRG/146C.c:1338)
void LoadInventoryAccessoryStats(AccessorySpawnLoadout& target,
                                 const InventoryArmor* source) {
  AccessoryInit part{};
  if (source != nullptr) {
    part.index = source->index;
    ReadInventoryArmorStats(part.equipment, *source);
  }
  BuildAccessoryStatsFromPart(target, part);
}

// decomp: BATTLE.PRG func_8006B214 (src/BATTLE/BATTLE.PRG/146C.c:1356)
void RemoveEquipmentBoostOnEquip(EquipStatusHost& host) {
  if ((host.AshleyStatuses() & kEquipmentBoostStatus) != 0) {
    host.QueueStatusLabels(kEquipmentBoostStatus);
    host.RemoveStatuses(kEquipmentBoostStatus);
  }
}

namespace {
// Ends the statuses of `mask` Ashley has: the labels take the statuses read
// before queueing them and the removal those read after.
void EndStatuses(EquipStatusHost& host, std::int32_t mask) {
  const auto ended = host.AshleyStatuses() & mask;
  if (ended != 0) {
    host.QueueStatusLabels(ended);
    host.RemoveStatuses(host.AshleyStatuses() & mask);
  }
}
}  // namespace

// decomp: BATTLE.PRG func_8006B270 (src/BATTLE/BATTLE.PRG/146C.c:1366)
void RemoveWeaponEnchantmentOnEquip(EquipStatusHost& host) {
  EndStatuses(host, kWeaponEnchantmentStatuses);
}

// decomp: BATTLE.PRG func_8006B2D4 (src/BATTLE/BATTLE.PRG/146C.c:1376)
void RemoveArmorEnchantmentOnEquip(EquipStatusHost& host) {
  EndStatuses(host, kArmorEnchantmentStatuses);
}

// decomp: BATTLE.PRG vs_battle_equipWeapon (src/BATTLE/BATTLE.PRG/146C.c:1386)
// The actor callback the original ran after loading (vs_battle_nop0) has no
// effect.
ItemsStatus EquipAshleyWeapon(AshleyWeaponState& ashley,
                              const InventoryWeapon* weapon,
                              const InventoryContents& inventory,
                              EquipStatusHost& host) {
  auto status = LoadInventoryWeaponStats(ashley.weapon, weapon, inventory);
  if (weapon == nullptr) {
    ashley.equipped_weapon_category = kUnarmedWeaponCategory;
  } else if (const auto* blade =
                 ReferencedSlot(inventory.blades, weapon->blade)) {
    ashley.equipped_weapon_category = blade->category;
  } else {
    KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
  }
  const auto& range = ashley.weapon.range;
  ashley.current_range = static_cast<std::uint8_t>(range.range + ashley.reach);
  ashley.current_range_y =
      static_cast<std::uint8_t>(range.second_range + ashley.reach);
  ashley.current_range_z =
      static_cast<std::uint8_t>(range.third_range + ashley.reach);
  ashley.current_attack_shape_angle = range.AttackShapeAngle();
  ashley.current_attack_shape = 1;
  RemoveEquipmentBoostOnEquip(host);
  RemoveWeaponEnchantmentOnEquip(host);
  return status;
}

// decomp: BATTLE.PRG vs_battle_equipShield (src/BATTLE/BATTLE.PRG/146C.c:1411)
ItemsStatus EquipAshleyShield(ShieldLoadout& target,
                              const InventoryShield* shield,
                              const InventoryContents& inventory,
                              EquipStatusHost& host) {
  const auto status = LoadInventoryShieldStats(target, shield, inventory);
  RemoveEquipmentBoostOnEquip(host);
  RemoveArmorEnchantmentOnEquip(host);
  return status;
}

// decomp: BATTLE.PRG vs_battle_equipArmor (src/BATTLE/BATTLE.PRG/146C.c:1419)
ItemsStatus EquipAshleyArmor(std::span<ArmorLoadout> limb_armor,
                             std::int32_t body_part,
                             const InventoryArmor* armor,
                             EquipStatusHost& host) {
  if (body_part < 0 ||
      static_cast<std::size_t>(body_part) >= limb_armor.size()) {
    return ItemsStatus::kBodyPartOutOfRange;
  }
  LoadInventoryArmorStats(limb_armor[static_cast<std::size_t>(body_part)],
                          armor);
  RemoveEquipmentBoostOnEquip(host);
  RemoveArmorEnchantmentOnEquip(host);
  return ItemsStatus::kOk;
}

// decomp: BATTLE.PRG vs_battle_equipAccessory
// (src/BATTLE/BATTLE.PRG/146C.c:1427)
void EquipAshleyAccessory(AccessorySpawnLoadout& target,
                          const InventoryArmor* accessory,
                          EquipStatusHost& host) {
  LoadInventoryAccessoryStats(target, accessory);
  RemoveEquipmentBoostOnEquip(host);
  RemoveArmorEnchantmentOnEquip(host);
}

// decomp: BATTLE.PRG vs_battle_copyUiBladeStats
// (src/BATTLE/BATTLE.PRG/146C.c:1435)
void WriteInventoryBladeStats(InventoryBlade& target,
                              const EquipmentPart& source) {
  constexpr auto kTypeFields = static_cast<std::uint8_t>(
      kBladeDamageTypeMask | (kBladeCostTypeMask << kBladeCostTypeShift));
  target.id = static_cast<std::uint8_t>(source.id);
  target.sub_id = static_cast<std::uint8_t>(source.sub_id);
  target.weapon_model_id = static_cast<std::uint8_t>(source.weapon_model_id);
  target.category = source.category;
  WriteWornStats(target, source);
  WriteAttributes(target, source);
  target.cost = source.cost;
  target.type_bits = static_cast<std::uint8_t>(
      (target.type_bits & ~kTypeFields) |
      (source.damage_type & kBladeDamageTypeMask) |
      ((source.cost_type & kBladeCostTypeMask) << kBladeCostTypeShift));
  target.opaque12 = source.opaque14;
  target.range = source.range;
  WriteClassAffinity(target, source);
  target.material = source.material;
}

// decomp: BATTLE.PRG vs_battle_copyUiGripStats
// (src/BATTLE/BATTLE.PRG/146C.c:1471)
void WriteInventoryGripStats(InventoryGrip& target,
                             const EquipmentPart& source) {
  target.id = source.id;
  target.sub_id = static_cast<std::uint8_t>(source.sub_id);
  target.category = source.category;
  target.gem_slots = source.gem_slots;
  WriteAttributes(target, source);
  target.types = source.types;
}

// decomp: BATTLE.PRG vs_battle_copyUiGemStats
// (src/BATTLE/BATTLE.PRG/146C.c:1489)
void WriteInventoryGemStats(InventoryGem& target, const EquipmentPart& source) {
  target.id = source.id;
  target.sub_id = static_cast<std::uint8_t>(source.sub_id);
  target.gem_effects = source.gem_effects;
  WriteAttributes(target, source);
  WriteClassAffinity(target, source);
}

// decomp: BATTLE.PRG vs_battle_copyUiEquipmentStats
// (src/BATTLE/BATTLE.PRG/146C.c:1509)
void WriteInventoryArmorStats(InventoryArmor& target,
                              const EquipmentPart& source) {
  target.id = static_cast<std::uint8_t>(source.id);
  target.sub_id = static_cast<std::uint8_t>(source.sub_id);
  target.weapon_model_id = static_cast<std::uint8_t>(source.weapon_model_id);
  target.category = source.category;
  WriteWornStats(target, source);
  target.gem_slots = source.gem_slots;
  WriteAttributes(target, source);
  target.types = source.types;
  WriteClassAffinity(target, source);
  target.material = source.material;
}

// decomp: BATTLE.PRG vs_battle_copyUiWeaponStats
// (src/BATTLE/BATTLE.PRG/146C.c:1545)
ItemsStatus StoreInventoryWeaponStats(InventoryWeapon& target,
                                      WeaponLoadout& source,
                                      InventoryContents& inventory) {
  if (source.blade.id == 0) return ItemsStatus::kOk;
  auto status = ItemsStatus::kOk;
  source.blade.current_dp = source.current_dp;
  source.blade.current_pp = source.current_pp;
  if (auto* blade = ReferencedSlot(inventory.blades, target.blade)) {
    WriteInventoryBladeStats(*blade, source.blade);
  } else {
    KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
  }
  if (auto* grip = ReferencedSlot(inventory.grips, target.grip)) {
    WriteInventoryGripStats(*grip, source.grip);
  } else {
    KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
  }
  for (std::size_t i = 0; i < kWeaponGemCount; ++i) {
    if (source.gems[i].id == 0) continue;
    if (auto* gem = ReferencedSlot(inventory.gems, target.gems[i])) {
      WriteInventoryGemStats(*gem, source.gems[i]);
    } else {
      KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
    }
  }
  target.name = source.name;
  return status;
}

// decomp: BATTLE.PRG vs_battle_copyUiShieldStats
// (src/BATTLE/BATTLE.PRG/146C.c:1572)
// Unlike loading, storing uses the gem references without masking the shield
// flag.
ItemsStatus StoreInventoryShieldStats(InventoryShield& target,
                                      ShieldLoadout& source,
                                      InventoryContents& inventory) {
  if (source.base.id == 0) return ItemsStatus::kOk;
  auto status = ItemsStatus::kOk;
  source.base.current_dp = source.current_dp;
  source.base.current_pp = source.current_pp;
  WriteInventoryArmorStats(target.base, source.base);
  for (std::size_t i = 0; i < kWeaponGemCount; ++i) {
    if (source.gems[i].id == 0) continue;
    if (auto* gem = ReferencedSlot(inventory.gems, target.gems[i])) {
      WriteInventoryGemStats(*gem, source.gems[i]);
    } else {
      KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
    }
  }
  return status;
}

// decomp: BATTLE.PRG vs_battle_copyUiArmorStats
// (src/BATTLE/BATTLE.PRG/146C.c:1592)
void StoreInventoryArmorStats(InventoryArmor& target, ArmorLoadout& source) {
  if (source.armor.id == 0) return;
  source.armor.current_dp = source.current_dp;
  WriteInventoryArmorStats(target, source.armor);
}

// decomp: BATTLE.PRG vs_battle_copyUiAccessoryStats
// (src/BATTLE/BATTLE.PRG/146C.c:1600)
void StoreInventoryAccessoryStats(InventoryArmor& target,
                                  const AccessorySpawnLoadout& source) {
  if (source.accessory.id != 0) {
    WriteInventoryArmorStats(target, source.accessory);
  }
}
}  // namespace VS::Items
