#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/Log.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Items/EquipmentAffinity.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/EquipmentStats.hpp"
#include "Items/EquipmentWear.hpp"
#include "Items/Inventory.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/InventoryLiveState.h"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Items/Loot.hpp"
#include "Items/LootLive.hpp"
#include "Items/LootLiveState.h"
#include "Items/Recovery.hpp"
#include "Legacy/Psx.h"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Gimmicks.h"
#include "Field/Room.h"
#include "Items/Categories.h"
}

namespace {
namespace items = VS::Items;
namespace inv = VS::Items::Inventory;
namespace live = VS::Items::Live;
using items::ItemCategory;
using items::ItemsStatus;

// ---------------------------------------------------------------------------
// Scripted hosts.

struct Notice {
  std::int32_t label{};
  std::int32_t stat{};
  items::StatChange change{};
  friend bool operator==(const Notice&, const Notice&) = default;
};

// A bounded draw past the scripted ones fails every chance.
constexpr std::int32_t kNoDraw = 255;
constexpr std::int32_t kGrowthBound = 256;

class ScriptedHost final : public items::EquipmentGrowthHost {
 public:
  ScriptedHost() = default;
  explicit ScriptedHost(std::vector<std::int32_t> draws)
      : draws_(std::move(draws)) {}

  std::int32_t NextBounded(std::int32_t bound) override {
    bounds.push_back(bound);
    return Next(kNoDraw);
  }
  std::int32_t NextAveraged(std::int32_t bound) override {
    averaged_bounds.push_back(bound);
    return Next(0);
  }
  void QueueStatChange(std::int32_t label, std::int32_t stat,
                       items::StatChange change) override {
    notices.push_back({label, stat, change});
  }

  std::vector<std::int32_t> bounds;
  std::vector<std::int32_t> averaged_bounds;
  std::vector<Notice> notices;

 private:
  std::int32_t Next(std::int32_t exhausted) {
    return next_ < draws_.size() ? draws_[next_++] : exhausted;
  }

  std::vector<std::int32_t> draws_;
  std::size_t next_ = 0;
};

class FakeStatusHost final : public items::EquipStatusHost {
 public:
  explicit FakeStatusHost(std::int32_t initial) : statuses(initial) {}

  std::int32_t AshleyStatuses() override {
    ++reads;
    return statuses;
  }
  void QueueStatusLabels(std::int32_t ended) override {
    labels.push_back(ended);
    statuses |= added_on_label;
  }
  void RemoveStatuses(std::int32_t ended) override {
    removed.push_back(ended);
    statuses &= ~ended;
  }

  std::int32_t statuses{};
  // Statuses a label callback adds before the removal reads them again.
  std::int32_t added_on_label{};
  std::int32_t reads{};
  std::vector<std::int32_t> labels;
  std::vector<std::int32_t> removed;
};

class FakeLootFlow final : public items::LootList, public items::LootFlowHost {
 public:
  bool ReserveEntry() override {
    calls.emplace_back("reserve");
    return can_reserve;
  }
  void PushReservedEntry(std::int32_t dropper,
                         const items::BattleLoot& loot) override {
    calls.emplace_back("push");
    droppers.push_back(dropper);
    pushed.push_back(loot);
  }
  void ReleaseReservedEntry() override { calls.emplace_back("release"); }
  bool HasEntryFrom(std::int32_t dropper) const override {
    return std::find(listed.begin(), listed.end(), dropper) != listed.end();
  }
  bool IsEmpty() const override { return empty && pushed.empty(); }
  void Clear() override { calls.emplace_back("clear"); }

  std::int32_t RewardStage() override { return reward_stage; }
  void OpenEnemyLootList() override { calls.emplace_back("open-enemy"); }
  void BeginPendingAbilityReward() override {
    calls.emplace_back("ability-reward");
  }
  void WaitForPostActionMovement() override { calls.emplace_back("wait"); }
  void OpenScriptedLootList() override { calls.emplace_back("open-scripted"); }
  void ResumeAfterRewards() override { calls.emplace_back("resume"); }
  void ResetLootPaging() override { calls.emplace_back("reset-paging"); }

  bool can_reserve = true;
  bool empty = true;
  std::int32_t reward_stage{};
  std::vector<std::int32_t> listed;
  std::vector<std::string> calls;
  std::vector<std::int32_t> droppers;
  std::vector<items::BattleLoot> pushed;
};

class FakeChestHost final : public items::ChestStateHost {
 public:
  bool ScriptFlag(std::int16_t flag) override {
    queried.push_back(flag);
    return std::find(set_flags.begin(), set_flags.end(), flag) !=
           set_flags.end();
  }
  void FinishLidRotation(std::int16_t lid_rotation_id) override {
    lids.push_back(lid_rotation_id);
  }
  void SetTilePassability(const items::TreasureChest& chest,
                          std::uint8_t passability) override {
    passabilities.emplace_back(chest.tile_x, passability);
  }

  std::vector<std::int16_t> set_flags;
  std::vector<std::int16_t> queried;
  std::vector<std::int16_t> lids;
  std::vector<std::pair<std::int16_t, std::uint8_t>> passabilities;
};

// Collects the "items" channel warnings while it lives.
struct ItemsWarnings {
  std::vector<std::string> messages;
  VS::Core::LogObserver saved =
      VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
        if (record.channel == "items") {
          CHECK(record.level == VS::Core::LogLevel::Warn);
          messages.emplace_back(record.message);
        }
      });

  ItemsWarnings() = default;
  ItemsWarnings(const ItemsWarnings&) = delete;
  ItemsWarnings& operator=(const ItemsWarnings&) = delete;
  ItemsWarnings(ItemsWarnings&&) = delete;
  ItemsWarnings& operator=(ItemsWarnings&&) = delete;
  ~ItemsWarnings() { VS::Core::SetLogObserver(std::move(saved)); }
};

// Ashley's inventory, its display order, her battle actor, the room's chests
// and the item id categories, cleared for one test and then restored.
struct LiveItemsState {
  Items_Inventory saved_inventory = Audio_Seq_unusedInventory;
  Items_InventoryIndices saved_indices = Audio_Seq_unusedInventoryIndices;
  unsigned char saved_invoked = Items_miscItemInvoked;
  Field_RoomRecord saved_room = Field_roomData;
  Battle_Actor* saved_character = Battle_characterState;
  std::array<u_int, 8> saved_upper_ids{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};

  LiveItemsState() {
    std::copy(std::begin(Items_itemCategoryUpperIds),
              std::end(Items_itemCategoryUpperIds), saved_upper_ids.begin());
    std::fill(std::begin(Items_itemCategoryUpperIds),
              std::end(Items_itemCategoryUpperIds), 0U);
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventoryIndices = {};
    Items_miscItemInvoked = 0;
    Field_roomData = {};
    actor.stats = &stats;
    Battle_characterState = &actor;
  }
  LiveItemsState(const LiveItemsState&) = delete;
  LiveItemsState& operator=(const LiveItemsState&) = delete;
  LiveItemsState(LiveItemsState&&) = delete;
  LiveItemsState& operator=(LiveItemsState&&) = delete;
  ~LiveItemsState() {
    Audio_Seq_unusedInventory = saved_inventory;
    Audio_Seq_unusedInventoryIndices = saved_indices;
    Items_miscItemInvoked = saved_invoked;
    Field_roomData = saved_room;
    Battle_characterState = saved_character;
    std::copy(saved_upper_ids.begin(), saved_upper_ids.end(),
              std::begin(Items_itemCategoryUpperIds));
  }
};

// ---------------------------------------------------------------------------
// Record fixtures.

// A part whose ratings step from `rating`: classes rating + i, affinities
// rating - i, strength rating, intelligence 2 * rating, agility -rating.
items::EquipmentPart MakePart(std::uint16_t id, std::int8_t rating) {
  items::EquipmentPart part{};
  part.id = id;
  part.sub_id = -2;
  part.weapon_model_id = -7;
  part.category = 3;
  part.strength = rating;
  part.intelligence = static_cast<std::int8_t>(2 * rating);
  part.agility = static_cast<std::int8_t>(-rating);
  part.current_dp = 300;
  part.max_dp = 400;
  part.current_pp = 50;
  part.max_pp = 90;
  part.damage_type = 1;
  part.cost_type = 2;
  part.cost = 12;
  part.material = 4;
  part.opaque14 = 9;
  part.gem_slots = 2;
  part.index = 5;
  part.range = {10, 20, 30, 0x2D};
  part.types = {1, -2, 3, -4};
  for (std::size_t i = 0; i < items::kPartClassSlots; ++i) {
    const auto step = static_cast<int>(i);
    part.classes[i] = static_cast<std::int8_t>(rating + step);
    part.affinities[i] = static_cast<std::int8_t>(rating - step);
  }
  return part;
}

items::ItemName Name(char letter) {
  items::ItemName name{};
  std::fill(name.begin(), name.end() - 1, letter);
  return name;
}

// Totals that step by `step` from `start`.
struct Ramp {
  std::int16_t start{};
  std::int16_t step{};
  [[nodiscard]] std::int16_t At(std::size_t i) const {
    return static_cast<std::int16_t>(start + step * static_cast<int>(i));
  }
};

items::ClassAffinityCurrent RampCurrent(Ramp classes, Ramp affinities) {
  items::ClassAffinityCurrent current{};
  for (auto row : {items::kEffectiveRow, items::kEquipmentRow}) {
    for (std::size_t i = 0; i < items::kClassCount; ++i) {
      current.classes[row][i] = classes.At(i);
    }
    for (std::size_t i = 0; i < items::kAffinityCount; ++i) {
      current.affinities[row][i] = affinities.At(i);
    }
  }
  return current;
}

items::ClassAffinityBaseline RampBaseline(Ramp classes, Ramp affinities) {
  items::ClassAffinityBaseline baseline{};
  for (std::size_t i = 0; i < items::kClassCount; ++i) {
    baseline.classes[i] = classes.At(i);
  }
  for (std::size_t i = 0; i < items::kAffinityCount; ++i) {
    baseline.affinities[i] = static_cast<std::uint16_t>(affinities.At(i));
  }
  return baseline;
}

items::WeaponSpawnLoadout MakeWeaponSpawn() {
  items::WeaponSpawnLoadout spawn{};
  spawn.blade = MakePart(11, 3);
  spawn.grip = MakePart(22, -1);
  spawn.grip.types = {5, 6, 7, 8};
  spawn.gems = {MakePart(31, 0), MakePart(32, 1), MakePart(33, 2)};
  spawn.material = 6;
  spawn.drop_rate = 77;
  spawn.index = 0x102;
  spawn.name = Name('W');
  return spawn;
}

items::ShieldSpawnLoadout MakeShieldSpawn() {
  items::ShieldSpawnLoadout spawn{};
  spawn.base = MakePart(41, 4);
  spawn.base.types = {-1, 2, -3, 4};
  spawn.gems = {MakePart(51, 1), items::EquipmentPart{}, MakePart(53, -2)};
  spawn.material = 3;
  spawn.drop_rate = 12;
  spawn.index = -1;
  return spawn;
}

items::InventoryBlade MakeInventoryBlade(std::uint8_t id) {
  items::InventoryBlade blade{};
  blade.id = id;
  blade.sub_id = 0xFE;
  blade.weapon_model_id = 0xF9;
  blade.category = 2;
  blade.max_dp = 410;
  blade.max_pp = 91;
  blade.current_dp = 310;
  blade.current_pp = 51;
  blade.strength = 3;
  blade.intelligence = -6;
  blade.agility = 1;
  blade.cost = 13;
  blade.type_bits = 0xE6;
  blade.opaque12 = 0x1234;
  blade.range = {1, 2, 3, 0x2C};
  blade.classes = {1, 2, 3, 4, 5, 6, 0, 0};
  blade.affinities = {-1, -2, -3, -4, -5, -6, -7, 0};
  blade.material = 0x0107;
  blade.assembled_weapon_index = 3;
  blade.index = 4;
  return blade;
}

items::InventoryArmor MakeInventoryArmor(std::uint8_t id) {
  items::InventoryArmor armor{};
  armor.id = id;
  armor.sub_id = 0x82;
  armor.weapon_model_id = 0x83;
  armor.category = 9;
  armor.max_dp = 500;
  armor.max_pp = 60;
  armor.current_dp = 450;
  armor.current_pp = 20;
  armor.gem_slots = 1;
  armor.strength = 4;
  armor.intelligence = -4;
  armor.agility = 2;
  armor.types = {2, -2, 1, -1};
  armor.classes = {3, -3, 2, -2, 1, -1, 0, 0};
  armor.affinities = {4, -4, 3, -3, 2, -2, 1, 0};
  armor.material = 0x0205;
  armor.limb = 3;
  armor.index = 6;
  return armor;
}

items::InventoryGem MakeInventoryGem(std::uint16_t id, std::uint8_t effects) {
  items::InventoryGem gem{};
  gem.id = id;
  gem.sub_id = 0x90;
  gem.gem_effects = effects;
  gem.strength = 1;
  gem.intelligence = 2;
  gem.agility = 3;
  gem.classes = {1, 2, 3, 4, 5, 6, 0, 0};
  gem.affinities = {-1, -2, -3, -4, -5, -6, -7, 0};
  gem.index = 0x107;
  return gem;
}

// One weapon (blade 1, grip 2, gems 3 and 5), one shield (gems 3 and 5 under
// the shield flag), an armour piece and a misc stack.
items::InventoryContents MakeInventory() {
  items::InventoryContents inventory{};
  inventory.blades[0] = MakeInventoryBlade(0x11);
  inventory.blades[0].category = 5;
  inventory.blades[0].range = {254, 20, 30, 0x2D};
  inventory.grips[1] = {0x1234, 0x81, 7, 3, -2, 5, -9, {9, -9, 8, -8}, 1, 2};
  inventory.gems[2] = MakeInventoryGem(0x31, items::kPpGemEffect);
  inventory.gems[4] = MakeInventoryGem(0x32, 0);
  inventory.armor[0] = MakeInventoryArmor(0x21);
  inventory.weapons[0] = {1, 1, 2, 1, {3, 0, 5, 0}, Name('W')};
  inventory.shields[0] = {
      1, 1, 0, MakeInventoryArmor(0x31), {0x83, 0, 0x85, 0}};
  inventory.misc[0] = {0x150, 7, 1};
  return inventory;
}

// The equipment of a defender: a shield and six armoured limbs.
items::DefenderEquipment MakeDefender() {
  items::DefenderEquipment defender{};
  defender.shield.base.id = 7;
  for (std::size_t i = 0; i < items::kLimbCount; ++i) {
    auto& limb = defender.limbs[i];
    limb.max_hp = 10;
    limb.name_index = static_cast<std::uint8_t>(0x10 + i);
    limb.armor.armor.id = static_cast<std::uint16_t>(20 + i);
  }
  defender.weapon_drawn = true;
  defender.shows_notices = true;
  return defender;
}

items::WeaponLoadout WeaponWithPpGem() {
  items::WeaponLoadout weapon{};
  weapon.gems[1].gem_effects = items::kPpGemEffect;
  return weapon;
}

constexpr std::array<std::uint32_t, items::kItemIdCategoryCount> kUpperIds{
    10, 20, 30, 40, 50, 60, 70, 80,
};

