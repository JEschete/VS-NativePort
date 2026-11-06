#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Event/ScriptVmLive.hpp"
#include "State/GameClock.hpp"
#include "State/GameClockLive.hpp"
#include "State/KillScore.hpp"
#include "State/KillScoreLive.hpp"
#include "State/SceneTimer.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/ScriptFlags.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Core/BiosRand.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "State/PlayTime.h"
#include "State/Timers.h"
}

namespace state = VS::State;
using state::KillScore;
using state::PlayTime;
using state::SceneTime;
using state::SceneTimer;
using state::ScriptFlags;
using state::ScriptFlagWrite;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

namespace {
constexpr int kCountDown = 2;
constexpr int kLastFlag = static_cast<int>(state::kScriptFlagCount) - 1;

class RecordingObserver final : public state::ScriptFlagObserver {
 public:
  std::vector<ScriptFlagWrite> writes;
  void FlagWritten(const ScriptFlagWrite& write) override {
    writes.push_back(write);
  }
};

// A flag record and its view.
struct TestFlags {
  std::array<std::byte, state::kScriptFlagCount> bytes{};
  RecordingObserver observer;
  ScriptFlags flags{bytes, &observer};
};

class FakeTimerHost final : public state::SceneTimerHost {
 public:
  int shown = 0;
  int hidden = 0;
  int draws = 0;
  int next_random = 0;
  void ShowTimer() override { ++shown; }
  void HideTimer() override { ++hidden; }
  int Random() override {
    ++draws;
    return next_random;
  }
};

[[nodiscard]] SceneTime TimeOf(const ScriptFlags& flags) {
  return SceneTime{
      .frames =
          static_cast<std::int8_t>(flags.Get(state::kSceneTimerFramesFlag)),
      .seconds =
          static_cast<std::int8_t>(flags.Get(state::kSceneTimerSecondsFlag)),
      .minutes =
          static_cast<std::int8_t>(flags.Get(state::kSceneTimerMinutesFlag)),
      .hours =
          static_cast<std::int8_t>(flags.Get(state::kSceneTimerHoursFlag))};
}

[[nodiscard]] SceneTimer CountingDown(SceneTime time) {
  return SceneTimer{.mode = kCountDown, .time = time};
}

// The scene timer's live state, its flags, the puzzle timer HUD, the BIOS
// random sequence and the room trigger state, restored after one test.
struct LiveTimerState {
  State_SceneTimer saved_timer = State_sceneTimer;
  int saved_tick_speed = State_tickSpeed;
  State_StateFlags saved_flags = Audio_Seq_unusedStateFlags;
  char saved_hud = Battle_lowerScreenUiState;
  unsigned saved_random = Core_Random_State();
  void* saved_triggers = Event_roomEventTriggers;
  VS::Event::RoomEventLease saved_room = VS::Event::LiveRoomScriptSections();
  char saved_phase = Event_playbackPhase;
  char saved_direct = Event_isDirectEventRequest;
  std::int16_t saved_event = Event_eventId;
  std::uint16_t saved_source = Event_eventSource;

  LiveTimerState() = default;
  LiveTimerState(const LiveTimerState&) = delete;
  LiveTimerState& operator=(const LiveTimerState&) = delete;
  LiveTimerState(LiveTimerState&&) = delete;
  LiveTimerState& operator=(LiveTimerState&&) = delete;
  ~LiveTimerState() {
    State_sceneTimer = saved_timer;
    State_tickSpeed = saved_tick_speed;
    Audio_Seq_unusedStateFlags = saved_flags;
    Battle_lowerScreenUiState = saved_hud;
    Core_Random_Seed(saved_random);
    Event_roomEventTriggers = saved_triggers;
    VS::Event::PublishLiveRoomScriptSections(saved_room);
    Event_playbackPhase = saved_phase;
    Event_isDirectEventRequest = saved_direct;
    Event_eventId = saved_event;
    Event_eventSource = saved_source;
  }
};

struct LiveTimerExpiry {
  LiveTimerState saved;
  static constexpr std::size_t kTriggerSlots = 16;
  static constexpr std::uint16_t kNoTrigger = 0xFFFF;
  static constexpr std::uint8_t kPlayEvent = 0xFF;
  static constexpr std::uint8_t kExpiryEvent = 9;

