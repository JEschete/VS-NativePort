#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Actors/ActorRecord.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SfxChannelGroups.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/ActionEffectDispatchLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionPreviewLive.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/AttackPrerequisiteTable.hpp"
#include "Battle/AttackPrerequisitesLive.hpp"
#include "Battle/BasicActionEffectsLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/DamageCalculationsLive.hpp"
#include "Battle/DamagePoliciesLive.hpp"
#include "Battle/DamageRecoveryEffectsLive.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Battle/HitChance.hpp"
#include "Battle/HitModifiersLive.hpp"
#include "Battle/StatFactorsLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Random.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/PurgeTables.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/Templates/RingsLive.hpp"
#include "Field/Interactions.hpp"
#include "Input/Buttons.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Settings.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/Inventory.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Items/Loot.hpp"
#include "Items/LootLive.hpp"
#include "Items/LootLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/InventoryTerminalWorld.hpp"
#include "Menu/Loot/LootRowsLive.hpp"
#include "Menu/Loot/LootScreen.hpp"
#include "Menu/Loot/LootStagingLive.hpp"
#include "Menu/Loot/LootTerminalLive.hpp"
#include "Menu/Loot/Paging.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/KillScoreLive.hpp"
#include "State/Progress.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;
using VS::Battle::kActionTypeAbility;
using VS::Battle::kActionTypeBaseAttack;
using VS::Battle::kActionTypeBreakArt;
using VS::Battle::kActionTypeItem;
using VS::Battle::kActionTypeSpell;

using VS::Legacy::ClearOTagR;
using VS::Legacy::kPadLup;
using VS::Legacy::kPadRdown;
using VS::Legacy::kPadRleft;
using VS::Legacy::kPadRright;
using VS::Legacy::kPadRup;

