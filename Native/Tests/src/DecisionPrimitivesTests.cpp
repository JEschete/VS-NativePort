#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/DecisionPrimitives.hpp"

namespace {
namespace ai = VS::Battle::Ai;
using ai::CandidateOption;

ai::ActionCandidate ReadyCandidate() {
  ai::ActionCandidate candidate;
  candidate.SetTargetId(3);
  candidate.SetAction(42);
  candidate.SetWeight(40);
  candidate.SetThreshold(5);
  candidate.SetDistanceSquared(25);
  return candidate;
}

class FakeExecutionHost final : public ai::ActionExecutionHost {
 public:
  [[nodiscard]] std::uint16_t ReadinessLevel() const override {
    return readiness;
  }
  [[nodiscard]] ai::ActionCooldown Cooldown() const override {
    return cooldown;
  }
  [[nodiscard]] bool TargetIsGimmick(unsigned target) const override {
    targets.push_back(target);
    return gimmick;
  }
  [[nodiscard]] std::int32_t DistanceSquared(
      unsigned /*target*/) const override {
    return distance_squared;
  }
  [[nodiscard]] ai::TargetRelation Relation(
      unsigned /*target*/) const override {
    return relation;
  }
  [[nodiscard]] std::optional<bool> HostileNearGimmick(
      unsigned /*target*/) const override {
    return hostile_near;
  }
  [[nodiscard]] std::optional<bool> AshleyMoving() const override {
    return ashley_moving;
  }

  std::uint16_t readiness = 5;
  ai::ActionCooldown cooldown;
  bool gimmick = false;
  std::int32_t distance_squared = 25;
  ai::TargetRelation relation{.ready = true, .in_sight = true};
  std::optional<bool> hostile_near = true;
  std::optional<bool> ashley_moving = false;
  mutable std::vector<unsigned> targets;
};

class FakeFallbackCache final : public ai::FallbackCandidateCache {
 public:
  [[nodiscard]] std::uint32_t SlotMask() const override { return mask; }
  void SetSlotMask(std::uint32_t value) override { mask = value; }
  [[nodiscard]] int Count() const override { return count; }
  [[nodiscard]] bool Store(std::size_t index,
                           const ai::ActionCandidate& candidate) override {
    if (!allocated) return false;
    stored.at(index) = candidate;
    return true;
  }
  void SetCount(int value) override { count = value; }

  std::uint32_t mask = 0;
  int count = 0;
  bool allocated = true;
  std::array<ai::ActionCandidate, ai::kFallbackCandidateCapacity> stored{};
};

class FakeRandom final : public ai::CandidateRandom {
 public:
  explicit FakeRandom(std::vector<int> values) : values_(std::move(values)) {}

  [[nodiscard]] int Next() override { return values_.at(draws_++); }
  [[nodiscard]] std::size_t Draws() const { return draws_; }

 private:
  std::vector<int> values_;
  std::size_t draws_ = 0;
};

std::vector<ai::RankedCandidate> Ranking(std::span<const int> scores,
                                         unsigned weight) {
  std::vector<ai::RankedCandidate> ranking;
  for (std::size_t index = 0; index < scores.size(); ++index) {
    ranking.push_back({.score = scores[index],
                       .action = 42,
                       .weight = weight,
                       .handle = index});
  }
  return ranking;
}

std::vector<std::size_t> Handles(std::span<const ai::RankedCandidate> ranking) {
  std::vector<std::size_t> handles;
  for (const ai::RankedCandidate& entry : ranking) {
    handles.push_back(entry.handle);
  }
  return handles;
}
}  // namespace

TEST_CASE(
    "Action cooldown applies to the room delay or a harmful action on an "
    "empty slot",
    "[ai][decisions]") {
  const bool room_delay = GENERATE(false, true);
  const bool harmful = GENERATE(false, true);
  const bool gimmick = GENERATE(false, true);
  const std::uint16_t action_delay = GENERATE(0, 1, 0xFFFF);
  const std::uint8_t minimum = GENERATE(0, 1, 255);
  ai::ActionCandidate candidate;
  candidate.SetOption(CandidateOption::kHarmful, harmful);
  const ai::ActionCooldown cooldown{.uses_room_action_delay = room_delay,
                                    .action_delay = action_delay,
                                    .minimum_action_delay = minimum};
  const bool required = room_delay || (harmful && !gimmick);
  CHECK(ai::DoesActionRequireCooldown(cooldown, candidate, gimmick) ==
        required);
  CHECK(ai::IsActionCooldownReady(cooldown, candidate, gimmick) ==
        (action_delay == 0 && (!required || minimum == 0)));
}