items::WeaponLoadout ExpectedWeapon(const items::WeaponSpawnLoadout& spawn) {
  items::WeaponLoadout weapon{};
  weapon.name = spawn.name;
  weapon.blade = spawn.blade;
  weapon.blade.material = 6;
  weapon.grip = spawn.grip;
  weapon.gems = spawn.gems;
  weapon.weapon_model_id = 249;
  weapon.damage_type_value = 6;
  weapon.risk = 12;
  weapon.recovery_delay = 9;
  weapon.drop_rate = 77;
  weapon.index = 2;
  weapon.damage_type = 1;
  weapon.action_type = 2;
  weapon.current_pp = 50;
  weapon.max_pp = 90;
  weapon.current_dp = 300;
  weapon.max_dp = 400;
  weapon.attributes = {5, 5, 10, 10, -5, 0xFFFB};
  weapon.range = {10, 20, 30, 0x2D};
  weapon.class_affinity = RampCurrent({6, 4}, {6, -4});
  weapon.baseline = RampBaseline({6, 4}, {6, -4});
  return weapon;
}

struct DamageTypeCase {
  std::uint8_t damage_type{};
  std::uint8_t value{};
  ItemsStatus status{};
};

void CheckDamageTypeValues(std::span<const DamageTypeCase> cases) {
  for (const auto& expected : cases) {
    auto spawn = MakeWeaponSpawn();
    spawn.blade.damage_type = expected.damage_type;
    items::WeaponLoadout weapon{};
    weapon.damage_type_value = 0x5A;
    INFO("damage type " << int{expected.damage_type});
    CHECK(items::BuildWeaponStatsFromParts(weapon, spawn) == expected.status);
    CHECK(weapon.damage_type_value == expected.value);
  }
}

// The record ReadInventoryArmorStats makes of MakeInventoryArmor(id).
items::EquipmentPart ReadArmorPart(std::uint8_t id) {
  items::EquipmentPart part{};
  part.id = id;
  part.sub_id = -126;
  part.weapon_model_id = -125;
  part.category = 9;
  part.max_dp = 500;
  part.max_pp = 60;
  part.current_dp = 450;
  part.current_pp = 20;
  part.gem_slots = 1;
  part.strength = 4;
  part.intelligence = -4;
  part.agility = 2;
  part.index = 6;
  part.types = {2, -2, 1, -1};
  part.classes = {3, -3, 2, -2, 1, -1, 0, 0};
  part.affinities = {4, -4, 3, -3, 2, -2, 1, 0};
  part.material = 5;
  return part;
}

items::EquipmentPart ReadGemPart(const items::InventoryGem& gem) {
  items::EquipmentPart part{};
  items::ReadInventoryGemStats(part, gem);
  return part;
}

// The weapon MakeInventory's weapon 1 assembles.
items::WeaponLoadout ExpectedInventoryWeapon(
    const items::InventoryContents& inventory) {
  items::WeaponSpawnLoadout parts{};
  items::ReadInventoryBladeStats(parts.blade, inventory.blades[0]);
  items::ReadInventoryGripStats(parts.grip, inventory.grips[1]);
  parts.gems[0] = ReadGemPart(inventory.gems[2]);
  parts.gems[2] = ReadGemPart(inventory.gems[4]);
  parts.material = 7;
  parts.index = 1;
  parts.name = Name('W');
  items::WeaponLoadout weapon{};
  CHECK(items::BuildWeaponStatsFromParts(weapon, parts) == ItemsStatus::kOk);
  return weapon;
}

items::ShieldLoadout ExpectedInventoryShield(
    const items::InventoryContents& inventory) {
  items::ShieldSpawnLoadout parts{};
  parts.base = ReadArmorPart(0x31);
  parts.gems[0] = ReadGemPart(inventory.gems[2]);
  parts.gems[2] = ReadGemPart(inventory.gems[4]);
  parts.material = 5;
  parts.index = 1;
  items::ShieldLoadout shield{};
  items::BuildShieldStatsFromParts(shield, parts);
  return shield;
}

items::ArmorLoadout ExpectedInventoryArmor(std::uint8_t id) {
  items::ArmorLoadout armor{};
  items::BuildArmorStatsFromPart(armor, {ReadArmorPart(id), 5, 0, 0, 6});
  return armor;
}

items::AccessorySpawnLoadout ExpectedInventoryAccessory(std::uint8_t id) {
  items::AccessorySpawnLoadout accessory{};
  items::BuildAccessoryStatsFromPart(accessory, {ReadArmorPart(id), 0, 6});
  return accessory;
}

template <typename Loadout>
Loadout BuiltFromNothing() {
  Loadout loadout{};
  if constexpr (std::is_same_v<Loadout, items::WeaponLoadout>) {
    CHECK(items::BuildWeaponStatsFromParts(loadout, {}) == ItemsStatus::kOk);
  } else {
    items::BuildShieldStatsFromParts(loadout, {});
  }
  return loadout;
}
}  // namespace

// ---------------------------------------------------------------------------
// Building loadouts from parts.

TEST_CASE("A weapon loadout totals its blade and gems",
          "[items-slice][items]") {
  const auto spawn = MakeWeaponSpawn();
  items::WeaponLoadout weapon{};
  weapon.affinity_modifiers.fill(77);
  CHECK(items::BuildWeaponStatsFromParts(weapon, spawn) == ItemsStatus::kOk);
  auto expected = ExpectedWeapon(spawn);
  expected.affinity_modifiers[7] = 77;
  CHECK(weapon == expected);
  CHECK(weapon.attributes.current_strength == 5);
  CHECK(weapon.class_affinity.classes[items::kEffectiveRow][5] == 26);
}

TEST_CASE("A weapon's damage type indexes the grip's type tables in order",
          "[items-slice][items]") {
  constexpr std::array<DamageTypeCase, 7> kCases{{
      {3, 8, ItemsStatus::kOk},
      {4, 255, ItemsStatus::kOk},
      {11, 6, ItemsStatus::kOk},
      {12, 255, ItemsStatus::kOk},
      {19, 248, ItemsStatus::kOk},
      {20, 0, ItemsStatus::kDamageTypeOutOfRange},
      {255, 0, ItemsStatus::kDamageTypeOutOfRange},
  }};
  CheckDamageTypeValues(kCases);
}

TEST_CASE("A shield loadout totals its base and gems", "[items-slice][items]") {
  const auto spawn = MakeShieldSpawn();
  items::ShieldLoadout shield{};
  shield.reserved0.fill(-9);
  items::BuildShieldStatsFromParts(shield, spawn);
  items::ShieldLoadout expected{};
  expected.reserved0.fill(-9);
  expected.base = spawn.base;
  expected.base.material = 3;
  expected.gems = spawn.gems;
  expected.weapon_model_id = 249;
  expected.drop_rate = 12;
  expected.index = 255;
  expected.current_pp = 50;
  expected.max_pp = 90;
  expected.current_dp = 300;
  expected.max_dp = 400;
  expected.attributes = {3, 3, 6, 6, -3, 0xFFFD};
  expected.types = {255, 2, 253, 4};
  expected.class_affinity = RampCurrent({3, 3}, {3, -3});
  expected.baseline = RampBaseline({3, 3}, {3, -3});
  CHECK(shield == expected);
}

TEST_CASE("An accessory loadout widens its part's ratings",
          "[items-slice][items]") {
  const auto part = MakePart(61, 2);
  items::AccessorySpawnLoadout accessory{};
  accessory.classes.fill(0x7777);
  accessory.affinities.fill(0x7777);
  items::BuildAccessoryStatsFromPart(accessory, {part, 33, 4, 0, 0});
  items::AccessorySpawnLoadout expected{};
  expected.accessory = part;
  expected.current_strength = 2;
  expected.current_intelligence = 4;
  expected.current_agility = -2;
  expected.drop_rate = 33;
  expected.index = 4;
  expected.types = {1, 0xFFFE, 3, 0xFFFC};
  expected.classes = {2, 3, 4, 5, 6, 7, 0x7777, 0x7777};
  expected.affinities = {2, 1, 0, 0xFFFF, 0xFFFE, 0xFFFD, 0xFFFC, 0x7777};
  CHECK(accessory == expected);
}

TEST_CASE("An armour loadout copies its part's ratings",
          "[items-slice][items]") {
  const auto part = MakePart(62, 2);
  items::ArmorLoadout armor{};
  armor.affinity_modifiers.fill(5);
  armor.reserved9b = 0x9B;
  items::BuildArmorStatsFromPart(armor, {part, 7, 40, 0x33, 9});
  items::ArmorLoadout expected{};
  expected.armor = part;
  expected.armor.material = 7;
  expected.current_dp = 300;
  expected.max_dp = 400;
  expected.attributes = {2, 2, 4, 4, -2, 0xFFFE};
  expected.types = {1, 0xFFFE, 3, 0xFFFC};
  expected.class_affinity = RampCurrent({2, 1}, {2, -1});
  expected.affinity_modifiers[7] = 5;
  expected.drop_rate = 40;
  expected.opaque99 = 0x33;
  expected.index = 9;
  expected.reserved9b = 0x9B;
  expected.baseline = RampBaseline({2, 1}, {2, -1});
  CHECK(armor == expected);
}

// ---------------------------------------------------------------------------
// Reading and writing inventory records.

TEST_CASE("Inventory blades and grips read into part records",
          "[items-slice][items]") {
  items::EquipmentPart blade{};
  blade.types = {9, 9, 9, 9};
  blade.gem_slots = 7;
  blade.classes[7] = 44;
  items::ReadInventoryBladeStats(blade, MakeInventoryBlade(0x42));
  items::EquipmentPart expected{};
  expected.id = 0x42;
  expected.sub_id = -2;
  expected.weapon_model_id = -7;
  expected.category = 2;
  expected.max_dp = 410;
  expected.max_pp = 91;
  expected.current_dp = 310;
  expected.current_pp = 51;
  expected.strength = 3;
  expected.intelligence = -6;
  expected.agility = 1;
  expected.cost = 13;
  expected.damage_type = 2;
  expected.cost_type = 1;
  expected.opaque14 = 0x34;
  expected.range = {1, 2, 3, 0x2C};
  expected.index = 4;
  expected.classes = {1, 2, 3, 4, 5, 6, 0, 44};
  expected.affinities = {-1, -2, -3, -4, -5, -6, -7, 0};
  expected.material = 7;
  expected.types = {9, 9, 9, 9};
  expected.gem_slots = 7;
  CHECK(blade == expected);

  items::EquipmentPart grip{};
  items::ReadInventoryGripStats(
      grip, {0x1234, 0x81, 7, 3, -2, 5, -9, {9, -9, 8, -8}, 1, 0x105});
  items::EquipmentPart expected_grip{};
  expected_grip.id = 0x1234;
  expected_grip.sub_id = -127;
  expected_grip.category = 7;
  expected_grip.gem_slots = 3;
  expected_grip.strength = -2;
  expected_grip.intelligence = 5;
  expected_grip.agility = -9;
  expected_grip.index = 5;
  expected_grip.types = {9, -9, 8, -8};
  CHECK(grip == expected_grip);
}

TEST_CASE("Inventory armour and gems read into part records",
          "[items-slice][items]") {
  items::EquipmentPart armor{};
  items::ReadInventoryArmorStats(armor, MakeInventoryArmor(0x21));
  CHECK(armor == ReadArmorPart(0x21));

  items::EquipmentPart gem{};
  gem.material = 8;
  items::ReadInventoryGemStats(gem, MakeInventoryGem(0x1FF, 9));
  items::EquipmentPart expected{};
  expected.id = 0x1FF;
  expected.sub_id = -112;
  expected.gem_effects = 9;
  expected.strength = 1;
  expected.intelligence = 2;
  expected.agility = 3;
  expected.index = 7;
  expected.classes = {1, 2, 3, 4, 5, 6, 0, 0};
  expected.affinities = {-1, -2, -3, -4, -5, -6, -7, 0};
  expected.material = 8;
  CHECK(gem == expected);
}

TEST_CASE("Part records write back into inventory blades and grips",
          "[items-slice][items]") {
  items::InventoryBlade blade{};
  blade.type_bits = 0xE3;
  blade.reserved11 = 0x55;
  blade.assembled_weapon_index = 3;
  blade.index = 4;
  auto part = MakePart(0x1AB, 5);
  part.damage_type = 5;
  part.cost_type = 0xE;
  items::WriteInventoryBladeStats(blade, part);
  const items::InventoryBlade expected{0xAB,
                                       0xFE,
                                       0xF9,
                                       3,
                                       400,
                                       90,
                                       300,
                                       50,
                                       5,
                                       10,
                                       -5,
                                       12,
                                       0xF9,
                                       0x55,
                                       9,
                                       {10, 20, 30, 0x2D},
                                       {5, 6, 7, 8, 9, 10, 0, 0},
                                       {5, 4, 3, 2, 1, 0, -1, 0},
                                       4,
                                       3,
                                       4};
  CHECK(blade == expected);

  items::InventoryGrip grip{};
  grip.assembled_weapon_index = 7;
  grip.index = 8;
  items::WriteInventoryGripStats(grip, MakePart(0x1234, 1));
  CHECK(grip == items::InventoryGrip{
                    0x1234, 0xFE, 3, 2, 1, 2, -1, {1, -2, 3, -4}, 7, 8});
}

TEST_CASE("Part records write back into inventory gems and armour",
          "[items-slice][items]") {
  items::InventoryGem gem{};
  gem.reserved3 = 0x77;
  gem.set_item_index = 5;
  gem.index = 6;
  auto gem_part = MakePart(0x222, 3);
  gem_part.gem_effects = 9;
  items::WriteInventoryGemStats(gem, gem_part);
  CHECK(gem == items::InventoryGem{0x222,
                                   0xFE,
                                   0x77,
                                   9,
                                   3,
                                   6,
                                   -3,
                                   {3, 4, 5, 6, 7, 8, 0, 0},
                                   {3, 2, 1, 0, -1, -2, -3, 0},
                                   5,
                                   6});

  items::InventoryArmor armor{};
  armor.limb = 3;
  armor.index = 6;
  items::WriteInventoryArmorStats(armor, MakePart(0x1C5, -1));
  CHECK(armor == items::InventoryArmor{0xC5,
                                       0xFE,
                                       0xF9,
                                       3,
                                       400,
                                       90,
                                       300,
                                       50,
                                       2,
                                       -1,
                                       -2,
                                       1,
                                       {1, -2, 3, -4},
                                       {-1, 0, 1, 2, 3, 4, 0, 0},
                                       {-1, -2, -3, -4, -5, -6, -7, 0},
                                       4,
                                       3,
                                       6});
}

// ---------------------------------------------------------------------------
// Loading inventory equipment.

TEST_CASE("An inventory weapon loads its referenced parts",
          "[items-slice][items]") {
  const auto inventory = MakeInventory();
  items::WeaponLoadout weapon{};
  CHECK(items::LoadInventoryWeaponStats(weapon, &inventory.weapons[0],
                                        inventory) == ItemsStatus::kOk);
  CHECK(weapon == ExpectedInventoryWeapon(inventory));
  CHECK(weapon.blade.material == 7);
  CHECK(weapon.damage_type_value == 8);

  items::WeaponLoadout unarmed{};
  unarmed.risk = 9;
  CHECK(items::LoadInventoryWeaponStats(unarmed, nullptr, inventory) ==
        ItemsStatus::kOk);
  CHECK(unarmed == BuiltFromNothing<items::WeaponLoadout>());
}