int Tests_CombatTargetResults(void);

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
constexpr std::array<std::array<std::uint16_t, 4>, 10> kBreakArtThresholds{{
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
constexpr std::array<std::uint16_t, 22> kAbilityThresholds{{
    20,  54,   72,   95,   125,  165,  216,  283,  369,  482,  628,
    818, 1066, 1387, 1805, 2349, 3055, 3973, 5167, 6719, 8737, 11359,
}};

struct AbilityFixture {
  decltype(VS::Battle::actionDefinitions) saved_definitions =
      VS::Battle::actionDefinitions;
  ActionState* saved_action = VS::Battle::actionState;
  VS::Battle::Actor* saved_character = VS::Battle::characterState;
  std::array<VS::Battle::Actor*, 2> saved_actors{VS::Battle::actors[0],
                                                 VS::Battle::actors[1]};
  std::array<VS::Actors::ActorRecord*, 2> saved_records{
      VS::Actors::actorRecords[0], VS::Actors::actorRecords[1]};
  VS::Input::Settings saved_settings = VS::Input::settings;
  decltype(VS::Input::buttonsPressed) saved_pressed = VS::Input::buttonsPressed;
  decltype(VS::State::scoreData) saved_score = VS::State::scoreData;
  decltype(VS::Battle::floatingLabels) saved_labels =
      VS::Battle::floatingLabels;
  decltype(VS::Battle::floatingLabelCount) saved_label_count =
      VS::Battle::floatingLabelCount;
  int saved_tick_speed = VS::State::tickSpeed;
  std::uint8_t saved_difficulty = VS::State::LiveScriptFlag(1);
  ActionState action{};
  VS::Battle::Actor actors[2]{};
  VS::Battle::ActorStats stats[2]{};
  VS::Actors::ActorRecord records[2]{};
  VS::Battle::FloatingLabelSlots labels{};

  AbilityFixture() {
    VS::Battle::actionState = &action;
    VS::Battle::characterState = &actors[0];
    for (int index = 0; index < 2; ++index) {
      VS::Battle::actors[index] = &actors[index];
      VS::Actors::actorRecords[index] = &records[index];
      actors[index].id = index;
      actors[index].stats = &stats[index];
      actors[index].room_spawn_index = 1;
      stats[index].actor_id = index;
      stats[index].current_hp = 600;
      stats[index].current_mp = 120;
      records[index].instance.action_animation_state = 1;
    }
    std::fill_n(VS::Battle::actionDefinitions.data(), saved_definitions.size(),
                ActionDefinition{});
    for (const auto& [id, cost] : kAbilityCosts) {
      VS::Battle::actionDefinitions[id].type = kActionTypeAbility;
      VS::Battle::actionDefinitions[id].cost = cost;
      VS::Battle::actionDefinitions[id].unlocked = 1;
    }
    VS::Battle::actionDefinitions[1].type = kActionTypeBaseAttack;
    VS::Battle::actionDefinitions[54].type = kActionTypeSpell;
    VS::Battle::actionDefinitions[99].type = kActionTypeSpell;
    VS::Battle::actionDefinitions[141].type = kActionTypeBreakArt;
    VS::Input::settings = {};
    for (int index = 0; index < 3; ++index) {
      VS::Input::settings.mapped_chain_abilities[index] =
          static_cast<char>(22 + index);
      VS::Input::settings.mapped_defense_abilities[index] =
          static_cast<char>(40 + index);
    }
    VS::Input::buttonsPressed = kPadRright;
    VS::Battle::floatingLabels = &labels;
    VS::Battle::floatingLabelCount = 0;
    VS::State::tickSpeed = 2;
    VS::State::SetLiveScriptFlag(1, 0);
    Prepare();
  }

  ~AbilityFixture() {
    std::copy(saved_definitions.begin(), saved_definitions.end(),
              VS::Battle::actionDefinitions.data());
    VS::Battle::actionState = saved_action;
    VS::Battle::characterState = saved_character;
    for (int index = 0; index < 2; ++index) {
      VS::Battle::actors[index] = saved_actors[index];
      VS::Actors::actorRecords[index] = saved_records[index];
    }
    VS::Input::settings = saved_settings;
    VS::Input::buttonsPressed = saved_pressed;
    VS::State::scoreData = saved_score;
    VS::Battle::floatingLabels = saved_labels;
    VS::Battle::floatingLabelCount = saved_label_count;
    VS::State::tickSpeed = saved_tick_speed;
    VS::State::SetLiveScriptFlag(1, saved_difficulty);
  }

  void Prepare(bool defense = false, int action_id = 1, unsigned index = 0) {
    action = {};
    action.queue_index = index;
    auto& request = Request();
    request.action_id = action_id;
    request.primary_target.target.target_actor = defense ? 1 : 0;
    request.target_count = 1;
    request.targets[0].target.target_actor = defense ? 0 : 1;
    action.ability_window = 100;
  }

  void PrepareLimbResults(int limb_count, unsigned queue_index,
                          bool filtered_entries) {
    Prepare(false, 1, queue_index);
    VS::Actors::actorRecords[1] = nullptr;
    auto& target = stats[1];
    target.current_hp = target.max_hp = 63;
    target.current_mp = target.max_mp = 40;
    for (auto& limb : target.limbs) {
      limb.hp = limb.max_hp = 20;
    }
    auto& request = Request();
    const int first_limb = filtered_entries ? 2 : 0;
    request.target_count = first_limb + limb_count;
    if (filtered_entries) {
      auto& other_actor = request.targets[0];
      other_actor.target.target_actor = 0;
      other_actor.target.hp = 50;
      other_actor.hp_effect = 1;
      auto& world_point = request.targets[1];
      world_point = other_actor;
      world_point.targets_position = 1;
      world_point.target.target_actor = 1;
    }
    for (int limb = 0; limb < limb_count; ++limb) {
      auto& hit = request.targets[first_limb + limb];
      const auto amount = static_cast<std::int16_t>(limb + 1);
      hit.target.target_actor = 1;
      hit.target.target_limb = static_cast<char>(limb);
      hit.target.hp = hit.target.mp = amount;
      hit.hp_effect = hit.mp_effect = 1;
      hit.limbs[limb].effects.hp_change = amount;
    }
  }

  ActionRequestRecord& Request() {
    return action.queue[action.queue_index & 3];
  }

  void CheckInputResult(int success, int slot) const {
    CHECK(action.ability_input_succeeded == success);
    CHECK(action.chain_ability_slot == slot);
    CHECK(action.previous_chain_input == slot);
  }
};

using MediatorTable = decltype(VS::Battle::actionEffectMediators);
using NativeAbilityEffect = MediatorTable::value_type;
using AbilityEffect = NativeAbilityEffect;

struct AbilityHit {
  int calculator;
  int factor;
  AbilityEffect apply;
  bool self = false;
};

struct AbilityEffectFixture : AbilityFixture {
  decltype(VS::Battle::statCalculators) saved_calculators =
      VS::Battle::statCalculators;
  decltype(VS::Battle::limbArmorDpLoss) saved_limb_damage =
      VS::Battle::limbArmorDpLoss;
  decltype(VS::Battle::statusEffectParams) saved_status_params =
      VS::Battle::statusEffectParams;
  std::array<VS::Battle::Ai::ActorAiState*, 2> saved_ai{
      VS::Battle::Ai::actorAiStates[0], VS::Battle::Ai::actorAiStates[1]};
  std::uint32_t saved_last_value = VS::Battle::lastActionStatValue;
  int saved_critical = VS::Battle::criticalHitOccurred;
  VS::Battle::Ai::ActorAiState ai[2]{};
  VS::Battle::HitResult source{};
  VS::Battle::HitResult target{};

  AbilityEffectFixture() {
    action.queue_index = 3;
    action.selection.action_id = 99;
    action.selection.primary_target.hp_effect = 1;
    action.selection.primary_target.target.hp = 101;
    action.selection.primary_target.target.mp = 9;
    action.selection.targets[0].target.hp = 83;
    action.selection.targets[0].target.mp = 18;
    VS::Battle::actionDefinitions[99].type = kActionTypeSpell;
    VS::Battle::actionDefinitions[99].cost = 25;
    VS::Battle::actionDefinitions[99].unlocked = 1;
    VS::Battle::InitializeLiveActionHit(&source);
    VS::Battle::InitializeLiveActionHit(&target);
    target.target.target_actor = 1;
    for (int index = 0; index < 2; ++index) {
      VS::Battle::Ai::actorAiStates[index] = &ai[index];
      auto& actor = stats[index];
      actor.max_hp = 1000;
      actor.max_mp = 300;
      actor.current_hp = index == 0 ? 600 : 800;
      actor.current_mp = index == 0 ? 100 : 200;
      actor.walk_speed = actor.base_walk_speed = 8;
      actor.run_speed = actor.base_run_speed = 12;
      actor.weapon.blade.id = 1;
      actor.weapon.current_dp = 100;
      actor.weapon.max_dp = 1000;
      actor.weapon.current_pp = 70;
      actor.weapon.max_pp = 255;
      actor.shield.base.id = 1;
      actor.shield.current_pp = 90;
      actor.shield.max_pp = 255;
      for (int limb = 0; limb < 6; ++limb) {
        actor.limbs[limb].hp = 50;
        actor.limbs[limb].max_hp = 100;
        actor.dp_splash[limb][limb] = 100;
      }
    }
    BindCalculators();
    VS::Battle::statusEffectParams[12] = {0x1000, 0, 0, 0x1000, 0};
    VS::Battle::statusEffectParams[13] = {0x2000, 15, 0, 0x2000, 0};
    VS::Battle::statusEffectParams[14] = {0x4000, 0, 2, 0x20000, 0};
    VS::Battle::statusEffectParams[15] = {0x8000, 15, 0, 0x8800, 0x10};
  }

  ~AbilityEffectFixture() {
    std::copy(saved_calculators.begin(), saved_calculators.end(),
              VS::Battle::statCalculators.begin());
    std::copy(saved_limb_damage.begin(), saved_limb_damage.end(),
              VS::Battle::limbArmorDpLoss.begin());
    std::copy(saved_status_params.begin(), saved_status_params.end(),
              VS::Battle::statusEffectParams.begin());
    VS::Battle::lastActionStatValue = saved_last_value;
    VS::Battle::criticalHitOccurred = saved_critical;
    for (int index = 0; index < 2; ++index) {
      VS::Battle::Ai::actorAiStates[index] = saved_ai[index];
    }
  }

  static void BindCalculators() {
    // BATTLE.PRG _statCalculators, original pointer table at file 0x7FB0C.
    VS::Battle::statCalculators[1] = VS::Battle::LiveLastActionStatValue;
    VS::Battle::statCalculators[4] = VS::Battle::LiveScalePreviousTargetHit;
    VS::Battle::statCalculators[5] = VS::Battle::LiveScaleSourceMissingHp;
    VS::Battle::statCalculators[6] = VS::Battle::LiveScaleTargetCurrentHp;
    VS::Battle::statCalculators[8] = VS::Battle::LiveWeaponPpWithChainOffset;
    VS::Battle::statCalculators[9] = VS::Battle::LiveScaleIncomingSpellDamage;
    VS::Battle::statCalculators[10] =
        VS::Battle::LiveScaleIncomingPhysicalDamage;
    VS::Battle::statCalculators[11] = VS::Battle::LiveScaleSourceMissingMp;
    VS::Battle::statCalculators[18] = VS::Battle::LiveScalePreviousTargetHit;
    VS::Battle::statCalculators[19] = VS::Battle::LiveScaleIncomingSpellDamage;
    VS::Battle::statCalculators[20] =
        VS::Battle::LiveScaleIncomingPhysicalDamage;
    VS::Battle::statCalculators[21] = VS::Battle::LiveScaleIncomingSpellCost;
    VS::Battle::statCalculators[22] = VS::Battle::LiveScaleIncomingDamage;
    VS::Battle::statCalculators[23] = VS::Battle::LiveSourceShieldPp;
  }

  void Queue(int id, int hit, const AbilityHit& contract) {
    auto& definition = VS::Battle::actionDefinitions[id];
    definition.category = id <= 31 ? 14 : 0;
    definition.hits[hit].stat_calculator_id = contract.calculator;
    definition.hits[hit].stat_factor = contract.factor;
    contract.apply(&definition, &source, contract.self ? &source : &target, hit,
                   0);
  }

  void Commit() {
    VS::Battle::ApplyLiveActorActionResult(&source);
    VS::Battle::ApplyLiveActorActionResult(&target);
  }

  void CheckResources(int source_hp, int source_mp, int target_hp,
                      int target_mp) const {
    CHECK(stats[0].current_hp == source_hp);
    CHECK(stats[0].current_mp == source_mp);
    CHECK(stats[1].current_hp == target_hp);
    CHECK(stats[1].current_mp == target_mp);
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
  decltype(VS::Battle::hitFunctions) saved_hits = VS::Battle::hitFunctions;
  decltype(VS::Battle::attackPrerequisiteFunctions) saved_prerequisites =
      VS::Battle::attackPrerequisiteFunctions;
  decltype(VS::Battle::actionEffectMediators) saved_effects =
      VS::Battle::actionEffectMediators;
  std::uint32_t saved_previous_hit = VS::Battle::previousHitResult;
  std::uint32_t saved_prerequisite_result = VS::Battle::hitPrerequisiteResult;

  ArtResolutionFixture() {
    // BATTLE.PRG dispatch tables at file 0x7FA34, 0x7FA54 and 0x7FB78.
    VS::Battle::hitFunctions[1] = VS::Battle::PreviousLiveHitSuccessThreshold;
    VS::Battle::hitFunctions[3] = VS::Battle::LiveAgilityHitChance;
    VS::Battle::attackPrerequisiteFunctions[1] =
        VS::Battle::AcceptAttackPrerequisiteSlot;
    VS::Battle::attackPrerequisiteFunctions[2] =
        VS::Battle::AcceptAttackPrerequisiteSlot;
    VS::Battle::actionEffectMediators[58] =
        VS::Battle::QueueLiveHitDamageAndEquipmentWear;
    VS::Battle::statCalculators[25] = VS::Battle::LivePhysicalDamageWithDebuffs;
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
    std::copy(saved_hits.begin(), saved_hits.end(),
              VS::Battle::hitFunctions.begin());
    std::copy(saved_prerequisites.begin(), saved_prerequisites.end(),
              VS::Battle::attackPrerequisiteFunctions.begin());
    std::copy(saved_effects.begin(), saved_effects.end(),
              VS::Battle::actionEffectMediators.begin());
    VS::Battle::previousHitResult = saved_previous_hit;
    VS::Battle::hitPrerequisiteResult = saved_prerequisite_result;
  }

  ActionRequestRecord Resolve(int id) {
    ActionRequestRecord input{};
    ActionRequestRecord result{};
    input.target_count = 1;
    input.targets[0].target.target_actor = 1;
    VS::Battle::ResolveLiveActionTargets(id, &result, &input, 0);
    return result;
  }

  void CheckResolvedHits(int id, int damage) {
    const auto resolved = Resolve(id);
    REQUIRE(resolved.target_count == 1);
    CHECK(resolved.targets[0].target.hp == damage);
    CHECK(resolved.targets[0].hp_effect == 1);
    stats[1].cancels_hit_results = 1;
    actors[1].room_spawn_index = 0x80;
    const auto cancelled = Resolve(id);
    CHECK(cancelled.targets[0].target.hp == 0);
    CHECK(cancelled.targets[0].target.impact_kind == 4);
    target = resolved.targets[0];
  }
};

struct LootInventoryFixture {
  VS::Items::InventoryContents saved_inventory = VS::Items::inventory;
  VS::Items::InventoryIndices saved_indices = VS::Items::inventoryIndices;
  std::uint8_t* saved_index_pointer = VS::Menu::Inventory::categoryOrders[6];
  VS::Items::InventoryContents source{};

  LootInventoryFixture() {
    VS::Items::inventory = {};
    VS::Items::inventoryIndices = {};
    VS::Menu::Inventory::categoryOrders[6] =
        VS::Items::inventoryIndices.misc.data();
    for (auto& item : VS::Items::inventory.misc) {
      item.id = 324;
      item.count = 100;
    }
    VS::Items::inventory.misc[0].id = 323;
    VS::Items::inventory.misc[0].count = 99;
    source.misc[0].id = 323;
    source.misc[0].count = 2;
  }

  ~LootInventoryFixture() {
    VS::Items::inventory = saved_inventory;
    VS::Items::inventoryIndices = saved_indices;
    VS::Menu::Inventory::categoryOrders[6] = saved_index_pointer;
  }

  int Copy(bool write) {
    const int flags = static_cast<int>(VS::Items::ItemCategory::kMisc) |
                      (write ? VS::Items::Inventory::kCopyItemWrite : 0);
    return VS::Items::Inventory::CopyLiveItem(flags, &VS::Items::inventory, 0,
                                              &source);
  }

  static int Quantity(int id) {
    int quantity = 0;
    for (const auto& item : VS::Items::inventory.misc) {
      if (item.id == id) quantity += item.count;
    }
    return quantity;
  }

  static void CheckUnchanged(
      const decltype(VS::Items::inventory.misc)& before) {
    for (std::size_t slot = 0; slot < before.size(); ++slot) {
      CHECK(VS::Items::inventory.misc[slot].id == before[slot].id);
      CHECK(VS::Items::inventory.misc[slot].count == before[slot].count);
    }
  }

  void CheckQueryUnchanged(int expected) {
    const auto before = VS::Items::inventory.misc;
    CHECK(Copy(false) == expected);
    CheckUnchanged(before);
  }

  void CheckWrite(int expected) {
    const auto inventory_before = VS::Items::inventory.misc;
    const int before = Quantity(323);
    const int other = Quantity(324);
    const auto incoming = source.misc[0].count;
    CHECK(Copy(true) == expected);
    if (expected == 0) CheckUnchanged(inventory_before);
    CHECK(Quantity(323) == before + (expected != 0 ? incoming : 0));
    CHECK(Quantity(324) == other);
    CHECK(source.misc[0].id == 323);
    CHECK(source.misc[0].count == incoming);
    for (const auto& item : VS::Items::inventory.misc) {
      CHECK(item.count >= 0);
      CHECK(item.count <= 100);
    }
  }
};

struct LootRandomFixture {
  decltype(std::to_array(VS::Core::randomShuffleTable)) saved_table =
      std::to_array(VS::Core::randomShuffleTable);
  int saved_index = VS::Core::randomShuffleIndex;

  LootRandomFixture() { VS::Core::InitializeLiveRandomShuffleTable(); }

  ~LootRandomFixture() {
    std::copy(saved_table.begin(), saved_table.end(),
              VS::Core::randomShuffleTable);
    VS::Core::randomShuffleIndex = saved_index;
    VS::Core::SeedBiosRand(1);
  }

  static void CheckSequence(VS::Core::Random& expected) {
    CHECK(VS::Core::NextLiveRandomBounded(255) == expected.Next(255));
    CHECK(VS::Core::NextBiosRand() == expected.Raw());
  }

  static void ForceFirstRoll(int roll) {
    std::fill_n(VS::Core::randomShuffleTable, 97, 0);
    VS::Core::randomShuffleIndex = 96;
    VS::Core::randomShuffleTable[0] = 32767 - (roll * 32768 + 254) / 255;
    VS::Core::SeedBiosRand(1234);
  }

  static void CheckOneForcedDraw() {
    VS::Core::Rand15 expected(1234);
    CHECK(VS::Core::randomShuffleIndex == 0);
    CHECK(VS::Core::randomShuffleTable[0] == expected.Next());
    CHECK(VS::Core::NextBiosRand() == expected.Next());
    for (int index = 1; index < 97; ++index) {
      CHECK(VS::Core::randomShuffleTable[index] == 0);
    }
  }
};

void CheckLootItem(const VS::Items::LootedItem& item, int id, int count,
                   int flags) {
  CHECK(item.item_id == id);
  CHECK(static_cast<int>(item.count) == count);
  CHECK(static_cast<int>(item.difficulty_flags) == flags);
}

struct EnemyLootFixture : LootInventoryFixture {
  LootRandomFixture random;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 384> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  VS::Items::LootListNode* saved_head = VS::Items::lootListHead;
  VS::Battle::Actor* saved_actor = VS::Battle::actors[1];
  VS::Items::InventoryContents* saved_staging = VS::Menu::Loot::stagedInventory;
  VS::Items::BattleLoot* saved_entry = VS::Menu::Framework::lootEntry;
  VS::Menu::Loot::LootRow* saved_rows = VS::Menu::Loot::lootList;
  std::uint8_t saved_count = VS::Menu::Loot::lootListCount;
  std::uint8_t saved_offset = VS::Menu::Loot::lootListOffset;
  std::uint8_t saved_selection = VS::Menu::Loot::selectedLoot;
  std::uint8_t saved_difficulty = VS::State::stateFlags.difficulty;
  std::array<VS::Menu::Loot::LootRow, VS::Menu::Loot::kRowCapacity> rows{};
  VS::Battle::Actor enemy{};
  VS::Battle::ActorStats stats{};

  EnemyLootFixture() {
    VS::Items::lootListHead = nullptr;
    enemy.id = 1;
    enemy.room_spawn_index = 1;
    enemy.stats = &stats;
    VS::Battle::actors[1] = &enemy;
    VS::Items::inventory = {};
    source = {};
    VS::Menu::Loot::stagedInventory = &source;
    VS::Menu::Framework::lootEntry = nullptr;
    VS::Menu::Loot::lootList = rows.data();
    VS::Menu::Loot::lootListCount = 0;
    VS::Menu::Loot::lootListOffset = 0;
    VS::Menu::Loot::selectedLoot = 0;
    VS::State::stateFlags.difficulty = 0;
  }

  ~EnemyLootFixture() {
    VS::Items::ClearLiveRoomLootList();
    VS::Items::lootListHead = saved_head;
    VS::Battle::actors[1] = saved_actor;
    VS::Menu::Loot::stagedInventory = saved_staging;
    VS::Menu::Framework::lootEntry = saved_entry;
    VS::Menu::Loot::lootList = saved_rows;
    VS::Menu::Loot::lootListCount = saved_count;
    VS::Menu::Loot::lootListOffset = saved_offset;
    VS::Menu::Loot::selectedLoot = saved_selection;
    VS::State::stateFlags.difficulty = saved_difficulty;
  }

  void SetGuaranteed(int count) {
    if (count > 0) {
      stats.drop_item.id = 323;
      stats.drop_item.count = 2;
    }
    if (count > 1) {
      stats.second_drop_item.id = 324;
      stats.second_drop_item.count = 3;
    }
  }

  void CheckQueued(int guaranteed, bool candidate, bool success) {
    REQUIRE(VS::Items::lootListHead != nullptr);
    CHECK(VS::Items::lootListHead->actor_id == enemy.id);
    CHECK(VS::Items::lootListHead->next == nullptr);
    const auto& loot = VS::Items::lootListHead->loot;
    CheckLootItem(loot.misc[0], guaranteed > 0 ? 323 : 0,
                  guaranteed > 0 ? 2 : 0, guaranteed > 0 ? 3 : 0);
    CheckLootItem(loot.misc[1], guaranteed > 1 ? 324 : 0,
                  guaranteed > 1 ? 3 : 0, guaranteed > 1 ? 3 : 0);
    CheckLootItem(loot.misc[2], candidate ? 325 : 0, success ? 1 : 0,
                  success ? 3 : 0);
  }

  void DeliverRows(int count) {
    REQUIRE(VS::Menu::Loot::lootListCount == count);
    for (int index = 0; index < count; ++index) {
      const auto& row = rows[index];
      CHECK(row.category == static_cast<int>(VS::Items::ItemCategory::kMisc));
      CHECK(row.state == 1);
      CHECK(row.slide_step == 0);
      const int flags =
          static_cast<int>(row.category) | VS::Items::Inventory::kCopyItemWrite;
      CHECK(VS::Items::Inventory::CopyLiveItem(flags, &VS::Items::inventory,
                                               row.item_index, &source) != 0);
    }
  }

  void CheckCleared() {
    VS::Items::ClearLiveRoomLootList();
    CHECK(VS::Items::lootListHead == nullptr);
    CHECK(VS::Core::Memory::GameHeap().Empty());
  }
};

VS::Items::EquipmentPart EquipmentDropPart(std::uint16_t id) {
  VS::Items::EquipmentPart part{};
  part.id = id;
  part.sub_id = 7;
  part.weapon_model_id = 44;
  part.category = 2;
  part.strength = 12;
  part.intelligence = -5;
  part.agility = 6;
  part.current_dp = 1234;
  part.max_dp = 9876;
  part.current_pp = 345;
  part.max_pp = 654;
  part.damage_type = 7;
  part.cost_type = 15;
  part.cost = 11;
  part.material = 5;
  part.recovery_delay = 0x42;
  part.gem_slots = 3;
  part.gem_effects = 5;
  part.range = {.range = 5, .second_range = 7, .third_range = 9};
  part.range.SetAttackShape(2);
  part.range.SetAttackShapeAngle(13);
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
void CheckDropIdentity(const Item& item, const VS::Items::EquipmentPart& part) {
  CHECK(item.id == part.id);
  CHECK(static_cast<std::uint8_t>(item.sub_id) ==
        static_cast<std::uint8_t>(part.sub_id));
  CHECK(item.strength == part.strength);
  CHECK(item.intelligence == part.intelligence);
  CHECK(item.agility == part.agility);
}

template <typename Item>
void CheckDropDurability(const Item& item,
                         const VS::Items::EquipmentPart& part) {
  CHECK(static_cast<std::uint8_t>(item.weapon_model_id) ==
        static_cast<std::uint8_t>(part.weapon_model_id));
  CHECK(static_cast<std::uint8_t>(item.category) ==
        static_cast<std::uint8_t>(part.category));
  CHECK(item.current_dp == part.current_dp);
  CHECK(item.max_dp == part.max_dp);
  CHECK(item.current_pp == part.current_pp);
  CHECK(item.max_pp == part.max_pp);
  CHECK(item.material == part.material);
}

template <typename Item>
void CheckDropAffinities(const Item& item,
                         const VS::Items::EquipmentPart& part) {
  for (int index = 0; index < 8; ++index) {
    CHECK(item.classes[index] == (index < 6 ? part.classes[index] : 0));
    CHECK(item.affinities[index] == (index < 7 ? part.affinities[index] : 0));
  }
}

template <typename Item>
void CheckDropTypes(const Item& item, const VS::Items::EquipmentPart& part) {
  for (int index = 0; index < 4; ++index) {
    CHECK(item.types[index] == part.types[index]);
  }
}

template <typename Item>
void CheckZeroedDrop(const Item& item) {
  const Item empty{};
  CHECK(std::memcmp(&item, &empty, sizeof(item)) == 0);
}

void CheckBladeDrop(const VS::Items::InventoryBlade& item,
                    const VS::Items::EquipmentPart& part) {
  CheckDropIdentity(item, part);
  CheckDropDurability(item, part);
  CheckDropAffinities(item, part);
  CHECK(static_cast<std::uint8_t>(item.cost) == part.cost);
  CHECK(item.DamageType() == (part.damage_type & 3));
  CHECK(item.CostType() == (part.cost_type & 7));
  CHECK(item.recovery_delay == part.recovery_delay);
  CHECK(std::memcmp(&item.range, &part.range, sizeof(item.range)) == 0);
}

void CheckGripDrop(const VS::Items::InventoryGrip& item,
                   const VS::Items::EquipmentPart& part) {
  CheckDropIdentity(item, part);
  CheckDropTypes(item, part);
  CHECK(static_cast<std::uint8_t>(item.category) ==
        static_cast<std::uint8_t>(part.category));
  CHECK(static_cast<std::uint8_t>(item.gem_slots) == part.gem_slots);
}

void CheckArmorDrop(const VS::Items::InventoryArmor& item,
                    const VS::Items::EquipmentPart& part) {
  CheckDropIdentity(item, part);
  CheckDropDurability(item, part);
  CheckDropTypes(item, part);
  CheckDropAffinities(item, part);
  CHECK(static_cast<std::uint8_t>(item.gem_slots) == part.gem_slots);
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
      CHECK(static_cast<std::uint8_t>(gem.gem_effects) == part.gem_effects);
    }
  }
}

void PrepareEquipmentDrop(VS::Items::WeaponLoadout& source) {
  source.blade = EquipmentDropPart(14);
  source.grip = EquipmentDropPart(96);
  PrepareDropGems(source);
  source.current_dp = 9;
  source.current_pp = 8;
  for (int index = 0; index < 23; ++index) {
    source.name[index] = static_cast<char>(10 + index);
  }
  source.name[23] = static_cast<char>(0xE7);
}

void PrepareEquipmentDrop(VS::Items::ShieldLoadout& source) {
  source.base = EquipmentDropPart(147);
  PrepareDropGems(source);
  source.current_dp = 9;
  source.current_pp = 8;
}

void PrepareEquipmentDrop(VS::Items::ArmorLoadout& source) {
  source.armor = EquipmentDropPart(149);
  source.current_dp = 9;
}

void PrepareEquipmentDrop(VS::Items::AccessorySpawnLoadout& source) {
  source.accessory = EquipmentDropPart(237);
}

void CheckEquipmentDrop(const VS::Items::LootedWeapon& drop,
                        const VS::Items::WeaponLoadout& source) {
  CHECK(drop.header.difficulty_flags == 3);
  CheckBladeDrop(drop.blade, source.blade);
  CheckGripDrop(drop.grip, source.grip);
  CheckDropGems(drop, source);
  CHECK(drop.name == source.name);
}

void CheckEquipmentDrop(const VS::Items::LootedShield& drop,
                        const VS::Items::ShieldLoadout& source) {
  CHECK(drop.header.difficulty_flags == 3);
  CheckArmorDrop(drop.shield, source.base);
  CheckDropGems(drop, source);
}

void CheckEquipmentDrop(const VS::Items::LootedArmor& drop,
                        const VS::Items::ArmorLoadout& source) {
  CHECK(drop.header.difficulty_flags == 3);
  CheckArmorDrop(drop.armor, source.armor);
}

void CheckEquipmentDrop(const VS::Items::LootedArmor& drop,
                        const VS::Items::AccessorySpawnLoadout& source) {
  CHECK(drop.header.difficulty_flags == 3);
  CheckArmorDrop(drop.armor, source.accessory);
}

// The typed loot rolls (Loot.hpp) over the legacy records, drawing from the
// game's random sequence as the battle loot does.
template <typename Drop, typename Loadout, auto Roll>
int RollLegacyDrop(Drop* looted, Loadout* base) {
  VS::Items::Live::LiveItemRandom random;
  auto typed = *looted;
  const bool dropped = Roll(typed, *base, random);
  *looted = typed;
  return dropped ? 1 : 0;
}

int RollLegacyArmorDrop(VS::Items::LootedArmor* looted,
                        VS::Items::ArmorLoadout* base) {
  VS::Items::Live::LiveItemRandom random;
  std::array typed{looted[0], looted[1]};
  const bool dropped = VS::Items::RollEnemyArmorDrop(typed, *base, random);
  looted[0] = typed[0];
  looted[1] = typed[1];
  return dropped ? 1 : 0;
}

bool CopyLegacyGuaranteedDrops(
    VS::Items::BattleLoot& loot,
    const std::array<VS::Items::MiscItemStack, 2>& source) {
  auto typed = loot;
  const std::array typed_source{source[0], source[1]};
  const bool copied =
      VS::Items::CopyGuaranteedEnemyItemDrops(typed, typed_source);
  loot = typed;
  return copied;
}

bool RollLegacyItemDrop(VS::Items::BattleLoot& loot,
                        const VS::Items::MiscItemDrop& source) {
  VS::Items::Live::LiveItemRandom random;
  auto typed = loot;
  const bool dropped = VS::Items::RollEnemyItemDrop(typed, source, random);
  loot = typed;
  return dropped;
}

template <typename Drop, typename Loadout>
void CheckEquipmentRoll(int roll, int rate, int (*drop)(Drop*, Loadout*)) {
  LootRandomFixture random;
  random.ForceFirstRoll(roll);
  Loadout source{};
  PrepareEquipmentDrop(source);
  source.drop_rate = static_cast<decltype(source.drop_rate)>(rate);
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
  source.drop_rate = static_cast<decltype(source.drop_rate)>(255);
}

template <typename Item, typename Loadout>
void CheckClaimedGems(const Item& item, const Loadout& source, int parent) {
  for (int index = 0; index < 3; ++index) {
    const int slot = item.gems[index];
    const auto& part = source.gems[index];
    CHECK((slot != 0) == (part.id != 0));
    CHECK(slot <= 48);
    if (slot > 0 && slot <= 48) {
      const auto& gem = VS::Items::inventory.gems[slot - 1];
      CheckDropIdentity(gem, part);
      CheckDropAffinities(gem, part);
      CHECK(static_cast<std::uint8_t>(gem.gem_effects) == part.gem_effects);
      CHECK(gem.index == slot);
      CHECK(gem.set_item_index == parent);
    }
  }
}

struct EquipmentStagingFixture : EnemyLootFixture {
  void CollectArmorPage(std::vector<int>& seen, bool claim) {
    for (int index = 0; index < VS::Menu::Loot::lootListCount; ++index) {
      const auto& row = rows[index];
      REQUIRE(row.category ==
              static_cast<int>(VS::Items::ItemCategory::kArmor));
      if (row.item_index < 0 || row.item_index >= 16) {
        FAIL("Staged armor row points outside the original capacity");
        return;
      }
      seen.push_back(source.armor[row.item_index].id);
      if (claim) {
        CHECK(row.state == 1);
        CHECK(VS::Items::Inventory::CopyLiveItem(0x14, &VS::Items::inventory,
                                                 row.item_index, &source) != 0);
      }
    }
  }

  void AdvanceArmorPage(bool first_claimed) {
    VS::Menu::Loot::lootListCount = 0;
    REQUIRE(VS::Menu::Loot::Live::AdvanceStagedInventory() == 1);
    REQUIRE(VS::Menu::Loot::lootListCount == 8);
    for (int index = 0; index < 8; ++index) {
      CHECK(rows[index].state == !first_claimed);
    }
    VS::Items::inventory = {};
    VS::Menu::Loot::Live::RefreshClaimableRows();
  }

  void CheckArmorBatchComplete(std::vector<int>& seen) {
    std::sort(seen.begin(), seen.end());
    REQUIRE(seen.size() == 24);
    for (int index = 0; index < 24; ++index) CHECK(seen[index] == 149 + index);
    VS::Menu::Loot::lootListCount = 0;
    CHECK(VS::Menu::Loot::Live::AdvanceStagedInventory() == 0);
    VS::Core::Random expected;
    for (int index = 0; index < 40; ++index) expected.Next(255);
    random.CheckSequence(expected);
    CheckCleared();
    CHECK(VS::Menu::Loot::Live::AdvanceStagedInventory() == 0);
  }

  void QueueArmorBatch() {
    for (int actor = 0; actor < 8; ++actor) {
      enemy.id = actor + 1;
      EnableEquipmentDrop(stats.limbs[0].armor);
      EnableEquipmentDrop(stats.limbs[1].armor);
      EnableEquipmentDrop(stats.accessory);
      stats.limbs[0].armor.armor.id =
          static_cast<std::uint16_t>(149 + actor * 3);
      stats.limbs[1].armor.armor.id =
          static_cast<std::uint16_t>(150 + actor * 3);
      stats.accessory.accessory.id =
          static_cast<std::uint16_t>(151 + actor * 3);
      VS::Items::QueueLiveEnemyLoot(&enemy);
    }
  }

  void Stage(int flags) {
    EnableEquipmentDrop(stats.weapon);
    EnableEquipmentDrop(stats.shield);
    EnableEquipmentDrop(stats.accessory);
    EnableEquipmentDrop(stats.limbs[0].armor);
    EnableEquipmentDrop(stats.limbs[1].armor);
    stats.limbs[1].armor.armor.id = 153;
    VS::Items::QueueLiveEnemyLoot(&enemy);
    REQUIRE(VS::Items::lootListHead != nullptr);
    auto& loot = VS::Items::lootListHead->loot;
    loot.weapon.header.difficulty_flags = static_cast<char>(flags);
    loot.shield.header.difficulty_flags = static_cast<char>(flags);
    loot.accessory.header.difficulty_flags = static_cast<char>(flags);
    for (auto& armor : loot.armor) {
      armor.header.difficulty_flags = static_cast<char>(flags);
    }
    VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
  }

  static void CheckRow(const VS::Menu::Loot::LootRow& row, int index,
                       bool fits) {
    const std::array categories{
        static_cast<int>(VS::Items::ItemCategory::kWeapon),
        static_cast<int>(VS::Items::ItemCategory::kShield),
        static_cast<int>(VS::Items::ItemCategory::kArmor),
        static_cast<int>(VS::Items::ItemCategory::kArmor),
        static_cast<int>(VS::Items::ItemCategory::kArmor)};
    CHECK(row.category == categories[index]);
    CHECK(row.item_index == (index < 2 ? 0 : index - 2));
    CHECK(row.slide_step == 0);
    CHECK(row.state == fits);
  }

  void Claim(int armor_count) {
    const auto source_before = source;
    int claimed_armor = 0;
    for (int index = 0; index < 5; ++index) {
      const auto& row = rows[index];
      const bool fits = index < 2 || armor_count + claimed_armor < 16;
      CheckRow(row, index, fits);
      const int flags =
          static_cast<int>(row.category) | VS::Items::Inventory::kCopyItemWrite;
      const int result = VS::Items::Inventory::CopyLiveItem(
          flags, &VS::Items::inventory, row.item_index, &source);
      CHECK(result ==
            (fits ? (index < 2 ? 1 : armor_count + claimed_armor + 1) : 0));
      if (index >= 2 && fits) ++claimed_armor;
      VS::Menu::Loot::Live::RefreshClaimableRows();
    }
    CHECK(std::memcmp(&source, &source_before, sizeof(source)) == 0);
    CheckClaimedArmor(armor_count, claimed_armor);
  }

  void CheckClaimedArmor(int armor_count, int claimed_armor) const {
    for (int index = 0; index < claimed_armor; ++index) {
      const auto& expected = index < 2 ? stats.limbs[index].armor.armor
                                       : stats.accessory.accessory;
      const auto& armor = VS::Items::inventory.armor[armor_count + index];
      CheckArmorDrop(armor, expected);
      CHECK(armor.index == armor_count + index + 1);
      CHECK(armor.limb == 0);
    }
  }

  void CheckWeaponParts() {
    const auto& inventory = VS::Items::inventory;
    const auto& weapon = inventory.weapons[0];
    CHECK(weapon.index == 1);
    CHECK(weapon.blade == 1);
    CHECK(weapon.grip == 1);
    CHECK(weapon.is_equipped == 0);
    CHECK(weapon.name == stats.weapon.name);
    CheckBladeDrop(inventory.blades[0], stats.weapon.blade);
    CheckGripDrop(inventory.grips[0], stats.weapon.grip);
    CHECK(inventory.blades[0].assembled_weapon_index == 1);
    CHECK(inventory.grips[0].assembled_weapon_index == 1);
    CheckClaimedGems(weapon, stats.weapon, 1);
  }

  void CheckShieldParts() {
    const auto& shield = VS::Items::inventory.shields[0];
    CHECK(shield.index == 1);
    CHECK(shield.is_equipped == 0);
    CheckArmorDrop(shield.base, stats.shield.base);
    CheckClaimedGems(shield, stats.shield, 0x81);
    CHECK(VS::Items::Inventory::CountLiveItems(
              static_cast<int>(VS::Items::ItemCategory::kGem),
              &VS::Items::inventory) == 4);
  }
};

struct LootPagingMenuFixture : EquipmentStagingFixture {
  VS::Input::Settings saved_settings = VS::Input::settings;
  decltype(VS::Input::buttonsPressed) saved_buttons = VS::Input::buttonsPressed;
  decltype(VS::Input::buttonRepeat) saved_repeat = VS::Input::buttonRepeat;
  VS::Menu::Framework::MenuRow* saved_menu_rows =
      VS::Menu::Framework::menuItems;
  decltype(VS::Menu::Framework::itemNames) saved_names =
      VS::Menu::Framework::itemNames;
  decltype(VS::Battle::stringContext) saved_string_context =
      VS::Battle::stringContext;
  VS::Menu::TextBoxState saved_box = VS::Menu::textBoxes[7];
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  int saved_sound_group = VS::Audio::nextSfxChannelGroup7E;
  decltype(VS::Menu::Loot::lootClaimed) saved_claimed =
      VS::Menu::Loot::lootClaimed;
  decltype(VS::Menu::Loot::controlsEnabled) saved_controls =
      VS::Menu::Loot::controlsEnabled;
  decltype(VS::Menu::Loot::showLoot) saved_show = VS::Menu::Loot::showLoot;
  decltype(VS::Menu::Loot::previewedLoot) saved_preview =
      VS::Menu::Loot::previewedLoot;
  decltype(VS::Menu::Loot::lootLeftEdge) saved_edge =
      VS::Menu::Loot::lootLeftEdge;
  std::array<VS::Menu::Framework::MenuRow, 40> menu_rows{};
  char item_names[512][24]{};
  std::vector<std::uint32_t> storage =
      std::vector<std::uint32_t>(0x20000 / 4 + 2);
  std::array<std::uint32_t, 32> ordering{};
  std::array<std::uint32_t, 0x180> sfx_offsets{};
  std::array<std::uint16_t, 0x180> sfx_selectors{};
  VS::Audio::Playback::ScopedLiveEffectTables sfx_tables{
      {.program_offsets = sfx_offsets,
       .bank_selectors = sfx_selectors,
       .program_data = {}}};

  LootPagingMenuFixture() {
    VS::Menu::Inventory::ResetInventoryTerminalSession();
    VS::Menu::Inventory::BindLiveLootInventoryText(
        VS::tests::inventory_terminal::LootText());
    VS::Menu::Loot::ResetLootTerminalModuleData();
    VS::Menu::Framework::menuItems = menu_rows.data();
    VS::Menu::Framework::itemNames = item_names;
    VS::Menu::textBoxes[7] = {};
    VS::Menu::InitLiveInformationTextBox(
        VS::Menu::InformationHeader::kInformation);
    sfx_offsets.fill(UINT32_MAX);
    VS::Input::settings.information = 0;
    VS::Menu::Loot::lootClaimed = 0;
    VS::Menu::Loot::lootLeftEdge = 0;
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering.data() + 2;
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Menu::Loot::UpdateLiveLootSelection(1);
  }

  ~LootPagingMenuFixture() {
    VS::Menu::Loot::ResetLootTerminalModuleData();
    VS::Menu::Inventory::ResetInventoryTerminalSession();
    VS::Input::settings = saved_settings;
    VS::Input::buttonsPressed = saved_buttons;
    VS::Input::buttonRepeat = saved_repeat;
    VS::Menu::Framework::menuItems = saved_menu_rows;
    VS::Menu::Framework::itemNames = saved_names;
    VS::Battle::stringContext = saved_string_context;
    VS::Menu::textBoxes[7] = saved_box;
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_ordering;
    VS::Render::frameBuf = saved_framebuffer;
    VS::Audio::nextSfxChannelGroup7E = saved_sound_group;
    VS::Menu::Loot::lootClaimed = saved_claimed;
    VS::Menu::Loot::controlsEnabled = saved_controls;
    VS::Menu::Loot::showLoot = saved_show;
    VS::Menu::Loot::previewedLoot = saved_preview;
    VS::Menu::Loot::lootLeftEdge = saved_edge;
  }

  static int Tick(int buttons = 0) {
    VS::Input::buttonsPressed = static_cast<std::uint16_t>(buttons);
    VS::Input::buttonRepeat = static_cast<std::uint16_t>(buttons);
    return VS::Menu::Loot::UpdateLiveLootSelection(0);
  }

  static void Step(int buttons = 0) { CHECK(Tick(buttons) == 0); }

  static void CheckClaims(int claimed, int bag_count) {
    CHECK(VS::Menu::Loot::lootClaimed == claimed);
    CHECK(VS::Items::Inventory::CountLiveItems(
              static_cast<int>(VS::Items::ItemCategory::kArmor), nullptr) ==
          bag_count);
  }

  static void CheckNextArmorPage() {
    CHECK(VS::Menu::Loot::lootListCount == 8);
    CHECK(VS::Menu::Loot::selectedLoot == 0);
    CHECK(VS::Menu::Loot::lootListOffset == 0);
    CHECK(VS::Menu::Loot::controlsEnabled == 0);
    CHECK(VS::Menu::Loot::showLoot == 1);
  }

  void OpenLeaveConfirmation() {
    Step(kPadRdown);
    Step();
    Step();
    SettleRows();
    Step();
  }

  void SettleRows() {
    using VS::Menu::Framework::RowState;
    for (auto& row : menu_rows) {
      if (row.state > RowState::kIdle) row.state = RowState::kIdle;
    }
  }
};

struct EquipmentLootFixture : LootInventoryFixture {
  bool shield;
  int attached;
  EquipmentCapacity occupied;

  EquipmentLootFixture(bool is_shield, int gem_count, EquipmentCapacity counts)
      : shield(is_shield), attached(gem_count), occupied(counts) {
    VS::Items::inventory = {};
    source = {};
    PrepareSource();
    FillTarget(VS::Items::inventory, shield, occupied);
  }

  void PrepareSource() {
    if (shield) {
      auto part = EquipmentDropPart(147);
      VS::Items::WriteLiveInventoryArmorStats(&source.shields[0].base, &part);
      source.shields[0].index = 1;
      source.shields[0].is_equipped = 1;
      source.shields[0].base.limb = 1;
    } else {
      auto blade = EquipmentDropPart(14);
      auto grip = EquipmentDropPart(96);
      VS::Items::WriteLiveInventoryBladeStats(&source.blades[0], &blade);
      VS::Items::WriteLiveInventoryGripStats(&source.grips[0], &grip);
      source.blades[0].index = 1;
      source.blades[0].assembled_weapon_index = 1;
      source.grips[0].index = 1;
      source.grips[0].assembled_weapon_index = 1;
      source.weapons[0].index = 1;
      source.weapons[0].is_equipped = 1;
      source.weapons[0].blade = 1;
      source.weapons[0].grip = 1;
      for (int index = 0; index < 23; ++index) {
        source.weapons[0].name[index] = static_cast<char>(10 + index);
      }
      source.weapons[0].name[23] = static_cast<char>(0xE7);
    }
    for (int index = 0; index < attached; ++index) {
      auto gem = EquipmentDropPart(static_cast<std::uint16_t>(300 + index));
      VS::Items::WriteLiveInventoryGemStats(&source.gems[index], &gem);
      source.gems[index].index = static_cast<std::int16_t>(index + 1);
      source.gems[index].set_item_index = shield ? 0x81 : 1;
      auto& slots = shield ? source.shields[0].gems : source.weapons[0].gems;
      slots[index] = static_cast<std::uint8_t>(index + 1);
    }
  }

  static void FillTarget(VS::Items::InventoryContents& target, bool shield,
                         EquipmentCapacity occupied) {
    for (int index = 0; index < occupied.parents; ++index) {
      if (shield) {
        target.shields[index].base.id = 147;
        target.shields[index].index = static_cast<char>(index + 1);
      } else {
        target.weapons[index].blade = static_cast<std::uint8_t>(index + 1);
        target.weapons[index].grip = static_cast<std::uint8_t>(index + 1);
        target.weapons[index].index = static_cast<char>(index + 1);
      }
    }
    for (int index = 0; index < occupied.blades; ++index) {
      target.blades[index].id = 14;
      target.blades[index].index = static_cast<char>(index + 1);
      if (!shield && index < occupied.parents) {
        target.blades[index].assembled_weapon_index =
            static_cast<char>(index + 1);
      }
    }
    for (int index = 0; index < occupied.grips; ++index) {
      target.grips[index].id = 96;
      target.grips[index].index = static_cast<std::uint16_t>(index + 1);
      if (!shield && index < occupied.parents) {
        target.grips[index].assembled_weapon_index =
            static_cast<std::uint16_t>(index + 1);
      }
    }
    for (int index = 0; index < occupied.gems; ++index) {
      target.gems[index].id = 300;
      target.gems[index].index = static_cast<std::int16_t>(index + 1);
    }
  }

  int CopyEquipment(bool write) {
    const int category =
        shield ? static_cast<int>(VS::Items::ItemCategory::kShield)
               : static_cast<int>(VS::Items::ItemCategory::kWeapon);
    const int flags =
        category | (write ? VS::Items::Inventory::kCopyItemWrite : 0);
    return VS::Items::Inventory::CopyLiveItem(flags, &VS::Items::inventory, 0,
                                              &source);
  }

  VS::Items::InventoryContents ExpectedInventory(
      const VS::Items::InventoryContents& before) const {
    auto expected = before;
    const int parent = occupied.parents + 1;
    if (shield) {
      expected.shields[occupied.parents] = source.shields[0];
      expected.shields[occupied.parents].index = static_cast<char>(parent);
      expected.shields[occupied.parents].is_equipped = 0;
    } else {
      auto& weapon = expected.weapons[occupied.parents];
      weapon = source.weapons[0];
      weapon.index = static_cast<char>(parent);
      weapon.is_equipped = 0;
      weapon.blade = static_cast<std::uint8_t>(occupied.blades + 1);
      weapon.grip = static_cast<std::uint8_t>(occupied.grips + 1);
      auto& blade = expected.blades[occupied.blades];
      blade = source.blades[0];
      blade.index = static_cast<char>(occupied.blades + 1);
      blade.assembled_weapon_index = static_cast<char>(parent);
      auto& grip = expected.grips[occupied.grips];
      grip = source.grips[0];
      grip.index = static_cast<std::uint16_t>(occupied.grips + 1);
      grip.assembled_weapon_index = static_cast<std::uint16_t>(parent);
    }
    for (int index = 0; index < attached; ++index) {
      const int slot = occupied.gems + index;
      expected.gems[slot] = source.gems[index];
      expected.gems[slot].index = static_cast<std::int16_t>(slot + 1);
      expected.gems[slot].set_item_index =
          static_cast<std::uint16_t>(parent | (shield ? 0x80 : 0));
      auto& slots = shield ? expected.shields[occupied.parents].gems
                           : expected.weapons[occupied.parents].gems;
      slots[index] = static_cast<std::uint8_t>(slot + 1);
    }
    return expected;
  }

  static void CheckInventory(const VS::Items::InventoryContents& actual,
                             const VS::Items::InventoryContents& expected) {
    CHECK(std::memcmp(&actual, &expected, sizeof(actual)) == 0);
  }

  void CheckTransfer() {
    const auto before = VS::Items::inventory;
    const auto source_before = source;
    const bool fits = occupied.parents < 8 && occupied.gems + attached <= 48 &&
                      (shield || (occupied.blades < 16 && occupied.grips < 16));
    const int result = fits ? occupied.parents + 1 : 0;
    CHECK(CopyEquipment(false) == result);
    CheckInventory(VS::Items::inventory, before);
    CheckInventory(source, source_before);
    CHECK(CopyEquipment(true) == result);
    CheckInventory(source, source_before);
    CheckInventory(VS::Items::inventory,
                   fits ? ExpectedInventory(before) : before);
  }
};

struct LootPresentationFixture : EnemyLootFixture {
  std::uint8_t saved_intro = VS::State::LiveScriptFlag(0xAB);
  std::uint32_t saved_control = VS::Field::controlMode;
  std::uint32_t saved_flow = VS::Battle::actionFlowMode;
  VS::Actors::ActorRecord* saved_player = VS::Actors::actorRecords[0];
  VS::Items::LootListNode* saved_menu_list = VS::Menu::Framework::lootLists;
  VS::Items::BattleLoot* saved_menu_entry = VS::Menu::Framework::lootEntry;
  VS::Actors::ActorRecord locked_player_pose{};

  LootPresentationFixture() {
    // A locked pose keeps this presentation-gate test independent of model I/O.
    locked_player_pose.animation_id = 0x63;
    VS::Actors::actorRecords[0] = &locked_player_pose;
    VS::Menu::Framework::lootLists = nullptr;
    VS::Menu::Framework::lootEntry = nullptr;
    VS::Field::controlMode = 1;
    VS::Battle::actionFlowMode = 11;
  }

  ~LootPresentationFixture() {
    VS::State::SetLiveScriptFlag(0xAB, saved_intro);
    VS::Field::controlMode = saved_control;
    VS::Battle::actionFlowMode = saved_flow;
    VS::Actors::actorRecords[0] = saved_player;
    VS::Menu::Framework::lootLists = saved_menu_list;
    VS::Menu::Framework::lootEntry = saved_menu_entry;
  }
};

struct AbilityUnlockFixture : AbilityFixture {
  VS::State::ArtsStatus saved_progress = VS::State::artsStatus;
  decltype(VS::Battle::battleAbilityPointRequirements) saved_points =
      VS::Battle::battleAbilityPointRequirements;
  decltype(kBreakArtThresholds)::value_type saved_art_points[10]{};

  AbilityUnlockFixture() {
    VS::State::artsStatus = {};
    std::fill_n(VS::State::artsStatus.arts_learned, 12, 4);
    std::copy(kAbilityThresholds.begin(), kAbilityThresholds.end(),
              VS::Battle::battleAbilityPointRequirements.begin());
    for (unsigned category = 0; category < kBreakArtThresholds.size();
         ++category) {
      saved_art_points[category] =
          VS::Battle::breakArtPointRequirements[category];
      std::copy(kBreakArtThresholds[category].begin(),
                kBreakArtThresholds[category].end(),
                VS::Battle::breakArtPointRequirements[category].begin());
    }
  }

  ~AbilityUnlockFixture() {
    VS::State::artsStatus = saved_progress;
    std::copy(saved_points.begin(), saved_points.end(),
              VS::Battle::battleAbilityPointRequirements.begin());
    for (unsigned category = 0; category < kBreakArtThresholds.size();
         ++category) {
      std::copy(saved_art_points[category].begin(),
                saved_art_points[category].end(),
                VS::Battle::breakArtPointRequirements[category].begin());
    }
  }
};

struct ChainCreditFixture : AbilityUnlockFixture {
  std::uint8_t saved_story = VS::State::LiveScriptFlag(0x82);
  std::uint32_t saved_flow = VS::Battle::actionFlowMode;
  std::uint32_t saved_control = VS::Field::controlMode;
  int saved_reward = VS::Battle::recoveryRewardMode;

  ChainCreditFixture() {
    VS::State::SetLiveScriptFlag(0x82, 10);
    action.ability_input_succeeded = 1;
    action.chain_ability_slot = 1;
    stats[1].current_hp = 0;
    records[0].animation_id = 0x63;
  }

  ~ChainCreditFixture() {
    VS::State::SetLiveScriptFlag(0x82, saved_story);
    VS::Battle::actionFlowMode = saved_flow;
    VS::Field::controlMode = saved_control;
    VS::Battle::recoveryRewardMode = saved_reward;
  }

  void CheckTerminated() const {
    CHECK(VS::Battle::actionFlowMode == 9);
    CHECK(action.queue_index == 0);
    CHECK(action.chain_step == 0);
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
  decltype(VS::Battle::actionEffectMediators) saved_effects =
      VS::Battle::actionEffectMediators;
  std::uint8_t saved_story = VS::State::LiveScriptFlag(0x82);
  VS::Battle::HitResult source{};
  VS::Battle::HitResult target{};
  unsigned counter;
  int action_id = 184;
  int apply = 1;

  BreakArtKillFixture(unsigned category, unsigned tier)
      : counter(category % 10) {
    actors[0].equipped_weapon_category = static_cast<std::uint8_t>(category);
    VS::State::SetLiveScriptFlag(0x82, 10);
    VS::State::artsStatus.arts_learned[counter] =
        static_cast<std::uint8_t>(tier);
    VS::State::artsStatus.kills.weapon_category_kills[counter] =
        static_cast<std::uint16_t>(kBreakArtThresholds[counter][tier] - 1);
    VS::Battle::InitializeLiveActionHit(&source);
    VS::Battle::InitializeLiveActionHit(&target);
    target.target.target_actor = 1;
    target.target.hp = stats[1].current_hp;
    VS::Battle::actionDefinitions[184].type = kActionTypeBreakArt;
    VS::Battle::actionDefinitions[160].type = kActionTypeItem;
    VS::Battle::actionEffectMediators[0] =
        [](ActionDefinition*, VS::Battle::HitResult*, VS::Battle::HitResult*,
           int, int) {};
  }

  ~BreakArtKillFixture() {
    std::copy(saved_effects.begin(), saved_effects.end(),
              VS::Battle::actionEffectMediators.begin());
    VS::State::SetLiveScriptFlag(0x82, saved_story);
  }

  void Reject(KillGate gate) {
    switch (gate) {
      case KillGate::kPreview:
        apply = 0;
        break;
      case KillGate::kPositionSource:
        source.targets_position = 1;
        break;
      case KillGate::kEnemySource:
        source.target.target_actor = 1;
        break;
      case KillGate::kPositionTarget:
        target.targets_position = 1;
        break;
      case KillGate::kDummy:
        stats[1].actor_id = 0x80;
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
        target.target.target_actor = 0;
        break;
      case KillGate::kAlreadyCounted:
        actors[1].kill_counted = 1;
        break;
      case KillGate::kStory:
        VS::State::SetLiveScriptFlag(0x82, 9);
        break;
      case KillGate::kFourArts:
        VS::State::artsStatus.arts_learned[counter] = 4;
        break;
      case KillGate::kPendingThreshold:
        ++VS::State::artsStatus.kills.weapon_category_kills[counter];
        break;
    }
  }

  void Resolve() {
    VS::Battle::ApplyLiveActionHitEffect(
        &VS::Battle::actionDefinitions[action_id], &source, &target, apply, 0);
  }
};

void CheckAbilityProgress(const VS::State::ArtsStatus& before) {
  CHECK(std::to_array(VS::State::artsStatus.arts_learned) ==
        std::to_array(before.arts_learned));
  CHECK(std::to_array(VS::State::artsStatus.kills.weapon_category_kills) ==
        std::to_array(before.kills.weapon_category_kills));
  CHECK(VS::State::artsStatus.kills.battle_abilities_unlocked ==
        before.kills.battle_abilities_unlocked);
  CHECK(VS::State::artsStatus.kills.total_kills == before.kills.total_kills);
}
}  // namespace

TEST_CASE(
    "Combat sandbox applies matching target HP and MP effects with clamping",
    "[combat][sandbox]") {
  REQUIRE(Tests_CombatTargetResults() == 1);
}

TEST_CASE("Every matching limb result contributes once to its actor",
          "[combat][sandbox][multi-limb]") {
  const int limb_count = GENERATE(0, 1, 2, 6);
  const unsigned queue_index = GENERATE(0U, 5U, 7U);
  const bool filtered_entries = GENERATE(false, true);
  CAPTURE(limb_count, queue_index, filtered_entries);
  AbilityFixture fixture;
  fixture.PrepareLimbResults(limb_count, queue_index, filtered_entries);
  auto& target = fixture.stats[1];
  VS::Battle::ApplyLiveActionTargetEffects(1, nullptr);

  const int total = limb_count * (limb_count + 1) / 2;
  CHECK(target.current_hp == 63 - total);
  CHECK(target.current_mp == 40 - total);
  for (std::size_t limb = 0; limb < std::size(target.limbs); ++limb) {
    const int damage = limb < static_cast<std::size_t>(limb_count)
                           ? static_cast<int>(limb) + 1
                           : 0;
    CHECK(target.limbs[limb].hp == 20 - damage);
  }
  CHECK(fixture.stats[0].current_hp == 600);
  CHECK(fixture.stats[0].current_mp == 120);
}

TEST_CASE("Indestructible gimmick hit applies without an actor record",
          "[combat][sandbox][gimmick]") {
  AbilityFixture fixture;
  auto& hit = fixture.Request().targets[0];
  hit.target.impact_kind = 4;
  fixture.actors[1].room_spawn_index = 0x80;
  auto& target = fixture.stats[1];
  target.actor_id = 0x80;
  target.cancels_hit_results = 1;
  target.current_hp = target.max_hp = 1;
  // Field_CreateGimmickInstance registers VS::Field::gimmickInstances only.
  VS::Actors::actorRecords[1] = nullptr;

  VS::Battle::ApplyLiveActionTargetEffects(1, nullptr);

  CHECK(target.current_hp == 1);
  CHECK(VS::Battle::floatingLabelCount == 0);
}

TEST_CASE("Blocked hit still plays the defense reaction on a real actor",
          "[combat][sandbox][gimmick]") {
  AbilityFixture fixture;
  fixture.Request().targets[0].target.impact_kind = 4;
  auto& instance = fixture.records[1].instance;
  instance.movement_mode = 1;
  instance.jump_countdown = 5;

  VS::Battle::ApplyLiveActionTargetEffects(1, nullptr);

  CHECK(static_cast<int>(instance.movement_mode) == 0);
  CHECK(static_cast<int>(instance.jump_countdown) == 0);
}

TEST_CASE("Stat item effects bind plugin-local renderers, not EFFPURGE",
          "[combat][items][effects][item-plugin]") {
  const auto count = VS::Effects::kEffectEntryPointIndices
      [std::size(VS::Effects::kEffectEntryPointIndices) - 1];
  for (unsigned slot = 0; slot < count; ++slot) {
    CAPTURE(slot);
    CHECK(VS::Effects::kEffectEntryPoints[slot] !=
          &VS::Effects::LiveInvokeAllPluginUpdates);
  }
  CHECK(VS::Effects::kEffectEntryPoints
            [VS::Effects::kEffectEntryPointIndices[198]] ==
        &VS::Effects::Templates::UpdateLivePlg198Ring);
  CHECK(VS::Effects::kEffectEntryPoints
            [VS::Effects::kEffectEntryPointIndices[199]] ==
        &VS::Effects::Templates::UpdateLivePlg198Ring);
}

TEST_CASE("Ability rewards use all original cumulative thresholds",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 22));
  const int delta = GENERATE(-1, 0, 1);
  auto& progress = VS::State::artsStatus;
  progress.kills.battle_abilities_unlocked =
      static_cast<std::uint16_t>(unlocked);
  progress.kills.total_kills =
      static_cast<std::uint16_t>(kAbilityThresholds[unlocked] + delta);
  const auto before = progress;
  CHECK(VS::Battle::LivePendingBattleAbilityUnlocks() == (delta >= 0 ? 1 : 0));
  CheckAbilityProgress(before);
}

