#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/RewardFlow.hpp"
#include "Battle/RewardPresentationRecorder.hpp"

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
using LootId = VS::Menu::Framework::LootListId;
enum class Op {
  kReadPhase,
  kSetLoot,
  kSetFollow,
  kEffects,
  kUpdate,
  kStatus,
  kNoticePending,
  kPhase,
  kPaused,
  kFlag,
  kPending,
  kSelection,
  kDefinition,
  kUnlock,
  kRoomPalette,
  kSceneColors,
  kClut,
  kUnfreeze,
  kFreeze,
  kRoom,
  kMinimap,
  kAmbience,
  kIdle,
  kPose,
  kEffect,
  kPlatforms,
  kNotice,
  kAbilities,
  kLootHead,
  kLoot,
  kResults,
  kRows,
  kReleaseNotice,
  kOldResume,
  kOldBegin,
  kOldGrant,
};
struct Event {
  Op op{};
  Word value{};
  bool operator==(const Event&) const = default;
};
template <typename T>
T Value(const std::optional<T>& source) {
  if (!source.has_value()) {
    throw std::bad_optional_access{};
  }
  return source.value();
}
struct World final
    : reward_tests::RewardPresentationRecorder<b::RewardDispatchWorld, Op> {
  Word phase = 99, paused = 77, effects = 3, pending = 0;
  std::optional<std::int16_t> menu{1};
  std::optional<std::int32_t> notification{0}, follow{0}, loot_opened{0};
  std::optional<b::RewardSelection> selection{b::RewardSelection{}};
  std::array<bool, 256> definitions{};
  std::uint8_t unlock_flag = 1, reward_flag = 0;
  LootId loot_head{};
  std::size_t phase_reads{}, selection_reads{}, status_reads{};
  std::vector<Event> events;
  std::function<void(Event)> callback;
  bool reset_before_menu = false;

  void Record(Event event) {
    events.push_back(event);
    if (callback) {
      callback(event);
    }
  }
  void RecordPresentation(Op operation, std::uint32_t value) override {
    Record({operation, value});
  }
  std::vector<Op> Operations() const {
    std::vector<Op> result;
    for (const auto& event : events) {
      result.push_back(event.op);
    }
    return result;
  }
  std::size_t Count(Op op) const {
    return static_cast<std::size_t>(
        std::count_if(events.begin(), events.end(),
                      [op](Event event) { return event.op == op; }));
  }
  Word CurrentPhase() override {
    ++phase_reads;
    const Word value = phase;
    Record({Op::kReadPhase, value});
    return value;
  }
  void SetLootMenuOpened(std::int32_t value) override {
    loot_opened = value;
    Record({Op::kSetLoot, static_cast<Word>(value)});
  }
  void SetFollowAction(std::int32_t value) override {
    follow = value;
    Record({Op::kSetFollow, static_cast<Word>(value)});
  }
  void UpdateMenu() override { Record({Op::kUpdate}); }
  std::int16_t MenuStatus() override {
    ++status_reads;
    Record({Op::kStatus});
    return Value(menu);
  }
  Word EffectsBusyFlags() override {
    const Word value = effects;
    Record({Op::kEffects, value});
    return value;
  }
  std::int32_t SceneNotificationPending() override {
    const auto value = Value(notification);
    Record({Op::kNoticePending, static_cast<Word>(value)});
    return value;
  }
  std::int32_t FollowAction() override { return Value(follow); }
  std::int32_t LootMenuOpened() override { return Value(loot_opened); }
  void ResumeFieldAfterMenu() override { Record({Op::kOldResume}); }
  void ReleaseMenuRows() override { Record({Op::kRows}); }
  void ReleaseSceneNotification() override { Record({Op::kReleaseNotice}); }
  void BeginPendingAbilityReward() override { Record({Op::kOldBegin}); }
  void GrantPendingAbilityUnlock() override { Record({Op::kOldGrant}); }
  void SetPhase(b::RewardPhase value) override {
    phase = static_cast<Word>(value);
    Record({Op::kPhase, phase});
  }
  void SetInteractionPaused(bool value) override {
    paused = value ? 1U : 0U;
    Record({Op::kPaused, paused});
  }
  std::uint8_t ScriptFlag(int id) override {
    if (id != 0xAB && id != 0xB6) {
      throw std::out_of_range("unexpected story flag");
    }
    const auto value = id == 0xAB ? reward_flag : unlock_flag;
    Record({Op::kFlag, static_cast<Word>(id)});
    return value;
  }
  Word PendingUnlocks() override {
    const Word value = pending;
    Record({Op::kPending, value});
    return value;
  }
  b::RewardSelection Selection() override {
    ++selection_reads;
    Record({Op::kSelection});
    return Value(selection);
  }
  bool DefinitionUnlocked(std::uint16_t action) override {
    Record({Op::kDefinition, action});
    return definitions.at(action);
  }
  void MarkDefinitionUnlocked(std::uint16_t action) override {
    definitions.at(action) = true;
    Record({Op::kUnlock, action});
  }
  void RoomPalettes(const VS::Field::ColorTransform&) override {
    Record({Op::kRoomPalette});
  }
  void SceneColors(const VS::Field::ColorTransform&) override {
    Record({Op::kSceneColors});
  }
  void ShowActionLearned(std::uint16_t action) override {
    Record({Op::kNotice, action});
  }
  void OpenAbilityUnlocks(std::uint16_t art, std::uint16_t ability) override {
    Record({Op::kAbilities, static_cast<Word>(art) << 16 | ability});
  }
  LootId LootHead() override {
    Record({Op::kLootHead});
    return loot_head;
  }
  void OpenLoot(LootId owner) override {
    Record({Op::kLoot, static_cast<Word>(owner)});
  }
  void OpenResults() override { Record({Op::kResults}); }
  void PrepareStartCallback(Op menu_op) {
    loot_opened = INT32_MIN;
    follow = INT32_MIN;
    reward_flag = 3;
    loot_head = LootId{17};
    callback = [this, menu_op](Event event) {
      if (event.op == menu_op) {
        reset_before_menu = Value(loot_opened) == 0 &&
                            Value(follow) == (menu_op == Op::kLoot ? 1 : 0);
        loot_opened = 0x10000;
        follow = INT32_MIN;
        loot_head = LootId{23};
      }
    };
  }
};

