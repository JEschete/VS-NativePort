#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/RewardFlow.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Content/ModuleData.h"

namespace {
namespace b = VS::Battle;
using LootId = VS::Menu::Framework::LootListId;
using Word = std::uint32_t;
enum class Op {
  kPhase,
  kPaused,
  kFollow,
  kFlag,
  kPending,
  kSelection,
  kDefinition,
  kUnlock,
  kRoomPalette,
  kSceneColors,
  kClut,
  kFreeze,
  kUnfreeze,
  kRoom,
  kMinimap,
  kAmbience,
  kIdle,
  kPose,
  kEffect,
  kPlatforms,
  kNotice,
  kAbilityMenu,
  kLootHead,
  kLootMenu,
  kResults,
  kUpdate,
  kStatus,
  kRelease
};
struct Event {
  Op op{};
  Word first{}, second{};
  bool operator==(const Event&) const = default;
};
struct World final : b::RewardWorld {
  Word phase = 99, paused = 77;
  std::int32_t follow = 0;
  std::array<std::uint8_t, 3> b6{1, 1, 1};
  std::array<std::uint8_t, 2> ab{0, 0};
  std::size_t b6_reads{}, ab_reads{}, selection_reads{}, definition_reads{};
  Word pending{};
  std::optional<b::RewardSelection> selection{b::RewardSelection{}};
  std::array<Word, 256> definitions{};
  LootId loot{};
  std::optional<std::int16_t> menu_status{1};
  std::vector<Event> events;
  std::vector<VS::Field::ColorTransform> transforms;
  std::function<void(Event)> callback;

  void Record(Event event) {
    events.push_back(event);
    if (callback) callback(event);
  }
  void SetPhase(b::RewardPhase value) override {
    phase = static_cast<Word>(value);
    Record({Op::kPhase, phase});
  }
  void SetInteractionPaused(bool value) override {
    paused = value ? 1 : 0;
    Record({Op::kPaused, paused});
  }
  std::int32_t FollowAction() override {
    Record({Op::kFollow});
    return follow;
  }
  std::uint8_t ScriptFlag(int flag) override {
    const auto value = flag == 0xB6 ? b6.at(b6_reads++) : ab.at(ab_reads++);
    Record({Op::kFlag, static_cast<Word>(flag), value});
    return value;
  }
  Word PendingUnlocks() override {
    const Word value = pending;
    Record({Op::kPending, value});
    return value;
  }
  b::RewardSelection Selection() override {
    ++selection_reads;
    if (!selection.has_value()) throw std::bad_optional_access();
    return selection.value();
  }
  bool DefinitionUnlocked(std::uint16_t action) override {
    ++definition_reads;
    return (definitions.at(action) & 0x8000) != 0;
  }
  void MarkDefinitionUnlocked(std::uint16_t action) override {
    definitions.at(action) |= 0x8000;
    Record({Op::kUnlock, action});
  }
  void RoomPalettes(const VS::Field::ColorTransform& value) override {
    transforms.push_back(value);
    Record({Op::kRoomPalette});
  }
  void SceneColors(const VS::Field::ColorTransform& value) override {
    transforms.push_back(value);
    Record({Op::kSceneColors});
  }
  void EnableClutAnimation(bool value) override {
    Record({Op::kClut, value ? 1U : 0U});
  }
  void FreezeActorAnimations() override { Record({Op::kFreeze}); }
  void UnfreezeActorAnimations() override { Record({Op::kUnfreeze}); }
  void PauseRoomAnimations(bool value) override {
    Record({Op::kRoom, value ? 1U : 0U});
  }
  void ShowMinimap(bool value) override {
    Record({Op::kMinimap, value ? 1U : 0U});
  }
  void DuckAmbience(bool value) override {
    Record({Op::kAmbience, value ? 1U : 0U});
  }
  void RestoreIdlePose() override { Record({Op::kIdle}); }
  void PlayLearnedPose() override { Record({Op::kPose}); }
  void SpawnLearnedEffect() override { Record({Op::kEffect}); }
  void PauseMovingPlatforms() override { Record({Op::kPlatforms}); }
  void ShowActionLearned(std::uint16_t action) override {
    Record({Op::kNotice, action});
  }
  void OpenAbilityUnlocks(std::uint16_t art, std::uint16_t ability) override {
    Record({Op::kAbilityMenu, art, ability});
  }
  LootId LootHead() override {
    Record({Op::kLootHead});
    return loot;
  }
  void OpenLoot(LootId owner) override {
    Record({Op::kLootMenu, static_cast<Word>(owner)});
  }
  void OpenResults() override { Record({Op::kResults}); }
  void UpdateMenu() override { Record({Op::kUpdate}); }
  std::int16_t MenuStatus() override {
    Record({Op::kStatus});
    if (!menu_status.has_value()) throw std::bad_optional_access();
    return menu_status.value();
  }
  void ReleaseMenuRows() override { Record({Op::kRelease}); }

