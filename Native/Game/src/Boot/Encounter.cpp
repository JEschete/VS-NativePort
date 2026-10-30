#include "Battle/Encounter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Battle/TargetPages.h"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Diagnostics/GameplayTrace.hpp"
#include "Field/EncounterSpawnProfile.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Damage.h"
#include "Battle/EncounterHooks.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Boot/GameLoop.h"
#include "Boot/Lifecycle.h"
#include "Boot/PlayerSettings.h"
#include "Boot/Session.h"
#include "Content/DiscLayout.h"
#include "Core/BiosRand.h"
#include "Core/Diagnostics.h"
#include "Core/Random.h"
#include "Core/Scratch.h"
#include "Effects/Runtime.h"
#include "Event/Cutscene.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/Camera.h"
#include "Field/Doors.h"
#include "Field/GameMode.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoading.h"
#include "Field/RoomRendering.h"
#include "Field/Transitions.h"
#include "Field/Zone.h"
#include "Input/Controller.h"
#include "Items/EquipmentStats.h"
#include "Items/Loot.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Flags.h"
#include "State/Location.h"
#include "State/Progress.h"
#include "Title/TitleMenu.h"
}

namespace VS::Game {
namespace {
EncounterSetup configuration;
std::array<Field_MpdEnemySpawn, EncounterLimits::kEnemies> placements{};
std::atomic<bool> stop_requested{false};
bool configured = false;
bool active = false;
bool entered = false;
bool room_prepared = false;
bool gameplay_inspection_ready = false;
bool player_stats_initialized = false;
bool external_event_started = false;

std::optional<EncounterRouletteState> InspectRoulette(int menu) {
  constexpr int kResultsMenu = 15;
  if (menu != kResultsMenu || Audio_Seq_unusedStateFlags.menuFSubmenu != 0) {
    return std::nullopt;
  }
  return EncounterRouletteState{
      .entry_phase = Menu_Framework_submenuStates[kResultsMenu],
      .screen_state = Menu_Results_screenState,
      .timer = Menu_Results_screenTimer,
      .animation = Menu_Results_elementAnimationState,
      .reel = Menu_Results_buffReelIndex,
      .selection = Menu_Results_buffReelSelection,
      .bios_state = Core_Random_State()};
}
constexpr int kDirectEventTrigger = 0x40;
int pending_door = -1;
int pending_door_zone = 0;
int pending_door_room = 0;

int ResumeEncounter() {
  Boot_isGameplayActive = 1;
  Boot_RunGameLoop();
  return 0;
}

int EnterOrFinishEncounter() {
  if (entered) return 0;
  entered = true;
  // Reuse _sysReinit and _initGameData rather than manufacturing player stats.
  // Provenance: SLUS main.c:8309; TITLE/22C.c:4698.
  Boot_InitializeTitleSession();
  Boot_InitializePlayerSettings();
  Title_InitializeNewGameData();
  State_savedEntryDoor.destinationZoneId = configuration.zone;
  State_savedEntryDoor.destinationRoomId = configuration.room;
  State_savedEntryDoor.entryTileX = configuration.player.x;
  State_savedEntryDoor.entryTileZ = configuration.player.z;
  State_savedEntryDoor.entryElevation = configuration.player.elevation;
  State_savedEntryDoor.tileLocation.entryDirection =
      configuration.player.direction;
  Boot_gameEntryMode = 0;
  if (configuration.attract_demo) {
    // Title_Run's idle timeout: _setTitleExitFlags (TITLE.PRG 22C.c:4741)
    // sets the demo flags and door; its cause becomes vs_battle_exec's entry
    // mode (SLUS main.c:8344), which returns to title (146C.c:7558).
    Title_PrepareExit(*configuration.attract_demo);
    Boot_gameEntryMode = *configuration.attract_demo;
  }
  Boot_isClearSavePending = 0;
  Boot_LoadBattleOverlays();
  InitializeEncounterPlayerWeapon(configuration.weapon);
  InitializeEncounterPlayerProfile(configuration.profile);
  if (configuration.story_progress) {
    // Script flag 0x82 is the campaign progress counter tested by MPD enemy
    // placements (Field_SpawnRoomActors, BATTLE.PRG func_80089114,
    // 146C.c:14901). Declared Lab fixture state, written before any room
    // loads; never changed during play.
    constexpr std::int16_t kStoryProgressFlag = 0x82;
    State_SetScriptFlag(kStoryProgressFlag,
                        static_cast<char>(*configuration.story_progress));
    VS_LOG_INFO("encounter", "declared story progress={}",
                *configuration.story_progress);
  }
  // The original shuffle initialization always seeds with 1. Initialize that
  // same table from the requested seed; combat still calls its real RNG.
  Core_Random_Seed(configuration.seed);
  for (auto& value : Core_randomShuffleTable) value = Core_Random_Rand();
  Core_randomShuffleIndex =
      static_cast<int>(std::size(Core_randomShuffleTable)) - 1;
  Core_interfaceRandomState = configuration.seed;
  VS_LOG_INFO("encounter", "starting zone={} room={} enemies={} seed={}",
              configuration.zone, configuration.room,
              configuration.enemies.size(), configuration.seed);
  return ResumeEncounter();
}

EncounterActorState InspectActor(int slot, const Battle_Actor& actor) {
  const auto& stats = *actor.stats;
  EncounterActorState snapshot;
  snapshot.slot = slot;
  snapshot.hp = stats.currentHp;
  snapshot.mp = stats.currentMp;
  snapshot.max_hp = stats.maxHp;
  snapshot.max_mp = stats.maxMp;
  snapshot.risk = stats.risk;
  snapshot.strength = stats.strength;
  snapshot.intelligence = stats.intelligence;
  snapshot.agility = stats.agility;
  snapshot.total_strength = stats.totalStrength;
  snapshot.total_intelligence = stats.totalIntelligence;
  snapshot.total_agility = stats.totalAgility;
  snapshot.walk_speed = static_cast<unsigned char>(stats.walkSpeed);
  snapshot.run_speed = static_cast<unsigned char>(stats.runSpeed);
  snapshot.statuses = static_cast<std::uint32_t>(stats.statuses);
  std::ranges::copy(stats.statusTimers, snapshot.status_timers.begin());
  snapshot.command = actor.commandOpcode;
  snapshot.command_state = actor.commandState;
  snapshot.weapon_drawn = actor.weaponDrawn;
  snapshot.weapon_category = actor.equippedWeaponCategory;
  snapshot.weapon_blade = stats.weapon.blade.id;
  snapshot.weapon_grip = stats.weapon.grip.id;
  snapshot.weapon_model = static_cast<unsigned char>(stats.weapon.wepId);
  snapshot.shield_model = static_cast<unsigned char>(stats.shield.wepId);
  snapshot.actor_flags = Actors_ActorStateFlags(slot);
  snapshot.recovery = stats.actionRecoveryTimer;
  Actors_ActorLocationQuery location{};
  Actors_ActorPlacement(slot, &location);
  snapshot.x = location.location.position.vx;
  snapshot.y = location.location.position.vy;
  snapshot.z = location.location.position.vz;
  snapshot.spawn_index = actor.roomSpawnIndex;
  snapshot.defeated = actor.defeated != 0;
  snapshot.awareness_marker =
      static_cast<unsigned char>(Battle_actorMarkers[slot]);
  const auto* ai = Battle_Ai_actorAiStates[slot];
  if (ai != nullptr) {
    snapshot.ai_present = true;
    snapshot.ai_candidates = ai->candidateCount;
    snapshot.ai_engagement_frames = ai->engagementFrames;
    snapshot.ai_movement_mode = ai->requestedMovementMode;
    snapshot.ai_destination_x = ai->destination.tile.tileX;
    snapshot.ai_destination_z = ai->destination.tile.tileZ;
    if (ai->candidateCount > 0 && ai->candidateOrder[0] != nullptr) {
      snapshot.ai_target = ai->candidateOrder[0]->targetId;
    }
  }
  return snapshot;
}

void InspectTargeting(EncounterState& state, const Battle_ActionState& action) {
  state.action = action.selection.actionId;
  state.source_actor = action.selection.primaryTarget.target.targetActor;
  state.target_count = action.selection.targetCount;
  const auto page = Battle_GetTargetPageInfo();
  state.target_page = {page.total, page.page,    page.pages,
                       page.first, page.visible, page.selected};
  constexpr unsigned kTargetingControl = 2;
  constexpr unsigned kFirstTargetPhase = 1, kLastTargetPhase = 3;
  if (state.control_mode != kTargetingControl ||
      state.action_phase < kFirstTargetPhase ||
      state.action_phase > kLastTargetPhase) {
    return;
  }
  const auto& center = action.areaCenter;
  state.area_center = {center.vx, center.vy, center.vz};
  if (action.previewCount > std::size(action.previews)) {
    throw std::runtime_error("encounter target previews exceed storage");
  }
  for (unsigned index = 0; index < action.previewCount; ++index) {
    const auto& target = action.previews[index];
    state.target_previews.push_back(
        {target.actorId,
         target.limb,
         {target.position.vx, target.position.vy, target.position.vz}});
  }
}

void LearnAction(unsigned action) {
  constexpr unsigned kActionCount = 256;
  constexpr unsigned kBitsPerByte = 8;
  if (action == 0 || action >= kActionCount) {
    throw std::runtime_error("encounter profile references an invalid action");
  }
  const unsigned index = action / kBitsPerByte;
  const auto bits = static_cast<std::uint8_t>(
      static_cast<unsigned char>(State_learnedActionFlags[index]) |
      (0x80u >> (action % kBitsPerByte)));
  State_learnedActionFlags[index] = std::bit_cast<char>(bits);
  VS_LOG_INFO("encounter.profile", "learned_action={}", action);
}

unsigned EquippedBreakArt() {
  constexpr unsigned kWeaponFamilies = 9;
  constexpr unsigned kArtsPerWeapon = 4;
  for (const auto& weapon : Audio_Seq_unusedInventory.weapons) {
    if (!weapon.isEquipped) continue;
    if (weapon.blade == 0 ||
        weapon.blade > std::size(Audio_Seq_unusedInventory.blades)) {
      throw std::runtime_error("encounter profile has no valid equipped blade");
    }
    const int category = static_cast<unsigned char>(
        Audio_Seq_unusedInventory.blades[weapon.blade - 1].category);
    if (category < 1 || static_cast<unsigned>(category) > kWeaponFamilies) {
      throw std::runtime_error(
          "encounter profile has an invalid weapon family");
    }
    return Battle_ActionId_DaggerArtOne +
           static_cast<unsigned>(category - 1) * kArtsPerWeapon;
  }
  return Battle_ActionId_DaggerArtOne + kWeaponFamilies * kArtsPerWeapon;
}

void StockEncounterItems(EncounterProfile profile) {
  // Named misc IDs from ITEMNAME.BIN.yaml, checked against the original
  // MAINMENU/2D10.c misc-action map. Exclude equipment and unnamed holes.
  constexpr auto kItems = std::to_array<u_short>(
      {323, 324, 325, 326, 327, 328, 329, 330, 331, 332, 333, 334, 335,
       336, 337, 338, 339, 340, 341, 342, 343, 344, 345, 346, 347, 348,
       349, 350, 351, 352, 353, 354, 355, 356, 386, 387, 388, 389, 390,
       391, 392, 393, 394, 395, 399, 403, 407, 411, 415, 419, 423, 424,
       425, 426, 427, 428, 429, 430, 431, 432, 433, 434, 435, 436, 437,
       438, 439, 440, 441, 442, 443, 444, 445, 446, 447, 448, 449, 450,
       451, 452, 453, 454, 455, 456, 458, 459, 460, 461, 462, 463, 464,
       465, 466, 467, 468, 469, 470, 471, 472, 473, 474, 475, 476, 477,
       478, 479, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489});
  constexpr std::size_t kHalf = kItems.size() / 2;
  constexpr char kStack = 10;
  auto& items = Audio_Seq_unusedInventory.misc;
  auto& indices = Audio_Seq_unusedInventoryIndices.misc;
  static_assert(kItems.size() % 2 == 0 && kHalf <= std::size(items));
  std::ranges::fill(items, Items_InventoryMisc{});
  std::ranges::fill(indices, 0);
  const auto first = profile == EncounterProfile::kAllItemsA ? 0 : kHalf;
  for (std::size_t slot = 0; slot < kHalf; ++slot) {
    const auto index = static_cast<char>(slot + 1);
    items[slot] = {kItems[first + slot], kStack, index};
    indices[slot] = static_cast<u_char>(index);
  }
  VS_LOG_INFO("encounter.profile", "misc_stacks={} count_each={}", kHalf,
              static_cast<int>(kStack));
}

struct AdjustmentDefinition {
  const char* name;
  int status_parameter;
};

// Status parameter indices from BATTLE/146C.c _setApplicableStatuses callers.
constexpr std::array<AdjustmentDefinition, 13> kAdjustments = {
    {{"HP -50 (MIN 1)", -1},
     {"MP -25 (MIN 0)", -1},
     {"RISK +25 (MAX 100)", -1},
     {"POISON", 14},
     {"PARALYSIS", 13},
     {"NUMBNESS", 15},
     {"CURSE", 16},
     {"SILENCE", 12},
     {"STRENGTH DOWN", 5},
     {"INTELLIGENCE DOWN", 7},
     {"AGILITY DOWN", 9},
     {"EQUIPMENT DOWN", 19},
     {"RESTORE MP TO CURRENT MAXIMUM", -1}},
};
static_assert(kAdjustments.size() ==
              static_cast<std::size_t>(EncounterAdjustment::kCount));

std::expected<void, std::string> ApplyAdjustment(EncounterAdjustment adjustment,
                                                 Battle_ActorStats& stats) {
  constexpr int kHpStep = 50, kMpStep = 25, kRiskStep = 25, kMaxRisk = 100;
  switch (adjustment) {
    case EncounterAdjustment::kLowerHp:
      stats.currentHp =
          static_cast<std::int16_t>(std::max(1, stats.currentHp - kHpStep));
      return {};
    case EncounterAdjustment::kLowerMp:
      stats.currentMp =
          static_cast<std::int16_t>(std::max(0, stats.currentMp - kMpStep));
      return {};
    case EncounterAdjustment::kRestoreMp:
      stats.currentMp = stats.maxMp;
      return {};
    case EncounterAdjustment::kRaiseRisk:
      stats.risk =
          static_cast<u_short>(std::min(kMaxRisk, stats.risk + kRiskStep));
      return {};
    default:
      break;
  }
  const auto parameter =
      kAdjustments[static_cast<std::size_t>(adjustment)].status_parameter;
  if (Battle_statusEffectParams[parameter].statusFlags == 0) {
    return std::unexpected("STATUS DEFINITIONS NOT LOADED");
  }
  Battle_HitResult hit{};
  hit.target.targetActor = 0;
  if (Battle_QueueApplicableStatusChanges(parameter, &hit) == 0) {
    return std::unexpected("STATUS ALREADY ACTIVE, BLOCKED OR IMMUNE");
  }
  // These original hit-result names are inverted: inflictStatuses cures;
  // removeStatuses applies. Use the same order as production hit resolution.
  if (hit.inflictStatuses != 0) {
    Battle_RemoveActorStatuses(hit.inflictStatuses, &stats);
  }
  if (hit.removeStatuses != 0) {
    Battle_InflictActorStatuses(hit.removeStatuses, &stats);
  }
  return {};
}
void LearnProfileBattleAbilities(bool all_abilities) {
  constexpr unsigned kMappedAbilities = 3;
  // Original BATTLE.PRG 5BF94.c:3657 scans 14 entries in each ability list.
  constexpr unsigned kAbilityCount = 14;
  constexpr unsigned kFirstChainAbility = 22;
  constexpr unsigned kFirstDefenseAbility = 40;
  constexpr unsigned kAfterLastDefenseAbility = 54;
  const unsigned count = all_abilities ? kAbilityCount : kMappedAbilities;
  for (unsigned slot = 0; slot < count; ++slot) {
    const auto chain =
        static_cast<unsigned char>(Battle_chainAbilityActionIds[slot]);
    const auto defense =
        static_cast<unsigned char>(Battle_defenseAbilityActionIds[slot]);
    if (chain < kFirstChainAbility || chain >= kFirstDefenseAbility ||
        defense < kFirstDefenseAbility || defense >= kAfterLastDefenseAbility) {
      throw std::runtime_error(
          "encounter profile ability table is unavailable");
    }
    LearnAction(chain);
    LearnAction(defense);
    if (slot < kMappedAbilities) {
      Input_settings.mappedChainAbilities[slot] = static_cast<char>(chain);
      Input_settings.mappedDefenseAbilities[slot] = static_cast<char>(defense);
    }
  }
}

void LearnProfileSpells(bool all_spells) {
  for (unsigned spell_class = 0;
       spell_class < std::size(Battle_spellClassCounts); ++spell_class) {
    if (Battle_spellClassCounts[spell_class] == 0 ||
        Battle_spellClassSpellIds[spell_class] == nullptr) {
      throw std::runtime_error("encounter profile spell table is unavailable");
    }
    const unsigned count =
        all_spells ? Battle_spellClassCounts[spell_class] : 1;
    for (unsigned index = 0; index < count; ++index) {
      const unsigned action = Battle_spellClassSpellIds[spell_class][index];
      LearnAction(action);
      // MAINMENU/2D10.c and MENU0/84.c use four consecutive action IDs per
      // rank.
      constexpr unsigned kSpellRanks = 4;
      if (all_spells && action >= Battle_WarlockSpell_Explosion &&
          action <= Battle_WarlockSpell_Meteor &&
          (action - Battle_WarlockSpell_Explosion) % kSpellRanks == 0) {
        for (unsigned rank = 1; rank < kSpellRanks; ++rank) {
          LearnAction(action + rank);
        }
      }
    }
  }
  Battle_ApplyLearnedActionUnlocks();
}
}  // namespace

const char* EncounterProfileId(EncounterProfile profile) {
  switch (profile) {
    case EncounterProfile::kPrologue:
      return "prologue";
    case EncounterProfile::kPostPrologue:
      return "post-prologue";
    case EncounterProfile::kPostMinotaur:
      return "post-minotaur";
    case EncounterProfile::kAllItemsA:
      return "all-items-a";
    case EncounterProfile::kAllItemsB:
      return "all-items-b";
    case EncounterProfile::kAllSpells:
      return "all-spells";
    case EncounterProfile::kAllAbilities:
      return "all-abilities";
    case EncounterProfile::kAllBreakArts:
      return "all-break-arts";
    case EncounterProfile::kCount:
      break;
  }
  throw std::runtime_error("invalid encounter Ashley profile");
}

void InitializeEncounterPlayerProfile(EncounterProfile profile) {
  (void)EncounterProfileId(profile);
  if (profile == EncounterProfile::kPrologue) return;
  // Lab-only startup state. Original 5BF94.c:3618 enables Map/Score at 3.
  constexpr int kPostPrologueStage = 3;
  constexpr unsigned kPostPrologueMenus = 0x120;
  Audio_Seq_unusedStateFlags.introState = kPostPrologueStage;
  Input_settings.menuFlags |= kPostPrologueMenus;
  if (profile == EncounterProfile::kAllItemsA ||
      profile == EncounterProfile::kAllItemsB) {
    StockEncounterItems(profile);
  }
  const bool all_spells = profile == EncounterProfile::kAllSpells;
  const bool all_abilities = profile == EncounterProfile::kAllAbilities;
  const bool all_break_arts = profile == EncounterProfile::kAllBreakArts;
  if (all_break_arts) {
    // Original MENU1/30.c: ten categories, four consecutive arts each.
    constexpr unsigned kBreakArtCount = 40;
    for (unsigned index = 0; index < kBreakArtCount; ++index) {
      const unsigned action = Battle_ActionId_DaggerArtOne + index;
      if (Battle_actionDefinitions[action].type != Battle_ActionType_BreakArt) {
        throw std::runtime_error(
            "encounter profile requires valid Break Art definitions");
      }
      LearnAction(action);
    }
  }
  if (profile == EncounterProfile::kPostMinotaur || all_abilities) {
    LearnProfileBattleAbilities(all_abilities);
    if (!all_abilities) LearnAction(EquippedBreakArt());
  }
  if (profile == EncounterProfile::kPostMinotaur || all_spells) {
    LearnProfileSpells(all_spells);
  } else if (all_abilities || all_break_arts) {
    Battle_ApplyLearnedActionUnlocks();
  }
  VS_LOG_INFO("encounter.profile",
              "Ashley={} representative_test_actions={} all_spells={} "
              "all_abilities={} all_break_arts={}",
              EncounterProfileId(profile),
              profile == EncounterProfile::kPostMinotaur, all_spells,
              all_abilities, all_break_arts);
}

const char* EncounterWeaponId(EncounterWeapon weapon) {
  constexpr auto kIds = std::to_array<const char*>({
      "default",
      "dagger",
      "sword",
      "greatsword",
      "axe-mace",
      "greataxe",
      "staff",
      "heavy-mace",
      "polearm",
      "crossbow",
      "bare-hands",
  });
  static_assert(kIds.size() ==
                static_cast<std::size_t>(EncounterWeapon::kCount));
  if (weapon < EncounterWeapon::kDefault || weapon >= EncounterWeapon::kCount) {
    throw std::runtime_error("invalid encounter Ashley weapon");
  }
  return kIds[static_cast<std::size_t>(weapon)];
}

void InitializeEncounterPlayerWeapon(EncounterWeapon choice) {
  (void)EncounterWeaponId(choice);
  if (choice == EncounterWeapon::kDefault) return;
  auto& inventory = Audio_Seq_unusedInventory;
  if (choice == EncounterWeapon::kBareHands) {
    for (auto& weapon : inventory.weapons) weapon.isEquipped = 0;
    return;
  }
  auto source = ReadEncounterWeapon(choice);
  if (!source) throw std::runtime_error(source.error());
  const auto equipped = [](const auto& weapon) {
    return weapon.isEquipped != 0;
  };
  if (std::ranges::count_if(inventory.weapons, equipped) != 1) {
    throw std::runtime_error(
        "weapon setup requires one New Game equipped weapon");
  }
  auto& weapon = *std::ranges::find_if(inventory.weapons, equipped);
  if (weapon.blade == 0 || weapon.blade > std::size(inventory.blades) ||
      weapon.grip == 0 || weapon.grip > std::size(inventory.grips) ||
      std::ranges::any_of(weapon.gems, [](auto gem) { return gem != 0; })) {
    throw std::runtime_error(
        "weapon setup requires valid gem-free stock parts");
  }
  // Original BATTLE/146C.c applyWeaponStats/copyUiWeaponStats preserve the
  // stock inventory links; ordinary Ashley spawning loads stats/models later.
  Items_WeaponLoadout stats{};
  Items_BuildWeaponStatsFromParts(&stats, &*source);
  Items_StoreInventoryWeaponStats(&weapon, &stats);
  // Match MAINMENU/2D10.c's weapon equip restriction (staves are one-handed).
  if (Menu_Framework_weaponHands[static_cast<int>(choice) - 1] != 0) {
    for (auto& shield : inventory.shields) shield.isEquipped = 0;
  }
}

std::expected<void, std::string> ConfigureEncounter(const EncounterSetup& setup,
                                                    const EncounterZone& zone) {
  if (active) return std::unexpected("an encounter is already active");
  configured = false;
  const auto errors = ValidateEncounter(setup, zone);
  if (!errors.empty()) {
    std::ostringstream message;
    for (const auto& error : errors) message << error << "; ";
    return std::unexpected(message.str());
  }
  if (setup.external_event) {
    const auto* store = Content::DataStore::Get();
    const auto path = std::format("EVENT/{:04}.EVT", *setup.external_event);
    const auto* entry = store ? store->Find(path) : nullptr;
    constexpr unsigned kSectorsPerEvent =
        VS_0000_EVT_SIZE / Content::DataStore::kDataSectorBytes;
    if (entry == nullptr || entry->form != "raw" ||
        entry->size != VS_0000_EVT_SIZE ||
        entry->lba !=
            VS_0000_EVT_LBA + static_cast<unsigned>(*setup.external_event) *
                                  kSectorsPerEvent) {
      return std::unexpected(
          "External event is unavailable or has an invalid "
          "original disc layout: " +
          path);
    }
    const auto bytes = store->ReadAll(*entry);
    if (!bytes) return std::unexpected(bytes.error());
  }
  if (setup.weapon >= EncounterWeapon::kDagger &&
      setup.weapon <= EncounterWeapon::kCrossbow) {
    const auto weapon = ReadEncounterWeapon(setup.weapon);
    if (!weapon) return std::unexpected(weapon.error());
  }
  std::array<Field_MpdEnemySpawn, placements.size()> staged_placements{};
  for (std::size_t index = 0; index < setup.enemies.size(); ++index) {
    const auto& spawn = setup.enemies[index];
    const auto definition =
        std::ranges::find(zone.enemies, spawn.enemy, &EncounterEnemy::index);
    const auto source = ReadEncounterSpawnProfile(*definition);
    if (!source) return std::unexpected(source.error());
    const auto placement = PrepareEncounterSpawn(*source, spawn, index);
    if (!placement) return std::unexpected(placement.error());
    staged_placements[index] = *placement;
    VS_LOG_DEBUG(
        "encounter.spawn",
        "instance={} definition={} source={} row={} "
        "source_retains_after_defeat={} isolated_retains_after_defeat={}",
        index, spawn.enemy, definition->spawn_asset, definition->spawn_index,
        source->actorStateByte != 0, placement->actorStateByte != 0);
  }
  configuration = setup;
  placements = staged_placements;
  stop_requested = false;
  entered = false;
  room_prepared = false;
  player_stats_initialized = false;
  external_event_started = false;
  pending_door = -1;
  configured = true;
  return {};
}

void InitializeEncounterPlayerStats(const EncounterStatOverrides& overrides) {
  if (!overrides.Valid()) {
    throw std::runtime_error("Ashley stat overrides must be in 1..999");
  }
  if (overrides == EncounterStatOverrides{}) return;
  auto* actor = Battle_actors[0];
  if (actor == nullptr || actor->stats == nullptr) {
    throw std::runtime_error("Ashley is unavailable for encounter stat setup");
  }
  auto& stats = *actor->stats;
  if (overrides.max_hp) {
    stats.currentHp = stats.maxHp =
        static_cast<std::int16_t>(*overrides.max_hp);
  }
  if (overrides.max_mp) {
    stats.currentMp = stats.maxMp =
        static_cast<std::int16_t>(*overrides.max_mp);
  }
  if (overrides.strength) {
    stats.strength = stats.totalStrength =
        static_cast<u_short>(*overrides.strength);
  }
  if (overrides.intelligence) {
    stats.intelligence = stats.totalIntelligence =
        static_cast<u_short>(*overrides.intelligence);
  }
  if (overrides.agility) {
    stats.agility = stats.totalAgility =
        static_cast<u_short>(*overrides.agility);
  }
  VS_LOG_INFO("encounter.stats",
              "starting HP={}/{} MP={}/{} STR={} INT={} AGI={}",
              stats.currentHp, stats.maxHp, stats.currentMp, stats.maxMp,
              stats.strength, stats.intelligence, stats.agility);
}

void RunEncounter() {
  if (!configured || active) {
    VS_LOG_ERROR("encounter", "invalid encounter lifecycle entry");
    return;
  }
  active = true;
  Boot_InitializePlatformServices();
  Boot_RunSessionLoop(EnterOrFinishEncounter, ResumeEncounter);
  Battle_ResetTargetPages();
  active = false;
  external_event_started = false;
  pending_door = -1;
  gameplay_inspection_ready = false;
  configured = false;
  room_prepared = false;
  VS_LOG_INFO("encounter", "session finished; no normal title entry invoked");
}

void RequestEncounterStop() { stop_requested = true; }

const char* EncounterAdjustmentName(EncounterAdjustment adjustment) {
  return kAdjustments.at(static_cast<std::size_t>(adjustment)).name;
}

std::expected<EncounterAdjustmentResult, std::string> AdjustEncounterPlayer(
    EncounterAdjustment adjustment) {
  const auto reject = [](const char* reason)
      -> std::expected<EncounterAdjustmentResult, std::string> {
    VS_LOG_WARN("encounter.adjust", "rejected: {}", reason);
    return std::unexpected(reason);
  };
  if (static_cast<std::size_t>(adjustment) >= kAdjustments.size()) {
    return reject("UNKNOWN LAB ADJUSTMENT");
  }
  if (!active || !room_prepared || stop_requested.load()) {
    return reject("NO READY COMBAT LAB ENCOUNTER");
  }
  constexpr int kFieldControl = 1;
  if (Field_controlMode != kFieldControl || Battle_actionFlowMode != 0 ||
      Menu_Framework_menuState.currentMenu !=
          Menu_Framework_MenuState_NoneClosed) {
    return reject("RETURN TO IDLE FIELD BEFORE ADJUSTING ASHLEY");
  }
  auto* actor = Battle_actors[0];
  if (actor == nullptr || actor->stats == nullptr || actor->defeated ||
      actor->stats->currentHp <= 0 || Audio_Seq_unusedStateFlags.gameOver) {
    return reject("ASHLEY IS NOT ALIVE AND READY");
  }
  EncounterAdjustmentResult result{InspectActor(0, *actor), {}};
  if (const auto applied = ApplyAdjustment(adjustment, *actor->stats);
      !applied) {
    return reject(applied.error().c_str());
  }
  result.after = InspectActor(0, *actor);
  VS_LOG_INFO("encounter.adjust", "applied: {}",
              EncounterAdjustmentName(adjustment));
  return result;
}

EncounterState InspectEncounter() {
  if (!active || !room_prepared) return {};
  return InspectGameplay();
}

EncounterState InspectGameplay() {
  EncounterState state;
  state.ready = gameplay_inspection_ready && Boot_isGameplayActive != 0 &&
                Field_controlMode != 0;
  if (!state.ready) return state;
  state.zone = Field_zoneContext.zndId;
  state.room = Field_zoneContext.mapId;
  state.control_mode = Field_controlMode;
  state.game_over = Audio_Seq_unusedStateFlags.gameOver != 0;
  state.field_rendering_enabled = Field_isFieldRenderingEnabled != 0;
  state.menu_state =
      static_cast<unsigned char>(Menu_Framework_menuState.currentMenu);
  if (state.menu_state != Menu_Framework_MenuState_NoneClosed &&
      Menu_Framework_itemsText != nullptr) {
    state.menu_highlighted_row = Menu_Framework_HighlightedRow();
  }
  state.roulette = InspectRoulette(state.menu_state);
  state.frame_buffer = Render_frameBuf;
  state.transition = Field_screenTransitionStep;
  state.native_buttons = Input_buttonsState;
  state.native_pressed = Input_buttonsPressed.allButtons;
  state.action_phase = Battle_actionFlowMode;
  for (unsigned action = 0; action < std::size(Battle_actionDefinitions);
       ++action) {
    if (Battle_actionDefinitions[action].unlocked) {
      state.learned_actions.push_back(static_cast<int>(action));
    }
  }
  constexpr int kLootFlag = 0xAB;
  constexpr int kLootAvailable = 3;
  state.rewards_available = State_ScriptFlag(kLootFlag) >= kLootAvailable;
  if (Battle_actionState != nullptr) {
    InspectTargeting(state, *Battle_actionState);
  }
  for (int slot = 0; slot < EncounterLimits::kActorSlots; ++slot) {
    const auto* actor = Battle_actors[slot];
    if (actor == nullptr || actor->stats == nullptr) continue;
    state.actors.push_back(InspectActor(slot, *actor));
  }
  for (std::size_t slot = 0; slot < std::size(Audio_Seq_unusedInventory.misc);
       ++slot) {
    const auto& item = Audio_Seq_unusedInventory.misc[slot];
    if (item.id != 0) {
      state.items.push_back({static_cast<int>(slot), item.id, item.count});
    }
  }
  for (const auto* node = Items_lootListHead; node != nullptr;
       node = node->next) {
    if (state.pending_loot.size() >= EncounterLimits::kActorSlots) {
      throw std::runtime_error("encounter loot list exceeds actor storage");
    }
    EncounterLootState loot;
    loot.source_actor = node->droppedMask;
    for (std::size_t slot = 0; slot < std::size(node->loot.misc); ++slot) {
      const auto& item = node->loot.misc[slot];
      if (item.itemId != 0 && item.count != 0) {
        loot.items.push_back({static_cast<int>(slot), item.itemId, item.count});
      }
    }
    state.pending_loot.push_back(std::move(loot));
  }
  if (Field_roomData.floorTriggers != nullptr) {
    const auto count = Field_roomData.header.floorTriggersLength /
                       sizeof(*Field_roomData.floorTriggers);
    for (std::size_t i = 0; i < count; ++i) {
      const auto& trigger = Field_roomData.floorTriggers[i];
      state.floor_triggers.push_back(
          {trigger.kind, trigger.tileX, trigger.tileZ, trigger.activation,
           static_cast<unsigned char>(trigger.markerFade)});
    }
  }
  return state;
}

namespace {
nlohmann::json InspectThreads() {
  auto threads = nlohmann::json::array();
  const auto base = reinterpret_cast<std::uintptr_t>(Event_scriptBuffer);
  for (std::size_t index = 0; index < std::size(Event_threadWaitFrames);
       ++index) {
    const auto pc =
        reinterpret_cast<std::uintptr_t>(Event_threadCursors[index]);
    nlohmann::json thread{{"thread", index}, {"pc", pc}};
    if (base != 0 && pc >= base) thread["script_base_delta"] = pc - base;
    thread["wait_frames"] = Event_threadWaitFrames[index];
    thread["dialogue_wait"] = Event_isTextBoxAwaitingClose[index];
    threads.push_back(std::move(thread));
  }
  return threads;
}

nlohmann::json InspectModels() {
  auto models = nlohmann::json::array();
  for (std::size_t slot = 0; slot < std::size(Actors_actorRecords); ++slot) {
    const auto* actor = Actors_actorRecords[slot];
    if (actor == nullptr) continue;
    models.push_back(
        {{"slot", slot},
         {"model", actor->modelId},
         {"bones", actor->instance.boneCount},
         {"position",
          {actor->instance.position.vx, actor->instance.position.vy,
           actor->instance.position.vz}},
         {"facing", actor->instance.facing},
         {"animation", actor->animationId},
         {"animation_set", actor->animationSet},
         {"frame", actor->pose.frame},
         {"length", actor->animationLength},
         {"queued_animation", actor->queuedAnimation},
         {"holding_last_frame", actor->pose.holdingLastFrame != 0},
         {"animation_locked", actor->animationLocked != 0},
         {"attached", actor->instance.isAttached != 0},
         {"attachment_parent",
          static_cast<unsigned>(actor->instance.attachParent)},
         {"attachment_bone", static_cast<unsigned>(actor->instance.attachBone)},
         {"appearance",
          static_cast<unsigned>(actor->instance.actionAnimationState)}});
  }
  return models;
}
}  // namespace

nlohmann::json InspectGameplayScene(const EncounterState& state) {
  const bool available = state.ready && gameplay_inspection_ready &&
                         Boot_isGameplayActive != 0 && Field_controlMode != 0;
  nlohmann::json scene{{"available", available},
                       {"in_game", Boot_isGameplayActive != 0}};
  if (!available) return scene;
  const auto& camera = Field_sphericalCamera;
  scene["id"] = Field_zoneContext.currentSceneId;
  scene["event"] = {
      {"id", Event_eventId},
      {"source", Event_eventSource},
      {"phase", Event_playbackPhase},
      {"script_buffer", reinterpret_cast<std::uintptr_t>(Event_scriptBuffer)},
      {"skip_state", Event_skipState},
      {"forced_skip", Event_isSkipForced != 0},
      {"main_thread_finished", Event_isMainThreadFinished != 0},
      {"threads", InspectThreads()}};
  auto waits = nlohmann::json::array();
  // The legacy declaration spans 80 bytes; the event VM owns eight boxes.
  constexpr std::size_t kScriptTextBoxes = 8;
  for (std::size_t box = 0; box < kScriptTextBoxes; ++box) {
    waits.push_back(Event_isTextBoxAwaitingClose[box] != 0);
  }
  scene["event"]["dialogue_waits"] = std::move(waits);
  scene["camera"] = {{"yaw", camera.values.yaw},
                     {"pitch", camera.values.pitch},
                     {"distance", camera.values.distance},
                     {"mode", camera.values.mode},
                     {"delta_yaw", camera.delta.yaw},
                     {"delta_pitch", camera.delta.pitch},
                     {"delta_distance", camera.delta.distance}};
  const auto& view = Render_frameScratch.viewMatrix;
  scene["camera"]["view_rotation"] = {
      {view.m[0][0], view.m[0][1], view.m[0][2]},
      {view.m[1][0], view.m[1][1], view.m[1][2]},
      {view.m[2][0], view.m[2][1], view.m[2][2]}};
  scene["camera"]["view_translation"] = {view.t[0], view.t[1], view.t[2]};
  scene["effects"] = {{"busy", Effects_effectBusyFlags},
                      {"phase", Effects_primaryPhase},
                      {"tracks", Effects_trackCount},
                      {"started_tracks", Effects_startedTrackCount},
                      {"pending_tracks", Effects_pendingTracks}};
  scene["models"] = InspectModels();
  return scene;
}

namespace {
EncounterDoorRequest CheckDoorRequest(int index) {
  if (!active ||
      configuration.room_policy != EncounterRoomPolicy::kOriginalRoom) {
    return {EncounterDoorRequestStatus::kInvalid,
            "original-room session required"};
  }
  if (!gameplay_inspection_ready || !room_prepared || Field_controlMode != 1 ||
      Event_playbackPhase != 0 || Field_ScreenTransitionPhase() != 0 ||
      Actors_actorRecords[0] == nullptr) {
    return {EncounterDoorRequestStatus::kBusy,
            "source event/transition still active"};
  }
  if (Field_roomData.header.doorsLength % sizeof(Field_MpdDoorRecord) != 0 ||
      Field_roomData.doors == nullptr || index < 0 ||
      static_cast<unsigned>(index) >=
          Field_roomData.header.doorsLength / sizeof(Field_MpdDoorRecord)) {
    return {EncounterDoorRequestStatus::kInvalid,
            "door index/storage does not match inventory"};
  }
  auto& door = Field_roomData.doors[index];
  const auto value = static_cast<unsigned char>(
      State_ScriptFlag(static_cast<std::int16_t>(door.requiredFlagId)));
  if (value < door.requiredFlagValue) {
    return {EncounterDoorRequestStatus::kBlocked,
            std::format("flag {} is {} but requires {}",
                        static_cast<unsigned>(door.requiredFlagId), value,
                        static_cast<unsigned>(door.requiredFlagValue))};
  }
  return {EncounterDoorRequestStatus::kAccepted, {}};
}

void ApplyPendingDoorRequest() {
  if (pending_door < 0 || !gameplay_inspection_ready || !room_prepared) return;
  if (Field_zoneContext.zndId != pending_door_zone ||
      Field_zoneContext.mapId != pending_door_room) {
    VS_LOG_ERROR("field.door-test",
                 "source changed before queued door {} executed", pending_door);
    pending_door = -1;
    return;
  }
  const auto checked = CheckDoorRequest(pending_door);
  if (checked.status == EncounterDoorRequestStatus::kBusy) return;
  const int index = std::exchange(pending_door, -1);
  if (checked.status != EncounterDoorRequestStatus::kAccepted) {
    VS_LOG_WARN("field.door-test", "queued door {} rejected: {}", index,
                checked.reason);
    return;
  }
  auto& door = Field_roomData.doors[index];
  VS_LOG_INFO("field.door-test", "begin source={}/{} door={} destination={}/{}",
              pending_door_zone, pending_door_room, index,
              door.destinationZoneId, door.destinationRoomId);
  // Same handoff as Field_CheckActorTriggers (146C.c), after its door gate.
  constexpr int kTransitionSpeed = 8;
  if (door.destinationZoneId == Field_zoneContext.zndId) {
    Field_BeginRoomTransition(&door, 0, kTransitionSpeed);
  } else {
    Field_BeginZoneTransition(&door, 0, kTransitionSpeed);
  }
  Actors_ResetActorRunMomentum(0);
  Field_SetEntranceDoorRecord(reinterpret_cast<char (*)[sizeof(door)]>(&door));
}
}  // namespace

EncounterDoorRequest RequestEncounterDoorTransition(int index) {
  if (pending_door >= 0) {
    return {EncounterDoorRequestStatus::kBusy, "door request already pending"};
  }
  const auto checked = CheckDoorRequest(index);
  if (checked.status != EncounterDoorRequestStatus::kAccepted) return checked;
  pending_door = index;
  pending_door_zone = Field_zoneContext.zndId;
  pending_door_room = Field_zoneContext.mapId;
  return {EncounterDoorRequestStatus::kAccepted, {}};
}
}  // namespace VS::Game