TEST_CASE("F00225 busy effects keep all unused owners lazy",
          "[reward-dispatch][F00225]") {
  World world;
  world.effects = GENERATE(1U, 2U, 3U, 0xFFFFFFFFU, 0x10001U, 0x80000002U);
  world.menu.reset();
  world.notification.reset();
  world.selection.reset();
  b::WaitForRewardUnlockEffects(world);
  CHECK(world.events == std::vector<Event>{{Op::kEffects, world.effects}});
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
}

TEST_CASE("F00225 clear low bits open the actual reward unlock policy",
          "[reward-dispatch][F00225]") {
  World world;
  world.effects = GENERATE(0U, 4U, 0x10000U, 0x80000000U, 0xFFFFFFFCU);
  world.menu.reset();
  world.notification.reset();
  world.pending = 0xABCD9876;
  b::WaitForRewardUnlockEffects(world);
  CHECK(world.events.front() == Event{Op::kEffects, world.effects});
  CHECK(world.events.back() == Event{Op::kAbilities, 0xABCD9876});
  CHECK(world.phase == 6);
  CHECK(world.paused == 1);
  CHECK(world.Count(Op::kOldGrant) == 0);
}

TEST_CASE("F00225 notice and pause callbacks select the current unlock target",
          "[reward-dispatch][F00225]") {
  World world;
  world.effects = 4;
  world.selection = b::RewardSelection{.action_id = 1};
  world.callback = [&](Event event) {
    if (event.op == Op::kRoomPalette) {
      world.selection = b::RewardSelection{.action_id = 255};
    }
    if (event.op == Op::kNotice) {
      world.selection = b::RewardSelection{};
    }
  };
  b::WaitForRewardUnlockEffects(world);
  CHECK(world.Count(Op::kNotice) == 1);
  CHECK(std::find(world.events.begin(), world.events.end(),
                  Event{Op::kNotice, 255}) != world.events.end());
  CHECK(world.definitions.at(0));
  CHECK_FALSE(world.definitions.at(1));
  CHECK_FALSE(world.definitions.at(255));
  CHECK(world.selection_reads == 3);
}

TEST_CASE("F00225 clear effects propagate a missing used action",
          "[reward-dispatch][F00225]") {
  World world;
  world.effects = 4;
  world.selection.reset();
  CHECK_THROWS_AS(b::WaitForRewardUnlockEffects(world),
                  std::bad_optional_access);
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
}

