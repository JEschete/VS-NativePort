#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Core/Random.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Abilities.h"
#include "Battle/ActionEffects.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/AttackPrerequisites.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/HitChance.h"
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Core/Random.h"
#include "Effects/EffectResources.h"
#include "Effects/PurgeTables.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/Ring.h"
#include "Field/Interactions.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loot.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Loot/LootInventory.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Loot/Paging.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Flags.h"
#include "State/Progress.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
#include "State/Timers.h"
}

extern "C" int Tests_CombatTargetResults(void);

namespace {
// Original named action definitions, SLUS_010.40 file offset 0x3C1DC.
constexpr std::array<std::pair<int, int>, 28> kAbilityCosts{{
    {22, 1}, {23, 2}, {24, 1}, {25, 2}, {26, 1}, {27, 1}, {28, 2},
    {29, 2}, {30, 1}, {31, 3}, {32, 3}, {34, 3}, {35, 3}, {37, 3},
    {40, 1}, {41, 6}, {42, 2}, {43, 2}, {44, 4}, {45, 4}, {46, 4},
    {47, 4}, {48, 4}, {49, 4}, {50, 4}, {51, 4}, {52, 4}, {53, 6},
}};

// SLUS_010.40 _artsPointsRequirements / _battleAbilitiesPointsRequirements,
// file offsets 0x405E4 / 0x4065C.
constexpr std::array<std::array<u_short, 4>, 10> kBreakArtThresholds{{
    {10, 65, 175, 340},
    {20, 90, 235, 425},
    {25, 110, 260, 485},
    {18, 80, 210, 420},
    {20, 100, 245, 465},
    {15, 90, 215, 410},
    {20, 95, 205, 385},
    {15, 95, 220, 405},
    {20, 95, 230, 430},
    {30, 105, 250, 460},
}};
constexpr std::array<u_short, 22> kAbilityThresholds{{
    20,  54,   72,   95,   125,  165,  216,  283,  369,  482,  628,
    818, 1066, 1387, 1805, 2349, 3055, 3973, 5167, 6719, 8737, 11359,
}};

struct AbilityFixture {
  decltype(std::to_array(Battle_actionDefinitions)) saved_definitions =
      std::to_array(Battle_actionDefinitions);
  Battle_ActionState* saved_action = Battle_actionState;
  Battle_Actor* saved_character = Battle_characterState;
  std::array<Battle_Actor*, 2> saved_actors{Battle_actors[0], Battle_actors[1]};
  std::array<Actors_ActorRecord*, 2> saved_records{Actors_actorRecords[0],
                                                   Actors_actorRecords[1]};
  State_Settings saved_settings = Input_settings;
  decltype(Input_buttonsPressed) saved_pressed = Input_buttonsPressed;
  decltype(Audio_Seq_unusedScoredata) saved_score = Audio_Seq_unusedScoredata;
  decltype(Battle_floatingLabels) saved_labels = Battle_floatingLabels;
  decltype(Battle_floatingLabelCount) saved_label_count =
      Battle_floatingLabelCount;
  decltype(Battle_abilityInputResult) saved_input_result =
      Battle_abilityInputResult;
  u_int saved_actor_flags = Actors_stateFlagsScratch;
  int saved_tick_speed = State_tickSpeed;
  char saved_difficulty = State_ScriptFlag(1);
  Battle_ActionState action{};
  Battle_Actor actors[2]{};
  Battle_ActorStats stats[2]{};
  Actors_ActorRecord records[2]{};
  Battle_FloatingLabel labels[32]{};

  AbilityFixture() {
    Battle_actionState = &action;
    Battle_characterState = &actors[0];
    for (int index = 0; index < 2; ++index) {
      Battle_actors[index] = &actors[index];
      Actors_actorRecords[index] = &records[index];
      actors[index].id = index;
      actors[index].stats = &stats[index];
      actors[index].roomSpawnIndex = 1;
      stats[index].actorId = index;
      stats[index].currentHp = 600;
      stats[index].currentMp = 120;
      records[index].instance.actionAnimationState = 1;
    }
    std::fill_n(Battle_actionDefinitions, saved_definitions.size(),
                Battle_ActionDefinition{});
    for (const auto& [id, cost] : kAbilityCosts) {
      Battle_actionDefinitions[id].type = Battle_ActionType_Ability;
      Battle_actionDefinitions[id].cost = cost;
      Battle_actionDefinitions[id].unlocked = 1;
    }
    Battle_actionDefinitions[1].type = Battle_ActionType_BaseAttack;
    Battle_actionDefinitions[54].type = Battle_ActionType_Spell;
    Battle_actionDefinitions[99].type = Battle_ActionType_Spell;
    Battle_actionDefinitions[141].type = Battle_ActionType_BreakArt;
    Input_settings = {};
    for (int index = 0; index < 3; ++index) {
      Input_settings.mappedChainAbilities[index] =
          static_cast<char>(22 + index);
      Input_settings.mappedDefenseAbilities[index] =
          static_cast<char>(40 + index);
    }
    Input_buttonsPressed.allButtons = PADRright;
    Battle_floatingLabels = &labels;
    Battle_floatingLabelCount = 0;
    Battle_abilityInputResult = 1;
    State_tickSpeed = 2;
    State_SetScriptFlag(1, 0);
    Prepare();
  }

  ~AbilityFixture() {
    std::copy(saved_definitions.begin(), saved_definitions.end(),
              Battle_actionDefinitions);
    Battle_actionState = saved_action;
    Battle_characterState = saved_character;
    for (int index = 0; index < 2; ++index) {
      Battle_actors[index] = saved_actors[index];
      Actors_actorRecords[index] = saved_records[index];
    }
    Input_settings = saved_settings;
    Input_buttonsPressed = saved_pressed;
    Audio_Seq_unusedScoredata = saved_score;
    Battle_floatingLabels = saved_labels;
    Battle_floatingLabelCount = saved_label_count;
    Battle_abilityInputResult = saved_input_result;
    Actors_stateFlagsScratch = saved_actor_flags;
    State_tickSpeed = saved_tick_speed;
    State_SetScriptFlag(1, saved_difficulty);
  }

  void Prepare(bool defense = false, int action_id = 1, unsigned index = 0) {
    action = {};
    action.queueIndex = index;
    auto& request = Request();
    request.actionId = action_id;
    request.primaryTarget.target.targetActor = defense ? 1 : 0;
    request.targetCount = 1;
    request.targets[0].target.targetActor = defense ? 0 : 1;
    action.abilityWindow = 100;
  }

  Battle_ActionRequest& Request() {
    return action.queue[action.queueIndex & 3];
  }

  void CheckInputResult(int success, int slot) const {
    CHECK(action.abilityInputSucceeded == success);
    CHECK(action.chainAbilitySlot == slot);
    CHECK(action.previousChainInput == slot);
  }
};

using AbilityEffect = int (*)(Battle_ActionDefinition*, Battle_HitResult*,
                              Battle_HitResult*, int, int);

struct AbilityHit {
  int calculator;
  int factor;
  AbilityEffect apply;
  bool self = false;
};

struct AbilityEffectFixture : AbilityFixture {
  decltype(std::to_array(Battle_statCalculators)) saved_calculators =
      std::to_array(Battle_statCalculators);
  decltype(std::to_array(Battle_limbArmorDpLoss)) saved_limb_damage =
      std::to_array(Battle_limbArmorDpLoss);
  decltype(std::to_array(Battle_statusEffectParams)) saved_status_params =
      std::to_array(Battle_statusEffectParams);
  std::array<Battle_Ai_ActorAiState*, 2> saved_ai{Battle_Ai_actorAiStates[0],
                                                  Battle_Ai_actorAiStates[1]};
  u_int saved_last_value = Battle_lastActionStatValue;
  int saved_critical = Battle_criticalHitOccurred;
  Battle_Ai_ActorAiState ai[2]{};
  Battle_HitResult source{};
  Battle_HitResult target{};

  AbilityEffectFixture() {
    action.queueIndex = 3;
    action.selection.actionId = 99;
    action.selection.primaryTarget.hpEffect = 1;
    action.selection.primaryTarget.target.hp = 101;
    action.selection.primaryTarget.target.mp = 9;
    action.selection.targets[0].target.hp = 83;
    action.selection.targets[0].target.mp = 18;
    Battle_actionDefinitions[99].type = Battle_ActionType_Spell;
    Battle_actionDefinitions[99].cost = 25;
    Battle_actionDefinitions[99].unlocked = 1;
    Battle_InitializeHitResult(&source);
    Battle_InitializeHitResult(&target);
    target.target.targetActor = 1;
    for (int index = 0; index < 2; ++index) {
      Battle_Ai_actorAiStates[index] = &ai[index];
      auto& actor = stats[index];
      actor.maxHp = 1000;
      actor.maxMp = 300;
      actor.currentHp = index == 0 ? 600 : 800;
      actor.currentMp = index == 0 ? 100 : 200;
      actor.walkSpeed = actor.baseWalkSpeed = 8;
      actor.runSpeed = actor.baseRunSpeed = 12;
      actor.weapon.blade.id = 1;
      actor.weapon.currentDp = 100;
      actor.weapon.maxDp = 1000;
      actor.weapon.currentPp = 70;
      actor.weapon.maxPp = 255;
      actor.shield.base.id = 1;
      actor.shield.currentPp = 90;
      actor.shield.maxPp = 255;
      for (int limb = 0; limb < 6; ++limb) {
        actor.limbs[limb].hp = 50;
        actor.limbs[limb].maxHp = 100;
        actor.dpSplash[limb][limb] = 100;
      }
    }
    BindCalculators();
    Battle_statusEffectParams[12] = {0x1000, 0, 0, 0x1000, 0};
    Battle_statusEffectParams[13] = {0x2000, 15, 0, 0x2000, 0};
    Battle_statusEffectParams[14] = {0x4000, 0, 2, 0x20000, 0};
    Battle_statusEffectParams[15] = {0x8000, 15, 0, 0x8800, 0x10};
  }

  ~AbilityEffectFixture() {
    std::copy(saved_calculators.begin(), saved_calculators.end(),
              Battle_statCalculators);
    std::copy(saved_limb_damage.begin(), saved_limb_damage.end(),
              Battle_limbArmorDpLoss);
    std::copy(saved_status_params.begin(), saved_status_params.end(),
              Battle_statusEffectParams);
    Battle_lastActionStatValue = saved_last_value;
    Battle_criticalHitOccurred = saved_critical;
    for (int index = 0; index < 2; ++index) {
      Battle_Ai_actorAiStates[index] = saved_ai[index];
    }
  }

  static void BindCalculators() {
    // BATTLE.PRG _statCalculators, original pointer table at file 0x7FB0C.
    Battle_statCalculators[1] = Battle_LastActionStatValue;
    Battle_statCalculators[4] = Battle_ScalePreviousTargetHit;
    Battle_statCalculators[5] = Battle_ScaleSourceMissingHp;
    Battle_statCalculators[6] = Battle_ScaleTargetCurrentHp;
    Battle_statCalculators[8] = Battle_WeaponPpWithChainOffset;
    Battle_statCalculators[9] = Battle_ScaleIncomingSpellDamage;
    Battle_statCalculators[10] = Battle_ScaleIncomingPhysicalDamage;
    Battle_statCalculators[11] = Battle_ScaleSourceMissingMp;
    Battle_statCalculators[18] = Battle_ScalePreviousTargetHit;
    Battle_statCalculators[19] = Battle_ScaleIncomingSpellEffect;
    Battle_statCalculators[20] = Battle_ScaleIncomingPhysicalDamage;
    Battle_statCalculators[21] = Battle_ScaleIncomingSpellCost;
    Battle_statCalculators[22] = Battle_ScaleIncomingDamage;
    Battle_statCalculators[23] = Battle_SourceShieldPp;
  }

  void Queue(int id, int hit, const AbilityHit& contract) {
    auto& definition = Battle_actionDefinitions[id];
    definition.category = id <= 31 ? 14 : 0;
    definition.hits[hit].statCalculatorId = contract.calculator;
    definition.hits[hit].statFactor = contract.factor;
    contract.apply(&definition, &source, contract.self ? &source : &target, hit,
                   0);
  }

  void Commit() {
    Battle_ApplyActorActionResult(&source);
    Battle_ApplyActorActionResult(&target);
  }

  void CheckResources(int source_hp, int source_mp, int target_hp,
                      int target_mp) const {
    CHECK(stats[0].currentHp == source_hp);
    CHECK(stats[0].currentMp == source_mp);
    CHECK(stats[1].currentHp == target_hp);
    CHECK(stats[1].currentMp == target_mp);
  }
};

struct TwoHitArt {
  int id;
  int damage_type;
  int factor;
  int first_affinity;
  int second_affinity;
};

// Both hit words in SLUS_010.40 action records, file offset 0x3C1DC.
constexpr auto kTwoHitArts = std::to_array<TwoHitArt>({
    {186, 3, 2, 0, 0},
    {194, 2, 2, 0, 0},
    {210, 1, 2, 0, 3},
    {211, 1, 3, 6, 7},
    {216, 3, 1, 7, 3},
    {217, 3, 2, 6, 2},
    {218, 3, 2, 7, 4},
    {219, 3, 3, 6, 5},
    {222, 1, 3, 6, 6},
});

struct ArtResolutionFixture : AbilityEffectFixture {
  decltype(std::to_array(Battle_hitFunctions)) saved_hits =
      std::to_array(Battle_hitFunctions);
  decltype(std::to_array(
      Battle_attackPrerequisiteFunctions)) saved_prerequisites =
      std::to_array(Battle_attackPrerequisiteFunctions);
  decltype(std::to_array(Battle_actionEffectMediators)) saved_effects =
      std::to_array(Battle_actionEffectMediators);
  std::uint32_t saved_previous_hit = Battle_previousHitResult;
  std::uint32_t saved_prerequisite_result = Battle_hitPrerequisiteResult;

  ArtResolutionFixture() {
    // BATTLE.PRG dispatch tables at file 0x7FA34, 0x7FA54 and 0x7FB78.
    Battle_hitFunctions[1] = Battle_PreviousHitSuccessThreshold;
    Battle_hitFunctions[3] = Battle_AgilityHitChance;
    Battle_attackPrerequisiteFunctions[1] = Battle_AcceptAttackPrerequisite;
    Battle_attackPrerequisiteFunctions[2] = Battle_AcceptAttackPrerequisite;
    Battle_actionEffectMediators[58] =
        [](Battle_ActionDefinition* definition, Battle_HitResult* source_hit,
           Battle_HitResult* target_hit, int hit, int random) {
          Battle_QueueHitDamageAndEquipmentWear(definition, source_hit,
                                                target_hit, hit, random);
        };
    Battle_statCalculators[25] = Battle_PhysicalDamageWithDebuffs;
    stats[0].agility = 200;
    stats[0].strength = 150;
    stats[1].strength = 80;
    for (int index = 0; index < 7; ++index) {
      stats[1].limbs[0].affinities[index] =
          static_cast<std::int16_t>(10 * index);
      if (index < 4) {
        stats[1].limbs[0].types[index] = static_cast<std::int16_t>(10 * index);
      }
    }
  }

