#pragma once
#include "Items/ItemRecords.hpp"
// Hand-maintained (tools/port/owned.csv).

struct Battle_Actor;

// Binds the loot rules (Loot.hpp) to the battle loot list
// (VS::Items::lootListHead, whose nodes live on the game heap), the battle flow
// around the loot menu and the treasure chests of the loaded room
// (Field_roomData), in the shapes the copied callers use.
namespace VS::Items {
// Rolls the drops of a defeated enemy (its battle actor statistics) into a
// new loot list entry.
void QueueLiveEnemyLoot(const Battle_Actor* enemy);
void OpenLivePendingEnemyLoot();
// A script's two items: an id whose low 16 bits are 0 is no item.
void PresentLiveScriptedLoot(int id0, int count0, int id1, int count1);
void ClearLiveRoomLootList();

// The chest standing on the tile, or null.
VS::Items::TreasureChest* FindLiveTreasureChestAtTile(int tile_x, int tile_z,
                                                      int tile_layer);
void RestoreLiveRoomChestStates();
// The room's chests and their count; null and 0 without a loot section.
VS::Items::TreasureChest* LiveRoomTreasureChests(int* chest_count_out);
// Copies a room's MPD loot section (one VS::Items::BattleLoot) into its first
// chest.
void LoadLiveRoomChestLoot(const void* loot_section);
}  // namespace VS::Items