TEST_CASE("An inventory weapon refuses part references past their tables",
          "[items-slice][items]") {
  const auto inventory = MakeInventory();
  using Refs = std::array<std::uint8_t, 4>;
  for (const auto& [blade, grip, gems] :
       {std::tuple{17, 0, Refs{}}, std::tuple{0, 17, Refs{}},
        std::tuple{0, 0, Refs{49, 0, 0, 0}},
        std::tuple{0, 0, Refs{0, 0, 49, 0}}}) {
    const items::InventoryWeapon source{1,
                                        static_cast<std::uint8_t>(blade),
                                        static_cast<std::uint8_t>(grip),
                                        0,
                                        gems,
                                        {}};
    items::WeaponLoadout weapon{};
    CHECK(items::LoadInventoryWeaponStats(weapon, &source, inventory) ==
          ItemsStatus::kPartReferenceOutOfRange);
    CHECK(weapon.index == 1);
  }
  const items::InventoryWeapon bare{};
  items::WeaponLoadout weapon{};
  CHECK(items::LoadInventoryWeaponStats(weapon, &bare, inventory) ==
        ItemsStatus::kOk);
}

TEST_CASE("An inventory shield masks the shield flag off its gem references",
          "[items-slice][items]") {
  const auto inventory = MakeInventory();
  items::ShieldLoadout shield{};
  CHECK(items::LoadInventoryShieldStats(shield, &inventory.shields[0],
                                        inventory) == ItemsStatus::kOk);
  CHECK(shield == ExpectedInventoryShield(inventory));

  for (const std::uint8_t reference :
       {std::uint8_t{0x80}, std::uint8_t{0xB1}}) {
    auto source = inventory.shields[0];
    source.gems = {reference, 0, 0, 0};
    items::ShieldLoadout refused{};
    CHECK(items::LoadInventoryShieldStats(refused, &source, inventory) ==
          ItemsStatus::kPartReferenceOutOfRange);
    CHECK(refused.base.id == 0x31);
  }
  items::ShieldLoadout none{};
  CHECK(items::LoadInventoryShieldStats(none, nullptr, inventory) ==
        ItemsStatus::kOk);
  CHECK(none == BuiltFromNothing<items::ShieldLoadout>());
}

TEST_CASE("Inventory armour and accessories load their records",
          "[items-slice][items]") {
  const auto record = MakeInventoryArmor(0x21);
  items::ArmorLoadout armor{};
  items::LoadInventoryArmorStats(armor, &record);
  CHECK(armor == ExpectedInventoryArmor(0x21));
  CHECK(armor.armor.material == 5);
  items::ArmorLoadout no_armor{};
  no_armor.armor.id = 9;
  items::LoadInventoryArmorStats(no_armor, nullptr);
  CHECK(no_armor == items::ArmorLoadout{});

  items::AccessorySpawnLoadout accessory{};
  items::LoadInventoryAccessoryStats(accessory, &record);
  CHECK(accessory == ExpectedInventoryAccessory(0x21));
  items::AccessorySpawnLoadout no_accessory{};
  no_accessory.index = 3;
  items::LoadInventoryAccessoryStats(no_accessory, nullptr);
  CHECK(no_accessory == items::AccessorySpawnLoadout{});
}

// ---------------------------------------------------------------------------
// Equipping Ashley.

namespace {
std::array<int, 5> ReachOf(const items::AshleyWeaponState& ashley) {
  return {ashley.current_range, ashley.current_range_y, ashley.current_range_z,
          ashley.current_attack_shape, ashley.current_attack_shape_angle};
}
}  // namespace

TEST_CASE("Equipping Ashley's weapon sets her reach and attack shape",
          "[items-slice][items]") {
  const auto inventory = MakeInventory();
  items::AshleyWeaponState ashley{};
  ashley.reach = 3;
  ashley.equipped_weapon_category = 7;
  FakeStatusHost host(0);
  CHECK(items::EquipAshleyWeapon(ashley, &inventory.weapons[0], inventory,
                                 host) == ItemsStatus::kOk);
  CHECK(ashley.weapon == ExpectedInventoryWeapon(inventory));
  CHECK(ashley.equipped_weapon_category == 5);
  CHECK(ReachOf(ashley) == std::array{1, 23, 33, 1, 5});
  CHECK(host.reads == 2);
  CHECK(host.labels.empty());
}

TEST_CASE("Unequipping Ashley's weapon leaves her unarmed",
          "[items-slice][items]") {
  const auto inventory = MakeInventory();
  items::AshleyWeaponState ashley{};
  ashley.equipped_weapon_category = 7;
  FakeStatusHost host(0);
  CHECK(items::EquipAshleyWeapon(ashley, nullptr, inventory, host) ==
        ItemsStatus::kOk);
  CHECK(ashley.equipped_weapon_category == items::kUnarmedWeaponCategory);
  CHECK(ashley.weapon == BuiltFromNothing<items::WeaponLoadout>());

  // A weapon without a blade has no category to take.
  const items::InventoryWeapon bladeless{1, 0, 2, 0, {}, {}};
  ashley.equipped_weapon_category = 7;
  CHECK(items::EquipAshleyWeapon(ashley, &bladeless, inventory, host) ==
        ItemsStatus::kPartReferenceOutOfRange);
  CHECK(ashley.equipped_weapon_category == 7);
  CHECK(ashley.weapon.index == 1);
}

TEST_CASE("Equipping ends boosts and enchantments in the original's order",
          "[items-slice][items]") {
  const auto inventory = MakeInventory();
  items::AshleyWeaponState ashley{};
  FakeStatusHost weapon_host(0x200000);
  weapon_host.added_on_label = 0x400000;
  CHECK(items::EquipAshleyWeapon(ashley, &inventory.weapons[0], inventory,
                                 weapon_host) == ItemsStatus::kOk);
  CHECK(weapon_host.labels == std::vector<std::int32_t>{0x200000});
  CHECK(weapon_host.removed == std::vector<std::int32_t>{0x600000});
  CHECK(weapon_host.reads == 3);

  items::ShieldLoadout shield{};
  FakeStatusHost shield_host(0x02300000);
  CHECK(items::EquipAshleyShield(shield, &inventory.shields[0], inventory,
                                 shield_host) == ItemsStatus::kOk);
  CHECK(shield == ExpectedInventoryShield(inventory));
  CHECK(shield_host.labels == std::vector<std::int32_t>{0x100000, 0x02000000});
  CHECK(shield_host.removed == std::vector<std::int32_t>{0x100000, 0x02000000});
  CHECK(shield_host.statuses == 0x200000);
}

TEST_CASE("Equipping armour refuses a body part outside the limbs",
          "[items-slice][items]") {
  const auto record = MakeInventoryArmor(0x21);
  std::array<items::ArmorLoadout, items::kLimbCount> limbs{};
  for (const auto body_part : {-1, 6}) {
    FakeStatusHost host(0);
    CHECK(items::EquipAshleyArmor(limbs, body_part, &record, host) ==
          ItemsStatus::kBodyPartOutOfRange);
    CHECK(host.reads == 0);
  }
  FakeStatusHost host(0);
  CHECK(items::EquipAshleyArmor(limbs, 2, &record, host) == ItemsStatus::kOk);
  CHECK(limbs[2] == ExpectedInventoryArmor(0x21));
  CHECK(limbs[1] == items::ArmorLoadout{});
  CHECK(host.reads == 2);

  items::AccessorySpawnLoadout accessory{};
  FakeStatusHost accessory_host(0);
  items::EquipAshleyAccessory(accessory, &record, accessory_host);
  CHECK(accessory == ExpectedInventoryAccessory(0x21));
  CHECK(accessory_host.reads == 2);
}

// ---------------------------------------------------------------------------
// Storing loadouts back into the inventory.

namespace {
items::WeaponLoadout MakeStoredWeapon() {
  items::WeaponLoadout weapon{};
  weapon.blade = MakePart(0x11, 2);
  weapon.grip = MakePart(0x22, 1);
  weapon.gems[0] = MakePart(0x33, 1);
  weapon.current_dp = 123;
  weapon.current_pp = 45;
  weapon.name = Name('S');
  return weapon;
}

items::ShieldLoadout MakeStoredShield() {
  items::ShieldLoadout shield{};
  shield.base = MakePart(0x44, 1);
  shield.gems[0] = MakePart(0x55, 1);
  shield.current_dp = 77;
  shield.current_pp = 8;
  return shield;
}

template <typename Record>
Record Written(Record record, const items::EquipmentPart& part) {
  if constexpr (std::is_same_v<Record, items::InventoryBlade>) {
    items::WriteInventoryBladeStats(record, part);
  } else if constexpr (std::is_same_v<Record, items::InventoryGrip>) {
    items::WriteInventoryGripStats(record, part);
  } else if constexpr (std::is_same_v<Record, items::InventoryGem>) {
    items::WriteInventoryGemStats(record, part);
  } else {
    items::WriteInventoryArmorStats(record, part);
  }
  return record;
}
}  // namespace

TEST_CASE("A weapon loadout stores its parts and name into the inventory",
          "[items-slice][items]") {
  items::InventoryContents inventory{};
  items::InventoryWeapon target{1, 1, 1, 1, {1, 0, 0, 0}, Name('W')};
  auto weapon = MakeStoredWeapon();
  CHECK(items::StoreInventoryWeaponStats(target, weapon, inventory) ==
        ItemsStatus::kOk);
  CHECK(weapon.blade.current_dp == 123);
  CHECK(weapon.blade.current_pp == 45);
  CHECK(inventory.blades[0] == Written(items::InventoryBlade{}, weapon.blade));
  CHECK(inventory.grips[0] == Written(items::InventoryGrip{}, weapon.grip));
  CHECK(inventory.gems[0] == Written(items::InventoryGem{}, weapon.gems[0]));
  CHECK(target.name == Name('S'));
}

TEST_CASE("A weapon loadout refuses a gem reference past the gem table",
          "[items-slice][items]") {
  items::InventoryContents inventory{};
  items::InventoryWeapon target{1, 1, 1, 1, {49, 0, 0, 0}, Name('W')};
  auto weapon = MakeStoredWeapon();
  CHECK(items::StoreInventoryWeaponStats(target, weapon, inventory) ==
        ItemsStatus::kPartReferenceOutOfRange);
  CHECK(inventory.blades[0].id == 0x11);
  CHECK(target.name == Name('S'));

  // A weapon without a blade stores nothing.
  items::InventoryContents untouched{};
  items::InventoryWeapon kept{1, 1, 1, 1, {1, 0, 0, 0}, Name('W')};
  weapon.blade.id = 0;
  CHECK(items::StoreInventoryWeaponStats(kept, weapon, untouched) ==
        ItemsStatus::kOk);
  CHECK(untouched == items::InventoryContents{});
  CHECK(kept.name == Name('W'));
}

TEST_CASE("A shield loadout stores its gems through raw references",
          "[items-slice][items]") {
  items::InventoryContents inventory{};
  items::InventoryShield target{1, 1, 0, {}, {0x83, 0, 0, 0}};
  auto shield = MakeStoredShield();
  CHECK(items::StoreInventoryShieldStats(target, shield, inventory) ==
        ItemsStatus::kPartReferenceOutOfRange);
  CHECK(target.base == Written(items::InventoryArmor{}, shield.base));
  CHECK(target.base.current_dp == 77);

  target.gems = {3, 0, 0, 0};
  CHECK(items::StoreInventoryShieldStats(target, shield, inventory) ==
        ItemsStatus::kOk);
  CHECK(inventory.gems[2] == Written(items::InventoryGem{}, shield.gems[0]));

  const auto stored = target;
  shield.base.id = 0;
  shield.current_dp = 1;
  CHECK(items::StoreInventoryShieldStats(target, shield, inventory) ==
        ItemsStatus::kOk);
  CHECK(target == stored);
}

TEST_CASE("Armour and accessory loadouts store into their inventory records",
          "[items-slice][items]") {
  auto target = MakeInventoryArmor(0x21);
  items::ArmorLoadout armor{};
  armor.armor = MakePart(0x66, 1);
  armor.current_dp = 77;
  items::StoreInventoryArmorStats(target, armor);
  CHECK(armor.armor.current_dp == 77);
  CHECK(target == Written(MakeInventoryArmor(0x21), armor.armor));
  CHECK(target.limb == 3);
  armor.armor.id = 0;
  auto kept = MakeInventoryArmor(0x21);
  items::StoreInventoryArmorStats(kept, armor);
  CHECK(kept == MakeInventoryArmor(0x21));

  items::AccessorySpawnLoadout accessory{};
  accessory.accessory = MakePart(0x67, 2);
  auto ring = MakeInventoryArmor(0x21);
  items::StoreInventoryAccessoryStats(ring, accessory);
  CHECK(ring == Written(MakeInventoryArmor(0x21), accessory.accessory));
  accessory.accessory.id = 0;
  auto kept_ring = MakeInventoryArmor(0x21);
  items::StoreInventoryAccessoryStats(kept_ring, accessory);
  CHECK(kept_ring == MakeInventoryArmor(0x21));
}

// ---------------------------------------------------------------------------
// Class and affinity growth.

namespace {
struct ThresholdCase {
  std::int8_t rating{};
  std::int32_t threshold{};
};

// A rating grows on a draw up to its threshold and not above it.
void CheckGrowthThresholds(std::span<const ThresholdCase> cases) {
  for (const auto& [rating, threshold] : cases) {
    for (const auto draw : {threshold, threshold + 1}) {
      INFO("rating " << int{rating} << " draw " << draw);
      items::EquipmentPart part{};
      part.classes[0] = rating;
      ScriptedHost host({draw});
      CHECK(items::GrowEquipmentClass(part, 0, {}, host) == ItemsStatus::kOk);
      CHECK(part.classes[0] == (draw == threshold ? rating + 1 : rating));
      CHECK(host.bounds.front() == kGrowthBound);
    }
  }
}

// After class 0 grows, class 1 decays on a draw up to its threshold.
void CheckDecayThresholds(std::span<const ThresholdCase> cases) {
  for (const auto& [rating, threshold] : cases) {
    for (const auto draw : {threshold, threshold + 1}) {
      INFO("rating " << int{rating} << " draw " << draw);
      items::EquipmentPart part{};
      part.classes[1] = rating;
      ScriptedHost host({0, draw, kNoDraw});
      CHECK(items::GrowEquipmentClass(part, 0, {}, host) == ItemsStatus::kOk);
      CHECK(part.classes[0] == 1);
      CHECK(part.classes[1] == (draw == threshold ? rating - 1 : rating));
    }
  }
}

struct AffinityPair {
  std::size_t grown{};
  std::size_t counterpart{};
};

// Affinities after `pair.grown` grows from zero and every decay succeeds.
items::PartAffinities ExpectedAffinityGrowth(AffinityPair pair) {
  items::PartAffinities expected{};
  expected[pair.grown] = 1;
  if (pair.grown == 0) {
    for (std::size_t i = 1; i < items::kAffinityCount; ++i) expected[i] = -1;
  } else {
    expected[0] = -1;
    expected[pair.counterpart] = -1;
  }
  return expected;
}

using GrowAffinityFn = ItemsStatus (*)(const items::ItemAction&,
                                       items::EquipmentPart&, std::int32_t,
                                       items::GrowthNotices,
                                       items::EquipmentGrowthHost&);

void CheckAffinityPairs(GrowAffinityFn grow,
                        std::span<const AffinityPair> pairs) {
  for (std::size_t affinity = 0; affinity < pairs.size(); ++affinity) {
    INFO("affinity " << affinity);
    items::EquipmentPart part{};
    ScriptedHost host(std::vector<std::int32_t>(7, 0));
    CHECK(grow({}, part, static_cast<std::int32_t>(affinity), {0x40, true},
               host) == ItemsStatus::kOk);
    CHECK(part.affinities == ExpectedAffinityGrowth(pairs[affinity]));
    REQUIRE_FALSE(host.notices.empty());
    const auto stat = static_cast<std::int32_t>(6 + pairs[affinity].grown);
    CHECK(host.notices.front() ==
          Notice{0x40, stat, items::StatChange::kIncrease});
  }
}
}  // namespace

