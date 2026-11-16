#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENUF.PRG results screen: Ashley's total STR/INT/AGI reads and the stat
// bonuses the bonus reels award (F03066-F03073).

#include <cstdint>
#include <optional>

namespace VS::Menu::Results {
// Every bonus clamps the raised value to this cap (MENUF.PRG 3B8.c:2892).
inline constexpr std::int32_t kResultsStatCap = 999;

// Ashley's stat-record fields (vs_battle_actor2 0x18-0x2D) the results award
// reads and raises. HP/MP are signed halfwords; STR/INT/AGI are unsigned, so
// a bonus that wraps them below zero reads back above the cap.
struct AshleyResultStats {
  std::int16_t current_hp{};           // 0x18
  std::int16_t max_hp{};               // 0x1A
  std::int16_t current_mp{};           // 0x1C
  std::int16_t max_mp{};               // 0x1E
  std::uint16_t strength{};            // 0x22, current
  std::uint16_t total_strength{};      // 0x24, permanent
  std::uint16_t intelligence{};        // 0x26, current
  std::uint16_t total_intelligence{};  // 0x28, permanent
  std::uint16_t agility{};             // 0x2A, current
  std::uint16_t total_agility{};       // 0x2C, permanent
};

// Ashley's stat record as the results screen sees it. ReadAshleyStats returns
// nothing when the character or its stat record is missing; the operations
// below then refuse instead of reading or writing through it.
class AshleyStatHost {
 public:
  AshleyStatHost() = default;
  AshleyStatHost(const AshleyStatHost&) = delete;
  AshleyStatHost& operator=(const AshleyStatHost&) = delete;
  AshleyStatHost(AshleyStatHost&&) = delete;
  AshleyStatHost& operator=(AshleyStatHost&&) = delete;
  virtual ~AshleyStatHost() = default;
  [[nodiscard]] virtual std::optional<AshleyResultStats> ReadAshleyStats() = 0;
  virtual void WriteAshleyStats(const AshleyResultStats& stats) = 0;
};

// Permanent (total) attribute reads; nothing when the record is missing.
[[nodiscard]] std::optional<std::int32_t> AshleyTotalStrength(
    AshleyStatHost& host);
[[nodiscard]] std::optional<std::int32_t> AshleyTotalIntelligence(
    AshleyStatHost& host);
[[nodiscard]] std::optional<std::int32_t> AshleyTotalAgility(
    AshleyStatHost& host);

// Raise the permanent value and then the current value by `amount`, each
// truncated to its 16-bit field and clamped to kResultsStatCap. Returns false
// (writing nothing) when the record is missing.
bool ApplyStrengthBonus(AshleyStatHost& host, std::int32_t amount);
bool ApplyIntelligenceBonus(AshleyStatHost& host, std::int32_t amount);
bool ApplyAgilityBonus(AshleyStatHost& host, std::int32_t amount);
// Raise the maximum and then the current HP/MP the same way, with a signed
// cap comparison: values that wrap negative stay negative.
bool ApplyHpBonus(AshleyStatHost& host, std::int32_t amount);
bool ApplyMpBonus(AshleyStatHost& host, std::int32_t amount);
}  // namespace VS::Menu::Results
