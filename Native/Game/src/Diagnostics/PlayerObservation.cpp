#include "Diagnostics/PlayerObservation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <format>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Location.hpp"
#include "Actors/Movement.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionMenu.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionRecords.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Damage.hpp"
#include "Battle/EncounterHooks.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/TargetingLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Text/TextCode.hpp"
#include "Diagnostics/GameplayTrace.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Field/ChestInteractionsLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Interactions.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/LootLive.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/BattleAbilitiesLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/ItemListRows.hpp"
#include "Menu/Loot/LootScreen.hpp"
#include "Menu/Magic.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::SVECTOR;

using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;
using VS::Battle::kActionTypeSpell;
using VS::Battle::kWarlockSpellAvalanche;
using VS::Battle::kWarlockSpellExplosion;
using VS::Battle::kWarlockSpellFlameSphere;
using VS::Battle::kWarlockSpellGaeaStrike;
using VS::Battle::kWarlockSpellMeteor;
using VS::Battle::kWarlockSpellRadialSurge;
using VS::Battle::kWarlockSpellThunderburst;

namespace VS::Game {
namespace {
constexpr unsigned kFullyBlockedPassability = 5;
constexpr int kFootprintClearFlag = 4;
void InspectActorActions(const VS::Battle::ActorStats& stats,
                         PlayerActorObservation& snapshot) {
  for (const auto& limb : stats.limb_actions) {
    for (const auto& action : limb) {
      if (action.action_id != 0 &&
          action.action_id < std::size(VS::Battle::actionDefinitions) &&
          std::ranges::find(snapshot.action_ids, action.action_id) ==
              snapshot.action_ids.end()) {
        snapshot.action_ids.push_back(action.action_id);
        if (VS::Battle::actionDefinitions[action.action_id].type ==
            kActionTypeSpell) {
          snapshot.spell_action_ids.push_back(action.action_id);
        }
      }
    }
  }
}

PlayerActorObservation InspectPlayerActor(const EncounterActorState& base) {
  PlayerActorObservation snapshot;
  static_cast<EncounterActorState&>(snapshot) = base;
  const int slot = base.slot;
  const auto* actor = VS::Battle::actors[slot];
  if (actor == nullptr || actor->stats == nullptr) {
    throw std::runtime_error("diagnostic actor vanished within a frame");
  }
  const auto& stats = *actor->stats;
  if (VS::Actors::actorRecords[slot] != nullptr) {
    snapshot.model = VS::Actors::actorRecords[slot]->model_id;
  }
  snapshot.status_immunities =
      static_cast<std::uint32_t>(stats.status_immunities);
  InspectActorActions(stats, snapshot);
  snapshot.base_attack_type = stats.weapon.action_type;
  snapshot.shield_pp = stats.shield.current_pp;
  snapshot.weapon_pp = stats.weapon.current_pp;
  // Original 7CD68.c:437 reads this four-byte range shape without modifying it.
  constexpr int kRangeUnit = 32;
  VS::Battle::Ai::ActionRangeShape range{};
  range.range_y =
      std::bit_cast<char>(static_cast<std::uint8_t>(stats.current_range_y));
  range.range_z =
      std::bit_cast<char>(static_cast<std::uint8_t>(stats.current_range_z));
  range.shape = stats.current_attack_shape;
  range.angle = stats.current_attack_shape_angle;
  snapshot.weapon_range = {
      {static_cast<int>(stats.current_range) * kRangeUnit,
       static_cast<int>(stats.current_range_y) * kRangeUnit,
       static_cast<int>(stats.current_range_z) * kRangeUnit},
      static_cast<int>(stats.current_attack_shape),
      static_cast<int>(stats.current_attack_shape_angle),
      VS::Battle::LiveCalculateRangeShapeExtent(&range)};
  if (const auto* ai = VS::Battle::Ai::actorAiStates[slot]; ai != nullptr) {
    constexpr unsigned kHostileRelation = 3;
    const auto& relation = ai->relations[0];
    snapshot.relation_to_player = static_cast<int>(relation.classification);
    snapshot.relation_active = relation.active != 0;
    snapshot.player_visible = relation.visible != 0;
    snapshot.hostile_to_player = relation.classification == kHostileRelation;
  }
  return snapshot;
}

std::string InspectGameText(std::span<const std::byte> bytes) {
  constexpr std::string_view kLetters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::string result;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const auto value = std::to_integer<unsigned char>(bytes[index]);
    if (value == VS::Core::TextCode::kTerminator) break;
    if (value >= VS::Core::TextCode::kChunkSize) {
      if (++index >= bytes.size()) break;
    } else if (value < kLetters.size()) {
      result += kLetters[value];
    } else if (value == VS::Core::TextCode::kSpace) {
      result += ' ';
    } else if (value == VS::Core::TextCode::kHyphen) {
      result += '-';
    } else {
      result += std::format("[{:02X}]", value);
    }
  }
  return result;
}

// Ranks _warlockMagicMenu offers for a learned spell (MENU0.PRG 84.c:164,
// HandleInput and LevelledSpellInit): a levelled spell with its LV2 action
// unlocked opens the rank selector with 2..4 rows; anything else casts as is.
int WarlockSpellRankCount(int action) {
  constexpr std::array kLevelledSpells{
      kWarlockSpellExplosion,   kWarlockSpellThunderburst,
      kWarlockSpellFlameSphere, kWarlockSpellGaeaStrike,
      kWarlockSpellAvalanche,   kWarlockSpellRadialSurge,
      kWarlockSpellMeteor};
  if (std::ranges::find(kLevelledSpells, action) == kLevelledSpells.end() ||
      !VS::Battle::actionDefinitions[action + 1].unlocked) {
    return 1;
  }
  if (!VS::Battle::actionDefinitions[action + 2].unlocked) return 2;
  constexpr int kThirdRank = 3;
  constexpr int kFourthRank = 4;
  return VS::Battle::actionDefinitions[action + kThirdRank].unlocked
             ? kFourthRank
             : kThirdRank;
}

void InspectActionOptions(PlayerObservationState& state) {
  const auto* player = VS::Battle::actors[0];
  if (player == nullptr || player->stats == nullptr ||
      VS::Battle::actionState == nullptr) {
    return;
  }
  const auto append = [&](const auto& ids, std::size_t count,
                          std::string_view family) {
    if (count > std::size(ids)) {
      throw std::runtime_error("action inspection exceeds the native list");
    }
    int menu_order = 0;
    for (std::size_t index = 0; index < count; ++index) {
      const auto id = static_cast<unsigned char>(ids[index]);
      if (id == 0 || !VS::Battle::actionDefinitions[id].unlocked) continue;
      const int ranks = family == "warlock" ? WarlockSpellRankCount(id) : 1;
      for (int rank = 0; rank < ranks; ++rank) {
        const int action = id + rank;
        const auto& definition = VS::Battle::actionDefinitions[action];
        state.action_options.push_back(
            {action, InspectGameText(std::as_bytes(std::span(definition.name))),
             std::string(family), menu_order, static_cast<int>(definition.type),
             static_cast<int>(definition.cost),
             VS::Battle::LiveActorActionRestrictionFlags(0, action),
             std::bit_cast<std::uint16_t>(definition.ai_flags), rank, ranks});
      }
      ++menu_order;
    }
  };
  // Counts are the original menu loops, not the overbroad SDK array extents.
  constexpr std::size_t kWarlockRows = 18, kShamanRows = 6;
  constexpr std::size_t kSorcererRows = 18, kEnchanterRows = 8;
  constexpr std::size_t kBattleAbilityRows = 14;
  append(VS::Menu::Magic::warlockSpellIds, kWarlockRows, "warlock");
  append(VS::Menu::Magic::shamanSpellIds, kShamanRows, "shaman");
  append(VS::Menu::Magic::sorcererSpellIds, kSorcererRows, "sorcerer");
  append(VS::Menu::Magic::enchanterSpellIds, kEnchanterRows, "enchanter");
  append(VS::Battle::chainAbilityActionIds, kBattleAbilityRows, "chain");
  append(VS::Battle::defenseAbilityActionIds, kBattleAbilityRows, "defense");
}

void InspectItemOptions(PlayerObservationState& state) {
  const auto* player = VS::Battle::actors[0];
  if (player == nullptr || player->stats == nullptr) return;
  constexpr int kFirstItem = 323;
  constexpr unsigned kFirstRankedSpell = 106, kRankedSpellCount = 28;
  constexpr unsigned kRanks = 3;
  const auto& action_map = VS::Menu::Inventory::kMiscItemToActionMap;
  // MENU/Inventory.c item shortcut uses this map without granting a spell.
  // Ids past the map (upstream's bronzeKey guard admits 457) get no option,
  // like the native shortcut and item-list refusals (#467).
  for (const auto& item : state.items) {
    if (item.id < kFirstItem || item.count <= 0 ||
        static_cast<std::size_t>(item.id - kFirstItem) >= action_map.size()) {
      continue;
    }
    unsigned action =
        action_map[static_cast<std::size_t>(item.id - kFirstItem)];
    if (action - kFirstRankedSpell < kRankedSpellCount) {
      for (unsigned rank = 0; rank < kRanks; ++rank) {
        if (VS::Battle::actionDefinitions[action].unlocked) ++action;
      }
    }
    if (action >= std::size(VS::Battle::actionDefinitions)) {
      throw std::runtime_error(
          "observed item action exceeds native definitions");
    }
    const auto& definition = VS::Battle::actionDefinitions[action];
    const auto& hit = definition.hits[0];
    state.item_options.push_back(
        {item.id, static_cast<int>(action), item.count,
         InspectGameText(std::as_bytes(std::span(definition.name))),
         VS::Battle::LiveActorActionRestrictionFlags(0,
                                                     static_cast<int>(action)),
         static_cast<int>(hit.effect_id),
         static_cast<int>(hit.stat_calculator_id),
         static_cast<int>(hit.stat_factor)});
  }
}

void InspectItemMenuRow(EncounterMenuRowState& row) {
  if (row.index < 0 || static_cast<std::size_t>(row.index) >=
                           std::size(VS::Items::inventoryIndices.misc)) {
    throw std::runtime_error("item menu row exceeds inventory index storage");
  }
  const auto slot =
      static_cast<unsigned char>(VS::Items::inventoryIndices.misc[row.index]);
  if (slot == 0 || slot > std::size(VS::Items::inventory.misc)) {
    throw std::runtime_error(
        "item menu row has an invalid native inventory index");
  }
  const auto& item = VS::Items::inventory.misc[slot - 1];
  row.item_id = item.id;
  row.item_count = static_cast<unsigned char>(item.count);
}

EncounterMenuState InspectPlayerMenu(const EncounterState& state) {
  EncounterMenuState result;
  constexpr unsigned kMenuControl = 5, kMenuIdMask = 0x3F;
  constexpr unsigned kCombatControl = 2, kAbilityUnlockPhase = 14,
                     kLootPhase = 12;
  constexpr unsigned kEventControl = 3;
  // Field_BeginChestInteraction / Field_OpenChestContents (BATTLE.PRG
  // func_80074050 / func_800743E0) open the same loot menu at mode 9, flow 3.
  constexpr unsigned kChestControl = 9, kChestContentsPhase = 3;
  constexpr unsigned kSubmenuStateMask = 0xF;
  constexpr int kMainMenuEntries = 10;
  result.event_owned = state.control_mode == kEventControl &&
                       VS::Event::isMenuTransitionActive != 0;
  if ((state.control_mode != kMenuControl && !result.event_owned &&
       !(state.control_mode == kCombatControl &&
         (state.action_phase == kAbilityUnlockPhase ||
          state.action_phase == kLootPhase)) &&
       !(state.control_mode == kChestControl &&
         state.action_phase == kChestContentsPhase)) ||
      VS::Menu::Framework::menuItems == nullptr) {
    return result;
  }
  result.available = true;
  result.loading = VS::Menu::Framework::menuState.control.loading != 0;
  result.shortcut =
      static_cast<unsigned char>(VS::Menu::Framework::shortcutInvoked);
  if ((state.menu_state & kMenuIdMask) == kLootPhase) {
    result.loot_input_ready = VS::Menu::Loot::controlsEnabled != 0;
    result.loot_count = VS::Menu::Loot::lootListCount;
  }
  result.selected_menu = static_cast<int>(state.menu_state & kMenuIdMask);
  result.submenu_phase = static_cast<unsigned char>(
      VS::Menu::Framework::submenuStates[state.menu_state & kSubmenuStateMask]);
  result.selected_row = state.menu_highlighted_row;
  result.list_phase =
      static_cast<unsigned char>(VS::Menu::Framework::itemsListRow);
  result.assignment_phase =
      static_cast<int>(VS::Menu::BattleAbilities::abilityMenuState);
  result.assignment_type = VS::Menu::BattleAbilities::selectedAbilityType;
  result.assignment_selected = VS::Menu::BattleAbilities::highlightedAbility +
                               VS::Menu::BattleAbilities::firstAbility;
  // The rank selector's cursor and row count (menu memories 12 and 13) persist
  // after the selector closes; consumers pair them with a levelled option.
  constexpr int kRankCursorMemory = 12;
  constexpr int kRankCountMemory = 13;
  result.spell_level =
      VS::Menu::Framework::menuMemory.cursor_memories[kRankCursorMemory];
  result.spell_level_count =
      VS::Menu::Framework::menuMemory.cursor_memories[kRankCountMemory];
  if (result.selected_menu == 0) {
    for (int index = 0; index < kMainMenuEntries; ++index) {
      if ((VS::Input::settings.menu_flags & (1u << index)) == 0) continue;
      const auto& row = VS::Menu::Framework::menuItems[index];
      result.rows.push_back(
          {index, InspectGameText(std::as_bytes(std::span(row.text))),
           row.unselectable != 0, row.selected != 0});
      if (row.selected != 0) result.selected_row = index;
    }
  } else if (!result.loading && VS::Menu::Framework::itemsText != nullptr) {
    constexpr unsigned kMaximumObservedRows = 256;
    if (VS::Menu::Framework::subMenuRowCount > kMaximumObservedRows) {
      throw std::runtime_error("menu inspection exceeds supported row count");
    }
    const auto* text = reinterpret_cast<const VS::Menu::Framework::ItemText*>(
        VS::Menu::Framework::itemsText);
    for (unsigned index = 0; index < VS::Menu::Framework::subMenuRowCount;
         ++index) {
      result.rows.push_back(
          {static_cast<int>(index),
           InspectGameText(std::as_bytes(std::span(text[index].name))),
           (text[index].flags & VS::Menu::Framework::kRowDisabled) != 0,
           static_cast<int>(index) == result.selected_row});
      constexpr int kInventoryMenu = 4, kItemShortcut = 5, kInputReady = 17;
      if (result.selected_menu == kInventoryMenu &&
          result.shortcut == kItemShortcut &&
          result.list_phase == kInputReady) {
        InspectItemMenuRow(result.rows.back());
      }
    }
  }
  return result;
}

void InspectIncomingEffect(const ActionState& action,
                           const ActionRequestRecord& queued,
                           EncounterReactionTimingState& snapshot) {
  constexpr unsigned kHpDamage = 1, kCombinedDamage = 3;
  const auto incoming_id = action.selection.action_id;
  if (incoming_id < std::size(VS::Battle::actionDefinitions)) {
    snapshot.incoming_type = VS::Battle::actionDefinitions[incoming_id].type;
    snapshot.incoming_cost = VS::Battle::actionDefinitions[incoming_id].cost;
    snapshot.incoming_affinity =
        static_cast<unsigned char>(action.selection.effect_variant);
  }
  if (queued.target_count > std::size(queued.targets)) {
    throw std::runtime_error(
        "reaction inspection exceeds native target storage");
  }
  for (unsigned index = 0; index < queued.target_count; ++index) {
    const auto& hit = queued.targets[index];
    if (hit.targets_position != 0 || hit.target.target_actor != 0) continue;
    if (hit.hp_effect == kHpDamage || hit.hp_effect == kCombinedDamage) {
      snapshot.incoming_hp_damage +=
          std::max(0, static_cast<int>(hit.target.hp));
    }
    if (hit.mp_effect == kHpDamage || hit.mp_effect == kCombinedDamage) {
      snapshot.incoming_mp_damage +=
          std::max(0, static_cast<int>(hit.target.mp));
    }
    snapshot.incoming_statuses |=
        static_cast<std::uint32_t>(hit.remove_statuses);
  }
}

void InspectReactionEligibility(EncounterReactionTimingState& snapshot) {
  const auto present = [](int slot) {
    return slot >= 0 && slot < EncounterLimits::kActorSlots &&
           VS::Battle::actors[slot] != nullptr &&
           VS::Battle::actors[slot]->stats != nullptr;
  };
  if (!present(snapshot.source_actor) || !present(snapshot.target_actor) ||
      VS::Battle::characterState == nullptr ||
      VS::Battle::characterState->stats == nullptr) {
    snapshot.eligibility_status = "actor_state_unavailable";
    return;
  }
  snapshot.eligibility_status = "evaluated";
  const auto& mapped = snapshot.source_actor == 0 ? snapshot.chain_actions
                                                  : snapshot.defense_actions;
  for (std::size_t slot = 0; slot < mapped.size(); ++slot) {
    if (mapped[slot] == 0) continue;
    if (static_cast<std::size_t>(mapped[slot]) >=
        std::size(VS::Battle::actionDefinitions)) {
      snapshot.eligibility_status = "invalid_equipped_action";
      snapshot.eligible.fill(false);
      return;
    }
    snapshot.eligible[slot] = VS::Battle::LiveCanTriggerBattleAbility(
                                  static_cast<int>(slot) + 1) != 0;
  }
}

EncounterReactionTimingState InspectReactionTiming(
    const EncounterState& state) {
  EncounterReactionTimingState snapshot;
  snapshot.tick_speed = VS::State::tickSpeed;
  snapshot.story_timing = VS::State::LiveScriptFlag(1) != 0;
  for (std::size_t slot = 0; slot < snapshot.chain_actions.size(); ++slot) {
    snapshot.chain_actions[slot] = static_cast<unsigned char>(
        VS::Input::settings.mapped_chain_abilities[slot]);
    snapshot.defense_actions[slot] = static_cast<unsigned char>(
        VS::Input::settings.mapped_defense_abilities[slot]);
  }
  constexpr unsigned kAbilityTimingPhase = 7, kQueueIndexMask = 3;
  if (VS::Battle::actionState == nullptr) return snapshot;
  const auto& action = *VS::Battle::actionState;
  snapshot.active =
      state.control_mode == 2 && state.action_phase == kAbilityTimingPhase;
  snapshot.queue_index = action.queue_index;
  snapshot.chain_step = action.chain_step;
  snapshot.timer = action.ability_timer;
  snapshot.window = action.ability_window;
  snapshot.previous_slot =
      static_cast<unsigned char>(action.previous_chain_input);
  snapshot.previous_buttons =
      static_cast<unsigned char>(action.previous_chain_buttons);
  snapshot.attempted = action.ability_input_attempted != 0;
  snapshot.succeeded =
      static_cast<unsigned char>(action.ability_input_succeeded);
  const auto& queued = action.queue[action.queue_index & kQueueIndexMask];
  snapshot.action = queued.action_id;
  InspectIncomingEffect(action, queued, snapshot);
  if (queued.primary_target.targets_position != 0 || queued.target_count == 0 ||
      queued.targets[0].targets_position != 0) {
    if (snapshot.active) snapshot.eligibility_status = "not_actor_targeted";
    return snapshot;
  }
  snapshot.source_actor = queued.primary_target.target.target_actor;
  snapshot.target_actor = queued.targets[0].target.target_actor;
  if (!snapshot.active) return snapshot;
  InspectReactionEligibility(snapshot);
  return snapshot;
}

void InspectRoomDoors(PlayerObservationState& state) {
  if (VS::Field::roomData.doors == nullptr) return;
  if (VS::Field::roomData.header.doors_length %
          sizeof(VS::Field::MpdDoorRecord) !=
      0) {
    throw std::runtime_error(
        "observed room door storage has an incomplete record");
  }
  const auto count = VS::Field::roomData.header.doors_length /
                     sizeof(VS::Field::MpdDoorRecord);
  for (unsigned index = 0; index < count; ++index) {
    const auto& door = VS::Field::roomData.doors[index];
    state.doors.push_back({static_cast<int>(index),
                           door.destination_zone_id,
                           door.destination_room_id,
                           static_cast<int>(door.tile_location.tile_x),
                           static_cast<int>(door.tile_location.tile_z),
                           static_cast<int>(door.tile_location.floor_layer),
                           {static_cast<int>(door.entry_tile_x),
                            static_cast<int>(door.entry_tile_z),
                            static_cast<int>(door.entry_elevation), 0},
                           static_cast<int>(door.required_flag_id),
                           static_cast<int>(door.required_flag_value),
                           door.door_id,
                           door.has_entry_step != 0});
    state.door_flag_values.push_back(
        static_cast<unsigned char>(VS::State::LiveScriptFlag(
            static_cast<std::int16_t>(door.required_flag_id))));
  }
}

// Mirrors VS::Items::RoomTreasureChests (func_8008E370) and the opened-flag
// test in Field_FindFacingUnlootedChest (func_80074950): openedFlag + 0x3C0.
void InspectRoomChests(PlayerObservationState& state) {
  int count = 0;
  const auto* chests = VS::Items::LiveRoomTreasureChests(&count);
  if (chests == nullptr) return;
  if (VS::Field::roomData.header.containers_length %
          sizeof(VS::Items::TreasureChest) !=
      0) {
    throw std::runtime_error(
        "observed room container storage has an incomplete record");
  }
  constexpr int kChestOpenedFlagBase = 0x3C0;
  for (int index = 0; index < count; ++index) {
    const auto& chest = chests[index];
    EncounterChestState snapshot;
    snapshot.index = index;
    snapshot.tile_x = chest.tile_x;
    snapshot.tile_z = chest.tile_z;
    snapshot.layer = chest.layer;
    snapshot.facing = chest.facing;
    snapshot.lock_level = chest.lock_level;
    snapshot.opened_flag = chest.opened_flag + kChestOpenedFlagBase;
    snapshot.opened = VS::State::LiveScriptFlag(
                          static_cast<std::int16_t>(snapshot.opened_flag)) != 0;
    for (std::size_t slot = 0; slot < std::size(chest.loot.misc); ++slot) {
      const auto& item = chest.loot.misc[slot];
      if (item.item_id != 0 && item.count != 0) {
        snapshot.misc.push_back(
            {static_cast<int>(slot), item.item_id, item.count});
      }
    }
    state.chests.push_back(std::move(snapshot));
  }
}

int NavigationSupportHeight(int x, int z) {
  const int packed = VS::Field::LiveFloorHeightAt(x, z);
  if (packed < 0) return packed;
  constexpr int kTileSize = 128, kInactiveMode = 0xFE;
  constexpr int kNoSupport = 3000, kHeightMask = 0x7FFF;
  int support = kNoSupport;
  // VS::Field::LiveGimmickSupportHeight, original 40564.c:317. Do not call that
  // gameplay query here: it writes VS::Actors::supportingGimmickIndex.
  for (const auto* object : VS::Field::gimmickInstances) {
    if (object != nullptr && !object->is_skipped && !object->motion_state &&
        object->mode != kInactiveMode && object->tile_x == x / kTileSize &&
        object->tile_z == z / kTileSize) {
      support = std::min(support, static_cast<int>(object->position_y));
    }
  }
  support -= kTileSize;
  return support < 0 ? support & kHeightMask : packed;
}

bool NavigationPointClear(int x, int z, int floor_height, int body_height) {
  const auto* map = VS::Field::roomData.collision_map;
  constexpr int kTileSize = 128, kHeightSign = 0x4000, kHeightMask = 0x7FFF;
  if (x < 0 || z < 0 || x >= map->width * kTileSize ||
      z >= map->height * kTileSize) {
    return false;
  }
  const auto* cell =
      VS::Field::LiveCollisionCellAt(x / kTileSize, z / kTileSize, 0);
  if (cell == nullptr || cell->passability > kFullyBlockedPassability ||
      cell->is_pit) {
    return false;
  }
  SVECTOR point{static_cast<std::int16_t>(x), 0, static_cast<std::int16_t>(z),
                0};
  if (VS::Field::FindLiveCollisionCellAtPosition(&point, 0) == nullptr) {
    return false;
  }
  const int floor = NavigationSupportHeight(x, z);
  const int ceiling = VS::Field::LiveCeilingHeightAt(x, z);
  const auto height = [](int packed) {
    const int value = packed & kHeightMask;
    return value & kHeightSign ? value - (kHeightMask + 1) : value;
  };
  constexpr int kFootprintHeightDelta = 64;
  return floor >= 0 &&
         std::abs(height(floor) - floor_height) <= kFootprintHeightDelta &&
         (ceiling < 0 || height(floor) - height(ceiling) >= body_height);
}

bool NavigationFootprintClear(int x, int z, int radius, int body_height) {
  const int packed = NavigationSupportHeight(x, z);
  constexpr int kHeightMask = 0x7FFF, kHeightSign = 0x4000;
  const int value = packed & kHeightMask;
  const int height = value & kHeightSign ? value - (kHeightMask + 1) : value;
  const int diagonal = radius * 7 / 10;
  for (const auto& offset :
       std::array<std::array<int, 2>, 9>{{{0, 0},
                                          {radius, 0},
                                          {-radius, 0},
                                          {0, radius},
                                          {0, -radius},
                                          {diagonal, diagonal},
                                          {-diagonal, diagonal},
                                          {diagonal, -diagonal},
                                          {-diagonal, -diagonal}}}) {
    if (!NavigationPointClear(x + offset[0], z + offset[1], height,
                              body_height)) {
      return false;
    }
  }
  return true;
}

EncounterNavigationState InspectNavigation() {
  EncounterNavigationState result;
  const auto* map = VS::Field::roomData.collision_map;
  const auto* player = VS::Actors::actorRecords[0];
  if (map == nullptr || player == nullptr ||
      VS::Field::roomData.tile_cells == nullptr) {
    return result;
  }
  constexpr int kPatchSide = 13, kHalfPatch = kPatchSide / 2;
  constexpr int kTileSize = 128;
  const int cells = map->width * map->height;
  if (map->width <= 0 || map->height <= 0 ||
      map->width > EncounterLimits::kTiles ||
      map->height > EncounterLimits::kTiles ||
      VS::Field::roomData.header.collision_map_length < sizeof(*map) ||
      map->cell_count != cells ||
      VS::Field::roomData.header.tile_cells_length <
          cells * sizeof(VS::Field::MpdTileCell)) {
    throw std::runtime_error("invalid runtime navigation storage");
  }
  VS::Actors::ActorLocationQuery placement{};
  VS::Actors::LiveActorPlacement(0, &placement);
  constexpr std::size_t kFacingTileBytes = 4;
  std::array<std::uint8_t, kFacingTileBytes> facing{};
  VS::Field::TileInFrontOfActorEntry(&placement, facing.data());
  result.facing_tile = {facing[0], facing[2], facing[1]};
  result.collision_radius = player->collision_radius;
  result.collision_height = player->collision_height;
  result.movement_mode = player->instance.movement_mode;
  result.locomotion_state = player->instance.locomotion_state;
  result.route_phase = player->route_phase;
  result.jumping = player->instance.is_jumping != 0;
  result.width = std::min(kPatchSide, map->width * 2);
  result.depth = std::min(kPatchSide, map->height * 2);
  result.origin_x =
      std::clamp(placement.location.position.vx / result.spacing - kHalfPatch,
                 0, map->width * 2 - result.width) *
      result.spacing;
  result.origin_z =
      std::clamp(placement.location.position.vz / result.spacing - kHalfPatch,
                 0, map->height * 2 - result.depth) *
      result.spacing;
  // Original 146C.c:17083/17097 and 40564.c:139; queries do not move actors.
  for (int z = 0; z < result.depth; ++z) {
    for (int x = 0; x < result.width; ++x) {
      const int world_x = result.origin_x + x * result.spacing;
      const int world_z = result.origin_z + z * result.spacing;
      const auto* cell = VS::Field::LiveCollisionCellAt(world_x / kTileSize,
                                                        world_z / kTileSize, 0);
      SVECTOR point{static_cast<std::int16_t>(world_x), 0,
                    static_cast<std::int16_t>(world_z), 0};
      const bool passable =
          cell != nullptr && cell->passability <= kFullyBlockedPassability &&
          VS::Field::FindLiveCollisionCellAtPosition(&point, 0) != nullptr;
      const bool pit = cell != nullptr && cell->is_pit;
      result.samples.push_back(
          {NavigationSupportHeight(world_x, world_z),
           VS::Field::LiveCeilingHeightAt(world_x, world_z),
           (passable ? 1 : 0) | (pit ? 2 : 0) |
               (NavigationFootprintClear(world_x, world_z,
                                         result.collision_radius,
                                         result.collision_height)
                    ? kFootprintClearFlag
                    : 0)});
    }
  }
  result.available = true;
  return result;
}

nlohmann::json EncodeReaction(const EncounterReactionTimingState& timing) {
  return {{"active", timing.active},
          {"eligibility_status", timing.eligibility_status},
          {"source_actor", timing.source_actor},
          {"target_actor", timing.target_actor},
          {"action", timing.action},
          {"queue_index", timing.queue_index},
          {"chain_step", timing.chain_step},
          {"timer", timing.timer},
          {"window", timing.window},
          {"tick_speed", timing.tick_speed},
          {"story_timing", timing.story_timing},
          {"previous_slot", timing.previous_slot},
          {"previous_buttons", timing.previous_buttons},
          {"attempted", timing.attempted},
          {"succeeded", timing.succeeded},
          {"chain_actions", timing.chain_actions},
          {"defense_actions", timing.defense_actions},
          {"eligible", timing.eligible},
          {"incoming_type", timing.incoming_type},
          {"incoming_affinity", timing.incoming_affinity},
          {"incoming_cost", timing.incoming_cost},
          {"incoming_hp_damage", timing.incoming_hp_damage},
          {"incoming_mp_damage", timing.incoming_mp_damage},
          {"incoming_statuses", timing.incoming_statuses}};
}

nlohmann::json EncodePlayerMenu(const EncounterMenuState& menu) {
  auto rows = nlohmann::json::array();
  for (const auto& row : menu.rows) {
    rows.push_back({{"index", row.index},
                    {"text", row.text},
                    {"disabled", row.disabled},
                    {"selected", row.selected},
                    {"item_id", row.item_id},
                    {"item_count", row.item_count}});
  }
  return {{"available", menu.available},
          {"event_owned", menu.event_owned},
          {"loading", menu.loading},
          {"selected_menu", menu.selected_menu},
          {"submenu_phase", menu.submenu_phase},
          {"selected_row", menu.selected_row},
          {"list_phase", menu.list_phase},
          {"assignment_phase", menu.assignment_phase},
          {"assignment_type", menu.assignment_type},
          {"assignment_selected", menu.assignment_selected},
          {"spell_level", menu.spell_level},
          {"spell_level_count", menu.spell_level_count},
          {"shortcut", menu.shortcut},
          {"loot_input_ready", menu.loot_input_ready},
          {"loot_count", menu.loot_count},
          {"rows", rows}};
}

nlohmann::json EncodePlayerDoors(const PlayerObservationState& state) {
  auto doors = nlohmann::json::array();
  if (state.doors.size() != state.door_flag_values.size()) {
    throw std::runtime_error("observed door flags do not match their records");
  }
  for (std::size_t index = 0; index < state.doors.size(); ++index) {
    const auto& door = state.doors[index];
    doors.push_back({{"index", door.index},
                     {"destination_zone", door.destination_zone},
                     {"destination_room", door.destination_room},
                     {"tile_x", door.tile_x},
                     {"tile_z", door.tile_z},
                     {"layer", door.layer},
                     {"required_flag", door.required_flag},
                     {"required_value", door.required_value},
                     {"current_flag_value", state.door_flag_values[index]},
                     {"door_id", door.door_id}});
  }
  return doors;
}

// Whole-room tile grid for crate planning; the navigation patch only spans
// ~6 tiles. Floors exclude gimmicks (exported separately). Static per room, so
// it is emitted on room change and periodically to bound frame size.
nlohmann::json EncodeRoomTiles() {
  static const void* last_map = nullptr;
  static unsigned calls = 0;
  const auto* map = VS::Field::roomData.collision_map;
  if (map == nullptr || VS::Field::roomData.tile_cells == nullptr ||
      map->width <= 0 || map->height <= 0 ||
      map->width > EncounterLimits::kTiles ||
      map->height > EncounterLimits::kTiles) {
    last_map = nullptr;
    return nullptr;
  }
  constexpr unsigned kResyncCalls = 120;
  if (map == last_map && ++calls % kResyncCalls != 0) return nullptr;
  last_map = map;
  calls = 0;
  constexpr int kTileSize = 128;
  auto floors = nlohmann::json::array();
  auto ceilings = nlohmann::json::array();
  auto flags = nlohmann::json::array();
  for (int z = 0; z < map->height; ++z) {
    for (int x = 0; x < map->width; ++x) {
      const int world_x = x * kTileSize + kTileSize / 2;
      const int world_z = z * kTileSize + kTileSize / 2;
      const auto* cell = VS::Field::LiveCollisionCellAt(x, z, 0);
      SVECTOR point{static_cast<std::int16_t>(world_x), 0,
                    static_cast<std::int16_t>(world_z), 0};
      const bool passable =
          cell != nullptr && cell->passability <= kFullyBlockedPassability &&
          VS::Field::FindLiveCollisionCellAtPosition(&point, 0) != nullptr;
      const bool pit = cell != nullptr && cell->is_pit;
      floors.push_back(VS::Field::LiveFloorHeightAt(world_x, world_z));
      ceilings.push_back(VS::Field::LiveCeilingHeightAt(world_x, world_z));
      flags.push_back((passable ? 1 : 0) | (pit ? 2 : 0));
    }
  }
  return {{"width", map->width},
          {"depth", map->height},
          {"floor", std::move(floors)},
          {"ceiling", std::move(ceilings)},
          {"flags", std::move(flags)}};
}

// Live VS::Field::gimmickInstances (D_800F45E0). Reads fields only: the
// gameplay queries Field_FindInteractableGimmickAhead (func_800A39F8) and
// VS::Field::LiveGimmickSupportHeight write interaction/support globals.
nlohmann::json EncodePlayerGimmicks() {
  auto gimmicks = nlohmann::json::array();
  const auto* ashley = VS::Actors::actorRecords[0];
  const int carried = ashley != nullptr && ashley->action_pose_active
                          ? ashley->instance.attach_bone
                          : -1;
  for (int slot = 0;
       slot < static_cast<int>(std::size(VS::Field::gimmickInstances));
       ++slot) {
    const auto* object = VS::Field::gimmickInstances[slot];
    if (object == nullptr) continue;
    gimmicks.push_back(
        {{"slot", slot},
         {"object_id", object->object_info.object_id},
         {"interaction", object->object_info.interaction},
         {"falling", object->object_info.is_falling != 0},
         {"tile_x", static_cast<int>(object->tile_x)},
         {"tile_z", static_cast<int>(object->tile_z)},
         {"position",
          {object->position_x, object->position_y, object->position_z}},
         {"mode", object->mode},
         {"motion_state", object->motion_state},
         {"skipped", object->is_skipped != 0},
         {"visible", object->is_visible != 0},
         {"parent", object->parent},
         {"carrier", object->object_info.carrier},
         {"carried", slot == carried}});
  }
  return gimmicks;
}

nlohmann::json EncodePlayerChests(const PlayerObservationState& state) {
  auto chests = nlohmann::json::array();
  for (const auto& chest : state.chests) {
    auto misc = nlohmann::json::array();
    for (const auto& item : chest.misc) {
      misc.push_back(
          {{"slot", item.slot}, {"id", item.id}, {"count", item.count}});
    }
    chests.push_back({{"index", chest.index},
                      {"tile_x", chest.tile_x},
                      {"tile_z", chest.tile_z},
                      {"layer", chest.layer},
                      {"facing", chest.facing},
                      {"lock_level", chest.lock_level},
                      {"opened_flag", chest.opened_flag},
                      {"opened", chest.opened},
                      {"misc", std::move(misc)}});
  }
  return chests;
}

nlohmann::json EncodePlayerOptions(const PlayerObservationState& state) {
  auto options = nlohmann::json::array();
  for (const auto& option : state.action_options) {
    options.push_back({{"id", option.id},
                       {"name", option.name},
                       {"family", option.family},
                       {"menu_order", option.menu_order},
                       {"type", option.type},
                       {"cost", option.cost},
                       {"restrictions", option.restrictions},
                       {"ai_flags", option.ai_flags},
                       {"rank", option.rank},
                       {"rank_count", option.rank_count}});
  }
  return options;
}

nlohmann::json EncodeNavigation(const EncounterNavigationState& state) {
  return {{"available", state.available},
          {"origin", {state.origin_x, state.origin_z}},
          {"width", state.width},
          {"depth", state.depth},
          {"spacing", state.spacing},
          {"collision_radius", state.collision_radius},
          {"collision_height", state.collision_height},
          {"movement_mode", state.movement_mode},
          {"locomotion_state", state.locomotion_state},
          {"route_phase", state.route_phase},
          {"jumping", state.jumping},
          {"facing_tile", state.facing_tile},
          {"columns", {"packed_support", "packed_ceiling", "flags"}},
          {"samples", state.samples}};
}
nlohmann::json EncodePlayerActor(const PlayerActorObservation& actor) {
  auto data = EncodeActor(actor);
  data.update(
      {{"model", actor.model},
       {"status_immunities", actor.status_immunities},
       {"action_ids", actor.action_ids},
       {"spell_action_ids", actor.spell_action_ids},
       {"base_attack_type", actor.base_attack_type},
       {"shield_pp", actor.shield_pp},
       {"weapon_pp", actor.weapon_pp},
       {"weapon_range",
        {{"components_world", actor.weapon_range.components_world},
         {"shape", actor.weapon_range.shape},
         {"angle_steps", actor.weapon_range.angle_steps},
         {"native_extent_world", actor.weapon_range.native_extent_world}}},
       {"relation_to_player", actor.relation_to_player},
       {"relation_active", actor.relation_active},
       {"player_visible", actor.player_visible},
       {"hostile_to_player", actor.hostile_to_player}});
  return data;
}
}  // namespace