TEST_CASE("A rating grows below 70 - (rating + 100) / 4",
          "[items-slice][items]") {
  constexpr std::array<ThresholdCase, 4> kCases{
      {{0, 45}, {99, 21}, {-100, 70}, {-128, 77}}};
  CheckGrowthThresholds(kCases);
}

TEST_CASE("A rating decays below (rating + 100) / 4 + 20",
          "[items-slice][items]") {
  constexpr std::array<ThresholdCase, 4> kCases{
      {{0, 45}, {-99, 20}, {99, 69}, {127, 76}}};
  CheckDecayThresholds(kCases);
}

TEST_CASE("Ratings at the limits draw nothing", "[items-slice][items]") {
  for (const std::int8_t rating : {std::int8_t{100}, std::int8_t{127}}) {
    items::EquipmentPart part{};
    part.classes[0] = rating;
    ScriptedHost host({0});
    CHECK(items::GrowEquipmentClass(part, 0, {}, host) == ItemsStatus::kOk);
    CHECK(part.classes[0] == rating);
    CHECK(host.bounds.empty());
  }
  items::EquipmentPart part{};
  part.classes[1] = -100;
  part.classes[2] = -128;
  ScriptedHost host({0});
  CHECK(items::GrowEquipmentClass(part, 0, {}, host) == ItemsStatus::kOk);
  CHECK(part.classes == items::PartClasses{1, -100, -128, 0, 0, 0, 0, 0});
  CHECK(host.bounds == std::vector<std::int32_t>{kGrowthBound});
}

TEST_CASE("A class growth decays the next two classes, wrapping at evil",
          "[items-slice][items]") {
  items::EquipmentPart part{};
  ScriptedHost host({0, 0, 0});
  CHECK(items::GrowEquipmentClass(part, 5, {0x30, true}, host) ==
        ItemsStatus::kOk);
  CHECK(part.classes == items::PartClasses{-1, -1, 0, 0, 0, 1, 0, 0});
  CHECK(host.notices ==
        std::vector<Notice>{{0x30, 5, items::StatChange::kIncrease},
                            {0x30, 0, items::StatChange::kDecrease},
                            {0x30, 1, items::StatChange::kDecrease}});
  items::EquipmentPart hidden{};
  ScriptedHost hidden_host({0, 0, 0});
  CHECK(items::GrowEquipmentClass(hidden, 5, {0x30, false}, hidden_host) ==
        ItemsStatus::kOk);
  CHECK(hidden.classes == part.classes);
  CHECK(hidden_host.notices.empty());
  for (const auto class_index : {-1, 6}) {
    ScriptedHost refused({0});
    CHECK(items::GrowEquipmentClass(part, class_index, {}, refused) ==
          ItemsStatus::kClassOutOfRange);
    CHECK(refused.bounds.empty());
  }
}

TEST_CASE("Armour affinities grow against the attack's affinity",
          "[items-slice][items]") {
  constexpr std::array<AffinityPair, 7> kOpposing{
      {{0, 0}, {3, 1}, {4, 2}, {1, 3}, {2, 4}, {6, 5}, {5, 6}}};
  CheckAffinityPairs(&items::GrowOpposingEquipmentAffinity, kOpposing);
}

TEST_CASE("Weapon affinities grow with the matching affinity",
          "[items-slice][items]") {
  constexpr std::array<AffinityPair, 7> kMatching{
      {{0, 0}, {1, 3}, {2, 4}, {3, 1}, {4, 2}, {5, 6}, {6, 5}}};
  CheckAffinityPairs(&items::GrowMatchingEquipmentAffinity, kMatching);
}

TEST_CASE("An action's first-hit affinity overrides the given affinity",
          "[items-slice][items]") {
  items::ItemAction action{};
  action.first_hit_affinity = 3;
  items::EquipmentPart part{};
  ScriptedHost host(std::vector<std::int32_t>(3, 0));
  CHECK(items::GrowMatchingEquipmentAffinity(action, part, 6, {}, host) ==
        ItemsStatus::kOk);
  CHECK(part.affinities == ExpectedAffinityGrowth({2, 4}));

  action.first_hit_affinity = 8;
  ScriptedHost refused_host({0});
  CHECK(
      items::GrowMatchingEquipmentAffinity(action, part, 0, {}, refused_host) ==
      ItemsStatus::kAffinityOutOfRange);
  for (const auto affinity : {-1, 7}) {
    CHECK(items::GrowOpposingEquipmentAffinity({}, part, affinity, {},
                                               refused_host) ==
          ItemsStatus::kAffinityOutOfRange);
  }
  CHECK(refused_host.bounds.empty());

  items::EquipmentPart unchanged{};
  ScriptedHost failed({46});
  CHECK(items::GrowOpposingEquipmentAffinity({}, unchanged, 0, {}, failed) ==
        ItemsStatus::kOk);
  CHECK(unchanged == items::EquipmentPart{});
  CHECK(failed.bounds == std::vector<std::int32_t>{kGrowthBound});
}

// ---------------------------------------------------------------------------
// Growth of the struck and striking equipment.

namespace {
struct ReachCase {
  std::uint8_t category{};
  std::vector<std::int32_t> bounds;
};

// With the shield raised and every limb armoured, an action category's reach
// shows in the candidate count the selection draws over.
void CheckStrikeReach(const std::vector<ReachCase>& cases) {
  for (const auto& [category, bounds] : cases) {
    INFO("category " << int{category});
    auto defender = MakeDefender();
    items::ItemAction action{};
    action.category = category;
    ScriptedHost host;
    CHECK(items::GrowDefenderEquipmentRatings(action, defender, {}, 2, host) ==
          ItemsStatus::kOk);
    CHECK(host.bounds == bounds);
  }
}

items::ItemAction ActionOfCategory(std::uint8_t category) {
  items::ItemAction action{};
  action.category = category;
  return action;
}

// Equipment rows whose classes and affinities hold `class_affinity`.
items::ClassAffinityCurrent RowsWith(std::size_t class_index,
                                     std::int16_t class_total,
                                     std::size_t affinity_index,
                                     std::int16_t affinity_total,
                                     std::int16_t effective_affinity) {
  items::ClassAffinityCurrent rows{};
  for (const auto row : {items::kEffectiveRow, items::kEquipmentRow}) {
    rows.classes[row][class_index] = class_total;
    rows.affinities[row][affinity_index] = affinity_total;
  }
  rows.affinities[items::kEffectiveRow][affinity_index] = effective_affinity;
  return rows;
}
}  // namespace

TEST_CASE("An action category decides which equipment a hit can strike",
          "[items-slice][items]") {
  const std::vector<std::int32_t> none;
  const std::vector<std::int32_t> shield_or_any_limb{7};
  const std::vector<std::int32_t> shield_or_hit_limb{2};
  const std::vector<std::int32_t> hit_limb{1};
  CheckStrikeReach({
      {0, none},
      {1, shield_or_any_limb},
      {2, shield_or_any_limb},
      {3, shield_or_any_limb},
      {4, shield_or_any_limb},
      {5, shield_or_hit_limb},
      {6, hit_limb},
      {7, shield_or_hit_limb},
      {8, hit_limb},
      {9, none},
      {10, none},
      {11, hit_limb},
      {12, hit_limb},
      {13, none},
      {14, shield_or_hit_limb},
      {15, none},
      {16, none},
      {255, none},
  });
}

TEST_CASE("A defender's growth refuses unknown actors and limbs",
          "[items-slice][items]") {
  auto defender = MakeDefender();
  defender.weapon_drawn.reset();
  ScriptedHost refused;
  CHECK(items::GrowDefenderEquipmentRatings(ActionOfCategory(1), defender,
                                            {1, 1}, 2, refused) ==
        ItemsStatus::kActorOutOfRange);
  CHECK(refused.bounds.empty());

  // An action that strikes only the hit limb needs no weapon-drawn state.
  ScriptedHost limb_only;
  CHECK(items::GrowDefenderEquipmentRatings(ActionOfCategory(6), defender,
                                            {1, 1}, 2,
                                            limb_only) == ItemsStatus::kOk);
  CHECK(limb_only.bounds ==
        std::vector<std::int32_t>{1, kGrowthBound, kGrowthBound});

  auto drawn = MakeDefender();
  for (const auto hit_limb : {-1, 6}) {
    ScriptedHost outside;
    CHECK(items::GrowDefenderEquipmentRatings(ActionOfCategory(5), drawn,
                                              {1, 1}, hit_limb, outside) ==
          ItemsStatus::kHitLimbOutOfRange);
    CHECK(outside.bounds.empty());
  }
}

TEST_CASE("A defender's struck limb is drawn from its armoured limbs",
          "[items-slice][items]") {
  auto defender = MakeDefender();
  defender.weapon_drawn = false;
  defender.limbs[1].max_hp = 0;
  defender.limbs[3].armor.armor.id = 0;
  // Candidates are limbs 0, 2, 4 and 5; draw 6 wraps to limb 4.
  ScriptedHost host({6, 0});
  CHECK(items::GrowDefenderEquipmentRatings(ActionOfCategory(1), defender,
                                            {2, 0}, 0,
                                            host) == ItemsStatus::kOk);
  const auto& armor = defender.limbs[4].armor;
  CHECK(armor.armor.classes[1] == 1);
  CHECK(armor.class_affinity == RowsWith(1, 1, 0, 0, 0));
  CHECK(host.notices ==
        std::vector<Notice>{{0x14, 1, items::StatChange::kIncrease}});
  CHECK(host.bounds.front() == 4);

  auto unarmoured = MakeDefender();
  unarmoured.limbs[1].max_hp = 0;
  ScriptedHost none;
  CHECK(items::GrowDefenderEquipmentRatings(ActionOfCategory(6), unarmoured,
                                            {1, 1}, 1,
                                            none) == ItemsStatus::kOk);
  CHECK(none.bounds.empty());
}

TEST_CASE("A defender's raised shield grows with the attacker's affinity",
          "[items-slice][items]") {
  for (const bool shown : {true, false}) {
    auto defender = MakeDefender();
    defender.shows_notices = shown;
    defender.shield.gems[0].affinities[3] = 4;
    defender.shield.affinity_modifiers[3] = 10;
    ScriptedHost host({0, 0});
    CHECK(items::GrowDefenderEquipmentRatings(ActionOfCategory(5), defender,
                                              {0, 4}, 0,
                                              host) == ItemsStatus::kOk);
    CHECK(defender.shield.base.affinities[3] == 1);
    CHECK(defender.shield.class_affinity == RowsWith(0, 0, 3, 5, 15));
    const auto expected =
        shown ? std::vector<Notice>{{0xF1, 9, items::StatChange::kIncrease}}
              : std::vector<Notice>{};
    CHECK(host.notices == expected);
  }
}

TEST_CASE("An attacker's blade grows against the defender's ratings",
          "[items-slice][items]") {
  items::WeaponLoadout weapon{};
  weapon.blade.id = 1;
  weapon.gems[0].classes[2] = 5;
  ScriptedHost host({0, kNoDraw, kNoDraw, 0, kNoDraw, kNoDraw});
  CHECK(items::GrowAttackerWeaponRatings({}, weapon, true, {3, 2}, host) ==
        ItemsStatus::kOk);
  CHECK(weapon.blade.classes[2] == 1);
  CHECK(weapon.blade.affinities[3] == 1);
  CHECK(host.notices ==
        std::vector<Notice>{{0xF0, 2, items::StatChange::kIncrease},
                            {0xF0, 9, items::StatChange::kIncrease}});
  CHECK(weapon.class_affinity == RowsWith(2, 6, 3, 1, 1));
  CHECK(host.bounds.size() == 6);
}

TEST_CASE("An attacker's spell grows nothing and a bare weapon only rebuilds",
          "[items-slice][items]") {
  items::WeaponLoadout spell_weapon{};
  spell_weapon.blade.id = 1;
  spell_weapon.class_affinity.classes[0][0] = 99;
  items::ItemAction spell{};
  spell.type = items::kSpellActionType;
  ScriptedHost spell_host;
  CHECK(items::GrowAttackerWeaponRatings(spell, spell_weapon, true, {1, 1},
                                         spell_host) == ItemsStatus::kOk);
  CHECK(spell_weapon.class_affinity.classes[0][0] == 99);
  CHECK(spell_host.bounds.empty());

  items::WeaponLoadout bare{};
  bare.gems[0].classes[0] = 3;
  ScriptedHost bare_host;
  CHECK(items::GrowAttackerWeaponRatings({}, bare, true, {1, 1}, bare_host) ==
        ItemsStatus::kOk);
  CHECK(bare.class_affinity == RowsWith(0, 3, 0, 0, 0));
  CHECK(bare_host.bounds.empty());

  items::WeaponLoadout weapon{};
  weapon.blade.id = 1;
  ScriptedHost refused;
  CHECK(items::GrowAttackerWeaponRatings({}, weapon, true, {7, 0}, refused) ==
        ItemsStatus::kClassOutOfRange);
  CHECK(refused.bounds.empty());
}

TEST_CASE("Loadout rows rebuild from part and gem ratings",
          "[items-slice][items]") {
  items::ArmorLoadout armor{};
  armor.armor = MakePart(1, 2);
  armor.affinity_modifiers.fill(1);
  items::RebuildArmorClassAffinity(armor);
  auto expected_armor = RampCurrent({2, 1}, {2, -1});
  for (std::size_t i = 0; i < items::kAffinityCount; ++i) {
    expected_armor.affinities[items::kEffectiveRow][i] =
        static_cast<std::int16_t>(3 - static_cast<int>(i));
  }
  CHECK(armor.class_affinity == expected_armor);

  items::WeaponLoadout weapon{};
  weapon.blade = MakePart(1, 2);
  weapon.grip = MakePart(2, 50);
  weapon.gems = {MakePart(3, 1), MakePart(4, 1), items::EquipmentPart{}};
  items::RebuildWeaponClassAffinity(weapon);
  CHECK(weapon.class_affinity == RampCurrent({4, 3}, {4, -3}));

  items::ShieldLoadout shield{};
  shield.base = weapon.blade;
  shield.gems = weapon.gems;
  items::RebuildShieldClassAffinity(shield);
  CHECK(shield.class_affinity == weapon.class_affinity);
}

// ---------------------------------------------------------------------------
// Wear and recovery.

TEST_CASE("A PP gem raises a weapon's PP by 5%, truncating",
          "[items-slice][items]") {
  struct Case {
    std::int16_t base{};
    bool has_gem{};
    std::int16_t expected{};
  };
  const auto gemmed = WeaponWithPpGem();
  const items::WeaponLoadout plain{};
  for (const auto& [base, has_gem, expected] :
       {Case{32767, true, -31131}, Case{-100, true, -105}, Case{19, true, 19},
        Case{20, true, 21}, Case{20, false, 20}, Case{32767, false, 32767}}) {
    INFO("base " << base << " gem " << has_gem);
    CHECK(items::ApplyPpGemMultiplier(base, has_gem ? gemmed : plain) ==
          expected);
  }
}

namespace {
constexpr std::array<std::int16_t, items::kLimbCount> kLimbDpLoss{
    0, 5, -3, 301, 0, 0,
};

items::DefenderEquipment WornDefender() {
  auto defender = MakeDefender();
  defender.shield.current_dp = 100;
  for (auto& limb : defender.limbs) limb.armor.current_dp = 300;
  return defender;
}

std::array<std::uint16_t, items::kLimbCount> LimbDp(
    const items::DefenderEquipment& defender) {
  std::array<std::uint16_t, items::kLimbCount> dp{};
  for (std::size_t i = 0; i < items::kLimbCount; ++i) {
    dp[i] = defender.limbs[i].armor.current_dp;
  }
  return dp;
}
}  // namespace