  bool Saw(Op op) const {
    return std::ranges::any_of(events,
                               [op](Event event) { return event.op == op; });
  }
};

TEST_CASE("F04123 darkens the room and pauses after every service",
          "[reward-flow][F04123]") {
  World world;
  world.selection.reset();
  b::BeginRewardPause(world);
  CHECK(world.events == std::vector<Event>{{Op::kRoomPalette},
                                           {Op::kClut, 0},
                                           {Op::kUnfreeze},
                                           {Op::kRoom, 1},
                                           {Op::kMinimap, 0},
                                           {Op::kAmbience, 1},
                                           {Op::kPaused, 1}});
  CHECK(world.transforms ==
        std::vector<VS::Field::ColorTransform>{{13, 4, -4, -4, 0}});
  CHECK(world.phase == 99);
  CHECK(world.selection_reads == 0);
}

TEST_CASE("F04123 commits only paused after callbacks change other globals",
          "[reward-flow][F04123]") {
  World world;
  world.callback = [&](Event event) {
    if (event.op == Op::kAmbience) {
      CHECK(world.paused == 77);
      world.phase = 0xABCDEFFF;
      world.follow = -1;
      world.paused = 0xFACE;
      world.selection.reset();
    }
  };
  b::BeginRewardPause(world);
  CHECK(world.paused == 1);
  CHECK(world.phase == 0xABCDEFFF);
  CHECK(world.follow == -1);
}

TEST_CASE("F04124 restores scene colors without a minimap operation",
          "[reward-flow][F04124]") {
  World world;
  world.selection.reset();
  b::EndRewardPause(world);
  CHECK(world.events == std::vector<Event>{{Op::kClut, 1},
                                           {Op::kSceneColors},
                                           {Op::kFreeze},
                                           {Op::kRoom, 0},
                                           {Op::kAmbience, 0},
                                           {Op::kPaused, 0}});
  CHECK(world.transforms ==
        std::vector<VS::Field::ColorTransform>{{8, 4, 0, 0, 0}});
  CHECK(world.phase == 99);
}

TEST_CASE(
    "F04124 clears the complete paused field last without restoring peers",
    "[reward-flow][F04124]") {
  World world;
  world.callback = [&](Event event) {
    if (event.op == Op::kAmbience) {
      CHECK(world.paused == 77);
      world.phase = 0xFF010203;
      world.follow = -7;
      world.paused = 0xFFFFFFFF;
    }
  };
  b::EndRewardPause(world);
  CHECK(world.paused == 0);
  CHECK(world.phase == 0xFF010203);
  CHECK(world.follow == -7);
}

TEST_CASE(
    "F04125 stores phase before services and uses their current follow flag",
    "[reward-flow][F04125]") {
  World world;
  const auto current = GENERATE(0, -1, 0x10000, INT32_MIN);
  world.follow = current == 0 ? 1 : 0;
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kClut) CHECK(world.phase == 2);
    if (event.op == Op::kAmbience) {
      world.follow = current;
      world.phase = 0xABCD1234;
    }
  };
  b::ResumeAfterRewards(world);
  CHECK(world.phase == 0xABCD1234);
  CHECK(world.paused == 0);
  CHECK(world.Saw(Op::kIdle) == (current != 0));
  CHECK(world.events.front() == Event{Op::kPhase, 2});
}

