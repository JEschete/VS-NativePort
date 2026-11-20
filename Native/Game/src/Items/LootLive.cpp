// Hand-maintained (tools/port/owned.csv).
// Live binding for the loot rules (Loot.hpp): the battle loot list
// (VS::Items::lootListHead, nodes on the game heap), the battle flow around the
// loot menu and the treasure chests of the loaded room (Field_roomData).
#include "Items/LootLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <optional>
#include <span>
#include <utility>

#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Battle/Rewards.h"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Collision.h"
#include "Field/CollisionMapLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/Gimmicks.h"
#include "Field/Interactions.hpp"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomEffectsLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Items/Loot.hpp"
#include "Items/LootLiveState.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Loot/Paging.hpp"
#include "State/ScriptFlagsLive.hpp"

// decomp: _lootListHead (_lootListHead@BATTLE/BATTLE.PRG)
VS::Items::LootListNode* VS::Items::lootListHead = nullptr;

namespace VS::Items {
namespace {
using Live::WarnNullRecord;
using Live::WarnRefusal;

// The script flag holding the reward stage (LootFlowHost::RewardStage).
constexpr std::int16_t kRewardStageFlag = 0xAB;
// VS::Field::controlMode while a menu has the pad.
constexpr std::uint32_t kMenuControlMode = 2;
// Battle_actionFlowMode while the enemy loot list, and the scripted one, is
// open.
constexpr std::uint32_t kEnemyLootFlowMode = 12;
constexpr std::uint32_t kScriptedLootFlowMode = 4;

// VS::Items::lootListHead's nodes. The reserved node is a game heap block that
// is not yet listed; it is pushed or freed before the list is dropped.
class LiveLootList final : public LootList {
 public:
  LiveLootList() = default;
  LiveLootList(const LiveLootList&) = delete;
  LiveLootList& operator=(const LiveLootList&) = delete;
  LiveLootList(LiveLootList&&) = delete;
  LiveLootList& operator=(LiveLootList&&) = delete;
  ~LiveLootList() override { FreeReserved(); }

  bool ReserveEntry() override {
    FreeReserved();
    void* const block =
        Core::Memory::AllocateLiveFromHighEnd(sizeof(VS::Items::LootListNode));
    if (block == nullptr) return false;
    reserved_ = ::new (block) VS::Items::LootListNode{};
    return true;
  }

  void PushReservedEntry(std::int32_t dropper,
                         const BattleLoot& loot) override {
    if (reserved_ == nullptr) {
      WarnNullRecord("PushReservedLootEntry");
      return;
    }
    reserved_->loot = loot;
    reserved_->actor_id = dropper;
    reserved_->next = lootListHead;
    lootListHead = std::exchange(reserved_, nullptr);
  }

  void ReleaseReservedEntry() override { FreeReserved(); }

  bool HasEntryFrom(std::int32_t dropper) const override {
    for (const auto* node = lootListHead; node != nullptr; node = node->next) {
      if (node->actor_id == dropper) return true;
    }
    return false;
  }

  bool IsEmpty() const override { return lootListHead == nullptr; }

  void Clear() override {
    auto* node = std::exchange(lootListHead, nullptr);
    while (node != nullptr) {
      auto* const next = node->next;
      Core::Memory::FreeLiveFromHighEnd(node);
      node = next;
    }
  }

 private:
  void FreeReserved() {
    if (reserved_ == nullptr) return;
    Core::Memory::FreeLiveFromHighEnd(std::exchange(reserved_, nullptr));
  }

  VS::Items::LootListNode* reserved_ = nullptr;
};

class LiveLootFlowHost final : public LootFlowHost {
 public:
  // func_8006FB48.s compares the flag as an unsigned byte (andi 0xFF; sltiu
  // 3).
  std::int32_t RewardStage() override {
    return static_cast<std::uint8_t>(
        VS::State::LiveScriptFlag(kRewardStageFlag));
  }
  void OpenEnemyLootList() override {
    VS::Menu::Framework::OpenLiveLootListMenu(lootListHead);
    VS::Field::controlMode = kMenuControlMode;
    Battle_actionFlowMode = kEnemyLootFlowMode;
    VS::Field::LivePauseFieldForMenu();
  }
  void BeginPendingAbilityReward() override {
    VS::Battle::LiveBeginPendingAbilityReward();
  }
  void WaitForPostActionMovement() override {
    VS::Battle::WaitForLivePostActionMovement();
  }
  void OpenScriptedLootList() override {
    Battle_lootMenuOpened = 1;
    VS::Menu::Framework::OpenLiveLootListMenu(lootListHead);
    Battle_actionFlowMode = kScriptedLootFlowMode;
    VS::Battle::BeginLiveRewardPause();
  }
  void ResumeAfterRewards() override { VS::Battle::ResumeLiveAfterRewards(); }
  void ResetLootPaging() override { VS::Menu::Loot::Live::ResetPaging(); }
};

class LiveChestStateHost final : public ChestStateHost {
 public:
  bool ScriptFlag(std::int16_t flag) override {
    return VS::State::LiveScriptFlag(flag) != 0;
  }
  void FinishLidRotation(std::int16_t lid_rotation_id) override {
    VS::Field::LiveFinishRoomTiltRotation(lid_rotation_id);
  }
  // The original looked the cell up (a pure lookup) before reading the flag.
  void SetTilePassability(const TreasureChest& chest,
                          std::uint8_t passability) override {
    auto* const cell = VS::Field::LiveCollisionCellAt(
        static_cast<std::uint32_t>(chest.tile_x),
        static_cast<std::uint32_t>(chest.tile_z), chest.layer);
    if (cell != nullptr) cell->passability = passability;
  }
};

std::optional<EnemyDrops> ToEnemyDrops(const Battle_Actor* enemy) {
  if (enemy == nullptr || enemy->stats == nullptr) return std::nullopt;
  const auto& stats = *enemy->stats;
  EnemyDrops drops{};
  drops.actor_id = enemy->id;
  drops.weapon = stats.weapon;
  drops.shield = stats.shield;
  drops.accessory = stats.accessory;
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    drops.limb_armor[i] = stats.limbs[i].armor;
  }
  drops.guaranteed_items = {stats.dropItem, stats.secondDropItem};
  drops.random_item = stats.randomDropItem;
  return drops;
}

// The room's chest records; none without a loot section.
std::span<TreasureChest> LiveRoomChestRecords() {
  if (Field_roomData.containers == nullptr) return {};
  return {Field_roomData.containers,
          RoomTreasureChests(Field_roomData.header.containersLength)};
}
}  // namespace