PlayerObservationState InspectPlayerObservation(const EncounterState& base) {
  PlayerObservationState state;
  static_cast<EncounterState&>(state) = base;
  if (!base.ready) return state;
  state.reaction_timing = InspectReactionTiming(base);
  for (std::size_t index = 0; index < state.status_masks.size(); ++index) {
    state.status_masks[index] = static_cast<std::uint32_t>(
        VS::Battle::statusEffectParams[index].status_flags);
  }
  InspectActionOptions(state);
  InspectItemOptions(state);
  state.menu = InspectPlayerMenu(base);
  InspectRoomDoors(state);
  InspectRoomChests(state);
  state.navigation = InspectNavigation();
  state.target_selection_step = VS::Battle::targetSelectionStep;
  for (const auto& actor : base.actors) {
    state.player_actors.push_back(InspectPlayerActor(actor));
  }
  return state;
}

namespace {
nlohmann::json EncodeTargetForecasts(const EncounterState& base) {
  auto forecasts = nlohmann::json::array();
  if (!base.ready || base.target_previews.empty()) return forecasts;
  const auto* action = VS::Battle::actionState;
  if (action == nullptr ||
      base.target_previews.size() != action->preview_count ||
      action->preview_count > std::size(action->previews)) {
    throw std::runtime_error(
        "native target forecast storage changed within a frame");
  }
  constexpr std::array<int, 4> kAmountEffects{58, 59, 62, 63};
  for (std::size_t index = 0; index < base.target_previews.size(); ++index) {
    const auto& target = action->previews[index];
    const auto& observed = base.target_previews[index];
    if (target.actor_id != observed.actor || target.limb != observed.limb) {
      throw std::runtime_error(
          "native target forecast identity changed within a frame");
    }
    // Original 146C.c:3235 copies these cached values without another roll.
    const int effect = static_cast<std::uint8_t>(target.effect);
    nlohmann::json forecast = {{"actor", target.actor_id},
                               {"limb", target.limb},
                               {"effect_id", effect},
                               {"hit_threshold", target.hit_chance},
                               {"amount", nullptr}};
    if (std::ranges::find(kAmountEffects, effect) != kAmountEffects.end()) {
      forecast["amount"] = std::bit_cast<std::uint16_t>(target.preview_amount);
    }
    forecasts.push_back(std::move(forecast));
  }
  return forecasts;
}
}  // namespace

