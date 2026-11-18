#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy view of the battle loot list that the copied code reads and the
// loot menu pages through. LootLive.cpp defines it and is the only code that
// adds or frees its nodes (Loot.hpp LootList).

struct Items_LootListNode;

// decomp: _lootListHead (BATTLE/BATTLE.PRG 146C.h): the newest entry; each
// node is a game heap block.
extern Items_LootListNode* Items_lootListHead;