TEST_CASE("Successful lethal chain timing retains ability credit",
          "[combat][abilities][acquisition][chain-credit]") {
  ChainCreditFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 22));
  auto& progress = VS::State::artsStatus;
  progress.kills.battle_abilities_unlocked =
      static_cast<std::uint16_t>(unlocked);
  progress.kills.total_kills = kAbilityThresholds[unlocked] - 1;
  auto expected = progress;
  ++expected.kills.total_kills;
  VS::Battle::LiveAdvanceBattleAbilitySequence();
  CheckAbilityProgress(expected);
  fixture.CheckTerminated();
}

TEST_CASE("Lethal chain credit preserves failure story and capacity gates",
          "[combat][abilities][acquisition][chain-credit]") {
  ChainCreditFixture fixture;
  const int gate = GENERATE(0, 1, 2, 3);
  if (gate == 0) fixture.action.ability_input_succeeded = 0;
  if (gate == 1) VS::State::SetLiveScriptFlag(0x82, 9);
  if (gate == 2) {
    VS::State::artsStatus.kills.total_kills = UINT16_MAX;
  }
  if (gate == 3) {
    VS::State::artsStatus.kills.battle_abilities_unlocked = 22;
  }
  const auto before = VS::State::artsStatus;
  VS::Battle::LiveAdvanceBattleAbilitySequence();
  CheckAbilityProgress(before);
  fixture.CheckTerminated();
}

