#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

// Typed item records: equipment parts, the loadouts an actor fights with, the
// inventory tables the menus edit and the loot an enemy or chest drops. Each
// record keeps the byte layout the game stores (so a binding can move it in
// and out of the original memory unchanged) but is a plain value type that a
// modded item table can build without any PS1 structure.
namespace VS::Items {
// Record extents.
inline constexpr std::size_t kNameBytes = 24;
inline constexpr std::size_t kPartTypeSlots = 4;
inline constexpr std::size_t kPartClassSlots = 8;
inline constexpr std::size_t kPartAffinitySlots = 8;
// The classes (human .. evil) and affinities (physical .. dark) the game uses
// of each eight-entry table.
inline constexpr std::size_t kClassCount = 6;
inline constexpr std::size_t kAffinityCount = 7;
inline constexpr std::size_t kWeaponGemCount = 3;
inline constexpr std::size_t kShieldGemSlots = 4;
inline constexpr std::size_t kLimbCount = 6;

// Rows of ClassAffinityCurrent: the totals with temporary modifiers applied,
// and the equipment's own totals.
inline constexpr std::size_t kEffectiveRow = 0;
inline constexpr std::size_t kEquipmentRow = 1;

// Inventory table extents, in ItemCategory order.
inline constexpr std::size_t kWeaponSlots = 8;
inline constexpr std::size_t kShieldSlots = 8;
inline constexpr std::size_t kBladeSlots = 16;
inline constexpr std::size_t kGripSlots = 16;
inline constexpr std::size_t kArmorSlots = 16;
inline constexpr std::size_t kGemSlots = 48;
inline constexpr std::size_t kMiscSlots = 64;

// The inventory and menu categories (MAINMENU.PRG item lists).
enum class ItemCategory : std::uint8_t {
  kWeapon,
  kBlade,
  kGrip,
  kShield,
  kArmor,
  kGem,
  kMisc,
  // The manage-equipment row; it has two list entries and no item table.
  kManage,
};
inline constexpr std::size_t kItemCategoryCount = 8;

// Slots per category; the menus list the inventory tables with these counts
// (MAINMENU.PRG vs_mainMenu_inventoryItemCapacities). The manage row lists two
// entries.
inline constexpr std::size_t kManageEntries = 2;
using CategoryCounts = std::array<std::uint8_t, kItemCategoryCount>;
inline constexpr CategoryCounts kCategoryCapacities{
    kWeaponSlots, kBladeSlots, kGripSlots, kShieldSlots,
    kArmorSlots,  kGemSlots,   kMiscSlots, kManageEntries,
};

// Why an operation refused (part of) its work. Every refused case is one the
// original would have resolved by reading or writing outside the records the
// operation was given.
enum class ItemsStatus : std::uint8_t {
  kOk,
  kDamageTypeOutOfRange,
  kPartReferenceOutOfRange,
  kBodyPartOutOfRange,
  kHitLimbOutOfRange,
  kHitIndexOutOfRange,
  kAffinityOutOfRange,
  kClassOutOfRange,
  kCategoryOutOfRange,
  kSlotOutOfRange,
  kDisplayEntryOutOfRange,
  kNoChests,
  kActorOutOfRange,
};

[[nodiscard]] constexpr std::string_view Describe(ItemsStatus status) {
  switch (status) {
    case ItemsStatus::kOk:
      return "ok";
    case ItemsStatus::kDamageTypeOutOfRange:
      return "blade damage type outside the grip's type tables";
    case ItemsStatus::kPartReferenceOutOfRange:
      return "part reference outside its inventory table";
    case ItemsStatus::kBodyPartOutOfRange:
      return "body part outside the limb table";
    case ItemsStatus::kHitLimbOutOfRange:
      return "hit limb outside the limb table";
    case ItemsStatus::kHitIndexOutOfRange:
      return "hit index outside the action's two hits";
    case ItemsStatus::kAffinityOutOfRange:
      return "affinity outside physical..dark";
    case ItemsStatus::kClassOutOfRange:
      return "class outside human..evil";
    case ItemsStatus::kCategoryOutOfRange:
      return "item category outside the inventory tables";
    case ItemsStatus::kSlotOutOfRange:
      return "slot outside its inventory table";
    case ItemsStatus::kDisplayEntryOutOfRange:
      return "display entry outside its inventory table";
    case ItemsStatus::kNoChests:
      return "room has no treasure chest";
    case ItemsStatus::kActorOutOfRange:
      return "actor outside the battle actor table";
  }
  return "unknown";
}

// Keeps the first refusal of a multi-part operation.
constexpr void KeepFirstRefusal(ItemsStatus& status, ItemsStatus next) {
  if (status == ItemsStatus::kOk) status = next;
}

// The game's random draws (SLUS_010.40 vs_main_getRand and
// vs_main_getRandSmoothed) that item rules consume.
class ItemRandom {
 public:
  ItemRandom() = default;
  ItemRandom(const ItemRandom&) = delete;
  ItemRandom& operator=(const ItemRandom&) = delete;
  ItemRandom(ItemRandom&&) = delete;
  ItemRandom& operator=(ItemRandom&&) = delete;
  virtual ~ItemRandom() = default;