TEST_CASE("Gimmick proximity finds a hostile actor on a neighbouring tile",
          "[ai][decisions]") {
  const int dx = GENERATE(-2, -1, 0, 1, 2);
  const int dz = GENERATE(-2, -1, 0, 1, 2);
  const std::array<ai::ActorProximity, 1> actors{
      {{.classification = 3,
        .tile = ai::GridTile{.x = static_cast<std::uint8_t>(10 + dx),
                             .z = static_cast<std::uint8_t>(10 + dz)}}}};
  CHECK(ai::HasNearbyEnemy({.x = 10, .z = 10}, actors) ==
        std::optional<bool>{(dx * dx) + (dz * dz) < 3});
}

TEST_CASE("Gimmick proximity compares tiles as unsigned bytes",
          "[ai][decisions]") {
  const std::array<ai::ActorProximity, 1> far{
      {{.classification = 3, .tile = ai::GridTile{.x = 0, .z = 0}}}};
  CHECK(ai::HasNearbyEnemy({.x = 255, .z = 255}, far) ==
        std::optional<bool>{false});
  const std::array<ai::ActorProximity, 1> near{
      {{.classification = 3, .tile = ai::GridTile{.x = 254, .z = 255}}}};
  CHECK(ai::HasNearbyEnemy({.x = 255, .z = 254}, near) ==
        std::optional<bool>{true});
}

TEST_CASE(
    "Gimmick proximity ignores non-hostile actors and refuses a hostile actor "
    "without a model",
    "[ai][decisions]") {
  const ai::GridTile gimmick{.x = 10, .z = 10};
  const ai::ActorProximity near_hostile{.classification = 3, .tile = gimmick};
  const ai::ActorProximity modelless_hostile{.classification = 3,
                                             .tile = std::nullopt};
  const std::array<ai::ActorProximity, 4> friendly{
      {{.classification = 0, .tile = gimmick},
       {.classification = 1, .tile = gimmick},
       {.classification = 2, .tile = gimmick},
       {.classification = 2, .tile = std::nullopt}}};
  CHECK(ai::HasNearbyEnemy(gimmick, friendly) == std::optional<bool>{false});
  CHECK(ai::HasNearbyEnemy(gimmick, {}) == std::optional<bool>{false});
  const std::array<ai::ActorProximity, 2> near_first{near_hostile,
                                                     modelless_hostile};
  CHECK(ai::HasNearbyEnemy(gimmick, near_first) == std::optional<bool>{true});
  const std::array<ai::ActorProximity, 2> modelless_first{modelless_hostile,
                                                          near_hostile};
  CHECK_FALSE(ai::HasNearbyEnemy(gimmick, modelless_first).has_value());
}

TEST_CASE(
    "Candidate execution requires a weight, a ready room and an elapsed delay",
    "[ai][decisions]") {
  const int condition = GENERATE(0, 1, 2, 3, 4, 5);
  FakeExecutionHost host;
  ai::ActionCandidate candidate = ReadyCandidate();
  if (condition == 1) candidate.SetWeight(0);
  if (condition == 2) host.readiness = 4;
  if (condition == 3) host.cooldown.action_delay = 1;
  if (condition == 4) {
    candidate.SetOption(CandidateOption::kHarmful, true);
    host.cooldown.minimum_action_delay = 1;
  }
  if (condition == 5) host.readiness = 0xFFFF;
  CHECK(ai::CanExecuteActionCandidate(host, candidate) ==
        std::optional<bool>{condition == 0 || condition == 5});
  CHECK(host.targets == std::vector<unsigned>{3});
}

TEST_CASE(
    "Candidate execution keeps the target inside its range and outside its "
    "minimum range",
    "[ai][decisions]") {
  const std::int32_t distance = GENERATE(-1, 0, 24, 25, 26);
  const bool minimum_range = GENERATE(false, true);
  FakeExecutionHost host;
  host.distance_squared = distance;
  ai::ActionCandidate candidate = ReadyCandidate();
  candidate.SetRange(5);
  candidate.SetOption(CandidateOption::kMinimumRange, minimum_range);
  const bool reachable = distance <= 25 && !(minimum_range && distance < 25);
  CHECK(ai::CanExecuteActionCandidate(host, candidate) ==
        std::optional<bool>{reachable});
}