  struct TriggerTable {
    std::array<std::uint16_t, kTriggerSlots> offsets{};
    std::array<std::uint8_t, 4> trigger{};
  } table;

  LiveTimerExpiry() {
    table.offsets.fill(kNoTrigger);
    table.offsets[0] =
        static_cast<std::uint16_t>(offsetof(TriggerTable, trigger));
    table.trigger = {static_cast<std::uint8_t>(state::kSceneTimerExpiryTrigger),
                     kExpiryEvent, 0, kPlayEvent};
    const auto bytes = std::as_bytes(std::span(&table, 1));
    const auto room = VS::Event::RoomEventSections::Copy(
        {.triggers = {reinterpret_cast<const std::uint8_t*>(bytes.data()),
                      bytes.size()}});
    REQUIRE(room);
    VS::Event::PublishLiveRoomScriptSections(*room);
    Event_roomEventTriggers = &table;
    Event_playbackPhase = 0;
    Event_isDirectEventRequest = 0;
    Event_eventId = 0;
  }

  void Start(int packed_time) {
    State_tickSpeed = state::kNormalTickSpeed;
    VS::State::StartLiveSceneTimer(kCountDown, 0, packed_time);
  }
};

// The score record and the battle action and player, restored after one
// test.
struct LiveScoreState {
  State_ScoreData saved_score = Audio_Seq_unusedScoredata;
  Battle_ActionState* saved_action = Battle_actionState;
  Battle_Actor* saved_player = Battle_characterState;

  LiveScoreState() = default;
  LiveScoreState(const LiveScoreState&) = delete;
  LiveScoreState& operator=(const LiveScoreState&) = delete;
  LiveScoreState(LiveScoreState&&) = delete;
  LiveScoreState& operator=(LiveScoreState&&) = delete;
  ~LiveScoreState() {
    Audio_Seq_unusedScoredata = saved_score;
    Battle_actionState = saved_action;
    Battle_characterState = saved_player;
  }
};
}  // namespace

TEST_CASE("Script flags read and write only inside the record",
          "[state-boot]") {
  TestFlags test;
  CHECK(test.flags.Contains(0));
  CHECK(test.flags.Contains(kLastFlag));

  CHECK(test.flags.Set(0x82, 0x90));
  CHECK(test.flags.Set(0x82, 3));
  CHECK(test.flags.Get(0x82) == 3);
  CHECK(test.observer.writes ==
        std::vector<ScriptFlagWrite>{{.id = 0x82, .before = 0, .after = 0x90},
                                     {.id = 0x82, .before = 0x90, .after = 3}});

  CHECK(test.flags.Set(kLastFlag, 0xFF));
  CHECK(test.flags.Get(kLastFlag) == 0xFF);
  CHECK(test.observer.writes.size() == 3);
}

TEST_CASE("Script flags refuse out-of-range ids", "[state-boot]") {
  TestFlags test;
  CHECK_FALSE(test.flags.Contains(-1));
  CHECK_FALSE(test.flags.Contains(kLastFlag + 1));
  CHECK_FALSE(test.flags.Set(kLastFlag + 1, 1));
  CHECK_FALSE(test.flags.Set(-1, 1));
  CHECK(test.flags.Get(kLastFlag + 1) == 0);
  CHECK(test.flags.Get(-1) == 0);
  CHECK(test.observer.writes.empty());
}

TEST_CASE("A zone reset clears only the zone flags, one write each",
          "[state-boot]") {
  TestFlags test;
  std::ranges::fill(test.bytes, std::byte{0xFF});
  test.flags.ResetZone(true);
  CHECK(test.observer.writes.empty());
  CHECK(test.flags.Get(state::kFirstZoneScriptFlag) == 0xFF);

  test.flags.ResetZone(false);
  REQUIRE(test.observer.writes.size() ==
          static_cast<std::size_t>(state::kZoneScriptFlagEnd -
                                   state::kFirstZoneScriptFlag));
  CHECK(test.observer.writes.front() ==
        ScriptFlagWrite{
            .id = state::kFirstZoneScriptFlag, .before = 0xFF, .after = 0});
  CHECK(test.observer.writes.back().id == state::kZoneScriptFlagEnd - 1);
  for (int id = state::kFirstZoneScriptFlag; id < state::kZoneScriptFlagEnd;
       ++id) {
    CHECK(test.flags.Get(id) == 0);
  }
  CHECK(test.flags.Get(state::kFirstZoneScriptFlag - 1) == 0xFF);
  CHECK(test.flags.Get(state::kZoneScriptFlagEnd) == 0xFF);
}

