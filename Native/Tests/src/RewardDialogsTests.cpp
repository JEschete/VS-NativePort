#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/RewardFlow.hpp"

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
enum class Op {
  kUpdate,
  kStatus,
  kEffects,
  kNotification,
  kResumeField,
  kReleaseRows,
  kReleaseNotification,
  kBeginAbilityReward,
  kGrantAbilityUnlock,
  kFollow,
  kLootOpened,
  kPhase,
  kPaused,
  kFlag,
  kPending,
  kSelection,
  kDefinition,
  kClut,
  kSceneColors,
  kFreeze,
  kRoom,
  kAmbience,
  kIdle,
  kPose,
  kEffect,
  kPlatforms,
};
struct Event {
  Op op{};
  Word value{};
  bool operator==(const Event&) const = default;
};
template <class T>
const T& ReadPresent(const std::optional<T>& value) {
  if (!value.has_value()) {
    throw std::bad_optional_access();
  }
  return value.value();
}
struct World final : b::RewardDialogWorld {
  Word phase = 99, paused = 77, effects = 3;
  std::optional<std::int16_t> menu{1};
  std::optional<std::int32_t> notification{0}, follow{0}, loot_opened{0};
  std::optional<Word> ability_owner{17};
  std::optional<b::RewardSelection> selection{b::RewardSelection{}};
  std::array<bool, 256> definitions{};
  std::uint8_t flag = 1;
  Word pending{};
  std::size_t selection_reads{}, definition_reads{};
  std::vector<Event> events;
  std::function<void(Event)> callback;

  void Record(Event event) {
    events.push_back(event);
    if (callback) callback(event);
  }
  void ConfigureTwoResumes(std::int32_t initial_follow, std::size_t& ends) {
    selection.reset();
    callback = [this, initial_follow, &ends](Event event) {
      if (event.op == Op::kUpdate) {
        menu = -1;
        follow = initial_follow;
      }
      if (event.op == Op::kReleaseRows) {
        loot_opened = INT32_MIN;
        follow = 1;
      }
      if (event.op == Op::kAmbience && ++ends == 2) {
        follow = 0;
      }
    };
  }
  void UpdateMenu() override { Record({Op::kUpdate}); }
  std::int16_t MenuStatus() override {
    Record({Op::kStatus});
    return ReadPresent(menu);
  }
  Word EffectsBusyFlags() override {
    const Word value = effects;
    Record({Op::kEffects, value});
    return value;
  }
  std::int32_t SceneNotificationPending() override {
    const auto value = ReadPresent(notification);
    Record({Op::kNotification, static_cast<Word>(value)});
    return value;
  }
  std::int32_t FollowAction() override {
    const auto value = ReadPresent(follow);
    Record({Op::kFollow, static_cast<Word>(value)});
    return value;
  }
  std::int32_t LootMenuOpened() override {
    const auto value = ReadPresent(loot_opened);
    Record({Op::kLootOpened, static_cast<Word>(value)});
    return value;
  }
  void ResumeFieldAfterMenu() override { Record({Op::kResumeField}); }
  void ReleaseMenuRows() override { Record({Op::kReleaseRows}); }
  void ReleaseSceneNotification() override {
    Record({Op::kReleaseNotification});
  }
  void BeginPendingAbilityReward() override {
    Record({Op::kBeginAbilityReward, ReadPresent(ability_owner)});
  }
  void GrantPendingAbilityUnlock() override {
    Record({Op::kGrantAbilityUnlock, ReadPresent(ability_owner)});
  }
  void SetPhase(b::RewardPhase value) override {
    phase = static_cast<Word>(value);
    Record({Op::kPhase, phase});
  }
  void SetInteractionPaused(bool value) override {
    paused = value ? 1U : 0U;
    Record({Op::kPaused, paused});
  }
  std::uint8_t ScriptFlag(int) override {
    Record({Op::kFlag, flag});
    return flag;
  }
  Word PendingUnlocks() override {
    Record({Op::kPending, pending});
    return pending;
  }
  b::RewardSelection Selection() override {
    ++selection_reads;
    Record({Op::kSelection});
    return ReadPresent(selection);
  }
  bool DefinitionUnlocked(std::uint16_t action) override {
    ++definition_reads;
    Record({Op::kDefinition, action});
    return definitions.at(action);
  }
  void MarkDefinitionUnlocked(std::uint16_t action) override {
    definitions.at(action) = true;
  }
  void RoomPalettes(const VS::Field::ColorTransform&) override {}
  void SceneColors(const VS::Field::ColorTransform&) override {
    Record({Op::kSceneColors});
  }
  void EnableClutAnimation(bool value) override {
    Record({Op::kClut, value ? 1U : 0U});
  }
  void FreezeActorAnimations() override { Record({Op::kFreeze}); }
  void UnfreezeActorAnimations() override {}
  void PauseRoomAnimations(bool value) override {
    Record({Op::kRoom, value ? 1U : 0U});
  }
  void ShowMinimap(bool) override {}
  void DuckAmbience(bool value) override {
    Record({Op::kAmbience, value ? 1U : 0U});
  }
  void RestoreIdlePose() override { Record({Op::kIdle}); }
  void PlayLearnedPose() override { Record({Op::kPose}); }
  void SpawnLearnedEffect() override { Record({Op::kEffect}); }
  void PauseMovingPlatforms() override { Record({Op::kPlatforms}); }
  void ShowActionLearned(std::uint16_t) override {}
  void OpenAbilityUnlocks(std::uint16_t, std::uint16_t) override {}
  VS::Menu::Framework::LootListId LootHead() override { return {}; }
  void OpenLoot(VS::Menu::Framework::LootListId) override {}
  void OpenResults() override {}
};