TEST_CASE("Pending ability rewards do not discard later chain credit",
          "[combat][abilities][acquisition][chain-credit]") {
  ChainCreditFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 22));
  const int extra = GENERATE(0, 1, 500);
  auto& progress = VS::State::artsStatus;
  progress.kills.battle_abilities_unlocked =
      static_cast<std::uint16_t>(unlocked);
  progress.kills.total_kills =
      static_cast<std::uint16_t>(kAbilityThresholds[unlocked] + extra);
  auto expected = progress;
  ++expected.kills.total_kills;
  VS::Battle::LiveAdvanceBattleAbilitySequence();
  CheckAbilityProgress(expected);
  fixture.CheckTerminated();
}

TEST_CASE("Pending chain credit alone does not trigger ability learning",
          "[combat][abilities][acquisition][chain-credit]") {
  ChainCreditFixture fixture;
  VS::Battle::recoveryRewardMode = GENERATE(0, 1);
  VS::State::artsStatus.kills.total_kills = kAbilityThresholds[0];
  const auto before = VS::State::artsStatus;
  REQUIRE(VS::Battle::LivePendingBattleAbilityUnlocks() == 1);
  VS::Battle::LiveBeginPendingAbilityReward();
  CHECK(VS::Battle::actionFlowMode == 0);
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
  fixture.actors[0].equipped_weapon_category =
      static_cast<std::uint8_t>(category + category_offset);
  auto& progress = VS::State::artsStatus;
  progress.arts_learned[category] = static_cast<std::uint8_t>(tier);
  progress.kills.weapon_category_kills[category] =
      static_cast<std::uint16_t>(kBreakArtThresholds[category][tier] + delta);
  progress.kills.total_kills = 65535;
  progress.kills.battle_abilities_unlocked =
      static_cast<std::uint16_t>(unlocked);
  const auto before = progress;
  CHECK(VS::Battle::LivePendingBattleAbilityUnlocks() ==
        (delta == 0 ? 0x10000 : (unlocked < 22 ? 1 : 0)));
  CheckAbilityProgress(before);
}

