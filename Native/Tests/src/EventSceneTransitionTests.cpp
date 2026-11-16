#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Event/EventSceneTransition.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptStep.hpp"
#include "Field/DoorRecord.hpp"

extern "C" {
#include "Battle/ActionFlow.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptVm.h"
#include "State/RoomMemory.h"
}

namespace {
namespace event = VS::Event;
using event::ScriptStep;

enum class Call : std::uint8_t {
  kRefuse,
  kForgetRoomEnemies,
  kSetGameplayHudHidden,
  kScriptFlag,
  kSetScriptFlag,
  kBeginEventRewardResults,
  kBeginPostActionRewards,
  kUpdateRewardSequence,
  kSetMusicSlot,
  kOpenGameMenu,
  kUpdateMenuOverlay,
  kGameMenuStatus,
  kReleaseMenuRows,
  kSetZoneMusicSwitchEnabled,
  kBeginScriptedRoomTransition,
  kBeginScriptedZoneTransition,
};

struct Recorded {
  Call call;
  int first = 0;
  int second = 0;

  friend bool operator==(const Recorded&, const Recorded&) = default;
};

struct Transition {
  VS::Field::DoorRecord door;
  std::uint8_t effect = 0;
  std::uint8_t speed = 0;
};

// Records every service call in order; flag reads, reward updates and menu
// statuses come from the queued values (0 once they run out).
class RecordingHost final : public event::SceneTransitionHost {
 public:
  std::vector<Recorded> calls;
  std::optional<Transition> transition;
  std::uint8_t flag_value = 0;
  std::deque<bool> reward_running;
  std::deque<std::int16_t> menu_statuses;

  void Refuse(event::SceneTransitionRefusal /*refusal*/, int value) override {
    calls.push_back({Call::kRefuse, value});
  }
  void ForgetRoomEnemies(std::uint8_t room, std::uint8_t zone) override {
    calls.push_back({Call::kForgetRoomEnemies, room, zone});
  }
  void SetGameplayHudHidden(std::uint8_t command) override {
    calls.push_back({Call::kSetGameplayHudHidden, command});
  }
  std::uint8_t ScriptFlag(int id) override {
    calls.push_back({Call::kScriptFlag, id});
    return flag_value;
  }
  void SetScriptFlag(int id, std::uint8_t value) override {
    calls.push_back({Call::kSetScriptFlag, id, value});
  }
  void BeginEventRewardResults() override {
    calls.push_back({Call::kBeginEventRewardResults});
  }
  void BeginPostActionRewards() override {
    calls.push_back({Call::kBeginPostActionRewards});
  }
  bool UpdateRewardSequence() override {
    calls.push_back({Call::kUpdateRewardSequence});
    if (reward_running.empty()) return false;
    const bool running = reward_running.front();
    reward_running.pop_front();
    return running;
  }
  void SetMusicSlot(std::uint8_t slot) override {
    calls.push_back({Call::kSetMusicSlot, slot});
  }
  void OpenGameMenu(std::uint8_t menu, int mode) override {
    calls.push_back({Call::kOpenGameMenu, menu, mode});
  }
  void UpdateMenuOverlay() override {
    calls.push_back({Call::kUpdateMenuOverlay});
  }
  std::int16_t GameMenuStatus() override {
    calls.push_back({Call::kGameMenuStatus});
    if (menu_statuses.empty()) return 0;
    const std::int16_t status = menu_statuses.front();
    menu_statuses.pop_front();
    return status;
  }
  void ReleaseMenuRows() override { calls.push_back({Call::kReleaseMenuRows}); }
  void SetZoneMusicSwitchEnabled(bool enabled) override {
    calls.push_back({Call::kSetZoneMusicSwitchEnabled, enabled ? 1 : 0});
  }
  void BeginScriptedRoomTransition(const VS::Field::DoorRecord& door,
                                   std::uint8_t effect,
                                   std::uint8_t speed) override {
    calls.push_back({Call::kBeginScriptedRoomTransition, effect, speed});
    transition = Transition{door, effect, speed};
  }
  void BeginScriptedZoneTransition(const VS::Field::DoorRecord& door,
                                   std::uint8_t effect,
                                   std::uint8_t speed) override {
    calls.push_back({Call::kBeginScriptedZoneTransition, effect, speed});
    transition = Transition{door, effect, speed};
  }
};

using Calls = std::vector<Recorded>;

constexpr Recorded ShortRefusal(int length) { return {Call::kRefuse, length}; }

std::array<std::uint8_t, event::kRunMenuTransitionLength> MenuCommand(
    std::uint8_t mode) {
  return {0x59, mode, 0, 0};
}

ScriptStep RunMenu(std::uint8_t mode, bool& active, RecordingHost& host) {
  const auto bytes = MenuCommand(mode);
  return event::ScriptRunMenuTransition(bytes, active, host);
}

Recorded ResetResultsScreen() {
  return {Call::kSetScriptFlag, event::kResultsScreenFlag,
          event::kGameClearScreen};
}
}  // namespace