extern "C" void Boot_EncounterRoomPlacements(void** data, unsigned int* size) {
  if (!VS::Game::active) return;
  VS::Game::room_prepared = false;
  if (!VS::Game::UsesConfiguredEncounterPlacements(
          VS::Game::configuration, Field_zoneContext.zndId,
          Field_zoneContext.mapId, VS::Game::external_event_started)) {
    VS_LOG_DEBUG("encounter.spawn", "using original placements zone={} room={}",
                 Field_zoneContext.zndId, Field_zoneContext.mapId);
    return;
  }
  *data = VS::Game::placements.data();
  *size = static_cast<unsigned int>(VS::Game::configuration.enemies.size() *
                                    sizeof(Field_MpdEnemySpawn));
  VS_LOG_DEBUG("encounter.spawn", "installing {} room placements",
               VS::Game::configuration.enemies.size());
}

extern "C" int Boot_EncounterUsesRoomEvents() {
  return !VS::Game::active ||
         VS::Game::UsesEncounterRoomEvents(VS::Game::configuration,
                                           VS::Game::external_event_started);
}

extern "C" int Boot_EncounterLoadRoomEvents() {
  // _mpdClearedSection is a terminated offset table, not a nullable list.
  // Provenance: BATTLE/4A0A8.c:4302, func_800BEC58.
  constexpr std::size_t kTriggerSlots = 16;
  constexpr std::uint16_t kEndOfTriggers = 0xFFFF;
  std::array<std::uint16_t, kTriggerSlots> empty_triggers;
  empty_triggers.fill(kEndOfTriggers);
  return Event_LoadRoomEventSections(
      empty_triggers.data(), sizeof(empty_triggers), nullptr, 0, nullptr, 0, 0);
}

