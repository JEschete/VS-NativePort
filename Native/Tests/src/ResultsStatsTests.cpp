#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "Battle/AbilityState.hpp"
#include "Battle/Actor.h"
#include "Menu/Results/ResultsStats.hpp"
#include "Menu/Results/ResultsStatsLive.hpp"

namespace {
namespace r = VS::Menu::Results;

class RecordHost final : public r::AshleyStatHost {
 public:
  r::AshleyResultStats stats{};
  bool available = true;
  int writes = 0;

  std::optional<r::AshleyResultStats> ReadAshleyStats() override {
    if (!available) return std::nullopt;
    return stats;
  }
  void WriteAshleyStats(const r::AshleyResultStats& written) override {
    stats = written;
    ++writes;
  }
};

r::AshleyResultStats Sample() {
  return {.current_hp = 101,
          .max_hp = 202,
          .current_mp = 33,
          .max_mp = 44,
          .strength = 55,
          .total_strength = 66,
          .intelligence = 77,
          .total_intelligence = 88,
          .agility = 99,
          .total_agility = 111};
}

bool SameStats(const r::AshleyResultStats& a, const r::AshleyResultStats& b) {
  return a.current_hp == b.current_hp && a.max_hp == b.max_hp &&
         a.current_mp == b.current_mp && a.max_mp == b.max_mp &&
         a.strength == b.strength && a.total_strength == b.total_strength &&
         a.intelligence == b.intelligence &&
         a.total_intelligence == b.total_intelligence &&
         a.agility == b.agility && a.total_agility == b.total_agility;
}

std::int32_t ReadOr(const std::optional<std::int32_t>& value,
                    std::int32_t fallback) {
  return value.has_value() ? *value : fallback;
}

using AttributeBonus = bool (*)(r::AshleyStatHost&, std::int32_t);
using Attribute = std::uint16_t r::AshleyResultStats::*;
using Vital = std::int16_t r::AshleyResultStats::*;

// Raises permanent then current; each is truncated to 16 bits and clamped to
// 999 with an unsigned comparison.
void CheckAttributeCap(AttributeBonus apply, Attribute permanent,
                       Attribute current) {
  RecordHost host;
  host.stats = Sample();
  host.stats.*permanent = 990;
  host.stats.*current = 100;
  auto expected = host.stats;
  CHECK(apply(host, 20));
  expected.*permanent = 999;
  expected.*current = 120;
  CHECK(SameStats(host.stats, expected));
  CHECK(host.writes == 1);
}

void CheckAttributeWrap(AttributeBonus apply, Attribute permanent,
                        Attribute current) {
  RecordHost host;
  host.stats = Sample();
  // A bonus that wraps below zero reads back as 0xFFFF and clamps to the cap.
  host.stats.*permanent = 3;
  host.stats.*current = 0;
  CHECK(apply(host, -4));
  CHECK(host.stats.*permanent == 999);
  CHECK(host.stats.*current == 999);

  // Only the low 16 bits of the sum survive, and 0x7FFF+1 is not negative.
  host.stats.*permanent = 10;
  host.stats.*current = 0x7FFF;
  CHECK(apply(host, 0x10005));
  CHECK(host.stats.*permanent == 15);
  CHECK(host.stats.*current == 999);
}

void CheckAttributeBonus(AttributeBonus apply, Attribute permanent,
                         Attribute current) {
  CheckAttributeCap(apply, permanent, current);
  CheckAttributeWrap(apply, permanent, current);
}

// As above with a signed comparison: wrapped negative HP/MP stays negative.
void CheckVitalBonus(AttributeBonus apply, Vital maximum, Vital current) {
  RecordHost host;
  host.stats = Sample();
  host.stats.*maximum = 980;
  host.stats.*current = -5;
  auto expected = host.stats;
  CHECK(apply(host, 30));
  expected.*maximum = 999;
  expected.*current = 25;
  CHECK(SameStats(host.stats, expected));

  host.stats.*maximum = 0x7FFF;
  host.stats.*current = 10;
  CHECK(apply(host, 1));
  CHECK(host.stats.*maximum == -0x8000);
  CHECK(host.stats.*current == 11);

  host.stats.*maximum = 0;
  host.stats.*current = 999;
  CHECK(apply(host, 0x10000));
  CHECK(host.stats.*maximum == 0);
  CHECK(host.stats.*current == 999);
}

void CheckBonusRefusal(AttributeBonus apply) {
  RecordHost host;
  host.stats = Sample();
  host.available = false;
  CHECK_FALSE(apply(host, 5));
  CHECK(host.writes == 0);
  CHECK(SameStats(host.stats, Sample()));
}

class LiveCharacter {
 public:
  Battle_ActorStats stats{};
  Battle_Actor actor{};

  LiveCharacter() : saved_(VS::Battle::characterState) {
    actor.stats = &stats;
    VS::Battle::characterState = &actor;
  }
  ~LiveCharacter() { VS::Battle::characterState = saved_; }
  LiveCharacter(const LiveCharacter&) = delete;
  LiveCharacter& operator=(const LiveCharacter&) = delete;
  LiveCharacter(LiveCharacter&&) = delete;
  LiveCharacter& operator=(LiveCharacter&&) = delete;

 private:
  Battle_Actor* saved_;
};
}  // namespace

