#include "Diagnostics/PlayerObservation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <format>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "Diagnostics/GameplayTrace.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Damage.h"
#include "Battle/EncounterHooks.h"
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
#include "Core/Text/TextCode.h"
#include "Event/ScriptVm.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Input/Controller.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Magic.h"
#include "State/Flags.h"
#include "State/Timers.h"
}

namespace VS::Game {
namespace {
constexpr unsigned kFullyBlockedPassability = 5;
constexpr int kFootprintClearFlag = 4;
void InspectActorActions(const Battle_ActorStats& stats,
                         PlayerActorObservation& snapshot) {
  for (const auto& limb : stats.limbActions) {
    for (const auto& action : limb) {
      if (action.actionId != 0 &&
          action.actionId < std::size(Battle_actionDefinitions) &&
          std::ranges::find(snapshot.action_ids, action.actionId) ==
              snapshot.action_ids.end()) {
        snapshot.action_ids.push_back(action.actionId);
        if (Battle_actionDefinitions[action.actionId].type ==
            Battle_ActionType_Spell) {
          snapshot.spell_action_ids.push_back(action.actionId);
        }
      }
    }
  }
}

PlayerActorObservation InspectPlayerActor(const EncounterActorState& base) {
  PlayerActorObservation snapshot;
  static_cast<EncounterActorState&>(snapshot) = base;
  const int slot = base.slot;
  const auto* actor = Battle_actors[slot];
  if (actor == nullptr || actor->stats == nullptr) {
    throw std::runtime_error("diagnostic actor vanished within a frame");
  }
  const auto& stats = *actor->stats;
  if (Actors_actorRecords[slot] != nullptr) {
    snapshot.model = Actors_actorRecords[slot]->modelId;
  }
  snapshot.status_immunities =
      static_cast<std::uint32_t>(stats.statusImmunities);
  InspectActorActions(stats, snapshot);
  snapshot.base_attack_type = stats.weapon.actionType;
  snapshot.shield_pp = stats.shield.currentPp;
  snapshot.weapon_pp = stats.weapon.currentPp;
  // Original 7CD68.c:437 reads this four-byte range shape without modifying it.
  constexpr int kRangeUnit = 32;
  Battle_Ai_ActionRangeShape range{};
  range.rangeY =
      std::bit_cast<char>(static_cast<std::uint8_t>(stats.currentRangeY));
  range.rangeZ =
      std::bit_cast<char>(static_cast<std::uint8_t>(stats.currentRangeZ));
  range.shape = stats.currentAttackShape;
  range.angle = stats.currentAttackShapeAngle;
  snapshot.weapon_range = {{static_cast<int>(stats.currentRange) * kRangeUnit,
                            static_cast<int>(stats.currentRangeY) * kRangeUnit,
                            static_cast<int>(stats.currentRangeZ) * kRangeUnit},
                           static_cast<int>(stats.currentAttackShape),
                           static_cast<int>(stats.currentAttackShapeAngle),
                           Battle_CalculateRangeShapeExtent(&range)};
  if (const auto* ai = Battle_Ai_actorAiStates[slot]; ai != nullptr) {
    constexpr unsigned kHostileRelation = 3;
    const auto& relation = ai->relations[0];
    snapshot.relation_to_player = static_cast<int>(relation.classification);
    snapshot.relation_active = relation.active != 0;
    snapshot.player_visible = relation.visible != 0;
    snapshot.hostile_to_player = relation.classification == kHostileRelation;
  }
  return snapshot;
}

std::string InspectGameText(std::span<const char> bytes) {
  constexpr std::string_view kLetters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::string result;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const auto value = static_cast<unsigned char>(bytes[index]);
    if (value == Core_TextCode_Terminator) break;
    if (value >= Core_TextCode_ChunkSize) {
      if (++index >= bytes.size()) break;
    } else if (value < kLetters.size()) {
      result += kLetters[value];
    } else if (value == Core_TextCode_Space) {
      result += ' ';
    } else if (value == Core_TextCode_Hyphen) {
      result += '-';
    } else {
      result += std::format("[{:02X}]", value);
    }
  }
  return result;
}

void InspectActionOptions(PlayerObservationState& state) {
  const auto* player = Battle_actors[0];
  if (player == nullptr || player->stats == nullptr ||
      Battle_actionState == nullptr) {
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
      if (id == 0 || !Battle_actionDefinitions[id].unlocked) continue;
      const auto& definition = Battle_actionDefinitions[id];
      state.action_options.push_back(
          {id, InspectGameText(definition.name), std::string(family),
           menu_order++, static_cast<int>(definition.type),
           static_cast<int>(definition.cost),
           Battle_ActorActionRestrictionFlags(0, id),
           std::bit_cast<std::uint16_t>(definition.aiFlags)});
    }
  };
  // Counts are the original menu loops, not the overbroad SDK array extents.
  constexpr std::size_t kWarlockRows = 18, kShamanRows = 6;
  constexpr std::size_t kSorcererRows = 18, kEnchanterRows = 8;
  constexpr std::size_t kBattleAbilityRows = 14;
  append(Menu_Magic_warlockSpellIds, kWarlockRows, "warlock");
  append(Menu_Magic_shamanSpellIds, kShamanRows, "shaman");
  append(Menu_Magic_sorcererSpellIds, kSorcererRows, "sorcerer");
  append(Menu_Magic_enchanterSpellIds, kEnchanterRows, "enchanter");
  append(Battle_chainAbilityActionIds, kBattleAbilityRows, "chain");
  append(Battle_defenseAbilityActionIds, kBattleAbilityRows, "defense");
}

