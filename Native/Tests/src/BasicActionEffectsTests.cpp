#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/BasicActionEffects.hpp"
#include "Battle/BasicActionEffectsLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
#include "State/ScriptFlags.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
}

namespace {
namespace battle = VS::Battle;
using Handler = std::remove_extent_t<decltype(Battle_actionEffectMediators)>;
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

Battle_HitResult SeedHit() {
  std::array<std::uint32_t, sizeof(Battle_HitResult) / 4> words{};
  words.fill(0xD6A593EF);
  return std::bit_cast<Battle_HitResult>(words);
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
  Battle_HitResult hit = SeedHit();

  LiveFixture() {
    std::ranges::fill(Battle_actors, nullptr);
    std::ranges::fill(Battle_statusEffectParams, Battle_StatusEffectParams{});
    actor.stats = &stats;
    Battle_actors[41] = &actor;
    hit.target.targetActor = 41;
    hit.target.targetLimb = 5;
    stats.actorId = 255;
  }
  ~LiveFixture() {
    std::ranges::copy(saved_actors, Battle_actors);
    std::ranges::copy(saved_rules, Battle_statusEffectParams);
    std::ranges::copy(saved_mediators, Battle_actionEffectMediators);
  }
};

void CheckSourceSuccess() {
  LiveFixture fixture;
  std::ranges::fill(Battle_actors, nullptr);
  for (unsigned old = 0; old < 4; ++old) {
    auto outcome = static_cast<battle::SourceActionOutcome>(old);
    battle::MarkSourceActionSuccess(outcome);
    CHECK(outcome == battle::SourceActionOutcome::kSuccess);
    fixture.hit.sourceResult = old;
    auto expected =
        std::bit_cast<std::array<std::uint32_t, sizeof(Battle_HitResult) / 4>>(
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
      fixture.stats.actorId = static_cast<unsigned char>(identity);
      fixture.stats.currentHp =
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp));
      fixture.stats.statuses = 0x11;
      fixture.stats.statusImmunities = -1;
      Battle_statusEffectParams[kInstantDeathRule].statusFlags = 0x10;
      Battle_statusEffectParams[kInstantDeathRule].blockingStatuses = -1;
      const auto stats = Bytes(fixture.stats);
      auto expected = fixture.hit;
      expected.target.hp =
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp));
      expected.hpEffect = 1;
      if (identity != 128) {
        expected.inflictStatuses |= 0x10;
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
  std::ranges::fill(Battle_actors, nullptr);
  fixture.hit.target.targetActor = 255;
  for (std::int8_t limb = 0; limb < 6; ++limb) {
    for (const auto hp : {-32768, -16385, -7, 0, 16384, 32767}) {
      fixture.hit.target.targetLimb = limb;
      fixture.hit.target.hp = static_cast<std::int16_t>(hp);
      auto expected = fixture.hit;
      const auto doubled =
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp * 2));
      expected.armorDpChange = doubled;
      expected.limbs[limb].effects.opaque2 = doubled;
      expected.armorDpEffect = 1;
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
  const auto tables =
      std::span(Content_modulePointers, Content_modulePointerCount);
  const auto table = std::ranges::find(
      tables, reinterpret_cast<void**>(Battle_actionEffectMediators),
      &Content_ModulePointerTable::target);
  REQUIRE(table != tables.end());
  REQUIRE(VS::Content::RelocateModulePointerTable(
      *table, image, {Content_moduleSymbols, Content_moduleSymbolCount}));
  for (std::size_t index = 0; index < std::size(Battle_actionEffectMediators);
       ++index) {
    std::array<std::uint8_t, 4> bytes{};
    std::copy_n(image.begin() + static_cast<std::ptrdiff_t>(
                                    kMediatorOffset + index * bytes.size()),
                bytes.size(), bytes.begin());
    if (std::bit_cast<std::uint32_t>(bytes) == address) {
      REQUIRE(Battle_actionEffectMediators[index] == expected);
      return Battle_actionEffectMediators[index];
    }
  }
  FAIL("Original mediator address not found");
  return nullptr;
}

void CheckRelocatedSource(std::uint32_t address) {
  LiveFixture fixture;
  auto expected = fixture.hit;
  expected.sourceResult = 2;
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
  fixture.stats.currentHp = 997;
  Relocated(0x80084248, battle::QueueLiveInstantDeath)(
      nullptr, nullptr, &fixture.hit, kUnusedIndex, 0);
  CHECK(fixture.hit.target.hp == 997);
  CHECK(fixture.stats.currentHp == 997);
}
TEST_CASE("Armor wear original void mediator invokes the native policy",
          "[basic-action-effects][F00471]") {
  LiveFixture fixture;
  fixture.hit.target.hp = -7;
  Relocated(0x800842AC, battle::QueueLiveDamageBasedArmorWear)(
      nullptr, nullptr, &fixture.hit, kUnusedIndex, 0);
  CHECK(fixture.hit.armorDpChange == -14);
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
}  // namespace