// --- F01099 func_800B91C4 ScriptClearRoomActorCache ---

TEST_CASE("ScriptClearRoomActorCache forgets byte 2's room of byte 1's zone",
          "[event][scene-transition]") {
  RecordingHost host;
  const std::array<std::uint8_t, 3> bytes{0x53, 31, 7};
  CHECK(event::ScriptClearRoomActorCache(bytes, host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{{Call::kForgetRoomEnemies, 7, 31}});
}

TEST_CASE("ScriptClearRoomActorCache refuses a short instruction",
          "[event][scene-transition]") {
  RecordingHost host;
  const std::array<std::uint8_t, 2> bytes{0x53, 31};
  CHECK(event::ScriptClearRoomActorCache(bytes, host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{ShortRefusal(2)});
}

TEST_CASE("ScriptClearRoomActorCache opcode clears the live room records",
          "[event][scene-transition]") {
  const auto saved = std::to_array(State_recentRoomEnemyStates);
  const int saved_slot = State_currentRoomStateSlot;
  constexpr unsigned char kRemembered = 0x5A;
  for (State_RoomEnemyStates& record : State_recentRoomEnemyStates) {
    record.mapId = 9;
    record.zoneId = 12;
    std::memset(record.actorStates, kRemembered, sizeof(record.actorStates));
  }
  State_recentRoomEnemyStates[1].zoneId = 13;
  State_recentRoomEnemyStates[2].mapId = 8;
  State_currentRoomStateSlot = 0;
  std::array<std::uint8_t, 3> bytes{0x53, 12, 9};
  CHECK(Event_ScriptClearRoomActorCacheOpcode(bytes.data(), 0) == 0);
  const auto first_byte = [](const State_RoomEnemyStates& record) {
    unsigned char value = 0;
    std::memcpy(&value, record.actorStates, sizeof(value));
    return value;
  };
  CHECK(first_byte(State_recentRoomEnemyStates[0]) == 0);
  CHECK(first_byte(State_recentRoomEnemyStates[1]) == kRemembered);
  CHECK(first_byte(State_recentRoomEnemyStates[2]) == kRemembered);
  CHECK(first_byte(State_recentRoomEnemyStates[3]) == 0);
  CHECK(State_currentRoomStateSlot == 0);
  std::copy(saved.begin(), saved.end(), State_recentRoomEnemyStates);
  State_currentRoomStateSlot = saved_slot;
}

// --- F01104 func_800B92D0 ScriptSetHudVisible ---

TEST_CASE(
    "ScriptSetHudVisible passes the visibility byte with its low bit "
    "flipped",
    "[event][scene-transition]") {
  constexpr std::array<std::pair<std::uint8_t, std::uint8_t>, 4> kCases{{
      {1, 0},
      {0, 1},
      {0xFE, 0xFF},
      {0x03, 0x02},
  }};
  for (const auto& [visible, command] : kCases) {
    RecordingHost host;
    const std::array<std::uint8_t, 2> bytes{0x58, visible};
    CHECK(event::ScriptSetHudVisible(bytes, host) == ScriptStep::kAdvance);
    CHECK(host.calls == Calls{{Call::kSetGameplayHudHidden, command}});
  }
}

TEST_CASE("ScriptSetHudVisible refuses a short instruction",
          "[event][scene-transition]") {
  RecordingHost host;
  const std::array<std::uint8_t, 1> bytes{0x58};
  CHECK(event::ScriptSetHudVisible(bytes, host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{ShortRefusal(1)});
}

// --- F01105 func_800B92F4 ScriptRunMenuTransition ---

TEST_CASE(
    "ScriptRunMenuTransition opens the cube puzzle screen of modes 10 "
    "to 12",
    "[event][scene-transition]") {
  constexpr std::array<std::pair<std::uint8_t, std::uint8_t>, 3> kScreens{{
      {10, 6},
      {11, 4},
      {12, 5},
  }};
  for (const auto& [mode, screen] : kScreens) {
    RecordingHost host;
    bool active = false;
    CHECK(RunMenu(mode, active, host) == ScriptStep::kHold);
    CHECK(active);
    CHECK(host.calls ==
          Calls{{Call::kSetScriptFlag, event::kResultsScreenFlag, screen},
                {Call::kBeginEventRewardResults}});
  }
}

TEST_CASE(
    "ScriptRunMenuTransition holds a cube puzzle until its sequence "
    "ends and resets the screen",
    "[event][scene-transition]") {
  RecordingHost host;
  host.reward_running = {true, false};
  bool active = true;
  CHECK(RunMenu(11, active, host) == ScriptStep::kHold);
  CHECK(active);
  CHECK(host.calls == Calls{{Call::kUpdateRewardSequence}});
  host.calls.clear();
  CHECK(RunMenu(11, active, host) == ScriptStep::kAdvanceAndYield);
  CHECK_FALSE(active);
  CHECK(host.calls ==
        Calls{{Call::kUpdateRewardSequence}, ResetResultsScreen()});
}

TEST_CASE(
    "ScriptRunMenuTransition starts the time attack and keeps its "
    "screen when it ends",
    "[event][scene-transition]") {
  RecordingHost host;
  bool active = false;
  CHECK(RunMenu(13, active, host) == ScriptStep::kHold);
  CHECK(active);
  CHECK(host.calls == Calls{{Call::kSetScriptFlag, event::kResultsScreenFlag,
                             event::kTimeAttackStartScreen},
                            {Call::kBeginEventRewardResults}});
  host.calls.clear();
  host.reward_running = {true};
  CHECK(RunMenu(13, active, host) == ScriptStep::kHold);
  CHECK(RunMenu(13, active, host) == ScriptStep::kAdvanceAndYield);
  CHECK_FALSE(active);
  CHECK(host.calls ==
        Calls{{Call::kUpdateRewardSequence}, {Call::kUpdateRewardSequence}});
}

TEST_CASE(
    "ScriptRunMenuTransition shows the time attack results in place of "
    "the game clear",
    "[event][scene-transition]") {
  RecordingHost host;
  host.flag_value = event::kTimeAttackStartScreen;
  bool active = false;
  CHECK(RunMenu(14, active, host) == ScriptStep::kHold);
  CHECK(active);
  CHECK(host.calls == Calls{{Call::kScriptFlag, event::kResultsScreenFlag},
                            {Call::kSetScriptFlag, event::kResultsScreenFlag,
                             event::kTimeAttackResultsScreen},
                            {Call::kBeginEventRewardResults},
                            {Call::kSetMusicSlot, event::kResultsMusicSlot}});
}

TEST_CASE("ScriptRunMenuTransition keeps another screen for the game clear",
          "[event][scene-transition]") {
  for (const std::uint8_t screen : {0, 2, 5, 0xFF}) {
    RecordingHost host;
    host.flag_value = screen;
    bool active = false;
    CHECK(RunMenu(14, active, host) == ScriptStep::kHold);
    CHECK(host.calls == Calls{{Call::kScriptFlag, event::kResultsScreenFlag},
                              {Call::kBeginEventRewardResults},
                              {Call::kSetMusicSlot, event::kResultsMusicSlot}});
  }
}

TEST_CASE("ScriptRunMenuTransition resets the screen when the game clear ends",
          "[event][scene-transition]") {
  RecordingHost host;
  bool active = true;
  CHECK(RunMenu(14, active, host) == ScriptStep::kAdvanceAndYield);
  CHECK_FALSE(active);
  CHECK(host.calls ==
        Calls{{Call::kUpdateRewardSequence}, ResetResultsScreen()});
}

TEST_CASE(
    "ScriptRunMenuTransition runs the post-action rewards without the "
    "screen flag",
    "[event][scene-transition]") {
  RecordingHost host;
  bool active = false;
  CHECK(RunMenu(15, active, host) == ScriptStep::kHold);
  CHECK(active);
  CHECK(host.calls == Calls{{Call::kBeginPostActionRewards}});
  host.calls.clear();
  host.reward_running = {true};
  CHECK(RunMenu(15, active, host) == ScriptStep::kHold);
  CHECK(active);
  CHECK(RunMenu(15, active, host) == ScriptStep::kAdvanceAndYield);
  CHECK_FALSE(active);
  CHECK(host.calls ==
        Calls{{Call::kUpdateRewardSequence}, {Call::kUpdateRewardSequence}});
}

TEST_CASE("ScriptRunMenuTransition opens any other mode as a script game menu",
          "[event][scene-transition]") {
  for (const std::uint8_t mode : {0, 3, 9, 16, 0xFF}) {
    RecordingHost host;
    bool active = false;
    CHECK(RunMenu(mode, active, host) == ScriptStep::kHold);
    CHECK(active);
    CHECK(host.calls ==
          Calls{{Call::kOpenGameMenu, mode, event::kScriptGameMenuMode}});
  }
}

TEST_CASE(
    "ScriptRunMenuTransition holds a game menu until its status is "
    "closed",
    "[event][scene-transition]") {
  RecordingHost host;
  host.menu_statuses = {0, -2, 0x0102, event::kGameMenuClosed};
  bool active = true;
  for (int frame = 0; frame < 3; ++frame) {
    CHECK(RunMenu(4, active, host) == ScriptStep::kHold);
    CHECK(active);
  }
  CHECK(RunMenu(4, active, host) == ScriptStep::kAdvanceAndYield);
  CHECK_FALSE(active);
  const Recorded update{Call::kUpdateMenuOverlay};
  const Recorded status{Call::kGameMenuStatus};
  CHECK(host.calls == Calls{update,
                            status,
                            update,
                            status,
                            update,
                            status,
                            update,
                            status,
                            {Call::kReleaseMenuRows}});
}

TEST_CASE("ScriptRunMenuTransition refuses a short instruction",
          "[event][scene-transition]") {
  RecordingHost host;
  const std::array<std::uint8_t, 3> bytes{0x59, 14, 0};
  bool active = true;
  CHECK(event::ScriptRunMenuTransition(bytes, active, host) ==
        ScriptStep::kAdvance);
  CHECK(active);
  CHECK(host.calls == Calls{ShortRefusal(3)});
}

TEST_CASE(
    "ScriptRunMenuTransition opcode clears the live active flag when "
    "the rewards end",
    "[event][scene-transition]") {
  const char saved_active = Event_isMenuTransitionActive;
  const auto saved_phase = Battle_actionFlowMode;
  // No reward phase runs, so the sequence has ended.
  Battle_actionFlowMode = 0;
  Event_isMenuTransitionActive = 5;
  std::array<std::uint8_t, event::kRunMenuTransitionLength> bytes =
      MenuCommand(event::kTimeAttackStartMode);
  CHECK(Event_ScriptRunMenuTransitionOpcode(bytes.data(), 0) == 4);
  CHECK(Event_isMenuTransitionActive == 0);
  Event_isMenuTransitionActive = saved_active;
  Battle_actionFlowMode = saved_phase;
}

// --- F01120 func_800B98C0 ScriptChangeRoom ---

TEST_CASE("ScriptChangeRoom starts a room transition within the zone",
          "[event][scene-transition]") {
  RecordingHost host;
  const std::array<std::uint8_t, event::kChangeRoomLength> bytes{
      0x68, 0, 23, 4, 3, 17, 1, 0x01, 6, 9};
  std::int16_t state = event::kRoomChangeIdle;
  CHECK(event::ScriptChangeRoom(bytes, state, host) == ScriptStep::kHold);
  CHECK(state == event::kRoomChangeStopsEvent);
  CHECK(host.calls == Calls{{Call::kBeginScriptedRoomTransition, 6, 9}});
  const auto transition = host.transition;
  REQUIRE(transition.has_value());
  if (!transition.has_value()) return;
  const VS::Field::DoorRecord expected{
      .destination_zone_id = 0,
      .destination_room_id = 23,
      .entry_direction = 3,
      .entry_tile_x = 4,
      .entry_tile_z = 17,
      .entry_elevation = 1,
      .has_entry_step = true,
  };
  CHECK(transition->door == expected);
}

TEST_CASE(
    "ScriptChangeRoom starts a zone transition with the zone music "
    "switch",
    "[event][scene-transition]") {
  constexpr std::array<std::pair<std::uint8_t, int>, 2> kOptions{{
      {0x0C, 1},
      {0x04, 0},
  }};
  for (const auto& [options, music_switch] : kOptions) {
    RecordingHost host;
    const std::array<std::uint8_t, event::kChangeRoomLength> bytes{
        0x68, 5, 2, 0, 0, 0, 0, options, 1, 3};
    std::int16_t state = event::kRoomChangeIdle;
    CHECK(event::ScriptChangeRoom(bytes, state, host) == ScriptStep::kHold);
    CHECK(state == event::kRoomChangeKeepsEvent);
    CHECK(host.calls == Calls{{Call::kSetZoneMusicSwitchEnabled, music_switch},
                              {Call::kBeginScriptedZoneTransition, 1, 3}});
    const auto transition = host.transition;
    REQUIRE(transition.has_value());
    if (!transition.has_value()) return;
    CHECK(transition->door.destination_zone_id == 5);
    CHECK(transition->door.destination_room_id == 2);
  }
}

TEST_CASE("ScriptChangeRoom masks the door fields and zeroes the unset ones",
          "[event][scene-transition]") {
  RecordingHost host;
  const std::array<std::uint8_t, event::kChangeRoomLength> bytes{
      0x68, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0xFF, 0xFF};
  std::int16_t state = event::kRoomChangeIdle;
  CHECK(event::ScriptChangeRoom(bytes, state, host) == ScriptStep::kHold);
  CHECK(state == event::kRoomChangeStopsEvent);
  const auto transition = host.transition;
  REQUIRE(transition.has_value());
  if (!transition.has_value()) return;
  const VS::Field::DoorRecord expected{
      .destination_room_id = 0xFF,
      .entry_direction = VS::Field::kDoorEntryDirectionMask,
      .entry_tile_x = VS::Field::kDoorTileMask,
      .entry_tile_z = VS::Field::kDoorTileMask,
      .entry_elevation = VS::Field::kDoorEntryElevationMask,
      .has_entry_step = true,
      .reserved_entry_bit = true,
  };
  CHECK(transition->door == expected);
  CHECK(transition->effect == 0xFF);
  CHECK(transition->speed == 0xFF);
}

TEST_CASE("ScriptChangeRoom holds until the room loader reports the arrival",
          "[event][scene-transition]") {
  const std::array<std::uint8_t, event::kChangeRoomLength> bytes{
      0x68, 3, 4, 5, 1, 6, 2, 0x04, 0, 0};
  constexpr std::array<std::int16_t, 4> kWaiting{
      event::kRoomChangeKeepsEvent, event::kRoomChangeStopsEvent, -1, 0x0102};
  for (const std::int16_t waiting : kWaiting) {
    RecordingHost host;
    std::int16_t state = waiting;
    CHECK(event::ScriptChangeRoom(bytes, state, host) == ScriptStep::kHold);
    CHECK(state == waiting);
    CHECK(host.calls.empty());
  }
  RecordingHost host;
  std::int16_t state = event::kRoomChangeArrived;
  CHECK(event::ScriptChangeRoom(bytes, state, host) == ScriptStep::kAdvance);
  CHECK(state == event::kRoomChangeIdle);
  CHECK(host.calls.empty());
}

TEST_CASE("ScriptChangeRoom refuses a short instruction",
          "[event][scene-transition]") {
  RecordingHost host;
  const std::array<std::uint8_t, 9> bytes{0x68, 1, 2, 3, 4, 5, 6, 7, 8};
  std::int16_t state = event::kRoomChangeIdle;
  CHECK(event::ScriptChangeRoom(bytes, state, host) == ScriptStep::kAdvance);
  CHECK(state == event::kRoomChangeIdle);
  CHECK(host.calls == Calls{ShortRefusal(9)});
}

TEST_CASE("ScriptChangeRoom opcode maps the live room change state",
          "[event][scene-transition]") {
  const std::int16_t saved = Event_roomChangeState;
  std::array<std::uint8_t, event::kChangeRoomLength> bytes{0x68, 3, 4,    5, 1,
                                                           6,    2, 0x04, 0, 0};
  Event_roomChangeState = event::kRoomChangeKeepsEvent;
  CHECK(Event_ScriptChangeRoomOpcode(bytes.data(), 0) == 1);
  CHECK(Event_roomChangeState == event::kRoomChangeKeepsEvent);
  Event_roomChangeState = event::kRoomChangeArrived;
  CHECK(Event_ScriptChangeRoomOpcode(bytes.data(), 0) == 0);
  CHECK(Event_roomChangeState == event::kRoomChangeIdle);
  Event_roomChangeState = saved;
}