  ~ArtResolutionFixture() {
    std::copy(saved_hits.begin(), saved_hits.end(), Battle_hitFunctions);
    std::copy(saved_prerequisites.begin(), saved_prerequisites.end(),
              Battle_attackPrerequisiteFunctions);
    std::copy(saved_effects.begin(), saved_effects.end(),
              Battle_actionEffectMediators);
    Battle_previousHitResult = saved_previous_hit;
    Battle_hitPrerequisiteResult = saved_prerequisite_result;
  }

  Battle_ActionRequest Resolve(int id) {
    Battle_ActionRequest input{};
    Battle_ActionRequest result{};
    input.targetCount = 1;
    input.targets[0].target.targetActor = 1;
    Battle_ResolveActionTargets(id, &result, &input, 0);
    return result;
  }

  void CheckResolvedHits(int id, int damage) {
    const auto resolved = Resolve(id);
    REQUIRE(resolved.targetCount == 1);
    CHECK(resolved.targets[0].target.hp == damage);
    CHECK(resolved.targets[0].hpEffect == 1);
    stats[1].cancelsHitResults = 1;
    actors[1].roomSpawnIndex = 0x80;
    const auto cancelled = Resolve(id);
    CHECK(cancelled.targets[0].target.hp == 0);
    CHECK(cancelled.targets[0].target.impactKind == 4);
    target = resolved.targets[0];
  }
};

struct LootInventoryFixture {
  Items_Inventory saved_inventory = Audio_Seq_unusedInventory;
  Items_InventoryIndices saved_indices = Audio_Seq_unusedInventoryIndices;
  std::uint8_t saved_capacity = Items_Inventory_inventoryItemCapacities[6];
  std::uint8_t* saved_index_pointer = Menu_Inventory_inventoryIndices[6];
  Items_Inventory source{};

  LootInventoryFixture() {
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventoryIndices = {};
    Items_Inventory_inventoryItemCapacities[6] = 64;
    Menu_Inventory_inventoryIndices[6] = Audio_Seq_unusedInventoryIndices.misc;
    for (auto& item : Audio_Seq_unusedInventory.misc) {
      item.id = 324;
      item.count = 100;
    }
    Audio_Seq_unusedInventory.misc[0].id = 323;
    Audio_Seq_unusedInventory.misc[0].count = 99;
    source.misc[0].id = 323;
    source.misc[0].count = 2;
  }

  ~LootInventoryFixture() {
    Audio_Seq_unusedInventory = saved_inventory;
    Audio_Seq_unusedInventoryIndices = saved_indices;
    Items_Inventory_inventoryItemCapacities[6] = saved_capacity;
    Menu_Inventory_inventoryIndices[6] = saved_index_pointer;
  }

  int Copy(bool write) {
    const int flags =
        static_cast<int>(Items_ItemCategory_Misc) |
        (write ? static_cast<int>(Menu_Framework_CopyItemFlag_Write) : 0);
    return Items_Inventory_CopyItem(flags, &Audio_Seq_unusedInventory, 0,
                                    &source);
  }

  static int Quantity(int id) {
    int quantity = 0;
    for (const auto& item : Audio_Seq_unusedInventory.misc) {
      if (item.id == id) quantity += item.count;
    }
    return quantity;
  }

  static void CheckUnchanged(
      const decltype(std::to_array(Audio_Seq_unusedInventory.misc))& before) {
    for (std::size_t slot = 0; slot < before.size(); ++slot) {
      CHECK(Audio_Seq_unusedInventory.misc[slot].id == before[slot].id);
      CHECK(Audio_Seq_unusedInventory.misc[slot].count == before[slot].count);
    }
  }

  void CheckQueryUnchanged(int expected) {
    const auto before = std::to_array(Audio_Seq_unusedInventory.misc);
    CHECK(Copy(false) == expected);
    CheckUnchanged(before);
  }

  void CheckWrite(int expected) {
    const auto inventory_before = std::to_array(Audio_Seq_unusedInventory.misc);
    const int before = Quantity(323);
    const int other = Quantity(324);
    const auto incoming = source.misc[0].count;
    CHECK(Copy(true) == expected);
    if (expected == 0) CheckUnchanged(inventory_before);
    CHECK(Quantity(323) == before + (expected != 0 ? incoming : 0));
    CHECK(Quantity(324) == other);
    CHECK(source.misc[0].id == 323);
    CHECK(source.misc[0].count == incoming);
    for (const auto& item : Audio_Seq_unusedInventory.misc) {
      CHECK(item.count >= 0);
      CHECK(item.count <= 100);
    }
  }
};

struct LootRandomFixture {
  decltype(std::to_array(Core_randomShuffleTable)) saved_table =
      std::to_array(Core_randomShuffleTable);
  int saved_index = Core_randomShuffleIndex;

  LootRandomFixture() { Core_Random_InitializeShuffleTable(); }

  ~LootRandomFixture() {
    std::copy(saved_table.begin(), saved_table.end(), Core_randomShuffleTable);
    Core_randomShuffleIndex = saved_index;
    Core_Random_Seed(1);
  }

  static void CheckSequence(VS::Core::Random& expected) {
    CHECK(Core_Random_NextBounded(255) == expected.Next(255));
    CHECK(Core_Random_Rand() == expected.Raw());
  }

  static void ForceFirstRoll(int roll) {
    std::fill_n(Core_randomShuffleTable, 97, 0);
    Core_randomShuffleIndex = 96;
    Core_randomShuffleTable[0] = 32767 - (roll * 32768 + 254) / 255;
    Core_Random_Seed(1234);
  }

  static void CheckOneForcedDraw() {
    VS::Core::Rand15 expected(1234);
    CHECK(Core_randomShuffleIndex == 0);
    CHECK(Core_randomShuffleTable[0] == expected.Next());
    CHECK(Core_Random_Rand() == expected.Next());
    for (int index = 1; index < 97; ++index) {
      CHECK(Core_randomShuffleTable[index] == 0);
    }
  }
};

void CheckLootItem(const Items_LootedItem& item, int id, int count, int flags) {
  CHECK(item.itemId == id);
  CHECK(static_cast<int>(item.count) == count);
  CHECK(static_cast<int>(item.difficultyFlags) == flags);
}

struct EnemyLootFixture : LootInventoryFixture {
  LootRandomFixture random;
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  Items_LootListNode* saved_head = Items_lootListHead;
  Battle_Actor* saved_actor = Battle_actors[1];
  Items_Inventory* saved_staging = Menu_Loot_inventory;
  Items_BattleLoot* saved_entry = Menu_Framework_lootEntry;
  Menu_Loot_LootListItem* saved_rows = Menu_Loot_lootList;
  u_char saved_count = Menu_Loot_lootListCount;
  u_char saved_offset = Menu_Loot_lootListOffset;
  char saved_selection = Menu_Loot_selectedLoot;
  char saved_difficulty = Audio_Seq_unusedStateFlags.difficulty;
  std::array<Core_HeapBlockHeader, 256> heap{};
  std::array<Menu_Loot_LootListItem, kMenuLootRowCapacity> rows{};
  Battle_Actor enemy{};
  Battle_ActorStats stats{};
  unsigned initial_free_units = 0;

  EnemyLootFixture() {
    Core_Memory_InitializeHeap(heap.data(), sizeof(heap));
    initial_free_units = Core_legacyHeapList.next->blockSize;
    Items_lootListHead = nullptr;
    enemy.id = 1;
    enemy.roomSpawnIndex = 1;
    enemy.stats = &stats;
    Battle_actors[1] = &enemy;
    Audio_Seq_unusedInventory = {};
    source = {};
    Menu_Loot_inventory = &source;
    Menu_Framework_lootEntry = nullptr;
    Menu_Loot_lootList = rows.data();
    Menu_Loot_lootListCount = 0;
    Menu_Loot_lootListOffset = 0;
    Menu_Loot_selectedLoot = 0;
    Audio_Seq_unusedStateFlags.difficulty = 0;
  }

  ~EnemyLootFixture() {
    Items_ClearRoomLootList();
    Core_legacyHeapList = saved_heap;
    Core_legacySecondaryHeapList = saved_secondary;
    Items_lootListHead = saved_head;
    Battle_actors[1] = saved_actor;
    Menu_Loot_inventory = saved_staging;
    Menu_Framework_lootEntry = saved_entry;
    Menu_Loot_lootList = saved_rows;
    Menu_Loot_lootListCount = saved_count;
    Menu_Loot_lootListOffset = saved_offset;
    Menu_Loot_selectedLoot = saved_selection;
    Audio_Seq_unusedStateFlags.difficulty = saved_difficulty;
  }

  void SetGuaranteed(int count) {
    if (count > 0) {
      stats.dropItem.id = 323;
      stats.dropItem.count = 2;
    }
    if (count > 1) {
      stats.secondDropItem.id = 324;
      stats.secondDropItem.count = 3;
    }
  }

  void CheckQueued(int guaranteed, bool candidate, bool success) {
    REQUIRE(Items_lootListHead != nullptr);
    CHECK(Items_lootListHead->droppedMask == enemy.id);
    CHECK(Items_lootListHead->next == nullptr);
    const auto& loot = Items_lootListHead->loot;
    CheckLootItem(loot.misc[0], guaranteed > 0 ? 323 : 0,
                  guaranteed > 0 ? 2 : 0, guaranteed > 0 ? 3 : 0);
    CheckLootItem(loot.misc[1], guaranteed > 1 ? 324 : 0,
                  guaranteed > 1 ? 3 : 0, guaranteed > 1 ? 3 : 0);
    CheckLootItem(loot.misc[2], candidate ? 325 : 0, success ? 1 : 0,
                  success ? 3 : 0);
  }

  void DeliverRows(int count) {
    REQUIRE(Menu_Loot_lootListCount == count);
    for (int index = 0; index < count; ++index) {
      const auto& row = rows[index];
      CHECK(row.itemCategory == Items_ItemCategory_Misc);
      CHECK(row.state == 1);
      CHECK(row.opaque2 == 0);
      const int flags = static_cast<int>(row.itemCategory) |
                        static_cast<int>(Menu_Framework_CopyItemFlag_Write);
      CHECK(Items_Inventory_CopyItem(flags, &Audio_Seq_unusedInventory,
                                     row.itemIndex, &source) != 0);
    }
  }