TEST_CASE("A hit takes DP off the weapon, raised shield and struck armour",
          "[items-slice][items]") {
  items::WeaponLoadout weapon{};
  weapon.current_dp = 10;
  auto defender = WornDefender();
  CHECK(items::ConsumeHitEquipmentDp({}, weapon, defender, kLimbDpLoss, 15) ==
        ItemsStatus::kOk);
  CHECK(weapon.current_dp == 0);
  CHECK(defender.shield.current_dp == 85);
  CHECK(LimbDp(defender) == std::array<std::uint16_t, items::kLimbCount>{
                                300, 295, 303, 0, 300, 300});
}

TEST_CASE("A spell spares the weapon and a lowered shield keeps its DP",
          "[items-slice][items]") {
  items::WeaponLoadout weapon{};
  items::ItemAction spell{};
  spell.type = items::kSpellActionType;
  weapon.current_dp = 10;
  auto lowered = WornDefender();
  lowered.weapon_drawn = false;
  CHECK(items::ConsumeHitEquipmentDp(spell, weapon, lowered, kLimbDpLoss, 15) ==
        ItemsStatus::kOk);
  CHECK(weapon.current_dp == 10);
  CHECK(lowered.shield.current_dp == 100);

  auto unknown = WornDefender();
  unknown.weapon_drawn.reset();
  CHECK(items::ConsumeHitEquipmentDp({}, weapon, unknown, kLimbDpLoss, 15) ==
        ItemsStatus::kActorOutOfRange);
  CHECK(LimbDp(unknown)[1] == 300);
}

namespace {
struct PpOutcome {
  std::uint16_t weapon{};
  std::uint16_t shield{};
  friend bool operator==(const PpOutcome&, const PpOutcome&) = default;
};

PpOutcome PpAfterHit(bool gems, std::int32_t dp_amount) {
  auto weapon = gems ? WeaponWithPpGem() : items::WeaponLoadout{};
  weapon.current_pp = 50;
  weapon.max_pp = 90;
  auto defender = MakeDefender();
  defender.shield.current_pp = 40;
  defender.shield.max_pp = 90;
  if (gems) defender.shield.gems[0].gem_effects = items::kPpGemEffect;
  CHECK(items::AccumulateHitEquipmentPp({}, weapon, defender, dp_amount) ==
        ItemsStatus::kOk);
  return {weapon.current_pp, defender.shield.current_pp};
}
}  // namespace

TEST_CASE("A hit adds a tenth of its DP as PP, up to the maximum",
          "[items-slice][items]") {
  CHECK(PpAfterHit(true, 100) == PpOutcome{60, 49});
  CHECK(PpAfterHit(true, 400) == PpOutcome{90, 78});
  CHECK(PpAfterHit(false, 100) == PpOutcome{60, 50});
}

TEST_CASE("A negative PP gain wraps and a spell or lowered shield adds none",
          "[items-slice][items]") {
  // A negative gain wraps below zero, as the original's unsigned store did.
  auto weapon = WeaponWithPpGem();
  weapon.current_pp = 5;
  weapon.max_pp = 90;
  auto lowered = MakeDefender();
  lowered.weapon_drawn = false;
  lowered.shield.current_pp = 40;
  CHECK(items::AccumulateHitEquipmentPp({}, weapon, lowered, -200) ==
        ItemsStatus::kOk);
  CHECK(weapon.current_pp == 65520);
  CHECK(lowered.shield.current_pp == 40);

  items::ItemAction spell{};
  spell.type = items::kSpellActionType;
  CHECK(items::AccumulateHitEquipmentPp(spell, weapon, lowered, 100) ==
        ItemsStatus::kOk);
  CHECK(weapon.current_pp == 65520);
  lowered.weapon_drawn.reset();
  CHECK(items::AccumulateHitEquipmentPp({}, weapon, lowered, 100) ==
        ItemsStatus::kActorOutOfRange);
}

TEST_CASE("An item's recovery is five points per stat factor",
          "[items-slice][items]") {
  const items::ItemAction action{0, 0, 0, {3, 31}};
  ScriptedHost fixed;
  CHECK(items::CalculateItemRecovery(action, 0, false, fixed) ==
        std::int16_t{15});
  CHECK(items::CalculateItemRecovery(action, 1, false, fixed) ==
        std::int16_t{155});
  CHECK(fixed.averaged_bounds.empty());

  ScriptedHost varied({0, 10});
  CHECK(items::CalculateItemRecovery(action, 0, true, varied) ==
        std::int16_t{10});
  CHECK(items::CalculateItemRecovery(action, 0, true, varied) ==
        std::int16_t{20});
  CHECK(varied.averaged_bounds == std::vector<std::int32_t>{11, 11});

  ScriptedHost low({0});
  CHECK(items::CalculateItemRecovery({}, 0, true, low) == std::int16_t{0});
  for (const auto hit : {-1, 2}) {
    ScriptedHost outside({0});
    CHECK_FALSE(
        items::CalculateItemRecovery(action, hit, true, outside).has_value());
    CHECK(outside.averaged_bounds.empty());
  }
}

// ---------------------------------------------------------------------------
// Inventory queries.

TEST_CASE("An item id counts what the inventory holds of it",
          "[items-slice][items]") {
  items::InventoryContents inventory{};
  inventory.blades[3].id = 7;
  inventory.grips[0].id = 15;
  inventory.armor[2].id = 25;
  // Ids between the armour and gem bounds are never searched for.
  inventory.armor[3].id = 35;
  inventory.gems[5].id = 45;
  inventory.misc[0] = {55, 0, 0};
  inventory.misc[1] = {55, 9, 0};
  inventory.misc[2] = {55, 4, 0};
  inventory.misc[3] = {72, 3, 0};
  inventory.misc[4] = {85, 2, 0};
  using Case = std::pair<std::int32_t, std::int32_t>;
  for (const auto& [id, expected] :
       {Case{0, 1}, Case{7, 1}, Case{8, 0}, Case{15, 1}, Case{16, 0},
        Case{25, 1}, Case{35, 0}, Case{45, 1}, Case{46, 0}, Case{55, 9},
        Case{56, 0}, Case{72, 3}, Case{85, 0}, Case{-1, 0}}) {
    INFO("id " << id);
    CHECK(items::InventoryItemCount(id, kUpperIds, inventory) == expected);
  }
}

namespace {
items::InventoryContents MiscStacks() {
  items::InventoryContents inventory{};
  inventory.misc[1] = {0x100, 5, 0};
  inventory.misc[2] = {0x150, 2, 0};
  inventory.misc[5] = {0x150, 1, 0};
  inventory.misc[10] = {0x160, 0, 0};
  return inventory;
}
}  // namespace

TEST_CASE("Consuming a misc item takes one from the next stack holding it",
          "[items-slice][items]") {
  auto inventory = MiscStacks();
  std::uint8_t invoked = 3;
  CHECK(items::ConsumeMiscItem(0x150, invoked, inventory));
  CHECK(inventory.misc[5] == items::InventoryMisc{});
  CHECK(inventory.misc[2].count == 2);
  CHECK(invoked == 3);
  // The scan wraps past the last slot.
  CHECK(items::ConsumeMiscItem(0x150, invoked, inventory));
  CHECK(inventory.misc[2] == items::InventoryMisc{0x150, 1, 0});
}

TEST_CASE("Consuming a misc item restarts below the invoked items and wraps",
          "[items-slice][items]") {
  auto inventory = MiscStacks();
  // An id below the invoked items scans from the first slot.
  std::uint8_t invoked = 7;
  CHECK(items::ConsumeMiscItem(0x100, invoked, inventory));
  CHECK(invoked == 0);
  CHECK(inventory.misc[1].count == 4);

  // An empty stack wraps to 255 and keeps its id.
  invoked = 0;
  CHECK(items::ConsumeMiscItem(0x160, invoked, inventory));
  CHECK(inventory.misc[10] == items::InventoryMisc{0x160, 255, 0});

  invoked = 9;
  CHECK_FALSE(items::ConsumeMiscItem(0x170, invoked, inventory));
  CHECK(invoked == 9);
}

namespace {
// An emptied record that keeps its 1-based slot number.
template <typename Record>
Record Emptied(std::int32_t slot) {
  Record record{};
  record.index = static_cast<decltype(record.index)>(slot);
  return record;
}
}  // namespace

TEST_CASE("Clearing a weapon slot empties the weapon and its parts",
          "[items-slice][items]") {
  auto inventory = MakeInventory();
  auto expected = inventory;
  expected.weapons[0] = Emptied<items::InventoryWeapon>(1);
  expected.blades[0] = Emptied<items::InventoryBlade>(1);
  expected.grips[1] = Emptied<items::InventoryGrip>(2);
  expected.gems[2] = Emptied<items::InventoryGem>(3);
  expected.gems[4] = Emptied<items::InventoryGem>(5);
  CHECK(inv::ClearItemSlot(ItemCategory::kWeapon, 1, inventory) ==
        ItemsStatus::kOk);
  CHECK(inventory == expected);

  // A weapon without parts refuses them and still empties its slot.
  inventory.weapons[1] = {2, 0, 0, 1, {}, Name('X')};
  expected.weapons[1] = Emptied<items::InventoryWeapon>(2);
  CHECK(inv::ClearItemSlot(ItemCategory::kWeapon, 2, inventory) ==
        ItemsStatus::kSlotOutOfRange);
  CHECK(inventory == expected);

  for (const auto slot : {0, 9}) {
    CHECK(inv::ClearItemSlot(ItemCategory::kWeapon, slot, inventory) ==
          ItemsStatus::kSlotOutOfRange);
  }
  CHECK(inventory == expected);
}

TEST_CASE("Clearing a shield slot empties the shield and its gems",
          "[items-slice][items]") {
  auto inventory = MakeInventory();
  inventory.shields[0].gems = {3, 0, 5, 0};
  auto expected = inventory;
  expected.shields[0] = Emptied<items::InventoryShield>(1);
  expected.gems[2] = Emptied<items::InventoryGem>(3);
  expected.gems[4] = Emptied<items::InventoryGem>(5);
  CHECK(inv::ClearItemSlot(ItemCategory::kShield, 1, inventory) ==
        ItemsStatus::kOk);
  CHECK(inventory == expected);

  // Gem references that carry the shield flag name no gem slot.
  auto flagged = MakeInventory();
  auto flagged_expected = flagged;
  flagged_expected.shields[0] = Emptied<items::InventoryShield>(1);
  CHECK(inv::ClearItemSlot(ItemCategory::kShield, 1, flagged) ==
        ItemsStatus::kSlotOutOfRange);
  CHECK(flagged == flagged_expected);
}

TEST_CASE("Clearing a part or stack slot keeps only its slot number",
          "[items-slice][items]") {
  auto inventory = MakeInventory();
  auto expected = inventory;
  expected.misc[0] = Emptied<items::InventoryMisc>(1);
  expected.armor[0] = Emptied<items::InventoryArmor>(1);
  expected.grips[1] = Emptied<items::InventoryGrip>(2);
  CHECK(inv::ClearItemSlot(ItemCategory::kMisc, 1, inventory) ==
        ItemsStatus::kOk);
  CHECK(inv::ClearItemSlot(ItemCategory::kArmor, 1, inventory) ==
        ItemsStatus::kOk);
  CHECK(inv::ClearItemSlot(ItemCategory::kGrip, 2, inventory) ==
        ItemsStatus::kOk);
  CHECK(inventory == expected);

  CHECK(inv::ClearItemSlot(ItemCategory::kArmor, 17, inventory) ==
        ItemsStatus::kSlotOutOfRange);
  CHECK(inv::ClearItemSlot(ItemCategory::kGem, 49, inventory) ==
        ItemsStatus::kSlotOutOfRange);
  CHECK(inv::ClearItemSlot(ItemCategory::kManage, 1, inventory) ==
        ItemsStatus::kOk);
  CHECK(inventory == expected);
}

TEST_CASE("A slot's occupancy is its blade or item id",
          "[items-slice][items]") {
  const auto inventory = MakeInventory();
  using Case = std::tuple<ItemCategory, std::size_t, std::optional<int>>;
  for (const auto& [category, index, expected] :
       {Case{ItemCategory::kWeapon, 0, 1}, Case{ItemCategory::kBlade, 0, 0x11},
        Case{ItemCategory::kGrip, 1, 0x1234}, Case{ItemCategory::kGrip, 0, 0},
        Case{ItemCategory::kShield, 0, 0x31},
        Case{ItemCategory::kArmor, 0, 0x21}, Case{ItemCategory::kGem, 4, 0x32},
        Case{ItemCategory::kMisc, 0, 0x150}, Case{ItemCategory::kManage, 0, 0},
        Case{ItemCategory::kWeapon, 8, std::nullopt},
        Case{ItemCategory::kGem, 48, std::nullopt},
        Case{ItemCategory::kMisc, 64, std::nullopt}}) {
    INFO("category " << static_cast<int>(category) << " index " << index);
    const auto id = inv::ItemOccupancyId(category, index, inventory);
    REQUIRE(id.has_value() == expected.has_value());
    if (id) CHECK(int{*id} == *expected);
  }
}

TEST_CASE("Each category's display order is its row of the indices",
          "[items-slice][items]") {
  items::InventoryIndices indices{};
  using Case = std::pair<ItemCategory, std::span<std::uint8_t>>;
  for (const auto& [category, row] :
       {Case{ItemCategory::kWeapon, indices.weapons},
        Case{ItemCategory::kBlade, indices.blades},
        Case{ItemCategory::kGrip, indices.grips},
        Case{ItemCategory::kShield, indices.shields},
        Case{ItemCategory::kArmor, indices.armor},
        Case{ItemCategory::kGem, indices.gems},
        Case{ItemCategory::kMisc, indices.misc}}) {
    const auto order = inv::DisplayOrder(indices, category);
    CHECK(order.data() == row.data());
    CHECK(order.size() == row.size());
  }
  CHECK(inv::DisplayOrder(indices, ItemCategory::kManage).empty());
}

TEST_CASE("A display position is found and appended by 1-based slot",
          "[items-slice][items]") {
  std::array<std::uint8_t, 4> order{3, 1, 6, 0};
  CHECK(inv::FindDisplayPosition(order, 0) == 2);
  CHECK(inv::FindDisplayPosition(order, 5) == 3);
  CHECK(inv::FindDisplayPosition(order, 7) == 0);
  CHECK(inv::FindDisplayPosition(order, 255) == 0);

  inv::AppendDisplayIndex(order, 8);
  CHECK(order == std::array<std::uint8_t, 4>{3, 1, 6, 9});
  inv::AppendDisplayIndex(order, 10);
  CHECK(order == std::array<std::uint8_t, 4>{3, 1, 6, 9});
}