TEST_CASE("F00226 notification remains lazy behind the current busy menu",
          "[reward-dispatch][F00226]") {
  World world;
  world.menu = -1;
  world.notification.reset();
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) {
      world.menu = 1;
    }
  };
  b::UpdateRewardUnlockDialog(world);
  CHECK(world.Operations() == std::vector<Op>{Op::kUpdate, Op::kStatus});
  CHECK(world.paused == 77);
  CHECK(world.phase == 99);
}

TEST_CASE("F00226 every nonzero full notification result blocks cleanup",
          "[reward-dispatch][F00226]") {
  World world;
  world.menu =
      GENERATE(std::int16_t{-1}, std::int16_t{257}, std::int16_t{-32768});
  world.notification = GENERATE(-1, 1, 0x10000, INT32_MIN);
  world.selection.reset();
  b::UpdateRewardUnlockDialog(world);
  CHECK(world.Operations() ==
        std::vector<Op>{Op::kUpdate, Op::kStatus, Op::kNoticePending});
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
}

TEST_CASE("F00226 cleanup uses reward end then rows notice and fresh unlocks",
          "[reward-dispatch][F00226]") {
  World world;
  world.menu = 257;
  world.callback = [&](Event event) {
    if (event.op == Op::kNoticePending) {
      world.menu.reset();
      world.notification = -1;
    }
    if (event.op == Op::kRows) {
      world.pending = 0;
    }
    if (event.op == Op::kReleaseNotice) {
      world.unlock_flag = 0;
      world.pending = 0x10000;
      world.selection.reset();
    }
  };
  b::UpdateRewardUnlockDialog(world);
  CHECK(world.Operations() ==
        std::vector<Op>{Op::kUpdate, Op::kStatus, Op::kNoticePending, Op::kClut,
                        Op::kSceneColors, Op::kFreeze, Op::kRoom, Op::kAmbience,
                        Op::kPaused, Op::kRows, Op::kReleaseNotice, Op::kFlag,
                        Op::kPending, Op::kPhase, Op::kPlatforms});
  CHECK(world.status_reads == 1);
  CHECK(world.selection_reads == 0);
  CHECK(world.phase == 5);
  CHECK(world.paused == 0);
  CHECK(world.Count(Op::kOldResume) == 0);
}

TEST_CASE("F00226 refuses a removed current owner without silent completion",
          "[reward-dispatch][F00226]") {
  World world;
  const bool lost_menu = GENERATE(false, true);
  world.menu = 0;
  world.callback = [&](Event event) {
    if (lost_menu && event.op == Op::kUpdate) {
      world.menu.reset();
    }
    if (!lost_menu && event.op == Op::kReleaseNotice) {
      world.selection.reset();
    }
  };
  CHECK_THROWS_AS(b::UpdateRewardUnlockDialog(world), std::bad_optional_access);
  CHECK(world.events.back().op == (lost_menu ? Op::kStatus : Op::kSelection));
  CHECK(world.phase == 99);
}

TEST_CASE("F00227 full reset order precedes the actual results callback",
          "[reward-dispatch][F00227]") {
  World world;
  world.PrepareStartCallback(Op::kResults);
  b::BeginEventRewardResults(world);
  CHECK(world.events.at(0) == Event{Op::kSetLoot, 0});
  CHECK(world.events.at(1) == Event{Op::kSetFollow, 0});
  CHECK(world.reset_before_menu);
  CHECK(world.phase == 3);
}

TEST_CASE(
    "F00227 results callback full word edits survive without a second reset",
    "[reward-dispatch][F00227]") {
  World world;
  world.PrepareStartCallback(Op::kResults);
  b::BeginEventRewardResults(world);
  CHECK(Value(world.loot_opened) == 0x10000);
  CHECK(Value(world.follow) == INT32_MIN);
  CHECK(world.Count(Op::kSetLoot) == 1);
  CHECK(world.Count(Op::kSetFollow) == 1);
}

TEST_CASE("F00227 low story stage follows actual loot fallback and resume",
          "[reward-dispatch][F00227]") {
  World world;
  world.loot_opened = -1;
  world.follow = -1;
  world.selection.reset();
  b::BeginEventRewardResults(world);
  CHECK(world.phase == 2);
  CHECK(world.paused == 0);
  CHECK(Value(world.follow) == 0);
  CHECK(Value(world.loot_opened) == 0);
  CHECK(world.Count(Op::kFlag) == 2);
  CHECK(world.Count(Op::kResults) == 0);
  CHECK(world.Count(Op::kIdle) == 0);
}