  void CheckCleared() {
    Items_ClearRoomLootList();
    CHECK(Items_lootListHead == nullptr);
    CHECK(Core_legacyHeapList.next == Core_legacyHeapList.prev);
    CHECK(Core_legacyHeapList.next->blockSize == initial_free_units);
  }
};

Items_EquipmentPartStats EquipmentDropPart(u_short id) {
  Items_EquipmentPartStats part{};
  part.id = id;
  part.subId = 7;
  part.weaponModelId = 44;
  part.category = 2;
  part.strength = 12;
  part.intelligence = -5;
  part.agility = 6;
  part.currentDp = 1234;
  part.maxDp = 9876;
  part.currentPp = 345;
  part.maxPp = 654;
  part.damageType = 7;
  part.costType = 15;
  part.cost = 11;
  part.material = 5;
  part.opaque14 = 0x42;
  part.gemSlots = 3;
  part.gemEffects = 5;
  part.range = {5, 7, 9, 2, 13};
  for (int index = 0; index < 4; ++index) {
    part.types[index] = static_cast<signed char>(index * 7 - 21);
  }
  for (int index = 0; index < 8; ++index) {
    part.classes[index] = static_cast<signed char>(index * 11 - 33);
    part.affinities[index] = static_cast<signed char>(index * 13 - 39);
  }
  return part;
}

template <typename Item>
void CheckDropIdentity(const Item& item, const Items_EquipmentPartStats& part) {
  CHECK(item.id == part.id);
  CHECK(static_cast<std::uint8_t>(item.subId) ==
        static_cast<std::uint8_t>(part.subId));
  CHECK(item.strength == part.strength);
  CHECK(item.intelligence == part.intelligence);
  CHECK(item.agility == part.agility);
}

template <typename Item>
void CheckDropDurability(const Item& item,
                         const Items_EquipmentPartStats& part) {
  CHECK(item.wepId == part.weaponModelId);
  CHECK(static_cast<std::uint8_t>(item.category) ==
        static_cast<std::uint8_t>(part.category));
  CHECK(item.currentDp == part.currentDp);
  CHECK(item.maxDp == part.maxDp);
  CHECK(item.currentPp == part.currentPp);
  CHECK(item.maxPp == part.maxPp);
  CHECK(item.material == part.material);
}

template <typename Item>
void CheckDropAffinities(const Item& item,
                         const Items_EquipmentPartStats& part) {
  for (int index = 0; index < 8; ++index) {
    CHECK(item.classes[index] == (index < 6 ? part.classes[index] : 0));
    CHECK(item.affinities[index] == (index < 7 ? part.affinities[index] : 0));
  }
}

template <typename Item>
void CheckDropTypes(const Item& item, const Items_EquipmentPartStats& part) {
  for (int index = 0; index < 4; ++index) {
    CHECK(item.types[index] == part.types[index]);
  }
}

template <typename Item>
void CheckZeroedDrop(const Item& item) {
  const Item empty{};
  CHECK(std::memcmp(&item, &empty, sizeof(item)) == 0);
}

void CheckBladeDrop(const Items_InventoryBlade& item,
                    const Items_EquipmentPartStats& part) {
  CheckDropIdentity(item, part);
  CheckDropDurability(item, part);
  CheckDropAffinities(item, part);
  CHECK(item.cost == part.cost);
  // Copied C uses unsigned bitfields; compare their wire bits in C++.
  CHECK((item.damageType & 3) == (part.damageType & 3));
  CHECK((item.costType & 7) == (part.costType & 7));
  CHECK(item.opaque12 == part.opaque14);
  CHECK(std::memcmp(&item.range, &part.range, sizeof(item.range)) == 0);
}

void CheckGripDrop(const Items_InventoryGrip& item,
                   const Items_EquipmentPartStats& part) {
  CheckDropIdentity(item, part);
  CheckDropTypes(item, part);
  CHECK(static_cast<std::uint8_t>(item.category) ==
        static_cast<std::uint8_t>(part.category));
  CHECK(item.gemSlots == part.gemSlots);
}

void CheckArmorDrop(const Items_InventoryArmor& item,
                    const Items_EquipmentPartStats& part) {
  CheckDropIdentity(item, part);
  CheckDropDurability(item, part);
  CheckDropTypes(item, part);
  CheckDropAffinities(item, part);
  CHECK(item.gemSlots == part.gemSlots);
}

template <typename Loadout>
void PrepareDropGems(Loadout& source) {
  source.gems[0] = EquipmentDropPart(300);
  source.gems[2] = EquipmentDropPart(302);
}

template <typename Drop, typename Loadout>
void CheckDropGems(const Drop& drop, const Loadout& source) {
  for (int index = 0; index < 3; ++index) {
    const auto& gem = drop.gems[index];
    const auto& part = source.gems[index];
    if (part.id == 0) {
      CheckZeroedDrop(gem);
    } else {
      CheckDropIdentity(gem, part);
      CheckDropAffinities(gem, part);
      CHECK(gem.gemEffects == part.gemEffects);
    }
  }
}

void PrepareEquipmentDrop(Items_WeaponLoadout& source) {
  source.blade = EquipmentDropPart(14);
  source.grip = EquipmentDropPart(96);
  PrepareDropGems(source);
  source.currentDp = 9;
  source.currentPp = 8;
  for (int index = 0; index < 23; ++index) {
    source.name[index] = static_cast<char>(10 + index);
  }
  source.name[23] = static_cast<char>(0xE7);
}

void PrepareEquipmentDrop(Items_ShieldLoadout& source) {
  source.base = EquipmentDropPart(147);
  PrepareDropGems(source);
  source.currentDp = 9;
  source.currentPp = 8;
}

void PrepareEquipmentDrop(Items_ArmorLoadout& source) {
  source.armor = EquipmentDropPart(149);
  source.currentDp = 9;
}

void PrepareEquipmentDrop(Items_AccessorySpawnLoadout& source) {
  source.accessory = EquipmentDropPart(237);
}

void CheckEquipmentDrop(const Items_LootedWeapon& drop,
                        const Items_WeaponLoadout& source) {
  CHECK(drop.difficultyFlags == 3);
  CheckBladeDrop(drop.blade, source.blade);
  CheckGripDrop(drop.grip, source.grip);
  CheckDropGems(drop, source);
  CHECK(std::memcmp(drop.name, source.name, sizeof(drop.name)) == 0);
}

void CheckEquipmentDrop(const Items_LootedShield& drop,
                        const Items_ShieldLoadout& source) {
  CHECK(drop.difficultyFlags == 3);
  CheckArmorDrop(drop.shield, source.base);
  CheckDropGems(drop, source);
}

void CheckEquipmentDrop(const Items_LootedArmor& drop,
                        const Items_ArmorLoadout& source) {
  CHECK(drop.difficultyFlags == 3);
  CheckArmorDrop(drop.armor, source.armor);
}

void CheckEquipmentDrop(const Items_LootedAccessory& drop,
                        const Items_AccessorySpawnLoadout& source) {
  CHECK(drop.difficultyFlags == 3);
  CheckArmorDrop(drop.accessory, source.accessory);
}

template <typename Drop, typename Loadout>
void CheckEquipmentRoll(int roll, int rate, int (*drop)(Drop*, Loadout*)) {
  LootRandomFixture random;
  random.ForceFirstRoll(roll);
  Loadout source{};
  PrepareEquipmentDrop(source);
  source.dropRate = static_cast<decltype(source.dropRate)>(rate);
  const auto before = source;
  std::array<Drop, 2> result{};
  CHECK(drop(result.data(), &source) == (roll < rate));
  if (roll < rate) {
    CheckEquipmentDrop(result[0], source);
    result[0] = {};
  }
  CheckZeroedDrop(result);
  CHECK(std::memcmp(&source, &before, sizeof(source)) == 0);
  random.CheckOneForcedDraw();
}

struct EquipmentCapacity {
  int parents;
  int blades;
  int grips;
  int gems;
};

template <typename Loadout>
void EnableEquipmentDrop(Loadout& source) {
  PrepareEquipmentDrop(source);
  source.dropRate = static_cast<decltype(source.dropRate)>(255);
}

template <typename Item, typename Loadout>
void CheckClaimedGems(const Item& item, const Loadout& source, int parent) {
  for (int index = 0; index < 3; ++index) {
    const int slot = item.gems[index];
    const auto& part = source.gems[index];
    CHECK((slot != 0) == (part.id != 0));
    CHECK(slot <= 48);
    if (slot > 0 && slot <= 48) {
      const auto& gem = Audio_Seq_unusedInventory.gems[slot - 1];
      CheckDropIdentity(gem, part);
      CheckDropAffinities(gem, part);
      CHECK(gem.gemEffects == part.gemEffects);
      CHECK(gem.index == slot);
      CHECK(gem.setItemIndex == parent);
    }
  }
}

struct EquipmentStagingFixture : EnemyLootFixture {
  void CollectArmorPage(std::vector<int>& seen, bool claim) {
    for (int index = 0; index < Menu_Loot_lootListCount; ++index) {
      const auto& row = rows[index];
      REQUIRE(row.itemCategory == Items_ItemCategory_Armor);
      if (row.itemIndex < 0 || row.itemIndex >= 16) {
        FAIL("Staged armor row points outside the original capacity");
        return;
      }
      seen.push_back(source.armor[row.itemIndex].id);
      if (claim) {
        CHECK(row.state == 1);
        CHECK(Items_Inventory_CopyItem(0x14, &Audio_Seq_unusedInventory,
                                       row.itemIndex, &source) != 0);
      }
    }
  }

  void AdvanceArmorPage(bool first_claimed) {
    Menu_Loot_lootListCount = 0;
    REQUIRE(Menu_Loot_AdvanceStagedInventory() == 1);
    REQUIRE(Menu_Loot_lootListCount == 8);
    for (int index = 0; index < 8; ++index) {
      CHECK(rows[index].state == !first_claimed);
    }
    Audio_Seq_unusedInventory = {};
    Menu_Loot_RefreshClaimableRows();
  }

  void CheckArmorBatchComplete(std::vector<int>& seen) {
    std::sort(seen.begin(), seen.end());
    REQUIRE(seen.size() == 24);
    for (int index = 0; index < 24; ++index) CHECK(seen[index] == 149 + index);
    Menu_Loot_lootListCount = 0;
    CHECK(Menu_Loot_AdvanceStagedInventory() == 0);
    VS::Core::Random expected;
    for (int index = 0; index < 40; ++index) expected.Next(255);
    random.CheckSequence(expected);
    CheckCleared();
    CHECK(Menu_Loot_AdvanceStagedInventory() == 0);
  }

  void QueueArmorBatch() {
    for (int actor = 0; actor < 8; ++actor) {
      enemy.id = actor + 1;
      EnableEquipmentDrop(stats.limbs[0].armor);
      EnableEquipmentDrop(stats.limbs[1].armor);
      EnableEquipmentDrop(stats.accessory);
      stats.limbs[0].armor.armor.id = static_cast<u_short>(149 + actor * 3);
      stats.limbs[1].armor.armor.id = static_cast<u_short>(150 + actor * 3);
      stats.accessory.accessory.id = static_cast<u_short>(151 + actor * 3);
      Items_QueueEnemyLoot(&enemy);
    }
  }

  void Stage(int flags) {
    EnableEquipmentDrop(stats.weapon);
    EnableEquipmentDrop(stats.shield);
    EnableEquipmentDrop(stats.accessory);
    EnableEquipmentDrop(stats.limbs[0].armor);
    EnableEquipmentDrop(stats.limbs[1].armor);
    stats.limbs[1].armor.armor.id = 153;
    Items_QueueEnemyLoot(&enemy);
    REQUIRE(Items_lootListHead != nullptr);
    auto& loot = Items_lootListHead->loot;
    loot.weapon.difficultyFlags = static_cast<char>(flags);
    loot.shield.difficultyFlags = static_cast<char>(flags);
    loot.accessory.difficultyFlags = static_cast<char>(flags);
    for (auto& armor : loot.armor) {
      armor.difficultyFlags = static_cast<char>(flags);
    }
    Menu_Loot_BuildStagedInventory(Items_lootListHead);
  }

  static void CheckRow(const Menu_Loot_LootListItem& row, int index,
                       bool fits) {
    const std::array categories{
        Items_ItemCategory_Weapon, Items_ItemCategory_Shield,
        Items_ItemCategory_Armor, Items_ItemCategory_Armor,
        Items_ItemCategory_Armor};
    CHECK(row.itemCategory == categories[index]);
    CHECK(row.itemIndex == (index < 2 ? 0 : index - 2));
    CHECK(row.opaque2 == 0);
    CHECK(row.state == fits);
  }

  void Claim(int armor_count) {
    const auto source_before = source;
    int claimed_armor = 0;
    for (int index = 0; index < 5; ++index) {
      const auto& row = rows[index];
      const bool fits = index < 2 || armor_count + claimed_armor < 16;
      CheckRow(row, index, fits);
      const int flags = static_cast<int>(row.itemCategory) |
                        static_cast<int>(Menu_Framework_CopyItemFlag_Write);
      const int result = Items_Inventory_CopyItem(
          flags, &Audio_Seq_unusedInventory, row.itemIndex, &source);
      CHECK(result ==
            (fits ? (index < 2 ? 1 : armor_count + claimed_armor + 1) : 0));
      if (index >= 2 && fits) ++claimed_armor;
      Menu_Loot_RefreshClaimableRows();
    }
    CHECK(std::memcmp(&source, &source_before, sizeof(source)) == 0);
    CheckClaimedArmor(armor_count, claimed_armor);
  }

  void CheckClaimedArmor(int armor_count, int claimed_armor) const {
    for (int index = 0; index < claimed_armor; ++index) {
      const auto& expected = index < 2 ? stats.limbs[index].armor.armor
                                       : stats.accessory.accessory;
      const auto& armor = Audio_Seq_unusedInventory.armor[armor_count + index];
      CheckArmorDrop(armor, expected);
      CHECK(armor.index == armor_count + index + 1);
      CHECK(armor.limb == 0);
    }
  }

  void CheckWeaponParts() {
    const auto& inventory = Audio_Seq_unusedInventory;
    const auto& weapon = inventory.weapons[0];
    CHECK(weapon.index == 1);
    CHECK(weapon.blade == 1);
    CHECK(weapon.grip == 1);
    CHECK(weapon.isEquipped == 0);
    CHECK(std::memcmp(weapon.name, stats.weapon.name, sizeof(weapon.name)) ==
          0);
    CheckBladeDrop(inventory.blades[0], stats.weapon.blade);
    CheckGripDrop(inventory.grips[0], stats.weapon.grip);
    CHECK(inventory.blades[0].assembledWeaponIndex == 1);
    CHECK(inventory.grips[0].assembledWeaponIndex == 1);
    CheckClaimedGems(weapon, stats.weapon, 1);
  }

  void CheckShieldParts() {
    const auto& shield = Audio_Seq_unusedInventory.shields[0];
    CHECK(shield.index == 1);
    CHECK(shield.isEquipped == 0);
    CheckArmorDrop(shield.base, stats.shield.base);
    CheckClaimedGems(shield, stats.shield, 0x81);
    CHECK(Items_Inventory_CountItems(Items_ItemCategory_Gem,
                                     &Audio_Seq_unusedInventory) == 4);
  }
};

struct LootPagingMenuFixture : EquipmentStagingFixture {
  u_int* saved_sfx_offsets = Audio_Playback_soundEffectProgramOffsets;
  u_short* saved_sfx_selectors = Audio_Playback_soundEffectBankSelectors;
  State_Settings saved_settings = Input_settings;
  decltype(Input_buttonsPressed) saved_buttons = Input_buttonsPressed;
  decltype(Input_buttonRepeat) saved_repeat = Input_buttonRepeat;
  Menu_Framework_MenuRow* saved_menu_rows = Menu_Framework_menuItems;
  decltype(Menu_Framework_itemNames) saved_names = Menu_Framework_itemNames;
  decltype(Battle_stringContext) saved_string_context = Battle_stringContext;
  Menu_TextBoxState saved_box = Menu_textBoxes[7];
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  int saved_sound_group = Audio_nextSfxChannelGroup7E;
  decltype(Menu_Loot_lootClaimed) saved_claimed = Menu_Loot_lootClaimed;
  decltype(Menu_Loot_controlsEnabled) saved_controls =
      Menu_Loot_controlsEnabled;
  decltype(Menu_Loot_showLoot) saved_show = Menu_Loot_showLoot;
  decltype(Menu_Loot_previewedLoot) saved_preview = Menu_Loot_previewedLoot;
  decltype(Menu_Loot_lootLeftEdge) saved_edge = Menu_Loot_lootLeftEdge;
  std::array<Menu_Framework_MenuRow, 40> menu_rows{};
  char item_names[512][24]{};
  std::vector<u_long> storage = std::vector<u_long>(0x20000 / 4 + 2);
  std::array<u_long, 32> ordering{};
  std::array<u_int, 0x180> sfx_offsets{};
  std::array<u_short, 0x180> sfx_selectors{};

  LootPagingMenuFixture() {
    Menu_Framework_menuItems = menu_rows.data();
    Menu_Framework_itemNames = item_names;
    Menu_textBoxes[7] = {};
    Menu_InitInformationTextBox(Menu_InformationBoxHeader_Information);
    sfx_offsets.fill(UINT32_MAX);
    Audio_Playback_soundEffectProgramOffsets = sfx_offsets.data();
    Audio_Playback_soundEffectBankSelectors = sfx_selectors.data();
    Input_settings.information = 0;
    Menu_Loot_lootClaimed = 0;
    Menu_Loot_lootLeftEdge = 0;
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    Render_frameScratch.primitiveCursor = storage.data() + 1;
    Render_frameScratch.overlayOrderingTable = ordering.data() + 2;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Menu_Loot_UpdateLootSelection(1);
  }

  ~LootPagingMenuFixture() {
    Audio_Playback_soundEffectProgramOffsets = saved_sfx_offsets;
    Audio_Playback_soundEffectBankSelectors = saved_sfx_selectors;
    Input_settings = saved_settings;
    Input_buttonsPressed = saved_buttons;
    Input_buttonRepeat = saved_repeat;
    Menu_Framework_menuItems = saved_menu_rows;
    Menu_Framework_itemNames = saved_names;
    Battle_stringContext = saved_string_context;
    Menu_textBoxes[7] = saved_box;
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    Audio_nextSfxChannelGroup7E = saved_sound_group;
    Menu_Loot_lootClaimed = saved_claimed;
    Menu_Loot_controlsEnabled = saved_controls;
    Menu_Loot_showLoot = saved_show;
    Menu_Loot_previewedLoot = saved_preview;
    Menu_Loot_lootLeftEdge = saved_edge;
  }

