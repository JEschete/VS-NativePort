#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Battle/ActionOutcomes.hpp"

namespace {
namespace b = VS::Battle;
enum class DefeatCall { kScore, kStreak, kLoot, kMark };
struct Enemy final : b::EnemyDefeatRecord {
  std::uint8_t spawn = 1, defeated{};
  std::vector<DefeatCall> calls;
  std::function<void(DefeatCall)> callback;
  std::uint8_t Spawn() const override { return spawn; }
  bool Defeated() const override { return defeated != 0; }
  void Record(DefeatCall call) {
    calls.push_back(call);
    if (callback) callback(call);
  }
  void RecordKill() override { Record(DefeatCall::kScore); }
  void UpdateWeaponStreak() override { Record(DefeatCall::kStreak); }
  void QueueLoot() override { Record(DefeatCall::kLoot); }
  void MarkDefeated() override {
    defeated = 1;
    Record(DefeatCall::kMark);
  }
};
struct Action final : b::ActionOutcomeAction {
  std::array<b::ActionTargetHit, b::kActionTargetCapacity> targets{};
  b::ActionTargetHit primary{}, applied{};
  std::uint16_t count{};
  std::vector<int> reactions;
  int applications{};
  std::function<void()> reaction, result;
  std::uint16_t Count() const override { return count; }
  b::ActionTargetHit Target(std::size_t index) const override {
    return targets.at(index);
  }
  b::ActionTargetHit Primary() const override { return primary; }
  void DamageReaction(int actor) override {
    reactions.push_back(actor);
    if (reaction) reaction();
  }
  void ApplyPrimaryResult() override {
    applied = primary;
    ++applications;
    if (result) result();
  }
};
struct Sound {
  std::uint8_t id{};
  b::ActionPoint point{};
  bool operator==(const Sound&) const = default;
};
struct OutcomeWorld final : b::ActionOutcomeWorld {
  Action first, replacement;
  Action* current = &first;
  b::ActionPoint point{11, -22, 33, 0};
  std::uint8_t category = 1;
  int queue_reads{}, recoveries{};
  std::vector<int> placements, category_reads;
  std::vector<Sound> sounds;
  std::function<void(int)> placed;
  std::function<void(std::uint8_t)> sounded;
  b::ActionOutcomeAction& Queued() override {
    ++queue_reads;
    return *current;
  }
  b::ActionPoint Placement(int actor) override {
    placements.push_back(actor);
    const auto saved = point;
    if (placed) placed(actor);
    return saved;
  }
  std::uint8_t WeaponCategory(int actor) override {
    category_reads.push_back(actor);
    return category;
  }
  void PlaySound(std::uint8_t sound, b::ActionPoint position) override {
    sounds.push_back({sound, position});
    if (sounded) sounded(sound);
  }
  void RecoveryPose() override { ++recoveries; }
};
enum class AnimationCall { kRefresh, kMarker, kDeath, kLock };
struct DefeatWorld final : b::ActorDefeatWorld {
  struct Record {
    bool present = true, retained{};
    std::uint8_t command = 77, spawn = 1;
    std::int16_t hp{};
  } first, replacement;
  Record* current = &first;
  std::vector<AnimationCall> calls;
  std::vector<int> slots;
  std::function<void(AnimationCall)> callback;
  int state_reads{};
  void RecordCall(AnimationCall call, int actor) {
    calls.push_back(call);
    slots.push_back(actor);
    if (callback) callback(call);
  }
  void RefreshReservations(int actor) override {
    RecordCall(AnimationCall::kRefresh, actor);
  }
  bool HasActor(int) override { return current->present; }
  void SetCommandState(int, std::uint8_t state) override {
    current->command = state;
  }
  std::int16_t Hp(int) override { return current->hp; }
  void HideMarker(int actor) override {
    RecordCall(AnimationCall::kMarker, actor);
  }
  bool Retained(int) override { return current->retained; }
  void BeginDeath(int actor) override {
    ++state_reads;
    RecordCall(AnimationCall::kDeath, actor);
  }
  void LockAnimation(int actor) override {
    RecordCall(AnimationCall::kLock, actor);
  }
  std::uint8_t Spawn(int) override { return current->spawn; }
};
struct LearnedWorld final : b::LearnedActionWorld {
  std::array<std::uint8_t, 32> flags{};
  std::array<std::uint32_t, 256> words{};
  std::uint8_t LearnedByte(std::size_t index) const override {
    return flags.at(index);
  }
  void SetUnlocked(std::size_t index, bool unlocked) override {
    auto& word = words.at(index);
    word = (word & ~0x8000U) | (unlocked ? 0x8000U : 0U);
  }
};

TEST_CASE("F00048 eligible defeat orders services before its final flag",
          "[action-outcomes][F00048]") {
  Enemy enemy;
  enemy.callback = [&](DefeatCall call) {
    if (call != DefeatCall::kMark) CHECK(enemy.defeated != 1);
    if (call == DefeatCall::kScore) {
      enemy.spawn = 0x80;
      enemy.defeated = 255;
    }
    if (call == DefeatCall::kLoot) enemy.defeated = 0;
  };
  b::ResolveEnemyDefeat(0, -256, enemy);
  CHECK(enemy.calls == std::vector{DefeatCall::kScore, DefeatCall::kStreak,
                                   DefeatCall::kLoot, DefeatCall::kMark});
  CHECK(enemy.defeated == 1);
}
TEST_CASE("F00048 uses full source and any nonzero kill request",
          "[action-outcomes][F00048]") {
  const int source = GENERATE(0, 1, 256, -256);
  const int count = GENERATE(0, 1, -1, 256);
  Enemy enemy;
  b::ResolveEnemyDefeat(source, count, enemy);
  CHECK(enemy.calls.size() == (source == 0 && count != 0 ? 4 : 2));
  CHECK(enemy.calls.at(enemy.calls.size() - 2) == DefeatCall::kLoot);
  CHECK(enemy.defeated == 1);
}
TEST_CASE("F00048 rejects only its original repeat and dynamic guards",
          "[action-outcomes][F00048]") {
  const bool dynamic = GENERATE(false, true);
  Enemy enemy;
  enemy.spawn = dynamic ? 0x80 : 1;
  enemy.defeated = dynamic ? 0 : 128;
  b::ResolveEnemyDefeat(0, 1, enemy);
  CHECK(enemy.calls.empty());
  CHECK(enemy.defeated == (dynamic ? 0 : 128));
}
TEST_CASE("F00059 selects each weapon category without narrowing the source",
          "[action-outcomes][F00059]") {
  const auto category = GENERATE(range(1, 11));
  constexpr std::array<std::array<std::uint8_t, 2>, 10> kExpected{
      {{0x47, 0x48},
       {0x49, 0x4A},
       {0x4B, 0x4C},
       {0x50, 0x51},
       {0x52, 0x53},
       {0x4D, 0},
       {0x4E, 0x4F},
       {0x54, 0x55},
       {0x69, 0x6A},
       {0x5B, 0x5C}}};
  OutcomeWorld world;
  world.category = static_cast<std::uint8_t>(category);
  world.first.count = 1;
  world.first.targets.at(0) = {false, 255, 4};
  const auto expected = kExpected.at(static_cast<std::size_t>(category - 1));
  CHECK(b::PlayWeaponHitSounds(257, world) == 1);
  CHECK(world.placements == std::vector{255});
  CHECK(world.category_reads == std::vector{257});
  CHECK(world.sounds.at(0) == Sound{expected.at(0), world.point});
  CHECK(world.sounds.size() == (category == 6 ? 1 : 2));
  if (category != 6) {
    CHECK(world.sounds.at(1) == Sound{expected.at(1), world.point});
  }
}
TEST_CASE("F00059 rereads impact and count while retaining its queued owner",
          "[action-outcomes][F00059]") {
  OutcomeWorld world;
  world.first.count = 1;
  world.first.targets.at(0) = {false, 7, 5};
  world.first.targets.at(1) = {false, 8, 3};
  world.placed = [&](int actor) {
    if (actor == 7) {
      world.first.targets.at(0).impact_kind = 0;
      world.first.targets.at(0).target_actor = 99;
      world.first.count = 2;
      world.current = &world.replacement;
    }
  };
  world.sounded = [&](std::uint8_t sound) {
    if (sound == 0x47) {
      world.category = 6;
      world.point = {44, 55, 66, 0};
    }
  };
  CHECK(b::PlayWeaponHitSounds(0, world) == 1);
  CHECK(world.queue_reads == 1);
  CHECK(world.placements == std::vector{7, 8});
  CHECK(world.sounds == std::vector<Sound>{{0x47, {11, -22, 33, 0}},
                                           {0x48, {11, -22, 33, 0}},
                                           {0x33, {44, 55, 66, 0}}});
}
TEST_CASE("F00059 ignores an unused source and still places silent actor hits",
          "[action-outcomes][F00059]") {
  const int mode = GENERATE(0, 1, 2, 3, 4);
  OutcomeWorld world;
  world.first.count = mode == 0 ? 0 : 1;
  world.first.targets.at(0) = {
      mode == 1, 255, static_cast<std::uint8_t>(mode == 2 ? 3 : mode + 2)};
  CHECK(b::PlayWeaponHitSounds(-1000, world) == 1);
  CHECK(world.category_reads.empty());
  CHECK(world.placements.size() == (mode >= 2 ? 1 : 0));
  CHECK(world.sounds.size() == (mode == 2 ? 1 : 0));
}
TEST_CASE("F00059 rereads source category for each native target up to 255",
          "[action-outcomes][F00059]") {
  OutcomeWorld world;
  world.first.count = 255;
  world.category = 6;
  world.sounded = [&](std::uint8_t) { world.category = 0; };
  CHECK(b::PlayWeaponHitSounds(0, world) == 1);
  CHECK(world.placements.size() == 255);
  CHECK(world.category_reads.size() == 255);
  CHECK(world.sounds.size() == 1);
}
TEST_CASE("F00059 categories outside the weapon sound table remain silent",
          "[action-outcomes][F00059]") {
  const auto category = GENERATE(0, 11, 255);
  OutcomeWorld world;
  world.category = static_cast<std::uint8_t>(category);
  world.first.count = 1;
  CHECK(b::PlayWeaponHitSounds(0, world) == 1);
  CHECK(world.placements == std::vector{0});
  CHECK(world.category_reads == std::vector{0});
  CHECK(world.sounds.empty());
}
TEST_CASE("F00061 signed gate avoids state only for sources at least sixteen",
          "[action-outcomes][F00061]") {
  const int source = GENERATE(-256, -1, 16, 256);
  OutcomeWorld world;
  b::ApplyActionSourceEffects(source, world);
  CHECK(world.queue_reads == (source >= 16 ? 0 : 1));
  CHECK(world.first.applications == 0);
  CHECK(world.first.reactions.empty());
}
TEST_CASE("F00061 selects animation without skipping the actual result",
          "[action-outcomes][F00061]") {
  const int source = GENERATE(0, 15);
  const auto impact = GENERATE(0, 1, 2, 3, 4, 5, 6, 255);
  OutcomeWorld world;
  world.first.primary = {false, source, static_cast<std::uint8_t>(impact)};
  b::ApplyActionSourceEffects(source, world);
  CHECK(world.first.reactions.size() == (impact <= 1 ? 1 : 0));
  CHECK(world.recoveries == (impact == 5 && source == 0 ? 1 : 0));
  CHECK(world.first.applications == 1);
}
TEST_CASE("F00061 keeps the request but applies its post-reaction primary",
          "[action-outcomes][F00061]") {
  OutcomeWorld world;
  world.first.primary = {false, 0, 0};
  world.first.reaction = [&] {
    world.first.primary = {true, 9, 255};
    world.current = &world.replacement;
  };
  b::ApplyActionSourceEffects(0, world);
  CHECK(world.first.applied.targets_position);
  CHECK(world.first.applied.target_actor == 9);
  CHECK(world.first.applied.impact_kind == 255);
  CHECK(world.first.applications == 1);
  CHECK(world.replacement.applications == 0);
}
TEST_CASE("F00061 unmatched and position primaries perform no services",
          "[action-outcomes][F00061]") {
  const bool position = GENERATE(false, true);
  OutcomeWorld world;
  world.first.primary = {position, position ? 0 : 1, 0};
  b::ApplyActionSourceEffects(0, world);
  CHECK(world.first.applications == 0);
  CHECK(world.first.reactions.empty());
}
TEST_CASE("F00156 refresh can create or remove the actor before its first read",
          "[action-outcomes][F00156]") {
  const bool create = GENERATE(false, true);
  DefeatWorld world;
  world.first.present = !create;
  world.first.hp = 1;
  world.callback = [&](AnimationCall) { world.first.present = create; };
  CHECK(b::BeginActorDefeatAnimation(257, world) == 0);
  CHECK(world.first.command == (create ? 0 : 77));
  CHECK(world.calls == std::vector{AnimationCall::kRefresh});
  CHECK(world.slots == std::vector{257});
  CHECK(world.state_reads == 0);
}
TEST_CASE("F00156 living and negative HP clear only the command",
          "[action-outcomes][F00156]") {
  const auto hp = GENERATE(-32768, -1, 1, 32767);
  DefeatWorld world;
  world.first.hp = static_cast<std::int16_t>(hp);
  CHECK(b::BeginActorDefeatAnimation(1, world) == 0);
  CHECK(world.first.command == 0);
  CHECK(world.calls == std::vector{AnimationCall::kRefresh});
  CHECK(world.state_reads == 0);
}
TEST_CASE("F00156 marker replacement controls command retention and lazy state",
          "[action-outcomes][F00156]") {
  DefeatWorld world;
  world.replacement.retained = true;
  world.callback = [&](AnimationCall call) {
    if (call == AnimationCall::kMarker) world.current = &world.replacement;
  };
  CHECK(b::BeginActorDefeatAnimation(1, world) == 0);
  CHECK(world.first.command == 0);
  CHECK(world.replacement.command == 0x20);
  CHECK(world.calls ==
        std::vector{AnimationCall::kRefresh, AnimationCall::kMarker});
  CHECK(world.state_reads == 0);
}
TEST_CASE(
    "F00156 post-death fresh spawn preserves the full slot-zero exemption",
    "[action-outcomes][F00156]") {
  const int slot = GENERATE(0, 1, 256);
  DefeatWorld world;
  world.callback = [&](AnimationCall call) {
    if (call == AnimationCall::kDeath) world.current = &world.replacement;
    if (call == AnimationCall::kLock) world.replacement.spawn = 0x80;
  };
  CHECK(b::BeginActorDefeatAnimation(slot, world) == (slot == 0 ? 1 : 0));
  CHECK(world.calls ==
        std::vector{AnimationCall::kRefresh, AnimationCall::kMarker,
                    AnimationCall::kDeath, AnimationCall::kLock});
  CHECK(world.first.command == 0x20);
  CHECK(world.state_reads == 1);
}
TEST_CASE("F01851 applies MSB-first flags by index and clears stale unlocks",
          "[action-outcomes][F01851]") {
  LearnedWorld world;
  world.flags.at(0) = 0x81;
  world.flags.at(1) = 0x80;
  world.flags.at(31) = 1;
  world.words.fill(0xFEDC7654);
  b::ApplyLearnedActionUnlocks(world);
  CHECK(world.words.at(0) == 0xFEDCF654);
  CHECK(world.words.at(7) == 0xFEDCF654);
  CHECK(world.words.at(8) == 0xFEDCF654);
  CHECK(world.words.at(255) == 0xFEDCF654);
  CHECK(world.words.at(1) == 0xFEDC7654);
  world.flags.fill(0);
  b::ApplyLearnedActionUnlocks(world);
  CHECK(world.words.at(0) == 0xFEDC7654);
  CHECK(world.words.at(255) == 0xFEDC7654);
}
}  // namespace
