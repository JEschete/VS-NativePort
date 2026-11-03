#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"

// Loot: what defeated enemies and scripts drop, the loot list the reward menu
// shows, and the treasure chests of a room's MPD loot section.
namespace VS::Items {
// Draws are in [0, kDropRateDraw); an item drops when the draw is below its
// drop rate, so a rate of 255 always drops.
inline constexpr std::int32_t kDropRateDraw = 0xFF;
// The dropper id of loot a script presents.
inline constexpr std::int32_t kScriptedLootDropper = 0xFF;
// The reward stage (script flag 0xAB) past which defeated enemies' loot is
// offered instead of waiting for movement to settle.
inline constexpr std::int32_t kEnemyLootRewardStage = 2;
// A chest's opened state is script flag kChestOpenedFlagBase + opened_flag.
inline constexpr std::int32_t kChestOpenedFlagBase = 0x3C0;
// The door passability an open chest leaves on its tile, and a closed one.
inline constexpr std::uint8_t kOpenChestPassability = 5;
inline constexpr std::uint8_t kClosedChestPassability = 0;

// What a defeated enemy can drop (decomp: vs_battle_actorStats).
struct EnemyDrops {
  std::int32_t actor_id{};
  WeaponLoadout weapon{};
  ShieldLoadout shield{};
  AccessorySpawnLoadout accessory{};
  std::array<ArmorLoadout, kLimbCount> limb_armor{};
  std::array<MiscItemStack, kGuaranteedDropCount> guaranteed_items{};
  MiscItemDrop random_item{};
};

// The loot list the reward menu pages through (decomp: _lootListHead). An
// entry is reserved before its loot is rolled and is then either pushed or
// released.
class LootList {
 public:
  LootList() = default;
  LootList(const LootList&) = delete;
  LootList& operator=(const LootList&) = delete;
  LootList(LootList&&) = delete;
  LootList& operator=(LootList&&) = delete;
  virtual ~LootList() = default;

  // Takes a node for a new entry; false when there is no room for one.
  [[nodiscard]] virtual bool ReserveEntry() = 0;
  // Puts the reserved node, holding `loot`, at the head of the list.
  virtual void PushReservedEntry(std::int32_t dropper,
                                 const BattleLoot& loot) = 0;
  // Gives the reserved node back.
  virtual void ReleaseReservedEntry() = 0;
  [[nodiscard]] virtual bool HasEntryFrom(std::int32_t dropper) const = 0;
  [[nodiscard]] virtual bool IsEmpty() const = 0;
  // Gives every entry back.
  virtual void Clear() = 0;
};

// The battle flow steps around the loot menu.
class LootFlowHost {
 public:
  LootFlowHost() = default;
  LootFlowHost(const LootFlowHost&) = delete;
  LootFlowHost& operator=(const LootFlowHost&) = delete;
  LootFlowHost(LootFlowHost&&) = delete;
  LootFlowHost& operator=(LootFlowHost&&) = delete;
  virtual ~LootFlowHost() = default;

  // Script flag 0xAB.
  [[nodiscard]] virtual std::int32_t RewardStage() = 0;
  // Opens the loot list over the paused field (action flow mode 12).
  virtual void OpenEnemyLootList() = 0;
  virtual void BeginPendingAbilityReward() = 0;
  virtual void WaitForPostActionMovement() = 0;
  // Opens the loot list in the reward pause (action flow mode 4).
  virtual void OpenScriptedLootList() = 0;
  virtual void ResumeAfterRewards() = 0;
  virtual void ResetLootPaging() = 0;
};

// The room state a chest's opened flag drives.
class ChestStateHost {
 public:
  ChestStateHost() = default;
  ChestStateHost(const ChestStateHost&) = delete;
  ChestStateHost& operator=(const ChestStateHost&) = delete;
  ChestStateHost(ChestStateHost&&) = delete;
  ChestStateHost& operator=(ChestStateHost&&) = delete;
  virtual ~ChestStateHost() = default;

  [[nodiscard]] virtual bool ScriptFlag(std::int16_t flag) = 0;
  // Settles the lid's tilt effect at its resting (open) angle.
  virtual void FinishLidRotation(std::int16_t lid_rotation_id) = 0;
  // Sets the door passability of the chest's tile, if the room has one.
  virtual void SetTilePassability(const TreasureChest& chest,
                                  std::uint8_t passability) = 0;
};

// Each roll draws once and, on a drop, copies the enemy's item into the loot
// and marks it dropped.
[[nodiscard]] bool RollEnemyWeaponDrop(LootedWeapon& looted,
                                       const WeaponLoadout& base,
                                       ItemRandom& random);
[[nodiscard]] bool RollEnemyShieldDrop(LootedShield& looted,
                                       const ShieldLoadout& base,
                                       ItemRandom& random);
[[nodiscard]] bool RollEnemyAccessoryDrop(LootedArmor& looted,
                                          const AccessorySpawnLoadout& base,
                                          ItemRandom& random);
// A dropped armour fills the first empty looted-armour slot; with both full
// it is lost but still counts as a drop roll that failed.
[[nodiscard]] bool RollEnemyArmorDrop(
    std::span<LootedArmor, kLootArmorSlots> looted, const ArmorLoadout& base,
    ItemRandom& random);
// Copies both guaranteed items (item slots 0-1) without drawing.
[[nodiscard]] bool CopyGuaranteedEnemyItemDrops(
    BattleLoot& looted,
    std::span<const MiscItemStack, kGuaranteedDropCount> base);
// The random item takes item slot 2; an absent item draws nothing.
[[nodiscard]] bool RollEnemyItemDrop(BattleLoot& looted,
                                     const MiscItemDrop& base,
                                     ItemRandom& random);

// Rolls everything `enemy` can drop and lists it once per enemy.
void QueueEnemyLoot(const EnemyDrops& enemy, LootList& list,
                    ItemRandom& random);
// Once the reward stage allows it, offers the listed enemy loot (or the
// pending ability reward); until then waits for movement to settle.
void OpenPendingEnemyLoot(const LootList& list, LootFlowHost& flow);
// Lists the script's items (an id of 0 is no item), then offers the loot list
// or resumes after the rewards when it is empty.
void PresentScriptedLoot(
    std::span<const MiscItemStack, kGuaranteedDropCount> items, LootList& list,
    LootFlowHost& flow);
void ClearRoomLootList(LootList& list, LootFlowHost& flow);

// The first chest standing on the tile.
[[nodiscard]] std::optional<std::size_t> TreasureChestAtTile(
    std::span<const TreasureChest> chests, std::int32_t tile_x,
    std::int32_t tile_z, std::int32_t tile_layer);
// Opens or closes each chest (and its tile) to match its opened flag.
void RestoreRoomChestStates(std::span<TreasureChest> chests,
                            ChestStateHost& host);
// The whole chest records in a room's loot section; none without a section.
[[nodiscard]] std::size_t RoomTreasureChests(
    std::optional<std::uint32_t> section_bytes);
// The section's loot belongs to the first chest; a room without a chest
// refuses it with kNoChests.
[[nodiscard]] ItemsStatus LoadRoomChestLoot(std::span<TreasureChest> chests,
                                            const BattleLoot& loot);
}  // namespace VS::Items
