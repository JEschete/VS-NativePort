// Score rank policies (F03060, F03061, F02832, F02833, F02835, F04157)
// against the original MENUF.PRG and MENU9.PRG instructions. Each line is the
// flat vector of score_rank_common.LAYOUT; mode selects the entry. Prints the
// mode's outputs (score_rank_common.OUTPUT_NAMES), with the preset prior rank
// when no rank row matches.
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Menu/Score/ScoreRank.hpp"

namespace {
namespace s = VS::Menu::Score;

constexpr std::size_t kEarlyFirst = 22;
constexpr std::size_t kLateFirst = 184;
constexpr std::size_t kLateSplit = 216;

class Host final : public s::ProgressHost {
public:
    std::uint32_t order = 0;
    void ResetScreen() override { order = order * 10 + 1; }
    void ClearCursorMemory() override { order = order * 10 + 2; }
    void BackupCamera() override { order = order * 10 + 3; }
};

struct Vector {
    std::vector<std::uint64_t> values;
    std::size_t next = 0;
    std::uint64_t Take() { return values.at(next++); }
};

bool ReadVector(Vector& vector)
{
    // mode..cursor: see score_rank_common.LAYOUT.
    constexpr std::size_t kSize = 1 + 1 + 6 + 10 + 8 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 6 + 1 + 1 + 1 + 1 + 1 + 16
        + 16 + 64 + 1 + 1;
    vector.values.resize(kSize);
    for (auto& value : vector.values) {
        if (!(std::cin >> value))
            return false;
    }
    return true;
}

bool Bit(std::uint64_t word, std::size_t bit) { return ((word >> bit) & 1U) != 0; }

bool RunVector()
{
    Vector in;
    if (!ReadVector(in))
        return false;
    const auto mode = in.Take();
    s::ScoreRecord record;
    record.titles = static_cast<std::uint32_t>(in.Take());
    for (auto& kills : record.enemy_kills)
        kills = static_cast<std::uint16_t>(in.Take());
    for (auto& attacks : record.weapon_attacks)
        attacks = static_cast<std::uint16_t>(in.Take());
    for (auto& word : record.boss_trial_words)
        word = static_cast<std::uint32_t>(in.Take());
    record.max_chain = static_cast<std::uint16_t>(in.Take());
    record.completion_time_minutes = static_cast<std::uint32_t>(in.Take());
    record.map_completion = static_cast<std::uint32_t>(in.Take());
    record.opened_chest_count = static_cast<std::uint32_t>(in.Take());
    record.streak_score = static_cast<std::uint32_t>(in.Take());
    record.misc_score = static_cast<std::uint32_t>(in.Take());
    const auto clear = static_cast<std::uint32_t>(in.Take());
    const auto score = static_cast<std::uint32_t>(in.Take());
    const auto prior_rank = static_cast<std::uint32_t>(in.Take());

    s::ProgressWorld world;
    for (auto& boss : world.conditions.bosses_defeated)
        boss = in.Take() != 0;
    world.conditions.gold_key = in.Take() != 0;
    world.conditions.chest_key = in.Take() != 0;
    const auto early = in.Take();
    const auto late_low = in.Take();
    const auto late_high = in.Take();
    for (std::size_t index = 0; index < 32; ++index) {
        world.conditions.actions_unlocked.at(kEarlyFirst + index) = Bit(early, index);
        world.conditions.actions_unlocked.at(kLateFirst + index) = Bit(late_low, index);
    }
    for (std::size_t index = 0; index < 8; ++index)
        world.conditions.actions_unlocked.at(kLateSplit + index) = Bit(late_high, index);
    for (auto& room : world.room_flags)
        room = static_cast<std::uint32_t>(in.Take());
    for (auto& mask : world.countable_room_masks)
        mask = static_cast<std::uint32_t>(in.Take());
    for (auto& chest : world.chests_opened)
        chest = static_cast<std::uint8_t>(in.Take());
    world.clear_count = static_cast<std::uint8_t>(in.Take());
    world.cursor_memory = in.Take() != 0;
    world.record = record;

    const auto prior16 = static_cast<std::uint16_t>(prior_rank);
    switch (mode) {
    case 0: {
        // _determineRank stores the row index with sw; no row keeps the prior int.
        const auto row = s::RiskbreakerRankIndex(record.titles, score);
        std::cout << (row ? static_cast<std::int32_t>(*row) : static_cast<std::int32_t>(prior_rank));
        break;
    }
    case 1:
        std::cout << s::ScoreFromRecord(record, clear);
        break;
    case 2: std::cout << s::DetermineRiskbreakerRank(record.titles, score).value_or(prior16); break;
    case 3: std::cout << s::AwardTitles(record, static_cast<std::uint16_t>(clear), world.conditions); break;
    case 4: std::cout << s::CalculateScore(record, static_cast<std::uint16_t>(clear)); break;
    default: {
        Host host;
        const auto progress = s::CalculateProgress(world, host);
        std::cout << host.order << ' ' << world.room_flags[1] << ' ' << progress.map_completion << ' '
                  << progress.opened_chest_count << ' ' << progress.clear_count << ' ' << world.record.map_completion
                  << ' ' << world.record.opened_chest_count << ' ' << world.record.titles << ' ' << progress.score
                  << ' ' << progress.rank.value_or(prior16);
        break;
    }
    }
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    while (RunVector()) { }
    return 0;
}