TEST_CASE("Play time carries whole seconds, minutes and hours",
          "[state-boot]") {
  PlayTime time{.frames = 10, .seconds = 5, .minutes = 0, .hours = 0};
  state::AdvancePlayTime(time, state::kSlowedTickSpeed);
  CHECK(time == PlayTime{.frames = 14, .seconds = 5});

  time = PlayTime{.frames = 58, .seconds = 59, .minutes = 59, .hours = 3};
  state::AdvancePlayTime(time, state::kNormalTickSpeed);
  CHECK(time == PlayTime{.frames = 0, .seconds = 0, .minutes = 0, .hours = 4});

  time = PlayTime{.frames = 58, .seconds = 30};
  state::AdvancePlayTime(time, state::kNormalTickSpeed);
  CHECK(time == PlayTime{.frames = 0, .seconds = 31});
}

TEST_CASE("Play time stops at 100 hours", "[state-boot]") {
  PlayTime time{.frames = 58, .seconds = 59, .minutes = 59, .hours = 99};
  state::AdvancePlayTime(time, state::kNormalTickSpeed);
  CHECK(time == PlayTime{.hours = state::kMaxPlayTimeHours});
  state::AdvancePlayTime(time, state::kNormalTickSpeed);
  CHECK(time == PlayTime{.hours = state::kMaxPlayTimeHours});
}

TEST_CASE("Packed countdown times unpack with signed C division",
          "[state-boot]") {
  CHECK(state::UnpackSceneTime(0x01020304) ==
        SceneTime{.frames = 4, .seconds = 3, .minutes = 2, .hours = 1});
  CHECK(state::UnpackSceneTime(-1) == SceneTime{.frames = -1});
  CHECK(state::UnpackSceneTime(-257) == SceneTime{.frames = -1, .seconds = -1});
  CHECK(state::UnpackSceneTime(INT32_MIN) == SceneTime{.hours = -128});
}

TEST_CASE("Starting the scene timer sets its mode and mirrors its time",
          "[state-boot]") {
  TestFlags test;
  FakeTimerHost host;
  SceneTimer timer{.paused_mode = 5};
  state::StartSceneTimer(timer, kCountDown, 0x1234, 0x0001021E, test.flags,
                         host);
  const SceneTime expected{.frames = 30, .seconds = 2, .minutes = 1};
  CHECK(timer == SceneTimer{.mode = kCountDown,
                            .paused_mode = 5,
                            .reset_scope = 0x1234,
                            .time = expected});
  CHECK(TimeOf(test.flags) == expected);
  CHECK(host.shown == 1);
  CHECK(host.draws == 0);

  state::StartSceneTimer(timer, state::kSceneTimerCountsUp, 7, 0x0001021E,
                         test.flags, host);
  CHECK(timer.mode == state::kSceneTimerCountsUp);
  CHECK(timer.reset_scope == 7);
  CHECK(timer.time == SceneTime{});
  CHECK(TimeOf(test.flags) == SceneTime{});
  CHECK(host.shown == 2);
}

TEST_CASE("Resetting the scene timer stops it and keeps its pause and scope",
          "[state-boot]") {
  TestFlags test;
  FakeTimerHost host;
  SceneTimer timer{.mode = kCountDown,
                   .paused_mode = kCountDown,
                   .reset_scope = 9,
                   .time = {.frames = 4, .seconds = 3}};
  std::ranges::fill(test.bytes, std::byte{0x11});
  state::ResetSceneTimer(timer, test.flags, host);
  CHECK(timer == SceneTimer{.mode = state::kSceneTimerStopped,
                            .paused_mode = kCountDown,
                            .reset_scope = 9});
  CHECK(TimeOf(test.flags) == SceneTime{});
  CHECK(host.hidden == 1);
}