TEST_CASE(
    "F04126 performs separate short circuit B6 reads before pending query",
    "[reward-flow][F04126]") {
  World world;
  const int gate = GENERATE(0, 1, 2, 3);
  world.b6 = gate == 0   ? std::array<std::uint8_t, 3>{0, 1, 1}
             : gate == 1 ? std::array<std::uint8_t, 3>{1, 2, 1}
             : gate == 2 ? std::array<std::uint8_t, 3>{2, 1, 3}
                         : std::array<std::uint8_t, 3>{3, 3, 2};
  world.pending = 0x80000000;
  if (gate != 3) world.selection.reset();
  b::CheckRewardUnlocks(world);
  CHECK(world.b6_reads == static_cast<std::size_t>(std::min(gate + 1, 3)));
  CHECK(world.Saw(Op::kPending) == (gate != 3));
  CHECK(world.phase == (gate == 3 ? 2 : 5));
  CHECK(world.selection_reads == (gate == 3 ? 1 : 0));
}

TEST_CASE("F04126 keeps both learned callbacks even if the pose clears follow",
          "[reward-flow][F04126]") {
  World world;
  world.b6 = {0, 0, 0};
  world.pending = GENERATE(1U, 0x10000U, 0xFFFF0000U, 0xFFFFFFFFU);
  world.follow = -1;
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kPose) {
      CHECK(world.phase == 5);
      world.follow = 0;
      world.phase = 91;
    }
  };
  b::CheckRewardUnlocks(world);
  CHECK(world.events == std::vector<Event>{{Op::kFlag, 0xB6, 0},
                                           {Op::kPending, world.pending},
                                           {Op::kPhase, 5},
                                           {Op::kFollow},
                                           {Op::kPose},
                                           {Op::kEffect},
                                           {Op::kPlatforms}});
  CHECK(world.phase == 91);
  CHECK(world.selection_reads == 0);
}

TEST_CASE("F04126 checks selection gates without an actor or a zero-id lookup",
          "[reward-flow][F04126]") {
  World world;
  const int gate = GENERATE(0, 1, 2, 3, 4, 5, 6);
  auto& action = world.selection.emplace();
  action.action_id = gate == 0 ? 0 : gate == 6 ? 255 : 1;
  if (gate == 1) world.definitions.at(1) = 0x8000;
  if (gate == 2) action.targets_position = 0x80000000;
  if (gate == 3) action.continued = 255;
  if (gate == 4) action.primary_actor = 255;
  b::CheckRewardUnlocks(world);
  CHECK(world.phase == (gate >= 5 ? 5 : 2));
  CHECK(world.definition_reads == (gate == 0 ? 0 : 1));
  CHECK(world.Saw(Op::kPending) == false);
  CHECK(world.Saw(Op::kPlatforms) == (gate >= 5));
}

TEST_CASE("F04126 fallback invokes the actual resume policy",
          "[reward-flow][F04126]") {
  World world;
  world.b6 = {0, 0, 0};
  world.callback = [&](Event event) {
    if (event.op == Op::kAmbience) world.follow = -1;
  };
  b::CheckRewardUnlocks(world);
  CHECK(world.phase == 2);
  CHECK(world.paused == 0);
  CHECK(world.events.back() == Event{Op::kIdle});
}

