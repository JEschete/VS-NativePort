// Hand-maintained (tools/port/owned.csv).
#include "Menu/Results/ResultsStats.hpp"

#include <cstdint>
#include <optional>

namespace VS::Menu::Results {
namespace {
using Attribute = std::uint16_t AshleyResultStats::*;
using Vital = std::int16_t AshleyResultStats::*;

// The original adds in a register and stores the low halfword (sh), then
// reloads it zero-extended (lhu) for an unsigned `sltiu 1000` cap test.
std::uint16_t RaisedAttribute(std::uint16_t value, std::int32_t amount) {
  const auto raised = static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(value) + static_cast<std::uint32_t>(amount));
  return raised > kResultsStatCap ? static_cast<std::uint16_t>(kResultsStatCap)
                                  : raised;
}

// As above, but HP/MP reload sign-extended (lh) for a signed `slti 1000`.
std::int16_t RaisedVital(std::int16_t value, std::int32_t amount) {
  const auto raised = static_cast<std::int16_t>(static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(static_cast<std::uint16_t>(value)) +
      static_cast<std::uint32_t>(amount)));
  return raised > kResultsStatCap ? static_cast<std::int16_t>(kResultsStatCap)
                                  : raised;
}

std::optional<std::int32_t> ReadAttribute(AshleyStatHost& host,
                                          Attribute field) {
  const auto stats = host.ReadAshleyStats();
  if (!stats.has_value()) return std::nullopt;
  return static_cast<std::int32_t>((*stats).*field);
}

bool RaiseAttribute(AshleyStatHost& host, Attribute permanent,
                    Attribute current, std::int32_t amount) {
  auto stats = host.ReadAshleyStats();
  if (!stats.has_value()) return false;
  (*stats).*permanent = RaisedAttribute((*stats).*permanent, amount);
  (*stats).*current = RaisedAttribute((*stats).*current, amount);
  host.WriteAshleyStats(*stats);
  return true;
}

bool RaiseVital(AshleyStatHost& host, Vital maximum, Vital current,
                std::int32_t amount) {
  auto stats = host.ReadAshleyStats();
  if (!stats.has_value()) return false;
  (*stats).*maximum = RaisedVital((*stats).*maximum, amount);
  (*stats).*current = RaisedVital((*stats).*current, amount);
  host.WriteAshleyStats(*stats);
  return true;
}
}  // namespace

// decomp: MENUF.PRG getTotalStrength (src/MENU/MENUF.PRG/3B8.c:2888; was
// _getTotalStrength), 0x80108EA8-0x80108EC8.
std::optional<std::int32_t> AshleyTotalStrength(AshleyStatHost& host) {
  return ReadAttribute(host, &AshleyResultStats::total_strength);
}

// decomp: MENUF.PRG getTotalIntelligence (src/MENU/MENUF.PRG/3B8.c:2890; was
// _getTotalIntelligence), 0x80108EC8-0x80108EE8.
std::optional<std::int32_t> AshleyTotalIntelligence(AshleyStatHost& host) {
  return ReadAttribute(host, &AshleyResultStats::total_intelligence);
}

// decomp: MENUF.PRG getTotalAgility (src/MENU/MENUF.PRG/3B8.c:2895; was
// _getTotalAgility), 0x80108EE8-0x80108F08.
std::optional<std::int32_t> AshleyTotalAgility(AshleyStatHost& host) {
  return ReadAttribute(host, &AshleyResultStats::total_agility);
}

// decomp: MENUF.PRG raiseMaxStrength (src/MENU/MENUF.PRG/3B8.c:2897; was
// _raiseMaxStrength), 0x80108F08-0x80108F98.
bool ApplyStrengthBonus(AshleyStatHost& host, std::int32_t amount) {
  return RaiseAttribute(host, &AshleyResultStats::total_strength,
                        &AshleyResultStats::strength, amount);
}

// decomp: MENUF.PRG raiseMaxIntelligence (src/MENU/MENUF.PRG/3B8.c:2912; was
// _raiseMaxIntelligence), 0x80108F98-0x80109028.
bool ApplyIntelligenceBonus(AshleyStatHost& host, std::int32_t amount) {
  return RaiseAttribute(host, &AshleyResultStats::total_intelligence,
                        &AshleyResultStats::intelligence, amount);
}

// decomp: MENUF.PRG raiseMaxAgility (src/MENU/MENUF.PRG/3B8.c:2927; was
// _raiseMaxAgility), 0x80109028-0x801090B8.
bool ApplyAgilityBonus(AshleyStatHost& host, std::int32_t amount) {
  return RaiseAttribute(host, &AshleyResultStats::total_agility,
                        &AshleyResultStats::agility, amount);
}

// decomp: MENUF.PRG raiseMaxHP (src/MENU/MENUF.PRG/3B8.c:2942; was
// _raiseMaxHP), 0x801090B8-0x80109148.
bool ApplyHpBonus(AshleyStatHost& host, std::int32_t amount) {
  return RaiseVital(host, &AshleyResultStats::max_hp,
                    &AshleyResultStats::current_hp, amount);
}

// decomp: MENUF.PRG raiseMaxMP (src/MENU/MENUF.PRG/3B8.c:2957; was
// _raiseMaxMP), 0x80109148-0x801091D8.
bool ApplyMpBonus(AshleyStatHost& host, std::int32_t amount) {
  return RaiseVital(host, &AshleyResultStats::max_mp,
                    &AshleyResultStats::current_mp, amount);
}
}  // namespace VS::Menu::Results