TEST_CASE("Ability reward cap prevents further pending unlocks",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  auto& progress = VS::State::artsStatus;
  progress.kills.battle_abilities_unlocked =
      static_cast<std::uint16_t>(GENERATE(22, 23, 65535));
  progress.kills.total_kills = 65535;
  const auto before = progress;
  CHECK(VS::Battle::LivePendingBattleAbilityUnlocks() == 0);
  CheckAbilityProgress(before);
}

TEST_CASE("Ineligible ability reward menus leave progression unchanged",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 23));
  auto& progress = VS::State::artsStatus;
  progress.kills.battle_abilities_unlocked =
      static_cast<std::uint16_t>(unlocked);
  progress.kills.total_kills = static_cast<std::uint16_t>(
      unlocked == 22 ? 65535 : kAbilityThresholds[unlocked] - 1);
  const auto before = progress;
  CHECK(VS::Menu::BattleAbilities::LiveUpdateAbilityUnlock(1) == 1);
  CheckAbilityProgress(before);
}

TEST_CASE("Qualifying lethal actions count once at every Break Art threshold",
          "[combat][break-arts][acquisition]") {
  const unsigned category = GENERATE(Catch::Generators::range(1u, 11u));
  const unsigned tier = GENERATE(Catch::Generators::range(0u, 4u));
  const int action_id = GENERATE(1, 22, 184);
  BreakArtKillFixture fixture(category, tier);
  fixture.action_id = action_id;
  auto expected = VS::State::artsStatus;
  ++expected.kills.weapon_category_kills[fixture.counter];
  fixture.Resolve();
  CHECK(fixture.actors[1].kill_counted == 1);
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
  const auto before = VS::State::artsStatus;
  fixture.Resolve();
  CheckAbilityProgress(before);
  const bool counted =
      gate == KillGate::kAlreadyCounted || gate == KillGate::kStory ||
      gate == KillGate::kFourArts || gate == KillGate::kPendingThreshold;
  CHECK(fixture.actors[1].kill_counted == counted);
  CHECK(fixture.actors[0].kill_counted == 0);
}