TEST_CASE("F00220 reloads selection after pause and after the notice",
          "[reward-flow][F00220]") {
  World world;
  world.selection = b::RewardSelection{.action_id = 1};
  world.definitions.at(0) = 0xAABB1122;
  world.definitions.at(1) = 0xCCDD1234;
  world.callback = [&](Event event) {
    if (event.op == Op::kRoomPalette) {
      CHECK(world.phase == 6);
      world.selection = b::RewardSelection{.action_id = 255};
    }
    if (event.op == Op::kNotice) {
      CHECK(event.first == 255);
      CHECK(world.paused == 1);
      world.selection = b::RewardSelection{};
    }
  };
  b::ShowPendingRewardUnlock(world);
  CHECK(world.definitions.at(0) == 0xAABB9122);
  CHECK(world.definitions.at(1) == 0xCCDD1234);
  CHECK(world.definitions.at(255) == 0);
  CHECK(world.events.back() == Event{Op::kUnlock, 0});
  CHECK_FALSE(world.Saw(Op::kPending));
}

TEST_CASE(
    "F00220 caches the entire packed pending value across pause callbacks",
    "[reward-flow][F00220]") {
  World world;
  world.pending = GENERATE(0U, 1U, 0x10000U, 0xFEDCBA98U, 0xFFFFFFFFU);
  const Word original = world.pending;
  world.callback = [&](Event event) {
    if (event.op == Op::kRoomPalette) {
      CHECK(world.phase == 6);
      world.pending = 0x12345678;
      world.selection.reset();
    }
  };
  b::ShowPendingRewardUnlock(world);
  CHECK(world.events.front() == Event{Op::kPending, original});
  CHECK(world.events.back() ==
        Event{Op::kAbilityMenu, original >> 16, original & 0xFFFF});
  CHECK(world.selection_reads == 1);
  CHECK(world.paused == 1);
}

TEST_CASE("F00220 applies all initial action gates before any callback",
          "[reward-flow][F00220]") {
  World world;
  const int gate = GENERATE(0, 1, 2, 3, 4);
  auto& action = world.selection.emplace();
  action.action_id = gate == 0 ? 0 : 255;
  if (gate == 1) world.definitions.at(255) = 0x8000;
  if (gate == 2) action.targets_position = 1;
  if (gate == 3) action.continued = 128;
  if (gate == 4) action.primary_actor = 128;
  b::ShowPendingRewardUnlock(world);
  CHECK(world.events.front() == Event{Op::kPending, 0});
  CHECK(world.events.back() == Event{Op::kAbilityMenu, 0, 0});
  CHECK(world.definition_reads == (gate == 0 ? 0 : 1));
}

TEST_CASE("F00221 lower stages resume without touching loot or action owners",
          "[reward-flow][F00221]") {
  World world;
  world.ab.at(0) = GENERATE(0, 1, 2);
  world.selection.reset();
  b::OpenPendingLootRewards(world);
  CHECK(world.phase == 2);
  CHECK_FALSE(world.Saw(Op::kLootHead));
  CHECK_FALSE(world.Saw(Op::kPending));
  CHECK(world.selection_reads == 0);
}

TEST_CASE("F00221 opens the captured loot owner before phase and pause",
          "[reward-flow][F00221]") {
  World world;
  world.ab.at(0) = GENERATE(3, 127, 128, 255);
  world.loot = LootId{37};
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kLootMenu) {
      CHECK(world.phase == 99);
      CHECK(world.paused == 77);
      world.loot = LootId{42};
      world.phase = 98;
    }
  };
  b::OpenPendingLootRewards(world);
  CHECK(world.events.at(2) == Event{Op::kLootMenu, 37});
  CHECK(world.events.at(3) == Event{Op::kPhase, 4});
  CHECK(world.phase == 4);
  CHECK(world.paused == 1);
  CHECK(world.loot == LootId{42});
}

TEST_CASE("F00221 an empty loot list follows the real unlock policy lazily",
          "[reward-flow][F00221]") {
  World world;
  world.ab.at(0) = 3;
  world.b6.at(0) = 0;
  world.pending = 0x10000;
  world.selection.reset();
  b::OpenPendingLootRewards(world);
  CHECK(world.phase == 5);
  CHECK(world.events.back() == Event{Op::kPlatforms});
  CHECK(world.selection_reads == 0);
  CHECK_FALSE(world.Saw(Op::kRoomPalette));
}

