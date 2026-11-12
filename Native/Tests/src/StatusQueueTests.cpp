#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/StatusQueue.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
}

namespace {
namespace battle = VS::Battle;
using Feedback = battle::StatusFeedback;
constexpr std::size_t kWardRule = 18;
constexpr std::size_t kBanishRule = 29;
constexpr std::uint32_t kHighStatus = 0x80000000;
constexpr int kNegativeExecute = -17;
constexpr unsigned kExemptActor = 128;
constexpr unsigned kRecoveryTimer = 0xABCD;
constexpr unsigned kRecoveryBonus = 43;
using LiveTable = decltype(Battle_actionEffectMediators);
using LiveHandler = std::remove_extent_t<LiveTable>;
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

using MaskHandler = std::optional<std::uint32_t> (*)(
    std::span<const battle::StatusRule>, const battle::StatusTarget&,
    battle::QueuedStatuses&);

void CheckMaskHandler(MaskHandler handler, std::size_t rule_id) {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(rule_id) = {.statuses = 0x80000001, .cancelled = 2};
  battle::StatusTarget target{.active = 2, .immunities = 1};
  battle::QueuedStatuses queued{
      .added = 4, .removed = 8, .feedback = Feedback::kNegative};
  CHECK(handler(rules, target, queued) == 0x80000002);
  CHECK(queued.added == 0x80000004);
  CHECK(queued.removed == 10);
  CHECK(queued.feedback == Feedback::kNegative);
  const auto before = queued;
  CHECK_FALSE(handler(std::span(rules).first(rule_id), target, queued));
  CHECK(queued == before);
  target.exempt = true;
  CHECK(handler(rules, target, queued) == 0);
  CHECK(queued == before);
}

void CheckUnchangedMaskHandler(MaskHandler handler, std::size_t rule_id) {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(rule_id) = {.statuses = 2, .cancelled = 2, .blocking = 4};
  battle::StatusTarget target{.active = 4};
  battle::QueuedStatuses queued{.feedback = Feedback::kNegative};
  const auto before = queued;
  CHECK(handler(rules, target, queued) == 0);
  CHECK(queued == before);
  target = {.immunities = 2};
  CHECK(handler(rules, target, queued) == 0);
  CHECK(queued == before);
  target = {.active = 2};
  CHECK(handler(rules, target, queued) == 2);
  CHECK(queued == battle::QueuedStatuses{2, 2, Feedback::kNegative});
}

void CheckCureHandler(Handler handler, std::size_t rule_id) {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(rule_id) = {
      .statuses = 0x80000005, .cancelled = 8, .blocking = 0xFFFFFFFF};
  battle::StatusTarget target{.active = 0x80000007, .immunities = 0xFFFFFFFF};
  battle::QueuedStatuses queued{
      .added = 0x80000003, .removed = 16, .feedback = Feedback::kNegative};
  CHECK(handler(rules, target, queued) == Feedback::kPositive);
  CHECK(queued ==
        battle::QueuedStatuses{0x80000003, 0x80000015, Feedback::kPositive});
  const auto before = queued;
  CHECK(handler(rules, target, queued) == Feedback::kPositive);
  CHECK(queued == before);
  target.exempt = true;
  CHECK(handler(rules, target, queued) == Feedback::kNone);
  CHECK(queued == before);
  CHECK_FALSE(handler(std::span(rules).first(rule_id), target, queued));
  CHECK(queued == before);
}

void CheckInactiveCure(Handler handler, std::size_t rule_id) {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(rule_id) = {.statuses = kHighStatus, .cancelled = 1};
  for (const auto feedback :
       {Feedback::kNone, Feedback::kNegative, Feedback::kPositive,
        std::bit_cast<Feedback>(std::uint8_t{3})}) {
    battle::QueuedStatuses queued{
        .added = kHighStatus, .removed = 2, .feedback = feedback};
    const auto before = queued;
    CHECK(handler(rules, {.active = 1}, queued) == Feedback::kNone);
    CHECK(queued == before);
  }
}

struct LiveFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_statusEffectParams)) saved_rules =
      std::to_array(Battle_statusEffectParams);
  decltype(std::to_array(Battle_actionEffectMediators)) saved_mediators =
      std::to_array(Battle_actionEffectMediators);
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
    std::ranges::copy(saved_mediators, Battle_actionEffectMediators);
  }
};