TEST_CASE("All battle abilities charge original Risk with chain growth and cap",
          "[combat][abilities][cost]") {
  AbilityFixture fixture;
  // BATTLE.PRG _getActionCost, 146C.c:13075.
  const int step = GENERATE(0, 9, 10, 31);
  const int risk = GENERATE(0, 97, 100);
  for (const auto& [id, base_cost] : kAbilityCosts) {
    CAPTURE(id, step, risk);
    fixture.action.chain_step = step;
    fixture.stats[0].risk = risk;
    const int cost =
        base_cost + (id < 40 && step >= 10 ? (step * step) / 10 : 0);
    const auto expected = static_cast<std::uint32_t>(
        (1 << 24) + (kActionTypeAbility << 16) + cost);
    CHECK(VS::Battle::LiveActionCostAndAvailability(id, &fixture.stats[0], 0) ==
          expected);
    CHECK(fixture.stats[0].risk == risk);
    CHECK(VS::Battle::LiveActionCostAndAvailability(id, &fixture.stats[0], 1) ==
          expected);
    CHECK(fixture.stats[0].risk == std::min(100, risk + cost));
    CHECK(fixture.stats[0].current_hp == 600);
    CHECK(fixture.stats[0].current_mp == 120);
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
  auto& definition = VS::Battle::actionDefinitions[action_id];
  definition.type = kActionTypeBreakArt;
  definition.cost = cost;
  auto& actor = fixture.stats[0];
  actor.current_hp = static_cast<decltype(actor.current_hp)>(hp);
  actor.risk = 7;
  actor.weapon.current_pp = 35;
  const auto expected = static_cast<std::uint32_t>((affordable ? 1 << 24 : 0) +
                                                   (kActionTypeBreakArt << 16) +
                                                   cost + (affordable ? 0 : 1));
  CHECK(VS::Battle::LiveActionCostAndAvailability(action_id, &actor, 0) ==
        expected);
  CHECK(actor.current_hp == hp);
  CHECK(VS::Battle::LiveActionCostAndAvailability(action_id, &actor, 1) ==
        expected);
  CHECK(actor.current_hp == (affordable ? hp - cost : hp));
  CHECK(actor.current_mp == 120);
  CHECK(actor.risk == 7);
  CHECK(actor.weapon.current_pp == 35);
}

TEST_CASE(
    "Break Art menu eligibility preserves weapon status and resource gates",
    "[combat][break-arts][eligibility]") {
  AbilityFixture fixture;
  const int action_id = GENERATE(Catch::Generators::range(184, 224));
  const int gate = GENERATE(0, 1, 2, 3, 4, 5, 6);
  constexpr std::array<int, 4> kCosts{25, 40, 55, 75};
  const int cost = kCosts[(action_id - 184) % 4];
  auto& definition = VS::Battle::actionDefinitions[action_id];
  definition.type = kActionTypeBreakArt;
  definition.cost = cost;
  definition.unlocked = gate != 1;
  fixture.actors[0].weapon_drawn = gate != 2;
  auto& actor = fixture.stats[0];
  actor.statuses = gate == 3 ? 0x2000 : (gate == 4 ? 0x1000 : 0);
  actor.action_recovery_timer = gate == 5 ? 1 : 0;
  actor.current_hp =
      static_cast<decltype(actor.current_hp)>(gate == 6 ? cost : 999);
  actor.current_mp = 0;
  constexpr std::array<int, 7> kFlags{0, 4, 4, 4, 0, 1, 2};
  const auto before = actor;
  CHECK(VS::Battle::LiveActorActionRestrictionFlags(0, action_id) ==
        kFlags[gate]);
  CHECK(actor.current_hp == before.current_hp);
  CHECK(actor.current_mp == before.current_mp);
  CHECK(actor.statuses == before.statuses);
  CHECK(actor.action_recovery_timer == before.action_recovery_timer);
}

TEST_CASE("Ability eligibility requires mapped input",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(VS::Battle::LiveCanTriggerBattleAbility(0) == 1);
  REQUIRE(VS::Battle::LiveCanTriggerBattleAbility(1) == 1);

  SECTION("no mapped offensive buttons") {
    std::fill_n(VS::Input::settings.mapped_chain_abilities, 3, 0);
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
  SECTION("no mapped defensive buttons") {
    fixture.Prepare(true);
    REQUIRE(VS::Battle::LiveCanTriggerBattleAbility(0) == 1);
    std::fill_n(VS::Input::settings.mapped_defense_abilities, 3, 0);
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
}

TEST_CASE("Ability eligibility requires a fresh and distinct input",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(VS::Battle::LiveCanTriggerBattleAbility(0) == 1);
  REQUIRE(VS::Battle::LiveCanTriggerBattleAbility(1) == 1);
  SECTION("consecutive buttons must differ") {
    fixture.action.previous_chain_input = 1;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(1) == 0);
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(2) == 1);
  }
  SECTION("only one attempt per action") {
    fixture.action.ability_input_attempted = 1;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(2) == 0);
  }
}