TEST_CASE("F03066 total strength reads the permanent field",
          "[results-stats]") {
  RecordHost host;
  host.stats = Sample();
  CHECK(ReadOr(r::AshleyTotalStrength(host), -1) == 66);
  host.stats.total_strength = 0xFFFF;
  CHECK(ReadOr(r::AshleyTotalStrength(host), -1) == 0xFFFF);
  host.available = false;
  CHECK_FALSE(r::AshleyTotalStrength(host).has_value());
  CHECK(host.writes == 0);
}

TEST_CASE("F03067 total intelligence reads the permanent field",
          "[results-stats]") {
  RecordHost host;
  host.stats = Sample();
  CHECK(ReadOr(r::AshleyTotalIntelligence(host), -1) == 88);
  host.stats.total_intelligence = 0x8000;
  CHECK(ReadOr(r::AshleyTotalIntelligence(host), -1) == 0x8000);
  host.available = false;
  CHECK_FALSE(r::AshleyTotalIntelligence(host).has_value());
}

TEST_CASE("F03068 total agility reads the permanent field", "[results-stats]") {
  RecordHost host;
  host.stats = Sample();
  CHECK(ReadOr(r::AshleyTotalAgility(host), -1) == 111);
  host.stats.total_agility = 0xFFFF;
  CHECK(ReadOr(r::AshleyTotalAgility(host), -1) == 0xFFFF);
  host.available = false;
  CHECK_FALSE(r::AshleyTotalAgility(host).has_value());
}

TEST_CASE("F03069 strength bonus raises total and current with the cap",
          "[results-stats]") {
  CheckAttributeBonus(r::ApplyStrengthBonus,
                      &r::AshleyResultStats::total_strength,
                      &r::AshleyResultStats::strength);
  CheckBonusRefusal(r::ApplyStrengthBonus);
}

TEST_CASE("F03070 intelligence bonus raises total and current with the cap",
          "[results-stats]") {
  CheckAttributeBonus(r::ApplyIntelligenceBonus,
                      &r::AshleyResultStats::total_intelligence,
                      &r::AshleyResultStats::intelligence);
  CheckBonusRefusal(r::ApplyIntelligenceBonus);
}

TEST_CASE("F03071 agility bonus raises total and current with the cap",
          "[results-stats]") {
  CheckAttributeBonus(r::ApplyAgilityBonus,
                      &r::AshleyResultStats::total_agility,
                      &r::AshleyResultStats::agility);
  CheckBonusRefusal(r::ApplyAgilityBonus);
}

TEST_CASE("F03072 HP bonus raises maximum and current with a signed cap",
          "[results-stats]") {
  CheckVitalBonus(r::ApplyHpBonus, &r::AshleyResultStats::max_hp,
                  &r::AshleyResultStats::current_hp);
  CheckBonusRefusal(r::ApplyHpBonus);
}

TEST_CASE("F03073 MP bonus raises maximum and current with a signed cap",
          "[results-stats]") {
  CheckVitalBonus(r::ApplyMpBonus, &r::AshleyResultStats::max_mp,
                  &r::AshleyResultStats::current_mp);
  CheckBonusRefusal(r::ApplyMpBonus);
}

TEST_CASE("Live results totals read the character stat record",
          "[results-stats]") {
  LiveCharacter live;
  live.stats.totalStrength = 321;
  live.stats.strength = 1;
  live.stats.totalIntelligence = 0xFFFF;
  live.stats.totalAgility = 999;
  CHECK(r::AshleyLiveTotalStrength() == 321);
  CHECK(r::AshleyLiveTotalIntelligence() == 0xFFFF);
  CHECK(r::AshleyLiveTotalAgility() == 999);
}

TEST_CASE("Live results bonuses write only their own fields",
          "[results-stats]") {
  LiveCharacter live;
  live.stats.totalStrength = 995;
  live.stats.strength = 900;
  live.stats.maxHp = 400;
  live.stats.currentHp = -3;
  live.stats.maxMp = 998;
  live.stats.risk = 77;
  r::ApplyLiveStrengthBonus(10);
  r::ApplyLiveHpBonus(5);
  r::ApplyLiveMpBonus(5);
  r::ApplyLiveIntelligenceBonus(2);
  r::ApplyLiveAgilityBonus(3);
  CHECK(live.stats.totalStrength == 999);
  CHECK(live.stats.strength == 910);
  CHECK(live.stats.maxHp == 405);
  CHECK(live.stats.currentHp == 2);
  CHECK(live.stats.maxMp == 999);
  CHECK(live.stats.totalIntelligence == 2);
  CHECK(live.stats.agility == 3);
  CHECK(live.stats.risk == 77);
}

TEST_CASE("Live results stats refuse a missing character or record",
          "[results-stats]") {
  LiveCharacter live;
  live.stats.totalStrength = 50;
  live.actor.stats = nullptr;
  CHECK(r::AshleyLiveTotalStrength() == 0);
  r::ApplyLiveStrengthBonus(10);
  CHECK(live.stats.totalStrength == 50);
  CHECK_FALSE(r::ReadLiveAshleyResultStats().has_value());
  VS::Battle::characterState = nullptr;
  CHECK(r::AshleyLiveTotalAgility() == 0);
  r::ApplyLiveHpBonus(10);
  r::WriteLiveAshleyResultStats(r::AshleyResultStats{});
  CHECK(live.stats.maxHp == 0);
}