TEST_CASE(
    "Candidate execution reads the squared minimum range as a wrapping "
    "signed product",
    "[ai][decisions]") {
  const unsigned range = GENERATE(0u, 46340u, 46341u, 0xFFFFu);
  FakeExecutionHost host;
  ai::ActionCandidate candidate = ReadyCandidate();
  candidate.SetDistanceSquared(std::numeric_limits<std::int32_t>::max());
  candidate.SetRange(range);
  candidate.SetOption(CandidateOption::kMinimumRange, true);
  // 46340 squared is positive; 46341 and 0xFFFF squared wrap negative.
  CHECK(ai::CanExecuteActionCandidate(host, candidate) ==
        std::optional<bool>{range != 46340u});
}

TEST_CASE(
    "Candidate execution on a gimmick needs its action range, no room delay "
    "and a nearby hostile",
    "[ai][decisions]") {
  const int condition = GENERATE(0, 1, 2, 3, 4, 5);
  FakeExecutionHost host;
  host.gimmick = true;
  host.distance_squared = ai::kGimmickActionRangeSquared;
  ai::ActionCandidate candidate = ReadyCandidate();
  candidate.SetDistanceSquared(0x20000);
  candidate.SetOption(CandidateOption::kHarmful, true);
  candidate.SetOption(CandidateOption::kMinimumRange, true);
  candidate.SetRange(0x100);
  host.cooldown.minimum_action_delay = 1;
  if (condition == 1) ++host.distance_squared;
  if (condition == 2) {
    host.cooldown.uses_room_action_delay = true;
    host.cooldown.minimum_action_delay = 0;
  }
  if (condition == 3) host.hostile_near = false;
  if (condition == 4) host.hostile_near.reset();
  if (condition == 5) candidate.SetRange(0x200);
  const std::optional<bool> executable =
      ai::CanExecuteActionCandidate(host, candidate);
  if (condition == 4) {
    CHECK_FALSE(executable.has_value());
  } else {
    // A gimmick target needs no cooldown, but the minimum range still holds.
    CHECK(executable == std::optional<bool>{condition == 0});
  }
}

TEST_CASE(
    "Candidate execution needs a ready relation, sight, a clear path and a "
    "standing Ashley",
    "[ai][decisions]") {
  const bool ready = GENERATE(false, true);
  const bool in_sight = GENERATE(false, true);
  const bool obstructed = GENERATE(false, true);
  const bool ignores_sight = GENERATE(false, true);
  const bool ignores_obstruction = GENERATE(false, true);
  const bool ashley_moving = GENERATE(false, true);
  FakeExecutionHost host;
  host.relation = {
      .ready = ready, .in_sight = in_sight, .obstructed = obstructed};
  host.ashley_moving = ashley_moving;
  ai::ActionCandidate candidate = ReadyCandidate();
  candidate.SetOption(CandidateOption::kIgnoresSight, ignores_sight);
  candidate.SetOption(CandidateOption::kIgnoresObstruction,
                      ignores_obstruction);
  CHECK(ai::CanExecuteActionCandidate(host, candidate) ==
        std::optional<bool>{ready && !ashley_moving &&
                            (in_sight || ignores_sight) &&
                            (!obstructed || ignores_obstruction)});
}

TEST_CASE("Candidate execution refuses a ready target without Ashley's model",
          "[ai][decisions]") {
  FakeExecutionHost host;
  host.ashley_moving.reset();
  CHECK_FALSE(
      ai::CanExecuteActionCandidate(host, ReadyCandidate()).has_value());
  host.relation.ready = false;
  CHECK(ai::CanExecuteActionCandidate(host, ReadyCandidate()) ==
        std::optional<bool>{false});
}

TEST_CASE(
    "Condition scores wrap the status weight and refuse an overflowing total",
    "[ai][decisions]") {
  constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
  constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
  const auto wraps_low = static_cast<std::int32_t>(0xEAAAAAABu);
  CHECK(ai::ScoreActorCondition({1, 2, 3, 4}) ==
        std::optional<std::int32_t>{25});
  CHECK(ai::ScoreActorCondition({kMax, 0, 0, 0}) ==
        std::optional<std::int32_t>{-6});
  CHECK(ai::ScoreActorCondition({kMin, 0, 0, 0}) ==
        std::optional<std::int32_t>{0});
  CHECK(ai::ScoreActorCondition({0, -32768, -32768, -32768}) ==
        std::optional<std::int32_t>{-229376});
  CHECK(ai::ScoreActorCondition({wraps_low, 0, -2, 0}) ==
        std::optional<std::int32_t>{kMin});
  CHECK_FALSE(ai::ScoreActorCondition({wraps_low, 0, -3, 0}).has_value());
  CHECK_FALSE(
      ai::ScoreActorCondition({0x15555555, 32767, 32767, 32767}).has_value());
}

