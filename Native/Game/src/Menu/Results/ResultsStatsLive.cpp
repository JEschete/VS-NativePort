// Hand-maintained (tools/port/owned.csv).
// Binds the typed results stat award (ResultsStats.cpp) to
// Battle_characterState->stats.
#include "Menu/Results/ResultsStatsLive.hpp"

#include <cstdint>
#include <optional>

#include "Menu/Results/ResultsStats.hpp"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Core/Diagnostics.h"
}

namespace VS::Menu::Results {
namespace {
Battle_ActorStats* LiveStats(const char* entry) {
  if (Battle_characterState == nullptr) {
    Core_Warnf("menu.results", "%s refused: no character state", entry);
    return nullptr;
  }
  if (Battle_characterState->stats == nullptr) {
    Core_Warnf("menu.results", "%s refused: character has no stat record",
               entry);
    return nullptr;
  }
  return Battle_characterState->stats;
}

class LiveAshleyStatHost final : public AshleyStatHost {
 public:
  explicit LiveAshleyStatHost(const char* entry) : entry_(entry) {}
  std::optional<AshleyResultStats> ReadAshleyStats() override {
    const Battle_ActorStats* stats = LiveStats(entry_);
    if (stats == nullptr) return std::nullopt;
    return AshleyResultStats{
        .current_hp = stats->currentHp,
        .max_hp = stats->maxHp,
        .current_mp = stats->currentMp,
        .max_mp = stats->maxMp,
        .strength = stats->strength,
        .total_strength = stats->totalStrength,
        .intelligence = stats->intelligence,
        .total_intelligence = stats->totalIntelligence,
        .agility = stats->agility,
        .total_agility = stats->totalAgility,
    };
  }
  void WriteAshleyStats(const AshleyResultStats& view) override {
    Battle_ActorStats* stats = LiveStats(entry_);
    if (stats == nullptr) return;
    stats->currentHp = view.current_hp;
    stats->maxHp = view.max_hp;
    stats->currentMp = view.current_mp;
    stats->maxMp = view.max_mp;
    stats->strength = view.strength;
    stats->totalStrength = view.total_strength;
    stats->intelligence = view.intelligence;
    stats->totalIntelligence = view.total_intelligence;
    stats->agility = view.agility;
    stats->totalAgility = view.total_agility;
  }

 private:
  const char* entry_;
};

int ReadOrZero(const std::optional<std::int32_t>& value) {
  return value.has_value() ? *value : 0;
}
}  // namespace

std::optional<AshleyResultStats> ReadLiveAshleyResultStats() {
  LiveAshleyStatHost host("ReadLiveAshleyResultStats");
  return host.ReadAshleyStats();
}

void WriteLiveAshleyResultStats(const AshleyResultStats& stats) {
  LiveAshleyStatHost host("WriteLiveAshleyResultStats");
  host.WriteAshleyStats(stats);
}

int AshleyLiveTotalStrength() {
  LiveAshleyStatHost host("AshleyLiveTotalStrength");
  return ReadOrZero(AshleyTotalStrength(host));
}

int AshleyLiveTotalIntelligence() {
  LiveAshleyStatHost host("AshleyLiveTotalIntelligence");
  return ReadOrZero(AshleyTotalIntelligence(host));
}

int AshleyLiveTotalAgility() {
  LiveAshleyStatHost host("AshleyLiveTotalAgility");
  return ReadOrZero(AshleyTotalAgility(host));
}

void ApplyLiveStrengthBonus(int amount) {
  LiveAshleyStatHost host("ApplyLiveStrengthBonus");
  ApplyStrengthBonus(host, amount);
}

void ApplyLiveIntelligenceBonus(int amount) {
  LiveAshleyStatHost host("ApplyLiveIntelligenceBonus");
  ApplyIntelligenceBonus(host, amount);
}

void ApplyLiveAgilityBonus(int amount) {
  LiveAshleyStatHost host("ApplyLiveAgilityBonus");
  ApplyAgilityBonus(host, amount);
}

void ApplyLiveHpBonus(int amount) {
  LiveAshleyStatHost host("ApplyLiveHpBonus");
  ApplyHpBonus(host, amount);
}

void ApplyLiveMpBonus(int amount) {
  LiveAshleyStatHost host("ApplyLiveMpBonus");
  ApplyMpBonus(host, amount);
}
}  // namespace VS::Menu::Results