// decomp: BATTLE.PRG _dropLoot (src/BATTLE/BATTLE.PRG/146C.c:1720)
void QueueLiveEnemyLoot(const Battle_Actor* enemy) {
  const auto drops = ToEnemyDrops(enemy);
  if (!drops) {
    WarnNullRecord("QueueEnemyLoot");
    return;
  }
  LiveLootList list;
  Live::LiveItemRandom random;
  QueueEnemyLoot(*drops, list, random);
}

// decomp: BATTLE.PRG func_8006FB48 (src/BATTLE/BATTLE.PRG/146C.c:3372)
void OpenLivePendingEnemyLoot() {
  const LiveLootList list;
  LiveLootFlowHost flow;
  OpenPendingEnemyLoot(list, flow);
}

// decomp: BATTLE.PRG func_8007BA98 (src/BATTLE/BATTLE.PRG/146C.c:8470)
// The original stored the id's low 16 bits and the count's low byte.
void PresentLiveScriptedLoot(int id0, int count0, int id1, int count1) {
  const std::array<MiscItemStack, kGuaranteedDropCount> items{{
      {static_cast<std::uint16_t>(id0), static_cast<std::uint8_t>(count0)},
      {static_cast<std::uint16_t>(id1), static_cast<std::uint8_t>(count1)},
  }};
  LiveLootList list;
  LiveLootFlowHost flow;
  PresentScriptedLoot(items, list, flow);
}

// decomp: BATTLE.PRG func_8008A6FC (src/BATTLE/BATTLE.PRG/146C.c:15596)
void ClearLiveRoomLootList() {
  LiveLootList list;
  LiveLootFlowHost flow;
  ClearRoomLootList(list, flow);
}

// decomp: BATTLE.PRG func_8008D508 (src/BATTLE/BATTLE.PRG/146C.c:16832)
TreasureChest* FindLiveTreasureChestAtTile(int tile_x, int tile_z,
                                           int tile_layer) {
  const auto chests = LiveRoomChestRecords();
  const auto index = TreasureChestAtTile(chests, tile_x, tile_z, tile_layer);
  return index ? &chests[*index] : nullptr;
}

// decomp: BATTLE.PRG func_8008D710 (src/BATTLE/BATTLE.PRG/146C.c:16903)
void RestoreLiveRoomChestStates() {
  LiveChestStateHost host;
  RestoreRoomChestStates(LiveRoomChestRecords(), host);
}

// decomp: BATTLE.PRG func_8008E370 (src/BATTLE/BATTLE.PRG/146C.c:17285)
// A room with a chest section of less than one record keeps its (non-null)
// records with a count of 0, as the original returned.
TreasureChest* LiveRoomTreasureChests(int* chest_count_out) {
  if (chest_count_out == nullptr) {
    WarnNullRecord("RoomTreasureChests");
    return nullptr;
  }
  const auto chests = LiveRoomChestRecords();
  *chest_count_out = static_cast<int>(chests.size());
  return chests.data();
}

// decomp: BATTLE.PRG _loadMpdLootSection (src/BATTLE/BATTLE.PRG/146C.c:17389)
void LoadLiveRoomChestLoot(const void* loot_section) {
  if (loot_section == nullptr) {
    WarnNullRecord("LoadRoomChestLoot");
    return;
  }
  BattleLoot section{};
  std::memcpy(&section, loot_section, sizeof section);
  const auto status = LoadRoomChestLoot(LiveRoomChestRecords(), section);
  if (status != ItemsStatus::kOk) {
    // A room without a chest section ignores the loot, as the original did;
    // one too short for a chest refuses it.
    if (Field_roomData.containers != nullptr) {
      WarnRefusal("LoadRoomChestLoot", status);
    }
  }
}
}  // namespace VS::Items
