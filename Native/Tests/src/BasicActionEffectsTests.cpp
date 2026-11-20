#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionFlow.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/BasicActionEffects.hpp"
#include "Battle/BasicActionEffectsLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/DamageRecoveryEffectsLive.hpp"
#include "Battle/Hit.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.hpp"
#include "Core/RandomLive.hpp"
#include "State/ScriptFlags.hpp"
#include "State/ScriptFlagsLive.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionState;

namespace {
namespace battle = VS::Battle;
using Handler = VS::Battle::ActionEffectMediator;
using Field = battle::ActionResultField;
constexpr auto kHp = static_cast<std::size_t>(Field::kHp);
constexpr auto kArmor = static_cast<std::size_t>(Field::kArmorDp);
constexpr std::size_t kInstantDeathRule = 30;
constexpr std::size_t kMediatorOffset = 0x7FB78;
constexpr std::uint32_t kSourceMask = 0x0C000000;
constexpr std::uint32_t kSourceSuccess = 0x08000000;
constexpr int kUnusedIndex = -991;

template <typename T>
auto Bytes(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

VS::Battle::HitResult SeedHit() {
  std::array<std::uint32_t, sizeof(VS::Battle::HitResult) / 4> words{};
  words.fill(0xD6A593EF);
  return std::bit_cast<VS::Battle::HitResult>(words);
}

struct LiveFixture {
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  decltype(VS::Battle::statusEffectParams) saved_rules =
      VS::Battle::statusEffectParams;
  decltype(VS::Battle::actionEffectMediators) saved_mediators =
      VS::Battle::actionEffectMediators;
  VS::Battle::ActorStats stats{};
  VS::Battle::Actor actor{};
  VS::Battle::HitResult hit = SeedHit();

  LiveFixture() {
    std::ranges::fill(VS::Battle::actors, nullptr);
    std::ranges::fill(VS::Battle::statusEffectParams,
                      VS::Battle::StatusEffectParams{});
    actor.stats = &stats;
    VS::Battle::actors[41] = &actor;
    hit.target.target_actor = 41;
    hit.target.target_limb = 5;
    stats.actor_id = 255;
  }
  ~LiveFixture() {
    std::ranges::copy(saved_actors, VS::Battle::actors.begin());
    std::ranges::copy(saved_rules, VS::Battle::statusEffectParams.begin());
    std::ranges::copy(saved_mediators,
                      VS::Battle::actionEffectMediators.begin());
  }
};

void CheckSourceSuccess() {
  LiveFixture fixture;
  std::ranges::fill(VS::Battle::actors, nullptr);
  for (unsigned old = 0; old < 4; ++old) {
    auto outcome = static_cast<battle::SourceActionOutcome>(old);
    battle::MarkSourceActionSuccess(outcome);
    CHECK(outcome == battle::SourceActionOutcome::kSuccess);
    fixture.hit.source_result = old;
    auto expected = std::bit_cast<
        std::array<std::uint32_t, sizeof(VS::Battle::HitResult) / 4>>(
        fixture.hit);
    expected.at(7) = (expected.at(7) & ~kSourceMask) | kSourceSuccess;
    battle::MarkLiveSourceActionSuccess(nullptr, &fixture.hit, nullptr,
                                        kUnusedIndex, -1);
    CHECK(Bytes(fixture.hit) == Bytes(expected));
  }
}

TEST_CASE("F00469 marks only the source outcome without looking up actors",
          "[basic-action-effects][F00469]") {
  CheckSourceSuccess();
}
TEST_CASE("F00472 shares the source outcome policy without target dependence",
          "[basic-action-effects][F00472]") {
  CheckSourceSuccess();
}
TEST_CASE("F00475 shares the source outcome policy preserving hit neighbors",
          "[basic-action-effects][F00475]") {
  CheckSourceSuccess();
}

struct DeathQueue final : battle::InstantDeathQueue {
  std::array<battle::StatusRule, battle::kStatusRuleCount> rules{};
  battle::StatusTarget target{};
  battle::QueuedStatuses statuses{};
  battle::ActionResultChange hp{};
  std::uint16_t current_hp = 1;
  std::optional<std::uint16_t> hp_after_removal;
  std::vector<int> calls;

  void RemoveStatuses(battle::StatusRuleId rule) override {
    calls.push_back(static_cast<int>(rule));
    battle::QueueStatusRemovalMask(rules.at(static_cast<std::size_t>(rule)),
                                   target, statuses);
    if (hp_after_removal) {
      current_hp = hp_after_removal.value();
    }
  }
  std::uint16_t CurrentHp() override {
    calls.push_back(-1);
    return current_hp;
  }
  void QueueHp(battle::ActionResultChange change) override {
    calls.push_back(-2);
    hp = change;
  }
};

TEST_CASE("Instant death removes rule thirty and then reads current HP",
          "[basic-action-effects][F00470]") {
  DeathQueue queue;
  queue.rules.at(kInstantDeathRule) = {0x80000005, 0x40, 0xFFFFFFFF};
  queue.target = {0x80000007, 0xFFFFFFFF, false};
  queue.statuses = {0x20, 0x10, battle::StatusFeedback::kNegative};
  queue.hp_after_removal.emplace(0xFEDC);
  battle::QueueInstantDeath(queue);
  CHECK(queue.calls == std::vector<int>{30, -1, -2});
  CHECK(queue.statuses ==
        battle::QueuedStatuses{0x20, 0x80000015,
                               battle::StatusFeedback::kNegative});
  CHECK(queue.hp == battle::ActionResultChange{-292, 1});
  CHECK(queue.current_hp == 0xFEDC);
}

TEST_CASE("Instant death queues HP even when exempt or without active statuses",
          "[basic-action-effects][F00470]") {
  for (const auto exempt : {false, true}) {
    for (const auto hp : {0, 1, 32767, 32768, 65535}) {
      DeathQueue queue;
      queue.target = {.exempt = exempt};
      queue.rules.at(kInstantDeathRule).statuses = 0xFFFFFFFF;
      queue.statuses = {7, 9, battle::StatusFeedback::kPositive};
      queue.current_hp = static_cast<std::uint16_t>(hp);
      battle::QueueInstantDeath(queue);
      CHECK(queue.statuses ==
            battle::QueuedStatuses{7, 9, battle::StatusFeedback::kPositive});
      CHECK(queue.hp == battle::ActionResultChange{
                            std::bit_cast<std::int16_t>(queue.current_hp), 1});
      CHECK(queue.current_hp == hp);
    }
  }
}

TEST_CASE("Live instant death preserves all other hit and stats fields",
          "[basic-action-effects][F00470]") {
  for (const auto identity : {0, 128, 255}) {
    for (const auto hp : {-65537, -1, 0, 32768, 65535, 65536}) {
      LiveFixture fixture;
      fixture.stats.actor_id = static_cast<unsigned char>(identity);
      fixture.stats.current_hp =
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp));
      fixture.stats.statuses = 0x11;
      fixture.stats.status_immunities = -1;
      VS::Battle::statusEffectParams[kInstantDeathRule].status_flags = 0x10;
      VS::Battle::statusEffectParams[kInstantDeathRule].blocking_statuses = -1;
      const auto stats = Bytes(fixture.stats);
      auto expected = fixture.hit;
      expected.target.hp =
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp));
      expected.hp_effect = 1;
      if (identity != 128) {
        expected.inflict_statuses |= 0x10;
      }
      battle::QueueLiveInstantDeath(nullptr, nullptr, &fixture.hit,
                                    kUnusedIndex, 0);
      CHECK(Bytes(fixture.hit) == Bytes(expected));
      CHECK(Bytes(fixture.stats) == stats);
    }
  }
}