void AppendPlayerObservations(nlohmann::json& data,
                              const EncounterState& base) {
  const auto state = InspectPlayerObservation(base);
  data["player_schema"] = kPlayerObservationSchema;
  data["reaction_timing"] = EncodeReaction(state.reaction_timing);
  data["status_masks"] = state.status_masks;
  data["action_options"] = EncodePlayerOptions(state);
  data["menu"] = EncodePlayerMenu(state.menu);
  data["doors"] = EncodePlayerDoors(state);
  data["chests"] = EncodePlayerChests(state);
  data["gimmicks"] = EncodePlayerGimmicks();
  if (auto tiles = EncodeRoomTiles(); !tiles.is_null()) {
    data["room_tiles"] = std::move(tiles);
  }
  data["navigation"] = EncodeNavigation(state.navigation);
  auto items = nlohmann::json::array();
  for (const auto& item : state.item_options) {
    items.push_back({{"id", item.id},
                     {"action_id", item.action_id},
                     {"count", item.count},
                     {"name", item.name},
                     {"restrictions", item.restrictions},
                     {"effect_id", item.effect_id},
                     {"calculator_id", item.calculator_id},
                     {"stat_factor", item.stat_factor}});
  }
  data["item_options"] = std::move(items);
  data["target_page"]["selection_step"] = state.target_selection_step;
  data["target_forecasts"] = EncodeTargetForecasts(base);
  auto actors = nlohmann::json::array();
  for (const auto& actor : state.player_actors) {
    actors.push_back(EncodePlayerActor(actor));
  }
  data["actors"] = std::move(actors);
}
}  // namespace VS::Game