TEST_CASE("Pausing the scene timer saves its mode; resuming restores it",
          "[state-boot]") {
  FakeTimerHost host;
  SceneTimer timer{.mode = kCountDown};
  state::SetSceneTimerPaused(timer, true, host);
  CHECK(timer.mode == state::kSceneTimerStopped);
  CHECK(timer.paused_mode == kCountDown);
  CHECK(host.hidden == 1);
  state::SetSceneTimerPaused(timer, false, host);
  CHECK(timer.mode == kCountDown);
  CHECK(host.shown == 1);

  // A second pause saves the stopped mode, so the resume stays stopped.
  state::SetSceneTimerPaused(timer, true, host);
  state::SetSceneTimerPaused(timer, true, host);
  CHECK(timer.paused_mode == state::kSceneTimerStopped);
  state::SetSceneTimerPaused(timer, false, host);
  CHECK(timer.mode == state::kSceneTimerStopped);
}

TEST_CASE("Only the normal and slowed tick speeds are taken", "[state-boot]") {
  CHECK(state::SelectTickSpeed(state::kNormalTickSpeed,
                               state::kSlowedTickSpeed) ==
        state::kSlowedTickSpeed);
  CHECK(state::SelectTickSpeed(state::kSlowedTickSpeed,
                               state::kNormalTickSpeed) ==
        state::kNormalTickSpeed);
  CHECK(state::SelectTickSpeed(state::kSlowedTickSpeed, 3) ==
        state::kSlowedTickSpeed);
  CHECK(state::SelectTickSpeed(state::kNormalTickSpeed, 0) ==
        state::kNormalTickSpeed);
}

TEST_CASE("A stopped scene timer does not advance", "[state-boot]") {
  TestFlags test;
  FakeTimerHost host;
  SceneTimer timer{.time = {.frames = 4}};
  CHECK_FALSE(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed,
                                       test.flags, host));
  CHECK(timer.time == SceneTime{.frames = 4});
  CHECK(test.observer.writes.empty());
  CHECK(host.draws == 0);
}

TEST_CASE("Counting up advances frames and mirrors hundredths",
          "[state-boot]") {
  TestFlags test;
  FakeTimerHost host;
  host.next_random = 0;
  SceneTimer timer{.mode = state::kSceneTimerCountsUp, .time = {.frames = 28}};
  CHECK_FALSE(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed,
                                       test.flags, host));
  CHECK(timer.time == SceneTime{.frames = 30});
  CHECK(host.draws == 1);
  CHECK(test.flags.Get(state::kSceneTimerFramesFlag) == 50);
}

TEST_CASE("Counting up carries time and stops at one hour", "[state-boot]") {
  TestFlags test;
  FakeTimerHost host;
  SceneTimer timer{.mode = state::kSceneTimerCountsUp,
                   .time = {.frames = 58, .seconds = 12}};
  CHECK(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed, test.flags,
                                 host));
  CHECK(timer.time == SceneTime{.seconds = 13});
  CHECK(host.draws == 0);
  CHECK(TimeOf(test.flags) == SceneTime{.seconds = 13});

  timer.time = SceneTime{.frames = 58, .seconds = 59, .minutes = 59};
  CHECK(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed, test.flags,
                                 host));
  CHECK(timer.time == SceneTime{.hours = 1});
  CHECK(state::AdvanceSceneTimer(timer, state::kSlowedTickSpeed, test.flags,
                                 host));
  CHECK(timer.time == SceneTime{.hours = 1});
  CHECK(TimeOf(test.flags) == SceneTime{.hours = 1});
}

TEST_CASE("Counting down borrows through the hours and ends at zero",
          "[state-boot]") {
  TestFlags test;
  FakeTimerHost host;
  host.next_random = 0x1234;
  SceneTimer timer = CountingDown({.hours = 1});
  CHECK_FALSE(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed,
                                       test.flags, host));
  const SceneTime borrowed{
      .frames = 58, .seconds = 59, .minutes = 59, .hours = 0};
  CHECK(timer.time == borrowed);
  CHECK(test.flags.Get(state::kSceneTimerFramesFlag) == 97);
  CHECK(test.flags.Get(state::kSceneTimerSecondsFlag) == 59);
  CHECK(test.flags.Get(state::kSceneTimerMinutesFlag) == 59);
  CHECK(test.flags.Get(state::kSceneTimerHoursFlag) == 0);
}

