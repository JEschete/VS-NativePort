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

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Location.hpp"
#include "Actors/ShpModel.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/SoundState.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionMenu.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Damage.hpp"
#include "Battle/EncounterHooks.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/Sphere.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Battle/TargetPages.hpp"
#include "Battle/TargetVolume.hpp"
#include "Boot/GameLoop.hpp"
#include "Boot/PlayerSettings.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscLayout.h"
#include "Core/Diagnostics.hpp"
#include "Core/Log.hpp"
#include "Core/RandomLive.hpp"
#include "Diagnostics/GameplayTrace.hpp"
#include "Effects/Runtime.hpp"
#include "Ending/ClearResults.hpp"
#include "Ending/Credits/StaffRoll.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Event/RoomTriggerEntryLive.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Event/Triggers.hpp"
#include "Event/TriggersLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/ChestInteractionsLive.hpp"
#include "Field/DoorRecordLive.hpp"
#include "Field/EncounterSpawnProfile.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Interactions.hpp"
#include "Field/Mpd.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Field/TransitionsLiveState.hpp"
#include "Field/Zone.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/LootLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Results/Results.hpp"
#include "Menu/Results/TimeAttack.hpp"
#include "Render/Display.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Location.hpp"
#include "State/Progress.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/StateFlags.hpp"
#include "Title/TitleMenuLive.hpp"

using VS::Battle::ActionState;
using VS::Battle::kActionIdDaggerArtOne;
using VS::Battle::kActionTypeBreakArt;
using VS::Battle::kWarlockSpellExplosion;
using VS::Battle::kWarlockSpellMeteor;