TEST_CASE("Armor wear doubles signed HP with halfword wrap and replaces wear",
          "[basic-action-effects][F00471]") {
  for (std::int8_t limb = 0; limb < 6; ++limb) {
    for (const auto hp : {-32768, -16385, -7, 0, 16384, 32767}) {
      battle::ActorActionResult hit{};
      hit.target = 255;
      hit.limb = limb;
      hit.changes.fill({-13, 3});
      hit.limbs.fill({11, 23});
      hit.changes.at(kHp).amount = static_cast<std::int16_t>(hp);
      auto expected = hit;
      const auto doubled =
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp * 2));
      expected.changes.at(kArmor) = {doubled, 1};
      expected.limbs.at(static_cast<std::size_t>(limb)).armor_dp = doubled;
      CHECK(battle::QueueDamageBasedArmorWear(hit));
      CHECK(hit == expected);
      CHECK(battle::QueueDamageBasedArmorWear(hit));
      CHECK(hit == expected);
    }
  }
}

TEST_CASE("Armor wear refuses invalid signed limbs without partial writes",
          "[basic-action-effects][F00471]") {
  for (const auto limb : {-128, -1, 6, 127}) {
    battle::ActorActionResult hit{};
    hit.limb = static_cast<std::int8_t>(limb);
    hit.changes.fill({7, 3});
    hit.limbs.fill({11, 13});
    const auto before = hit;
    CHECK_FALSE(battle::QueueDamageBasedArmorWear(hit));
    CHECK(hit == before);
  }
}

TEST_CASE("Live armor wear changes only selected DP and its two effect bits",
          "[basic-action-effects][F00471]") {
  LiveFixture fixture;
  std::ranges::fill(VS::Battle::actors, nullptr);
  fixture.hit.target.target_actor = 255;
  for (std::int8_t limb = 0; limb < 6; ++limb) {
    for (const auto hp : {-32768, -16385, -7, 0, 16384, 32767}) {
      fixture.hit.target.target_limb = limb;
      fixture.hit.target.hp = static_cast<std::int16_t>(hp);
      auto expected = fixture.hit;
      const auto doubled =
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp * 2));
      expected.armor_dp_change = doubled;
      expected.limbs[limb].effects.opaque2 = doubled;
      expected.armor_dp_effect = 1;
      battle::QueueLiveDamageBasedArmorWear(nullptr, nullptr, &fixture.hit,
                                            kUnusedIndex, -1);
      CHECK(Bytes(fixture.hit) == Bytes(expected));
    }
  }
}

struct FlagRecorder final : battle::ActionScriptFlags,
                            VS::State::ScriptFlagObserver {
  std::array<std::byte, VS::State::kScriptFlagCount> bytes{};
  std::vector<VS::State::ScriptFlagWrite> writes;
  VS::State::ScriptFlags flags{bytes, this};

  void Set(battle::ActionScriptFlag flag, std::uint8_t value) override {
    REQUIRE(flags.Set(static_cast<int>(flag), value));
  }
  void FlagWritten(const VS::State::ScriptFlagWrite& write) override {
    writes.push_back(write);
  }
};

using FlagPolicy = void (*)(bool, battle::ActionScriptFlags&);
void CheckFlagPolicy(FlagPolicy policy, int index) {
  FlagRecorder flags;
  flags.bytes.fill(std::byte{0xA5});
  const auto before = flags.bytes;
  policy(false, flags);
  CHECK(flags.writes.empty());
  CHECK(flags.bytes == before);
  policy(true, flags);
  policy(true, flags);
  CHECK(flags.writes == std::vector<VS::State::ScriptFlagWrite>{
                            {index, 0xA5, 1}, {index, 1, 1}});
  auto expected = before;
  expected.at(static_cast<std::size_t>(index)) = std::byte{1};
  CHECK(flags.bytes == expected);
}

TEST_CASE("Action flag sixty four is an index written only on execution",
          "[basic-action-effects][F00473]") {
  CheckFlagPolicy(battle::SetActionFlag64, 64);
}
TEST_CASE("Action flag thirty two is an index written only on execution",
          "[basic-action-effects][F00474]") {
  CheckFlagPolicy(battle::SetActionFlag32, 32);
}

struct LiveFlags {
  std::array<std::byte, VS::State::kScriptFlagCount> saved{};
  LiveFlags() {
    for (std::size_t index = 0; index < saved.size(); ++index) {
      saved.at(index) = static_cast<std::byte>(
          VS::State::LiveScriptFlag(static_cast<int>(index)));
    }
  }
  ~LiveFlags() {
    for (std::size_t index = 0; index < saved.size(); ++index) {
      VS::State::SetLiveScriptFlag(static_cast<int>(index),
                                   std::to_integer<int>(saved.at(index)));
    }
  }
};

void CheckLiveFlag(Handler handler, int flag) {
  LiveFlags saved;
  for (const auto execute : {0, 1, -1, -2147483647}) {
    VS::State::SetLiveScriptFlag(flag, 0xA5);
    handler(nullptr, nullptr, nullptr, kUnusedIndex, execute);
    for (std::size_t index = 0; index < saved.saved.size(); ++index) {
      const auto expected = static_cast<int>(index) == flag
                                ? (execute != 0 ? 1 : 0xA5)
                                : std::to_integer<int>(saved.saved.at(index));
      CHECK(VS::State::LiveScriptFlag(static_cast<int>(index)) == expected);
    }
  }
}

TEST_CASE(
    "Live flag sixty four ignores unused arguments and accepts negative "
    "execute",
    "[basic-action-effects][F00473]") {
  CheckLiveFlag(battle::SetLiveActionFlag64, 64);
}
TEST_CASE(
    "Live flag thirty two ignores unused arguments and accepts negative "
    "execute",
    "[basic-action-effects][F00474]") {
  CheckLiveFlag(battle::SetLiveActionFlag32, 32);
}

std::vector<std::uint8_t> ReadOriginalModule() {
  auto directory = std::filesystem::current_path();
  const auto manifest = std::filesystem::path("Native") / "data" /
                        VS::Content::DataStore::ManifestName;
  while (!std::filesystem::is_regular_file(directory / manifest)) {
    REQUIRE(directory != directory.parent_path());
    directory = directory.parent_path();
  }
  REQUIRE(VS::Content::DataStore::Open((directory / manifest).parent_path()));
  const auto* store = VS::Content::DataStore::Get();
  REQUIRE(store != nullptr);
  const auto* entry = store->Find("BATTLE/BATTLE.PRG");
  REQUIRE(entry != nullptr);
  auto image = store->ReadAll(*entry);
  REQUIRE(image.has_value());
  return std::move(image).value();
}

Handler Relocated(std::uint32_t address, Handler expected) {
  const auto image = ReadOriginalModule();
  const auto tables = VS::Content::ModulePointerTables();
  const auto table = std::ranges::find(
      tables,
      reinterpret_cast<void**>(VS::Battle::actionEffectMediators.data()),
      &VS::Content::ModulePointerTable::target);
  REQUIRE(table != tables.end());
  REQUIRE(VS::Content::RelocateModulePointerTable(
      *table, image, VS::Content::ModuleSymbols()));
  for (std::size_t index = 0;
       index < std::size(VS::Battle::actionEffectMediators); ++index) {
    std::array<std::uint8_t, 4> bytes{};
    std::copy_n(image.begin() + static_cast<std::ptrdiff_t>(
                                    kMediatorOffset + index * bytes.size()),
                bytes.size(), bytes.begin());
    if (std::bit_cast<std::uint32_t>(bytes) == address) {
      REQUIRE(VS::Battle::actionEffectMediators[index] == expected);
      return VS::Battle::actionEffectMediators[index];
    }
  }
  FAIL("Original mediator address not found");
  return nullptr;
}