template <typename T>
auto Bytes(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

void CheckRelocatedMediator(LiveHandler handler, int rule_id, unsigned feedback,
                            bool removal) {
  LiveFixture fixture;
  const auto mediator =
      std::ranges::find(Battle_actionEffectMediators, handler);
  REQUIRE(mediator != std::end(Battle_actionEffectMediators));
  auto& rule = Battle_statusEffectParams[rule_id];
  rule.statusFlags = std::bit_cast<std::int32_t>(kHighStatus);
  rule.cancelledStatuses = 1;
  rule.blockingStatuses = removal ? -1 : 0;
  fixture.stats.statuses =
      removal ? std::bit_cast<std::int32_t>(kHighStatus | 1) : 1;
  fixture.stats.statusImmunities = removal ? -1 : 0;
  fixture.stats.actionRecoveryTimer = kRecoveryTimer;
  fixture.stats.recoveryDelayBonus = kRecoveryBonus;
  fixture.hit.removeStatuses = 2;
  fixture.hit.inflictStatuses = 4;
  auto expected_hit = fixture.hit;
  expected_hit.removeStatuses =
      std::bit_cast<std::int32_t>(removal ? 2 : kHighStatus | 2);
  expected_hit.inflictStatuses =
      std::bit_cast<std::int32_t>(removal ? kHighStatus | 4 : 5);
  expected_hit.statStatusEffect = feedback;
  auto expected_stats = fixture.stats;
  expected_stats.isAnalyzed = rule_id == kBanishRule;
  (*mediator)(nullptr, nullptr, &fixture.hit, -1, kNegativeExecute);
  CHECK(Bytes(fixture.hit) == Bytes(expected_hit));
  CHECK(Bytes(fixture.stats) == Bytes(expected_stats));
  fixture.stats.actorId = kExemptActor;
  (*mediator)(nullptr, nullptr, &fixture.hit, 0, 0);
  CHECK(Bytes(fixture.hit) == Bytes(expected_hit));
}

void CheckUnchangedHandler(Handler handler, std::size_t rule_id) {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(rule_id) = {.statuses = 2, .blocking = 4};
  battle::StatusTarget target{.active = 4};
  battle::QueuedStatuses queued{
      .added = kHighStatus, .removed = 1, .feedback = Feedback::kNegative};
  const auto before = queued;
  CHECK(handler(rules, target, queued) == Feedback::kNone);
  CHECK(queued == before);
  target = {.immunities = 2};
  CHECK(handler(rules, target, queued) == Feedback::kNone);
  CHECK(queued == before);
  target = {.active = 2};
  CHECK(handler(rules, target, queued) == Feedback::kNone);
  CHECK(queued == before);
}

std::vector<std::uint8_t> ReadOriginalBattleModule() {
  for (auto directory = std::filesystem::current_path();
       directory != directory.parent_path();
       directory = directory.parent_path()) {
    const auto root = directory / "Native" / "data";
    if (!std::filesystem::exists(root / VS::Content::DataStore::ManifestName)) {
      continue;
    }
    REQUIRE(VS::Content::DataStore::Open(root));
    const auto* store = VS::Content::DataStore::Get();
    REQUIRE(store != nullptr);
    const auto* entry = store->Find("BATTLE/BATTLE.PRG");
    REQUIRE(entry != nullptr);
    const auto image = store->ReadAll(*entry);
    REQUIRE(image.has_value());
    return image.value();
  }
  SKIP("Extracted Native/data is unavailable");
  return {};
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

TEST_CASE("Intelligence up queues rule eight and positive feedback",
          "[status-queue][F00443]") {
  CheckHandler(battle::QueueIntelligenceUp, 8, Feedback::kPositive);
  CheckUnchangedHandler(battle::QueueIntelligenceUp, 8);
}
TEST_CASE("Agility up queues rule ten and positive feedback",
          "[status-queue][F00444]") {
  CheckHandler(battle::QueueAgilityUp, 10, Feedback::kPositive);
  CheckUnchangedHandler(battle::QueueAgilityUp, 10);
}
TEST_CASE("Equipment up queues rule twenty and positive feedback",
          "[status-queue][F00445]") {
  CheckHandler(battle::QueueEquipmentUp, 20, Feedback::kPositive);
  CheckUnchangedHandler(battle::QueueEquipmentUp, 20);
}
TEST_CASE("Quicken queues rule eleven and positive feedback",
          "[status-queue][F00446]") {
  CheckHandler(battle::QueueQuicken, 11, Feedback::kPositive);
  CheckUnchangedHandler(battle::QueueQuicken, 11);
}
TEST_CASE("Silence queues rule twelve and negative feedback",
          "[status-queue][F00447]") {
  CheckHandler(battle::QueueSilence, 12, Feedback::kNegative);
  CheckUnchangedHandler(battle::QueueSilence, 12);
}
TEST_CASE("Regeneration queues rule seventeen and positive feedback",
          "[status-queue][F00449]") {
  CheckHandler(battle::QueueRegeneration, 17, Feedback::kPositive);
  CheckUnchangedHandler(battle::QueueRegeneration, 17);
}

TEST_CASE("Magic ward returns the raw rule mask without feedback",
          "[status-queue][F00448]") {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(kWardRule) = {.statuses = kHighStatus, .cancelled = 4};
  battle::StatusTarget target{.active = 4};
  battle::QueuedStatuses queued{.added = 1, .feedback = Feedback::kNegative};
  CHECK(battle::QueueMagicWard(rules, target, queued) == (kHighStatus | 4));
  CHECK(queued ==
        battle::QueuedStatuses{kHighStatus | 1, 4, Feedback::kNegative});
  const auto before = queued;
  target.exempt = true;
  CHECK(battle::QueueMagicWard(rules, target, queued) == 0);
  CHECK(queued == before);
  CHECK_FALSE(battle::QueueMagicWard(std::span(rules).first(kWardRule), target,
                                     queued));
  CHECK(queued == before);
}

TEST_CASE("Magic ward blocking immunity and reapplication keep feedback",
          "[status-queue][F00448]") {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(kWardRule) = {.statuses = 2, .cancelled = 2, .blocking = 4};
  battle::StatusTarget target{.active = 4};
  battle::QueuedStatuses queued{.feedback = Feedback::kPositive};
  CHECK(battle::QueueMagicWard(rules, target, queued) == 0);
  target = {.immunities = 2};
  CHECK(battle::QueueMagicWard(rules, target, queued) == 0);
  target = {.active = 2};
  CHECK(battle::QueueMagicWard(rules, target, queued) == 2);
  CHECK(queued == battle::QueuedStatuses{2, 2, Feedback::kPositive});
}

TEST_CASE("Banish separates preview status changes from execution analysis",
          "[status-queue][F00450]") {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(kBanishRule) = {.statuses = kHighStatus, .cancelled = 4};
  const battle::StatusTarget target{.active = 4};
  battle::QueuedStatuses queued{.added = 1, .feedback = Feedback::kNegative};
  CHECK(battle::QueueBanish(rules, target, queued, false) ==
        battle::BanishOutcome{kHighStatus | 4, false});
  CHECK(queued ==
        battle::QueuedStatuses{kHighStatus | 1, 4, Feedback::kNegative});
  const auto before = queued;
  CHECK(battle::QueueBanish(rules, target, queued, true) ==
        battle::BanishOutcome{kHighStatus | 4, true});
  CHECK(queued == before);
  CHECK_FALSE(battle::QueueBanish(std::span(rules).first(kBanishRule), target,
                                  queued, true));
  CHECK(queued == before);
}

TEST_CASE("Banish analysis is independent of blocking immunity and exemption",
          "[status-queue][F00450]") {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  rules.at(kBanishRule) = {.statuses = 2, .blocking = 4};
  const std::array<battle::StatusTarget, 4> targets{
      battle::StatusTarget{.exempt = true},
      {.active = 4},
      {.immunities = 2},
      {.active = 2}};
  for (const auto& target : targets) {
    battle::QueuedStatuses queued{
        .added = 1, .removed = 4, .feedback = Feedback::kPositive};
    const auto before = queued;
    CHECK(battle::QueueBanish(rules, target, queued, false) ==
          battle::BanishOutcome{0, false});
    CHECK(battle::QueueBanish(rules, target, queued, true) ==
          battle::BanishOutcome{0, true});
    CHECK(queued == before);
  }
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
    handlers.at(index)(nullptr, nullptr, &fixture.hit, -1, 23);
    CHECK(Bytes(fixture.hit) == Bytes(expected));
    fixture.stats.actorId = 128;
    handlers.at(index)(nullptr, nullptr, &fixture.hit, 0, 0);
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

TEST_CASE("New status callbacks preserve all hit and stats neighbors",
          "[status-queue][live]") {
  const std::array<LiveHandler, 6> handlers{
      battle::QueueLiveIntelligenceUp, battle::QueueLiveAgilityUp,
      battle::QueueLiveEquipmentUp,    battle::QueueLiveQuicken,
      battle::QueueLiveSilence,        battle::QueueLiveRegeneration};
  const std::array<int, 6> rules{8, 10, 20, 11, 12, 17};
  constexpr std::size_t kSilenceIndex = 4;
  for (std::size_t index = 0; index < handlers.size(); ++index) {
    LiveFixture fixture;
    fixture.stats.actionRecoveryTimer = kRecoveryTimer;
    fixture.stats.recoveryDelayBonus = kRecoveryBonus;
    const auto before_stats = Bytes(fixture.stats);
    Battle_statusEffectParams[rules.at(index)].statusFlags = 0x40000000;
    auto expected = fixture.hit;
    expected.removeStatuses = 0x40000000;
    expected.statStatusEffect = index == kSilenceIndex ? 1 : 2;
    handlers.at(index)(nullptr, nullptr, &fixture.hit, -1, kNegativeExecute);
    CHECK(Bytes(fixture.hit) == Bytes(expected));
    CHECK(Bytes(fixture.stats) == before_stats);
    Battle_statusEffectParams[rules.at(index)].blockingStatuses = 1;
    fixture.stats.statuses = 1;
    handlers.at(index)(nullptr, nullptr, &fixture.hit, 0, 0);
    CHECK(Bytes(fixture.hit) == Bytes(expected));
  }
}

TEST_CASE("Live magic ward preserves feedback and the complete stats record",
          "[status-queue][F00448][live]") {
  LiveFixture fixture;
  fixture.stats.actionRecoveryTimer = kRecoveryTimer;
  fixture.stats.cancelsHitResults = 1;
  fixture.stats.recoveryDelayBonus = kRecoveryBonus;
  Battle_statusEffectParams[kWardRule].statusFlags =
      std::bit_cast<std::int32_t>(kHighStatus);
  const auto before_stats = Bytes(fixture.stats);
  auto expected = fixture.hit;
  expected.removeStatuses = std::bit_cast<std::int32_t>(kHighStatus);
  battle::QueueLiveMagicWard(nullptr, nullptr, &fixture.hit, -1,
                             kNegativeExecute);
  CHECK(Bytes(fixture.hit) == Bytes(expected));
  CHECK(Bytes(fixture.stats) == before_stats);
  fixture.stats.actorId = kExemptActor;
  battle::QueueLiveMagicWard(nullptr, nullptr, &fixture.hit, 0, 1);
  CHECK(Bytes(fixture.hit) == Bytes(expected));
  CHECK(fixture.stats.isAnalyzed == 0);
}

TEST_CASE("Live Banish preview queues status changes without analyzing",
          "[status-queue][F00450][live]") {
  for (const unsigned analyzed : {0U, 1U}) {
    LiveFixture fixture;
    fixture.stats.isAnalyzed = static_cast<u_char>(analyzed);
    fixture.stats.actionRecoveryTimer = kRecoveryTimer;
    fixture.stats.cancelsHitResults = 1;
    fixture.stats.recoveryDelayBonus = kRecoveryBonus;
    Battle_statusEffectParams[kBanishRule].statusFlags =
        std::bit_cast<std::int32_t>(kHighStatus);
    const auto before_stats = Bytes(fixture.stats);
    auto expected = fixture.hit;
    expected.removeStatuses = std::bit_cast<std::int32_t>(kHighStatus);
    battle::QueueLiveBanish(nullptr, nullptr, &fixture.hit, -1, 0);
    CHECK(Bytes(fixture.hit) == Bytes(expected));
    CHECK(Bytes(fixture.stats) == before_stats);
  }
}

TEST_CASE(
    "Live Banish nonzero execution changes only analysis and preserves stats "
    "neighbors",
    "[status-queue][F00450][live]") {
  for (const int execute :
       {1, kNegativeExecute, std::numeric_limits<int>::min()}) {
    LiveFixture fixture;
    fixture.stats.actionRecoveryTimer = kRecoveryTimer;
    fixture.stats.cancelsHitResults = 1;
    fixture.stats.recoveryDelayBonus = kRecoveryBonus;
    fixture.stats.statusTimers[7] = 0xA5;
    auto expected_stats = fixture.stats;
    expected_stats.isAnalyzed = 1;
    const auto before_hit = Bytes(fixture.hit);
    battle::QueueLiveBanish(nullptr, nullptr, &fixture.hit, -1, execute);
    CHECK(Bytes(fixture.hit) == before_hit);
    CHECK(Bytes(fixture.stats) == Bytes(expected_stats));
    battle::QueueLiveBanish(nullptr, nullptr, &fixture.hit, 0, execute);
    CHECK(Bytes(fixture.stats) == Bytes(expected_stats));
  }
}

TEST_CASE(
    "Live Banish analyzes exempt and blocked actors independently of table "
    "slots",
    "[status-queue][F00450][live]") {
  LiveFixture fixture;
  Battle_statusEffectParams[kBanishRule].statusFlags = 2;
  fixture.stats.actorId = kExemptActor;
  const auto before_hit = Bytes(fixture.hit);
  battle::QueueLiveBanish(nullptr, nullptr, &fixture.hit, 0, 1);
  CHECK(Bytes(fixture.hit) == before_hit);
  CHECK(fixture.stats.isAnalyzed == 1);
  fixture.stats.actorId = 1;
  fixture.stats.isAnalyzed = 0;
  fixture.stats.statuses = 4;
  Battle_statusEffectParams[kBanishRule].blockingStatuses = 4;
  battle::QueueLiveBanish(nullptr, nullptr, &fixture.hit, 0, kNegativeExecute);
  CHECK(Bytes(fixture.hit) == before_hit);
  CHECK(fixture.stats.isAnalyzed == 1);
  Battle_actors[0] = &fixture.actor;
  fixture.hit.target.targetActor = 0;
  fixture.stats.statuses = 0;
  battle::QueueLiveBanish(nullptr, nullptr, &fixture.hit, 0, 0);
  CHECK(fixture.hit.removeStatuses == 2);
}

TEST_CASE("New status module entries resolve to their checked native callbacks",
          "[status-queue][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x80083B78) ==
        reinterpret_cast<void*>(&battle::QueueLiveIntelligenceUp));
  CHECK(resolve(0x80083BC8) ==
        reinterpret_cast<void*>(&battle::QueueLiveAgilityUp));
  CHECK(resolve(0x80083C18) ==
        reinterpret_cast<void*>(&battle::QueueLiveEquipmentUp));
  CHECK(resolve(0x80083C68) ==
        reinterpret_cast<void*>(&battle::QueueLiveQuicken));
  CHECK(resolve(0x80083CB8) ==
        reinterpret_cast<void*>(&battle::QueueLiveSilence));
  CHECK(resolve(0x80083D08) ==
        reinterpret_cast<void*>(&battle::QueueLiveMagicWard));
  CHECK(resolve(0x80083D2C) ==
        reinterpret_cast<void*>(&battle::QueueLiveRegeneration));
  CHECK(resolve(0x80083D7C) ==
        reinterpret_cast<void*>(&battle::QueueLiveBanish));
}

TEST_CASE(
    "Original module mediators dispatch all promoted status callbacks as void",
    "[status-queue][live][module][issue-349]") {
  constexpr std::array<LiveHandler, 30> kLiveHandlers{
      battle::QueueLiveStrengthDown,   battle::QueueLiveIntelligenceDown,
      battle::QueueLiveAgilityDown,    battle::QueueLiveEquipmentDown,
      battle::QueueLiveHalfMovement,   battle::QueueLiveStrengthUp,
      battle::QueueLiveIntelligenceUp, battle::QueueLiveAgilityUp,
      battle::QueueLiveEquipmentUp,    battle::QueueLiveQuicken,
      battle::QueueLiveSilence,        battle::QueueLiveMagicWard,
      battle::QueueLiveRegeneration,   battle::QueueLiveBanish,
      battle::QueueLiveParalysis,      battle::QueueLivePoison,
      battle::QueueLiveNumbness,       battle::QueueLiveCurse,
      battle::QueueLiveAirAttackUp,    battle::QueueLiveFireAttackUp,
      battle::QueueLiveEarthAttackUp,  battle::QueueLiveWaterAttackUp,
      battle::QueueLiveAirDefenseUp,   battle::QueueLiveFireDefenseUp,
      battle::QueueLiveEarthDefenseUp, battle::QueueLiveWaterDefenseUp,
      battle::QueueLiveParalysisCure,  battle::QueueLivePoisonCure,
      battle::QueueLiveCurseCure,      battle::QueueLiveNumbnessCure};
  VS::Content::DataStore::ScopedOverride store;
  const auto image = ReadOriginalBattleModule();
  LiveFixture fixture;
  const auto tables =
      std::span(Content_modulePointers, Content_modulePointerCount);
  const auto table = std::ranges::find(
      tables, reinterpret_cast<void**>(Battle_actionEffectMediators),
      &Content_ModulePointerTable::target);
  REQUIRE(table != tables.end());
  REQUIRE(VS::Content::RelocateModulePointerTable(
      *table, image, {Content_moduleSymbols, Content_moduleSymbolCount}));
  constexpr std::array<int, 30> kRules{5,  7,  9,  19, 4,  6,  8,  10, 20, 11,
                                       12, 18, 17, 29, 13, 14, 15, 16, 21, 22,
                                       23, 24, 25, 26, 27, 28, 13, 14, 16, 15};
  constexpr std::array<unsigned, 30> kFeedback{1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
                                               1, 3, 2, 3, 1, 1, 1, 1, 3, 3,
                                               3, 3, 3, 3, 3, 3, 2, 2, 2, 2};
  constexpr std::size_t kFirstCureIndex = 26;
  for (std::size_t index = 0; index < kLiveHandlers.size(); ++index) {
    CheckRelocatedMediator(kLiveHandlers.at(index), kRules.at(index),
                           kFeedback.at(index), index >= kFirstCureIndex);
  }
}

TEST_CASE("Paralysis queues rule thirteen and negative feedback",
          "[status-queue][F00451]") {
  CheckHandler(battle::QueueParalysis, 13, Feedback::kNegative);
}
TEST_CASE("Poison queues rule fourteen and negative feedback",
          "[status-queue][F00452]") {
  CheckHandler(battle::QueuePoison, 14, Feedback::kNegative);
}
TEST_CASE("Numbness queues rule fifteen and negative feedback",
          "[status-queue][F00453]") {
  CheckHandler(battle::QueueNumbness, 15, Feedback::kNegative);
}
TEST_CASE("Curse queues rule sixteen and negative feedback",
          "[status-queue][F00454]") {
  CheckHandler(battle::QueueCurse, 16, Feedback::kNegative);
}
TEST_CASE("Air attack enhancement queues rule twenty-one without feedback",
          "[status-queue][F00455]") {
  CheckMaskHandler(battle::QueueAirAttackUp, 21);
}
TEST_CASE("Fire attack enhancement queues rule twenty-two without feedback",
          "[status-queue][F00456]") {
  CheckMaskHandler(battle::QueueFireAttackUp, 22);
}
TEST_CASE("Earth attack enhancement queues rule twenty-three without feedback",
          "[status-queue][F00457]") {
  CheckMaskHandler(battle::QueueEarthAttackUp, 23);
}
TEST_CASE("Water attack enhancement queues rule twenty-four without feedback",
          "[status-queue][F00458]") {
  CheckMaskHandler(battle::QueueWaterAttackUp, 24);
}
TEST_CASE(
    "Ailment and attack enhancement module entries keep their original "
    "bindings",
    "[status-queue][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x80083DEC) ==
        reinterpret_cast<void*>(&battle::QueueLiveParalysis));
  CHECK(resolve(0x80083E3C) ==
        reinterpret_cast<void*>(&battle::QueueLivePoison));
  CHECK(resolve(0x80083E8C) ==
        reinterpret_cast<void*>(&battle::QueueLiveNumbness));
  CHECK(resolve(0x80083EDC) ==
        reinterpret_cast<void*>(&battle::QueueLiveCurse));
  CHECK(resolve(0x80083F2C) ==
        reinterpret_cast<void*>(&battle::QueueLiveAirAttackUp));
  CHECK(resolve(0x80083F50) ==
        reinterpret_cast<void*>(&battle::QueueLiveFireAttackUp));
  CHECK(resolve(0x80083F74) ==
        reinterpret_cast<void*>(&battle::QueueLiveEarthAttackUp));
  CHECK(resolve(0x80083F98) ==
        reinterpret_cast<void*>(&battle::QueueLiveWaterAttackUp));
}

TEST_CASE("Air defense enhancement queues rule twenty-five without feedback",
          "[status-queue][F00459]") {
  CheckMaskHandler(battle::QueueAirDefenseUp, 25);
  CheckUnchangedMaskHandler(battle::QueueAirDefenseUp, 25);
}
TEST_CASE("Fire defense enhancement queues rule twenty-six without feedback",
          "[status-queue][F00460]") {
  CheckMaskHandler(battle::QueueFireDefenseUp, 26);
  CheckUnchangedMaskHandler(battle::QueueFireDefenseUp, 26);
}
TEST_CASE("Earth defense enhancement queues rule twenty-seven without feedback",
          "[status-queue][F00461]") {
  CheckMaskHandler(battle::QueueEarthDefenseUp, 27);
  CheckUnchangedMaskHandler(battle::QueueEarthDefenseUp, 27);
}
TEST_CASE("Water defense enhancement queues rule twenty-eight without feedback",
          "[status-queue][F00462]") {
  CheckMaskHandler(battle::QueueWaterDefenseUp, 28);
  CheckUnchangedMaskHandler(battle::QueueWaterDefenseUp, 28);
}
TEST_CASE(
    "Paralysis cure removes rule thirteen with conditional positive feedback",
    "[status-queue][F00463]") {
  CheckCureHandler(battle::QueueParalysisCure, 13);
  CheckInactiveCure(battle::QueueParalysisCure, 13);
}
TEST_CASE(
    "Poison cure removes rule fourteen with conditional positive feedback",
    "[status-queue][F00464]") {
  CheckCureHandler(battle::QueuePoisonCure, 14);
  CheckInactiveCure(battle::QueuePoisonCure, 14);
}
TEST_CASE("Curse cure removes rule sixteen with conditional positive feedback",
          "[status-queue][F00465]") {
  CheckCureHandler(battle::QueueCurseCure, 16);
  CheckInactiveCure(battle::QueueCurseCure, 16);
}
TEST_CASE(
    "Numbness cure removes rule fifteen with conditional positive feedback",
    "[status-queue][F00466]") {
  CheckCureHandler(battle::QueueNumbnessCure, 15);
  CheckInactiveCure(battle::QueueNumbnessCure, 15);
}

TEST_CASE("Defense and cure module addresses keep their checked callbacks",
          "[status-queue][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x80083FBC) ==
        reinterpret_cast<void*>(&battle::QueueLiveAirDefenseUp));
  CHECK(resolve(0x80083FE0) ==
        reinterpret_cast<void*>(&battle::QueueLiveFireDefenseUp));
  CHECK(resolve(0x80084004) ==
        reinterpret_cast<void*>(&battle::QueueLiveEarthDefenseUp));
  CHECK(resolve(0x80084028) ==
        reinterpret_cast<void*>(&battle::QueueLiveWaterDefenseUp));
  CHECK(resolve(0x8008404C) ==
        reinterpret_cast<void*>(&battle::QueueLiveParalysisCure));
  CHECK(resolve(0x8008409C) ==
        reinterpret_cast<void*>(&battle::QueueLivePoisonCure));
  CHECK(resolve(0x800840EC) ==
        reinterpret_cast<void*>(&battle::QueueLiveCurseCure));
  CHECK(resolve(0x8008413C) ==
        reinterpret_cast<void*>(&battle::QueueLiveNumbnessCure));
}