TEST_CASE("Ability eligibility requires real non-dummy actor targets",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(VS::Battle::LiveCanTriggerBattleAbility(0) == 1);
  SECTION("source is a position") {
    fixture.Request().primary_target.targets_position = 1;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
  SECTION("target is a position") {
    fixture.Request().targets[0].targets_position = 1;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
  SECTION("no target") {
    fixture.Request().target_count = 0;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
  SECTION("training dummy") {
    fixture.actors[1].room_spawn_index = 0x80;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
}

TEST_CASE("Ability eligibility requires an appropriate combat relationship",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(VS::Battle::LiveCanTriggerBattleAbility(0) == 1);
  SECTION("self target") {
    fixture.Request().targets[0].target.target_actor = 0;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
  SECTION("enemy action does not target Ashley") {
    fixture.Prepare(true);
    fixture.Request().targets[0].target.target_actor = 1;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
  SECTION("Ashley cannot chain from magic") {
    fixture.Request().action_id = 99;
    CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  }
  SECTION("defenses cannot continue") {
    for (int id = 40; id <= 53; ++id) {
      CAPTURE(id);
      fixture.Prepare(true);
      fixture.Request().action_id = id;
      CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
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
    constexpr std::array kButtons{kPadRright, kPadRup, kPadRleft};
    VS::Input::buttonsPressed = kButtons[slot - 1];
    VS::State::SetLiveScriptFlag(1, static_cast<char>(difficulty));
    fixture.action.ability_timer = 100 + delta;
    constexpr int kStreak = 7;
    VS::State::scoreData.chain_streak = kStreak;
    VS::Battle::LiveUpdateBattleAbilityTiming();
    const bool success = delta > 0 && delta <= margin;
    const int result = delta <= -margin      ? 0
                       : delta <= 0          ? 1
                       : delta <= margin     ? 2
                       : delta <= 2 * margin ? 3
                                             : 4;
    REQUIRE(VS::Battle::floatingLabelCount == 1);
    CHECK(fixture.labels[0].value == result);
    CHECK(fixture.action.ability_input_attempted == 1);
    fixture.CheckInputResult(success ? (defense ? 2 : 1) : 0,
                             success ? slot : 0);
    CHECK(fixture.action.ability_timer == 102 + delta);
    // A successful defense extends the streak; a missed input ends it (#448).
    CHECK(VS::State::scoreData.chain_streak ==
          (success ? kStreak + (defense ? 1 : 0) : 0));
  }
}

TEST_CASE("Unarmed windows and repeated buttons cannot start an ability",
          "[combat][abilities][timing]") {
  AbilityFixture fixture;
  fixture.action.ability_timer = 102;
  SECTION("input before a timing window") {
    fixture.action.ability_window = 0;
    VS::Battle::LiveUpdateBattleAbilityTiming();
    CHECK(fixture.action.ability_input_attempted == 1);
    REQUIRE(VS::Battle::floatingLabelCount == 1);
    CHECK(fixture.labels[0].value == 1);
  }
  SECTION("repeat previous chain button") {
    fixture.action.previous_chain_input = 1;
    VS::Battle::LiveUpdateBattleAbilityTiming();
    CHECK(fixture.action.ability_input_attempted == 1);
    CHECK(VS::Battle::floatingLabelCount == 0);
  }
  CHECK(fixture.action.ability_input_succeeded == 0);
  CHECK(fixture.action.chain_ability_slot == 0);
}

TEST_CASE("Unmapped buttons and no input cannot start an ability",
          "[combat][abilities][timing]") {
  AbilityFixture fixture;
  fixture.action.ability_timer = 102;
  SECTION("unmapped button") {
    VS::Input::settings.mapped_chain_abilities[0] = 0;
    VS::Battle::LiveUpdateBattleAbilityTiming();
    CHECK(fixture.action.ability_input_attempted == 0);
    CHECK(VS::Battle::floatingLabelCount == 0);
  }
  SECTION("no button") {
    VS::Input::buttonsPressed = 0;
    VS::Battle::LiveUpdateBattleAbilityTiming();
    CHECK(fixture.action.ability_input_attempted == 0);
    CHECK(VS::Battle::floatingLabelCount == 0);
  }
  CHECK(fixture.action.ability_input_succeeded == 0);
  CHECK(fixture.action.chain_ability_slot == 0);
}

TEST_CASE("Simultaneous ability buttons keep the original input priority",
          "[combat][abilities][timing]") {
  const auto [buttons, slot] =
      GENERATE(std::make_pair(kPadRright | kPadRup, 2),
               std::make_pair(kPadRright | kPadRleft, 3),
               std::make_pair(kPadRup | kPadRleft, 3),
               std::make_pair(kPadRright | kPadRup | kPadRleft, 3));
  AbilityFixture fixture;
  fixture.action.ability_timer = 102;
  VS::Input::buttonsPressed = buttons;
  VS::Battle::LiveUpdateBattleAbilityTiming();
  fixture.CheckInputResult(1, slot);
}

TEST_CASE("Successful defensive timing saturates rather than wraps its score",
          "[combat][abilities][timing]") {
  const auto [before, after] =
      GENERATE(std::make_pair(0, 1), std::make_pair(65534, 65535),
               std::make_pair(65535, 65535));
  AbilityFixture fixture;
  fixture.Prepare(true);
  fixture.action.ability_timer = 102;
  VS::State::scoreData.chain_streak = before;
  VS::Battle::LiveUpdateBattleAbilityTiming();
  fixture.CheckInputResult(2, 1);
  CHECK(VS::State::scoreData.chain_streak == after);
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
      {22, {4, 7, VS::Battle::QueueLiveHitDamageAndEquipmentWear}, -72, 0},
      {23, {18, 3, VS::Battle::QueueLiveHpRecovery, true}, 32, 0},
      {24, {4, 3, VS::Battle::QueueLiveMpDrain}, 0, -32},
      {25, {18, 3, VS::Battle::QueueLiveMpRecovery, true}, 0, 32},
      {26, {5, 1, VS::Battle::QueueLiveHitDamageAndEquipmentWear}, -42, 0},
      {27, {11, 2, VS::Battle::QueueLiveMpDrain}, 0, -42},
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
  fixture.Queue(28, 0, {4, 4, VS::Battle::QueueLiveHitDamageAndEquipmentWear});
  fixture.Queue(28, 1, {18, 20, VS::Battle::QueueLiveWeaponDpRecovery, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 758, 200);
  CHECK(fixture.stats[0].weapon.current_dp == 304);
}

TEST_CASE("Crimson Pain applies both enemy and self HP damage",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(29, 0, {6, 10, VS::Battle::QueueLiveHitDamageAndEquipmentWear});
  fixture.Queue(29, 1,
                {6, 3, VS::Battle::QueueLiveHitDamageAndEquipmentWear, true});
  fixture.Commit();
  fixture.CheckResources(580, 100, 718, 200);
}

TEST_CASE("Instill damages the target and restores weapon PP",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(30, 0, {4, 1, VS::Battle::QueueLiveHitDamageAndEquipmentWear});
  fixture.Queue(30, 1, {4, 10, VS::Battle::QueueLiveWeaponPpRecovery, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 788, 200);
  CHECK(fixture.stats[0].weapon.current_pp == 173);
}

TEST_CASE("Phantom Pain converts weapon PP to damage and empties it",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(31, 0, {8, 0, VS::Battle::QueueLiveHitDamageAndEquipmentWear});
  fixture.Queue(31, 1, {1, 0, VS::Battle::QueueLiveWeaponPpDrain, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 728, 200);
  CHECK(fixture.stats[0].weapon.current_pp == 0);
}

TEST_CASE("Siphon Soul restores half the incoming spell cost as MP",
          "[combat][abilities][ability-effects]") {
  const bool spell = GENERATE(false, true);
  AbilityEffectFixture fixture;
  fixture.action.selection.action_id = spell ? 99 : 1;
  fixture.Queue(41, 0, {21, 5, VS::Battle::QueueLiveMpRecovery, true});
  fixture.Commit();
  fixture.CheckResources(600, spell ? 112 : 100, 800, 200);
}

TEST_CASE("Reflect abilities return damage only for the matching action type",
          "[combat][abilities][ability-effects]") {
  const int id = GENERATE(42, 43);
  const bool spell = GENERATE(false, true);
  const int incoming_effect = GENERATE(0, 1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.action_id = spell ? 99 : 1;
  fixture.action.selection.primary_target.hp_effect = incoming_effect;
  fixture.Queue(
      id, 0,
      {id == 42 ? 9 : 10, 4, VS::Battle::QueueLiveHitDamageAndEquipmentWear});
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
  fixture.action.selection.action_id = spell ? 99 : 1;
  fixture.action.selection.primary_target.hp_effect = incoming_effect;
  fixture.Queue(id, 0,
                {id == 44 ? 19 : 20, 2,
                 id == 44 ? VS::Battle::QueueLiveSpellTriggeredRecovery
                          : VS::Battle::QueueLiveNonSpellTriggeredRecovery,
                 true});
  fixture.Commit();
  const bool applies =
      (spell == (id == 44)) && (incoming_effect == 1 || incoming_effect == 3);
  fixture.CheckResources(applies ? 622 : 600, 100, 800, 200);
}

TEST_CASE("All seven affinity defenses recover only matching damage",
          "[combat][abilities][ability-effects]") {
  constexpr std::array<AbilityEffect, 7> kEffects{
      VS::Battle::QueueLivePhysicalAffinityRecovery,
      VS::Battle::QueueLiveAirAffinityRecovery,
      VS::Battle::QueueLiveFireAffinityRecovery,
      VS::Battle::QueueLiveEarthAffinityRecovery,
      VS::Battle::QueueLiveWaterAffinityRecovery,
      VS::Battle::QueueLiveLightAffinityRecovery,
      VS::Battle::QueueLiveDarkAffinityRecovery};
  const int affinity = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int incoming = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int incoming_effect = GENERATE(1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.affinity = static_cast<char>(incoming);
  fixture.action.selection.primary_target.hp_effect = incoming_effect;
  fixture.Queue(46 + affinity, 0, {22, 5, kEffects[affinity], true});
  fixture.Commit();
  const bool applies = affinity == incoming && incoming_effect != 2;
  fixture.CheckResources(applies ? 655 : 600, 100, 800, 200);
}

TEST_CASE("Phantom Shield converts shield PP to recovery only after damage",
          "[combat][abilities][ability-effects]") {
  const int incoming_effect = GENERATE(0, 1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.primary_target.hp_effect = incoming_effect;
  fixture.Queue(53, 0,
                {23, 0, VS::Battle::QueueLiveDamageTriggeredRecovery, true});
  fixture.Queue(53, 1, {1, 0, VS::Battle::QueueLiveShieldPpDrain, true});
  fixture.Commit();
  const bool applies = incoming_effect == 1 || incoming_effect == 3;
  fixture.CheckResources(applies ? 690 : 600, 100, 800, 200);
  CHECK(fixture.stats[0].shield.current_pp == (applies ? 0 : 90));
}

TEST_CASE("Offensive status abilities queue and apply their original ailments",
          "[combat][abilities][ability-effects]") {
  struct Example {
    int id;
    int mask;
    AbilityEffect apply;
  };
  constexpr auto kExamples = std::to_array<Example>({
      {32, 0x2000, VS::Battle::QueueLiveParalysis},
      {34, 0x8000, VS::Battle::QueueLiveNumbness},
      {35, 0x1000, VS::Battle::QueueLiveSilence},
      {37, 0x4000, VS::Battle::QueueLivePoison},
  });
  for (const auto& example : kExamples) {
    CAPTURE(example.id);
    AbilityEffectFixture fixture;
    fixture.Queue(example.id, 0, {0, 0, example.apply});
    CHECK(fixture.target.remove_statuses == example.mask);
    VS::Battle::InflictLiveActorStatuses(fixture.target.remove_statuses,
                                         &fixture.stats[1]);
    CHECK(fixture.stats[1].statuses == example.mask);
    CHECK(fixture.stats[0].statuses == 0);
    fixture.CheckResources(600, 100, 800, 200);
  }
}

TEST_CASE("Ward removes paralysis and numbness without curing poison",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  VS::Battle::InflictLiveActorStatuses(0xE000, &fixture.stats[0]);
  fixture.Queue(40, 0, {0, 0, VS::Battle::QueueLiveParalysisCure, true});
  fixture.Queue(40, 1, {0, 0, VS::Battle::QueueLiveNumbnessCure, true});
  CHECK(fixture.source.inflict_statuses == 0xA000);
  VS::Battle::RemoveLiveActorStatuses(fixture.source.inflict_statuses,
                                      &fixture.stats[0]);
  CHECK(fixture.stats[0].statuses == 0x4000);
  CHECK(fixture.stats[0].walk_speed == 8);
  CHECK(fixture.stats[0].run_speed == 12);
  fixture.CheckResources(600, 100, 800, 200);
}

TEST_CASE("Restorative abilities respect HP MP and equipment capacity",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  SECTION("Gain Life") {
    fixture.stats[0].current_hp = 995;
    fixture.Queue(23, 0, {18, 3, VS::Battle::QueueLiveHpRecovery, true});
    fixture.Commit();
    CHECK(fixture.stats[0].current_hp == 1000);
  }
  SECTION("Gain Magic") {
    fixture.stats[0].current_mp = 299;
    fixture.Queue(25, 0, {18, 3, VS::Battle::QueueLiveMpRecovery, true});
    fixture.Commit();
    CHECK(fixture.stats[0].current_mp == 300);
  }
  SECTION("Temper") {
    fixture.stats[0].weapon.current_dp = 995;
    fixture.Queue(28, 1, {18, 20, VS::Battle::QueueLiveWeaponDpRecovery, true});
    fixture.Commit();
    CHECK(fixture.stats[0].weapon.current_dp == 1000);
  }
  SECTION("Instill") {
    fixture.stats[0].weapon.current_pp = 250;
    fixture.Queue(30, 1, {4, 10, VS::Battle::QueueLiveWeaponPpRecovery, true});
    fixture.Commit();
    CHECK(fixture.stats[0].weapon.current_pp == 255);
  }
}

TEST_CASE("MP damage abilities cannot drain more MP than the target has",
          "[combat][abilities][ability-effects]") {
  const int id = GENERATE(24, 27);
  const int mp = GENERATE(0, 10, 200);
  AbilityEffectFixture fixture;
  fixture.stats[1].current_mp =
      static_cast<decltype(fixture.stats[1].current_mp)>(mp);
  fixture.Queue(
      id, 0,
      {id == 24 ? 4 : 11, id == 24 ? 3 : 2, VS::Battle::QueueLiveMpDrain});
  const int damage = std::min(mp, id == 24 ? 32 : 42);
  CHECK(fixture.target.target.mp == damage);
  CHECK(VS::Battle::lastActionStatValue == static_cast<std::uint32_t>(damage));
  fixture.Commit();
  fixture.CheckResources(600, 100, 800, mp - damage);
}

TEST_CASE("Two-hit Break Arts accumulate both original physical hit contracts",
          "[combat][break-arts][art-effects]") {
  for (const auto& art : kTwoHitArts) {
    CAPTURE(art.id);
    ArtResolutionFixture fixture;
    auto& definition = VS::Battle::actionDefinitions[art.id];
    definition.id = static_cast<decltype(definition.id)>(art.id);
    definition.type = kActionTypeBreakArt;
    definition.hits[0].damage_type = art.damage_type;
    definition.hits[1].damage_type = art.damage_type;
    definition.hits[0].affinity = art.first_affinity;
    definition.hits[1].affinity = art.second_affinity;
    for (int hit = 0; hit < 2; ++hit) {
      definition.hits[hit].effect_id = 58;
      definition.hits[hit].prerequisite_function_id = hit == 0 ? 2 : 1;
      definition.hits[hit].hit_function_id = hit == 0 ? 3 : 1;
    }
    const int first_affinity = std::max(0, art.first_affinity - 1);
    const int second_affinity = std::max(0, art.second_affinity - 1);
    const int first = 150 * (100 + 10 * art.factor) / 100 - 60 -
                      6 * (art.damage_type + first_affinity);
    const int second = 150 - 60 - 6 * (art.damage_type + second_affinity);
    CHECK(first > 0);
    CHECK(second > 0);
    fixture.Queue(
        art.id, 0,
        {25, art.factor, VS::Battle::QueueLiveHitDamageAndEquipmentWear});
    CHECK(fixture.target.target.hp == first);
    fixture.Queue(art.id, 1,
                  {25, 0, VS::Battle::QueueLiveHitDamageAndEquipmentWear});
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
    target_limb.armor.current_dp = 1234;
  }
  auto& armor_dp = fixture.stats[1].limbs[limb].armor.current_dp;
  auto& shield_dp = fixture.stats[1].shield.current_dp;
  armor_dp = shield_dp = static_cast<std::uint16_t>(dp);
  fixture.target.target.target_limb =
      static_cast<decltype(fixture.target.target.target_limb)>(limb);
  fixture.target.target.hp = 20;
  fixture.target.hp_effect = 1;
  fixture.Queue(id, 1, {0, 0, VS::Battle::QueueLiveDamageBasedArmorWear});
  CHECK(fixture.target.armor_dp_change == 40);
  CHECK(fixture.target.limbs[limb].effects.armor_dp_change == 40);
  fixture.Commit();
  CHECK(armor_dp == std::max(0, dp - 80));
  CHECK(shield_dp == std::max(0, dp - 40));
  for (int index = 0; index < 6; ++index) {
    if (index != limb) {
      CHECK(fixture.stats[1].limbs[index].armor.current_dp == 1234);
    }
  }
  fixture.CheckResources(600, 100, 780, 200);
}

TEST_CASE("Rejected loot transfers leave partial misc stacks unchanged",
          "[combat][loot][inventory-capacity]") {
  LootInventoryFixture fixture;
  CHECK(fixture.Copy(false) == 0);
  CHECK(static_cast<int>(VS::Items::inventory.misc[0].count) == 99);
  CHECK(fixture.Copy(true) == 0);
  CHECK(static_cast<int>(VS::Items::inventory.misc[0].count) == 99);
  CHECK(fixture.source.misc[0].count == 2);
  VS::Items::inventory.misc[63] = {};
  CHECK(fixture.Copy(true) == 64);
  CHECK(VS::Items::inventory.misc[0].count +
            VS::Items::inventory.misc[63].count ==
        101);
}

TEST_CASE("Misc loot capacity queries and writes preserve stack limits",
          "[combat][loot][inventory-capacity]") {
  const bool free_slot = GENERATE(false, true);
  const int first = GENERATE(1, 98, 99, 100);
  const int second = GENERATE(-1, 1, 99, 100);
  const int incoming = GENERATE(1, 2, 100);
  LootInventoryFixture fixture;
  auto& inventory = VS::Items::inventory;
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
  VS::Items::inventory = {};
  for (int slot = 0; slot < occupied; ++slot) {
    VS::Items::inventory.misc[slot].id = 324;
    VS::Items::inventory.misc[slot].count = 100;
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
  std::array<VS::Items::MiscItemStack, 2> source{};
  source[0].id = (present & 1) != 0 ? 323 : 0;
  source[0].count = 2;
  source[1].id = (present & 2) != 0 ? 324 : 0;
  source[1].count = 3;
  VS::Items::BattleLoot loot{};
  loot.misc[2].item_id = 325;
  loot.misc[2].count = 1;
  loot.misc[2].difficulty_flags = 2;
  CHECK(CopyLegacyGuaranteedDrops(loot, source) == (present != 0));
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
  VS::Items::MiscItemDrop source{};
  source.id = 325;
  source.drop_rate = static_cast<decltype(source.drop_rate)>(rate);
  VS::Items::BattleLoot loot{};
  const bool success = roll < rate;
  CHECK(RollLegacyItemDrop(loot, source) == success);
  CheckLootItem(loot.misc[2], 325, success ? 1 : 0, success ? 3 : 0);
  fixture.CheckOneForcedDraw();
}

TEST_CASE("Absent random enemy items consume no draw",
          "[combat][loot][drop-rules]") {
  LootRandomFixture fixture;
  VS::Core::Random expected;
  VS::Items::MiscItemDrop source{};
  source.drop_rate = static_cast<decltype(source.drop_rate)>(255);
  VS::Items::BattleLoot loot{};
  CHECK(!RollLegacyItemDrop(loot, source));
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
    fixture.stats.random_drop_item.id = 325;
    fixture.stats.random_drop_item.drop_rate =
        static_cast<decltype(fixture.stats.random_drop_item.drop_rate)>(
            expected.Next(255) + above_roll);
  }
  const bool success = candidate && above_roll != 0;
  VS::Items::QueueLiveEnemyLoot(&fixture.enemy);
  CHECK((VS::Items::lootListHead != nullptr) == (guaranteed != 0 || success));
  if (VS::Items::lootListHead != nullptr) {
    fixture.CheckQueued(guaranteed, candidate, success);
    VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
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
          "[combat][loot][drop-rules][action-outcomes][F00048]") {
  const int guard = GENERATE(0, 1, 2);
  const int guaranteed = GENERATE(0, 1);
  const bool retained = GENERATE(false, true);
  EnemyLootFixture fixture;
  fixture.SetGuaranteed(guaranteed);
  fixture.enemy.retain_after_defeat = retained ? 1 : 0;
  fixture.enemy.room_spawn_index = guard == 1 ? 0x80 : 1;
  fixture.enemy.defeated = guard == 2 ? 1 : 0;
  VS::Core::Random expected;
  if (guard == 0) {
    for (int index = 0; index < 3; ++index) expected.Next(255);
  }
  VS::Battle::ResolveLiveEnemyDefeat(0, 1, 0);
  auto* const first = VS::Items::lootListHead;
  VS::Battle::ResolveLiveEnemyDefeat(0, 1, 0);
  CHECK(VS::Items::lootListHead == first);
  CHECK((first != nullptr) == (guard == 0 && guaranteed != 0));
  CHECK(fixture.enemy.defeated == (guard != 1));
  fixture.random.CheckSequence(expected);
  fixture.CheckCleared();
}

void PrepareScoredEnemyDefeat(AbilityFixture& ability, EnemyLootFixture& loot) {
  VS::State::StoreLiveKillScore(
      {.weapon_kill_streak_blade_category = 3, .weapon_kill_streak = 4});
  ability.action.selection.action_id = 1;
  ability.stats[0].weapon.blade.category = 3;
  loot.enemy.kind = 4;
  loot.enemy.sub_type = 7;
  loot.stats.enemy_class = 2;
  loot.stats.max_hp = 100;
  loot.SetGuaranteed(1);
}

TEST_CASE(
    "F00048 production defeat combines score streak and loot exactly once",
    "[combat][loot][action-outcomes][F00048]") {
  AbilityFixture ability;
  EnemyLootFixture loot;
  PrepareScoredEnemyDefeat(ability, loot);
  VS::Battle::ResolveLiveEnemyDefeat(0, 1, -1);
  const auto score = VS::State::LiveKillScore();
  auto* const first = VS::Items::lootListHead;
  CHECK(score.enemy_kills.at(2) == 1);
  CHECK(score.enemy_kill_flags.at(0) == 128);
  CHECK(score.streak_score == 300);
  CHECK(score.weapon_kill_streak == 5);
  CHECK(first != nullptr);
  CHECK(loot.enemy.defeated == 1);
  VS::Battle::ResolveLiveEnemyDefeat(0, 1, 1);
  CHECK(VS::State::LiveKillScore() == score);
  CHECK(VS::Items::lootListHead == first);
}

TEST_CASE("Direct duplicate loot queues release rejected nodes after rolling",
          "[combat][loot][drop-rules]") {
  EnemyLootFixture fixture;
  fixture.SetGuaranteed(1);
  VS::Core::Random expected;
  VS::Items::QueueLiveEnemyLoot(&fixture.enemy);
  REQUIRE(VS::Items::lootListHead != nullptr);
  auto* const first = VS::Items::lootListHead;
  const auto free_units = VS::Core::Memory::GameHeap().FreeUnits();
  VS::Items::QueueLiveEnemyLoot(&fixture.enemy);
  CHECK(VS::Items::lootListHead == first);
  CHECK(first->next == nullptr);
  CHECK(VS::Core::Memory::GameHeap().FreeUnits() == free_units);
  auto second_enemy = fixture.enemy;
  second_enemy.id = 2;
  VS::Items::QueueLiveEnemyLoot(&second_enemy);
  REQUIRE(VS::Items::lootListHead != first);
  CHECK(VS::Items::lootListHead->actor_id == 2);
  CHECK(VS::Items::lootListHead->next == first);
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
  VS::State::stateFlags.difficulty = static_cast<char>(difficulty);
  VS::Items::BattleLoot loot{};
  loot.misc[0].item_id = has_id ? 323 : 0;
  loot.misc[0].count = has_count ? 2 : 0;
  loot.misc[0].difficulty_flags = static_cast<char>(flags);
  loot.misc[1].item_id = 324;
  loot.misc[1].count = 3;
  loot.misc[1].difficulty_flags = 3;
  loot.misc[2].item_id = 325;
  const bool eligible = has_id && has_count && (flags & (difficulty + 1)) != 0;
  VS::Menu::Loot::Live::StageDropRecord(&loot);
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
  VS::State::SetLiveScriptFlag(0xAB, static_cast<char>(intro));
  VS::Items::QueueLiveEnemyLoot(&fixture.enemy);
  auto* const queued = VS::Items::lootListHead;
  VS::Items::OpenLivePendingEnemyLoot();
  CHECK(VS::Field::controlMode == 12);
  CHECK(VS::Battle::actionFlowMode == 0);
  CHECK(VS::Menu::Framework::lootLists == nullptr);
  CHECK(VS::Menu::Framework::lootEntry == nullptr);
  CHECK(VS::Items::lootListHead == queued);
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
  CheckEquipmentRoll(
      roll, rate,
      RollLegacyDrop<VS::Items::LootedWeapon, VS::Items::WeaponLoadout,
                     &VS::Items::RollEnemyWeaponDrop>);
}

TEST_CASE("Shield drops preserve properties at every original rate boundary",
          "[combat][loot][equipment-drops]") {
  const int roll = GENERATE(Catch::Generators::range(0, 255));
  const int rate = GENERATE_COPY(0, roll, roll + 1, 255);
  CheckEquipmentRoll(
      roll, rate,
      RollLegacyDrop<VS::Items::LootedShield, VS::Items::ShieldLoadout,
                     &VS::Items::RollEnemyShieldDrop>);
}

TEST_CASE("Armor drops preserve properties at every original rate boundary",
          "[combat][loot][equipment-drops]") {
  const int roll = GENERATE(Catch::Generators::range(0, 255));
  const int rate = GENERATE_COPY(0, roll, roll + 1, 255);
  CheckEquipmentRoll(roll, rate, RollLegacyArmorDrop);
}

TEST_CASE("Accessory drops preserve properties at every original rate boundary",
          "[combat][loot][equipment-drops]") {
  const int roll = GENERATE(Catch::Generators::range(0, 255));
  const int rate = GENERATE_COPY(0, roll, roll + 1, 255);
  CheckEquipmentRoll(
      roll, rate,
      RollLegacyDrop<VS::Items::LootedArmor, VS::Items::AccessorySpawnLoadout,
                     &VS::Items::RollEnemyAccessoryDrop>);
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
      source.armor = EquipmentDropPart(static_cast<std::uint16_t>(149 + index));
      expected.Next(255);
    }
    if ((eligible & (1 << index)) != 0) {
      source.drop_rate = 255;
      if (source.armor.id != 0 && selected.size() < 2) {
        selected.push_back(index);
      }
    }
  }
  VS::Items::QueueLiveEnemyLoot(&fixture.enemy);
  CHECK((VS::Items::lootListHead != nullptr) == !selected.empty());
  if (VS::Items::lootListHead != nullptr) {
    const auto& loot = VS::Items::lootListHead->loot;
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
  VS::Items::inventory = {};
  fixture.source = {};
  auto part = EquipmentDropPart(static_cast<std::uint16_t>(id));
  VS::Items::WriteLiveInventoryArmorStats(&fixture.source.armor[0], &part);
  fixture.source.armor[0].index = 1;
  for (int index = 0; index < count; ++index) {
    VS::Items::inventory.armor[index].id = 150;
    VS::Items::inventory.armor[index].index =
        static_cast<std::uint8_t>(index + 1);
  }
  auto expected = VS::Items::inventory;
  const auto source_before = fixture.source;
  CHECK(VS::Items::Inventory::CopyLiveItem(
            static_cast<int>(VS::Items::ItemCategory::kArmor),
            &VS::Items::inventory, 0,
            &fixture.source) == (count < 16 ? count + 1 : 0));
  EquipmentLootFixture::CheckInventory(VS::Items::inventory, expected);
  CHECK(VS::Items::Inventory::CopyLiveItem(
            static_cast<int>(VS::Items::ItemCategory::kArmor) |
                VS::Items::Inventory::kCopyItemWrite,
            &VS::Items::inventory, 0,
            &fixture.source) == (count < 16 ? count + 1 : 0));
  if (count < 16) {
    expected.armor[count] = fixture.source.armor[0];
    expected.armor[count].index = static_cast<std::uint8_t>(count + 1);
    expected.armor[count].limb = 0;
  }
  EquipmentLootFixture::CheckInventory(VS::Items::inventory, expected);
  EquipmentLootFixture::CheckInventory(fixture.source, source_before);
}

TEST_CASE(
    "Equipment loot staging preserves rows and refreshes bag availability",
    "[combat][loot][equipment-drops]") {
  const int difficulty = GENERATE(0, 1);
  const int flags = GENERATE(0, 1, 2, 3);
  const int armor_count = GENERATE(0, 15, 16);
  EquipmentStagingFixture fixture;
  VS::State::stateFlags.difficulty = static_cast<char>(difficulty);
  for (int index = 0; index < armor_count; ++index) {
    VS::Items::inventory.armor[index].id = 150;
    VS::Items::inventory.armor[index].index =
        static_cast<std::uint8_t>(index + 1);
  }
  fixture.Stage(flags);
  const bool allowed = (flags & (difficulty + 1)) != 0;
  REQUIRE(VS::Menu::Loot::lootListCount == (allowed ? 5 : 0));
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
  VS::Items::BattleLoot loot{};
  loot.weapon.blade.id = 14;
  loot.weapon.grip.id = 96;
  loot.blade.blade.id = 14;
  loot.grip.grip.id = 96;
  loot.shield.shield.id = 147;
  loot.armor[0].armor.id = 149;
  loot.gem.gem.id = 300;
  loot.misc[0].item_id = 323;
  loot.misc[0].count = 1;
  const std::array<int (*)(VS::Items::BattleLoot&), 7> stage{
      [](VS::Items::BattleLoot& value) {
        return VS::Menu::Loot::Live::StageWeapon(&value.weapon);
      },
      [](VS::Items::BattleLoot& value) {
        return VS::Menu::Loot::Live::StageBlade(&value.blade.blade, 0);
      },
      [](VS::Items::BattleLoot& value) {
        return VS::Menu::Loot::Live::StageGrip(&value.grip.grip, 0);
      },
      [](VS::Items::BattleLoot& value) {
        return VS::Menu::Loot::Live::StageShield(&value.shield);
      },
      [](VS::Items::BattleLoot& value) {
        return VS::Menu::Loot::Live::StageArmor(&value.armor[0].armor);
      },
      [](VS::Items::BattleLoot& value) {
        return VS::Menu::Loot::Live::StageGem(&value.gem.gem, 0);
      },
      [](VS::Items::BattleLoot& value) {
        return VS::Menu::Loot::Live::StageMiscItem(&value.misc[0]);
      }};
  for (int index = 0; index < capacities[category]; ++index) {
    CAPTURE(category, index);
    CHECK(stage[category](loot) == index + 1);
    CHECK(VS::Items::Inventory::CountLiveItems(category, &fixture.source) ==
          index + 1);
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
  VS::Items::BattleLoot loot{};
  auto part = EquipmentDropPart(14);
  VS::Items::WriteLiveInventoryBladeStats(&loot.weapon.blade, &part);
  part.id = 96;
  VS::Items::WriteLiveInventoryGripStats(&loot.weapon.grip, &part);
  part.id = 147;
  VS::Items::WriteLiveInventoryArmorStats(&loot.shield.shield, &part);
  for (int index = 0; index < attached; ++index) {
    part.id = static_cast<std::uint16_t>(300 + index);
    VS::Items::WriteLiveInventoryGemStats(&loot.weapon.gems[index], &part);
    VS::Items::WriteLiveInventoryGemStats(&loot.shield.gems[index], &part);
  }
  const auto before = fixture.source;
  const auto input_before = loot;
  const bool fits = parents < 8 && gems + attached <= 48 &&
                    (shield || (blades < 16 && grips < 16));
  const int result = shield ? VS::Menu::Loot::Live::StageShield(&loot.shield)
                            : VS::Menu::Loot::Live::StageWeapon(&loot.weapon);
  CHECK(result == (fits ? parents + 1 : 0));
  CHECK(std::memcmp(&loot, &input_before, sizeof(loot)) == 0);
  if (!fits) EquipmentLootFixture::CheckInventory(fixture.source, before);
  CHECK(VS::Items::Inventory::CountLiveItems(
            static_cast<int>(VS::Items::ItemCategory::kGem), &fixture.source) ==
        gems + (fits ? attached : 0));
}

TEST_CASE(
    "Oversized enemy armor batches retain deferred loot without duplicates",
    "[combat][loot][staging-capacity]") {
  const bool claim_first = GENERATE(false, true);
  EquipmentStagingFixture fixture;
  fixture.QueueArmorBatch();
  VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
  REQUIRE(VS::Menu::Loot::lootListCount == 16);
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
  VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
  REQUIRE(VS::Menu::Loot::lootListCount == 16);
  const auto first = VS::Items::lootListHead->loot;
  VS::Menu::Loot::Live::ResetPaging();
  CHECK(VS::Menu::Loot::Live::AdvanceStagedInventory() == 0);
  CHECK(std::memcmp(&VS::Items::lootListHead->loot, &first, sizeof(first)) ==
        0);
  VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
  REQUIRE(VS::Menu::Loot::lootListCount == 16);
  fixture.CheckCleared();
  CHECK(VS::Menu::Loot::Live::AdvanceStagedInventory() == 0);

  fixture.stats = {};
  fixture.SetGuaranteed(1);
  VS::Items::QueueLiveEnemyLoot(&fixture.enemy);
  VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
  fixture.DeliverRows(1);
  CHECK(VS::Menu::Loot::Live::AdvanceStagedInventory() == 0);
  fixture.CheckCleared();
}

TEST_CASE("Single chest loot remains intact when staging is abandoned",
          "[combat][loot][staging-capacity]") {
  EnemyLootFixture fixture;
  VS::Items::BattleLoot chest{};
  chest.misc[0] = {323, 3, 2};
  chest.misc[1] = {467, 3, 1};
  VS::Menu::Framework::lootEntry = &chest;
  const auto before = chest;
  VS::Menu::Loot::Live::BuildStagedInventory(nullptr);
  REQUIRE(VS::Menu::Loot::lootListCount == 2);
  CHECK(VS::Menu::Loot::Live::AdvanceStagedInventory() == 0);
  VS::Menu::Loot::Live::ResetPaging();
  CHECK(std::memcmp(&chest, &before, sizeof(chest)) == 0);
  VS::Menu::Loot::Live::BuildStagedInventory(nullptr);
  fixture.DeliverRows(2);
  CHECK(std::memcmp(&chest, &before, sizeof(chest)) == 0);
}

TEST_CASE("Loot menu cancellation retains the page until positive confirmation",
          "[combat][loot][staging-capacity][loot-paging-ui]") {
  LootPagingMenuFixture fixture;
  fixture.QueueArmorBatch();
  VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
  const auto before = fixture.source;
  const auto queued = VS::Items::lootListHead->loot;
  fixture.OpenLeaveConfirmation();
  fixture.Step(kPadRdown);
  CHECK(VS::Menu::Loot::lootListCount == 16);
  EquipmentLootFixture::CheckInventory(fixture.source, before);
  CHECK(std::memcmp(&VS::Items::lootListHead->loot, &queued, sizeof(queued)) ==
        0);
  fixture.SettleRows();
  fixture.Step();
  fixture.OpenLeaveConfirmation();
  fixture.Step(kPadLup);
  fixture.Step(kPadRright);
  fixture.CheckNextArmorPage();
}

TEST_CASE("Loot menu take-all advances only after the current claims finish",
          "[combat][loot][staging-capacity][loot-paging-ui]") {
  LootPagingMenuFixture fixture;
  fixture.QueueArmorBatch();
  VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
  fixture.Step(kPadRleft);
  CHECK(VS::Menu::Loot::lootListCount == 16);
  for (int tick = 0; tick < 10 && VS::Menu::Loot::lootListCount != 8; ++tick) {
    fixture.Step();
  }
  REQUIRE(VS::Menu::Loot::lootListCount == 8);
  fixture.CheckClaims(16, 16);
  VS::Items::inventory = {};
  VS::Menu::Loot::Live::RefreshClaimableRows();
  fixture.Step(kPadRleft);
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
  VS::Items::lootListHead->loot.misc[0] = {467, 3, 1};
  VS::Menu::Loot::Live::BuildStagedInventory(VS::Items::lootListHead);
  REQUIRE(VS::Menu::Loot::lootListCount == 17);
  const auto before = fixture.source;
  const auto queued = VS::Items::lootListHead->loot;
  CHECK(fixture.Tick(kPadRdown) == 0);
  CHECK(fixture.Tick() == 0);
  CHECK(fixture.Tick(kPadRdown) == 0);
  CHECK(VS::Menu::Loot::lootListCount == 17);
  EquipmentLootFixture::CheckInventory(fixture.source, before);
  CHECK(std::memcmp(&VS::Items::lootListHead->loot, &queued, sizeof(queued)) ==
        0);
  CHECK(VS::Menu::Loot::lootClaimed == 0);
}
