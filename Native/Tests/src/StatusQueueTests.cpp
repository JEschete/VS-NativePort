#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/StatusQueue.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
}

namespace {
namespace battle = VS::Battle;
using Feedback = battle::StatusFeedback;
using Handler = std::optional<Feedback> (*)(std::span<const battle::StatusRule>,
                                            const battle::StatusTarget&,
                                            battle::QueuedStatuses&);

void CheckHandler(Handler handler, std::size_t rule_id, Feedback feedback) {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(rule_id) = {.statuses = 2, .cancelled = 4};
  battle::StatusTarget target{.active = 4};
  battle::QueuedStatuses queued{
      .added = 0x80000000, .removed = 0x100, .feedback = Feedback::kPositive};
  CHECK(handler(rules, target, queued) == feedback);
  CHECK(queued.added == 0x80000002);
  CHECK(queued.removed == 0x104);
  CHECK(queued.feedback == feedback);
  const auto before = queued;
  target.exempt = true;
  CHECK(handler(rules, target, queued) == Feedback::kNone);
  CHECK(queued == before);
  CHECK_FALSE(handler(std::span(rules).first(rule_id), target, queued));
  CHECK(queued == before);
}

struct LiveFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_statusEffectParams)) saved_rules =
      std::to_array(Battle_statusEffectParams);
  Battle_ActorStats stats{};
  Battle_Actor actor{};
  Battle_HitResult hit{};

  LiveFixture() {
    std::ranges::fill(Battle_actors, nullptr);
    std::ranges::fill(Battle_statusEffectParams, Battle_StatusEffectParams{});
    actor.stats = &stats;
    Battle_actors[41] = &actor;
    stats.actorId = 255;
    hit.target.targetActor = 41;
    hit.hpEffect = 3;
    hit.statStatusEffect = 3;
    hit.weaponDpEffect = 2;
    hit.reservedBits28 = 10;
    hit.limbs[5].packed = 0xAABBCCDD;
    hit.targetsPosition = 0x12345678;
  }
  ~LiveFixture() {
    std::ranges::copy(saved_actors, Battle_actors);
    std::ranges::copy(saved_rules, Battle_statusEffectParams);
  }
};

auto Bytes(const Battle_HitResult& hit) {
  return std::bit_cast<std::array<std::byte, sizeof(Battle_HitResult)>>(hit);
}
}  // namespace

TEST_CASE(
    "Status additions apply immunity and active cancellation independently",
    "[status-queue][F00405]") {
  const battle::StatusRule rule{.statuses = 15, .cancelled = 2};
  const battle::StatusTarget target{.active = 6, .immunities = 8};
  battle::QueuedStatuses queued{
      .added = 0x80000000, .removed = 0x100, .feedback = Feedback::kPositive};
  CHECK(battle::QueueApplicableStatusChanges(rule, target, queued) == 3);
  CHECK(queued.added == 0x80000003);
  CHECK(queued.removed == 0x102);
  CHECK(queued.feedback == Feedback::kPositive);
  const auto before = queued;
  CHECK(battle::QueueApplicableStatusChanges(rule, target, queued) == 3);
  CHECK(queued == before);
}

TEST_CASE("Blocking status or exemption preserves all queued state",
          "[status-queue][F00405]") {
  const battle::StatusRule rule{
      .statuses = 0xFFFFFFFF, .cancelled = 0xFFFFFFFF, .blocking = 0x80000000};
  battle::StatusTarget target{.active = 0x80000000};
  battle::QueuedStatuses queued{
      .added = 5, .removed = 7, .feedback = Feedback::kNegative};
  const auto before = queued;
  CHECK(battle::QueueApplicableStatusChanges(rule, target, queued) == 0);
  CHECK(queued == before);
  target.active = 0;
  target.exempt = true;
  CHECK(battle::QueueApplicableStatusChanges(rule, target, queued) == 0);
  CHECK(queued == before);
}

TEST_CASE(
    "Immunity does not prevent cancellation and existing statuses need "
    "reapplication",
    "[status-queue][F00405]") {
  battle::StatusRule rule{.statuses = 0x80000001, .cancelled = 1};
  battle::StatusTarget target{.active = 0x80000001, .immunities = 0xFFFFFFFF};
  battle::QueuedStatuses queued;
  CHECK(battle::QueueApplicableStatusChanges(rule, target, queued) == 1);
  CHECK(queued.added == 0);
  CHECK(queued.removed == 1);
  target.immunities = 0;
  CHECK(battle::QueueApplicableStatusChanges(rule, target, queued) == 1);
  CHECK(queued.added == 1);
  rule.cancelled = 0x80000001;
  CHECK(battle::QueueApplicableStatusChanges(rule, target, queued) ==
        0x80000001);
  CHECK(queued.added == 0x80000001);
}

TEST_CASE(
    "Status removal ignores immunity and blocking but honors actor exemption",
    "[status-queue][F00406]") {
  const battle::StatusRule rule{
      .statuses = 0x80000005, .cancelled = 8, .blocking = 0xFFFFFFFF};
  battle::StatusTarget target{.active = 0x80000007, .immunities = 0xFFFFFFFF};
  battle::QueuedStatuses queued{
      .added = 3, .removed = 16, .feedback = Feedback::kPositive};
  CHECK(battle::QueueStatusRemovalMask(rule, target, queued) == 0x80000005);
  CHECK(queued.removed == 0x80000015);
  CHECK(queued.added == 3);
  CHECK(queued.feedback == Feedback::kPositive);
  const auto before = queued;
  target.exempt = true;
  CHECK(battle::QueueStatusRemovalMask(rule, target, queued) == 0);
  CHECK(queued == before);
}