TEST_CASE("Targeting flags invert only action 0x60 against class-two enemies",
          "[ai][decisions]") {
  const std::uint16_t flags = GENERATE(0, 0x4000, 0x8000, 0xC000);
  const unsigned action = GENERATE(0x5Fu, 0x60u, 0x61u);
  const unsigned enemy_class = GENERATE(0u, 1u, 2u, 3u);
  const bool special = action == 0x60 && enemy_class == 2;
  const unsigned harmful = flags >> 15;
  const unsigned beneficial = flags >> 14;
  CHECK(ai::IsActionHarmfulToActor(action, flags, enemy_class) ==
        (special ? harmful ^ 1u : harmful & 1u));
  CHECK(ai::IsActionBeneficialToActor(action, flags, enemy_class) ==
        (special ? beneficial ^ 1u : beneficial & 1u));
}

TEST_CASE(
    "Beneficial flags for action 0x60 against class two keep the harmful bit",
    "[ai][decisions]") {
  CHECK(ai::IsActionBeneficialToActor(0x60, 0xC000, 2) == 2);
  CHECK(ai::IsActionBeneficialToActor(0x60, 0x8000, 2) == 3);
  CHECK(ai::IsActionBeneficialToActor(0x60, 0x4000, 2) == 0);
  CHECK(ai::IsActionHarmfulToActor(0x60, 0x7FFF, 2) == 1);
}

TEST_CASE(
    "Fallback caching keeps the first candidate per limb slot and marks full "
    "caches",
    "[ai][decisions]") {
  FakeFallbackCache cache;
  ai::ActionCandidate candidate = ReadyCandidate();
  CHECK(ai::CacheFallbackActionCandidate(cache, candidate, false) ==
        std::optional<bool>{false});
  CHECK(cache.mask == 0);
  std::vector<std::optional<bool>> cached;
  std::vector<std::optional<bool>> expected;
  for (unsigned slot = 0; slot < 24; ++slot) {
    candidate.SetSlot(slot / 4, slot % 4);
    candidate.SetScore(static_cast<std::int32_t>(slot));
    cached.push_back(ai::CacheFallbackActionCandidate(cache, candidate, true));
    expected.emplace_back(slot < 16);
  }
  CHECK(cached == expected);
  CHECK(cache.mask == 0xFFFFFF);
  CHECK(cache.count == 16);
  CHECK(cache.stored[15].Score() == 15);
}

TEST_CASE("Fallback caching ignores a limb slot that is already cached",
          "[ai][decisions]") {
  FakeFallbackCache cache;
  ai::ActionCandidate candidate = ReadyCandidate();
  candidate.SetSlot(0, 0);
  candidate.SetScore(7);
  CHECK(ai::CacheFallbackActionCandidate(cache, candidate, true) ==
        std::optional<bool>{true});
  candidate.SetScore(99);
  CHECK(ai::CacheFallbackActionCandidate(cache, candidate, true) ==
        std::optional<bool>{false});
  CHECK(cache.stored[0].Score() == 7);
  CHECK(cache.count == 1);
  CHECK(cache.mask == 1);
}

TEST_CASE(
    "Fallback caching refuses slots outside the limb table, bad counts and "
    "missing storage",
    "[ai][decisions]") {
  const int refusal = GENERATE(0, 1, 2, 3, 4);
  FakeFallbackCache cache;
  ai::ActionCandidate candidate = ReadyCandidate();
  candidate.SetSlot(5, 3);
  if (refusal == 0) candidate.SetSlot(6, 0);
  if (refusal == 1) candidate.SetSlot(7, 3);
  if (refusal == 2) cache.count = -1;
  if (refusal == 3) cache.count = 17;
  if (refusal == 4) cache.allocated = false;
  CHECK_FALSE(
      ai::CacheFallbackActionCandidate(cache, candidate, true).has_value());
  CHECK(cache.count == (refusal == 2 ? -1 : (refusal == 3 ? 17 : 0)));
  if (refusal < 4) CHECK(cache.mask == 0);
}

TEST_CASE(
    "Randomized promotion rolls each candidate and promotes the first winner",
    "[ai][decisions]") {
  const std::array<int, 4> scores{100, 90, 80, 70};
  auto ranking = Ranking(scores, 0x80);
  ranking[0].action = 0;
  // The roll keeps seven bits; the weight caps at 0x4C.
  FakeRandom random({0, 0x14C, 0x24B, 0});
  CHECK(ai::PromoteRandomizedActionCandidate(ranking, random) ==
        std::optional<bool>{true});
  CHECK(random.Draws() == 3);
  CHECK(Handles(ranking) == std::vector<std::size_t>{2, 0, 1, 3});
}

