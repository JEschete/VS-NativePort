// Results stat reads and bonuses (F03066-F03073) against the original
// MENUF.PRG instructions. Each line: operation, amount, available flag and the
// ten stat halfwords (current/max HP, current/max MP, STR, total STR, INT,
// total INT, AGI, total AGI). Prints: refused, read result, the ten fields.
#include <cstdint>
#include <iostream>
#include <optional>

#include "Menu/Results/ResultsStats.hpp"

namespace {
namespace r = VS::Menu::Results;

class Host final : public r::AshleyStatHost {
public:
    r::AshleyResultStats stats {};
    bool available = true;
    std::optional<r::AshleyResultStats> ReadAshleyStats() override
    {
        if (!available)
            return std::nullopt;
        return stats;
    }
    void WriteAshleyStats(const r::AshleyResultStats& written) override { stats = written; }
};

bool RunVector()
{
    std::int64_t operation = 0;
    std::int64_t amount = 0;
    std::int64_t available = 0;
    std::int64_t f[10] {};
    if (!(std::cin >> operation >> amount >> available))
        return false;
    for (auto& value : f)
        std::cin >> value;

    Host host;
    host.available = available != 0;
    host.stats = {
        .current_hp = static_cast<std::int16_t>(f[0]),
        .max_hp = static_cast<std::int16_t>(f[1]),
        .current_mp = static_cast<std::int16_t>(f[2]),
        .max_mp = static_cast<std::int16_t>(f[3]),
        .strength = static_cast<std::uint16_t>(f[4]),
        .total_strength = static_cast<std::uint16_t>(f[5]),
        .intelligence = static_cast<std::uint16_t>(f[6]),
        .total_intelligence = static_cast<std::uint16_t>(f[7]),
        .agility = static_cast<std::uint16_t>(f[8]),
        .total_agility = static_cast<std::uint16_t>(f[9]),
    };
    const auto bonus = static_cast<std::int32_t>(amount);
    bool refused = false;
    std::int64_t result = 0;
    const auto read = [&](std::optional<std::int32_t> value) {
        refused = !value.has_value();
        result = value.value_or(0);
    };
    switch (operation) {
    case 0: read(r::AshleyTotalStrength(host)); break;
    case 1: read(r::AshleyTotalIntelligence(host)); break;
    case 2: read(r::AshleyTotalAgility(host)); break;
    case 3: refused = !r::ApplyStrengthBonus(host, bonus); break;
    case 4: refused = !r::ApplyIntelligenceBonus(host, bonus); break;
    case 5: refused = !r::ApplyAgilityBonus(host, bonus); break;
    case 6: refused = !r::ApplyHpBonus(host, bonus); break;
    case 7: refused = !r::ApplyMpBonus(host, bonus); break;
    default: return false;
    }
    const auto& s = host.stats;
    std::cout << (refused ? 1 : 0) << ' ' << result << ' ' << s.current_hp << ' ' << s.max_hp
              << ' ' << s.current_mp << ' ' << s.max_mp << ' ' << s.strength << ' '
              << s.total_strength << ' ' << s.intelligence << ' ' << s.total_intelligence << ' '
              << s.agility << ' ' << s.total_agility << '\n';
    return true;
}
} // namespace

int main()
{
    while (RunVector()) { }
    return 0;
}