void InspectItemOptions(PlayerObservationState& state) {
  const auto* player = Battle_actors[0];
  if (player == nullptr || player->stats == nullptr) return;
  constexpr int kFirstItem = 323, kLastUsableItem = 457;
  constexpr unsigned kFirstRankedSpell = 106, kRankedSpellCount = 28;
  constexpr unsigned kRanks = 3;
  // MENU/Inventory.c item shortcut uses this map without granting a spell.
  for (const auto& item : state.items) {
    if (item.id < kFirstItem || item.id > kLastUsableItem || item.count <= 0) {
      continue;
    }
    unsigned action = Menu_Inventory_miscItemToActionMap[item.id - kFirstItem];
    if (action - kFirstRankedSpell < kRankedSpellCount) {
      for (unsigned rank = 0; rank < kRanks; ++rank) {
        if (Battle_actionDefinitions[action].unlocked) ++action;
      }
    }
    if (action >= std::size(Battle_actionDefinitions)) {
      throw std::runtime_error(
          "observed item action exceeds native definitions");
    }
    const auto& definition = Battle_actionDefinitions[action];
    const auto& hit = definition.hits[0];
    state.item_options.push_back(
        {item.id, static_cast<int>(action), item.count,
         InspectGameText(definition.name),
         Battle_ActorActionRestrictionFlags(0, static_cast<int>(action)),
         static_cast<int>(hit.effectId), static_cast<int>(hit.statCalculatorId),
         static_cast<int>(hit.statFactor)});
  }
}