TEST_CASE("F00222 opens results before selecting phase three and pausing",
          "[reward-flow][F00222]") {
  World world;
  world.ab.at(0) = GENERATE(3, 128, 255);
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kResults) {
      CHECK(world.phase == 99);
      world.phase = 0xFF000000;
    }
  };
  b::OpenRewardResults(world);
  CHECK(world.events.at(1) == Event{Op::kResults});
  CHECK(world.events.at(2) == Event{Op::kPhase, 3});
  CHECK(world.phase == 3);
  CHECK(world.paused == 1);
  CHECK_FALSE(world.Saw(Op::kLootHead));
}

TEST_CASE("F00222 delegates low stages and rereads AB rather than caching it",
          "[reward-flow][F00222]") {
  World world;
  world.ab = {2, 3};
  world.loot = LootId{9};
  world.selection.reset();
  b::OpenRewardResults(world);
  CHECK(world.events.at(0) == Event{Op::kFlag, 0xAB, 2});
  CHECK(world.events.at(1) == Event{Op::kFlag, 0xAB, 3});
  CHECK(world.phase == 4);
  CHECK(world.Saw(Op::kLootMenu));
  CHECK_FALSE(world.Saw(Op::kResults));
}

TEST_CASE("F00223 busy status is resolved only after updating the menu",
          "[reward-flow][F00223]") {
  World world;
  world.menu_status.reset();
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) world.menu_status = 1;
  };
  b::UpdateRewardResultsDialog(world);
  CHECK(world.events == std::vector<Event>{{Op::kUpdate}, {Op::kStatus}});
  CHECK(world.phase == 99);
  CHECK(world.paused == 77);
}

TEST_CASE("F00223 signed nonbusy status ends pause releases then delegates",
          "[reward-flow][F00223]") {
  World world;
  world.menu_status = 1;
  world.selection.reset();
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) world.menu_status = -1;
    if (event.op == Op::kRelease) {
      CHECK(world.paused == 0);
      world.ab.at(0) = 3;
      world.loot = LootId{77};
    }
  };
  b::UpdateRewardResultsDialog(world);
  CHECK(world.events.at(0) == Event{Op::kUpdate});
  CHECK(world.events.at(1) == Event{Op::kStatus});
  CHECK(world.events.at(8) == Event{Op::kRelease});
  CHECK(world.phase == 4);
  CHECK(world.events.at(11) == Event{Op::kLootMenu, 77});
  CHECK(world.paused == 1);
}

TEST_CASE("F00223 does not silently close a menu removed by update",
          "[reward-flow][F00223]") {
  World world;
  world.callback = [&](Event event) {
    if (event.op == Op::kUpdate) world.menu_status.reset();
  };
  CHECK_THROWS_AS(b::UpdateRewardResultsDialog(world),
                  std::bad_optional_access);
  CHECK_FALSE(world.Saw(Op::kRelease));
}

TEST_CASE("Reward flow retains eight original void entrypoint relocations",
          "[reward-flow][reward-abi]") {
  using Entry = void (*)();
  constexpr std::array<Entry, 8> kEntries{
      b::BeginLiveRewardPause,        b::EndLiveRewardPause,
      b::ResumeLiveAfterRewards,      b::CheckLiveRewardUnlocks,
      b::ShowLivePendingRewardUnlock, b::OpenLivePendingLootRewards,
      b::OpenLiveRewardResults,       b::UpdateLiveRewardResultsDialog};
  constexpr std::array<Word, 8> kAddresses{0x8007B410, 0x8007B470, 0x8007B4C4,
                                           0x8007B508, 0x8007B63C, 0x8007B764,
                                           0x8007B7DC, 0x8007B834};
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (std::size_t index = 0; index < kEntries.size(); ++index) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            kAddresses.at(index)) ==
          reinterpret_cast<void*>(kEntries.at(index)));
  }
}
}  // namespace