TEST_CASE("An ended countdown stays at zero and remains due", "[state-boot]") {
  TestFlags test;
  FakeTimerHost host;
  SceneTimer timer = CountingDown({.frames = 2});
  CHECK(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed, test.flags,
                                 host));
  CHECK(timer.time == SceneTime{});
  // An ended countdown stays at zero and stays due.
  const int draws = host.draws;
  CHECK(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed, test.flags,
                                 host));
  CHECK(timer.time == SceneTime{});
  CHECK(host.draws == draws);
  CHECK(TimeOf(test.flags) == SceneTime{});
}

TEST_CASE("A countdown with odd frames at an even speed never ends",
          "[state-boot]") {
  TestFlags test;
  FakeTimerHost host;
  SceneTimer timer = CountingDown({.frames = 1});
  for (int update = 0; update < 300; ++update) {
    REQUIRE_FALSE(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed,
                                           test.flags, host));
  }
  CHECK(timer.time.frames % 2 != 0);
}

TEST_CASE("The frames mirror scales hundredths with a random jitter",
          "[state-boot]") {
  struct Case {
    std::int8_t frames;
    int random;
    std::uint8_t mirror;
  };
  // Expected values from ((frames << 14) + random) * 5 >> 14, over 3, in
  // 32-bit unsigned arithmetic, truncated to a byte.
  for (const Case& c :
       {Case{30, 0, 50}, Case{30, 0x7FFF, 53}, Case{59, 0x7FFF, 101},
        Case{-1, 0, 83}, Case{-1, 0x7FFF, 1}}) {
    TestFlags test;
    FakeTimerHost host;
    host.next_random = c.random;
    // Counting down from frames + 2 by 2 lands on `frames`.
    SceneTimer timer = CountingDown(
        {.frames = static_cast<std::int8_t>(c.frames + 2), .seconds = 1});
    CHECK_FALSE(state::AdvanceSceneTimer(timer, state::kNormalTickSpeed,
                                         test.flags, host));
    CHECK(timer.time.frames == c.frames);
    CHECK(test.flags.Get(state::kSceneTimerFramesFlag) == c.mirror);
  }
}

TEST_CASE("A scored kill counts its class and adds its streak score",
          "[state-boot]") {
  KillScore score;
  score.weapon_kill_streak = 3;
  score.chain_streak = 4;
  const state::DefeatedEnemy enemy{
      .sub_type = 37,
      .scored = state::ScoredEnemy{.enemy_class = 2, .max_hp = 100}};
  CHECK(state::RecordEnemyKill(score, enemy));
  CHECK(score.enemy_kills[2] == 1);
  CHECK(score.kill_streak_enemy_class == 2);
  CHECK(score.kill_streak_enemy_count == 0);
  // 100 + 0 + 3 * 100 / 2 + 4 * 100 / 2.
  CHECK(score.streak_score == 450);
  CHECK(score.enemy_kill_flags[1] == 1U << 5);
}

TEST_CASE("Repeated kills extend a streak; a different class restarts it",
          "[state-boot]") {
  KillScore score;
  score.enemy_kills[2] = 1;
  score.enemy_kill_flags[1] = 1U << 5;
  score.kill_streak_enemy_class = 2;
  score.streak_score = 450;
  score.weapon_kill_streak = 3;
  score.chain_streak = 4;
  const state::DefeatedEnemy enemy{
      .sub_type = 37,
      .scored = state::ScoredEnemy{.enemy_class = 2, .max_hp = 100}};
  CHECK(state::RecordEnemyKill(score, enemy));
  CHECK(score.enemy_kills[2] == 2);
  CHECK(score.kill_streak_enemy_count == 1);
  CHECK(score.streak_score == 450 + 500);

  const state::DefeatedEnemy other{
      .sub_type = 255,
      .scored = state::ScoredEnemy{.enemy_class = 4, .max_hp = 10}};
  CHECK(state::RecordEnemyKill(score, other));
  CHECK(score.kill_streak_enemy_class == 4);
  CHECK(score.kill_streak_enemy_count == 0);
  CHECK(score.enemy_kill_flags[7] == 1U << 31);
}

