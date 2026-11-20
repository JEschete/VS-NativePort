// Hand-maintained (tools/port/owned.csv).
#include "Event/EventSceneTransitionLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Core/Log.hpp"
#include "Event/EventSceneTransition.hpp"
#include "Event/ScriptAudio.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptStep.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Field/DoorRecord.hpp"
#include "Field/DoorRecordLive.hpp"
#include "Field/Mpd.h"
#include "Field/TransitionsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "State/ScriptFlagsLive.hpp"

namespace VS::Event {
namespace {
[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS::Core::LogError("event.scene", "{} refused: {}", operation, reason);
  std::abort();
}

ScriptCommand RequiredCommand(const std::uint8_t* instruction,
                              std::size_t length, const char* operation) {
  if (instruction == nullptr) Refuse(operation, "missing instruction");
  return ScriptCommand{instruction, length};
}

Field_MpdDoorRecord PackedDoor(const Field::DoorRecord& door) {
  Field_MpdDoorRecord record{};
  Field::StoreDoorRecord(door, record);
  return record;
}

class LiveSceneTransitionServices final : public SceneTransitionHost {
 public:
  void Refuse(SceneTransitionRefusal /*refusal*/, int value) override {
    VS::Core::LogError("event.scene",
                       "scene handoff refused a {}-byte instruction", value);
    std::abort();
  }

  void ForgetRoomEnemies(std::uint8_t room, std::uint8_t zone) override {
    Actors::ForgetLiveRoomEnemies(room, zone);
  }
  void SetGameplayHudHidden(std::uint8_t command) override {
    static_cast<void>(Battle::SetLiveGameplayHudHidden(command));
  }

  std::uint8_t ScriptFlag(int id) override { return State::LiveScriptFlag(id); }
  void SetScriptFlag(int id, std::uint8_t value) override {
    State::SetLiveScriptFlag(id, value);
  }

  void BeginEventRewardResults() override {
    Battle::BeginLiveEventRewardResults();
  }
  void BeginPostActionRewards() override {
    Battle::BeginLivePostActionRewards();
  }
  bool UpdateRewardSequence() override {
    return Battle::UpdateLiveRewardSequence() != 0;
  }
  void SetMusicSlot(std::uint8_t slot) override {
    musicSlot = static_cast<char>(slot);
  }

  void OpenGameMenu(std::uint8_t menu, int mode) override {
    static_cast<void>(Menu::Framework::OpenLiveGameMenu(menu, mode));
  }
  void UpdateMenuOverlay() override {
    Menu::Framework::UpdateLiveMenuOverlay();
  }
  std::int16_t GameMenuStatus() override {
    const Menu::Framework::ActionSelection* selection =
        Menu::Framework::LiveActionSelectionState();
    if (selection == nullptr) {
      VS::Event::Refuse("ScriptRunMenuTransition", "missing menu state");
    }
    return selection->menu_status;
  }
  void ReleaseMenuRows() override { Menu::Framework::ReleaseLiveMenuRows(); }

  void SetZoneMusicSwitchEnabled(bool enabled) override {
    static_cast<void>(Audio::LiveSetZoneMusicSwitchEnabled(enabled ? 1 : 0));
  }
  void BeginScriptedRoomTransition(const Field::DoorRecord& door,
                                   std::uint8_t effect,
                                   std::uint8_t speed) override {
    const Field_MpdDoorRecord record = PackedDoor(door);
    Field::LiveBeginScriptedRoomTransition(&record, effect, speed);
  }
  void BeginScriptedZoneTransition(const Field::DoorRecord& door,
                                   std::uint8_t effect,
                                   std::uint8_t speed) override {
    const Field_MpdDoorRecord record = PackedDoor(door);
    Field::LiveBeginScriptedZoneTransition(&record, effect, speed);
  }
};
}  // namespace

SceneTransitionHost& LiveSceneTransitionHost() {
  static LiveSceneTransitionServices host;
  return host;
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800B91C4 (F01099, src/BATTLE/BATTLE.PRG/4A0A8.c:1780,
// 0x800B91C4-0x800B91F0), opcode 0x53 in the normal and skip tables.
std::intptr_t VS::Event::ScriptClearRoomActorCacheOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  const event::ScriptCommand command =
      event::RequiredCommand(instruction, event::kClearRoomActorCacheLength,
                             "ScriptClearRoomActorCache");
  return static_cast<std::intptr_t>(event::ScriptClearRoomActorCache(
      command, event::LiveSceneTransitionHost()));
}

// decomp: BATTLE.PRG func_800B92D0 (F01104, src/BATTLE/BATTLE.PRG/4A0A8.c:1813,
// 0x800B92D0-0x800B92F4), opcode 0x58 in the normal and skip tables.
std::intptr_t VS::Event::ScriptSetHudVisibleOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  const event::ScriptCommand command = event::RequiredCommand(
      instruction, event::kSetHudVisibleLength, "ScriptSetHudVisible");
  return static_cast<std::intptr_t>(
      event::ScriptSetHudVisible(command, event::LiveSceneTransitionHost()));
}

// decomp: BATTLE.PRG func_800B92F4 (F01105, src/BATTLE/BATTLE.PRG/4A0A8.c:1819,
// 0x800B92F4-0x800B94D8), opcode 0x59 in the normal and skip tables. The
// active flag is written only when the opcode sets or clears it, as the
// original wrote it, so a hold keeps whatever nonzero byte it found.
std::intptr_t VS::Event::ScriptRunMenuTransitionOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  const event::ScriptCommand command = event::RequiredCommand(
      instruction, event::kRunMenuTransitionLength, "ScriptRunMenuTransition");
  const bool was_active = VS::Event::isMenuTransitionActive != 0;
  bool active = was_active;
  const event::ScriptStep step = event::ScriptRunMenuTransition(
      command, active, event::LiveSceneTransitionHost());
  if (active != was_active) VS::Event::isMenuTransitionActive = active ? 1 : 0;
  return static_cast<std::intptr_t>(step);
}

// decomp: BATTLE.PRG func_800B98C0 (F01120, src/BATTLE/BATTLE.PRG/4A0A8.c:2018,
// 0x800B98C0-0x800B9A40), opcode 0x68 in the normal and skip tables.
std::intptr_t VS::Event::ScriptChangeRoomOpcode(std::uint8_t* instruction,
                                                std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  const event::ScriptCommand command = event::RequiredCommand(
      instruction, event::kChangeRoomLength, "ScriptChangeRoom");
  std::int16_t state = VS::Event::roomChangeState;
  const event::ScriptStep step =
      event::ScriptChangeRoom(command, state, event::LiveSceneTransitionHost());
  VS::Event::roomChangeState = state;
  return static_cast<std::intptr_t>(step);
}