  static int Tick(int buttons = 0) {
    Input_buttonsPressed.allButtons = static_cast<u_short>(buttons);
    Input_buttonRepeat = static_cast<u_short>(buttons);
    return Menu_Loot_UpdateLootSelection(0);
  }

  static void Step(int buttons = 0) { CHECK(Tick(buttons) == 0); }

  static void CheckClaims(int claimed, int bag_count) {
    CHECK(Menu_Loot_lootClaimed == claimed);
    CHECK(Items_Inventory_CountItems(Items_ItemCategory_Armor, nullptr) ==
          bag_count);
  }

  static void CheckNextArmorPage() {
    CHECK(Menu_Loot_lootListCount == 8);
    CHECK(Menu_Loot_selectedLoot == 0);
    CHECK(Menu_Loot_lootListOffset == 0);
    CHECK(Menu_Loot_controlsEnabled == 0);
    CHECK(Menu_Loot_showLoot == 1);
  }

  void OpenLeaveConfirmation() {
    Step(PADRdown);
    Step();
    Step();
    SettleRows();
    Step();
  }

  void SettleRows() {
    for (auto& row : menu_rows) {
      if (row.state > 1) row.state = 1;
    }
  }
};

struct EquipmentLootFixture : LootInventoryFixture {
  bool shield;
  int attached;
  EquipmentCapacity occupied;

  EquipmentLootFixture(bool is_shield, int gem_count, EquipmentCapacity counts)
      : shield(is_shield), attached(gem_count), occupied(counts) {
    Audio_Seq_unusedInventory = {};
    source = {};
    PrepareSource();
    FillTarget(Audio_Seq_unusedInventory, shield, occupied);
  }

  void PrepareSource() {
    if (shield) {
      auto part = EquipmentDropPart(147);
      Items_WriteInventoryArmorStats(&source.shields[0].base, &part);
      source.shields[0].index = 1;
      source.shields[0].isEquipped = 1;
      source.shields[0].base.limb = 1;
    } else {
      auto blade = EquipmentDropPart(14);
      auto grip = EquipmentDropPart(96);
      Items_WriteInventoryBladeStats(&source.blades[0], &blade);
      Items_WriteInventoryGripStats(&source.grips[0], &grip);
      source.blades[0].index = 1;
      source.blades[0].assembledWeaponIndex = 1;
      source.grips[0].index = 1;
      source.grips[0].assembledWeaponIndex = 1;
      source.weapons[0].index = 1;
      source.weapons[0].isEquipped = 1;
      source.weapons[0].blade = 1;
      source.weapons[0].grip = 1;
      for (int index = 0; index < 23; ++index) {
        source.weapons[0].name[index] = static_cast<char>(10 + index);
      }
      source.weapons[0].name[23] = static_cast<char>(0xE7);
    }
    for (int index = 0; index < attached; ++index) {
      auto gem = EquipmentDropPart(static_cast<u_short>(300 + index));
      Items_WriteInventoryGemStats(&source.gems[index], &gem);
      source.gems[index].index = static_cast<std::int16_t>(index + 1);
      source.gems[index].setItemIndex = shield ? 0x81 : 1;
      auto& slots = shield ? source.shields[0].gems : source.weapons[0].gems;
      slots[index] = static_cast<u_char>(index + 1);
    }
  }

  static void FillTarget(Items_Inventory& target, bool shield,
                         EquipmentCapacity occupied) {
    for (int index = 0; index < occupied.parents; ++index) {
      if (shield) {
        target.shields[index].base.id = 147;
        target.shields[index].index = static_cast<char>(index + 1);
      } else {
        target.weapons[index].blade = static_cast<u_char>(index + 1);
        target.weapons[index].grip = static_cast<u_char>(index + 1);
        target.weapons[index].index = static_cast<char>(index + 1);
      }
    }
    for (int index = 0; index < occupied.blades; ++index) {
      target.blades[index].id = 14;
      target.blades[index].index = static_cast<char>(index + 1);
      if (!shield && index < occupied.parents) {
        target.blades[index].assembledWeaponIndex =
            static_cast<char>(index + 1);
      }
    }
    for (int index = 0; index < occupied.grips; ++index) {
      target.grips[index].id = 96;
      target.grips[index].index = static_cast<u_short>(index + 1);
      if (!shield && index < occupied.parents) {
        target.grips[index].assembledWeaponIndex =
            static_cast<u_short>(index + 1);
      }
    }
    for (int index = 0; index < occupied.gems; ++index) {
      target.gems[index].id = 300;
      target.gems[index].index = static_cast<std::int16_t>(index + 1);
    }
  }

  int CopyEquipment(bool write) {
    const int category =
        shield ? Items_ItemCategory_Shield : Items_ItemCategory_Weapon;
    const int flags =
        category |
        (write ? static_cast<int>(Menu_Framework_CopyItemFlag_Write) : 0);
    return Items_Inventory_CopyItem(flags, &Audio_Seq_unusedInventory, 0,
                                    &source);
  }

  Items_Inventory ExpectedInventory(const Items_Inventory& before) const {
    auto expected = before;
    const int parent = occupied.parents + 1;
    if (shield) {
      expected.shields[occupied.parents] = source.shields[0];
      expected.shields[occupied.parents].index = static_cast<char>(parent);
      expected.shields[occupied.parents].isEquipped = 0;
    } else {
      auto& weapon = expected.weapons[occupied.parents];
      weapon = source.weapons[0];
      weapon.index = static_cast<char>(parent);
      weapon.isEquipped = 0;
      weapon.blade = static_cast<u_char>(occupied.blades + 1);
      weapon.grip = static_cast<u_char>(occupied.grips + 1);
      auto& blade = expected.blades[occupied.blades];
      blade = source.blades[0];
      blade.index = static_cast<char>(occupied.blades + 1);
      blade.assembledWeaponIndex = static_cast<char>(parent);
      auto& grip = expected.grips[occupied.grips];
      grip = source.grips[0];
      grip.index = static_cast<u_short>(occupied.grips + 1);
      grip.assembledWeaponIndex = static_cast<u_short>(parent);
    }
    for (int index = 0; index < attached; ++index) {
      const int slot = occupied.gems + index;
      expected.gems[slot] = source.gems[index];
      expected.gems[slot].index = static_cast<std::int16_t>(slot + 1);
      expected.gems[slot].setItemIndex =
          static_cast<u_short>(parent | (shield ? 0x80 : 0));
      auto& slots = shield ? expected.shields[occupied.parents].gems
                           : expected.weapons[occupied.parents].gems;
      slots[index] = static_cast<u_char>(slot + 1);
    }
    return expected;
  }

  static void CheckInventory(const Items_Inventory& actual,
                             const Items_Inventory& expected) {
    CHECK(std::memcmp(&actual, &expected, sizeof(actual)) == 0);
  }

  void CheckTransfer() {
    const auto before = Audio_Seq_unusedInventory;
    const auto source_before = source;
    const bool fits = occupied.parents < 8 && occupied.gems + attached <= 48 &&
                      (shield || (occupied.blades < 16 && occupied.grips < 16));
    const int result = fits ? occupied.parents + 1 : 0;
    CHECK(CopyEquipment(false) == result);
    CheckInventory(Audio_Seq_unusedInventory, before);
    CheckInventory(source, source_before);
    CHECK(CopyEquipment(true) == result);
    CheckInventory(source, source_before);
    CheckInventory(Audio_Seq_unusedInventory,
                   fits ? ExpectedInventory(before) : before);
  }
};

struct LootPresentationFixture : EnemyLootFixture {
  char saved_intro = State_ScriptFlag(0xAB);
  u_int saved_control = Field_controlMode;
  u_int saved_flow = Battle_actionFlowMode;
  Actors_ActorRecord* saved_player = Actors_actorRecords[0];
  Items_LootListNode* saved_menu_list = Menu_Framework_lootLists;
  Items_BattleLoot* saved_menu_entry = Menu_Framework_lootEntry;
  Actors_ActorRecord locked_player_pose{};

  LootPresentationFixture() {
    // A locked pose keeps this presentation-gate test independent of model I/O.
    locked_player_pose.animationId = 0x63;
    Actors_actorRecords[0] = &locked_player_pose;
    Menu_Framework_lootLists = nullptr;
    Menu_Framework_lootEntry = nullptr;
    Field_controlMode = 1;
    Battle_actionFlowMode = 11;
  }

  ~LootPresentationFixture() {
    State_SetScriptFlag(0xAB, saved_intro);
    Field_controlMode = saved_control;
    Battle_actionFlowMode = saved_flow;
    Actors_actorRecords[0] = saved_player;
    Menu_Framework_lootLists = saved_menu_list;
    Menu_Framework_lootEntry = saved_menu_entry;
  }
};

struct AbilityUnlockFixture : AbilityFixture {
  State_ArtsStatus saved_progress = Audio_Seq_unusedArtsStatus;
  decltype(std::to_array(Battle_battleAbilityPointRequirements)) saved_points =
      std::to_array(Battle_battleAbilityPointRequirements);
  decltype(kBreakArtThresholds)::value_type saved_art_points[10]{};

  AbilityUnlockFixture() {
    Audio_Seq_unusedArtsStatus = {};
    std::fill_n(Audio_Seq_unusedArtsStatus.artsLearned, 12, 4);
    std::copy(kAbilityThresholds.begin(), kAbilityThresholds.end(),
              Battle_battleAbilityPointRequirements);
    for (unsigned category = 0; category < kBreakArtThresholds.size();
         ++category) {
      saved_art_points[category] =
          std::to_array(Battle_breakArtPointRequirements[category]);
      std::copy(kBreakArtThresholds[category].begin(),
                kBreakArtThresholds[category].end(),
                Battle_breakArtPointRequirements[category]);
    }
  }

  ~AbilityUnlockFixture() {
    Audio_Seq_unusedArtsStatus = saved_progress;
    std::copy(saved_points.begin(), saved_points.end(),
              Battle_battleAbilityPointRequirements);
    for (unsigned category = 0; category < kBreakArtThresholds.size();
         ++category) {
      std::copy(saved_art_points[category].begin(),
                saved_art_points[category].end(),
                Battle_breakArtPointRequirements[category]);
    }
  }
};

struct ChainCreditFixture : AbilityUnlockFixture {
  char saved_story = State_ScriptFlag(0x82);
  u_int saved_flow = Battle_actionFlowMode;
  u_int saved_control = Field_controlMode;
  int saved_reward = Battle_recoveryRewardMode;

  ChainCreditFixture() {
    State_SetScriptFlag(0x82, 10);
    action.abilityInputSucceeded = 1;
    action.chainAbilitySlot = 1;
    stats[1].currentHp = 0;
    records[0].animationId = 0x63;
  }

  ~ChainCreditFixture() {
    State_SetScriptFlag(0x82, saved_story);
    Battle_actionFlowMode = saved_flow;
    Field_controlMode = saved_control;
    Battle_recoveryRewardMode = saved_reward;
  }

  void CheckTerminated() const {
    CHECK(Battle_actionFlowMode == 9);
    CHECK(action.queueIndex == 0);
    CHECK(action.chainStep == 0);
  }
};

enum class KillGate {
  kPreview,
  kPositionSource,
  kEnemySource,
  kPositionTarget,
  kDummy,
  kSpell,
  kItem,
  kNonlethal,
  kAshleyTarget,
  kAlreadyCounted,
  kStory,
  kFourArts,
  kPendingThreshold
};

struct BreakArtKillFixture : AbilityUnlockFixture {
  decltype(std::to_array(Battle_actionEffectMediators)) saved_effects =
      std::to_array(Battle_actionEffectMediators);
  char saved_story = State_ScriptFlag(0x82);
  Battle_HitResult source{};
  Battle_HitResult target{};
  unsigned counter;
  int action_id = 184;
  int apply = 1;

  BreakArtKillFixture(unsigned category, unsigned tier)
      : counter(category % 10) {
    actors[0].equippedWeaponCategory = static_cast<u_char>(category);
    State_SetScriptFlag(0x82, 10);
    Audio_Seq_unusedArtsStatus.artsLearned[counter] = static_cast<u_char>(tier);
    Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[counter] =
        static_cast<u_short>(kBreakArtThresholds[counter][tier] - 1);
    Battle_InitializeHitResult(&source);
    Battle_InitializeHitResult(&target);
    target.target.targetActor = 1;
    target.target.hp = stats[1].currentHp;
    Battle_actionDefinitions[184].type = Battle_ActionType_BreakArt;
    Battle_actionDefinitions[160].type = Battle_ActionType_Item;
    Battle_actionEffectMediators[0] =
        [](Battle_ActionDefinition* definition, Battle_HitResult* from,
           Battle_HitResult* to, int hit, int update) {
          Battle_IgnoreActionEffect(definition, from, to, hit, update);
        };
  }

  ~BreakArtKillFixture() {
    std::copy(saved_effects.begin(), saved_effects.end(),
              Battle_actionEffectMediators);
    State_SetScriptFlag(0x82, saved_story);
  }

  void Reject(KillGate gate) {
    switch (gate) {
      case KillGate::kPreview:
        apply = 0;
        break;
      case KillGate::kPositionSource:
        source.targetsPosition = 1;
        break;
      case KillGate::kEnemySource:
        source.target.targetActor = 1;
        break;
      case KillGate::kPositionTarget:
        target.targetsPosition = 1;
        break;
      case KillGate::kDummy:
        stats[1].actorId = 0x80;
        break;
      case KillGate::kSpell:
        action_id = 99;
        break;
      case KillGate::kItem:
        action_id = 160;
        break;
      case KillGate::kNonlethal:
        --target.target.hp;
        break;
      case KillGate::kAshleyTarget:
        target.target.targetActor = 0;
        break;
      case KillGate::kAlreadyCounted:
        actors[1].killCounted = 1;
        break;
      case KillGate::kStory:
        State_SetScriptFlag(0x82, 9);
        break;
      case KillGate::kFourArts:
        Audio_Seq_unusedArtsStatus.artsLearned[counter] = 4;
        break;
      case KillGate::kPendingThreshold:
        ++Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[counter];
        break;
    }
  }

  void Resolve() {
    Battle_ApplyActionHitEffect(&Battle_actionDefinitions[action_id], &source,
                                &target, apply, 0);
  }
};

void CheckAbilityProgress(const State_ArtsStatus& before) {
  CHECK(std::to_array(Audio_Seq_unusedArtsStatus.artsLearned) ==
        std::to_array(before.artsLearned));
  CHECK(std::to_array(Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills) ==
        std::to_array(before.kills.weaponCategoryKills));
  CHECK(Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked ==
        before.kills.battleAbilitiesUnlocked);
  CHECK(Audio_Seq_unusedArtsStatus.kills.totalKills == before.kills.totalKills);
}
}  // namespace