TEST_CASE("Kill counts, streaks and the streak score saturate",
          "[state-boot]") {
  KillScore score;
  score.enemy_kills[1] = state::kMaxEnemyClassKills;
  score.kill_streak_enemy_class = 1;
  score.kill_streak_enemy_count = state::kMaxStreak;
  score.streak_score = state::kMaxStreakScore - 10;
  CHECK(state::RecordEnemyKill(
      score, {.sub_type = 0,
              .scored = state::ScoredEnemy{.enemy_class = 1, .max_hp = 50}}));
  CHECK(score.enemy_kills[1] == state::kMaxEnemyClassKills);
  CHECK(score.kill_streak_enemy_count == state::kMaxStreak);
  CHECK(score.streak_score == state::kMaxStreakScore);
}

TEST_CASE("An unscored kill only sets its kill flag", "[state-boot]") {
  KillScore score;
  CHECK(state::RecordEnemyKill(score, {.sub_type = 64, .scored = {}}));
  KillScore expected;
  expected.enemy_kill_flags[2] = 1;
  CHECK(score == expected);

  CHECK(state::RecordEnemyKill(score, {.sub_type = static_cast<std::uint16_t>(
                                           state::kEnemyKillFlagCount),
                                       .scored = {}}));
  CHECK(score == expected);
}

TEST_CASE("An uncounted enemy class is refused but still scores",
          "[state-boot]") {
  KillScore score;
  CHECK_FALSE(state::RecordEnemyKill(
      score, {.sub_type = 3,
              .scored = state::ScoredEnemy{.enemy_class = 6, .max_hp = 40}}));
  CHECK(score.enemy_kills == decltype(score.enemy_kills){});
  CHECK(score.kill_streak_enemy_class == 6);
  CHECK(score.streak_score == 40);
  CHECK(score.enemy_kill_flags[0] == 1U << 3);
}

TEST_CASE("Weapon actions extend the weapon kill streak per blade category",
          "[state-boot]") {
  STATIC_REQUIRE(state::ExtendsWeaponKillStreak(0));
  STATIC_REQUIRE(state::ExtendsWeaponKillStreak(39));
  STATIC_REQUIRE_FALSE(state::ExtendsWeaponKillStreak(40));

  KillScore score;
  score.weapon_kill_streak_blade_category = 3;
  score.weapon_kill_streak = 7;
  state::UpdateWeaponKillStreak(score, 40, 3);
  CHECK(score.weapon_kill_streak == 7);
  state::UpdateWeaponKillStreak(score, 12, 3);
  CHECK(score.weapon_kill_streak == 8);
  state::UpdateWeaponKillStreak(score, 12, 5);
  CHECK(score.weapon_kill_streak_blade_category == 5);
  CHECK(score.weapon_kill_streak == 0);

  score.weapon_kill_streak = state::kMaxStreak;
  state::UpdateWeaponKillStreak(score, 1, 5);
  CHECK(score.weapon_kill_streak == state::kMaxStreak);
}

TEST_CASE("Live script flags read and write the flag record by halfword id",
          "[state-boot]") {
  LiveTimerState saved;
  ChannelWarnings warnings("state");
  VS::State::SetLiveScriptFlag(0x91, 0x1C2);
  CHECK(VS::State::LiveScriptFlag(0x91) == 0xC2);
  // The id is the original's signed halfword.
  CHECK(VS::State::LiveScriptFlag(0x10091) == 0xC2);
  CHECK(warnings.messages.empty());

  VS::State::SetLiveScriptFlag(kLastFlag + 1, 1);
  CHECK(VS::State::LiveScriptFlag(0xFFFF) == 0);
  CHECK(
      warnings.messages ==
      std::vector<std::string>{
          Refused("SetScriptFlag", "flag 1088 is outside the 1088 flag bytes"),
          Refused("ScriptFlag", "flag -1 is outside the 1088 flag bytes")});

  VS::State::SetLiveScriptFlag(state::kFirstZoneScriptFlag, 5);
  VS::State::ResetLiveZoneScriptFlags(1);
  CHECK(VS::State::LiveScriptFlag(state::kFirstZoneScriptFlag) == 5);
  VS::State::ResetLiveZoneScriptFlags(0);
  CHECK(VS::State::LiveScriptFlag(state::kFirstZoneScriptFlag) == 0);
}