TEST_CASE("F00196 resolves a busy owner only after updating the overlay",
          "[reward-dialogs][F00196]") {
  World world;
  world.menu.reset();
  world.ability_owner.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) world.menu = 1;
  };
  b::UpdateLootResults(world);
  CHECK(world.events == std::vector<Event>{{Op::kUpdate}, {Op::kStatus}});
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
}

TEST_CASE("F00196 resumes the field before rows and current ability reward",
          "[reward-dialogs][F00196]") {
  World world;
  const auto status =
      GENERATE(std::int16_t{-32768}, std::int16_t{-1}, std::int16_t{0},
               std::int16_t{2}, std::int16_t{257});
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) world.menu = status;
    if (event.op == Op::kResumeField) world.ability_owner = 21;
    if (event.op == Op::kReleaseRows) world.ability_owner = 43;
  };
  b::UpdateLootResults(world);
  CHECK(world.events == std::vector<Event>{{Op::kUpdate},
                                           {Op::kStatus},
                                           {Op::kResumeField},
                                           {Op::kReleaseRows},
                                           {Op::kBeginAbilityReward, 43}});
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
  CHECK(world.selection_reads == 0);
}

TEST_CASE("F00196 propagates missing current menu and ability owners",
          "[reward-dialogs][F00196]") {
  World world;
  const bool lost_menu = GENERATE(false, true);
  world.menu = 0;
  world.callback = [&](Event event) {
    if (lost_menu && event.op == Op::kUpdate) world.menu.reset();
    if (!lost_menu && event.op == Op::kReleaseRows) world.ability_owner.reset();
  };
  CHECK_THROWS_AS(b::UpdateLootResults(world), std::bad_optional_access);
  CHECK(world.events.back().op == (lost_menu ? Op::kStatus : Op::kReleaseRows));
  CHECK(world.phase == 99);
}

TEST_CASE("F00197 tests only the two low effect bits without menu owners",
          "[reward-dialogs][F00197]") {
  World world;
  const Word flags = GENERATE(0U, 1U, 2U, 3U, 4U, 0x10000U, 0x80000000U,
                              0xFFFFFFFCU, 0xFFFFFFFFU);
  const bool busy = (flags & 3U) != 0;
  world.effects = flags;
  world.menu.reset();
  world.notification.reset();
  if (busy) world.ability_owner.reset();
  b::UpdateAbilityUnlockDelay(world);
  CHECK(world.events.front() == Event{Op::kEffects, flags});
  CHECK(world.events.size() == (busy ? 1U : 2U));
  if (!busy) CHECK(world.events.back() == Event{Op::kGrantAbilityUnlock, 17});
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
}

TEST_CASE("F00197 does not swallow the actual unlock service refusal",
          "[reward-dialogs][F00197]") {
  World world;
  world.effects = 4;
  world.ability_owner.reset();
  CHECK_THROWS_AS(b::UpdateAbilityUnlockDelay(world), std::bad_optional_access);
  CHECK(world.events == std::vector<Event>{{Op::kEffects, 4}});
  CHECK(world.phase == 99);
}

TEST_CASE("F00198 keeps the notice query lazy behind the current busy menu",
          "[reward-dialogs][F00198]") {
  World world;
  world.menu = -1;
  world.notification.reset();
  world.ability_owner.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) world.menu = 1;
  };
  b::UpdateAbilityUnlockResults(world);
  CHECK(world.events == std::vector<Event>{{Op::kUpdate}, {Op::kStatus}});
  CHECK(world.paused == 77);
}

TEST_CASE("F00198 waits on any nonzero current notification result",
          "[reward-dialogs][F00198]") {
  World world;
  const auto pending = GENERATE(-1, 1, 0x10000, INT32_MIN);
  world.ability_owner.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) {
      world.menu = -1;
      world.notification = pending;
    }
  };
  b::UpdateAbilityUnlockResults(world);
  CHECK(world.events ==
        std::vector<Event>{{Op::kUpdate},
                           {Op::kStatus},
                           {Op::kNotification, static_cast<Word>(pending)}});
  CHECK(world.phase == 99);
}

