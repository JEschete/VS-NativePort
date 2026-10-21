#include "Battle/Encounter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Core/Log.hpp"
#include "Field/EncounterSpawnProfile.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
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
#include "Boot/GameLoop.h"
#include "Boot/Lifecycle.h"
#include "Boot/PlayerSettings.h"
#include "Boot/Session.h"
#include "Core/BiosRand.h"
#include "Core/Random.h"
#include "Event/Triggers.h"
#include "Field/GameMode.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoading.h"
#include "Field/RoomRendering.h"
#include "Field/Transitions.h"
#include "Field/Zone.h"
#include "Input/Controller.h"
#include "Items/Loot.h"
#include "Menu/Framework/Framework.h"
#include "Render/Display.h"
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
  Boot_isClearSavePending = 0;
  Boot_LoadBattleOverlays();
  InitializeEncounterPlayerProfile(configuration.profile);
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
  snapshot.risk = stats.risk;
  snapshot.statuses = static_cast<std::uint32_t>(stats.statuses);
  snapshot.command = actor.commandOpcode;
  snapshot.command_state = actor.commandState;
  snapshot.weapon_drawn = actor.weaponDrawn;
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
    snapshot.ai_engagement_frames = ai->opaque8E;
    snapshot.ai_movement_mode = ai->requestedMovementMode;
    snapshot.ai_destination_x = ai->destination.tile.tileX;
    snapshot.ai_destination_z = ai->destination.tile.tileZ;
    if (ai->candidateCount > 0 && ai->candidateOrder[0] != nullptr) {
      snapshot.ai_target = ai->candidateOrder[0]->targetId;
    }
  }
  return snapshot;
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
  throw std::runtime_error("encounter profile requires an equipped weapon");
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
  if (profile == EncounterProfile::kPostMinotaur) {
    constexpr unsigned kMappedAbilities = 3;
    for (unsigned slot = 0; slot < kMappedAbilities; ++slot) {
      const auto chain =
          static_cast<unsigned char>(Battle_chainAbilityActionIds[slot]);
      const auto defense =
          static_cast<unsigned char>(Battle_defenseAbilityActionIds[slot]);
      LearnAction(chain);
      LearnAction(defense);
      Input_settings.mappedChainAbilities[slot] = static_cast<char>(chain);
      Input_settings.mappedDefenseAbilities[slot] = static_cast<char>(defense);
    }
    for (unsigned spell_class = 0;
         spell_class < std::size(Battle_spellClassCounts); ++spell_class) {
      if (Battle_spellClassCounts[spell_class] == 0 ||
          Battle_spellClassSpellIds[spell_class] == nullptr) {
        throw std::runtime_error(
            "encounter profile spell table is unavailable");
      }
      LearnAction(Battle_spellClassSpellIds[spell_class][0]);
    }
    LearnAction(EquippedBreakArt());
    Battle_ApplyLearnedActionUnlocks();
  }
  VS_LOG_INFO("encounter.profile", "Ashley={} representative_test_actions={}",
              EncounterProfileId(profile),
              profile == EncounterProfile::kPostMinotaur);
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
  configured = true;
  return {};
}

void RunEncounter() {
  if (!configured || active) {
    VS_LOG_ERROR("encounter", "invalid encounter lifecycle entry");
    return;
  }
  active = true;
  Boot_InitializePlatformServices();
  Boot_RunSessionLoop(EnterOrFinishEncounter, ResumeEncounter);
  active = false;
  configured = false;
  room_prepared = false;
  VS_LOG_INFO("encounter", "session finished; no normal title entry invoked");
}

void RequestEncounterStop() { stop_requested = true; }

EncounterState InspectEncounter() {
  EncounterState state;
  state.ready = active && room_prepared && Field_controlMode != 0;
  if (!state.ready) return state;
  state.zone = Field_zoneContext.zndId;
  state.room = Field_zoneContext.mapId;
  state.control_mode = Field_controlMode;
  state.game_over = Audio_Seq_unusedStateFlags.gameOver != 0;
  state.field_rendering_enabled = Field_isFieldRenderingEnabled != 0;
  state.menu_state =
      static_cast<unsigned char>(Menu_Framework_menuState.currentMenu);
  state.frame_buffer = Render_frameBuf;
  state.transition = Field_screenTransitionStep;
  state.native_buttons = Input_buttonsState;
  state.native_pressed = Input_buttonsPressed.allButtons;
  state.action_phase = Battle_actionFlowMode;
  constexpr int kLootFlag = 0xAB;
  constexpr int kLootAvailable = 3;
  state.rewards_available = State_ScriptFlag(kLootFlag) >= kLootAvailable;
  if (Battle_actionState != nullptr) {
    state.action = Battle_actionState->selection.actionId;
    state.source_actor = Battle_actionState->sourceActorId;
    state.target_count = Battle_actionState->selection.targetCount;
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
  return state;
}
}  // namespace VS::Game

extern "C" void Boot_EncounterRoomPlacements(void** data, unsigned int* size) {
  if (!VS::Game::active) return;
  *data = VS::Game::placements.data();
  *size = static_cast<unsigned int>(VS::Game::configuration.enemies.size() *
                                    sizeof(Field_MpdEnemySpawn));
  VS::Game::room_prepared = false;
  VS_LOG_DEBUG("encounter.spawn", "installing {} room placements",
               VS::Game::configuration.enemies.size());
}

extern "C" int Boot_EncounterUsesRoomEvents() {
  return VS::Game::active ? 0 : 1;
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
  if (VS::Game::active) {
    VS::Game::room_prepared = true;
    if (VS::Game::stop_requested.load()) Boot_ResetToTitle();
  }
}