void CheckRelocatedSource(std::uint32_t address) {
  LiveFixture fixture;
  auto expected = fixture.hit;
  expected.source_result = 2;
  Relocated(address, battle::MarkLiveSourceActionSuccess)(
      nullptr, &fixture.hit, nullptr, kUnusedIndex, 0);
  CHECK(Bytes(fixture.hit) == Bytes(expected));
}

TEST_CASE(
    "F00469 original void mediator resolves and invokes its native policy",
    "[basic-action-effects][F00469]") {
  CheckRelocatedSource(0x80084228);
}
TEST_CASE("F00472 original void mediator resolves the shared native alias",
          "[basic-action-effects][F00472]") {
  CheckRelocatedSource(0x800842F0);
}
TEST_CASE("F00475 original void mediator resolves the shared native alias",
          "[basic-action-effects][F00475]") {
  CheckRelocatedSource(0x80084370);
}
TEST_CASE("Instant death original void mediator queues rather than kills",
          "[basic-action-effects][F00470]") {
  LiveFixture fixture;
  fixture.stats.current_hp = 997;
  Relocated(0x80084248, battle::QueueLiveInstantDeath)(
      nullptr, nullptr, &fixture.hit, kUnusedIndex, 0);
  CHECK(fixture.hit.target.hp == 997);
  CHECK(fixture.stats.current_hp == 997);
}
TEST_CASE("Armor wear original void mediator invokes the native policy",
          "[basic-action-effects][F00471]") {
  LiveFixture fixture;
  fixture.hit.target.hp = -7;
  Relocated(0x800842AC, battle::QueueLiveDamageBasedArmorWear)(
      nullptr, nullptr, &fixture.hit, kUnusedIndex, 0);
  CHECK(fixture.hit.armor_dp_change == -14);
  CHECK(fixture.hit.limbs[5].effects.opaque2 == -14);
}
TEST_CASE("Flag sixty four original void mediator invokes the native policy",
          "[basic-action-effects][F00473]") {
  LiveFixture fixture;
  CheckLiveFlag(Relocated(0x80084310, battle::SetLiveActionFlag64), 64);
}
TEST_CASE("Flag thirty two original void mediator invokes the native policy",
          "[basic-action-effects][F00474]") {
  LiveFixture fixture;
  CheckLiveFlag(Relocated(0x80084340, battle::SetLiveActionFlag32), 32);
}

struct RandomQueue final : battle::RandomStatQueue {
  battle::ActorActionResult hit{};
  std::optional<battle::ActorActionResult> during_draw;
  int draw{};
  int bound{};
  std::vector<int> events;
  std::vector<Field> fields;

  std::int32_t DrawBounded(std::int32_t maximum) override {
    events.push_back(1);
    bound = maximum;
    if (during_draw) {
      hit = during_draw.value();
    }
    return draw;
  }
  battle::ActionResultChange ReadChange(Field field) override {
    events.push_back(2);
    fields.push_back(field);
    return hit.changes.at(static_cast<std::size_t>(field));
  }
  void WriteChange(Field field, battle::ActionResultChange change) override {
    events.push_back(3);
    fields.push_back(field);
    hit.changes.at(static_cast<std::size_t>(field)) = change;
  }
};

using RandomPolicy = void (*)(battle::RandomStatQueue&);
struct RandomEffect {
  RandomPolicy policy;
  Handler live;
  Field field;
  std::uint8_t mode;
  std::uint32_t address;
  std::size_t offset;
  unsigned shift;
};
constexpr std::array<RandomEffect, 8> kRandomEffects{{
    {battle::QueueRandomStrengthIncrease,
     battle::QueueLiveRandomStrengthIncrease, Field::kStrength, 2, 0x8008490C,
     0x0A, 6},
    {battle::QueueRandomIntelligenceIncrease,
     battle::QueueLiveRandomIntelligenceIncrease, Field::kIntelligence, 2,
     0x80084958, 0x0C, 8},
    {battle::QueueRandomAgilityIncrease, battle::QueueLiveRandomAgilityIncrease,
     Field::kAgility, 2, 0x800849A4, 0x0E, 10},
    {battle::QueueRandomMaxHpIncrease, battle::QueueLiveRandomMaxHpIncrease,
     Field::kMaxHp, 2, 0x800849F0, 0x10, 12},
    {battle::QueueRandomMaxMpIncrease, battle::QueueLiveRandomMaxMpIncrease,
     Field::kMaxMp, 2, 0x80084A3C, 0x12, 14},
    {battle::QueueRandomStrengthDecrease,
     battle::QueueLiveRandomStrengthDecrease, Field::kStrength, 1, 0x80084A8C,
     0x0A, 6},
    {battle::QueueRandomIntelligenceDecrease,
     battle::QueueLiveRandomIntelligenceDecrease, Field::kIntelligence, 1,
     0x80084AD8, 0x0C, 8},
    {battle::QueueRandomAgilityDecrease, battle::QueueLiveRandomAgilityDecrease,
     Field::kAgility, 1, 0x80084B24, 0x0E, 10},
}};
constexpr std::array kPendingAmounts{0, 19, -17, 32767, 32768, 65535};

battle::ActorActionResult SeedQueuedResult() {
  battle::ActorActionResult hit{};
  hit.target = 255;
  hit.limb = -128;
  hit.impact = 0xA5;
  hit.changes.fill({-299, 3});
  hit.limbs.fill({123, -456});
  hit.inflict_statuses = 0xDEADBEEF;
  hit.remove_statuses = 0x87654321;
  return hit;
}

struct RandomSample {
  int draw;
  int pending;
  std::uint8_t mode;
};

void CheckRandomPolicySample(const RandomEffect& effect, bool mutate,
                             RandomSample sample) {
  RandomQueue queue;
  queue.draw = sample.draw;
  queue.hit = SeedQueuedResult();
  auto expected = queue.hit;
  expected.changes.at(static_cast<std::size_t>(effect.field)) = {
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(sample.pending)),
      sample.mode};
  if (mutate) {
    expected.inflict_statuses ^= 0x76543210;
    expected.changes.at(kHp) = {-789, 1};
    queue.during_draw.emplace(expected);
  } else {
    queue.hit = expected;
  }
  expected.changes.at(static_cast<std::size_t>(effect.field)) = {
      std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(sample.pending + sample.draw + 1)),
      effect.mode};
  effect.policy(queue);
  CHECK(queue.hit == expected);
  CHECK(queue.bound == 4);
  CHECK(queue.events == std::vector<int>{1, 2, 3});
  CHECK(queue.fields == std::vector<Field>{effect.field, effect.field});
}

void CheckRandomPolicy(const RandomEffect& effect, bool mutate) {
  for (int draw = 0; draw < 4; ++draw) {
    for (const auto pending : kPendingAmounts) {
      for (std::uint8_t mode = 0; mode < 4; ++mode) {
        CheckRandomPolicySample(effect, mutate, {draw, pending, mode});
      }
    }
  }
}