TEST_CASE("Randomized promotion draws for every candidate when none wins",
          "[ai][decisions]") {
  const std::array<int, 3> scores{9, 5, 7};
  auto ranking = Ranking(scores, 1);
  FakeRandom random({1, 1, 0x7F});
  CHECK(ai::PromoteRandomizedActionCandidate(ranking, random) ==
        std::optional<bool>{false});
  CHECK(random.Draws() == 3);
  CHECK(Handles(ranking) == std::vector<std::size_t>{0, 1, 2});
  std::vector<ai::RankedCandidate> empty;
  FakeRandom unused({});
  CHECK(ai::PromoteRandomizedActionCandidate(empty, unused) ==
        std::optional<bool>{false});
  CHECK(unused.Draws() == 0);
}

TEST_CASE("Randomized promotion refuses more candidates than the list holds",
          "[ai][decisions]") {
  std::vector<ai::RankedCandidate> ranking(ai::kCandidateCapacity + 1);
  FakeRandom random({0});
  CHECK_FALSE(
      ai::PromoteRandomizedActionCandidate(ranking, random).has_value());
  CHECK(random.Draws() == 0);
}

TEST_CASE(
    "Actor random draws sign-extend the accumulator before adding the step",
    "[ai][decisions]") {
  ai::ActorRandomState state{.accumulator = 0x7FFF, .step = 2};
  CHECK(ai::NextActorRandomMasked(state, 128) == std::optional<int>{1});
  CHECK(state.accumulator == 0x8001);
  CHECK(ai::NextActorRandomMasked(state, 128) == std::optional<int>{3});
  CHECK(state.accumulator == 0xFFFF8003);
  CHECK(ai::NextActorRandomMasked(state, 1) == std::optional<int>{0});
  CHECK(ai::NextActorRandomMasked(state, 3) == std::optional<int>{2});
}

TEST_CASE("Actor random draws refuse empty ranges and an overflowing sum",
          "[ai][decisions]") {
  const int range = GENERATE(0, -1, std::numeric_limits<int>::min());
  ai::ActorRandomState state{.accumulator = 0x1234, .step = 5};
  CHECK_FALSE(ai::NextActorRandomMasked(state, range).has_value());
  CHECK(state.accumulator == 0x1234);
  ai::ActorRandomState overflow{
      .accumulator = 0x7FFF, .step = std::numeric_limits<std::int32_t>::max()};
  CHECK_FALSE(ai::NextActorRandomMasked(overflow, 128).has_value());
  CHECK(overflow.accumulator == 0x7FFF);
}

TEST_CASE("Near target radius begins strictly beyond one hundred",
          "[ai][decisions]") {
  const std::uint32_t distance = GENERATE(0u, 99u, 100u, 101u, 0xFFFFFFFFu);
  CHECK(ai::IsAiTargetBeyondNearRadius(distance) == (distance > 100));
}

TEST_CASE(
    "Engaged target approach follows the timers before reading the roster",
    "[ai][decisions]") {
  const ai::EngagementTimers engaged{.engaged_frames = 1, .cooldown_frames = 1};
  const ai::EngagementTimers cooling{.cooldown_frames = 1};
  CHECK(ai::CanApproachEngagedTarget(4, engaged, nullptr, false) ==
        std::optional<bool>{true});
  CHECK(ai::CanApproachEngagedTarget(4, cooling, nullptr, true) ==
        std::optional<bool>{false});
  CHECK_FALSE(ai::CanApproachEngagedTarget(4, {}, nullptr, true).has_value());
  CHECK_FALSE(
      ai::CanApproachEngagedTarget(-1, engaged, nullptr, true).has_value());
  CHECK_FALSE(
      ai::CanApproachEngagedTarget(16, engaged, nullptr, true).has_value());
}

TEST_CASE(
    "Engaged target approach needs an active target and a free engagement "
    "slot",
    "[ai][decisions]") {
  const int branch = GENERATE(0, 1, 2, 3, 4, 5, 6);
  ai::EngagementRoster roster{.engaged_count = 2, .limit = 1};
  bool active = true;
  if (branch == 1) active = false;
  if (branch == 2) roster.approach_moves = 2;
  if (branch == 3) roster.approach_moves = -1;
  if (branch == 4) roster.engaged_mask = 0x8000;
  if (branch == 5) roster.engaged_count = 1;
  if (branch == 6) roster.approach_moves = 1;
  CHECK(ai::CanApproachEngagedTarget(15, {}, &roster, active) ==
        std::optional<bool>{branch == 4 || branch == 5});
}