TEST_CASE("Starting a live scene timer updates its record and HUD",
          "[state-boot]") {
  LiveTimerState saved;
  Battle_lowerScreenUiState = 0;
  VS::State::StartLiveSceneTimer(kCountDown, 3, 0x0104);
  CHECK(State_sceneTimer.mode == kCountDown);
  CHECK(State_sceneTimer.resetScope == 3);
  CHECK(State_sceneTimer.frames == 4);
  CHECK(State_sceneTimer.seconds == 1);
  CHECK(VS::State::LiveScriptFlag(state::kSceneTimerSecondsFlag) == 1);
  CHECK(Battle_lowerScreenUiState == 1);

  CHECK(VS::State::SetLiveTickSpeed(state::kNormalTickSpeed) ==
        state::kNormalTickSpeed);
  CHECK(VS::State::SetLiveTickSpeed(3) == state::kNormalTickSpeed);
  CHECK(State_tickSpeed == state::kNormalTickSpeed);
}

TEST_CASE("Pausing a live scene timer stores its mode and resume restores it",
          "[state-boot]") {
  LiveTimerState saved;
  State_sceneTimer = {.mode = static_cast<char>(kCountDown),
                      .pausedMode = static_cast<char>(kCountDown),
                      .resetScope = 3,
                      .frames = 4,
                      .seconds = 1,
                      .minutes = 0,
                      .hours = 0};
  Battle_lowerScreenUiState = 1;
  VS::State::SetLiveSceneTimerPaused(1);
  CHECK(State_sceneTimer.mode == state::kSceneTimerStopped);
  CHECK(State_sceneTimer.pausedMode == kCountDown);
  CHECK(Battle_lowerScreenUiState == 0);
  CHECK(VS::State::UpdateLiveSceneTimer() == 0);
  CHECK(State_sceneTimer.frames == 4);
  VS::State::SetLiveSceneTimerPaused(0);
  CHECK(State_sceneTimer.mode == kCountDown);
}

TEST_CASE("Resetting a live timer clears time and keeps pause state",
          "[state-boot]") {
  LiveTimerState saved;
  State_sceneTimer = {.mode = static_cast<char>(kCountDown),
                      .pausedMode = static_cast<char>(kCountDown),
                      .resetScope = 3,
                      .frames = 4,
                      .seconds = 1,
                      .minutes = 0,
                      .hours = 0};
  Battle_lowerScreenUiState = 1;
  VS::State::ResetLiveSceneTimer();
  CHECK(State_sceneTimer.mode == state::kSceneTimerStopped);
  CHECK(State_sceneTimer.pausedMode == kCountDown);
  CHECK(State_sceneTimer.resetScope == 3);
  CHECK(State_sceneTimer.frames == 0);
  CHECK(State_sceneTimer.seconds == 0);
  CHECK(Battle_lowerScreenUiState == 0);
}

TEST_CASE("An ended live countdown evaluates the room expiry triggers",
          "[state-boot]") {
  LiveTimerExpiry test;
  test.Start(4);
  CHECK(VS::State::UpdateLiveSceneTimer() == 0);
  CHECK(State_sceneTimer.frames == 2);
  CHECK(Event_playbackPhase == 0);

  CHECK(VS::State::UpdateLiveSceneTimer() == 1);
  CHECK(State_sceneTimer.frames == 0);
  CHECK(State_sceneTimer.mode == kCountDown);
  CHECK(Event_eventId == LiveTimerExpiry::kExpiryEvent);
  CHECK(Event_playbackPhase == 1);
}

TEST_CASE("A busy event player does not consume timer expiry", "[state-boot]") {
  LiveTimerExpiry test;
  test.Start(4);
  REQUIRE(VS::State::UpdateLiveSceneTimer() == 0);
  REQUIRE(VS::State::UpdateLiveSceneTimer() == 1);
  Event_isDirectEventRequest = 1;
  CHECK(VS::State::UpdateLiveSceneTimer() == 0);
  CHECK(Event_eventId == LiveTimerExpiry::kExpiryEvent);
}