struct RandomStateFixture {
  decltype(std::to_array(VS::Core::randomShuffleTable)) table =
      std::to_array(VS::Core::randomShuffleTable);
  int index = VS::Core::randomShuffleIndex;
  std::uint32_t bios = VS::Core::BiosRand().State();
  ~RandomStateFixture() {
    std::ranges::copy(table, VS::Core::randomShuffleTable);
    VS::Core::randomShuffleIndex = index;
    VS::Core::BiosRand().Seed(bios);
  }
};

void SetPending(VS::Battle::HitResult& hit, const RandomEffect& effect,
                int amount, unsigned mode) {
  auto halves = std::bit_cast<
      std::array<std::uint16_t, sizeof(VS::Battle::HitResult) / 2>>(hit);
  halves.at(effect.offset / 2) = static_cast<std::uint16_t>(amount);
  auto words = std::bit_cast<
      std::array<std::uint32_t, sizeof(VS::Battle::HitResult) / 4>>(halves);
  words.at(7) = (words.at(7) & ~(3U << effect.shift)) | (mode << effect.shift);
  hit = std::bit_cast<VS::Battle::HitResult>(words);
}

void CheckLiveRandomSample(Handler handler, const RandomEffect& effect,
                           RandomSample sample) {
  constexpr std::array kUnusedValues{0, 1, -1, std::numeric_limits<int>::min(),
                                     std::numeric_limits<int>::max()};
  for (const auto unused : kUnusedValues) {
    auto hit = SeedHit();
    SetPending(hit, effect, sample.pending, sample.mode);
    auto expected = hit;
    SetPending(expected, effect, sample.pending + sample.draw + 1, effect.mode);
    std::ranges::fill(VS::Core::randomShuffleTable, 0);
    VS::Core::randomShuffleIndex = 96;
    VS::Core::randomShuffleTable[0] = 32767 - sample.draw * 8192;
    VS::Core::BiosRand().Seed(12345);
    VS::Core::Rand15 expected_bios(12345);
    auto expected_table = std::to_array(VS::Core::randomShuffleTable);
    expected_table.fill(0);
    expected_table.at(0) = expected_bios.Next();
    handler(nullptr, nullptr, &hit, unused, unused);
    CHECK(Bytes(hit) == Bytes(expected));
    CHECK(VS::Core::BiosRand().State() == expected_bios.State());
    CHECK(VS::Core::randomShuffleIndex == 0);
    CHECK(std::to_array(VS::Core::randomShuffleTable) == expected_table);
  }
}

void CheckLiveRandom(const RandomEffect& effect) {
  LiveFixture fixture;
  RandomStateFixture saved;
  const auto handler = Relocated(effect.address, effect.live);
  std::ranges::fill(VS::Battle::actors, nullptr);
  for (int draw = 0; draw < 4; ++draw) {
    for (const auto pending : kPendingAmounts) {
      for (std::uint8_t mode = 0; mode < 4; ++mode) {
        CheckLiveRandomSample(handler, effect, {draw, pending, mode});
      }
    }
  }
}

TEST_CASE("F00486 queues all random strength increases with halfword wrap",
          "[random-stat-effects][F00486]") {
  CheckRandomPolicy(kRandomEffects.at(0), false);
}
TEST_CASE("F00487 queues all random intelligence increases with halfword wrap",
          "[random-stat-effects][F00487]") {
  CheckRandomPolicy(kRandomEffects.at(1), false);
}
TEST_CASE("F00488 queues all random agility increases with halfword wrap",
          "[random-stat-effects][F00488]") {
  CheckRandomPolicy(kRandomEffects.at(2), false);
}
TEST_CASE("F00489 queues all random maximum HP increases with halfword wrap",
          "[random-stat-effects][F00489]") {
  CheckRandomPolicy(kRandomEffects.at(3), false);
}
TEST_CASE("F00490 queues all random maximum MP increases with halfword wrap",
          "[random-stat-effects][F00490]") {
  CheckRandomPolicy(kRandomEffects.at(4), false);
}
TEST_CASE("F00491 adds positive strength amounts with decrease mode",
          "[random-stat-effects][F00491]") {
  CheckRandomPolicy(kRandomEffects.at(5), false);
}
TEST_CASE("F00492 adds positive intelligence amounts with decrease mode",
          "[random-stat-effects][F00492]") {
  CheckRandomPolicy(kRandomEffects.at(6), false);
}
TEST_CASE("F00493 adds positive agility amounts with decrease mode",
          "[random-stat-effects][F00493]") {
  CheckRandomPolicy(kRandomEffects.at(7), false);
}
TEST_CASE("F00486 rereads the pending result after its random callback",
          "[random-stat-effects][F00486]") {
  CheckRandomPolicy(kRandomEffects.at(0), true);
}
TEST_CASE("F00487 rereads the pending result after its random callback",
          "[random-stat-effects][F00487]") {
  CheckRandomPolicy(kRandomEffects.at(1), true);
}
TEST_CASE("F00488 rereads the pending result after its random callback",
          "[random-stat-effects][F00488]") {
  CheckRandomPolicy(kRandomEffects.at(2), true);
}
TEST_CASE("F00489 rereads the pending result after its random callback",
          "[random-stat-effects][F00489]") {
  CheckRandomPolicy(kRandomEffects.at(3), true);
}
TEST_CASE("F00490 rereads the pending result after its random callback",
          "[random-stat-effects][F00490]") {
  CheckRandomPolicy(kRandomEffects.at(4), true);
}
TEST_CASE("F00491 rereads the pending result after its random callback",
          "[random-stat-effects][F00491]") {
  CheckRandomPolicy(kRandomEffects.at(5), true);
}
TEST_CASE("F00492 rereads the pending result after its random callback",
          "[random-stat-effects][F00492]") {
  CheckRandomPolicy(kRandomEffects.at(6), true);
}
TEST_CASE("F00493 rereads the pending result after its random callback",
          "[random-stat-effects][F00493]") {
  CheckRandomPolicy(kRandomEffects.at(7), true);
}
TEST_CASE("F00486 original void mediator draws once even in preview",
          "[random-stat-effects][F00486]") {
  CheckLiveRandom(kRandomEffects.at(0));
}
TEST_CASE("F00487 original void mediator draws once even in preview",
          "[random-stat-effects][F00487]") {
  CheckLiveRandom(kRandomEffects.at(1));
}
TEST_CASE("F00488 original void mediator draws once even in preview",
          "[random-stat-effects][F00488]") {
  CheckLiveRandom(kRandomEffects.at(2));
}
TEST_CASE("F00489 original void mediator draws once even in preview",
          "[random-stat-effects][F00489]") {
  CheckLiveRandom(kRandomEffects.at(3));
}
TEST_CASE("F00490 original void mediator draws once even in preview",
          "[random-stat-effects][F00490]") {
  CheckLiveRandom(kRandomEffects.at(4));
}
TEST_CASE("F00491 original void mediator draws once even in preview",
          "[random-stat-effects][F00491]") {
  CheckLiveRandom(kRandomEffects.at(5));
}
TEST_CASE("F00492 original void mediator draws once even in preview",
          "[random-stat-effects][F00492]") {
  CheckLiveRandom(kRandomEffects.at(6));
}
TEST_CASE("F00493 original void mediator draws once even in preview",
          "[random-stat-effects][F00493]") {
  CheckLiveRandom(kRandomEffects.at(7));
}