TEST_CASE(
    "Combat sandbox applies matching target HP and MP effects with clamping",
    "[combat][sandbox]") {
  REQUIRE(Tests_CombatTargetResults() == 1);
}

TEST_CASE("Stat item effects bind plugin-local renderers, not EFFPURGE",
          "[combat][items][effects][item-plugin]") {
  const auto count = Effects_effectEntryPointIndices
      [std::size(Effects_effectEntryPointIndices) - 1];
  for (unsigned slot = 0; slot < count; ++slot) {
    CAPTURE(slot);
    CHECK(Effects_effectEntryPoints[slot] != Effects_InvokeAllPluginUpdates);
  }
  CHECK(Effects_effectEntryPoints[Effects_effectEntryPointIndices[198]] ==
        Effects_Plg198_Ring);
  CHECK(Effects_effectEntryPoints[Effects_effectEntryPointIndices[199]] ==
        Effects_Plg198_Ring);
}

TEST_CASE("Ability rewards use all original cumulative thresholds",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 22));
  const int delta = GENERATE(-1, 0, 1);
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.kills.battleAbilitiesUnlocked = static_cast<u_short>(unlocked);
  progress.kills.totalKills =
      static_cast<u_short>(kAbilityThresholds[unlocked] + delta);
  const auto before = progress;
  CHECK(Battle_PendingBattleAbilityUnlocks() == (delta >= 0 ? 1 : 0));
  CheckAbilityProgress(before);
}

TEST_CASE("Successful lethal chain timing retains ability credit",
          "[combat][abilities][acquisition][chain-credit]") {
  ChainCreditFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 22));
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.kills.battleAbilitiesUnlocked = static_cast<u_short>(unlocked);
  progress.kills.totalKills = kAbilityThresholds[unlocked] - 1;
  auto expected = progress;
  ++expected.kills.totalKills;
  Battle_AdvanceBattleAbilitySequence();
  CheckAbilityProgress(expected);
  fixture.CheckTerminated();
}

TEST_CASE("Lethal chain credit preserves failure story and threshold gates",
          "[combat][abilities][acquisition][chain-credit]") {
  ChainCreditFixture fixture;
  const int gate = GENERATE(0, 1, 2, 3);
  if (gate == 0) fixture.action.abilityInputSucceeded = 0;
  if (gate == 1) State_SetScriptFlag(0x82, 9);
  if (gate == 2) {
    Audio_Seq_unusedArtsStatus.kills.totalKills = kAbilityThresholds[0];
  }
  if (gate == 3) {
    Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked = 22;
  }
  const auto before = Audio_Seq_unusedArtsStatus;
  Battle_AdvanceBattleAbilitySequence();
  CheckAbilityProgress(before);
  fixture.CheckTerminated();
}

TEST_CASE("Pending chain credit alone does not trigger ability learning",
          "[combat][abilities][acquisition][chain-credit]") {
  ChainCreditFixture fixture;
  Battle_recoveryRewardMode = GENERATE(0, 1);
  Audio_Seq_unusedArtsStatus.kills.totalKills = kAbilityThresholds[0];
  const auto before = Audio_Seq_unusedArtsStatus;
  REQUIRE(Battle_PendingBattleAbilityUnlocks() == 1);
  Battle_BeginPendingAbilityReward();
  CHECK(Battle_actionFlowMode == 0);
  CheckAbilityProgress(before);
}

TEST_CASE("Pending Break Arts take priority over ability rewards",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  const int category = GENERATE(Catch::Generators::range(0, 10));
  const int tier = GENERATE(Catch::Generators::range(0, 4));
  const int category_offset = GENERATE(0, 10);
  const int delta = GENERATE(-1, 0);
  const int unlocked = GENERATE(0, 21, 22);
  fixture.actors[0].equippedWeaponCategory =
      static_cast<u_char>(category + category_offset);
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.artsLearned[category] = static_cast<u_char>(tier);
  progress.kills.weaponCategoryKills[category] =
      static_cast<u_short>(kBreakArtThresholds[category][tier] + delta);
  progress.kills.totalKills = 65535;
  progress.kills.battleAbilitiesUnlocked = static_cast<u_short>(unlocked);
  const auto before = progress;
  CHECK(Battle_PendingBattleAbilityUnlocks() ==
        (delta == 0 ? 0x10000 : (unlocked < 22 ? 1 : 0)));
  CheckAbilityProgress(before);
}

TEST_CASE("Ability reward cap prevents further pending unlocks",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.kills.battleAbilitiesUnlocked =
      static_cast<u_short>(GENERATE(22, 23, 65535));
  progress.kills.totalKills = 65535;
  const auto before = progress;
  CHECK(Battle_PendingBattleAbilityUnlocks() == 0);
  CheckAbilityProgress(before);
}

TEST_CASE("Ineligible ability reward menus leave progression unchanged",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 23));
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.kills.battleAbilitiesUnlocked = static_cast<u_short>(unlocked);
  progress.kills.totalKills = static_cast<u_short>(
      unlocked == 22 ? 65535 : kAbilityThresholds[unlocked] - 1);
  const auto before = progress;
  CHECK(Menu_BattleAbilities_UpdateAbilityUnlock(1) == 1);
  CheckAbilityProgress(before);
}

TEST_CASE("Qualifying lethal actions count once at every Break Art threshold",
          "[combat][break-arts][acquisition]") {
  const unsigned category = GENERATE(Catch::Generators::range(1u, 11u));
  const unsigned tier = GENERATE(Catch::Generators::range(0u, 4u));
  const int action_id = GENERATE(1, 22, 184);
  BreakArtKillFixture fixture(category, tier);
  fixture.action_id = action_id;
  auto expected = Audio_Seq_unusedArtsStatus;
  ++expected.kills.weaponCategoryKills[fixture.counter];
  fixture.Resolve();
  CHECK(fixture.actors[1].killCounted == 1);
  CheckAbilityProgress(expected);
  fixture.Resolve();
  CheckAbilityProgress(expected);
}

TEST_CASE("Break Art kill points preserve original qualification and cap gates",
          "[combat][break-arts][acquisition]") {
  const unsigned category = GENERATE(Catch::Generators::range(1u, 11u));
  const auto gate = GENERATE(
      KillGate::kPreview, KillGate::kPositionSource, KillGate::kEnemySource,
      KillGate::kPositionTarget, KillGate::kDummy, KillGate::kSpell,
      KillGate::kItem, KillGate::kNonlethal, KillGate::kAshleyTarget,
      KillGate::kAlreadyCounted, KillGate::kStory, KillGate::kFourArts,
      KillGate::kPendingThreshold);
  BreakArtKillFixture fixture(category, 0);
  fixture.Reject(gate);
  const auto before = Audio_Seq_unusedArtsStatus;
  fixture.Resolve();
  CheckAbilityProgress(before);
  const bool counted =
      gate == KillGate::kAlreadyCounted || gate == KillGate::kStory ||
      gate == KillGate::kFourArts || gate == KillGate::kPendingThreshold;
  CHECK(fixture.actors[1].killCounted == counted);
  CHECK(fixture.actors[0].killCounted == 0);
}

TEST_CASE("All battle abilities charge original Risk with chain growth and cap",
          "[combat][abilities][cost]") {
  AbilityFixture fixture;
  // BATTLE.PRG _getActionCost, 146C.c:13075.
  const int step = GENERATE(0, 9, 10, 31);
  const int risk = GENERATE(0, 97, 100);
  for (const auto& [id, base_cost] : kAbilityCosts) {
    CAPTURE(id, step, risk);
    fixture.action.chainStep = step;
    fixture.stats[0].risk = risk;
    const int cost =
        base_cost + (id < 40 && step >= 10 ? (step * step) / 10 : 0);
    const int expected = (1 << 24) + (Battle_ActionType_Ability << 16) + cost;
    CHECK(Battle_ActionCostAndAvailability(id, &fixture.stats[0], 0) ==
          expected);
    CHECK(fixture.stats[0].risk == risk);
    CHECK(Battle_ActionCostAndAvailability(id, &fixture.stats[0], 1) ==
          expected);
    CHECK(fixture.stats[0].risk == std::min(100, risk + cost));
    CHECK(fixture.stats[0].currentHp == 600);
    CHECK(fixture.stats[0].currentMp == 120);
  }
}

TEST_CASE("All forty Break Arts require surviving their original HP cost",
          "[combat][break-arts][cost]") {
  AbilityFixture fixture;
  const int action_id = GENERATE(Catch::Generators::range(184, 224));
  const int boundary = GENERATE(0, 1, 2, 3, 4);
  constexpr std::array<int, 4> kCosts{25, 40, 55, 75};
  const int cost = kCosts[(action_id - 184) % 4];
  const std::array<int, 5> health{0, cost - 1, cost, cost + 1, 999};
  const int hp = health[boundary];
  const bool affordable = hp > cost;
  auto& definition = Battle_actionDefinitions[action_id];
  definition.type = Battle_ActionType_BreakArt;
  definition.cost = cost;
  auto& actor = fixture.stats[0];
  actor.currentHp = static_cast<decltype(actor.currentHp)>(hp);
  actor.risk = 7;
  actor.weapon.currentPp = 35;
  const int expected = (affordable ? 1 << 24 : 0) +
                       (Battle_ActionType_BreakArt << 16) + cost +
                       (affordable ? 0 : 1);
  CHECK(Battle_ActionCostAndAvailability(action_id, &actor, 0) == expected);
  CHECK(actor.currentHp == hp);
  CHECK(Battle_ActionCostAndAvailability(action_id, &actor, 1) == expected);
  CHECK(actor.currentHp == (affordable ? hp - cost : hp));
  CHECK(actor.currentMp == 120);
  CHECK(actor.risk == 7);
  CHECK(actor.weapon.currentPp == 35);
}

TEST_CASE(
    "Break Art menu eligibility preserves weapon status and resource gates",
    "[combat][break-arts][eligibility]") {
  AbilityFixture fixture;
  const int action_id = GENERATE(Catch::Generators::range(184, 224));
  const int gate = GENERATE(0, 1, 2, 3, 4, 5, 6);
  constexpr std::array<int, 4> kCosts{25, 40, 55, 75};
  const int cost = kCosts[(action_id - 184) % 4];
  auto& definition = Battle_actionDefinitions[action_id];
  definition.type = Battle_ActionType_BreakArt;
  definition.cost = cost;
  definition.unlocked = gate != 1;
  fixture.actors[0].weaponDrawn = gate != 2;
  auto& actor = fixture.stats[0];
  actor.statuses = gate == 3 ? 0x2000 : (gate == 4 ? 0x1000 : 0);
  actor.actionRecoveryTimer = gate == 5 ? 1 : 0;
  actor.currentHp =
      static_cast<decltype(actor.currentHp)>(gate == 6 ? cost : 999);
  actor.currentMp = 0;
  constexpr std::array<int, 7> kFlags{0, 4, 4, 4, 0, 1, 2};
  const auto before = actor;
  CHECK(Battle_ActorActionRestrictionFlags(0, action_id) == kFlags[gate]);
  CHECK(actor.currentHp == before.currentHp);
  CHECK(actor.currentMp == before.currentMp);
  CHECK(actor.statuses == before.statuses);
  CHECK(actor.actionRecoveryTimer == before.actionRecoveryTimer);
}