TEST_CASE("Rebuilding a display order drops stale rows and lists new items",
          "[items-slice][items]") {
  items::InventoryContents inventory{};
  inventory.blades[0].id = 1;
  inventory.blades[2].id = 3;
  inventory.blades[5].id = 6;
  items::InventoryIndices indices{};
  indices.blades[0] = 3;
  indices.blades[1] = 17;
  indices.blades[2] = 2;
  auto expected = items::InventoryIndices{};
  expected.blades[0] = 3;
  expected.blades[1] = 1;
  expected.blades[2] = 6;
  CHECK(inv::RebuildDisplayIndices(ItemCategory::kBlade, indices, inventory) ==
        ItemsStatus::kDisplayEntryOutOfRange);
  CHECK(indices == expected);
  CHECK(inv::RebuildDisplayIndices(ItemCategory::kBlade, indices, inventory) ==
        ItemsStatus::kOk);
  CHECK(indices == expected);
  CHECK(inv::RebuildDisplayIndices(ItemCategory::kManage, indices, inventory) ==
        ItemsStatus::kCategoryOutOfRange);
  CHECK(indices == expected);
}

TEST_CASE("Item counts and first empty slots follow the category tables",
          "[items-slice][items]") {
  const auto inventory = MakeInventory();
  using Case = std::tuple<ItemCategory, std::int32_t, std::size_t>;
  for (const auto& [category, count, empty] :
       {Case{ItemCategory::kWeapon, 1, 1}, Case{ItemCategory::kBlade, 1, 1},
        Case{ItemCategory::kGrip, 1, 0}, Case{ItemCategory::kShield, 1, 1},
        Case{ItemCategory::kArmor, 1, 1}, Case{ItemCategory::kGem, 2, 0},
        Case{ItemCategory::kMisc, 1, 1}, Case{ItemCategory::kManage, 0, 0}}) {
    INFO("category " << static_cast<int>(category));
    CHECK(inv::CountItems(category, inventory) == count);
    CHECK(inv::FirstEmptySlot(category, inventory) == empty);
  }

  items::InventoryContents full{};
  for (auto& stack : full.misc) stack = {1, 1, 0};
  CHECK(inv::CountItems(ItemCategory::kMisc, full) == 64);
  CHECK(inv::FirstEmptySlot(ItemCategory::kMisc, full) == 64);
}

// ---------------------------------------------------------------------------
// Copying items between inventories.

namespace {
using CopyResult = std::pair<std::int32_t, ItemsStatus>;

CopyResult Result(const inv::ItemCopy& copy) {
  return {copy.slot, copy.status};
}

constexpr std::int32_t CopyFlags(ItemCategory category, bool write,
                                 std::int32_t parent = 0) {
  return (parent << inv::kCopyItemParentShift) |
         static_cast<std::int32_t>(category) |
         (write ? inv::kCopyItemWrite : 0);
}

// Copies between two inventories with the target as the displayed one.
struct CopyBench {
  items::InventoryContents source = MakeInventory();
  items::InventoryContents target{};
  items::InventoryIndices indices{};

  CopyResult Copy(std::int32_t flags, std::int32_t source_index) {
    return Result(
        inv::CopyItem(flags, target, source_index, source, indices, target));
  }
};

template <typename Record>
Record Copied(Record record, std::int32_t slot) {
  record.index = static_cast<decltype(record.index)>(slot);
  return record;
}
}  // namespace

TEST_CASE("A copied part takes the first empty slot and its parent",
          "[items-slice][items]") {
  CopyBench bench;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kBlade, true, 5), 0) ==
        CopyResult{1, ItemsStatus::kOk});
  auto blade = Copied(bench.source.blades[0], 1);
  blade.assembled_weapon_index = 5;
  CHECK(bench.target.blades[0] == blade);

  auto grip = Copied(bench.source.grips[1], 1);
  grip.assembled_weapon_index = 0x102;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kGrip, true, 0x102), 1) ==
        CopyResult{1, ItemsStatus::kOk});
  CHECK(bench.target.grips[0] == grip);

  auto gem = Copied(bench.source.gems[2], 1);
  gem.set_item_index = 0x83;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kGem, true, 0x83), 2) ==
        CopyResult{1, ItemsStatus::kOk});
  CHECK(bench.target.gems[0] == gem);

  CHECK(bench.Copy(CopyFlags(ItemCategory::kArmor, true, 7), 0) ==
        CopyResult{1, ItemsStatus::kOk});
  CHECK(bench.target.armor[0] == Copied(bench.source.armor[0], 1));
}

TEST_CASE("A part copy only probes without the write flag and refuses others",
          "[items-slice][items]") {
  CopyBench bench;
  bench.target.blades[0].id = 9;
  const auto before = bench.target;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kBlade, false), 0) ==
        CopyResult{2, ItemsStatus::kOk});
  for (const auto source_index : {-1, 16}) {
    CHECK(bench.Copy(CopyFlags(ItemCategory::kBlade, true), source_index) ==
          CopyResult{0, ItemsStatus::kSlotOutOfRange});
  }
  for (const auto flags : {8, 0x1F}) {
    CHECK(bench.Copy(flags, 0) ==
          CopyResult{0, ItemsStatus::kCategoryOutOfRange});
  }
  CHECK(bench.Copy(CopyFlags(ItemCategory::kManage, true), 0) ==
        CopyResult{0, ItemsStatus::kOk});
  CHECK(bench.target == before);

  for (auto& blade : bench.target.blades) blade.id = 9;
  const auto full = bench.target;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kBlade, true), 0) ==
        CopyResult{0, ItemsStatus::kOk});
  CHECK(bench.target == full);
}

TEST_CASE("A copied weapon copies its parts and points at their new slots",
          "[items-slice][items]") {
  CopyBench bench;
  bench.target.blades[0].id = 9;
  bench.target.gems[0].id = 9;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kWeapon, true), 0) ==
        CopyResult{1, ItemsStatus::kOk});
  CHECK(bench.target.weapons[0] ==
        items::InventoryWeapon{1, 2, 1, 0, {2, 0, 3, 0}, Name('W')});
  auto blade = Copied(bench.source.blades[0], 2);
  blade.assembled_weapon_index = 1;
  CHECK(bench.target.blades[1] == blade);
  auto grip = Copied(bench.source.grips[1], 1);
  grip.assembled_weapon_index = 1;
  CHECK(bench.target.grips[0] == grip);
  auto first_gem = Copied(bench.source.gems[2], 2);
  first_gem.set_item_index = 1;
  auto second_gem = Copied(bench.source.gems[4], 3);
  second_gem.set_item_index = 1;
  CHECK(bench.target.gems[1] == first_gem);
  CHECK(bench.target.gems[2] == second_gem);
}

TEST_CASE("A weapon copy probe or a bad weapon slot leaves the target alone",
          "[items-slice][items]") {
  CopyBench bench;
  bench.target.blades[0].id = 9;
  bench.target.gems[0].id = 9;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kWeapon, true), 0) ==
        CopyResult{1, ItemsStatus::kOk});
  const auto copied = bench.target;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kWeapon, false), 0) ==
        CopyResult{2, ItemsStatus::kOk});
  CHECK(bench.Copy(CopyFlags(ItemCategory::kWeapon, true), 8) ==
        CopyResult{0, ItemsStatus::kSlotOutOfRange});
  CHECK(bench.target == copied);
}

TEST_CASE("A weapon copy needs room for the weapon, its parts and its gems",
          "[items-slice][items]") {
  CopyBench full_blades;
  for (auto& blade : full_blades.target.blades) blade.id = 9;
  CHECK(full_blades.Copy(CopyFlags(ItemCategory::kWeapon, true), 0) ==
        CopyResult{0, ItemsStatus::kOk});

  CopyBench full_grips;
  for (auto& grip : full_grips.target.grips) grip.id = 9;
  CHECK(full_grips.Copy(CopyFlags(ItemCategory::kWeapon, true), 0) ==
        CopyResult{0, ItemsStatus::kOk});

  CopyBench full_weapons;
  for (auto& weapon : full_weapons.target.weapons) weapon.blade = 1;
  CHECK(full_weapons.Copy(CopyFlags(ItemCategory::kWeapon, true), 0) ==
        CopyResult{0, ItemsStatus::kOk});

  // Two set gems need two free gem slots.
  CopyBench gems;
  for (std::size_t i = 0; i + 1 < gems.target.gems.size(); ++i) {
    gems.target.gems[i].id = 9;
  }
  const auto before = gems.target;
  CHECK(gems.Copy(CopyFlags(ItemCategory::kWeapon, true), 0) ==
        CopyResult{0, ItemsStatus::kOk});
  CHECK(gems.target == before);
}

TEST_CASE("A copied shield flags its gems with its slot",
          "[items-slice][items]") {
  CopyBench bench;
  bench.source.shields[0].gems = {3, 0, 5, 0};
  bench.target.shields[0].base.id = 9;
  CHECK(bench.Copy(CopyFlags(ItemCategory::kShield, true), 0) ==
        CopyResult{2, ItemsStatus::kOk});
  CHECK(bench.target.shields[1] ==
        items::InventoryShield{
            2, 0, 0, bench.source.shields[0].base, {1, 0, 2, 0}});
  auto first_gem = Copied(bench.source.gems[2], 1);
  first_gem.set_item_index = 0x82;
  auto second_gem = Copied(bench.source.gems[4], 2);
  second_gem.set_item_index = 0x82;
  CHECK(bench.target.gems[0] == first_gem);
  CHECK(bench.target.gems[1] == second_gem);

  // Flagged gem references name no source gem; the shield still copies.
  CopyBench flagged;
  CHECK(flagged.Copy(CopyFlags(ItemCategory::kShield, true), 0) ==
        CopyResult{1, ItemsStatus::kSlotOutOfRange});
  CHECK(flagged.target.shields[0].gems ==
        std::array<std::uint8_t, items::kShieldGemSlots>{});
  CHECK(flagged.Copy(CopyFlags(ItemCategory::kShield, true), 8) ==
        CopyResult{0, ItemsStatus::kSlotOutOfRange});
}

TEST_CASE("A copied misc stack fills matching stacks before a new one",
          "[items-slice][items]") {
  CopyBench bench;
  bench.target.misc[0] = {0x150, 95, 1};
  bench.target.misc[1] = {0x200, 3, 2};
  CHECK(bench.Copy(CopyFlags(ItemCategory::kMisc, false), 0) ==
        CopyResult{3, ItemsStatus::kOk});
  CHECK(bench.target.misc[0].count == 95);
  CHECK(bench.Copy(CopyFlags(ItemCategory::kMisc, true), 0) ==
        CopyResult{3, ItemsStatus::kOk});
  CHECK(bench.target.misc[0] == items::InventoryMisc{0x150, 100, 1});
  CHECK(bench.target.misc[2] == items::InventoryMisc{0x150, 2, 0});
  CHECK(bench.indices.misc[0] == 1);
  CHECK(bench.indices.misc[1] == 2);
  CHECK(bench.indices.misc[2] == 0);
}

TEST_CASE("A copied misc stack that fits merges without a new stack",
          "[items-slice][items]") {
  CopyBench fits;
  fits.target.misc[0] = {0x150, 90, 1};
  CHECK(fits.Copy(CopyFlags(ItemCategory::kMisc, true), 0) ==
        CopyResult{1, ItemsStatus::kOk});
  CHECK(fits.target.misc[0] == items::InventoryMisc{0x150, 97, 1});
  CHECK(fits.target.misc[1] == items::InventoryMisc{});

  CHECK(fits.Copy(CopyFlags(ItemCategory::kMisc, true), 64) ==
        CopyResult{0, ItemsStatus::kSlotOutOfRange});
}

TEST_CASE("A misc copy into a full table only merges", "[items-slice][items]") {
  CopyBench full;
  for (auto& stack : full.target.misc) stack = {0x200, 1, 0};
  const auto before = full.target;
  CHECK(full.Copy(CopyFlags(ItemCategory::kMisc, true), 0) ==
        CopyResult{0, ItemsStatus::kOk});
  CHECK(full.target == before);

  full.target.misc[10] = {0x150, 50, 0};
  CHECK(full.Copy(CopyFlags(ItemCategory::kMisc, true), 0) ==
        CopyResult{1, ItemsStatus::kOk});
  CHECK(full.target.misc[10] == items::InventoryMisc{0x150, 57, 0});

  // An empty source stack copies nothing.
  CopyBench empty;
  empty.source.misc[0].count = 0;
  CHECK(empty.Copy(CopyFlags(ItemCategory::kMisc, true), 0) ==
        CopyResult{1, ItemsStatus::kOk});
  CHECK(empty.target.misc == decltype(empty.target.misc){});
}

TEST_CASE("Ashley's equipped loadouts store back into their inventory slots",
          "[items-slice][items]") {
  inv::EquippedLoadouts ashley{};
  ashley.weapon.index = 1;
  ashley.weapon.current_dp = 77;
  ashley.limb_armor[2].index = 1;
  ashley.limb_armor[2].current_dp = 33;
  ashley.accessory.index = 0;
  auto inventory = MakeInventory();

  auto expected_ashley = ashley;
  auto expected = inventory;
  CHECK(items::StoreInventoryWeaponStats(expected.weapons[0],
                                         expected_ashley.weapon,
                                         expected) == ItemsStatus::kOk);
  items::StoreInventoryArmorStats(expected.armor[0],
                                  expected_ashley.limb_armor[2]);

  CHECK(inv::StoreEquippedStats(ashley, inventory) == ItemsStatus::kOk);
  CHECK(inventory == expected);
  CHECK(ashley.weapon == expected_ashley.weapon);

  ashley.shield.index = 9;
  const auto stored = inventory;
  CHECK(inv::StoreEquippedStats(ashley, inventory) ==
        ItemsStatus::kPartReferenceOutOfRange);
  CHECK(inventory.shields == stored.shields);
}

TEST_CASE("Unequipping clears Ashley's loadout and every equipped mark",
          "[items-slice][items]") {
  auto inventory = MakeInventory();
  inventory.weapons[3].is_equipped = 1;
  items::AshleyWeaponState ashley{};
  ashley.weapon.blade.id = 4;
  auto expected_ashley = ashley;
  FakeStatusHost expected_host(0);
  CHECK(items::EquipAshleyWeapon(expected_ashley, nullptr, inventory,
                                 expected_host) == ItemsStatus::kOk);
  FakeStatusHost host(0);
  CHECK(inv::UnequipWeapon(ashley, inventory, host) == ItemsStatus::kOk);
  CHECK(ashley.weapon == expected_ashley.weapon);
  for (const auto& weapon : inventory.weapons) CHECK(weapon.is_equipped == 0);
  CHECK(inventory.shields[0].is_equipped == 1);
}

TEST_CASE("Unequipping a shield clears it and every equipped shield mark",
          "[items-slice][items]") {
  auto inventory = MakeInventory();
  FakeStatusHost expected_host(0);
  FakeStatusHost host(0);
  items::ShieldLoadout shield{};
  shield.base.id = 4;
  auto expected_shield = shield;
  CHECK(items::EquipAshleyShield(expected_shield, nullptr, inventory,
                                 expected_host) == ItemsStatus::kOk);
  CHECK(inv::UnequipShield(shield, inventory, host) == ItemsStatus::kOk);
  CHECK(shield == expected_shield);
  for (const auto& entry : inventory.shields) CHECK(entry.is_equipped == 0);
}

// ---------------------------------------------------------------------------
// Loot.

namespace {
items::WeaponLoadout DroppingWeapon(std::uint8_t drop_rate) {
  items::WeaponLoadout weapon{};
  weapon.blade = MakePart(0x11, 2);
  weapon.grip = MakePart(0x22, 1);
  weapon.gems = {MakePart(0x31, 1), items::EquipmentPart{}, MakePart(0x33, 0)};
  weapon.name = Name('D');
  weapon.drop_rate = drop_rate;
  return weapon;
}

items::LootedWeapon ExpectedLootedWeapon(const items::WeaponLoadout& base) {
  items::LootedWeapon looted{};
  items::WriteInventoryBladeStats(looted.blade, base.blade);
  items::WriteInventoryGripStats(looted.grip, base.grip);
  items::WriteInventoryGemStats(looted.gems[0], base.gems[0]);
  items::WriteInventoryGemStats(looted.gems[2], base.gems[2]);
  looted.name = base.name;
  looted.header.difficulty_flags = items::kLootDropped;
  return looted;
}
}  // namespace