TEST_CASE("F00228 reset order precedes the actual pending loot callback",
          "[reward-dispatch][F00228]") {
  World world;
  world.PrepareStartCallback(Op::kLoot);
  b::BeginPostActionRewards(world);
  CHECK(world.events.at(0) == Event{Op::kSetLoot, 0});
  CHECK(world.events.at(1) == Event{Op::kSetFollow, 1});
  CHECK(world.reset_before_menu);
  CHECK(world.phase == 4);
}

TEST_CASE("F00228 loot callback full word edits survive without a second reset",
          "[reward-dispatch][F00228]") {
  World world;
  world.PrepareStartCallback(Op::kLoot);
  b::BeginPostActionRewards(world);
  CHECK(Value(world.loot_opened) == 0x10000);
  CHECK(Value(world.follow) == INT32_MIN);
  CHECK(world.Count(Op::kSetFollow) == 1);
  CHECK(world.Count(Op::kResults) == 0);
}

TEST_CASE("F00228 post action resume rereads follow after end callbacks",
          "[reward-dispatch][F00228]") {
  World world;
  const bool clear_follow = GENERATE(false, true);
  world.selection.reset();
  world.callback = [&](Event event) {
    if (clear_follow && event.op == Op::kAmbience) {
      world.follow = 0;
    }
  };
  b::BeginPostActionRewards(world);
  CHECK(world.phase == 2);
  CHECK(world.paused == 0);
  CHECK(Value(world.loot_opened) == 0);
  CHECK(Value(world.follow) == (clear_follow ? 0 : 1));
  CHECK(world.Count(Op::kIdle) == (clear_follow ? 0U : 1U));
  CHECK(world.Count(Op::kSetFollow) == 1);
}

TEST_CASE("F00229 unknown full phases need no handler owners",
          "[reward-dispatch][F00229]") {
  World world;
  world.phase = GENERATE(0U, 1U, 2U, 7U, 0x10003U, 0x10004U, 0x10005U, 0x10006U,
                         0x80000003U, 0xFFFFFFFFU, 0x80000000U);
  world.menu.reset();
  world.notification.reset();
  world.selection.reset();
  world.follow.reset();
  world.loot_opened.reset();
  const Word original = world.phase;
  CHECK(b::UpdateRewardSequence(world) == 0);
  CHECK(world.events == std::vector<Event>{{Op::kReadPhase, original}});
  CHECK(world.phase_reads == 1);
  CHECK(world.phase == original);
}

TEST_CASE("F00229 menu phases execute one handler despite callback phase edits",
          "[reward-dispatch][F00229]") {
  World world;
  world.phase = GENERATE(3U, 4U, 6U);
  world.notification.reset();
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) {
      world.phase = 5;
    }
  };
  CHECK(b::UpdateRewardSequence(world) == 1);
  CHECK(world.Operations() ==
        std::vector<Op>{Op::kReadPhase, Op::kUpdate, Op::kStatus});
  CHECK(world.phase == 5);
  CHECK(world.phase_reads == 1);
}

TEST_CASE("F00229 effects handler uses the entry snapshot and returns one",
          "[reward-dispatch][F00229]") {
  World world;
  world.phase = 5;
  world.effects = 2;
  world.menu.reset();
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kReadPhase) {
      world.phase = 6;
    }
  };
  CHECK(b::UpdateRewardSequence(world) == 1);
  CHECK(world.Operations() == std::vector<Op>{Op::kReadPhase, Op::kEffects});
  CHECK(world.phase == 6);
  CHECK(world.phase_reads == 1);
}

TEST_CASE(
    "F00229 handled completion still returns one when phase becomes resume",
    "[reward-dispatch][F00229]") {
  World world;
  world.phase = GENERATE(3U, 4U, 6U);
  world.menu = 0;
  world.unlock_flag = 0;
  CHECK(b::UpdateRewardSequence(world) == 1);
  CHECK(world.phase == 2);
  CHECK(world.paused == 0);
  CHECK(world.phase_reads == 1);
  CHECK(world.status_reads == 1);
  CHECK(world.Count(Op::kOldResume) == 0);
}
}  // namespace