void InspectItemMenuRow(EncounterMenuRowState& row) {
  if (row.index < 0 || static_cast<std::size_t>(row.index) >=
                           std::size(Audio_Seq_unusedInventoryIndices.misc)) {
    throw std::runtime_error("item menu row exceeds inventory index storage");
  }
  const auto slot = static_cast<unsigned char>(
      Audio_Seq_unusedInventoryIndices.misc[row.index]);
  if (slot == 0 || slot > std::size(Audio_Seq_unusedInventory.misc)) {
    throw std::runtime_error(
        "item menu row has an invalid native inventory index");
  }
  const auto& item = Audio_Seq_unusedInventory.misc[slot - 1];
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
  result.event_owned =
      state.control_mode == kEventControl && Event_isMenuTransitionActive != 0;
  if ((state.control_mode != kMenuControl && !result.event_owned &&
       !(state.control_mode == kCombatControl &&
         (state.action_phase == kAbilityUnlockPhase ||
          state.action_phase == kLootPhase)) &&
       !(state.control_mode == kChestControl &&
         state.action_phase == kChestContentsPhase)) ||
      Menu_Framework_menuItems == nullptr) {
    return result;
  }
  result.available = true;
  result.loading = Menu_Framework_menuState.loading != 0;
  result.shortcut = static_cast<unsigned char>(Menu_Framework_shortcutInvoked);
  if ((state.menu_state & kMenuIdMask) == kLootPhase) {
    result.loot_input_ready = Menu_Loot_controlsEnabled != 0;
    result.loot_count = Menu_Loot_lootListCount;
  }
  result.selected_menu = static_cast<int>(state.menu_state & kMenuIdMask);
  result.submenu_phase = static_cast<unsigned char>(
      Menu_Framework_submenuStates[state.menu_state & kSubmenuStateMask]);
  result.selected_row = state.menu_highlighted_row;
  result.list_phase = static_cast<unsigned char>(Menu_Framework_itemsListRow);
  result.assignment_phase =
      static_cast<int>(Menu_BattleAbilities_abilityMenuState);
  result.assignment_type =
      static_cast<unsigned char>(Menu_BattleAbilities_selectedAbilityType);
  result.assignment_selected = Menu_BattleAbilities_highlightedAbility +
                               Menu_BattleAbilities_firstAbility;
  if (result.selected_menu == 0) {
    for (int index = 0; index < kMainMenuEntries; ++index) {
      if ((Input_settings.menuFlags & (1u << index)) == 0) continue;
      const auto& row = Menu_Framework_menuItems[index];
      result.rows.push_back({index, InspectGameText(row.text),
                             row.unselectable != 0, row.selected != 0});
      if (row.selected != 0) result.selected_row = index;
    }
  } else if (!result.loading && Menu_Framework_itemsText != nullptr) {
    constexpr unsigned kMaximumObservedRows = 256;
    if (Menu_Framework_subMenuRowCount > kMaximumObservedRows) {
      throw std::runtime_error("menu inspection exceeds supported row count");
    }
    const auto* text = reinterpret_cast<const Menu_Framework_ItemText*>(
        Menu_Framework_itemsText);
    for (unsigned index = 0; index < Menu_Framework_subMenuRowCount; ++index) {
      const auto* name = reinterpret_cast<const char*>(text[index].name);
      result.rows.push_back(
          {static_cast<int>(index),
           InspectGameText(std::span(name, sizeof(text[index].name))),
           (text[index].menuRowFlags & Menu_Framework_RowTypeFlag_Disabled) !=
               0,
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

void InspectIncomingEffect(const Battle_ActionState& action,
                           const Battle_ActionRequest& queued,
                           EncounterReactionTimingState& snapshot) {
  constexpr unsigned kHpDamage = 1, kCombinedDamage = 3;
  const auto incoming_id = action.selection.actionId;
  if (incoming_id < std::size(Battle_actionDefinitions)) {
    snapshot.incoming_type = Battle_actionDefinitions[incoming_id].type;
    snapshot.incoming_cost = Battle_actionDefinitions[incoming_id].cost;
    snapshot.incoming_affinity =
        static_cast<unsigned char>(action.selection.effectVariant);
  }
  if (queued.targetCount > std::size(queued.targets)) {
    throw std::runtime_error(
        "reaction inspection exceeds native target storage");
  }
  for (unsigned index = 0; index < queued.targetCount; ++index) {
    const auto& hit = queued.targets[index];
    if (hit.targetsPosition != 0 || hit.target.targetActor != 0) continue;
    if (hit.hpEffect == kHpDamage || hit.hpEffect == kCombinedDamage) {
      snapshot.incoming_hp_damage +=
          std::max(0, static_cast<int>(hit.target.hp));
    }
    if (hit.mpEffect == kHpDamage || hit.mpEffect == kCombinedDamage) {
      snapshot.incoming_mp_damage +=
          std::max(0, static_cast<int>(hit.target.mp));
    }
    snapshot.incoming_statuses |=
        static_cast<std::uint32_t>(hit.removeStatuses);
  }
}

void InspectReactionEligibility(EncounterReactionTimingState& snapshot) {
  const auto present = [](int slot) {
    return slot >= 0 && slot < EncounterLimits::kActorSlots &&
           Battle_actors[slot] != nullptr &&
           Battle_actors[slot]->stats != nullptr;
  };
  if (!present(snapshot.source_actor) || !present(snapshot.target_actor) ||
      Battle_characterState == nullptr ||
      Battle_characterState->stats == nullptr) {
    snapshot.eligibility_status = "actor_state_unavailable";
    return;
  }
  snapshot.eligibility_status = "evaluated";
  const auto& mapped = snapshot.source_actor == 0 ? snapshot.chain_actions
                                                  : snapshot.defense_actions;
  for (std::size_t slot = 0; slot < mapped.size(); ++slot) {
    if (mapped[slot] == 0) continue;
    if (static_cast<std::size_t>(mapped[slot]) >=
        std::size(Battle_actionDefinitions)) {
      snapshot.eligibility_status = "invalid_equipped_action";
      snapshot.eligible.fill(false);
      return;
    }
    snapshot.eligible[slot] =
        Battle_CanTriggerBattleAbility(static_cast<int>(slot) + 1) != 0;
  }
}

EncounterReactionTimingState InspectReactionTiming(
    const EncounterState& state) {
  EncounterReactionTimingState snapshot;
  snapshot.tick_speed = State_tickSpeed;
  snapshot.story_timing = State_ScriptFlag(1) != 0;
  for (std::size_t slot = 0; slot < snapshot.chain_actions.size(); ++slot) {
    snapshot.chain_actions[slot] =
        static_cast<unsigned char>(Input_settings.mappedChainAbilities[slot]);
    snapshot.defense_actions[slot] =
        static_cast<unsigned char>(Input_settings.mappedDefenseAbilities[slot]);
  }
  constexpr unsigned kAbilityTimingPhase = 7, kQueueIndexMask = 3;
  if (Battle_actionState == nullptr) return snapshot;
  const auto& action = *Battle_actionState;
  snapshot.active =
      state.control_mode == 2 && state.action_phase == kAbilityTimingPhase;
  snapshot.queue_index = action.queueIndex;
  snapshot.chain_step = action.chainStep;
  snapshot.timer = action.abilityTimer;
  snapshot.window = action.abilityWindow;
  snapshot.previous_slot =
      static_cast<unsigned char>(action.previousChainInput);
  snapshot.previous_buttons =
      static_cast<unsigned char>(action.previousChainButtons);
  snapshot.attempted = action.abilityInputAttempted != 0;
  snapshot.succeeded = static_cast<unsigned char>(action.abilityInputSucceeded);
  const auto& queued = action.queue[action.queueIndex & kQueueIndexMask];
  snapshot.action = queued.actionId;
  InspectIncomingEffect(action, queued, snapshot);
  if (queued.primaryTarget.targetsPosition != 0 || queued.targetCount == 0 ||
      queued.targets[0].targetsPosition != 0) {
    if (snapshot.active) snapshot.eligibility_status = "not_actor_targeted";
    return snapshot;
  }
  snapshot.source_actor = queued.primaryTarget.target.targetActor;
  snapshot.target_actor = queued.targets[0].target.targetActor;
  if (!snapshot.active) return snapshot;
  InspectReactionEligibility(snapshot);
  return snapshot;
}

void InspectRoomDoors(PlayerObservationState& state) {
  if (Field_roomData.doors == nullptr) return;
  if (Field_roomData.header.doorsLength % sizeof(Field_MpdDoorRecord) != 0) {
    throw std::runtime_error(
        "observed room door storage has an incomplete record");
  }
  const auto count =
      Field_roomData.header.doorsLength / sizeof(Field_MpdDoorRecord);
  for (unsigned index = 0; index < count; ++index) {
    const auto& door = Field_roomData.doors[index];
    state.doors.push_back(
        {static_cast<int>(index),
         door.destinationZoneId,
         door.destinationRoomId,
         static_cast<int>(door.tileLocation.tileX),
         static_cast<int>(door.tileLocation.tileZ),
         static_cast<int>(door.tileLocation.floorLayer),
         {static_cast<int>(door.entryTileX), static_cast<int>(door.entryTileZ),
          static_cast<int>(door.entryElevation), 0},
         static_cast<int>(door.requiredFlagId),
         static_cast<int>(door.requiredFlagValue),
         door.doorId,
         door.hasEntryStep != 0});
    state.door_flag_values.push_back(static_cast<unsigned char>(
        State_ScriptFlag(static_cast<std::int16_t>(door.requiredFlagId))));
  }
}

// Mirrors Items_RoomTreasureChests (func_8008E370) and the opened-flag test in
// Field_FindFacingUnlootedChest (func_80074950): openedFlag + 0x3C0.
void InspectRoomChests(PlayerObservationState& state) {
  int count = 0;
  const auto* chests = Items_RoomTreasureChests(&count);
  if (chests == nullptr) return;
  if (Field_roomData.header.containersLength % sizeof(Items_TreasureChest) !=
      0) {
    throw std::runtime_error(
        "observed room container storage has an incomplete record");
  }
  constexpr int kChestOpenedFlagBase = 0x3C0;
  for (int index = 0; index < count; ++index) {
    const auto& chest = chests[index];
    EncounterChestState snapshot;
    snapshot.index = index;
    snapshot.tile_x = chest.tileX;
    snapshot.tile_z = chest.tileZ;
    snapshot.layer = chest.layer;
    snapshot.facing = chest.facing;
    snapshot.lock_level = chest.lockLevel;
    snapshot.opened_flag = chest.openedFlag + kChestOpenedFlagBase;
    snapshot.opened =
        State_ScriptFlag(static_cast<std::int16_t>(snapshot.opened_flag)) != 0;
    for (std::size_t slot = 0; slot < std::size(chest.loot.misc); ++slot) {
      const auto& item = chest.loot.misc[slot];
      if (item.itemId != 0 && item.count != 0) {
        snapshot.misc.push_back(
            {static_cast<int>(slot), item.itemId, item.count});
      }
    }
    state.chests.push_back(std::move(snapshot));
  }
}

int NavigationSupportHeight(int x, int z) {
  const int packed = Field_FloorHeightAt(x, z);
  if (packed < 0) return packed;
  constexpr int kTileSize = 128, kInactiveMode = 0xFE;
  constexpr int kNoSupport = 3000, kHeightMask = 0x7FFF;
  int support = kNoSupport;
  // Field_GimmickSupportHeight, original 40564.c:317. Do not call that
  // gameplay query here: it writes Actors_supportingGimmickIndex.
  for (const auto* object : Field_gimmickInstances) {
    if (object != nullptr && !object->isSkipped && !object->motionState &&
        object->mode != kInactiveMode && object->tileX == x / kTileSize &&
        object->tileZ == z / kTileSize) {
      support = std::min(support, static_cast<int>(object->positionY));
    }
  }
  support -= kTileSize;
  return support < 0 ? support & kHeightMask : packed;
}

bool NavigationPointClear(int x, int z, int floor_height, int body_height) {
  const auto* map = Field_roomData.collisionMap;
  constexpr int kTileSize = 128, kHeightSign = 0x4000, kHeightMask = 0x7FFF;
  if (x < 0 || z < 0 || x >= map->width * kTileSize ||
      z >= map->height * kTileSize) {
    return false;
  }
  const auto* cell = Field_CollisionCellAt(x / kTileSize, z / kTileSize, 0);
  if (cell->doorPassability > kFullyBlockedPassability || cell->isPit) {
    return false;
  }
  SVECTOR point{static_cast<std::int16_t>(x), 0, static_cast<std::int16_t>(z),
                0};
  if (Field_FindCollisionCellAtPosition(&point, 0) == nullptr) return false;
  const int floor = NavigationSupportHeight(x, z);
  const int ceiling = Field_CeilingHeightAt(x, z);
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
  const auto* map = Field_roomData.collisionMap;
  const auto* player = Actors_actorRecords[0];
  if (map == nullptr || player == nullptr ||
      Field_roomData.tileCells == nullptr) {
    return result;
  }
  constexpr int kPatchSide = 13, kHalfPatch = kPatchSide / 2;
  constexpr int kTileSize = 128;
  const int cells = map->width * map->height;
  if (map->width <= 0 || map->height <= 0 ||
      map->width > EncounterLimits::kTiles ||
      map->height > EncounterLimits::kTiles ||
      Field_roomData.header.collisionMapLength < sizeof(*map) ||
      map->cellCount != cells ||
      Field_roomData.header.tileCellsLength <
          cells * sizeof(Field_MpdTileCell)) {
    throw std::runtime_error("invalid runtime navigation storage");
  }
  Actors_ActorLocationQuery placement{};
  Actors_ActorPlacement(0, &placement);
  constexpr std::size_t kFacingTileBytes = 4;
  std::array<u_char, kFacingTileBytes> facing{};
  Field_TileInFrontOfActor(&placement, facing.data());
  result.facing_tile = {facing[0], facing[2], facing[1]};
  result.collision_radius = player->collisionRadius;
  result.collision_height = player->collisionHeight;
  result.movement_mode = player->instance.movementMode;
  result.locomotion_state = player->instance.locomotionState;
  result.route_phase = player->routePhase;
  result.jumping = player->instance.isJumping != 0;
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
      const auto* cell =
          Field_CollisionCellAt(world_x / kTileSize, world_z / kTileSize, 0);
      SVECTOR point{static_cast<std::int16_t>(world_x), 0,
                    static_cast<std::int16_t>(world_z), 0};
      const bool passable =
          cell->doorPassability <= kFullyBlockedPassability &&
          Field_FindCollisionCellAtPosition(&point, 0) != nullptr;
      result.samples.push_back(
          {NavigationSupportHeight(world_x, world_z),
           Field_CeilingHeightAt(world_x, world_z),
           (passable ? 1 : 0) | (cell->isPit ? 2 : 0) |
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
                       {"ai_flags", option.ai_flags}});
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
        Battle_statusEffectParams[index].statusFlags);
  }
  InspectActionOptions(state);
  InspectItemOptions(state);
  state.menu = InspectPlayerMenu(base);
  InspectRoomDoors(state);
  InspectRoomChests(state);
  state.navigation = InspectNavigation();
  state.target_selection_step = Battle_targetSelectionStep;
  for (const auto& actor : base.actors) {
    state.player_actors.push_back(InspectPlayerActor(actor));
  }
  return state;
}

namespace {
nlohmann::json EncodeTargetForecasts(const EncounterState& base) {
  auto forecasts = nlohmann::json::array();
  if (!base.ready || base.target_previews.empty()) return forecasts;
  const auto* action = Battle_actionState;
  if (action == nullptr ||
      base.target_previews.size() != action->previewCount ||
      action->previewCount > std::size(action->previews)) {
    throw std::runtime_error(
        "native target forecast storage changed within a frame");
  }
  constexpr std::array<int, 4> kAmountEffects{58, 59, 62, 63};
  for (std::size_t index = 0; index < base.target_previews.size(); ++index) {
    const auto& target = action->previews[index];
    const auto& observed = base.target_previews[index];
    if (target.actorId != observed.actor || target.limb != observed.limb) {
      throw std::runtime_error(
          "native target forecast identity changed within a frame");
    }
    // Original 146C.c:3235 copies these cached values without another roll.
    const int effect = static_cast<std::uint8_t>(target.effect);
    nlohmann::json forecast = {{"actor", target.actorId},
                               {"limb", target.limb},
                               {"effect_id", effect},
                               {"hit_threshold", target.hitChance},
                               {"amount", nullptr}};
    if (std::ranges::find(kAmountEffects, effect) != kAmountEffects.end()) {
      forecast["amount"] = std::bit_cast<std::uint16_t>(target.previewAmount);
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