TEST_CASE("An enemy's weapon drops when the draw is below its rate",
          "[items-slice][items]") {
  const auto weapon = DroppingWeapon(10);
  items::LootedWeapon looted{};
  ScriptedHost missed({10});
  CHECK_FALSE(items::RollEnemyWeaponDrop(looted, weapon, missed));
  CHECK(looted == items::LootedWeapon{});
  CHECK(missed.bounds == std::vector<std::int32_t>{items::kDropRateDraw});

  ScriptedHost hit({9});
  CHECK(items::RollEnemyWeaponDrop(looted, weapon, hit));
  CHECK(looted == ExpectedLootedWeapon(weapon));

  // A rate of 0 never drops; 255 always does, since draws are below 255.
  ScriptedHost never({0});
  CHECK_FALSE(items::RollEnemyWeaponDrop(looted, DroppingWeapon(0), never));
  ScriptedHost always({254});
  CHECK(items::RollEnemyWeaponDrop(looted, DroppingWeapon(255), always));
}

TEST_CASE("An enemy's shield and accessory drop as inventory records",
          "[items-slice][items]") {
  items::ShieldLoadout shield{};
  shield.base = MakePart(0x41, 3);
  shield.gems = {items::EquipmentPart{}, MakePart(0x52, 1),
                 items::EquipmentPart{}};
  shield.drop_rate = 1;
  items::LootedShield looted_shield{};
  ScriptedHost host({0, 0, 1});
  CHECK(items::RollEnemyShieldDrop(looted_shield, shield, host));
  items::LootedShield expected_shield{};
  items::WriteInventoryArmorStats(expected_shield.shield, shield.base);
  items::WriteInventoryGemStats(expected_shield.gems[1], shield.gems[1]);
  expected_shield.header.difficulty_flags = items::kLootDropped;
  CHECK(looted_shield == expected_shield);

  items::AccessorySpawnLoadout accessory{};
  accessory.accessory = MakePart(0x61, -1);
  accessory.drop_rate = 1;
  items::LootedArmor looted_accessory{};
  CHECK(items::RollEnemyAccessoryDrop(looted_accessory, accessory, host));
  items::LootedArmor expected_accessory{};
  items::WriteInventoryArmorStats(expected_accessory.armor,
                                  accessory.accessory);
  expected_accessory.header.difficulty_flags = items::kLootDropped;
  CHECK(looted_accessory == expected_accessory);

  items::LootedArmor missed{};
  CHECK_FALSE(items::RollEnemyAccessoryDrop(missed, accessory, host));
  CHECK(missed == items::LootedArmor{});
}

TEST_CASE("Dropped armour fills the first free armour slot",
          "[items-slice][items]") {
  items::ArmorLoadout armor{};
  armor.armor = MakePart(0x21, 2);
  armor.drop_rate = 200;
  std::array<items::LootedArmor, items::kLootArmorSlots> looted{};
  ScriptedHost host({0, 0, 0});
  CHECK(items::RollEnemyArmorDrop(looted, armor, host));
  items::LootedArmor expected{};
  items::WriteInventoryArmorStats(expected.armor, armor.armor);
  expected.header.difficulty_flags = items::kLootDropped;
  CHECK(looted[0] == expected);
  CHECK(looted[1] == items::LootedArmor{});
  CHECK(items::RollEnemyArmorDrop(looted, armor, host));
  CHECK(looted[1] == expected);

  // With both slots full the draw is still taken and nothing drops.
  const auto full = looted;
  CHECK_FALSE(items::RollEnemyArmorDrop(looted, armor, host));
  CHECK(looted == full);
  CHECK(host.bounds.size() == 3);
}

TEST_CASE("An enemy's guaranteed and random items fill the item rows",
          "[items-slice][items]") {
  items::BattleLoot loot{};
  loot.misc[0] = {0x99, 0, 9};
  const std::array<items::MiscItemStack, items::kGuaranteedDropCount> none{};
  CHECK_FALSE(items::CopyGuaranteedEnemyItemDrops(loot, none));
  CHECK(loot.misc[0] == items::LootedItem{0, 0, 9});

  const std::array<items::MiscItemStack, items::kGuaranteedDropCount> stacks{
      {{0, 4, 0}, {0x150, 3, 0}}};
  CHECK(items::CopyGuaranteedEnemyItemDrops(loot, stacks));
  CHECK(loot.misc[0] == items::LootedItem{0, 0, 9});
  CHECK(loot.misc[1] == items::LootedItem{0x150, items::kLootDropped, 3});
}

TEST_CASE("An enemy's random item fills the third item row when drawn",
          "[items-slice][items]") {
  items::BattleLoot loot{};
  ScriptedHost host({5, 4});
  CHECK_FALSE(items::RollEnemyItemDrop(loot, {0, 255, 0}, host));
  CHECK(host.bounds.empty());
  CHECK_FALSE(items::RollEnemyItemDrop(loot, {0x160, 5, 0}, host));
  CHECK(loot.misc[2] == items::LootedItem{0x160, 0, 0});
  CHECK(items::RollEnemyItemDrop(loot, {0x161, 5, 0}, host));
  CHECK(loot.misc[2] == items::LootedItem{0x161, items::kLootDropped, 1});
}

namespace {
items::EnemyDrops ArmouredEnemy() {
  items::EnemyDrops enemy{};
  enemy.actor_id = 4;
  enemy.weapon = DroppingWeapon(1);
  enemy.shield.base = MakePart(0x41, 3);
  enemy.shield.drop_rate = 1;
  enemy.accessory.accessory = MakePart(0x61, 1);
  enemy.accessory.drop_rate = 1;
  enemy.limb_armor[1].armor = MakePart(0x21, 2);
  enemy.limb_armor[1].drop_rate = 1;
  enemy.limb_armor[4].armor = MakePart(0x22, 2);
  enemy.limb_armor[4].drop_rate = 1;
  enemy.random_item = {0x170, 1, 0};
  return enemy;
}
}  // namespace

TEST_CASE("An enemy's loot is rolled into a reserved list entry",
          "[items-slice][items]") {
  const auto enemy = ArmouredEnemy();
  FakeLootFlow list;
  ScriptedHost host({0, 9, 9, 0, 0, 0});
  items::QueueEnemyLoot(enemy, list, host);
  CHECK(host.bounds == std::vector<std::int32_t>(6, items::kDropRateDraw));
  CHECK(list.calls == std::vector<std::string>{"reserve", "push"});
  CHECK(list.droppers == std::vector<std::int32_t>{4});
  REQUIRE(list.pushed.size() == 1);
  CHECK(list.pushed.front().weapon == ExpectedLootedWeapon(enemy.weapon));
}

TEST_CASE("An enemy's rolled loot keeps only the equipment that dropped",
          "[items-slice][items]") {
  FakeLootFlow list;
  ScriptedHost host({0, 9, 9, 0, 0, 0});
  items::QueueEnemyLoot(ArmouredEnemy(), list, host);
  REQUIRE(list.pushed.size() == 1);
  const auto& loot = list.pushed.front();
  CHECK(loot.shield == items::LootedShield{});
  CHECK(loot.accessory == items::LootedArmor{});
  CHECK(loot.armor[0].armor.id == 0x21);
  CHECK(loot.armor[1].armor.id == 0x22);
  CHECK(loot.misc[2] == items::LootedItem{0x170, items::kLootDropped, 1});
}

TEST_CASE("An enemy's loot entry is released when nothing new drops",
          "[items-slice][items]") {
  items::EnemyDrops bare{};
  bare.actor_id = 2;
  FakeLootFlow nothing;
  ScriptedHost nothing_host;
  items::QueueEnemyLoot(bare, nothing, nothing_host);
  CHECK(nothing_host.bounds ==
        std::vector<std::int32_t>(3, items::kDropRateDraw));
  CHECK(nothing.calls == std::vector<std::string>{"reserve", "release"});

  auto listed_enemy = ArmouredEnemy();
  listed_enemy.guaranteed_items[0] = {0x150, 1, 0};
  FakeLootFlow listed;
  listed.listed = {4};
  ScriptedHost listed_host;
  items::QueueEnemyLoot(listed_enemy, listed, listed_host);
  CHECK(listed.calls == std::vector<std::string>{"reserve", "release"});

  FakeLootFlow full;
  full.can_reserve = false;
  ScriptedHost full_host;
  items::QueueEnemyLoot(listed_enemy, full, full_host);
  CHECK(full.calls == std::vector<std::string>{"reserve"});
  CHECK(full_host.bounds.empty());
}

TEST_CASE("Pending enemy loot opens once the reward stage passes",
          "[items-slice][items]") {
  using Case = std::tuple<std::int32_t, bool, std::string>;
  for (const auto& [stage, empty, call] :
       {Case{2, false, "wait"}, Case{-1, false, "wait"},
        Case{3, true, "ability-reward"}, Case{3, false, "open-enemy"}}) {
    FakeLootFlow flow;
    flow.reward_stage = stage;
    flow.empty = empty;
    items::OpenPendingEnemyLoot(flow, flow);
    CHECK(flow.calls == std::vector<std::string>{call});
  }
}

TEST_CASE("Scripted loot is listed under the script's dropper",
          "[items-slice][items]") {
  const std::array<items::MiscItemStack, items::kGuaranteedDropCount> loot{
      {{0x150, 2, 0}, {0, 0, 0}}};
  FakeLootFlow flow;
  items::PresentScriptedLoot(loot, flow, flow);
  CHECK(flow.calls ==
        std::vector<std::string>{"reserve", "push", "open-scripted"});
  CHECK(flow.droppers ==
        std::vector<std::int32_t>{items::kScriptedLootDropper});

  const std::array<items::MiscItemStack, items::kGuaranteedDropCount> none{};
  FakeLootFlow empty;
  items::PresentScriptedLoot(none, empty, empty);
  CHECK(empty.calls ==
        std::vector<std::string>{"reserve", "release", "resume"});

  FakeLootFlow listed;
  listed.can_reserve = false;
  listed.empty = false;
  items::PresentScriptedLoot(loot, listed, listed);
  CHECK(listed.calls == std::vector<std::string>{"reserve", "open-scripted"});
}

TEST_CASE("Leaving a room resets the loot pages before clearing the list",
          "[items-slice][items]") {
  FakeLootFlow flow;
  items::ClearRoomLootList(flow, flow);
  CHECK(flow.calls == std::vector<std::string>{"reset-paging", "clear"});
}

namespace {
struct ChestSpot {
  std::int16_t x{};
  std::int16_t z{};
  std::int16_t layer{};
  std::int16_t opened_flag{};
};

items::TreasureChest Chest(const ChestSpot& spot) {
  items::TreasureChest chest{};
  chest.tile_x = spot.x;
  chest.tile_z = spot.z;
  chest.layer = spot.layer;
  chest.lid_rotation_id = static_cast<std::int16_t>(spot.x + 100);
  chest.is_open = 7;
  chest.opened_flag = spot.opened_flag;
  return chest;
}
}  // namespace

TEST_CASE("A room's chest is found by its tile and layer",
          "[items-slice][items]") {
  const std::array<items::TreasureChest, 3> chests{
      Chest({1, 2, 0, 0}), Chest({1, 2, 1, 0}), Chest({5, 6, 1, 0})};
  CHECK(items::TreasureChestAtTile(chests, 1, 2, 1) == std::size_t{1});
  CHECK(items::TreasureChestAtTile(chests, 5, 6, 1) == std::size_t{2});
  CHECK_FALSE(items::TreasureChestAtTile(chests, 5, 6, 0).has_value());
  CHECK_FALSE(items::TreasureChestAtTile(chests, 0x10001, 2, 0).has_value());
  CHECK_FALSE(items::TreasureChestAtTile({}, 1, 2, 0).has_value());
}

TEST_CASE("A room's chests open again when their script flag is set",
          "[items-slice][items]") {
  std::array<items::TreasureChest, 3> chests{
      Chest({1, 2, 0, 0x20}), Chest({3, 4, 0, 0x21}), Chest({5, 6, 0, 0x7C40})};
  FakeChestHost host;
  host.set_flags = {0x3E0, -32768};
  items::RestoreRoomChestStates(chests, host);
  CHECK(host.queried == std::vector<std::int16_t>{0x3E0, 0x3E1, -32768});
  CHECK(chests[0].is_open == 1);
  CHECK(chests[1].is_open == 0);
  CHECK(chests[2].is_open == 1);
  CHECK(host.lids == std::vector<std::int16_t>{101, 105});
  CHECK(host.passabilities ==
        std::vector<std::pair<std::int16_t, std::uint8_t>>{
            {1, items::kOpenChestPassability},
            {3, items::kClosedChestPassability},
            {5, items::kOpenChestPassability}});
}

TEST_CASE("A room's chest section holds whole chest records",
          "[items-slice][items]") {
  constexpr auto kChestBytes =
      static_cast<std::uint32_t>(sizeof(items::TreasureChest));
  static_assert(kChestBytes == 0x234);
  CHECK(items::RoomTreasureChests(std::nullopt) == 0);
  CHECK(items::RoomTreasureChests(0U) == 0);
  CHECK(items::RoomTreasureChests(kChestBytes - 1) == 0);
  CHECK(items::RoomTreasureChests(kChestBytes * 3 + 5) == 3);

  std::array<items::TreasureChest, 2> chests{Chest({1, 2, 0, 0}),
                                             Chest({3, 4, 0, 0})};
  items::BattleLoot loot{};
  loot.misc[0] = {0x150, items::kLootDropped, 2};
  CHECK(items::LoadRoomChestLoot(chests, loot) == ItemsStatus::kOk);
  CHECK(chests[0].loot == loot);
  CHECK(chests[1].loot == items::BattleLoot{});
  CHECK(items::LoadRoomChestLoot({}, loot) == ItemsStatus::kNoChests);
}

// ---------------------------------------------------------------------------
// Live bindings.

namespace {
std::string Refused(std::string_view operation, std::string_view reason) {
  return std::string(operation) + " refused: " + std::string(reason);
}

constexpr int CategoryId(ItemCategory category) {
  return static_cast<int>(category);
}
}  // namespace

TEST_CASE("Ashley's live inventory counts item ids", "[items-slice][items]") {
  LiveItemsState state;
  std::copy(kUpperIds.begin(), kUpperIds.end(),
            std::begin(Items_itemCategoryUpperIds));
  items::InventoryContents counted{};
  counted.blades[3].id = 7;
  counted.misc[1] = {55, 9, 0};
  live::Commit(Audio_Seq_unusedInventory, counted);
  CHECK(items::CountLiveInventoryItem(7) == 1);
  CHECK(items::CountLiveInventoryItem(8) == 0);
  CHECK(items::CountLiveInventoryItem(55) == 9);
  CHECK(items::CountLiveInventoryItem(85) == 0);
  const auto saved = VS::State::LiveScriptFlag(1023);
  for (const auto [id, count] :
       {std::pair{7, 1}, std::pair{8, 0}, std::pair{55, 9}, std::pair{85, 0}}) {
    const std::array<std::uint8_t, 5> command{0, static_cast<std::uint8_t>(id),
                                              0, 0xFF, 3};
    CHECK(VS::Event::ScriptStoreInventoryItemPresence({command, {}, 0},
                                                      VS::Event::LiveScriptVm())
              .kind == VS::Event::ScriptFlowKind::kAdvance);
    CHECK(VS::State::LiveScriptFlag(1023) == count);
  }
  VS::State::SetLiveScriptFlag(1023, saved);
}