TEST_CASE("Ability eligibility requires mapped input",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
  REQUIRE(Battle_CanTriggerBattleAbility(1) == 1);

  SECTION("no mapped offensive buttons") {
    std::fill_n(Input_settings.mappedChainAbilities, 3, 0);
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("no mapped defensive buttons") {
    fixture.Prepare(true);
    REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
    std::fill_n(Input_settings.mappedDefenseAbilities, 3, 0);
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
}

TEST_CASE("Ability eligibility requires a fresh and distinct input",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
  REQUIRE(Battle_CanTriggerBattleAbility(1) == 1);
  SECTION("consecutive buttons must differ") {
    fixture.action.previousChainInput = 1;
    CHECK(Battle_CanTriggerBattleAbility(1) == 0);
    CHECK(Battle_CanTriggerBattleAbility(2) == 1);
  }
  SECTION("only one attempt per action") {
    fixture.action.abilityInputAttempted = 1;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
    CHECK(Battle_CanTriggerBattleAbility(2) == 0);
  }
}

TEST_CASE("Ability eligibility requires real non-dummy actor targets",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
  SECTION("source is a position") {
    fixture.Request().primaryTarget.targetsPosition = 1;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("target is a position") {
    fixture.Request().targets[0].targetsPosition = 1;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("no target") {
    fixture.Request().targetCount = 0;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("training dummy") {
    fixture.actors[1].roomSpawnIndex = 0x80;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
}

TEST_CASE("Ability eligibility requires an appropriate combat relationship",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
  SECTION("self target") {
    fixture.Request().targets[0].target.targetActor = 0;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("enemy action does not target Ashley") {
    fixture.Prepare(true);
    fixture.Request().targets[0].target.targetActor = 1;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("Ashley cannot chain from magic") {
    fixture.Request().actionId = 99;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("defenses cannot continue") {
    for (int id = 40; id <= 53; ++id) {
      CAPTURE(id);
      fixture.Prepare(true);
      fixture.Request().actionId = id;
      CHECK(Battle_CanTriggerBattleAbility(0) == 0);
    }
  }
}

TEST_CASE("Ability timing honors exact difficulty and chain-depth boundaries",
          "[combat][abilities][timing]") {
  // BATTLE.PRG func_80078AB4, 146C.c:7147.
  const int role = GENERATE(0, 1, 2, 3, 4);
  const bool defense = role != 0;
  constexpr std::array kActions{1, 1, 54, 99, 141};
  const int action_id = kActions[role];
  const int difficulty = GENERATE(0, 1);
  const unsigned depth = GENERATE(0u, 3u, 24u, 99u);
  const int slot = GENERATE(1, 2, 3);
  const int margin =
      std::max(2, (difficulty ? 8 : 10) + (action_id >= 54 ? 4 : 0) -
                      static_cast<int>(depth / 3));
  for (int delta : {-margin, -margin + 1, 0, 1, margin, margin + 1, 2 * margin,
                    2 * margin + 1}) {
    CAPTURE(defense, action_id, difficulty, depth, slot, margin, delta);
    AbilityFixture fixture;
    fixture.Prepare(defense, action_id, depth);
    constexpr std::array kButtons{PADRright, PADRup, PADRleft};
    Input_buttonsPressed.allButtons = kButtons[slot - 1];
    State_SetScriptFlag(1, static_cast<char>(difficulty));
    fixture.action.abilityTimer = 100 + delta;
    const auto streak = Audio_Seq_unusedScoredata.chainStreak;
    Battle_UpdateBattleAbilityTiming();
    const bool success = delta > 0 && delta <= margin;
    const int result = delta <= -margin      ? 0
                       : delta <= 0          ? 1
                       : delta <= margin     ? 2
                       : delta <= 2 * margin ? 3
                                             : 4;
    REQUIRE(Battle_floatingLabelCount == 1);
    CHECK(fixture.labels[0].value == result);
    CHECK(fixture.action.abilityInputAttempted == 1);
    fixture.CheckInputResult(success ? (defense ? 2 : 1) : 0,
                             success ? slot : 0);
    CHECK(fixture.action.abilityTimer == 102 + delta);
    CHECK(Audio_Seq_unusedScoredata.chainStreak ==
          streak + (defense && success && streak < 65535 ? 1 : 0));
  }
}

TEST_CASE("Unarmed windows and repeated buttons cannot start an ability",
          "[combat][abilities][timing]") {
  AbilityFixture fixture;
  fixture.action.abilityTimer = 102;
  SECTION("input before a timing window") {
    fixture.action.abilityWindow = 0;
    Battle_UpdateBattleAbilityTiming();
    CHECK(fixture.action.abilityInputAttempted == 1);
    REQUIRE(Battle_floatingLabelCount == 1);
    CHECK(fixture.labels[0].value == 1);
  }
  SECTION("repeat previous chain button") {
    fixture.action.previousChainInput = 1;
    Battle_UpdateBattleAbilityTiming();
    CHECK(fixture.action.abilityInputAttempted == 1);
    CHECK(Battle_floatingLabelCount == 0);
  }
  CHECK(fixture.action.abilityInputSucceeded == 0);
  CHECK(fixture.action.chainAbilitySlot == 0);
}

TEST_CASE("Unmapped buttons and no input cannot start an ability",
          "[combat][abilities][timing]") {
  AbilityFixture fixture;
  fixture.action.abilityTimer = 102;
  SECTION("unmapped button") {
    Input_settings.mappedChainAbilities[0] = 0;
    Battle_UpdateBattleAbilityTiming();
    CHECK(fixture.action.abilityInputAttempted == 0);
    CHECK(Battle_floatingLabelCount == 0);
  }
  SECTION("no button") {
    Input_buttonsPressed.allButtons = 0;
    Battle_UpdateBattleAbilityTiming();
    CHECK(fixture.action.abilityInputAttempted == 0);
    CHECK(Battle_floatingLabelCount == 0);
  }
  CHECK(fixture.action.abilityInputSucceeded == 0);
  CHECK(fixture.action.chainAbilitySlot == 0);
}

TEST_CASE("Simultaneous ability buttons keep the original input priority",
          "[combat][abilities][timing]") {
  const auto [buttons, slot] =
      GENERATE(std::make_pair(PADRright | PADRup, 2),
               std::make_pair(PADRright | PADRleft, 3),
               std::make_pair(PADRup | PADRleft, 3),
               std::make_pair(PADRright | PADRup | PADRleft, 3));
  AbilityFixture fixture;
  fixture.action.abilityTimer = 102;
  Input_buttonsPressed.allButtons = buttons;
  Battle_UpdateBattleAbilityTiming();
  fixture.CheckInputResult(1, slot);
}

TEST_CASE("Successful defensive timing saturates rather than wraps its score",
          "[combat][abilities][timing]") {
  const auto [before, after] =
      GENERATE(std::make_pair(0, 1), std::make_pair(65534, 65535),
               std::make_pair(65535, 65535));
  AbilityFixture fixture;
  fixture.Prepare(true);
  fixture.action.abilityTimer = 102;
  Audio_Seq_unusedScoredata.chainStreak = before;
  Battle_UpdateBattleAbilityTiming();
  fixture.CheckInputResult(2, 1);
  CHECK(Audio_Seq_unusedScoredata.chainStreak == after);
}

TEST_CASE("Single-hit offensive abilities apply their original stat factors",
          "[combat][abilities][ability-effects]") {
  struct Example {
    int id;
    AbilityHit hit;
    int hp_change;
    int mp_change;
  };
  const auto examples = std::to_array<Example>({
      {22, {4, 7, Battle_QueueHitDamageAndEquipmentWear}, -72, 0},
      {23, {18, 3, Battle_AccumulateHpRestoration, true}, 32, 0},
      {24, {4, 3, Battle_AccumulateMpDamage}, 0, -32},
      {25, {18, 3, Battle_AccumulateMpRestoration, true}, 0, 32},
      {26, {5, 1, Battle_QueueHitDamageAndEquipmentWear}, -42, 0},
      {27, {11, 2, Battle_AccumulateMpDamage}, 0, -42},
  });
  for (const auto& example : examples) {
    CAPTURE(example.id);
    AbilityEffectFixture fixture;
    fixture.Queue(example.id, 0, example.hit);
    fixture.Commit();
    fixture.CheckResources(600 + (example.hit.self ? example.hp_change : 0),
                           100 + (example.hit.self ? example.mp_change : 0),
                           800 + (example.hit.self ? 0 : example.hp_change),
                           200 + (example.hit.self ? 0 : example.mp_change));
  }
}

TEST_CASE("Temper damages the target and restores weapon DP",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(28, 0, {4, 4, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Queue(28, 1, {18, 20, Battle_AccumulateWeaponDpRestoration, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 758, 200);
  CHECK(fixture.stats[0].weapon.currentDp == 304);
}

TEST_CASE("Crimson Pain applies both enemy and self HP damage",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(29, 0, {6, 10, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Queue(29, 1, {6, 3, Battle_QueueHitDamageAndEquipmentWear, true});
  fixture.Commit();
  fixture.CheckResources(580, 100, 718, 200);
}

TEST_CASE("Instill damages the target and restores weapon PP",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(30, 0, {4, 1, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Queue(30, 1, {4, 10, Battle_AccumulateWeaponPpRestoration, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 788, 200);
  CHECK(fixture.stats[0].weapon.currentPp == 173);
}

TEST_CASE("Phantom Pain converts weapon PP to damage and empties it",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(31, 0, {8, 0, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Queue(31, 1, {1, 0, Battle_AccumulateWeaponPpDrain, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 728, 200);
  CHECK(fixture.stats[0].weapon.currentPp == 0);
}

TEST_CASE("Siphon Soul restores half the incoming spell cost as MP",
          "[combat][abilities][ability-effects]") {
  const bool spell = GENERATE(false, true);
  AbilityEffectFixture fixture;
  fixture.action.selection.actionId = spell ? 99 : 1;
  fixture.Queue(41, 0, {21, 5, Battle_AccumulateMpRestoration, true});
  fixture.Commit();
  fixture.CheckResources(600, spell ? 112 : 100, 800, 200);
}

TEST_CASE("Reflect abilities return damage only for the matching action type",
          "[combat][abilities][ability-effects]") {
  const int id = GENERATE(42, 43);
  const bool spell = GENERATE(false, true);
  const int incoming_effect = GENERATE(0, 1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.actionId = spell ? 99 : 1;
  fixture.action.selection.primaryTarget.hpEffect = incoming_effect;
  fixture.Queue(id, 0,
                {id == 42 ? 9 : 10, 4, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Commit();
  const bool applies =
      (spell == (id == 42)) && (incoming_effect == 1 || incoming_effect == 3);
  fixture.CheckResources(600, 100, applies ? 756 : 800, 200);
}

TEST_CASE("Absorb abilities recover damage only for the matching action type",
          "[combat][abilities][ability-effects]") {
  const int id = GENERATE(44, 45);
  const bool spell = GENERATE(false, true);
  const int incoming_effect = GENERATE(0, 1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.actionId = spell ? 99 : 1;
  fixture.action.selection.primaryTarget.hpEffect = incoming_effect;
  fixture.Queue(id, 0,
                {id == 44 ? 19 : 20, 2,
                 id == 44 ? Battle_QueueSpellTriggeredRecovery
                          : Battle_QueuePhysicalTriggeredRecovery,
                 true});
  fixture.Commit();
  const bool applies =
      (spell == (id == 44)) && (incoming_effect == 1 || incoming_effect == 3);
  fixture.CheckResources(applies ? 622 : 600, 100, 800, 200);
}

TEST_CASE("All seven affinity defenses recover only matching damage",
          "[combat][abilities][ability-effects]") {
  constexpr std::array<AbilityEffect, 7> kEffects{
      Battle_QueuePhysicalAffinityRecovery, Battle_QueueAirAffinityRecovery,
      Battle_QueueFireAffinityRecovery,     Battle_QueueEarthAffinityRecovery,
      Battle_QueueWaterAffinityRecovery,    Battle_QueueLightAffinityRecovery,
      Battle_QueueDarkAffinityRecovery};
  const int affinity = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int incoming = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int incoming_effect = GENERATE(1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.effectVariant = static_cast<char>(incoming);
  fixture.action.selection.primaryTarget.hpEffect = incoming_effect;
  fixture.Queue(46 + affinity, 0, {22, 5, kEffects[affinity], true});
  fixture.Commit();
  const bool applies = affinity == incoming && incoming_effect != 2;
  fixture.CheckResources(applies ? 655 : 600, 100, 800, 200);
}

TEST_CASE("Phantom Shield converts shield PP to recovery only after damage",
          "[combat][abilities][ability-effects]") {
  const int incoming_effect = GENERATE(0, 1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.primaryTarget.hpEffect = incoming_effect;
  fixture.Queue(53, 0, {23, 0, Battle_QueueDamageTriggeredRecovery, true});
  fixture.Queue(53, 1, {1, 0, Battle_AccumulateShieldPpDrain, true});
  fixture.Commit();
  const bool applies = incoming_effect == 1 || incoming_effect == 3;
  fixture.CheckResources(applies ? 690 : 600, 100, 800, 200);
  CHECK(fixture.stats[0].shield.currentPp == (applies ? 0 : 90));
}

TEST_CASE("Offensive status abilities queue and apply their original ailments",
          "[combat][abilities][ability-effects]") {
  struct Example {
    int id;
    int mask;
    AbilityEffect apply;
  };
  constexpr auto kExamples = std::to_array<Example>({
      {32, 0x2000, Battle_QueueParalysis},
      {34, 0x8000, Battle_QueueNumbness},
      {35, 0x1000, Battle_QueueSilence},
      {37, 0x4000, Battle_QueuePoison},
  });
  for (const auto& example : kExamples) {
    CAPTURE(example.id);
    AbilityEffectFixture fixture;
    fixture.Queue(example.id, 0, {0, 0, example.apply});
    CHECK(fixture.target.removeStatuses == example.mask);
    Battle_InflictActorStatuses(fixture.target.removeStatuses,
                                &fixture.stats[1]);
    CHECK(fixture.stats[1].statuses == example.mask);
    CHECK(fixture.stats[0].statuses == 0);
    fixture.CheckResources(600, 100, 800, 200);
  }
}

TEST_CASE("Ward removes paralysis and numbness without curing poison",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  Battle_InflictActorStatuses(0xE000, &fixture.stats[0]);
  fixture.Queue(40, 0, {0, 0, Battle_QueueParalysisCure, true});
  fixture.Queue(40, 1, {0, 0, Battle_QueueNumbnessCure, true});
  CHECK(fixture.source.inflictStatuses == 0xA000);
  Battle_RemoveActorStatuses(fixture.source.inflictStatuses, &fixture.stats[0]);
  CHECK(fixture.stats[0].statuses == 0x4000);
  CHECK(fixture.stats[0].walkSpeed == 8);
  CHECK(fixture.stats[0].runSpeed == 12);
  fixture.CheckResources(600, 100, 800, 200);
}

TEST_CASE("Restorative abilities respect HP MP and equipment capacity",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  SECTION("Gain Life") {
    fixture.stats[0].currentHp = 995;
    fixture.Queue(23, 0, {18, 3, Battle_AccumulateHpRestoration, true});
    fixture.Commit();
    CHECK(fixture.stats[0].currentHp == 1000);
  }
  SECTION("Gain Magic") {
    fixture.stats[0].currentMp = 299;
    fixture.Queue(25, 0, {18, 3, Battle_AccumulateMpRestoration, true});
    fixture.Commit();
    CHECK(fixture.stats[0].currentMp == 300);
  }
  SECTION("Temper") {
    fixture.stats[0].weapon.currentDp = 995;
    fixture.Queue(28, 1, {18, 20, Battle_AccumulateWeaponDpRestoration, true});
    fixture.Commit();
    CHECK(fixture.stats[0].weapon.currentDp == 1000);
  }
  SECTION("Instill") {
    fixture.stats[0].weapon.currentPp = 250;
    fixture.Queue(30, 1, {4, 10, Battle_AccumulateWeaponPpRestoration, true});
    fixture.Commit();
    CHECK(fixture.stats[0].weapon.currentPp == 255);
  }
}

TEST_CASE("MP damage abilities cannot drain more MP than the target has",
          "[combat][abilities][ability-effects]") {
  const int id = GENERATE(24, 27);
  const int mp = GENERATE(0, 10, 200);
  AbilityEffectFixture fixture;
  fixture.stats[1].currentMp =
      static_cast<decltype(fixture.stats[1].currentMp)>(mp);
  fixture.Queue(
      id, 0, {id == 24 ? 4 : 11, id == 24 ? 3 : 2, Battle_AccumulateMpDamage});
  const int damage = std::min(mp, id == 24 ? 32 : 42);
  CHECK(fixture.target.target.mp == damage);
  CHECK(Battle_lastActionStatValue == damage);
  fixture.Commit();
  fixture.CheckResources(600, 100, 800, mp - damage);
}

TEST_CASE("Two-hit Break Arts accumulate both original physical hit contracts",
          "[combat][break-arts][art-effects]") {
  for (const auto& art : kTwoHitArts) {
    CAPTURE(art.id);
    ArtResolutionFixture fixture;
    auto& definition = Battle_actionDefinitions[art.id];
    definition.id = static_cast<decltype(definition.id)>(art.id);
    definition.type = Battle_ActionType_BreakArt;
    definition.hits[0].damageType = art.damage_type;
    definition.hits[1].damageType = art.damage_type;
    definition.hits[0].affinity = art.first_affinity;
    definition.hits[1].affinity = art.second_affinity;
    for (int hit = 0; hit < 2; ++hit) {
      definition.hits[hit].effectId = 58;
      definition.hits[hit].prerequisiteFunctionId = hit == 0 ? 2 : 1;
      definition.hits[hit].hitFunctionId = hit == 0 ? 3 : 1;
    }
    const int first_affinity = std::max(0, art.first_affinity - 1);
    const int second_affinity = std::max(0, art.second_affinity - 1);
    const int first = 150 * (100 + 10 * art.factor) / 100 - 60 -
                      6 * (art.damage_type + first_affinity);
    const int second = 150 - 60 - 6 * (art.damage_type + second_affinity);
    CHECK(first > 0);
    CHECK(second > 0);
    fixture.Queue(art.id, 0,
                  {25, art.factor, Battle_QueueHitDamageAndEquipmentWear});
    CHECK(fixture.target.target.hp == first);
    fixture.Queue(art.id, 1, {25, 0, Battle_QueueHitDamageAndEquipmentWear});
    CHECK(fixture.target.target.hp == first + second);
    fixture.CheckResolvedHits(art.id, first + second);
    fixture.Commit();
    fixture.CheckResources(600, 100, 800 - first - second, 200);
  }
}

TEST_CASE("Armor-wearing arts saturate both original DP application stages",
          "[combat][break-arts][art-effects]") {
  const int id = GENERATE(202, 214);
  const int limb = GENERATE(Catch::Generators::range(0, 6));
  const int dp = GENERATE(0, 1, 39, 40, 41, 79, 80, 81, 65535);
  AbilityEffectFixture fixture;
  for (auto& target_limb : fixture.stats[1].limbs) {
    target_limb.armor.currentDp = 1234;
  }
  auto& armor_dp = fixture.stats[1].limbs[limb].armor.currentDp;
  auto& shield_dp = fixture.stats[1].shield.currentDp;
  armor_dp = shield_dp = static_cast<std::uint16_t>(dp);
  fixture.target.target.targetLimb =
      static_cast<decltype(fixture.target.target.targetLimb)>(limb);
  fixture.target.target.hp = 20;
  fixture.target.hpEffect = 1;
  fixture.Queue(id, 1, {0, 0, Battle_QueueDamageBasedArmorWear});
  CHECK(fixture.target.armorDpChange == 40);
  CHECK(fixture.target.limbs[limb].effects.opaque2 == 40);
  fixture.Commit();
  CHECK(armor_dp == std::max(0, dp - 80));
  CHECK(shield_dp == std::max(0, dp - 40));
  for (int index = 0; index < 6; ++index) {
    if (index != limb) {
      CHECK(fixture.stats[1].limbs[index].armor.currentDp == 1234);
    }
  }
  fixture.CheckResources(600, 100, 780, 200);
}

TEST_CASE("Rejected loot transfers leave partial misc stacks unchanged",
          "[combat][loot][inventory-capacity]") {
  LootInventoryFixture fixture;
  CHECK(fixture.Copy(false) == 0);
  CHECK(static_cast<int>(Audio_Seq_unusedInventory.misc[0].count) == 99);
  CHECK(fixture.Copy(true) == 0);
  CHECK(static_cast<int>(Audio_Seq_unusedInventory.misc[0].count) == 99);
  CHECK(fixture.source.misc[0].count == 2);
  Audio_Seq_unusedInventory.misc[63] = {};
  CHECK(fixture.Copy(true) == 64);
  CHECK(Audio_Seq_unusedInventory.misc[0].count +
            Audio_Seq_unusedInventory.misc[63].count ==
        101);
}

TEST_CASE("Misc loot capacity queries and writes preserve stack limits",
          "[combat][loot][inventory-capacity]") {
  const bool free_slot = GENERATE(false, true);
  const int first = GENERATE(1, 98, 99, 100);
  const int second = GENERATE(-1, 1, 99, 100);
  const int incoming = GENERATE(1, 2, 100);
  LootInventoryFixture fixture;
  auto& inventory = Audio_Seq_unusedInventory;
  inventory.misc[0].count =
      static_cast<decltype(inventory.misc[0].count)>(first);
  if (second >= 0) {
    inventory.misc[1].id = 323;
    inventory.misc[1].count =
        static_cast<decltype(inventory.misc[1].count)>(second);
  }
  if (free_slot) inventory.misc[63] = {};
  fixture.source.misc[0].count =
      static_cast<decltype(fixture.source.misc[0].count)>(incoming);
  const int stack_space = 100 - first + (second >= 0 ? 100 - second : 0);
  const int capacity = stack_space + (free_slot ? 100 : 0);
  const int expected =
      incoming <= stack_space ? 1 : (incoming <= capacity ? 64 : 0);
  fixture.CheckQueryUnchanged(expected);
  fixture.CheckWrite(expected);
}

TEST_CASE("Unmatched misc loot uses only available stock slots",
          "[combat][loot][inventory-capacity]") {
  const int occupied = GENERATE(0, 1, 63, 64);
  LootInventoryFixture fixture;
  Audio_Seq_unusedInventory = {};
  for (int slot = 0; slot < occupied; ++slot) {
    Audio_Seq_unusedInventory.misc[slot].id = 324;
    Audio_Seq_unusedInventory.misc[slot].count = 100;
  }
  const int expected = occupied < 64 ? occupied + 1 : 0;
  fixture.CheckQueryUnchanged(expected);
  fixture.CheckWrite(expected);
}

TEST_CASE("Guaranteed enemy items retain both entries without random draws",
          "[combat][loot][drop-rules]") {
  const int present = GENERATE(0, 1, 2, 3);
  LootRandomFixture fixture;
  VS::Core::Random expected;
  std::array<Items_MiscItemStack, 2> source{};
  source[0].id = (present & 1) != 0 ? 323 : 0;
  source[0].count = 2;
  source[1].id = (present & 2) != 0 ? 324 : 0;
  source[1].count = 3;
  Items_BattleLoot loot{};
  loot.misc[2].itemId = 325;
  loot.misc[2].count = 1;
  loot.misc[2].difficultyFlags = 2;
  CHECK(Items_CopyGuaranteedEnemyItemDrops(&loot, source.data()) ==
        (present != 0));
  CheckLootItem(loot.misc[0], source[0].id, source[0].id != 0 ? 2 : 0,
                source[0].id != 0 ? 3 : 0);
  CheckLootItem(loot.misc[1], source[1].id, source[1].id != 0 ? 3 : 0,
                source[1].id != 0 ? 3 : 0);
  CheckLootItem(loot.misc[2], 325, 1, 2);
  fixture.CheckSequence(expected);
}

TEST_CASE("Random enemy items use the strict original 255-value threshold",
          "[combat][loot][drop-rules]") {
  const int roll = GENERATE(Catch::Generators::range(0, 255));
  const int rate = GENERATE_COPY(0, roll, roll + 1, 255);
  LootRandomFixture fixture;
  fixture.ForceFirstRoll(roll);
  Items_MiscItemDrop source{};
  source.id = 325;
  source.dropRate = static_cast<decltype(source.dropRate)>(rate);
  Items_BattleLoot loot{};
  const bool success = roll < rate;
  CHECK(Items_RollEnemyItemDrop(&loot, &source) == success);
  CheckLootItem(loot.misc[2], 325, success ? 1 : 0, success ? 3 : 0);
  fixture.CheckOneForcedDraw();
}

TEST_CASE("Absent random enemy items consume no draw",
          "[combat][loot][drop-rules]") {
  LootRandomFixture fixture;
  VS::Core::Random expected;
  Items_MiscItemDrop source{};
  source.dropRate = static_cast<decltype(source.dropRate)>(255);
  Items_BattleLoot loot{};
  CHECK(Items_RollEnemyItemDrop(&loot, &source) == 0);
  CheckLootItem(loot.misc[2], 0, 0, 0);
  fixture.CheckSequence(expected);
}

TEST_CASE("Enemy loot rolls equipment before its optional item candidate",
          "[combat][loot][drop-rules]") {
  const int guaranteed = GENERATE(0, 1, 2);
  const bool candidate = GENERATE(false, true);
  const int armor_count = GENERATE(0, 1, 3, 6);
  const int above_roll = GENERATE(0, 1);
  EnemyLootFixture fixture;
  fixture.SetGuaranteed(guaranteed);
  for (int index = 0; index < armor_count; ++index) {
    fixture.stats.limbs[index].armor.armor.id = 1;
  }
  VS::Core::Random expected;
  for (int index = 0; index < 3 + armor_count; ++index) expected.Next(255);
  if (candidate) {
    fixture.stats.randomDropItem.id = 325;
    fixture.stats.randomDropItem.dropRate =
        static_cast<decltype(fixture.stats.randomDropItem.dropRate)>(
            expected.Next(255) + above_roll);
  }
  const bool success = candidate && above_roll != 0;
  Items_QueueEnemyLoot(&fixture.enemy);
  CHECK((Items_lootListHead != nullptr) == (guaranteed != 0 || success));
  if (Items_lootListHead != nullptr) {
    fixture.CheckQueued(guaranteed, candidate, success);
    Menu_Loot_BuildStagedInventory(Items_lootListHead);
    fixture.DeliverRows(guaranteed + (success ? 1 : 0));
  }
  CHECK(LootInventoryFixture::Quantity(323) == (guaranteed > 0 ? 2 : 0));
  CHECK(LootInventoryFixture::Quantity(324) == (guaranteed > 1 ? 3 : 0));
  CHECK(LootInventoryFixture::Quantity(325) == (success ? 1 : 0));
  fixture.CheckCleared();
  fixture.CheckCleared();
  fixture.random.CheckSequence(expected);
}

TEST_CASE("Defeat eligibility prevents repeat item rewards and random draws",
          "[combat][loot][drop-rules]") {
  const int guard = GENERATE(0, 1, 2);
  const int guaranteed = GENERATE(0, 1);
  const bool retained = GENERATE(false, true);
  EnemyLootFixture fixture;
  fixture.SetGuaranteed(guaranteed);
  fixture.enemy.retainAfterDefeat = retained ? 1 : 0;
  fixture.enemy.roomSpawnIndex = guard == 1 ? 0x80 : 1;
  fixture.enemy.defeated = guard == 2 ? 1 : 0;
  VS::Core::Random expected;
  if (guard == 0) {
    for (int index = 0; index < 3; ++index) expected.Next(255);
  }
  Battle_ResolveEnemyDefeat(0, 1, 0);
  auto* const first = Items_lootListHead;
  Battle_ResolveEnemyDefeat(0, 1, 0);
  CHECK(Items_lootListHead == first);
  CHECK((first != nullptr) == (guard == 0 && guaranteed != 0));
  CHECK(fixture.enemy.defeated == (guard != 1));
  fixture.random.CheckSequence(expected);
  fixture.CheckCleared();
}

TEST_CASE("Direct duplicate loot queues release rejected nodes after rolling",
          "[combat][loot][drop-rules]") {
  EnemyLootFixture fixture;
  fixture.SetGuaranteed(1);
  VS::Core::Random expected;
  Items_QueueEnemyLoot(&fixture.enemy);
  REQUIRE(Items_lootListHead != nullptr);
  auto* const first = Items_lootListHead;
  const auto free_units = Core_legacyHeapList.next->blockSize;
  Items_QueueEnemyLoot(&fixture.enemy);
  CHECK(Items_lootListHead == first);
  CHECK(first->next == nullptr);
  CHECK(Core_legacyHeapList.next->blockSize == free_units);
  auto second_enemy = fixture.enemy;
  second_enemy.id = 2;
  Items_QueueEnemyLoot(&second_enemy);
  REQUIRE(Items_lootListHead != first);
  CHECK(Items_lootListHead->droppedMask == 2);
  CHECK(Items_lootListHead->next == first);
  for (int index = 0; index < 9; ++index) expected.Next(255);
  fixture.random.CheckSequence(expected);
  fixture.CheckCleared();
}

TEST_CASE("Loot staging filters difficulty and incomplete misc candidates",
          "[combat][loot][drop-rules]") {
  const int difficulty = GENERATE(0, 1);
  const int flags = GENERATE(0, 1, 2, 3);
  const bool has_id = GENERATE(false, true);
  const bool has_count = GENERATE(false, true);
  EnemyLootFixture fixture;
  Audio_Seq_unusedStateFlags.difficulty = static_cast<char>(difficulty);
  Items_BattleLoot loot{};
  loot.misc[0].itemId = has_id ? 323 : 0;
  loot.misc[0].count = has_count ? 2 : 0;
  loot.misc[0].difficultyFlags = static_cast<char>(flags);
  loot.misc[1].itemId = 324;
  loot.misc[1].count = 3;
  loot.misc[1].difficultyFlags = 3;
  loot.misc[2].itemId = 325;
  const bool eligible = has_id && has_count && (flags & (difficulty + 1)) != 0;
  Menu_Loot_StageDropRecord(&loot);
  fixture.DeliverRows(eligible ? 2 : 1);
  CHECK(LootInventoryFixture::Quantity(323) == (eligible ? 2 : 0));
  CHECK(LootInventoryFixture::Quantity(324) == 3);
  CHECK(LootInventoryFixture::Quantity(325) == 0);
  VS::Core::Random expected;
  fixture.random.CheckSequence(expected);
}

TEST_CASE("Prologue progression suppresses presentation without losing loot",
          "[combat][loot][drop-rules]") {
  const int intro = GENERATE(0, 1, 2);
  const int guaranteed = GENERATE(0, 1);
  LootPresentationFixture fixture;
  fixture.SetGuaranteed(guaranteed);
  State_SetScriptFlag(0xAB, static_cast<char>(intro));
  Items_QueueEnemyLoot(&fixture.enemy);
  auto* const queued = Items_lootListHead;
  Items_OpenPendingEnemyLoot();
  CHECK(Field_controlMode == 12);
  CHECK(Battle_actionFlowMode == 0);
  CHECK(Menu_Framework_lootLists == nullptr);
  CHECK(Menu_Framework_lootEntry == nullptr);
  CHECK(Items_lootListHead == queued);
  if (queued != nullptr) fixture.CheckQueued(1, false, false);
  fixture.CheckCleared();
}

TEST_CASE("Composite equipment loot respects component capacities atomically",
          "[combat][loot][equipment-drops]") {
  const bool shield = GENERATE(false, true);
  const int parents = GENERATE(0, 7, 8);
  const int blades = GENERATE_COPY(shield ? 0 : parents, 15, 16);
  const int grips = GENERATE_COPY(shield ? 0 : parents, 15, 16);
  const int gems = GENERATE(0, 45, 46, 47, 48);
  const int attached = GENERATE(0, 1, 2, 3);
  EquipmentLootFixture fixture(shield, attached,
                               {parents, blades, grips, gems});
  fixture.CheckTransfer();
}

TEST_CASE("Weapon drops preserve properties at every original rate boundary",
          "[combat][loot][equipment-drops]") {
  const int roll = GENERATE(Catch::Generators::range(0, 255));
  const int rate = GENERATE_COPY(0, roll, roll + 1, 255);
  CheckEquipmentRoll(roll, rate, Items_RollEnemyWeaponDrop);
}

TEST_CASE("Shield drops preserve properties at every original rate boundary",
          "[combat][loot][equipment-drops]") {
  const int roll = GENERATE(Catch::Generators::range(0, 255));
  const int rate = GENERATE_COPY(0, roll, roll + 1, 255);
  CheckEquipmentRoll(roll, rate, Items_RollEnemyShieldDrop);
}

TEST_CASE("Armor drops preserve properties at every original rate boundary",
          "[combat][loot][equipment-drops]") {
  const int roll = GENERATE(Catch::Generators::range(0, 255));
  const int rate = GENERATE_COPY(0, roll, roll + 1, 255);
  CheckEquipmentRoll(roll, rate, Items_RollEnemyArmorDrop);
}

TEST_CASE("Accessory drops preserve properties at every original rate boundary",
          "[combat][loot][equipment-drops]") {
  const int roll = GENERATE(Catch::Generators::range(0, 255));
  const int rate = GENERATE_COPY(0, roll, roll + 1, 255);
  CheckEquipmentRoll(roll, rate, Items_RollEnemyAccessoryDrop);
}

TEST_CASE("Six limb rolls retain only the first two successful armor drops",
          "[combat][loot][equipment-drops]") {
  const int present = GENERATE(0, 1, 2, 3, 0x15, 0x3F);
  const int eligible = GENERATE(0, 0x2A, 0x3F);
  EnemyLootFixture fixture;
  VS::Core::Random expected;
  for (int index = 0; index < 3; ++index) expected.Next(255);
  std::vector<int> selected;
  for (int index = 0; index < 6; ++index) {
    auto& source = fixture.stats.limbs[index].armor;
    if ((present & (1 << index)) != 0) {
      source.armor = EquipmentDropPart(static_cast<u_short>(149 + index));
      expected.Next(255);
    }
    if ((eligible & (1 << index)) != 0) {
      source.dropRate = static_cast<char>(255);
      if (source.armor.id != 0 && selected.size() < 2) {
        selected.push_back(index);
      }
    }
  }
  Items_QueueEnemyLoot(&fixture.enemy);
  CHECK((Items_lootListHead != nullptr) == !selected.empty());
  if (Items_lootListHead != nullptr) {
    const auto& loot = Items_lootListHead->loot;
    for (std::size_t index = 0; index < selected.size(); ++index) {
      CheckEquipmentDrop(loot.armor[index],
                         fixture.stats.limbs[selected[index]].armor);
    }
    if (selected.size() == 1) CheckZeroedDrop(loot.armor[1]);
  }
  fixture.random.CheckSequence(expected);
  fixture.CheckCleared();
}

TEST_CASE("Armor and accessory claims honor the original shared bag capacity",
          "[combat][loot][equipment-drops]") {
  const int count = GENERATE(0, 15, 16);
  const int id = GENERATE(149, 237);
  LootInventoryFixture fixture;
  Audio_Seq_unusedInventory = {};
  fixture.source = {};
  auto part = EquipmentDropPart(static_cast<u_short>(id));
  Items_WriteInventoryArmorStats(&fixture.source.armor[0], &part);
  fixture.source.armor[0].index = 1;
  for (int index = 0; index < count; ++index) {
    Audio_Seq_unusedInventory.armor[index].id = 150;
    Audio_Seq_unusedInventory.armor[index].index =
        static_cast<u_char>(index + 1);
  }
  auto expected = Audio_Seq_unusedInventory;
  const auto source_before = fixture.source;
  CHECK(Items_Inventory_CopyItem(
            Items_ItemCategory_Armor, &Audio_Seq_unusedInventory, 0,
            &fixture.source) == (count < 16 ? count + 1 : 0));
  EquipmentLootFixture::CheckInventory(Audio_Seq_unusedInventory, expected);
  CHECK(Items_Inventory_CopyItem(
            static_cast<int>(Items_ItemCategory_Armor) |
                static_cast<int>(Menu_Framework_CopyItemFlag_Write),
            &Audio_Seq_unusedInventory, 0,
            &fixture.source) == (count < 16 ? count + 1 : 0));
  if (count < 16) {
    expected.armor[count] = fixture.source.armor[0];
    expected.armor[count].index = static_cast<u_char>(count + 1);
    expected.armor[count].limb = 0;
  }
  EquipmentLootFixture::CheckInventory(Audio_Seq_unusedInventory, expected);
  EquipmentLootFixture::CheckInventory(fixture.source, source_before);
}

TEST_CASE(
    "Equipment loot staging preserves rows and refreshes bag availability",
    "[combat][loot][equipment-drops]") {
  const int difficulty = GENERATE(0, 1);
  const int flags = GENERATE(0, 1, 2, 3);
  const int armor_count = GENERATE(0, 15, 16);
  EquipmentStagingFixture fixture;
  Audio_Seq_unusedStateFlags.difficulty = static_cast<char>(difficulty);
  for (int index = 0; index < armor_count; ++index) {
    Audio_Seq_unusedInventory.armor[index].id = 150;
    Audio_Seq_unusedInventory.armor[index].index =
        static_cast<u_char>(index + 1);
  }
  fixture.Stage(flags);
  const bool allowed = (flags & (difficulty + 1)) != 0;
  REQUIRE(Menu_Loot_lootListCount == (allowed ? 5 : 0));
  if (allowed) {
    fixture.Claim(armor_count);
    fixture.CheckWeaponParts();
    fixture.CheckShieldParts();
  } else {
    CheckZeroedDrop(fixture.source);
  }
  VS::Core::Random expected;
  for (int index = 0; index < 5; ++index) expected.Next(255);
  fixture.random.CheckSequence(expected);
  fixture.CheckCleared();
}

TEST_CASE("Loot staging uses every slot of the original inventory capacities",
          "[combat][loot][staging-capacity]") {
  const int category = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const std::array capacities{8, 16, 16, 8, 16, 48, 64};
  EnemyLootFixture fixture;
  Items_BattleLoot loot{};
  loot.weapon.blade.id = 14;
  loot.weapon.grip.id = 96;
  loot.blade.blade.id = 14;
  loot.grip.grip.id = 96;
  loot.shield.shield.id = 147;
  loot.armor[0].armor.id = 149;
  loot.gem.gem.id = 300;
  loot.misc[0].itemId = 323;
  loot.misc[0].count = 1;
  const std::array<int (*)(Items_BattleLoot&), 7> stage{
      [](Items_BattleLoot& value) {
        return Menu_Loot_StageWeapon(&value.weapon);
      },
      [](Items_BattleLoot& value) {
        return Menu_Loot_StageBlade(&value.blade.blade, 0);
      },
      [](Items_BattleLoot& value) {
        return Menu_Loot_StageGrip(&value.grip.grip, 0);
      },
      [](Items_BattleLoot& value) {
        return Menu_Loot_StageShield(&value.shield);
      },
      [](Items_BattleLoot& value) {
        return Menu_Loot_StageArmor(&value.armor[0].armor);
      },
      [](Items_BattleLoot& value) {
        return Menu_Loot_StageGem(&value.gem.gem, 0);
      },
      [](Items_BattleLoot& value) {
        return Menu_Loot_StageMiscItem(&value.misc[0]);
      }};
  for (int index = 0; index < capacities[category]; ++index) {
    CAPTURE(category, index);
    CHECK(stage[category](loot) == index + 1);
    CHECK(Items_Inventory_CountItems(category, &fixture.source) == index + 1);
  }
  const auto full = fixture.source;
  CHECK(stage[category](loot) == 0);
  EquipmentLootFixture::CheckInventory(fixture.source, full);
}

TEST_CASE("Composite loot staging preflights all component storage",
          "[combat][loot][staging-capacity]") {
  const bool shield = GENERATE(false, true);
  const int parents = GENERATE(0, 7, 8);
  const int blades = GENERATE_COPY(shield ? 0 : parents, 15, 16);
  const int grips = GENERATE_COPY(shield ? 0 : parents, 15, 16);
  const int gems = GENERATE(0, 45, 46, 47, 48);
  const int attached = GENERATE(0, 1, 2, 3);
  EnemyLootFixture fixture;
  EquipmentLootFixture::FillTarget(fixture.source, shield,
                                   {parents, blades, grips, gems});
  Items_BattleLoot loot{};
  auto part = EquipmentDropPart(14);
  Items_WriteInventoryBladeStats(&loot.weapon.blade, &part);
  part.id = 96;
  Items_WriteInventoryGripStats(&loot.weapon.grip, &part);
  part.id = 147;
  Items_WriteInventoryArmorStats(&loot.shield.shield, &part);
  for (int index = 0; index < attached; ++index) {
    part.id = static_cast<u_short>(300 + index);
    Items_WriteInventoryGemStats(&loot.weapon.gems[index], &part);
    Items_WriteInventoryGemStats(&loot.shield.gems[index], &part);
  }
  const auto before = fixture.source;
  const auto input_before = loot;
  const bool fits = parents < 8 && gems + attached <= 48 &&
                    (shield || (blades < 16 && grips < 16));
  const int result = shield ? Menu_Loot_StageShield(&loot.shield)
                            : Menu_Loot_StageWeapon(&loot.weapon);
  CHECK(result == (fits ? parents + 1 : 0));
  CHECK(std::memcmp(&loot, &input_before, sizeof(loot)) == 0);
  if (!fits) EquipmentLootFixture::CheckInventory(fixture.source, before);
  CHECK(Items_Inventory_CountItems(Items_ItemCategory_Gem, &fixture.source) ==
        gems + (fits ? attached : 0));
}

TEST_CASE(
    "Oversized enemy armor batches retain deferred loot without duplicates",
    "[combat][loot][staging-capacity]") {
  const bool claim_first = GENERATE(false, true);
  EquipmentStagingFixture fixture;
  fixture.QueueArmorBatch();
  Menu_Loot_BuildStagedInventory(Items_lootListHead);
  REQUIRE(Menu_Loot_lootListCount == 16);
  std::vector<int> seen;
  fixture.CollectArmorPage(seen, claim_first);
  fixture.AdvanceArmorPage(claim_first);
  fixture.CollectArmorPage(seen, true);
  fixture.CheckArmorBatchComplete(seen);
}

TEST_CASE("Pending loot pages reset safely before source release and reopen",
          "[combat][loot][staging-capacity]") {
  EquipmentStagingFixture fixture;
  fixture.QueueArmorBatch();
  Menu_Loot_BuildStagedInventory(Items_lootListHead);
  REQUIRE(Menu_Loot_lootListCount == 16);
  const auto first = Items_lootListHead->loot;
  Menu_Loot_ResetPaging();
  CHECK(Menu_Loot_AdvanceStagedInventory() == 0);
  CHECK(std::memcmp(&Items_lootListHead->loot, &first, sizeof(first)) == 0);
  Menu_Loot_BuildStagedInventory(Items_lootListHead);
  REQUIRE(Menu_Loot_lootListCount == 16);
  fixture.CheckCleared();
  CHECK(Menu_Loot_AdvanceStagedInventory() == 0);

  fixture.stats = {};
  fixture.SetGuaranteed(1);
  Items_QueueEnemyLoot(&fixture.enemy);
  Menu_Loot_BuildStagedInventory(Items_lootListHead);
  fixture.DeliverRows(1);
  CHECK(Menu_Loot_AdvanceStagedInventory() == 0);
  fixture.CheckCleared();
}

TEST_CASE("Single chest loot remains intact when staging is abandoned",
          "[combat][loot][staging-capacity]") {
  EnemyLootFixture fixture;
  Items_BattleLoot chest{};
  chest.misc[0] = {323, 3, 2};
  chest.misc[1] = {467, 3, 1};
  Menu_Framework_lootEntry = &chest;
  const auto before = chest;
  Menu_Loot_BuildStagedInventory(nullptr);
  REQUIRE(Menu_Loot_lootListCount == 2);
  CHECK(Menu_Loot_AdvanceStagedInventory() == 0);
  Menu_Loot_ResetPaging();
  CHECK(std::memcmp(&chest, &before, sizeof(chest)) == 0);
  Menu_Loot_BuildStagedInventory(nullptr);
  fixture.DeliverRows(2);
  CHECK(std::memcmp(&chest, &before, sizeof(chest)) == 0);
}

TEST_CASE("Loot menu cancellation retains the page until positive confirmation",
          "[combat][loot][staging-capacity][loot-paging-ui]") {
  LootPagingMenuFixture fixture;
  fixture.QueueArmorBatch();
  Menu_Loot_BuildStagedInventory(Items_lootListHead);
  const auto before = fixture.source;
  const auto queued = Items_lootListHead->loot;
  fixture.OpenLeaveConfirmation();
  fixture.Step(PADRdown);
  CHECK(Menu_Loot_lootListCount == 16);
  EquipmentLootFixture::CheckInventory(fixture.source, before);
  CHECK(std::memcmp(&Items_lootListHead->loot, &queued, sizeof(queued)) == 0);
  fixture.SettleRows();
  fixture.Step();
  fixture.OpenLeaveConfirmation();
  fixture.Step(PADLup);
  fixture.Step(PADRright);
  fixture.CheckNextArmorPage();
}

TEST_CASE("Loot menu take-all advances only after the current claims finish",
          "[combat][loot][staging-capacity][loot-paging-ui]") {
  LootPagingMenuFixture fixture;
  fixture.QueueArmorBatch();
  Menu_Loot_BuildStagedInventory(Items_lootListHead);
  fixture.Step(PADRleft);
  CHECK(Menu_Loot_lootListCount == 16);
  for (int tick = 0; tick < 10 && Menu_Loot_lootListCount != 8; ++tick) {
    fixture.Step();
  }
  REQUIRE(Menu_Loot_lootListCount == 8);
  fixture.CheckClaims(16, 16);
  Audio_Seq_unusedInventory = {};
  Menu_Loot_RefreshClaimableRows();
  fixture.Step(PADRleft);
  int result = 0;
  for (int tick = 0; tick < 10 && result == 0; ++tick) {
    result = fixture.Tick();
  }
  CHECK(result == 1);
  fixture.CheckClaims(24, 8);
}

TEST_CASE("Loot menu cannot abandon a key while another page is pending",
          "[combat][loot][staging-capacity][loot-paging-ui]") {
  LootPagingMenuFixture fixture;
  fixture.QueueArmorBatch();
  Items_lootListHead->loot.misc[0] = {467, 3, 1};
  Menu_Loot_BuildStagedInventory(Items_lootListHead);
  REQUIRE(Menu_Loot_lootListCount == 17);
  const auto before = fixture.source;
  const auto queued = Items_lootListHead->loot;
  CHECK(fixture.Tick(PADRdown) == 0);
  CHECK(fixture.Tick() == 0);
  CHECK(fixture.Tick(PADRdown) == 0);
  CHECK(Menu_Loot_lootListCount == 17);
  EquipmentLootFixture::CheckInventory(fixture.source, before);
  CHECK(std::memcmp(&Items_lootListHead->loot, &queued, sizeof(queued)) == 0);
  CHECK(Menu_Loot_lootClaimed == 0);
}