TEST_CASE("Strength down queues rule five and negative feedback",
          "[status-queue][F00437]") {
  CheckHandler(battle::QueueStrengthDown, 5, Feedback::kNegative);
}
TEST_CASE("Intelligence down queues rule seven and negative feedback",
          "[status-queue][F00438]") {
  CheckHandler(battle::QueueIntelligenceDown, 7, Feedback::kNegative);
}
TEST_CASE("Agility down queues rule nine and negative feedback",
          "[status-queue][F00439]") {
  CheckHandler(battle::QueueAgilityDown, 9, Feedback::kNegative);
}
TEST_CASE("Equipment down queues rule nineteen and negative feedback",
          "[status-queue][F00440]") {
  CheckHandler(battle::QueueEquipmentDown, 19, Feedback::kNegative);
}
TEST_CASE("Half movement queues rule four and negative feedback",
          "[status-queue][F00441]") {
  CheckHandler(battle::QueueHalfMovement, 4, Feedback::kNegative);
}
TEST_CASE("Strength up queues rule six and positive feedback",
          "[status-queue][F00442]") {
  CheckHandler(battle::QueueStrengthUp, 6, Feedback::kPositive);
}

TEST_CASE(
    "Status queue live primitives preserve signed return bits and hit "
    "neighbors",
    "[status-queue][live]") {
  LiveFixture fixture;
  Battle_statusEffectParams[0].statusFlags =
      std::numeric_limits<std::int32_t>::min();
  auto expected = fixture.hit;
  expected.removeStatuses = std::numeric_limits<std::int32_t>::min();
  CHECK(battle::QueueLiveApplicableStatusChanges(0, &fixture.hit) ==
        std::numeric_limits<std::int32_t>::min());
  CHECK(Bytes(fixture.hit) == Bytes(expected));
  fixture.stats.statuses = std::numeric_limits<std::int32_t>::min();
  expected.inflictStatuses = std::numeric_limits<std::int32_t>::min();
  CHECK(battle::QueueLiveStatusRemovalMask(0, &fixture.hit) ==
        std::numeric_limits<std::int32_t>::min());
  CHECK(Bytes(fixture.hit) == Bytes(expected));
}

TEST_CASE(
    "Status queue live callbacks preserve unused arguments and neighboring "
    "flags",
    "[status-queue][live]") {
  using LiveHandler = int (*)(Battle_ActionDefinition*, Battle_HitResult*,
                              Battle_HitResult*, int, int);
  const std::array<LiveHandler, 6> handlers{
      battle::QueueLiveStrengthDown, battle::QueueLiveIntelligenceDown,
      battle::QueueLiveAgilityDown,  battle::QueueLiveEquipmentDown,
      battle::QueueLiveHalfMovement, battle::QueueLiveStrengthUp};
  const std::array<int, 6> rules{5, 7, 9, 19, 4, 6};
  for (std::size_t index = 0; index < handlers.size(); ++index) {
    LiveFixture fixture;
    Battle_statusEffectParams[rules.at(index)].statusFlags = 0x40000000;
    auto expected = fixture.hit;
    expected.removeStatuses = 0x40000000;
    expected.statStatusEffect = index == 5 ? 2 : 1;
    CHECK(handlers.at(index)(nullptr, nullptr, &fixture.hit, -1, 23) ==
          (index == 5 ? 0x20000 : 0x10000));
    CHECK(Bytes(fixture.hit) == Bytes(expected));
    fixture.stats.actorId = 128;
    CHECK(handlers.at(index)(nullptr, nullptr, &fixture.hit, 0, 0) == 0);
    CHECK(Bytes(fixture.hit) == Bytes(expected));
  }
}

TEST_CASE(
    "Status queue original module entries resolve to checked native bindings",
    "[status-queue][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x80081020) ==
        reinterpret_cast<void*>(&battle::QueueLiveApplicableStatusChanges));
  CHECK(resolve(0x800810CC) ==
        reinterpret_cast<void*>(&battle::QueueLiveStatusRemovalMask));
  CHECK(resolve(0x80083998) ==
        reinterpret_cast<void*>(&battle::QueueLiveStrengthDown));
  CHECK(resolve(0x800839E8) ==
        reinterpret_cast<void*>(&battle::QueueLiveIntelligenceDown));
  CHECK(resolve(0x80083A38) ==
        reinterpret_cast<void*>(&battle::QueueLiveAgilityDown));
  CHECK(resolve(0x80083A88) ==
        reinterpret_cast<void*>(&battle::QueueLiveEquipmentDown));
  CHECK(resolve(0x80083AD8) ==
        reinterpret_cast<void*>(&battle::QueueLiveHalfMovement));
  CHECK(resolve(0x80083B28) ==
        reinterpret_cast<void*>(&battle::QueueLiveStrengthUp));
}