TEST_CASE("Ashley's live inventory consumes misc items from the invoked slot",
          "[items-slice][items]") {
  LiveItemsState state;
  live::Commit(Audio_Seq_unusedInventory, MakeInventory());
  Items_miscItemInvoked = 5;
  CHECK(items::ConsumeLiveMiscItem(0x150) == 1);
  CHECK(live::Typed(Audio_Seq_unusedInventory).misc[0] ==
        items::InventoryMisc{0x150, 6, 1});
  CHECK(Items_miscItemInvoked == 5);
  // An id below the invoked items rescans from the first slot.
  CHECK(items::ConsumeLiveMiscItem(0x100) == 0);
  CHECK(Items_miscItemInvoked == 0);
  CHECK(live::Typed(Audio_Seq_unusedInventory).misc[0].count == 6);
}

TEST_CASE("Ashley's live inventory refuses categories outside its tables",
          "[items-slice][items]") {
  LiveItemsState state;
  live::Commit(Audio_Seq_unusedInventory, MakeInventory());
  ItemsWarnings warnings;
  inv::ClearLiveItemSlot(8, 1);
  inv::ClearLiveItemSlot(-1, 1);
  CHECK(inv::CountLiveItems(8, nullptr) == 0);
  CHECK(inv::FindLiveFirstEmptySlot(-1, nullptr) == 0);
  // The manage row has no display order.
  CHECK(inv::FindLiveDisplayPosition(CategoryId(ItemCategory::kManage), 0) ==
        0);
  inv::RebuildLiveDisplayIndices(CategoryId(ItemCategory::kManage));
  inv::RebuildLiveDisplayIndices(8);
  CHECK(inv::CopyLiveItem(CopyFlags(ItemCategory::kBlade, true), nullptr, 0,
                          &Audio_Seq_unusedInventory) == 0);
  constexpr std::string_view kCategory =
      "item category outside the inventory tables";
  CHECK(warnings.messages == std::vector<std::string>{
                                 Refused("ClearItemSlot", kCategory),
                                 Refused("ClearItemSlot", kCategory),
                                 Refused("CountItems", kCategory),
                                 Refused("FirstEmptySlot", kCategory),
                                 Refused("FindDisplayPosition", kCategory),
                                 Refused("RebuildDisplayIndices", kCategory),
                                 Refused("RebuildDisplayIndices", kCategory),
                                 "CopyItem refused: null record",
                             });
  CHECK(live::Typed(Audio_Seq_unusedInventory) == MakeInventory());
  CHECK(live::Typed(Audio_Seq_unusedInventoryIndices) ==
        items::InventoryIndices{});
}

TEST_CASE("Ashley's live inventory counts and finds slots in any inventory",
          "[items-slice][items]") {
  LiveItemsState state;
  live::Commit(Audio_Seq_unusedInventory, MakeInventory());
  ItemsWarnings warnings;

  CHECK(inv::CountLiveItems(CategoryId(ItemCategory::kGem), nullptr) == 2);
  CHECK(inv::FindLiveFirstEmptySlot(CategoryId(ItemCategory::kGrip), nullptr) ==
        0);
  Items_Inventory full{};
  items::InventoryContents full_misc{};
  for (auto& stack : full_misc.misc) stack = {1, 1, 0};
  live::Commit(full, full_misc);
  CHECK(inv::CountLiveItems(CategoryId(ItemCategory::kMisc), &full) == 64);
  CHECK(inv::FindLiveFirstEmptySlot(CategoryId(ItemCategory::kMisc), &full) ==
        64);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Ashley's live inventory commits slot, display and copy changes",
          "[items-slice][items]") {
  LiveItemsState state;
  const auto inventory = MakeInventory();
  live::Commit(Audio_Seq_unusedInventory, inventory);
  ItemsWarnings warnings;

  inv::ClearLiveItemSlot(CategoryId(ItemCategory::kMisc), 1);
  auto expected = inventory;
  expected.misc[0] = Emptied<items::InventoryMisc>(1);
  CHECK(live::Typed(Audio_Seq_unusedInventory) == expected);

  inv::RebuildLiveDisplayIndices(CategoryId(ItemCategory::kGem));
  auto indices = items::InventoryIndices{};
  indices.gems[0] = 3;
  indices.gems[1] = 5;
  CHECK(live::Typed(Audio_Seq_unusedInventoryIndices) == indices);
  CHECK(inv::FindLiveDisplayPosition(CategoryId(ItemCategory::kGem), 4) == 2);
  CHECK(inv::FindLiveDisplayPosition(CategoryId(ItemCategory::kGem), 3) == 0);

  Items_Inventory other{};
  CHECK(inv::CopyLiveItem(CopyFlags(ItemCategory::kBlade, true, 5), &other, 0,
                          &Audio_Seq_unusedInventory) == 1);
  auto blade = Copied(inventory.blades[0], 1);
  blade.assembled_weapon_index = 5;
  CHECK(live::Typed(other).blades[0] == blade);
  CHECK(warnings.messages.empty());
}

TEST_CASE("A live misc copy into another inventory lists Ashley's stacks",
          "[items-slice][items]") {
  LiveItemsState state;
  live::Commit(Audio_Seq_unusedInventory, MakeInventory());
  ItemsWarnings warnings;
  Items_Inventory other{};
  CHECK(inv::CopyLiveItem(CopyFlags(ItemCategory::kMisc, true), &other, 0,
                          &Audio_Seq_unusedInventory) == 1);
  CHECK(live::Typed(other).misc[0] == items::InventoryMisc{0x150, 7, 0});
  CHECK(live::Typed(Audio_Seq_unusedInventoryIndices).misc[0] == 1);
  CHECK(live::Typed(Audio_Seq_unusedInventory) == MakeInventory());
  CHECK(warnings.messages.empty());
}

TEST_CASE("Ashley's live loadouts are refused without her battle actor",
          "[items-slice][items]") {
  LiveItemsState state;
  live::Commit(Audio_Seq_unusedInventory, MakeInventory());
  Items_InventoryArmor record{};
  live::Commit(record, MakeInventoryArmor(0x21));
  ItemsWarnings warnings;
  Battle_characterState = nullptr;
  inv::UnequipLiveWeapon();
  inv::UnequipLiveShield();
  inv::StoreLiveEquippedStats();
  items::EquipLiveAshleyArmor(2, &record);
  state.actor.stats = nullptr;
  Battle_characterState = &state.actor;
  items::EquipLiveAshleyAccessory(&record);
  constexpr std::string_view kActor = "actor outside the battle actor table";
  CHECK(warnings.messages == std::vector<std::string>{
                                 Refused("UnequipWeapon", kActor),
                                 Refused("UnequipShield", kActor),
                                 Refused("StoreEquippedStats", kActor),
                                 Refused("EquipAshleyArmor", kActor),
                                 Refused("EquipAshleyAccessory", kActor),
                             });
  CHECK(live::Typed(Audio_Seq_unusedInventory) == MakeInventory());
}

TEST_CASE("Unequipping Ashley's live shield clears every equipped mark",
          "[items-slice][items]") {
  LiveItemsState state;
  auto inventory = MakeInventory();
  live::Commit(Audio_Seq_unusedInventory, inventory);
  items::ShieldLoadout shield{};
  shield.base.id = 4;
  live::Commit(state.stats.shield, shield);
  FakeStatusHost host(0);
  CHECK(items::EquipAshleyShield(shield, nullptr, inventory, host) ==
        ItemsStatus::kOk);
  ItemsWarnings warnings;
  inv::UnequipLiveShield();
  CHECK(live::Typed(state.stats.shield) == shield);
  const auto unequipped = live::Typed(Audio_Seq_unusedInventory);
  for (const auto& entry : unequipped.shields) CHECK(entry.is_equipped == 0);
  CHECK(unequipped.weapons == inventory.weapons);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Ashley's live armour and accessory follow the equip rules",
          "[items-slice][items]") {
  LiveItemsState state;
  Items_InventoryArmor record{};
  live::Commit(record, MakeInventoryArmor(0x21));
  ItemsWarnings warnings;
  items::EquipLiveAshleyArmor(2, &record);
  CHECK(live::Typed(state.stats.limbs[2].armor) ==
        ExpectedInventoryArmor(0x21));
  CHECK(live::Typed(state.stats.limbs[1].armor) == items::ArmorLoadout{});
  items::EquipLiveAshleyArmor(6, &record);
  items::EquipLiveAshleyAccessory(&record);
  CHECK(live::Typed(state.stats.accessory) == ExpectedInventoryAccessory(0x21));

  auto rebuilt = live::Typed(state.stats.limbs[2].armor);
  items::RebuildArmorClassAffinity(rebuilt);
  items::RebuildLiveArmorClassAffinity(&state.stats, 2);
  CHECK(live::Typed(state.stats.limbs[2].armor) == rebuilt);
  items::RebuildLiveArmorClassAffinity(&state.stats, 6);
  items::RebuildLiveArmorClassAffinity(&state.stats, -1);
  items::RebuildLiveArmorClassAffinity(nullptr, 0);
  items::WriteLiveInventoryArmorStats(nullptr, nullptr);
  constexpr std::string_view kBodyPart = "body part outside the limb table";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("EquipAshleyArmor", kBodyPart),
            Refused("RebuildArmorClassAffinity", kBodyPart),
            Refused("RebuildArmorClassAffinity", kBodyPart),
            "RebuildArmorClassAffinity refused: null record",
            "WriteInventoryArmorStats refused: null record",
        });
}

TEST_CASE("The live item recovery reads the action's hit stat factor",
          "[items-slice][items]") {
  ItemsWarnings warnings;
  Battle_ActionDefinition action{};
  action.hits[0].statFactor = 3;
  action.hits[1].statFactor = 31;
  CHECK(items::CalculateLiveItemRecovery(&action, nullptr, nullptr, 0, 0) ==
        15);
  CHECK(items::CalculateLiveItemRecovery(&action, nullptr, nullptr, 1, 0) ==
        155);
  CHECK(items::CalculateLiveItemRecovery(&action, nullptr, nullptr, 2, 0) == 0);
  CHECK(items::CalculateLiveItemRecovery(nullptr, nullptr, nullptr, 0, 0) == 0);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("CalculateItemRecovery",
                    "hit index outside the action's two hits"),
            "CalculateItemRecovery refused: null record",
        });
}

namespace {
struct LiveRoomChests {
  std::array<Items_TreasureChest, 3> legacy{};
  items::BattleLoot loot{};
  Items_BattleLoot section{};

  LiveRoomChests() {
    const std::array<items::TreasureChest, 3> typed{
        Chest({1, 2, 0, 0}), Chest({1, 2, 1, 0}), Chest({5, 6, 1, 0})};
    for (std::size_t i = 0; i < legacy.size(); ++i) {
      live::Commit(legacy[i], typed[i]);
    }
    Field_roomData.containers = legacy.data();
    Field_roomData.header.containersLength =
        static_cast<u_int>(sizeof(Items_TreasureChest) * 2 + 5);
    loot.misc[0] = {0x150, items::kLootDropped, 2};
    live::Commit(section, loot);
  }
};
}  // namespace

TEST_CASE("The live room chests are the room's whole chest records",
          "[items-slice][items]") {
  LiveItemsState state;
  LiveRoomChests room;
  ItemsWarnings warnings;

  int count = -1;
  CHECK(items::LiveRoomTreasureChests(&count) == room.legacy.data());
  CHECK(count == 2);
  CHECK(items::FindLiveTreasureChestAtTile(1, 2, 1) == &room.legacy[1]);
  // The third record lies past the room's chest section.
  CHECK(items::FindLiveTreasureChestAtTile(5, 6, 1) == nullptr);

  items::LoadLiveRoomChestLoot(&room.section);
  CHECK(live::Typed(room.legacy[0]).loot == room.loot);
  CHECK(live::Typed(room.legacy[1]).loot == items::BattleLoot{});
  CHECK(warnings.messages.empty());
}

TEST_CASE("A live room without a whole chest record refuses chest loot",
          "[items-slice][items]") {
  LiveItemsState state;
  LiveRoomChests room;
  auto& section = room.section;
  ItemsWarnings warnings;
  int count = -1;

  // A section shorter than one chest refuses the loot.
  Field_roomData.header.containersLength =
      static_cast<u_int>(sizeof(Items_TreasureChest) - 1);
  CHECK(items::LiveRoomTreasureChests(&count) == room.legacy.data());
  CHECK(count == 0);
  items::LoadLiveRoomChestLoot(&section);
  // A room without a chest section ignores it.
  Field_roomData.containers = nullptr;
  CHECK(items::LiveRoomTreasureChests(&count) == nullptr);
  CHECK(count == 0);
  CHECK(items::FindLiveTreasureChestAtTile(1, 2, 0) == nullptr);
  items::LoadLiveRoomChestLoot(&section);

  CHECK(items::LiveRoomTreasureChests(nullptr) == nullptr);
  items::LoadLiveRoomChestLoot(nullptr);
  items::QueueLiveEnemyLoot(nullptr);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("LoadRoomChestLoot", "room has no treasure chest"),
            "RoomTreasureChests refused: null record",
            "LoadRoomChestLoot refused: null record",
            "QueueEnemyLoot refused: null record",
        });
}

TEST_CASE("Items native bindings retire the module entries they replace",
          "[items-slice][items][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  // The equipment, inventory and loot rules are typed C++ now; no BATTLE.PRG
  // image or table stores these addresses.
  constexpr std::array<unsigned, 54> kRetired{
      0x8006A334u, 0x8006A65Cu, 0x8006A8ECu, 0x8006A9F0u, 0x8006AB44u,
      0x8006AC74u, 0x8006ACFCu, 0x8006AE0Cu, 0x8006AEACu, 0x8006B02Cu,
      0x8006B110u, 0x8006B194u, 0x8006B214u, 0x8006B270u, 0x8006B2D4u,
      0x8006B338u, 0x8006B460u, 0x8006B4B8u, 0x8006B524u, 0x8006B57Cu,
      0x8006B6ACu, 0x8006B728u, 0x8006B7BCu, 0x8006B8C0u, 0x8006B9E0u,
      0x8006BAA8u, 0x8006BADCu, 0x8006BB0Cu, 0x8006BBECu, 0x8006BCB0u,
      0x8006BD14u, 0x8006BDA0u, 0x8006BDF0u, 0x8006BE64u, 0x8006FB48u,
      0x8007BA98u, 0x8007E200u, 0x8007E2ACu, 0x8007E380u, 0x8007E454u,
      0x8007FF8Cu, 0x800802C4u, 0x800803A4u, 0x80080534u, 0x800807E8u,
      0x80080A9Cu, 0x80080C9Cu, 0x80080F78u, 0x8008A6FCu, 0x8008D508u,
      0x8008D710u, 0x8008E370u, 0x8008E6A8u, 0x800CCB9Cu};
  for (const auto address : kRetired) {
    INFO("address " << address);
    CHECK(resolve(address) == nullptr);
  }
  // Battle_statCalculators[15] still stores the item recovery entry.
  CHECK(resolve(0x80083430u) ==
        reinterpret_cast<void*>(&items::CalculateLiveItemRecovery));
  CHECK(resolve(0x800F1900u) == static_cast<void*>(&Items_lootListHead));
  CHECK(resolve(0x800F5210u) == static_cast<void*>(&Items_miscItemInvoked));
}