using RecoveryPolicy = void (*)(battle::TriggeredRecoveryQueue&);
struct RecoveryEffect {
  RecoveryPolicy policy;
  Handler live;
  std::uint32_t address;
};
constexpr std::array<RecoveryEffect, 9> kRecoveryEffects{{
    {battle::QueueSpellTriggeredRecovery,
     battle::QueueLiveSpellTriggeredRecovery, 0x80084390},
    {battle::QueueNonSpellTriggeredRecovery,
     battle::QueueLiveNonSpellTriggeredRecovery, 0x80084440},
    {battle::QueuePhysicalAffinityRecovery,
     battle::QueueLivePhysicalAffinityRecovery, 0x800844F0},
    {battle::QueueAirAffinityRecovery, battle::QueueLiveAirAffinityRecovery,
     0x80084570},
    {battle::QueueFireAffinityRecovery, battle::QueueLiveFireAffinityRecovery,
     0x800845F0},
    {battle::QueueEarthAffinityRecovery, battle::QueueLiveEarthAffinityRecovery,
     0x80084670},
    {battle::QueueWaterAffinityRecovery, battle::QueueLiveWaterAffinityRecovery,
     0x800846F0},
    {battle::QueueLightAffinityRecovery, battle::QueueLiveLightAffinityRecovery,
     0x80084770},
    {battle::QueueDarkAffinityRecovery, battle::QueueLiveDarkAffinityRecovery,
     0x800847F0},
}};

struct RecoveryQueue final : battle::TriggeredRecoveryQueue {
  battle::ActorActionResult target = SeedQueuedResult();
  std::optional<battle::ActorActionResult> during;
  battle::ActorActionResult observed{};
  std::uint8_t type{}, affinity{};
  std::int16_t amount{}, recovered{};
  std::vector<int> events;
  std::uint8_t SelectedActionType() override {
    events.push_back(1);
    return type;
  }
  std::uint8_t SelectedAffinity() override {
    events.push_back(2);
    return affinity;
  }
  std::int16_t Calculate() override {
    events.push_back(3);
    type ^= 1;
    affinity ^= 1;
    if (during) target = during.value();
    return amount;
  }
  battle::ActionResultChange ReadHp() override {
    events.push_back(4);
    return target.changes.at(kHp);
  }
  void WriteHp(battle::ActionResultChange change) override {
    events.push_back(5);
    target.changes.at(kHp) = change;
  }
  void RecoverLimbs(std::int16_t value) override {
    events.push_back(6);
    recovered = value;
    observed = target;
  }
};

bool RecoveryMatches(std::size_t function, unsigned type, unsigned affinity) {
  if (function < 2) return (type == 1) == (function == 0);
  return affinity == function - 2;
}

void CheckRecoveryGates(std::size_t function) {
  for (unsigned type = 0; type < 8; ++type) {
    for (unsigned affinity = 0; affinity < 256; ++affinity) {
      RecoveryQueue queue;
      queue.type = static_cast<std::uint8_t>(type);
      queue.affinity = static_cast<std::uint8_t>(affinity);
      queue.amount = -7;
      const auto before = queue.target;
      const bool matches = RecoveryMatches(function, type, affinity);
      auto expected = before;
      if (matches) expected.changes.at(kHp) = {-306, 2};
      const int gate = function < 2 ? 1 : 2;
      kRecoveryEffects.at(function).policy(queue);
      CHECK(queue.target == expected);
      CHECK(queue.events == (matches ? std::vector<int>{gate, 3, 4, 5, 6}
                                     : std::vector<int>{gate}));
    }
  }
}