namespace VS::Game {
namespace {
EncounterSetup configuration;
std::array<VS::Field::MpdEnemySpawn, EncounterLimits::kEnemies> placements{};
std::atomic<bool> stopRequested{false};
bool configured = false;
bool active = false;
bool entered = false;
bool roomPrepared = false;
bool gameplayInspectionReady = false;
bool playerStatsInitialized = false;
bool externalEventStarted = false;
bool roomLocalEventStarted = false;

std::optional<EncounterRouletteState> InspectRoulette(int menu) {
  constexpr int kResultsMenu = 15;
  if (menu != kResultsMenu || VS::State::stateFlags.menu_f_submenu != 0) {
    return std::nullopt;
  }
  return EncounterRouletteState{
      .entry_phase = VS::Menu::Framework::submenuStates[kResultsMenu],
      .screen_state = VS::Menu::Results::screenState,
      .timer = VS::Menu::Results::screenTimer,
      .animation = VS::Menu::Results::elementAnimationState,
      .reel = VS::Menu::Results::buffReelIndex,
      .selection = VS::Menu::Results::buffReelSelection,
      .bios_state = VS::Core::BiosRand().State()};
}
constexpr int kDirectEventTrigger = 0x40;
int pendingDoor = -1;
int pendingDoorZone = 0;
int pendingDoorRoom = 0;

int ResumeEncounter() {
  VS::Boot::isGameplayActive = 1;
  VS::Boot::RunGameplayLoopLive();
  return 0;
}

int EnterOrFinishEncounter() {
  if (entered) return 0;
  entered = true;
  // Reuse _sysReinit and _initGameData rather than manufacturing player stats.
  // Provenance: SLUS main.c:8309; TITLE/22C.c:4698.
  VS::Boot::InitializeTitleSessionLive();
  VS::Boot::InitializePlayerSettings();
  VS::Title::InitializeLiveNewGameData();
  // VS::Title::Run follows _initGameData with _initEnvironment (TITLE.PRG
  // 22C.c:4463, VS::Title::InitializeSettings), whose state writes are
  // applied here without its publisher screens or audio setup.
  VS::State::stateFlags.difficulty = 1;
  VS::State::stateFlags.puzzle_mode = 1;
  VS::State::savedEntryDoor.destination_zone_id = configuration.zone;
  VS::State::savedEntryDoor.destination_room_id = configuration.room;
  VS::State::savedEntryDoor.entry_tile_x = configuration.player.x;
  VS::State::savedEntryDoor.entry_tile_z = configuration.player.z;
  VS::State::savedEntryDoor.entry_facing = configuration.player.facing;
  VS::State::savedEntryDoor.tile_location.entry_surface =
      configuration.player.surface;
  VS::Boot::gameEntryMode = 0;
  RestoreBootEventSelector();
  if (configuration.attract_demo) {
    // VS::Title::Run's idle timeout: _setTitleExitFlags (TITLE.PRG 22C.c:4741)
    // sets the demo flags and door; its cause becomes vs_battle_exec's entry
    // mode (SLUS main.c:8344), which returns to title (146C.c:7558).
    VS::Title::PrepareLiveExit(*configuration.attract_demo);
    VS::Boot::gameEntryMode = *configuration.attract_demo;
  }
  VS::Boot::isClearSavePending = 0;
  VS::Boot::LoadBattleOverlaysLive();
  InitializeEncounterPlayerWeapon(configuration.weapon);
  InitializeEncounterPlayerProfile(configuration.profile);
  if (configuration.story_progress) {
    // Script flag 0x82 is the campaign progress counter tested by MPD enemy
    // placements (VS::Field::SpawnRoomActors, BATTLE.PRG func_80089114,
    // 146C.c:14901). Declared Lab fixture state, written before any room
    // loads; never changed during play.
    constexpr std::int16_t kStoryProgressFlag = 0x82;
    VS::State::SetLiveScriptFlag(
        kStoryProgressFlag, static_cast<char>(*configuration.story_progress));
    VS::Core::LogInfo("encounter", "declared story progress={}",
                      *configuration.story_progress);
  }
  if (configuration.clear_count) {
    // New Game+ starts from a cleared save, whose stateFlags block (including
    // clearCount, incremented by the ending at ENDING.PRG _clearCount) is
    // restored wholesale. Room scripts test it as script flag 0, e.g. the
    // MAP029 entry's jumpFwdIfFlag 0 >= 1. Declared Lab fixture state, written
    // before any room loads; never changed during play.
    VS::State::stateFlags.clear_count =
        static_cast<char>(*configuration.clear_count);
    VS::Core::LogInfo("encounter", "declared clear count={}",
                      *configuration.clear_count);
  }
  for (const int flag : configuration.crossed_puzzles) {
    // Persistent puzzle time-trial unlock flags (e.g. MAP241 entry sets 223
    // when flag 286 == 2, after a MAP291 pass). Declared Lab fixture state,
    // written before any room loads; never changed during play.
    VS::State::SetLiveScriptFlag(static_cast<std::int16_t>(flag), 1);
    VS::Core::LogInfo("encounter", "declared crossed puzzle flag={}", flag);
  }
  if (configuration.inherited_music) {
    // Zones whose ZND music id is 0 keep the wave bank an earlier zone or
    // event left loaded (_finishLoadZnd, BATTLE.PRG 146C.c:14826). Load the
    // declared predecessor's mapped bank with the original
    // vs_main_loadAndWaitSoundSlot (SLUS main.c:10439); no music slot is
    // loaded or played.
    VS::Audio::LiveLoadWaveBankBlocking(*configuration.inherited_music);
    VS::Core::LogInfo(
        "encounter", "declared inherited music={} wave bank={}",
        *configuration.inherited_music,
        static_cast<int>(
            VS::Audio::kWaveBankFileMap[*configuration.inherited_music]));
  }
  // The original shuffle initialization always seeds with 1. Initialize that
  // same table from the requested seed; combat still calls its real RNG.
  VS::Core::SeedBiosRand(configuration.seed);
  for (auto& value : VS::Core::randomShuffleTable) {
    value = VS::Core::NextBiosRand();
  }
  VS::Core::randomShuffleIndex =
      static_cast<int>(std::size(VS::Core::randomShuffleTable)) - 1;
  VS::Core::interfaceRandomState = configuration.seed;
  VS::Core::LogInfo("encounter", "starting zone={} room={} enemies={} seed={}",
                    configuration.zone, configuration.room,
                    configuration.enemies.size(), configuration.seed);
  return ResumeEncounter();
}

EncounterActorState InspectActor(int slot, const VS::Battle::Actor& actor) {
  const auto& stats = *actor.stats;
  EncounterActorState snapshot;
  snapshot.slot = slot;
  snapshot.hp = stats.current_hp;
  snapshot.mp = stats.current_mp;
  snapshot.max_hp = stats.max_hp;
  snapshot.max_mp = stats.max_mp;
  snapshot.risk = stats.risk;
  snapshot.strength = stats.strength;
  snapshot.intelligence = stats.intelligence;
  snapshot.agility = stats.agility;
  snapshot.total_strength = stats.total_strength;
  snapshot.total_intelligence = stats.total_intelligence;
  snapshot.total_agility = stats.total_agility;
  snapshot.walk_speed = static_cast<unsigned char>(stats.walk_speed);
  snapshot.run_speed = static_cast<unsigned char>(stats.run_speed);
  snapshot.statuses = static_cast<std::uint32_t>(stats.statuses);
  std::ranges::copy(stats.status_timers, snapshot.status_timers.begin());
  snapshot.command = actor.command_opcode;
  snapshot.command_state = actor.command_state;
  snapshot.weapon_drawn = actor.weapon_drawn;
  snapshot.weapon_category = actor.equipped_weapon_category;
  snapshot.weapon_blade = stats.weapon.blade.id;
  snapshot.weapon_grip = stats.weapon.grip.id;
  snapshot.weapon_model =
      static_cast<unsigned char>(stats.weapon.weapon_model_id);
  snapshot.shield_model =
      static_cast<unsigned char>(stats.shield.weapon_model_id);
  snapshot.actor_flags = VS::Actors::LiveActorStateFlags(slot);
  snapshot.recovery = stats.action_recovery_timer;
  VS::Actors::ActorLocationQuery location{};
  VS::Actors::LiveActorPlacement(slot, &location);
  snapshot.x = location.location.position.vx;
  snapshot.y = location.location.position.vy;
  snapshot.z = location.location.position.vz;
  snapshot.spawn_index = actor.room_spawn_index;
  snapshot.defeated = actor.defeated != 0;
  snapshot.awareness_marker =
      static_cast<unsigned char>(VS::Battle::actorMarkers[slot]);
  const auto* ai = VS::Battle::Ai::actorAiStates[slot];
  if (ai != nullptr) {
    snapshot.ai_present = true;
    snapshot.ai_candidates = ai->candidate_count;
    snapshot.ai_engagement_frames = ai->engagement_frames;
    snapshot.ai_movement_mode = ai->requested_movement_mode;
    snapshot.ai_destination_x = ai->destination.tile.tile_x;
    snapshot.ai_destination_z = ai->destination.tile.tile_z;
    if (ai->candidate_count > 0 && ai->candidate_order[0] != nullptr) {
      snapshot.ai_target = ai->candidate_order[0]->target_id;
    }
  }
  return snapshot;
}

void InspectTargeting(EncounterState& state, const ActionState& action) {
  state.action = action.selection.action_id;
  state.source_actor = action.selection.primary_target.target.target_actor;
  state.target_count = action.selection.target_count;
  const auto page = VS::Battle::GetTargetPageInfo();
  state.target_page = {page.total, page.page,    page.pages,
                       page.first, page.visible, page.selected};
  constexpr unsigned kTargetingControl = 2;
  constexpr unsigned kFirstTargetPhase = 1, kLastTargetPhase = 3;
  if (state.control_mode != kTargetingControl ||
      state.action_phase < kFirstTargetPhase ||
      state.action_phase > kLastTargetPhase) {
    return;
  }
  const auto& center = action.area_center;
  state.area_center = {center.vx, center.vy, center.vz};
  if (action.preview_count > std::size(action.previews)) {
    throw std::runtime_error("encounter target previews exceed storage");
  }
  // BATTLE.PRG func_800C4794 positioning mode (sphere +0x3E): D-pad moves the
  // area cursor; a second Circle confirms it.
  const auto* sphere = reinterpret_cast<const VS::Battle::TargetSphereRecord*>(
      VS::Battle::targetSphere);
  state.area_positioning = sphere != nullptr && sphere->cursor_locked != 0;
  if (sphere != nullptr) {
    constexpr int kExtentUnits = 32;
    for (const auto& volume : sphere->volumes) {
      state.area_volumes.push_back(
          {volume.kind,
           volume.scale.extents[0] * kExtentUnits,
           volume.scale.extents[2] * kExtentUnits,
           volume.angle,
           {volume.position[0], volume.position[1], volume.position[2]}});
    }
  }
  for (unsigned index = 0; index < action.preview_count; ++index) {
    const auto& target = action.previews[index];
    state.target_previews.push_back(
        {target.actor_id,
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
      static_cast<unsigned char>(VS::State::learnedActionFlags[index]) |
      (0x80u >> (action % kBitsPerByte)));
  VS::State::learnedActionFlags[index] = std::bit_cast<char>(bits);
  VS::Core::LogInfo("encounter.profile", "learned_action={}", action);
}

unsigned EquippedBreakArt() {
  constexpr unsigned kWeaponFamilies = 9;
  constexpr unsigned kArtsPerWeapon = 4;
  for (const auto& weapon : VS::Items::inventory.weapons) {
    if (!weapon.is_equipped) continue;
    if (weapon.blade == 0 ||
        weapon.blade > std::size(VS::Items::inventory.blades)) {
      throw std::runtime_error("encounter profile has no valid equipped blade");
    }
    const int category = static_cast<unsigned char>(
        VS::Items::inventory.blades[weapon.blade - 1].category);
    if (category < 1 || static_cast<unsigned>(category) > kWeaponFamilies) {
      throw std::runtime_error(
          "encounter profile has an invalid weapon family");
    }
    return kActionIdDaggerArtOne +
           static_cast<unsigned>(category - 1) * kArtsPerWeapon;
  }
  return kActionIdDaggerArtOne + kWeaponFamilies * kArtsPerWeapon;
}

void StockEncounterItems(EncounterProfile profile) {
  // Named misc IDs from ITEMNAME.BIN.yaml, checked against the original
  // MAINMENU/2D10.c misc-action map. Exclude equipment and unnamed holes.
  constexpr auto kItems = std::to_array<std::uint16_t>(
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
  constexpr std::uint8_t kStack = 10;
  auto& items = VS::Items::inventory.misc;
  auto& indices = VS::Items::inventoryIndices.misc;
  static_assert(kItems.size() % 2 == 0 && kHalf <= std::size(items));
  std::ranges::fill(items, VS::Items::InventoryMisc{});
  std::ranges::fill(indices, 0);
  const auto first = profile == EncounterProfile::kAllItemsA ? 0 : kHalf;
  for (std::size_t slot = 0; slot < kHalf; ++slot) {
    const auto index = static_cast<std::uint8_t>(slot + 1);
    items[slot] = {kItems[first + slot], kStack, index};
    indices[slot] = index;
  }
  VS::Core::LogInfo("encounter.profile", "misc_stacks={} count_each={}", kHalf,
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

std::expected<void, std::string> ApplyAdjustment(
    EncounterAdjustment adjustment, VS::Battle::ActorStats& stats) {
  constexpr int kHpStep = 50, kMpStep = 25, kRiskStep = 25, kMaxRisk = 100;
  switch (adjustment) {
    case EncounterAdjustment::kLowerHp:
      stats.current_hp =
          static_cast<std::int16_t>(std::max(1, stats.current_hp - kHpStep));
      return {};
    case EncounterAdjustment::kLowerMp:
      stats.current_mp =
          static_cast<std::int16_t>(std::max(0, stats.current_mp - kMpStep));
      return {};
    case EncounterAdjustment::kRestoreMp:
      stats.current_mp = stats.max_mp;
      return {};
    case EncounterAdjustment::kRaiseRisk:
      stats.risk = static_cast<std::uint16_t>(
          std::min(kMaxRisk, stats.risk + kRiskStep));
      return {};
    default:
      break;
  }
  const auto parameter =
      kAdjustments[static_cast<std::size_t>(adjustment)].status_parameter;
  if (VS::Battle::statusEffectParams[parameter].status_flags == 0) {
    return std::unexpected("STATUS DEFINITIONS NOT LOADED");
  }
  VS::Battle::HitResult hit{};
  hit.target.target_actor = 0;
  if (VS::Battle::QueueLiveApplicableStatusChanges(parameter, &hit) == 0) {
    return std::unexpected("STATUS ALREADY ACTIVE, BLOCKED OR IMMUNE");
  }
  // These original hit-result names are inverted: inflict_statuses cures;
  // remove_statuses applies. Use the same order as production hit resolution.
  if (hit.inflict_statuses != 0) {
    VS::Battle::RemoveLiveActorStatuses(hit.inflict_statuses, &stats);
  }
  if (hit.remove_statuses != 0) {
    VS::Battle::InflictLiveActorStatuses(hit.remove_statuses, &stats);
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
        static_cast<unsigned char>(VS::Battle::chainAbilityActionIds[slot]);
    const auto defense =
        static_cast<unsigned char>(VS::Battle::defenseAbilityActionIds[slot]);
    if (chain < kFirstChainAbility || chain >= kFirstDefenseAbility ||
        defense < kFirstDefenseAbility || defense >= kAfterLastDefenseAbility) {
      throw std::runtime_error(
          "encounter profile ability table is unavailable");
    }
    LearnAction(chain);
    LearnAction(defense);
    if (slot < kMappedAbilities) {
      VS::Input::settings.mapped_chain_abilities[slot] =
          static_cast<char>(chain);
      VS::Input::settings.mapped_defense_abilities[slot] =
          static_cast<char>(defense);
    }
  }
}

void LearnProfileSpells(bool all_spells) {
  for (unsigned spell_class = 0;
       spell_class < std::size(VS::Battle::spellClassCounts); ++spell_class) {
    if (VS::Battle::spellClassCounts[spell_class] == 0 ||
        VS::Battle::spellClassSpellIds[spell_class] == nullptr) {
      throw std::runtime_error("encounter profile spell table is unavailable");
    }
    const unsigned count =
        all_spells ? VS::Battle::spellClassCounts[spell_class] : 1;
    for (unsigned index = 0; index < count; ++index) {
      const unsigned action =
          VS::Battle::spellClassSpellIds[spell_class][index];
      LearnAction(action);
      // MAINMENU/2D10.c and MENU0/84.c use four consecutive action IDs per
      // rank.
      constexpr unsigned kSpellRanks = 4;
      if (all_spells && action >= kWarlockSpellExplosion &&
          action <= kWarlockSpellMeteor &&
          (action - kWarlockSpellExplosion) % kSpellRanks == 0) {
        for (unsigned rank = 1; rank < kSpellRanks; ++rank) {
          LearnAction(action + rank);
        }
      }
    }
  }
  Battle::ApplyLiveLearnedActionUnlocks();
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

void RestoreBootEventSelector() {
  // Room-entry triggers (Event_EvaluateRoomEventTriggers type 1, BATTLE.PRG
  // func_800BEC58, 4A0A8.c:4302) select source 1 without writing the event
  // id, so a fresh boot leaves its BSS zero. Restore that per Lab script
  // instead of carrying the previous script's last event id into the next
  // scene label.
  VS::Event::eventId = 0;
}

void InitializeEncounterPlayerProfile(EncounterProfile profile) {
  (void)EncounterProfileId(profile);
  if (profile == EncounterProfile::kPrologue) return;
  // Lab-only startup state. Original 5BF94.c:3618 enables Map/Score at 3.
  constexpr int kPostPrologueStage = 3;
  constexpr unsigned kPostPrologueMenus = 0x120;
  VS::State::stateFlags.intro_state = kPostPrologueStage;
  VS::Input::settings.menu_flags |= kPostPrologueMenus;
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
      const unsigned action = kActionIdDaggerArtOne + index;
      if (VS::Battle::actionDefinitions[action].type != kActionTypeBreakArt) {
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
    Battle::ApplyLiveLearnedActionUnlocks();
  }
  VS::Core::LogInfo("encounter.profile",
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
  auto& inventory = VS::Items::inventory;
  if (choice == EncounterWeapon::kBareHands) {
    for (auto& weapon : inventory.weapons) weapon.is_equipped = 0;
    return;
  }
  auto source = ReadEncounterWeapon(choice);
  if (!source) throw std::runtime_error(source.error());
  const auto equipped = [](const auto& weapon) {
    return weapon.is_equipped != 0;
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
  VS::Items::WeaponLoadout stats{};
  VS::Items::BuildLiveWeaponStatsFromParts(&stats, &*source);
  VS::Items::StoreLiveInventoryWeaponStats(&weapon, &stats);
  // Match MAINMENU/2D10.c's weapon equip restriction (staves are one-handed).
  if (VS::Menu::Framework::weaponHands[static_cast<int>(choice) - 1] != 0) {
    for (auto& shield : inventory.shields) shield.is_equipped = 0;
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
  std::array<VS::Field::MpdEnemySpawn, placements.size()> staged_placements{};
  for (std::size_t index = 0; index < setup.enemies.size(); ++index) {
    const auto& spawn = setup.enemies[index];
    const auto definition =
        std::ranges::find(zone.enemies, spawn.enemy, &EncounterEnemy::index);
    const auto source = ReadEncounterSpawnProfile(*definition);
    if (!source) return std::unexpected(source.error());
    const auto placement = PrepareEncounterSpawn(*source, spawn, index);
    if (!placement) return std::unexpected(placement.error());
    staged_placements[index] = *placement;
    VS::Core::LogDebug(
        "encounter.spawn",
        "instance={} definition={} source={} row={} "
        "source_retains_after_defeat={} isolated_retains_after_defeat={}",
        index, spawn.enemy, definition->spawn_asset, definition->spawn_index,
        source->retain_after_defeat != 0, placement->retain_after_defeat != 0);
  }
  configuration = setup;
  placements = staged_placements;
  stopRequested = false;
  entered = false;
  roomPrepared = false;
  playerStatsInitialized = false;
  externalEventStarted = false;
  roomLocalEventStarted = false;
  pendingDoor = -1;
  configured = true;
  return {};
}

void InitializeEncounterPlayerStats(const EncounterStatOverrides& overrides) {
  if (!overrides.Valid()) {
    throw std::runtime_error("Ashley stat overrides must be in 1..999");
  }
  if (overrides == EncounterStatOverrides{}) return;
  auto* actor = VS::Battle::actors[0];
  if (actor == nullptr || actor->stats == nullptr) {
    throw std::runtime_error("Ashley is unavailable for encounter stat setup");
  }
  auto& stats = *actor->stats;
  if (overrides.max_hp) {
    stats.current_hp = stats.max_hp =
        static_cast<std::int16_t>(*overrides.max_hp);
  }
  if (overrides.max_mp) {
    stats.current_mp = stats.max_mp =
        static_cast<std::int16_t>(*overrides.max_mp);
  }
  if (overrides.strength) {
    stats.strength = stats.total_strength =
        static_cast<std::uint16_t>(*overrides.strength);
  }
  if (overrides.intelligence) {
    stats.intelligence = stats.total_intelligence =
        static_cast<std::uint16_t>(*overrides.intelligence);
  }
  if (overrides.agility) {
    stats.agility = stats.total_agility =
        static_cast<std::uint16_t>(*overrides.agility);
  }
  VS::Core::LogInfo(
      "encounter.stats", "starting HP={}/{} MP={}/{} STR={} INT={} AGI={}",
      stats.current_hp, stats.max_hp, stats.current_mp, stats.max_mp,
      stats.strength, stats.intelligence, stats.agility);
}

void RunEncounter() {
  if (!configured || active) {
    VS::Core::LogError("encounter", "invalid encounter lifecycle entry");
    return;
  }
  active = true;
  VS::Boot::InitializePlatformServicesLive();
  VS::Boot::RunLiveSessionLoop(EnterOrFinishEncounter, ResumeEncounter);
  VS::Battle::ResetTargetPages();
  active = false;
  externalEventStarted = false;
  roomLocalEventStarted = false;
  pendingDoor = -1;
  gameplayInspectionReady = false;
  configured = false;
  roomPrepared = false;
  VS::Core::LogInfo("encounter",
                    "session finished; no normal title entry invoked");
}

void RequestEncounterStop() { stopRequested = true; }

const char* EncounterAdjustmentName(EncounterAdjustment adjustment) {
  return kAdjustments.at(static_cast<std::size_t>(adjustment)).name;
}

std::expected<EncounterAdjustmentResult, std::string> AdjustEncounterPlayer(
    EncounterAdjustment adjustment) {
  const auto reject = [](const char* reason)
      -> std::expected<EncounterAdjustmentResult, std::string> {
    VS::Core::LogWarn("encounter.adjust", "rejected: {}", reason);
    return std::unexpected(reason);
  };
  if (static_cast<std::size_t>(adjustment) >= kAdjustments.size()) {
    return reject("UNKNOWN LAB ADJUSTMENT");
  }
  if (!active || !roomPrepared || stopRequested.load()) {
    return reject("NO READY COMBAT LAB ENCOUNTER");
  }
  constexpr int kFieldControl = 1;
  if (VS::Field::controlMode != kFieldControl ||
      VS::Battle::actionFlowMode != 0 ||
      VS::Menu::Framework::menuState.control.current_menu !=
          VS::Menu::Framework::kMenuClosed) {
    return reject("RETURN TO IDLE FIELD BEFORE ADJUSTING ASHLEY");
  }
  auto* actor = VS::Battle::actors[0];
  if (actor == nullptr || actor->stats == nullptr || actor->defeated ||
      actor->stats->current_hp <= 0 || VS::State::stateFlags.game_over) {
    return reject("ASHLEY IS NOT ALIVE AND READY");
  }
  EncounterAdjustmentResult result{InspectActor(0, *actor), {}};
  if (const auto applied = ApplyAdjustment(adjustment, *actor->stats);
      !applied) {
    return reject(applied.error().c_str());
  }
  result.after = InspectActor(0, *actor);
  VS::Core::LogInfo("encounter.adjust", "applied: {}",
                    EncounterAdjustmentName(adjustment));
  return result;
}

EncounterState InspectEncounter() {
  if (!active || !roomPrepared) return {};
  return InspectGameplay();
}

namespace {
void InspectLearnedActions(EncounterState& state) {
  for (unsigned action = 0; action < std::size(VS::Battle::actionDefinitions);
       ++action) {
    if (VS::Battle::actionDefinitions[action].unlocked) {
      state.learned_actions.push_back(static_cast<int>(action));
    }
  }
}

void InspectInventory(EncounterState& state) {
  for (std::size_t slot = 0; slot < std::size(VS::Items::inventory.misc);
       ++slot) {
    const auto& item = VS::Items::inventory.misc[slot];
    if (item.id != 0) {
      state.items.push_back({static_cast<int>(slot), item.id, item.count});
    }
  }
}

void InspectPendingLoot(EncounterState& state) {
  for (const auto* node = VS::Items::lootListHead; node != nullptr;
       node = node->next) {
    if (state.pending_loot.size() >= EncounterLimits::kActorSlots) {
      throw std::runtime_error("encounter loot list exceeds actor storage");
    }
    EncounterLootState loot;
    loot.source_actor = node->actor_id;
    for (std::size_t slot = 0; slot < std::size(node->loot.misc); ++slot) {
      const auto& item = node->loot.misc[slot];
      if (item.item_id != 0 && item.count != 0) {
        loot.items.push_back(
            {static_cast<int>(slot), item.item_id, item.count});
      }
    }
    state.pending_loot.push_back(std::move(loot));
  }
}

void InspectFloorTriggers(EncounterState& state) {
  if (VS::Field::roomData.floor_triggers == nullptr) return;
  const auto count = VS::Field::roomData.header.floor_triggers_length /
                     sizeof(*VS::Field::roomData.floor_triggers);
  for (std::size_t i = 0; i < count; ++i) {
    const auto& trigger = VS::Field::roomData.floor_triggers[i];
    state.floor_triggers.push_back(
        {trigger.kind, trigger.tile_x, trigger.tile_z, trigger.activation,
         static_cast<unsigned char>(trigger.marker_fade)});
  }
}
}  // namespace

EncounterState InspectGameplay() {
  EncounterState state;
  state.ready = gameplayInspectionReady && VS::Boot::isGameplayActive != 0 &&
                VS::Field::controlMode != 0;
  if (!state.ready) return state;
  state.zone = VS::Field::zoneContext.zone_id;
  state.room = VS::Field::zoneContext.room_id;
  state.control_mode = VS::Field::controlMode;
  state.game_over = VS::State::stateFlags.game_over != 0;
  state.field_rendering_enabled =
      VS::Field::LiveRoomRenderState().field_rendering_enabled;
  state.menu_state = VS::Menu::Framework::menuState.control.current_menu;
  if (state.menu_state != VS::Menu::Framework::kMenuClosed &&
      VS::Menu::Framework::itemsText != nullptr) {
    state.menu_highlighted_row = VS::Menu::Framework::LiveHighlightedRow();
  }
  state.roulette = InspectRoulette(state.menu_state);
  if (constexpr int kResultsMenu = 15; state.menu_state == kResultsMenu) {
    state.results_screen =
        static_cast<unsigned char>(VS::State::stateFlags.menu_f_submenu);
  }
  state.frame_buffer = VS::Render::frameBuf;
  state.transition = VS::Field::screenTransitionStep;
  state.native_buttons = VS::Input::buttonsState;
  state.native_pressed = VS::Input::buttonsPressed;
  state.action_phase = VS::Battle::actionFlowMode;
  InspectLearnedActions(state);
  constexpr int kLootFlag = 0xAB;
  constexpr int kLootAvailable = 3;
  state.rewards_available =
      VS::State::LiveScriptFlag(kLootFlag) >= kLootAvailable;
  if (VS::Battle::actionState != nullptr) {
    InspectTargeting(state, *VS::Battle::actionState);
  }
  for (int slot = 0; slot < EncounterLimits::kActorSlots; ++slot) {
    const auto* actor = VS::Battle::actors[slot];
    if (actor == nullptr || actor->stats == nullptr) continue;
    state.actors.push_back(InspectActor(slot, *actor));
  }
  InspectInventory(state);
  InspectPendingLoot(state);
  InspectFloorTriggers(state);
  return state;
}

namespace {
nlohmann::json InspectThreads() {
  auto threads = nlohmann::json::array();
  const auto script =
      Event::LeaseLiveScriptSource(Event::ScriptRegion::kEventScript);
  const auto base =
      script ? reinterpret_cast<std::uintptr_t>(script->Bytes().data()) : 0;
  for (std::size_t index = 0; index < std::size(VS::Event::threadWaitFrames);
       ++index) {
    const auto cursor =
        Event::LiveScriptVm().ThreadCursor(static_cast<int>(index));
    std::uintptr_t pc = 0;
    if (cursor.Present()) {
      const auto source = Event::LeaseLiveScriptSource(cursor.region);
      const auto bytes = source ? source->View(cursor) : std::nullopt;
      if (bytes) {
        pc = reinterpret_cast<std::uintptr_t>(bytes->data());
      }
    }
    nlohmann::json thread{{"thread", index}, {"pc", pc}};
    if (base != 0 && pc >= base) thread["script_base_delta"] = pc - base;
    thread["wait_frames"] = VS::Event::threadWaitFrames[index];
    thread["dialogue_wait"] = VS::Event::isTextBoxAwaitingClose[index];
    threads.push_back(std::move(thread));
  }
  return threads;
}

nlohmann::json InspectModels() {
  auto models = nlohmann::json::array();
  for (std::size_t slot = 0; slot < std::size(VS::Actors::actorRecords);
       ++slot) {
    const auto* actor = VS::Actors::actorRecords[slot];
    if (actor == nullptr) continue;
    models.push_back(
        {{"slot", slot},
         {"model", actor->model_id},
         {"bones", actor->instance.bone_count},
         {"position",
          {actor->instance.position.vx, actor->instance.position.vy,
           actor->instance.position.vz}},
         {"facing", actor->instance.facing},
         {"animation", actor->animation_id},
         {"animation_set", actor->animation_set},
         {"frame", actor->pose.frame},
         {"length", actor->animation_length},
         {"queued_animation", actor->queued_animation},
         {"holding_last_frame", actor->pose.holding_last_frame != 0},
         {"animation_locked", actor->animation_locked != 0},
         {"attached", actor->instance.is_attached != 0},
         {"attachment_parent",
          static_cast<unsigned>(actor->instance.attach_parent)},
         {"attachment_bone",
          static_cast<unsigned>(actor->instance.attach_bone)},
         {"appearance",
          static_cast<unsigned>(actor->instance.action_animation_state)}});
  }
  return models;
}
}  // namespace

nlohmann::json InspectGameplayScene(const EncounterState& state) {
  const bool available = state.ready && gameplayInspectionReady &&
                         VS::Boot::isGameplayActive != 0 &&
                         VS::Field::controlMode != 0;
  nlohmann::json scene{{"available", available},
                       {"in_game", VS::Boot::isGameplayActive != 0}};
  // ENDING.PRG staff roll (Start skips it) and _renderCongratulationsScreen,
  // which holds until any pressed button once the credits have finished.
  scene["ending"] = {
      {"credits_finished", VS::Ending::Credits::hasFinished != 0},
      {"credits_skip_requested", VS::Ending::Credits::wasSkipRequested != 0},
      {"results_frame", VS::Ending::resultsAnimationFrame},
      {"results_leaving", VS::Ending::isLeavingResults != 0}};
  if (!available) return scene;
  const auto& camera = VS::Field::sphericalCamera;
  const auto script =
      Event::LeaseLiveScriptSource(Event::ScriptRegion::kEventScript);
  scene["id"] = VS::Field::zoneContext.scene_id;
  scene["event"] = {
      {"id", VS::Event::eventId},
      {"source", VS::Event::eventSource},
      {"phase", VS::Event::playbackPhase},
      {"script_buffer",
       script ? reinterpret_cast<std::uintptr_t>(script->Bytes().data()) : 0},
      {"skip_state", VS::Event::skipState},
      {"forced_skip", VS::Event::isSkipForced != 0},
      {"main_thread_finished", VS::Event::isMainThreadFinished != 0},
      {"threads", InspectThreads()}};
  auto waits = nlohmann::json::array();
  for (const std::uint8_t wait : VS::Event::isTextBoxAwaitingClose) {
    waits.push_back(wait != 0);
  }
  scene["event"]["dialogue_waits"] = std::move(waits);
  // func_800B7398 (event opcode 0x1C) holds the script until a face button
  // confirms the highlighted text-box choice.
  scene["event"]["choice_active"] = VS::Event::isTextSelectionActive != 0;
  scene["camera"] = {{"yaw", camera.values.yaw},
                     {"pitch", camera.values.pitch},
                     {"distance", camera.values.distance},
                     {"mode", camera.values.mode},
                     {"delta_yaw", camera.motion.turn},
                     {"delta_pitch", camera.motion.target_yaw},
                     {"delta_distance", camera.motion.turn_speed}};
  const auto& view = VS::Render::FrameScratchpad().view_matrix;
  scene["camera"]["view_rotation"] = {
      {view.m[0][0], view.m[0][1], view.m[0][2]},
      {view.m[1][0], view.m[1][1], view.m[1][2]},
      {view.m[2][0], view.m[2][1], view.m[2][2]}};
  scene["camera"]["view_translation"] = {view.t[0], view.t[1], view.t[2]};
  scene["effects"] = {{"busy", VS::Effects::effectBusyFlags},
                      {"phase", VS::Effects::primaryPhase},
                      {"tracks", VS::Effects::trackCount},
                      {"started_tracks", VS::Effects::startedTrackCount},
                      {"pending_tracks", VS::Effects::pendingTracks}};
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
  if (!gameplayInspectionReady || !roomPrepared ||
      VS::Field::controlMode != 1 || VS::Event::playbackPhase != 0 ||
      VS::Field::LiveScreenTransitionPhase() != 0 ||
      VS::Actors::actorRecords[0] == nullptr) {
    return {EncounterDoorRequestStatus::kBusy,
            "source event/transition still active"};
  }
  if (VS::Field::roomData.header.doors_length %
              sizeof(VS::Field::MpdDoorRecord) !=
          0 ||
      VS::Field::roomData.doors == nullptr || index < 0 ||
      static_cast<unsigned>(index) >= VS::Field::roomData.header.doors_length /
                                          sizeof(VS::Field::MpdDoorRecord)) {
    return {EncounterDoorRequestStatus::kInvalid,
            "door index/storage does not match inventory"};
  }
  auto& door = VS::Field::roomData.doors[index];
  const auto value = static_cast<unsigned char>(VS::State::LiveScriptFlag(
      static_cast<std::int16_t>(door.required_flag_id)));
  if (value < door.required_flag_value) {
    return {EncounterDoorRequestStatus::kBlocked,
            std::format("flag {} is {} but requires {}",
                        static_cast<unsigned>(door.required_flag_id), value,
                        static_cast<unsigned>(door.required_flag_value))};
  }
  return {EncounterDoorRequestStatus::kAccepted, {}};
}

void ApplyPendingDoorRequest() {
  if (pendingDoor < 0 || !gameplayInspectionReady || !roomPrepared) return;
  if (VS::Field::zoneContext.zone_id != pendingDoorZone ||
      VS::Field::zoneContext.room_id != pendingDoorRoom) {
    VS::Core::LogError("field.door-test",
                       "source changed before queued door {} executed",
                       pendingDoor);
    pendingDoor = -1;
    return;
  }
  const auto checked = CheckDoorRequest(pendingDoor);
  if (checked.status == EncounterDoorRequestStatus::kBusy) return;
  const int index = std::exchange(pendingDoor, -1);
  if (checked.status != EncounterDoorRequestStatus::kAccepted) {
    VS::Core::LogWarn("field.door-test", "queued door {} rejected: {}", index,
                      checked.reason);
    return;
  }
  auto& door = VS::Field::roomData.doors[index];
  VS::Core::LogInfo("field.door-test",
                    "begin source={}/{} door={} destination={}/{}",
                    pendingDoorZone, pendingDoorRoom, index,
                    door.destination_zone_id, door.destination_room_id);
  // Same handoff as Field_CheckActorTriggers (146C.c), after its door gate.
  constexpr int kTransitionSpeed = 8;
  if (door.destination_zone_id == VS::Field::zoneContext.zone_id) {
    VS::Field::LiveBeginRoomTransition(&door, 0, kTransitionSpeed);
  } else {
    VS::Field::LiveBeginZoneTransition(&door, 0, kTransitionSpeed);
  }
  VS::Actors::ResetLiveActorRunMomentum(0);
  VS::Field::SetLiveEntranceDoor(VS::Field::DoorRecordOf(door));
}
}  // namespace

EncounterDoorRequest RequestEncounterDoorTransition(int index) {
  if (pendingDoor >= 0) {
    return {EncounterDoorRequestStatus::kBusy, "door request already pending"};
  }
  const auto checked = CheckDoorRequest(index);
  if (checked.status != EncounterDoorRequestStatus::kAccepted) return checked;
  pendingDoor = index;
  pendingDoorZone = VS::Field::zoneContext.zone_id;
  pendingDoorRoom = VS::Field::zoneContext.room_id;
  return {EncounterDoorRequestStatus::kAccepted, {}};
}
}  // namespace VS::Game

void VS::Boot::EncounterRoomPlacements(void** data, unsigned int* size) {
  if (!VS::Game::active) return;
  VS::Game::roomPrepared = false;
  if (!VS::Game::UsesConfiguredEncounterPlacements(
          VS::Game::configuration, VS::Field::zoneContext.zone_id,
          VS::Field::zoneContext.room_id, VS::Game::externalEventStarted)) {
    VS::Core::LogDebug(
        "encounter.spawn", "using original placements zone={} room={}",
        VS::Field::zoneContext.zone_id, VS::Field::zoneContext.room_id);
    return;
  }
  *data = VS::Game::placements.data();
  *size = static_cast<unsigned int>(VS::Game::configuration.enemies.size() *
                                    sizeof(VS::Field::MpdEnemySpawn));
  VS::Core::LogDebug("encounter.spawn", "installing {} room placements",
                     VS::Game::configuration.enemies.size());
}

int VS::Boot::EncounterUsesRoomEvents() {
  return !VS::Game::active ||
         VS::Game::UsesEncounterRoomEvents(VS::Game::configuration,
                                           VS::Game::externalEventStarted);
}

VS::Event::RoomEventLoad VS::Game::LoadEncounterRoomEvents() {
  // _mpdClearedSection is a terminated offset table, not a nullable list.
  // Provenance: BATTLE/4A0A8.c:4302, func_800BEC58.
  constexpr std::size_t kTriggerSlots = 16;
  constexpr std::uint16_t kEndOfTriggers = 0xFFFF;
  std::array<std::uint16_t, kTriggerSlots> empty_triggers;
  empty_triggers.fill(kEndOfTriggers);
  return Event::LoadLiveRoomEventSections(
      {.triggers = {
           reinterpret_cast<const std::uint8_t*>(empty_triggers.data()),
           sizeof(empty_triggers)}});
}

void VS::Boot::EncounterPollStop() {
  VS::Game::gameplayInspectionReady = true;
  if (VS::Game::active) {
    if (!VS::Game::playerStatsInitialized) {
      VS::Game::InitializeEncounterPlayerStats(
          VS::Game::configuration.stat_overrides);
      VS::Game::playerStatsInitialized = true;
    }

    VS::Game::roomPrepared = true;
    if (VS::Game::stopRequested.load()) {
      VS::Game::pendingDoor = -1;
      VS::Boot::ResetToTitleLive();
    } else {
      VS::Game::ApplyPendingDoorRequest();
      // External and room-local components start at the first free-control
      // frame in the configured room, after any production entry events.
      const bool configured_room_ready =
          VS::Event::playbackPhase == 0 && VS::Field::controlMode == 1 &&
          VS::Field::zoneContext.zone_id == VS::Game::configuration.zone &&
          VS::Field::zoneContext.room_id == VS::Game::configuration.room;
      if (VS::Game::configuration.external_event &&
          !VS::Game::externalEventStarted && configured_room_ready) {
        const int event = *VS::Game::configuration.external_event;
        // Original 4A0A8.c:4302 direct-event request; no story flag mutation.
        const int result = VS::Event::EvaluateRoomEventTriggersEntry(
            VS::Game::kDirectEventTrigger, event, nullptr, 0);
        if (result != 1) {
          VS::Core::Errorf("encounter.event",
                           "Cannot queue external event %d: result=%d", event,
                           result);
          std::abort();
        }
        VS::Game::externalEventStarted = true;
        VS::Core::LogInfo("encounter.event",
                          "Starting external event {} in zone {} room {}; "
                          "component fixture, not story progression",
                          event, VS::Field::zoneContext.zone_id,
                          VS::Field::zoneContext.room_id);
        VS::Event::BeginLiveQueuedCutscene();
      } else if (VS::Game::configuration.room_local_event &&
                 !VS::Game::roomLocalEventStarted && configured_room_ready) {
        const int selector = *VS::Game::configuration.room_local_event;
        const auto room = VS::Event::LiveRoomScriptSections();
        const auto locals = room ? room->Doors() : VS::Event::ScriptCommand{};
        constexpr std::size_t kDoorOffsetBytes = sizeof(std::int16_t);
        if (selector < 0 ||
            static_cast<std::size_t>(selector) >=
                locals.size() / kDoorOffsetBytes ||
            VS::Event::OperandHalf(locals, static_cast<std::size_t>(selector) *
                                               kDoorOffsetBytes) == 0) {
          VS::Core::Errorf("encounter.event",
                           "Room %d/%d has no local event %d",
                           VS::Field::zoneContext.zone_id,
                           VS::Field::zoneContext.room_id, selector);
          std::abort();
        }
        // Same queue state as a satisfied local room trigger (0x8000 event id)
        // in func_800BEC58 (4A0A8.c:4302); the trigger conditions are skipped.
        VS::Event::eventId = static_cast<int16_t>(selector);
        VS::Event::eventSource = 2;
        VS::Event::playbackPhase = 1;
        VS::Event::isDirectEventRequest = 0;
        VS::Game::roomLocalEventStarted = true;
        VS::Core::LogInfo("encounter.event",
                          "Starting room-local event {} in zone {} room {}; "
                          "trigger conditions not evaluated",
                          selector, VS::Field::zoneContext.zone_id,
                          VS::Field::zoneContext.room_id);
        VS::Event::BeginLiveQueuedCutscene();
      }
    }
  }
}

void VS::Boot::ResetGameplayInspection() {
  VS::Game::gameplayInspectionReady = false;
}