  // A draw in [0, bound).
  [[nodiscard]] virtual std::int32_t NextBounded(std::int32_t bound) = 0;
  // The mean of twelve NextBounded(bound) draws.
  [[nodiscard]] virtual std::int32_t NextAveraged(std::int32_t bound) = 0;
};

// The record a 1-based part reference names: null for 0 ("none") and for a
// reference past the table.
template <typename Table>
[[nodiscard]] constexpr auto* ReferencedSlot(Table& table,
                                             std::uint32_t reference) {
  using Element = std::remove_reference_t<decltype(table[0])>;
  return reference >= 1 && reference <= table.size()
             ? &table[reference - 1]
             : static_cast<Element*>(nullptr);
}

// A weapon's reach: three ranges and the attack shape packed in the fourth
// byte (shape in bits 0-2, angle in bits 3-7).
struct WeaponRange {
  std::uint8_t range{};
  std::uint8_t second_range{};
  std::uint8_t third_range{};
  std::uint8_t shape_bits{};
  [[nodiscard]] constexpr std::uint8_t AttackShapeAngle() const {
    constexpr int kAngleShift = 3;
    return static_cast<std::uint8_t>(shape_bits >> kAngleShift);
  }
  friend bool operator==(const WeaponRange&, const WeaponRange&) = default;
};

using PartTypes = std::array<std::int8_t, kPartTypeSlots>;
using PartClasses = std::array<std::int8_t, kPartClassSlots>;
using PartAffinities = std::array<std::int8_t, kPartAffinitySlots>;
using ItemName = std::array<char, kNameBytes>;

// decomp: vs_battle_equipment (src/BATTLE/BATTLE.PRG/146C.h): one blade,
// grip, gem, shield, armour or accessory as a loadout holds it.
struct EquipmentPart {
  std::uint16_t id{};
  std::int8_t sub_id{};
  std::int8_t weapon_model_id{};
  std::uint8_t category{};
  std::int8_t strength{};
  std::int8_t intelligence{};
  std::int8_t agility{};
  std::uint16_t current_dp{};
  std::uint16_t max_dp{};
  std::uint16_t current_pp{};
  std::uint16_t max_pp{};
  std::uint8_t damage_type{};
  std::uint8_t cost_type{};
  std::uint8_t cost{};
  std::uint8_t material{};
  std::uint8_t opaque14{};
  std::uint8_t gem_slots{};
  std::uint8_t gem_effects{};
  std::uint8_t index{};
  WeaponRange range{};
  PartTypes types{};
  PartClasses classes{};
  PartAffinities affinities{};
  friend bool operator==(const EquipmentPart&, const EquipmentPart&) = default;
};

using ClassTotals = std::array<std::int16_t, kPartClassSlots>;
using AffinityTotals = std::array<std::int16_t, kPartAffinitySlots>;

struct ClassAffinityCurrent {
  std::array<ClassTotals, 2> classes{};
  std::array<AffinityTotals, 2> affinities{};
  friend bool operator==(const ClassAffinityCurrent&,
                         const ClassAffinityCurrent&) = default;
};

struct ClassAffinityBaseline {
  ClassTotals classes{};
  std::array<std::uint16_t, kPartAffinitySlots> affinities{};
  friend bool operator==(const ClassAffinityBaseline&,
                         const ClassAffinityBaseline&) = default;
};

// The strength, intelligence and agility a loadout carries: current values
// (after boosts) and equipment totals.
struct LoadoutAttributes {
  std::int16_t current_strength{};
  std::uint16_t base_strength{};
  std::int16_t current_intelligence{};
  std::uint16_t base_intelligence{};
  std::int16_t current_agility{};
  std::uint16_t base_agility{};
  friend bool operator==(const LoadoutAttributes&,
                         const LoadoutAttributes&) = default;
};

// decomp: vs_battle_weaponInfo: an actor's assembled weapon.
struct WeaponLoadout {
  ItemName name{};
  EquipmentPart blade{};
  EquipmentPart grip{};
  std::array<EquipmentPart, kWeaponGemCount> gems{};
  std::uint8_t weapon_model_id{};
  std::uint8_t damage_type_value{};
  std::uint8_t risk{};
  std::uint8_t recovery_delay{};
  std::uint8_t drop_rate{};
  std::uint8_t index{};
  std::uint8_t damage_type{};
  std::uint8_t action_type{};
  std::uint16_t current_pp{};
  std::uint16_t max_pp{};
  std::uint16_t current_dp{};
  std::uint16_t max_dp{};
  LoadoutAttributes attributes{};
  WeaponRange range{};
  ClassAffinityCurrent class_affinity{};
  AffinityTotals affinity_modifiers{};
  ClassAffinityBaseline baseline{};
  friend bool operator==(const WeaponLoadout&, const WeaponLoadout&) = default;
};

// decomp: vs_battle_shieldInfo. It starts with words the items code does not
// use.
inline constexpr std::size_t kShieldHeaderWords = 6;
struct ShieldLoadout {
  std::array<std::int32_t, kShieldHeaderWords> reserved0{};
  EquipmentPart base{};
  std::array<EquipmentPart, kWeaponGemCount> gems{};
  std::uint8_t weapon_model_id{};
  std::uint8_t drop_rate{};
  std::uint8_t index{};
  std::uint8_t reserved_db{};
  std::uint16_t current_pp{};
  std::uint16_t max_pp{};
  std::uint16_t current_dp{};
  std::uint16_t max_dp{};
  LoadoutAttributes attributes{};
  std::array<std::uint8_t, kPartTypeSlots> types{};
  ClassAffinityCurrent class_affinity{};
  AffinityTotals affinity_modifiers{};
  ClassAffinityBaseline baseline{};
  friend bool operator==(const ShieldLoadout&, const ShieldLoadout&) = default;
};

// decomp: vs_battle_armorInfo: one limb's armour.
struct ArmorLoadout {
  EquipmentPart armor{};
  std::uint16_t current_dp{};
  std::uint16_t max_dp{};
  LoadoutAttributes attributes{};
  std::array<std::uint16_t, kPartTypeSlots> types{};
  ClassAffinityCurrent class_affinity{};
  AffinityTotals affinity_modifiers{};
  std::uint8_t drop_rate{};
  std::uint8_t opaque99{};
  std::uint8_t index{};
  std::uint8_t reserved9b{};
  ClassAffinityBaseline baseline{};
  friend bool operator==(const ArmorLoadout&, const ArmorLoadout&) = default;
};

// decomp: vs_battle_accessoryInfo.
struct AccessorySpawnLoadout {
  EquipmentPart accessory{};
  std::int16_t current_strength{};
  std::int16_t current_intelligence{};
  std::int16_t current_agility{};
  std::uint8_t drop_rate{};
  std::uint8_t index{};
  std::array<std::uint16_t, kPartTypeSlots> types{};
  std::array<std::uint16_t, kPartClassSlots> classes{};
  std::array<std::uint16_t, kPartAffinitySlots> affinities{};
  friend bool operator==(const AccessorySpawnLoadout&,
                         const AccessorySpawnLoadout&) = default;
};

// The part records an actor's weapon, shield, armour and accessory are built
// from (ZND enemy equipment, or the parts read from the inventory).
struct WeaponSpawnLoadout {
  EquipmentPart blade{};
  EquipmentPart grip{};
  std::array<EquipmentPart, kWeaponGemCount> gems{};
  std::uint8_t material{};
  std::uint8_t drop_rate{};
  std::int16_t index{};
  ItemName name{};
};

struct ShieldSpawnLoadout {
  EquipmentPart base{};
  std::array<EquipmentPart, kWeaponGemCount> gems{};
  std::uint8_t material{};
  std::uint8_t drop_rate{};
  std::int16_t index{};
};

struct ArmorInit {
  EquipmentPart equipment{};
  std::uint8_t material{};
  std::uint8_t drop_rate{};
  std::uint8_t opaque32{};
  std::uint8_t index{};
};

struct AccessoryInit {
  EquipmentPart equipment{};
  std::uint8_t drop_rate{};
  std::uint8_t index{};
  std::uint8_t reserved32{};
  std::uint8_t reserved33{};
};

// Inventory records (decomp: vs_battle_inventory*).
struct InventoryArmor {
  std::uint8_t id{};
  std::uint8_t sub_id{};
  std::uint8_t weapon_model_id{};
  std::uint8_t category{};
  std::uint16_t max_dp{};
  std::uint16_t max_pp{};
  std::uint16_t current_dp{};
  std::uint16_t current_pp{};
  std::uint8_t gem_slots{};
  std::int8_t strength{};
  std::int8_t intelligence{};
  std::int8_t agility{};
  PartTypes types{};
  PartClasses classes{};
  PartAffinities affinities{};
  std::uint16_t material{};
  std::uint8_t limb{};
  std::uint8_t index{};
  friend bool operator==(const InventoryArmor&,
                         const InventoryArmor&) = default;
};

// The blade's damage type (bits 0-1) and cost type (bits 2-4) share a byte.
inline constexpr std::uint8_t kBladeDamageTypeMask = 0x3;
inline constexpr int kBladeCostTypeShift = 2;
inline constexpr std::uint8_t kBladeCostTypeMask = 0x7;

struct InventoryBlade {
  std::uint8_t id{};
  std::uint8_t sub_id{};
  std::uint8_t weapon_model_id{};
  std::uint8_t category{};
  std::uint16_t max_dp{};
  std::uint16_t max_pp{};
  std::uint16_t current_dp{};
  std::uint16_t current_pp{};
  std::int8_t strength{};
  std::int8_t intelligence{};
  std::int8_t agility{};
  std::uint8_t cost{};
  std::uint8_t type_bits{};
  std::uint8_t reserved11{};
  std::uint16_t opaque12{};
  WeaponRange range{};
  PartClasses classes{};
  PartAffinities affinities{};
  std::uint16_t material{};
  std::uint8_t assembled_weapon_index{};
  std::uint8_t index{};
  [[nodiscard]] constexpr std::uint8_t DamageType() const {
    return type_bits & kBladeDamageTypeMask;
  }
  [[nodiscard]] constexpr std::uint8_t CostType() const {
    return (type_bits >> kBladeCostTypeShift) & kBladeCostTypeMask;
  }
  friend bool operator==(const InventoryBlade&,
                         const InventoryBlade&) = default;
};

struct InventoryGem {
  std::uint16_t id{};
  std::uint8_t sub_id{};
  std::uint8_t reserved3{};
  std::uint8_t gem_effects{};
  std::int8_t strength{};
  std::int8_t intelligence{};
  std::int8_t agility{};
  PartClasses classes{};
  PartAffinities affinities{};
  std::uint16_t set_item_index{};
  std::int16_t index{};
  friend bool operator==(const InventoryGem&, const InventoryGem&) = default;
};

struct InventoryGrip {
  std::uint16_t id{};
  std::uint8_t sub_id{};
  std::uint8_t category{};
  std::uint8_t gem_slots{};
  std::int8_t strength{};
  std::int8_t intelligence{};
  std::int8_t agility{};
  PartTypes types{};
  std::uint16_t assembled_weapon_index{};
  std::uint16_t index{};
  friend bool operator==(const InventoryGrip&, const InventoryGrip&) = default;
};

struct InventoryMisc {
  std::uint16_t id{};
  std::uint8_t count{};
  std::uint8_t index{};
  friend bool operator==(const InventoryMisc&, const InventoryMisc&) = default;
};

// A gem set in a shield names its owner with kShieldGemFlag over the shield's
// 1-based slot; loading a shield masks its gem references with
// kShieldGemSlotMask.
inline constexpr std::uint8_t kShieldGemFlag = 0x80;
inline constexpr std::uint8_t kShieldGemSlotMask = 0x7F;

struct InventoryShield {
  std::uint8_t index{};
  std::uint8_t is_equipped{};
  std::int16_t reserved2{};
  InventoryArmor base{};
  std::array<std::uint8_t, kShieldGemSlots> gems{};
  friend bool operator==(const InventoryShield&,
                         const InventoryShield&) = default;
};

// Part references are 1-based slots of the inventory tables; 0 is "none".
struct InventoryWeapon {
  std::uint8_t index{};
  std::uint8_t blade{};
  std::uint8_t grip{};
  std::uint8_t is_equipped{};
  std::array<std::uint8_t, kShieldGemSlots> gems{};
  ItemName name{};
  friend bool operator==(const InventoryWeapon&,
                         const InventoryWeapon&) = default;
};

// decomp: vs_battle_inventory: Ashley's (or a staged) inventory.
struct InventoryContents {
  std::array<InventoryWeapon, kWeaponSlots> weapons{};
  std::array<InventoryShield, kShieldSlots> shields{};
  std::array<InventoryBlade, kBladeSlots> blades{};
  std::array<InventoryGrip, kGripSlots> grips{};
  std::array<InventoryArmor, kArmorSlots> armor{};
  std::array<InventoryGem, kGemSlots> gems{};
  std::array<InventoryMisc, kMiscSlots> misc{};
  friend bool operator==(const InventoryContents&,
                         const InventoryContents&) = default;
};

// The menus' display order: per category, 1-based slots in list order, 0 for
// an empty row.
struct InventoryIndices {
  std::array<std::uint8_t, kWeaponSlots> weapons{};
  std::array<std::uint8_t, kBladeSlots> blades{};
  std::array<std::uint8_t, kGripSlots> grips{};
  std::array<std::uint8_t, kShieldSlots> shields{};
  std::array<std::uint8_t, kArmorSlots> armor{};
  std::array<std::uint8_t, kGemSlots> gems{};
  std::array<std::uint8_t, kMiscSlots> misc{};
  friend bool operator==(const InventoryIndices&,
                         const InventoryIndices&) = default;
};

// An enemy's guaranteed drop and its random drop chance.
struct MiscItemStack {
  std::uint16_t id{};
  std::uint8_t count{};
  std::uint8_t reserved03{};
};

struct MiscItemDrop {
  std::uint16_t id{};
  std::uint8_t drop_rate{};
  std::uint8_t reserved03{};
};

// Dropped items. difficulty_flags is kLootDropped for an item that dropped.
inline constexpr std::uint8_t kLootDropped = 3;
inline constexpr std::size_t kLootHeaderPadding = 3;

struct LootHeader {
  std::uint8_t difficulty_flags{};
  std::array<std::uint8_t, kLootHeaderPadding> reserved{};
  friend bool operator==(const LootHeader&, const LootHeader&) = default;
};

struct LootedWeapon {
  LootHeader header{};
  InventoryBlade blade{};
  InventoryGrip grip{};
  std::array<InventoryGem, kWeaponGemCount> gems{};
  ItemName name{};
  friend bool operator==(const LootedWeapon&, const LootedWeapon&) = default;
};

struct LootedBlade {
  LootHeader header{};
  InventoryBlade blade{};
  friend bool operator==(const LootedBlade&, const LootedBlade&) = default;
};

struct LootedGrip {
  LootHeader header{};
  InventoryGrip grip{};
  friend bool operator==(const LootedGrip&, const LootedGrip&) = default;
};

struct LootedShield {
  LootHeader header{};
  InventoryArmor shield{};
  std::array<InventoryGem, kWeaponGemCount> gems{};
  friend bool operator==(const LootedShield&, const LootedShield&) = default;
};

// Looted armour and accessories share the record.
struct LootedArmor {
  LootHeader header{};
  InventoryArmor armor{};
  friend bool operator==(const LootedArmor&, const LootedArmor&) = default;
};

struct LootedGem {
  LootHeader header{};
  InventoryGem gem{};
  friend bool operator==(const LootedGem&, const LootedGem&) = default;
};

struct LootedItem {
  std::uint16_t item_id{};
  std::uint8_t difficulty_flags{};
  std::uint8_t count{};
  friend bool operator==(const LootedItem&, const LootedItem&) = default;
};

inline constexpr std::size_t kLootArmorSlots = 2;
inline constexpr std::size_t kLootItemSlots = 3;
// The two guaranteed drops fill item slots 0-1 and the random drop slot 2.
inline constexpr std::size_t kGuaranteedDropCount = 2;
inline constexpr std::size_t kRandomDropSlot = 2;

// decomp: vs_battle_lootList's payload: one enemy's or chest's loot.
struct BattleLoot {
  LootedWeapon weapon{};
  LootedBlade blade{};
  LootedGrip grip{};
  LootedShield shield{};
  std::array<LootedArmor, kLootArmorSlots> armor{};
  LootedArmor accessory{};
  LootedGem gem{};
  std::array<LootedItem, kLootItemSlots> misc{};
  friend bool operator==(const BattleLoot&, const BattleLoot&) = default;
};

// decomp: vs_battle_treasureChest: one chest in a room's MPD loot section.
struct TreasureChest {
  std::int16_t tile_x{};
  std::int16_t tile_z{};
  std::int16_t layer{};
  std::int16_t lid_rotation_id{};
  std::int16_t is_open{};
  std::int16_t reserved_a{};
  std::int16_t lock_level{};
  std::int16_t reserved_e{};
  std::int16_t opened_flag{};
  std::int16_t facing{};
  BattleLoot loot{};
  friend bool operator==(const TreasureChest&, const TreasureChest&) = default;
};

// What item rules read of a battle action (decomp: vs_action_t).
inline constexpr std::uint8_t kSpellActionType = 1;
inline constexpr std::size_t kActionHitCount = 2;

struct ItemAction {
  std::uint8_t type{};
  std::uint8_t category{};
  // The first hit's affinity, 1-based (physical .. dark); 0 for none.
  std::uint8_t first_hit_affinity{};
  std::array<std::uint8_t, kActionHitCount> hit_stat_factors{};
  [[nodiscard]] constexpr bool IsSpell() const {
    return type == kSpellActionType;
  }
};

// The equipment of an actor that is hit (decomp: vs_battle_actorStats).
struct DefenderLimb {
  std::int16_t max_hp{};
  // The limb's name, which also labels its armour's stat-change notices.
  std::uint8_t name_index{};
  ArmorLoadout armor{};
};

struct DefenderEquipment {
  ShieldLoadout shield{};
  std::array<DefenderLimb, kLimbCount> limbs{};
  // Whether the actor holds its weapon (and so its shield) up; empty when
  // the actor is not in the battle actor table.
  std::optional<bool> weapon_drawn{};
  // Stat-change notices are shown for Ashley's equipment only.
  bool shows_notices{};
};

// The byte sizes and field offsets the game stores these records with.
namespace layout {
inline constexpr std::size_t kWeaponRangeBytes = 0x4;
static_assert(sizeof(WeaponRange) == kWeaponRangeBytes);
inline constexpr std::size_t kEquipmentPartBytes = 0x30;
static_assert(sizeof(EquipmentPart) == kEquipmentPartBytes);
inline constexpr std::size_t kEquipmentPartRangeOffset = 0x18;
static_assert(offsetof(EquipmentPart, range) == kEquipmentPartRangeOffset);
inline constexpr std::size_t kEquipmentPartTypesOffset = 0x1C;
static_assert(offsetof(EquipmentPart, types) == kEquipmentPartTypesOffset);
inline constexpr std::size_t kEquipmentPartAffinitiesOffset = 0x28;
static_assert(offsetof(EquipmentPart, affinities) ==
              kEquipmentPartAffinitiesOffset);
inline constexpr std::size_t kClassAffinityCurrentBytes = 0x40;
static_assert(sizeof(ClassAffinityCurrent) == kClassAffinityCurrentBytes);
inline constexpr std::size_t kClassAffinityBaselineBytes = 0x20;
static_assert(sizeof(ClassAffinityBaseline) == kClassAffinityBaselineBytes);
inline constexpr std::size_t kWeaponLoadoutBytes = 0x198;
static_assert(sizeof(WeaponLoadout) == kWeaponLoadoutBytes);
inline constexpr std::size_t kWeaponLoadoutWeaponModelIdOffset = 0x108;
static_assert(offsetof(WeaponLoadout, weapon_model_id) ==
              kWeaponLoadoutWeaponModelIdOffset);
inline constexpr std::size_t kWeaponLoadoutCurrentPpOffset = 0x110;
static_assert(offsetof(WeaponLoadout, current_pp) ==
              kWeaponLoadoutCurrentPpOffset);
inline constexpr std::size_t kWeaponLoadoutAttributesOffset = 0x118;
static_assert(offsetof(WeaponLoadout, attributes) ==
              kWeaponLoadoutAttributesOffset);
inline constexpr std::size_t kWeaponLoadoutRangeOffset = 0x124;
static_assert(offsetof(WeaponLoadout, range) == kWeaponLoadoutRangeOffset);
inline constexpr std::size_t kWeaponLoadoutClassAffinityOffset = 0x128;
static_assert(offsetof(WeaponLoadout, class_affinity) ==
              kWeaponLoadoutClassAffinityOffset);
inline constexpr std::size_t kWeaponLoadoutAffinityModifiersOffset = 0x168;
static_assert(offsetof(WeaponLoadout, affinity_modifiers) ==
              kWeaponLoadoutAffinityModifiersOffset);
inline constexpr std::size_t kWeaponLoadoutBaselineOffset = 0x178;
static_assert(offsetof(WeaponLoadout, baseline) ==
              kWeaponLoadoutBaselineOffset);
inline constexpr std::size_t kShieldLoadoutBytes = 0x164;
static_assert(sizeof(ShieldLoadout) == kShieldLoadoutBytes);
inline constexpr std::size_t kShieldLoadoutBaseOffset = 0x18;
static_assert(offsetof(ShieldLoadout, base) == kShieldLoadoutBaseOffset);
inline constexpr std::size_t kShieldLoadoutWeaponModelIdOffset = 0xD8;
static_assert(offsetof(ShieldLoadout, weapon_model_id) ==
              kShieldLoadoutWeaponModelIdOffset);
inline constexpr std::size_t kShieldLoadoutCurrentPpOffset = 0xDC;
static_assert(offsetof(ShieldLoadout, current_pp) ==
              kShieldLoadoutCurrentPpOffset);
inline constexpr std::size_t kShieldLoadoutAttributesOffset = 0xE4;
static_assert(offsetof(ShieldLoadout, attributes) ==
              kShieldLoadoutAttributesOffset);
inline constexpr std::size_t kShieldLoadoutTypesOffset = 0xF0;
static_assert(offsetof(ShieldLoadout, types) == kShieldLoadoutTypesOffset);
inline constexpr std::size_t kShieldLoadoutClassAffinityOffset = 0xF4;
static_assert(offsetof(ShieldLoadout, class_affinity) ==
              kShieldLoadoutClassAffinityOffset);
inline constexpr std::size_t kShieldLoadoutAffinityModifiersOffset = 0x134;
static_assert(offsetof(ShieldLoadout, affinity_modifiers) ==
              kShieldLoadoutAffinityModifiersOffset);
inline constexpr std::size_t kShieldLoadoutBaselineOffset = 0x144;
static_assert(offsetof(ShieldLoadout, baseline) ==
              kShieldLoadoutBaselineOffset);
inline constexpr std::size_t kArmorLoadoutBytes = 0xBC;
static_assert(sizeof(ArmorLoadout) == kArmorLoadoutBytes);
inline constexpr std::size_t kArmorLoadoutCurrentDpOffset = 0x30;
static_assert(offsetof(ArmorLoadout, current_dp) ==
              kArmorLoadoutCurrentDpOffset);
inline constexpr std::size_t kArmorLoadoutAttributesOffset = 0x34;
static_assert(offsetof(ArmorLoadout, attributes) ==
              kArmorLoadoutAttributesOffset);
inline constexpr std::size_t kArmorLoadoutTypesOffset = 0x40;
static_assert(offsetof(ArmorLoadout, types) == kArmorLoadoutTypesOffset);
inline constexpr std::size_t kArmorLoadoutClassAffinityOffset = 0x48;
static_assert(offsetof(ArmorLoadout, class_affinity) ==
              kArmorLoadoutClassAffinityOffset);
inline constexpr std::size_t kArmorLoadoutAffinityModifiersOffset = 0x88;
static_assert(offsetof(ArmorLoadout, affinity_modifiers) ==
              kArmorLoadoutAffinityModifiersOffset);
inline constexpr std::size_t kArmorLoadoutDropRateOffset = 0x98;
static_assert(offsetof(ArmorLoadout, drop_rate) == kArmorLoadoutDropRateOffset);
inline constexpr std::size_t kArmorLoadoutBaselineOffset = 0x9C;
static_assert(offsetof(ArmorLoadout, baseline) == kArmorLoadoutBaselineOffset);
inline constexpr std::size_t kAccessorySpawnLoadoutBytes = 0x60;
static_assert(sizeof(AccessorySpawnLoadout) == kAccessorySpawnLoadoutBytes);
inline constexpr std::size_t kAccessorySpawnLoadoutDropRateOffset = 0x36;
static_assert(offsetof(AccessorySpawnLoadout, drop_rate) ==
              kAccessorySpawnLoadoutDropRateOffset);
inline constexpr std::size_t kAccessorySpawnLoadoutTypesOffset = 0x38;
static_assert(offsetof(AccessorySpawnLoadout, types) ==
              kAccessorySpawnLoadoutTypesOffset);
inline constexpr std::size_t kAccessorySpawnLoadoutAffinitiesOffset = 0x50;
static_assert(offsetof(AccessorySpawnLoadout, affinities) ==
              kAccessorySpawnLoadoutAffinitiesOffset);
inline constexpr std::size_t kWeaponSpawnLoadoutBytes = 0x10C;
static_assert(sizeof(WeaponSpawnLoadout) == kWeaponSpawnLoadoutBytes);
inline constexpr std::size_t kWeaponSpawnLoadoutMaterialOffset = 0xF0;
static_assert(offsetof(WeaponSpawnLoadout, material) ==
              kWeaponSpawnLoadoutMaterialOffset);
inline constexpr std::size_t kWeaponSpawnLoadoutNameOffset = 0xF4;
static_assert(offsetof(WeaponSpawnLoadout, name) ==
              kWeaponSpawnLoadoutNameOffset);
inline constexpr std::size_t kShieldSpawnLoadoutBytes = 0xC4;
static_assert(sizeof(ShieldSpawnLoadout) == kShieldSpawnLoadoutBytes);
inline constexpr std::size_t kShieldSpawnLoadoutIndexOffset = 0xC2;
static_assert(offsetof(ShieldSpawnLoadout, index) ==
              kShieldSpawnLoadoutIndexOffset);
inline constexpr std::size_t kArmorInitBytes = 0x34;
static_assert(sizeof(ArmorInit) == kArmorInitBytes);
inline constexpr std::size_t kAccessoryInitBytes = 0x34;
static_assert(sizeof(AccessoryInit) == kAccessoryInitBytes);
inline constexpr std::size_t kInventoryArmorBytes = 0x28;
static_assert(sizeof(InventoryArmor) == kInventoryArmorBytes);
inline constexpr std::size_t kInventoryArmorTypesOffset = 0x10;
static_assert(offsetof(InventoryArmor, types) == kInventoryArmorTypesOffset);
inline constexpr std::size_t kInventoryArmorMaterialOffset = 0x24;
static_assert(offsetof(InventoryArmor, material) ==
              kInventoryArmorMaterialOffset);
inline constexpr std::size_t kInventoryBladeBytes = 0x2C;
static_assert(sizeof(InventoryBlade) == kInventoryBladeBytes);
inline constexpr std::size_t kInventoryBladeTypeBitsOffset = 0x10;
static_assert(offsetof(InventoryBlade, type_bits) ==
              kInventoryBladeTypeBitsOffset);
inline constexpr std::size_t kInventoryBladeRangeOffset = 0x14;
static_assert(offsetof(InventoryBlade, range) == kInventoryBladeRangeOffset);
inline constexpr std::size_t kInventoryBladeMaterialOffset = 0x28;
static_assert(offsetof(InventoryBlade, material) ==
              kInventoryBladeMaterialOffset);
inline constexpr std::size_t kInventoryGemBytes = 0x1C;
static_assert(sizeof(InventoryGem) == kInventoryGemBytes);
inline constexpr std::size_t kInventoryGemSetItemIndexOffset = 0x18;
static_assert(offsetof(InventoryGem, set_item_index) ==
              kInventoryGemSetItemIndexOffset);
inline constexpr std::size_t kInventoryGripBytes = 0x10;
static_assert(sizeof(InventoryGrip) == kInventoryGripBytes);
inline constexpr std::size_t kInventoryGripAssembledWeaponIndexOffset = 0xC;
static_assert(offsetof(InventoryGrip, assembled_weapon_index) ==
              kInventoryGripAssembledWeaponIndexOffset);
inline constexpr std::size_t kInventoryMiscBytes = 0x4;
static_assert(sizeof(InventoryMisc) == kInventoryMiscBytes);
inline constexpr std::size_t kInventoryShieldBytes = 0x30;
static_assert(sizeof(InventoryShield) == kInventoryShieldBytes);
inline constexpr std::size_t kInventoryShieldGemsOffset = 0x2C;
static_assert(offsetof(InventoryShield, gems) == kInventoryShieldGemsOffset);
inline constexpr std::size_t kInventoryWeaponBytes = 0x20;
static_assert(sizeof(InventoryWeapon) == kInventoryWeaponBytes);
inline constexpr std::size_t kInventoryContentsBytes = 0xF00;
static_assert(sizeof(InventoryContents) == kInventoryContentsBytes);
inline constexpr std::size_t kInventoryContentsShieldsOffset = 0x100;
static_assert(offsetof(InventoryContents, shields) ==
              kInventoryContentsShieldsOffset);
inline constexpr std::size_t kInventoryContentsBladesOffset = 0x280;
static_assert(offsetof(InventoryContents, blades) ==
              kInventoryContentsBladesOffset);
inline constexpr std::size_t kInventoryContentsGripsOffset = 0x540;
static_assert(offsetof(InventoryContents, grips) ==
              kInventoryContentsGripsOffset);
inline constexpr std::size_t kInventoryContentsArmorOffset = 0x640;
static_assert(offsetof(InventoryContents, armor) ==
              kInventoryContentsArmorOffset);
inline constexpr std::size_t kInventoryContentsGemsOffset = 0x8C0;
static_assert(offsetof(InventoryContents, gems) ==
              kInventoryContentsGemsOffset);
inline constexpr std::size_t kInventoryContentsMiscOffset = 0xE00;
static_assert(offsetof(InventoryContents, misc) ==
              kInventoryContentsMiscOffset);
inline constexpr std::size_t kInventoryIndicesBytes = 0xB0;
static_assert(sizeof(InventoryIndices) == kInventoryIndicesBytes);
inline constexpr std::size_t kMiscItemStackBytes = 0x4;
static_assert(sizeof(MiscItemStack) == kMiscItemStackBytes);
inline constexpr std::size_t kMiscItemDropBytes = 0x4;
static_assert(sizeof(MiscItemDrop) == kMiscItemDropBytes);
inline constexpr std::size_t kLootedWeaponBytes = 0xAC;
static_assert(sizeof(LootedWeapon) == kLootedWeaponBytes);
inline constexpr std::size_t kLootedBladeBytes = 0x30;
static_assert(sizeof(LootedBlade) == kLootedBladeBytes);
inline constexpr std::size_t kLootedGripBytes = 0x14;
static_assert(sizeof(LootedGrip) == kLootedGripBytes);
inline constexpr std::size_t kLootedShieldBytes = 0x80;
static_assert(sizeof(LootedShield) == kLootedShieldBytes);
inline constexpr std::size_t kLootedArmorBytes = 0x2C;
static_assert(sizeof(LootedArmor) == kLootedArmorBytes);
inline constexpr std::size_t kLootedGemBytes = 0x20;
static_assert(sizeof(LootedGem) == kLootedGemBytes);
inline constexpr std::size_t kLootedItemBytes = 0x4;
static_assert(sizeof(LootedItem) == kLootedItemBytes);
inline constexpr std::size_t kBattleLootBytes = 0x220;
static_assert(sizeof(BattleLoot) == kBattleLootBytes);
inline constexpr std::size_t kBattleLootShieldOffset = 0xF0;
static_assert(offsetof(BattleLoot, shield) == kBattleLootShieldOffset);
inline constexpr std::size_t kBattleLootArmorOffset = 0x170;
static_assert(offsetof(BattleLoot, armor) == kBattleLootArmorOffset);
inline constexpr std::size_t kBattleLootAccessoryOffset = 0x1C8;
static_assert(offsetof(BattleLoot, accessory) == kBattleLootAccessoryOffset);
inline constexpr std::size_t kBattleLootGemOffset = 0x1F4;
static_assert(offsetof(BattleLoot, gem) == kBattleLootGemOffset);
inline constexpr std::size_t kBattleLootMiscOffset = 0x214;
static_assert(offsetof(BattleLoot, misc) == kBattleLootMiscOffset);
inline constexpr std::size_t kTreasureChestBytes = 0x234;
static_assert(sizeof(TreasureChest) == kTreasureChestBytes);
inline constexpr std::size_t kTreasureChestLootOffset = 0x14;
static_assert(offsetof(TreasureChest, loot) == kTreasureChestLootOffset);
}  // namespace layout
}  // namespace VS::Items