std::int16_t RecoveryHalf(int value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

void CheckRecoveryCallbackSample(std::size_t function, int amount, int pending,
                                 std::uint8_t mode) {
  RecoveryQueue queue;
  queue.type = function == 0 ? 1 : 7;
  queue.affinity = static_cast<std::uint8_t>(function - 2);
  queue.amount = RecoveryHalf(amount);
  auto changed = SeedQueuedResult();
  changed.changes.at(kHp) = {RecoveryHalf(pending), mode};
  changed.inflict_statuses ^= 0xFEDCBA98;
  changed.limbs.at(3).armor_dp = 997;
  queue.during.emplace(changed);
  auto expected = changed;
  expected.changes.at(kHp) = {RecoveryHalf(pending + amount), 2};
  kRecoveryEffects.at(function).policy(queue);
  CHECK(queue.target == expected);
  CHECK(queue.observed == expected);
  CHECK(queue.recovered == amount);
  CHECK(queue.events == std::vector<int>{function < 2 ? 1 : 2, 3, 4, 5, 6});
}

void CheckRecoveryCallback(std::size_t function) {
  constexpr std::array kAmounts{-32768, -19, -1, 0, 1, 32767};
  for (const int amount : kAmounts) {
    for (const int pending : kPendingAmounts) {
      for (std::uint8_t mode = 0; mode < 4; ++mode) {
        CheckRecoveryCallbackSample(function, amount, pending, mode);
      }
    }
  }
}

struct RecoveryFixture;
RecoveryFixture* activeRecoveryFixture{};
struct RecoveryFixture : LiveFixture {
  ActionState* saved_state = VS::Battle::actionState;
  ActionState state{};
  decltype(VS::Battle::actionDefinitions) definitions =
      VS::Battle::actionDefinitions;
  decltype(VS::Battle::statCalculators) calculators =
      VS::Battle::statCalculators;
  decltype(VS::Battle::limbArmorDpLoss) armor = VS::Battle::limbArmorDpLoss;
  std::uint32_t last = VS::Battle::lastActionStatValue;
  ActionDefinition input{};
  VS::Battle::ActorStats replacement_stats{};
  VS::Battle::Actor replacement{};
  VS::Battle::HitResult source = SeedHit();
  VS::Battle::HitResult changed = SeedHit();
  VS::Battle::HitResult* expected_source{};
  std::int16_t amount{};
  int index{}, execute{}, calls{};
  bool arguments_match{};

  RecoveryFixture() {
    VS::Battle::actionState = &state;
    activeRecoveryFixture = this;
    replacement.stats = &replacement_stats;
    replacement_stats.limbs[5].max_hp = 100;
    replacement_stats.current_hp = 321;
    stats.limbs[0].max_hp = 100;
    stats.current_hp = 123;
    changed.target.target_actor = 41;
    input.type = 6;
    input.hits[0].stat_calculator_id = 1;
    input.hits[1].stat_calculator_id = 1;
  }
  ~RecoveryFixture() {
    activeRecoveryFixture = nullptr;
    VS::Battle::actionState = saved_state;
    VS::Battle::lastActionStatValue = last;
    std::ranges::copy(definitions, VS::Battle::actionDefinitions.data());
    std::ranges::copy(calculators, VS::Battle::statCalculators.begin());
    std::ranges::copy(armor, VS::Battle::limbArmorDpLoss.begin());
  }
  void Select(std::size_t function) {
    state.selection.action_id = function < 2 ? 255 : 65535;
    VS::Battle::actionDefinitions[255].type = function == 0 ? 1 : 7;
    state.selection.effect_variant = static_cast<char>(function - 2);
  }
  void Prepare(std::size_t function, int value, int pending) {
    Select(function);
    amount = RecoveryHalf(value);
    changed.target.hp = RecoveryHalf(pending);
    changed.hp_effect = static_cast<unsigned>(pending) & 3U;
    hit = SeedHit();
    hit.target.target_actor = 41;
    hit.targets_position = pending & 1;
    expected_source = index == 0 ? &source : &hit;
    calls = 0;
    VS::Battle::actors[41] = &actor;
    std::ranges::fill(VS::Battle::limbArmorDpLoss, 71);
  }
};

std::int16_t RecoveryCalculator(ActionDefinition* action,
                                VS::Battle::HitResult* source,
                                VS::Battle::HitResult* target, int index,
                                int execute) {
  auto& fixture = *activeRecoveryFixture;
  ++fixture.calls;
  fixture.arguments_match = action == &fixture.input &&
                            source == fixture.expected_source &&
                            target == &fixture.hit && index == fixture.index &&
                            execute == fixture.execute;
  *target = fixture.changed;
  VS::Battle::actors[41] = &fixture.replacement;
  fixture.state.selection.action_id = 65535;
  fixture.state.selection.effect_variant = 127;
  return fixture.amount;
}

void CheckLiveRecoverySample(RecoveryFixture& fixture, Handler handler,
                             std::size_t function, int amount, int pending) {
  fixture.Prepare(function, amount, pending);
  auto expected = fixture.changed;
  expected.target.hp = RecoveryHalf(pending + amount);
  expected.hp_effect = 2;
  expected.limbs[5].effects.opaque0 =
      RecoveryHalf(expected.limbs[5].effects.opaque0 + amount * 2);
  auto expected_armor = VS::Battle::limbArmorDpLoss;
  expected_armor.at(5) = RecoveryHalf(amount);
  const auto source = Bytes(fixture.source);
  handler(&fixture.input, fixture.expected_source, &fixture.hit, fixture.index,
          fixture.execute);
  CHECK(fixture.arguments_match);
  CHECK(fixture.calls == 1);
  CHECK(Bytes(fixture.hit) == Bytes(expected));
  CHECK(Bytes(fixture.source) == source);
  CHECK(VS::Battle::limbArmorDpLoss == expected_armor);
  CHECK(VS::Battle::lastActionStatValue ==
        static_cast<std::uint32_t>(static_cast<std::int32_t>(fixture.amount)));
  CHECK(fixture.stats.current_hp == 123);
  CHECK(fixture.replacement_stats.current_hp == 321);
}

void CheckLiveRecovery(std::size_t function) {
  RecoveryFixture fixture;
  VS::Battle::statCalculators[1] = RecoveryCalculator;
  const auto& effect = kRecoveryEffects.at(function);
  const auto handler = Relocated(effect.address, effect.live);
  for (const auto execute : {0, 1, -1}) {
    fixture.execute = execute;
    for (const auto index : {0, 1}) {
      fixture.index = index;
      for (const auto amount : {-32768, -19, 0, 1, 32767}) {
        for (const auto pending : kPendingAmounts) {
          CheckLiveRecoverySample(fixture, handler, function, amount, pending);
        }
      }
    }
  }
}

void CheckLiveRecoveryMiss(std::size_t function) {
  RecoveryFixture fixture;
  VS::Battle::statCalculators[1] = RecoveryCalculator;
  std::ranges::fill(VS::Battle::actors, nullptr);
  for (unsigned selected = 0; selected < 256; ++selected) {
    fixture.Select(function);
    const auto type = selected % 8;
    VS::Battle::actionDefinitions[255].type = type;
    fixture.state.selection.effect_variant = static_cast<char>(selected);
    if (RecoveryMatches(function, type, selected)) continue;
    kRecoveryEffects.at(function).live(nullptr, nullptr, nullptr,
                                       std::numeric_limits<int>::min(), -1);
    CHECK(fixture.calls == 0);
    CHECK(VS::Battle::lastActionStatValue == fixture.last);
  }
}

TEST_CASE("F00476 selects spells from the selected action not the input",
          "[triggered-recovery][F00476]") {
  CheckRecoveryGates(0);
  CheckLiveRecoveryMiss(0);
}
TEST_CASE("F00477 selects every nonspell type",
          "[triggered-recovery][F00477]") {
  CheckRecoveryGates(1);
  CheckLiveRecoveryMiss(1);
}
TEST_CASE("F00478 selects physical source affinity without definition lookup",
          "[triggered-recovery][F00478]") {
  CheckRecoveryGates(2);
  CheckLiveRecoveryMiss(2);
}
TEST_CASE("F00479 selects air source affinity without definition lookup",
          "[triggered-recovery][F00479]") {
  CheckRecoveryGates(3);
  CheckLiveRecoveryMiss(3);
}
TEST_CASE("F00480 selects fire source affinity without definition lookup",
          "[triggered-recovery][F00480]") {
  CheckRecoveryGates(4);
  CheckLiveRecoveryMiss(4);
}
TEST_CASE("F00481 selects earth source affinity without definition lookup",
          "[triggered-recovery][F00481]") {
  CheckRecoveryGates(5);
  CheckLiveRecoveryMiss(5);
}
TEST_CASE("F00482 selects water source affinity without definition lookup",
          "[triggered-recovery][F00482]") {
  CheckRecoveryGates(6);
  CheckLiveRecoveryMiss(6);
}
TEST_CASE("F00483 selects light source affinity without definition lookup",
          "[triggered-recovery][F00483]") {
  CheckRecoveryGates(7);
  CheckLiveRecoveryMiss(7);
}
TEST_CASE("F00476 rereads pending HP and recovers even a zero amount",
          "[triggered-recovery][F00476]") {
  CheckRecoveryCallback(0);
}
TEST_CASE("F00477 rereads pending HP and recovers even a zero amount",
          "[triggered-recovery][F00477]") {
  CheckRecoveryCallback(1);
}
TEST_CASE("F00478 rereads pending HP and recovers even a zero amount",
          "[triggered-recovery][F00478]") {
  CheckRecoveryCallback(2);
}
TEST_CASE("F00479 rereads pending HP and recovers even a zero amount",
          "[triggered-recovery][F00479]") {
  CheckRecoveryCallback(3);
}
TEST_CASE("F00480 rereads pending HP and recovers even a zero amount",
          "[triggered-recovery][F00480]") {
  CheckRecoveryCallback(4);
}
TEST_CASE("F00481 rereads pending HP and recovers even a zero amount",
          "[triggered-recovery][F00481]") {
  CheckRecoveryCallback(5);
}
TEST_CASE("F00482 rereads pending HP and recovers even a zero amount",
          "[triggered-recovery][F00482]") {
  CheckRecoveryCallback(6);
}
TEST_CASE("F00483 rereads pending HP and recovers even a zero amount",
          "[triggered-recovery][F00483]") {
  CheckRecoveryCallback(7);
}
TEST_CASE(
    "F00476 original VOID5 mediator integrates stat and recovery policies",
    "[triggered-recovery][F00476]") {
  CheckLiveRecovery(0);
}
TEST_CASE(
    "F00477 original VOID5 mediator integrates stat and recovery policies",
    "[triggered-recovery][F00477]") {
  CheckLiveRecovery(1);
}
TEST_CASE(
    "F00478 original VOID5 mediator integrates stat and recovery policies",
    "[triggered-recovery][F00478]") {
  CheckLiveRecovery(2);
}
TEST_CASE(
    "F00479 original VOID5 mediator integrates stat and recovery policies",
    "[triggered-recovery][F00479]") {
  CheckLiveRecovery(3);
}
TEST_CASE(
    "F00480 original VOID5 mediator integrates stat and recovery policies",
    "[triggered-recovery][F00480]") {
  CheckLiveRecovery(4);
}
TEST_CASE(
    "F00481 original VOID5 mediator integrates stat and recovery policies",
    "[triggered-recovery][F00481]") {
  CheckLiveRecovery(5);
}
TEST_CASE(
    "F00482 original VOID5 mediator integrates stat and recovery policies",
    "[triggered-recovery][F00482]") {
  CheckLiveRecovery(6);
}
TEST_CASE(
    "F00483 original VOID5 mediator integrates stat and recovery policies",
    "[triggered-recovery][F00483]") {
  CheckLiveRecovery(7);
}
TEST_CASE("F00484 dark recovery preserves unused-input gates",
          "[damage-recovery][F00484]") {
  CheckRecoveryGates(8);
  CheckLiveRecoveryMiss(8);
}
TEST_CASE("F00484 dark recovery reads current pending HP and always recovers",
          "[damage-recovery][F00484]") {
  CheckRecoveryCallback(8);
  CheckLiveRecovery(8);
}
TEST_CASE("F00485 primary damage mode gates independently of input identity",
          "[damage-recovery][F00485]") {
  RecoveryFixture fixture;
  for (std::uint8_t mode = 0; mode < 4; ++mode) {
    RecoveryQueue queue;
    queue.amount = -7;
    const auto before = queue.target;
    battle::QueueDamageTriggeredRecovery(mode, queue);
    if (mode == 1 || mode == 3) {
      CHECK(queue.target.changes.at(kHp) ==
            battle::ActionResultChange{-306, 2});
      CHECK(queue.events == std::vector<int>{3, 4, 5, 6});
    } else {
      CHECK(queue.target == before);
      fixture.state.selection.primary_target.hp_effect = mode;
      battle::QueueLiveDamageTriggeredRecovery(nullptr, nullptr, nullptr, -99,
                                               -1);
      CHECK(fixture.calls == 0);
    }
  }
}
TEST_CASE("F00485 original mediator recovers through real dispatch and limbs",
          "[damage-recovery][F00485]") {
  RecoveryFixture fixture;
  VS::Battle::statCalculators[1] = RecoveryCalculator;
  const auto handler =
      Relocated(0x80084870, battle::QueueLiveDamageTriggeredRecovery);
  for (const auto mode : {1U, 3U}) {
    for (const auto amount : {-32768, 0, 32767}) {
      fixture.state.selection.primary_target.hp_effect = mode;
      fixture.index = 1;
      fixture.execute = -1;
      CheckLiveRecoverySample(fixture, handler, 8, amount, 32767);
    }
  }
}

struct DamageFixture;
DamageFixture* activeDamageFixture{};
struct DamageFixture : RecoveryFixture {
  int critical_before = VS::Battle::criticalHitOccurred;
  int critical_seen{};
  bool mutate_owner = true;
  DamageFixture() {
    activeDamageFixture = this;
    source.target.target_actor = 41;
    hit.target.target_actor = 41;
    hit.targets_position = 0;
    changed.target.target_actor = 40;
    changed.targets_position = 0;
    changed.target.target_limb = 5;
    changed.inflict_statuses = 0x10;
    VS::Battle::actors[40] = &replacement;
    stats.actor_id = 41;
    replacement_stats.actor_id = 40;
    input.category = 1;
    stats.current_mp = 71;
    stats.weapon.current_dp = 100;
    stats.weapon.current_pp = 10;
    stats.weapon.max_pp = 500;
    stats.shield.current_dp = 100;
    stats.shield.current_pp = 10;
    stats.shield.max_pp = 500;
    actor.weapon_drawn = 1;
    amount = 30;
  }
  ~DamageFixture() {
    VS::Battle::criticalHitOccurred = critical_before;
    activeDamageFixture = nullptr;
  }
};
std::int16_t DamageCalculator(ActionDefinition* action,
                              VS::Battle::HitResult* source,
                              VS::Battle::HitResult* target, int hit,
                              int execute) {
  auto& fixture = *activeDamageFixture;
  ++fixture.calls;
  fixture.arguments_match =
      action == &fixture.input && source == &fixture.source &&
      target == &fixture.hit && hit == 1 && execute == fixture.execute;
  fixture.critical_seen = VS::Battle::criticalHitOccurred;
  VS::Battle::criticalHitOccurred = -7;
  *target = fixture.changed;
  if (fixture.mutate_owner) fixture.actor.stats = &fixture.replacement_stats;
  fixture.stats.current_mp = -19;
  fixture.stats.statuses = 0x8000;
  return fixture.amount;
}
TEST_CASE("F00494 caches stats owners but distributes to the current hit actor",
          "[damage-recovery][F00494]") {
  DamageFixture fixture;
  VS::Battle::statCalculators[1] = DamageCalculator;
  VS::Battle::criticalHitOccurred = 123;
  fixture.execute = -1;
  fixture.changed.target.hp = 32767;
  Relocated(0x80084B70, battle::QueueLiveHitDamageAndEquipmentWear)(
      &fixture.input, &fixture.source, &fixture.hit, 1, fixture.execute);
  CHECK(fixture.arguments_match);
  CHECK(fixture.critical_seen == 0);
  CHECK(fixture.hit.target.hp == RecoveryHalf(32767 + 30));
  CHECK(fixture.hit.hp_effect == 3);
  CHECK(fixture.hit.inflict_statuses == 0x8010);
  CHECK(fixture.stats.weapon.current_dp == 70);
  CHECK(fixture.stats.weapon.current_pp == 13);
  CHECK(fixture.stats.shield.current_dp == 70);
  CHECK(fixture.stats.shield.current_pp == 13);
}
TEST_CASE("F00494 preview removes damage status without consuming equipment",
          "[damage-recovery][F00494]") {
  DamageFixture fixture;
  VS::Battle::statCalculators[1] = DamageCalculator;
  fixture.execute = 0;
  fixture.actor.stats = nullptr;
  fixture.source.target.target_actor = 41;
  fixture.hit.target.target_actor = 40;
  fixture.replacement_stats.statuses = 0x8000;
  battle::QueueLiveHitDamageAndEquipmentWear(&fixture.input, &fixture.source,
                                             &fixture.hit, 1, 0);
  CHECK(fixture.hit.inflict_statuses == 0x8010);
  CHECK(fixture.stats.weapon.current_dp == 100);
  CHECK(fixture.stats.current_hp == 123);
}
TEST_CASE("F00494 position damage permits unused null stats owners",
          "[damage-recovery][F00494]") {
  DamageFixture fixture;
  VS::Battle::statCalculators[1] = DamageCalculator;
  fixture.actor.stats = nullptr;
  fixture.hit.targets_position = 1;
  fixture.changed.targets_position = -1;
  battle::QueueLiveHitDamageAndEquipmentWear(&fixture.input, &fixture.source,
                                             &fixture.hit, 1, 0);
  CHECK(fixture.hit.hp_effect == 3);
  CHECK(fixture.hit.inflict_statuses == 0x10);
  CHECK(fixture.calls == 1);
}
TEST_CASE("F00495 caps against current MP of the cached owner without flooring",
          "[damage-recovery][F00495]") {
  DamageFixture fixture;
  VS::Battle::statCalculators[1] = DamageCalculator;
  fixture.changed.target.mp = -32768;
  Relocated(0x80084CAC, battle::QueueLiveMpDrain)(
      &fixture.input, &fixture.source, &fixture.hit, 1, 0);
  CHECK(fixture.hit.target.mp == 32749);
  CHECK(fixture.hit.mp_effect == 1);
  CHECK(VS::Battle::lastActionStatValue == static_cast<std::uint32_t>(-19));
  CHECK(fixture.stats.current_mp == -19);
  CHECK(fixture.replacement_stats.current_mp == 0);
}
TEST_CASE("F00495 uncapped negative amount leaves the calculator last value",
          "[damage-recovery][F00495]") {
  DamageFixture fixture;
  VS::Battle::statCalculators[1] = DamageCalculator;
  fixture.amount = -30;
  fixture.changed.target.mp = 7;
  battle::QueueLiveMpDrain(&fixture.input, &fixture.source, &fixture.hit, 1, 0);
  CHECK(fixture.hit.target.mp == -23);
  CHECK(VS::Battle::lastActionStatValue == static_cast<std::uint32_t>(-30));
}
TEST_CASE(
    "F00496 queues unsigned current weapon PP and preserves no-blade state",
    "[damage-recovery][F00496]") {
  DamageFixture fixture;
  auto handler = Relocated(0x80084D44, battle::QueueLiveWeaponPpDrain);
  const auto before = Bytes(fixture.hit);
  handler(nullptr, nullptr, &fixture.hit, -99, 0);
  CHECK(Bytes(fixture.hit) == before);
  fixture.stats.weapon.blade.id = 65535;
  fixture.stats.weapon.current_pp = 65535;
  auto expected = fixture.hit;
  expected.weapon_pp_change = RecoveryHalf(expected.weapon_pp_change - 1);
  expected.weapon_pp_effect = 1;
  handler(nullptr, nullptr, &fixture.hit, -99, -1);
  CHECK(Bytes(fixture.hit) == Bytes(expected));
}
TEST_CASE(
    "F00497 shield gate permits unused null stats and preserves no shield",
    "[damage-recovery][F00497]") {
  DamageFixture fixture;
  auto handler = Relocated(0x80084DA8, battle::QueueLiveShieldPpDrain);
  const auto before = Bytes(fixture.hit);
  fixture.actor.stats = nullptr;
  for (auto mode : {0U, 2U}) {
    fixture.state.selection.primary_target.hp_effect = mode;
    handler(nullptr, nullptr, &fixture.hit, -99, 0);
    CHECK(Bytes(fixture.hit) == before);
  }
  fixture.actor.stats = &fixture.stats;
  fixture.state.selection.primary_target.hp_effect = 3;
  handler(nullptr, nullptr, &fixture.hit, -99, -1);
  CHECK(Bytes(fixture.hit) == before);
  fixture.stats.shield.base.id = 1;
  fixture.stats.shield.current_pp = 65535;
  auto expected = fixture.hit;
  expected.shield_pp_change = RecoveryHalf(expected.shield_pp_change - 1);
  expected.shield_pp_effect = 1;
  handler(nullptr, nullptr, &fixture.hit, -99, 0);
  CHECK(Bytes(fixture.hit) == Bytes(expected));
}
TEST_CASE("F00498 post-calculation position controls real limb recovery",
          "[damage-recovery][F00498]") {
  RecoveryFixture fixture;
  VS::Battle::statCalculators[1] = RecoveryCalculator;
  const auto handler = Relocated(0x80084E28, battle::QueueLiveHpRecovery);
  fixture.changed.targets_position = 0;
  CheckLiveRecoverySample(fixture, handler, 8, 30, 32767);
  fixture.Prepare(8, 0, -1);
  fixture.changed.targets_position = -1;
  const auto limbs = fixture.changed.limbs[5].effects.opaque0;
  handler(&fixture.input, &fixture.source, &fixture.hit, 0, 0);
  CHECK(fixture.hit.limbs[5].effects.opaque0 == limbs);
  CHECK(fixture.hit.hp_effect == 2);
}
TEST_CASE("F00499 MP recovery wraps current queue and preserves neighbors",
          "[damage-recovery][F00499]") {
  DamageFixture fixture;
  VS::Battle::statCalculators[1] = DamageCalculator;
  fixture.changed.target.mp = 32767;
  auto expected = fixture.changed;
  expected.target.mp = RecoveryHalf(32767 + 30);
  expected.mp_effect = 2;
  Relocated(0x80084EA0, battle::QueueLiveMpRecovery)(
      &fixture.input, &fixture.source, &fixture.hit, 1, 0);
  CHECK(Bytes(fixture.hit) == Bytes(expected));
  CHECK(fixture.stats.current_mp == -19);
}

void CheckResourceRecoverySample(RecoveryFixture& fixture, Handler handler,
                                 unsigned resource, int amount, int pending) {
  constexpr std::size_t kUnusedSelection = 8;
  fixture.Prepare(kUnusedSelection, amount, pending);
  fixture.changed.weapon_pp_change = RecoveryHalf(pending);
  fixture.changed.weapon_dp_change = RecoveryHalf(pending);
  fixture.changed.risk_change = RecoveryHalf(pending);
  auto expected = fixture.changed;
  const auto value = RecoveryHalf(pending + amount);
  if (resource == 0) {
    expected.weapon_pp_change = value;
    expected.weapon_pp_effect = 2;
  } else if (resource == 1) {
    expected.weapon_dp_change = value;
    expected.weapon_dp_effect = 2;
  } else if (amount != 0) {
    expected.risk_change = value;
    expected.risk_effect = 2;
  }
  handler(&fixture.input, fixture.expected_source, &fixture.hit, 1,
          fixture.execute);
  CHECK(fixture.arguments_match);
  CHECK(fixture.calls == 1);
  CHECK(Bytes(fixture.hit) == Bytes(expected));
}

void CheckResourceRecovery(unsigned resource) {
  RecoveryFixture fixture;
  VS::Battle::statCalculators[1] = RecoveryCalculator;
  constexpr std::array<Handler, 3> kHandlers{battle::QueueLiveWeaponPpRecovery,
                                             battle::QueueLiveWeaponDpRecovery,
                                             battle::QueueLiveRiskRecovery};
  constexpr std::array<std::uint32_t, 3> kAddresses{0x80084EEC, 0x80084F40,
                                                    0x80084F94};
  const auto handler =
      Relocated(kAddresses.at(resource), kHandlers.at(resource));
  for (const auto amount : {-32768, -1, 0, 1, 32767}) {
    for (const auto pending : kPendingAmounts) {
      for (const int execute : {0, -1, 1}) {
        fixture.index = 1;
        fixture.execute = execute;
        CheckResourceRecoverySample(fixture, handler, resource, amount,
                                    pending);
      }
    }
  }
}
TEST_CASE("F00500 restores PP after calculation with full halfword wrapping",
          "[resource-dispatch][F00500]") {
  CheckResourceRecovery(0);
}
TEST_CASE("F00501 restores weapon DP without touching limb or PP neighbors",
          "[resource-dispatch][F00501]") {
  CheckResourceRecovery(1);
}
TEST_CASE("F00502 leaves zero risk recovery and its mode unchanged",
          "[resource-dispatch][F00502]") {
  CheckResourceRecovery(2);
}
TEST_CASE("F00503 reserved original selector relocates to VOID refusal",
          "[resource-dispatch][F00503]") {
  LiveFixture fixture;
  CHECK(Relocated(0x80084FF0, battle::RejectLiveEffectSelector) ==
        battle::RejectLiveEffectSelector);
}
TEST_CASE("F00504 reserved original selector relocates to VOID refusal",
          "[resource-dispatch][F00504]") {
  LiveFixture fixture;
  CHECK(Relocated(0x80084FF8, battle::RejectLiveEffectSelector) ==
        battle::RejectLiveEffectSelector);
}
TEST_CASE("F00505 reserved original selector relocates to VOID refusal",
          "[resource-dispatch][F00505]") {
  LiveFixture fixture;
  CHECK(Relocated(0x80085000, battle::RejectLiveEffectSelector) ==
        battle::RejectLiveEffectSelector);
}
}  // namespace
