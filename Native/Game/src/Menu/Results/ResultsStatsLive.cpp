// Hand-maintained (tools/port/owned.csv).
// Binds the typed results stat award (ResultsStats.cpp) to
// VS::Battle::characterState->stats.
#include "Menu/Results/ResultsStatsLive.hpp"

#include <cstdint>
#include <optional>

#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Core/Diagnostics.hpp"
#include "Menu/Results/ResultsStats.hpp"

namespace VS::Menu::Results {
namespace {
VS::Battle::ActorStats* LiveStats(const char* entry) {
  if (VS::Battle::characterState == nullptr) {
    VS::Core::Warnf("menu.results", "%s refused: no character state", entry);
    return nullptr;
  }
  if (VS::Battle::characterState->stats == nullptr) {
    VS::Core::Warnf("menu.results", "%s refused: character has no stat record",
                    entry);
    return nullptr;
  }
  return VS::Battle::characterState->stats;
}

class LiveAshleyStatHost final : public AshleyStatHost {
 public:
  explicit LiveAshleyStatHost(const char* entry) : entry_(entry) {}
  std::optional<AshleyResultStats> ReadAshleyStats() override {
    const VS::Battle::ActorStats* stats = LiveStats(entry_);
    if (stats == nullptr) return std::nullopt;
    return AshleyResultStats{
        .current_hp = stats->current_hp,
        .max_hp = stats->max_hp,
        .current_mp = stats->current_mp,
        .max_mp = stats->max_mp,
        .strength = stats->strength,
        .total_strength = stats->total_strength,
        .intelligence = stats->intelligence,
        .total_intelligence = stats->total_intelligence,
        .agility = stats->agility,
        .total_agility = stats->total_agility,
    };
  }
  void WriteAshleyStats(const AshleyResultStats& view) override {
    VS::Battle::ActorStats* stats = LiveStats(entry_);
    if (stats == nullptr) return;
    stats->current_hp = view.current_hp;
    stats->max_hp = view.max_hp;
    stats->current_mp = view.current_mp;
    stats->max_mp = view.max_mp;
    stats->strength = view.strength;
    stats->total_strength = view.total_strength;
    stats->intelligence = view.intelligence;
    stats->total_intelligence = view.total_intelligence;
    stats->agility = view.agility;
    stats->total_agility = view.total_agility;
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