TEST_CASE("The live play time record round-trips", "[state-boot]") {
  const State_PlayTime saved = State_playTime;
  const PlayTime time{.frames = 6, .seconds = 7, .minutes = 8, .hours = 99};
  VS::State::StoreLivePlayTime(time);
  CHECK(State_playTime.t.hours == 99);
  CHECK(VS::State::LivePlayTime() == time);
  State_playTime = saved;
}

TEST_CASE("Live kill scoring records hostile and unscored kills",
          "[state-boot]") {
  LiveScoreState saved;
  ChannelWarnings warnings("state");
  VS::State::StoreLiveKillScore(KillScore{});

  Battle_ActorStats stats{};
  stats.enemyClass = 3;
  stats.maxHp = 80;
  Battle_Actor enemy{};
  enemy.subType = 40;
  enemy.kind = 4;
  enemy.stats = &stats;
  VS::State::RecordLiveEnemyKillScore(&enemy);
  KillScore score = VS::State::LiveKillScore();
  CHECK(score.enemy_kills[3] == 1);
  CHECK(score.streak_score == 80);
  CHECK(score.enemy_kill_flags[1] == 1U << 8);

  // A non-hostile actor only sets its kill flag.
  enemy.kind = 2;
  enemy.subType = 41;
  VS::State::RecordLiveEnemyKillScore(&enemy);
  score = VS::State::LiveKillScore();
  CHECK(score.enemy_kills[3] == 1);
  CHECK(score.streak_score == 80);
  CHECK(score.enemy_kill_flags[1] == (1U << 8 | 1U << 9));
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live kill scoring refuses an uncounted class and null enemy",
          "[state-boot]") {
  LiveScoreState saved;
  ChannelWarnings warnings("state");
  VS::State::StoreLiveKillScore(KillScore{});

  Battle_ActorStats stats{};
  stats.enemyClass = 7;
  stats.maxHp = 40;
  Battle_Actor enemy{};
  enemy.subType = 41;
  enemy.kind = 4;
  enemy.stats = &stats;
  VS::State::RecordLiveEnemyKillScore(&enemy);
  VS::State::RecordLiveEnemyKillScore(nullptr);
  const KillScore score = VS::State::LiveKillScore();
  CHECK(score.enemy_kills == decltype(score.enemy_kills){});
  CHECK(score.kill_streak_enemy_class == 7);
  CHECK(score.streak_score == 40);
  CHECK(score.enemy_kill_flags[1] == 1U << 9);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "RecordEnemyKillScore: enemy class 7 of sub-type 41 is outside "
            "the 6 counted classes; its count was refused",
            Refused("RecordEnemyKillScore", "no defeated enemy")});
}

TEST_CASE("The live weapon kill streak reads the action and the player",
          "[state-boot]") {
  LiveScoreState saved;
  ChannelWarnings warnings("state");
  VS::State::StoreLiveKillScore(KillScore{});

  Battle_actionState = nullptr;
  VS::State::UpdateLiveWeaponKillStreak(0);
  Battle_ActionState action{};
  Battle_actionState = &action;
  Battle_characterState = nullptr;
  action.selection.actionId = 50;
  VS::State::UpdateLiveWeaponKillStreak(0);
  CHECK(warnings.messages == std::vector<std::string>{Refused(
                                 "UpdateWeaponKillStreak", "no action state")});
  action.selection.actionId = 5;
  VS::State::UpdateLiveWeaponKillStreak(0);

  Battle_ActorStats stats{};
  stats.weapon.blade.category = 2;
  Battle_Actor player{};
  player.stats = &stats;
  Battle_characterState = &player;
  VS::State::UpdateLiveWeaponKillStreak(0);
  VS::State::UpdateLiveWeaponKillStreak(0);
  const KillScore score = VS::State::LiveKillScore();
  CHECK(score.weapon_kill_streak_blade_category == 2);
  CHECK(score.weapon_kill_streak == 1);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("UpdateWeaponKillStreak", "no action state"),
            Refused("UpdateWeaponKillStreak", "no player stats")});
}