TEST_CASE("F00198 releases notice after rows before the fresh ability owner",
          "[reward-dialogs][F00198]") {
  World world;
  world.notification = 1;
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) {
      world.menu = 257;
      world.notification = 0;
    }
    if (event.op == Op::kReleaseRows) world.ability_owner = 41;
    if (event.op == Op::kReleaseNotification) world.ability_owner = 42;
  };
  b::UpdateAbilityUnlockResults(world);
  CHECK(world.events == std::vector<Event>{{Op::kUpdate},
                                           {Op::kStatus},
                                           {Op::kNotification, 0},
                                           {Op::kResumeField},
                                           {Op::kReleaseRows},
                                           {Op::kReleaseNotification},
                                           {Op::kBeginAbilityReward, 42}});
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
}

TEST_CASE("F00198 refuses a removed menu rather than treating it as closed",
          "[reward-dialogs][F00198]") {
  World world;
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) world.menu.reset();
  };
  CHECK_THROWS_AS(b::UpdateAbilityUnlockResults(world),
                  std::bad_optional_access);
  CHECK(world.events == std::vector<Event>{{Op::kUpdate}, {Op::kStatus}});
}

TEST_CASE("F00198 propagates an ability owner removed by notice cleanup",
          "[reward-dialogs][F00198]") {
  World world;
  world.menu = 0;
  world.callback = [&](Event event) {
    if (event.op == Op::kReleaseNotification) world.ability_owner.reset();
  };
  CHECK_THROWS_AS(b::UpdateAbilityUnlockResults(world),
                  std::bad_optional_access);
  CHECK(world.events.back().op == Op::kReleaseNotification);
  CHECK(world.phase == 99);
}

TEST_CASE("F00224 busy status never needs follow loot or action owners",
          "[reward-dialogs][F00224]") {
  World world;
  world.menu.reset();
  world.follow.reset();
  world.loot_opened.reset();
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) world.menu = 1;
  };
  b::UpdatePendingLootDialog(world);
  CHECK(world.events == std::vector<Event>{{Op::kUpdate}, {Op::kStatus}});
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
}

TEST_CASE("F00224 current nonzero follow and fresh loot retain two resumes",
          "[reward-dialogs][F00224]") {
  World world;
  const auto follow = GENERATE(-1, 0x10000, INT32_MIN);
  std::size_t ends{};
  world.ConfigureTwoResumes(follow, ends);
  b::UpdatePendingLootDialog(world);
  CHECK(world.events.at(2) == Event{Op::kFollow, static_cast<Word>(follow)});
  CHECK(world.events.at(9) == Event{Op::kReleaseRows});
  CHECK(world.events.at(10) == Event{Op::kLootOpened, 0x80000000});
  CHECK(world.events.at(11) == Event{Op::kPhase, 2});
  CHECK(world.events.back() == Event{Op::kFollow, 0});
  CHECK(ends == 2);
}

TEST_CASE("F00224 two resumes leave no pause or unused action access",
          "[reward-dialogs][F00224]") {
  World world;
  const auto follow = GENERATE(-1, 0x10000, INT32_MIN);
  std::size_t ends{};
  world.ConfigureTwoResumes(follow, ends);
  b::UpdatePendingLootDialog(world);
  CHECK(world.phase == 2);
  CHECK(world.paused == 0);
  CHECK(world.selection_reads == 0);
}

TEST_CASE("F00224 release can clear loot and select actual lazy unlock policy",
          "[reward-dialogs][F00224]") {
  World world;
  world.menu = 0;
  world.loot_opened = 1;
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kReleaseRows) {
      world.loot_opened = 0;
      world.flag = 0;
      world.pending = 0x10000;
    }
  };
  b::UpdatePendingLootDialog(world);
  CHECK(world.events.at(2) == Event{Op::kFollow, 0});
  CHECK(world.events.at(3) == Event{Op::kReleaseRows});
  CHECK(world.events.at(4) == Event{Op::kLootOpened, 0});
  CHECK(world.phase == 5);
  CHECK(world.paused == 77);
  CHECK(world.events.back() == Event{Op::kPlatforms});
  CHECK(world.selection_reads == 0);
  CHECK(world.definition_reads == 0);
}

TEST_CASE("F00224 checks the action owner installed by row cleanup",
          "[reward-dialogs][F00224]") {
  World world;
  world.menu = 0;
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kReleaseRows) {
      world.selection = b::RewardSelection{.action_id = 255};
    }
  };
  b::UpdatePendingLootDialog(world);
  CHECK(world.phase == 5);
  CHECK(world.selection_reads == 1);
  CHECK(world.definition_reads == 1);
  CHECK(world.events.back() == Event{Op::kPlatforms});
}

TEST_CASE("F00224 propagates used missing action and out of range definition",
          "[reward-dialogs][F00224]") {
  World world;
  const bool missing = GENERATE(false, true);
  world.menu = 0;
  world.callback = [&](Event event) {
    if (event.op == Op::kReleaseRows) {
      if (missing) {
        world.selection.reset();
      } else {
        world.selection = b::RewardSelection{.action_id = 256};
      }
    }
  };
  if (missing) {
    CHECK_THROWS_AS(b::UpdatePendingLootDialog(world),
                    std::bad_optional_access);
  } else {
    CHECK_THROWS_AS(b::UpdatePendingLootDialog(world), std::out_of_range);
  }
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
  CHECK(world.selection_reads == 1);
}
}  // namespace