extern "C" void Boot_EncounterPollStop() {
  VS::Game::gameplay_inspection_ready = true;
  if (VS::Game::active) {
    if (!VS::Game::player_stats_initialized) {
      VS::Game::InitializeEncounterPlayerStats(
          VS::Game::configuration.stat_overrides);
      VS::Game::player_stats_initialized = true;
    }

    VS::Game::room_prepared = true;
    if (VS::Game::stop_requested.load()) {
      VS::Game::pending_door = -1;
      Boot_ResetToTitle();
    } else {
      VS::Game::ApplyPendingDoorRequest();
      if (VS::Game::configuration.external_event &&
          !VS::Game::external_event_started && Event_playbackPhase == 0 &&
          Field_controlMode == 1) {
        const int event = *VS::Game::configuration.external_event;
        // Original 4A0A8.c:4302 direct-event request; no story flag mutation.
        const int result = Event_EvaluateRoomEventTriggers(
            VS::Game::kDirectEventTrigger, event, nullptr, 0);
        if (result != 1) {
          Core_Errorf("encounter.event",
                      "Cannot queue external event %d: result=%d", event,
                      result);
          std::abort();
        }
        VS::Game::external_event_started = true;
        VS_LOG_INFO("encounter.event",
                    "Starting external event {} in zone {} room {}; "
                    "component fixture, not story progression",
                    event, Field_zoneContext.zndId, Field_zoneContext.mapId);
        Event_BeginQueuedCutscene();
      }
    }
  }
}

extern "C" void Boot_ResetGameplayInspection() {
  VS::Game::gameplay_inspection_ready = false;
}
