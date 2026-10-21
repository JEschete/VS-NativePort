#include "Battle/Encounter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <ranges>
#include <sstream>
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
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/EncounterHooks.h"
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
#include "Field/Transitions.h"
#include "Field/Zone.h"
#include "Input/Controller.h"
#include "State/Location.h"
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
}  // namespace

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
    VS_LOG_DEBUG("encounter.spawn",
                 "instance={} definition={} source={} row={}", index,
                 spawn.enemy, definition->spawn_asset, definition->spawn_index);
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
  state.transition = Field_screenTransitionStep;
  state.native_buttons = Input_buttonsState;
  state.native_pressed = Input_buttonsPressed.allButtons;
  state.action_phase = Battle_actionFlowMode;
  if (Battle_actionState != nullptr) {
    state.action = Battle_actionState->selection.actionId;
    state.source_actor = Battle_actionState->sourceActorId;
    state.target_count = Battle_actionState->selection.targetCount;
  }
  for (int slot = 0; slot < EncounterLimits::kActorSlots; ++slot) {
    const auto* actor = Battle_actors[slot];
    if (actor == nullptr || actor->stats == nullptr) continue;
    const auto& stats = *actor->stats;
    EncounterActorState snapshot;
    snapshot.slot = slot;
    snapshot.hp = stats.currentHp;
    snapshot.mp = stats.currentMp;
    snapshot.risk = stats.risk;
    snapshot.statuses = static_cast<std::uint32_t>(stats.statuses);
    snapshot.command = actor->commandOpcode;
    snapshot.command_state = actor->commandState;
    snapshot.weapon_drawn = actor->weaponDrawn;
    snapshot.actor_flags = Actors_ActorStateFlags(slot);
    snapshot.recovery = stats.actionRecoveryTimer;
    Actors_ActorLocationQuery location{};
    Actors_ActorPlacement(slot, &location);
    snapshot.x = location.location.position.vx;
    snapshot.y = location.location.position.vy;
    snapshot.z = location.location.position.vz;
    state.actors.push_back(snapshot);
  }
  for (std::size_t slot = 0; slot < std::size(Audio_Seq_unusedInventory.misc);
       ++slot) {
    const auto& item = Audio_Seq_unusedInventory.misc[slot];
    if (item.id != 0) {
      state.items.push_back({static_cast<int>(slot), item.id, item.count});
    }
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
