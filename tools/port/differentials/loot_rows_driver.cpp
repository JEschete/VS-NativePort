#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Menu/Loot/LootRows.hpp"

namespace {
using VS::Items::BattleLoot;
using VS::Items::InventoryContents;
using VS::Items::ItemCategory;
using VS::Menu::Loot::LootRow;
using VS::Menu::Loot::LootStaging;

enum Mode : int {
    kRefresh,
    kAppend,
    kCountCategory,
    kDifficultyMatch,
    kStage,
    kBuild,
    kKeyFlag,
};

constexpr std::size_t kRowBytes = sizeof(LootStaging::rows);
constexpr std::size_t kHeaderValues = 9;

// The copyItem room check loot_rows_differential.py stubs into the original.
class PredicateCheck final : public VS::Menu::Loot::ClaimCheck {
public:
    explicit PredicateCheck(int salt)
        : salt_(salt)
    {
    }

    bool Fits(const LootRow& row, const InventoryContents&) override
    {
        calls.push_back(row);
        return (row.category * 37 + row.item_index * 11 + salt_) % 5 != 0;
    }

    std::vector<LootRow> calls;

private:
    int salt_;
};

template <typename Record> bool ReadBytes(Record& record)
{
    std::array<std::uint8_t, sizeof(Record)> bytes { };
    for (auto& byte : bytes) {
        int value = 0;
        if (!(std::cin >> value))
            return false;
        byte = static_cast<std::uint8_t>(value);
    }
    std::memcpy(&record, bytes.data(), sizeof(record));
    return true;
}

template <typename Record> void PrintBytes(const Record& record)
{
    std::array<std::uint8_t, sizeof(Record)> bytes { };
    std::memcpy(bytes.data(), &record, sizeof(record));
    for (const auto byte : bytes) {
        std::cout << ' ' << static_cast<unsigned>(byte);
    }
}

void PrintCursor(const LootStaging& staging)
{
    std::cout << ' ' << static_cast<unsigned>(staging.cursor.count) << ' '
              << static_cast<unsigned>(staging.cursor.offset) << ' '
              << static_cast<unsigned>(staging.cursor.selected);
}

void PrintListing(const LootStaging& staging, const PredicateCheck& check)
{
    PrintCursor(staging);
    PrintBytes(staging.rows);
    PrintBytes(staging.inventory);
    std::cout << ' ' << check.calls.size();
    for (const auto& row : check.calls) {
        std::cout << ' ' << static_cast<unsigned>(row.category) << ' '
                  << static_cast<unsigned>(row.item_index);
    }
}
} // namespace

int main()
{
    static_assert(sizeof(InventoryContents) == 0xF00);
    static_assert(sizeof(BattleLoot) == 0x220);
    static_assert(kRowBytes == 176 * 4);
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::array<int, kHeaderValues> header { };
    while (std::cin >> header[0]) {
        for (std::size_t i = 1; i < header.size(); ++i) {
            if (!(std::cin >> header[i]))
                return 1;
        }
        const auto [mode, salt, difficulty, count, offset, selected, arg0, arg1,
            record_count]
            = header;
        LootStaging staging { };
        staging.difficulty = static_cast<std::uint8_t>(difficulty);
        staging.cursor = {
            .count = static_cast<std::uint8_t>(count),
            .offset = static_cast<std::uint8_t>(offset),
            .selected = static_cast<std::uint8_t>(selected),
        };
        if (!ReadBytes(staging.rows) || !ReadBytes(staging.inventory))
            return 1;
        std::vector<BattleLoot> records(static_cast<std::size_t>(record_count));
        for (auto& record : records) {
            if (!ReadBytes(record))
                return 1;
        }

        PredicateCheck check(salt);
        switch (mode) {
        case kRefresh:
            VS::Menu::Loot::RefreshClaimableRows(staging, check);
            PrintListing(staging, check);
            break;
        case kAppend: {
            const bool appended = VS::Menu::Loot::AppendLootRow(
                staging, static_cast<ItemCategory>(arg0), arg1);
            std::cout << ' ' << (appended ? 1 : 0);
            PrintCursor(staging);
            PrintBytes(staging.rows);
            break;
        }
        case kCountCategory: {
            const auto counted = VS::Menu::Loot::CountContainedCategory(
                static_cast<ItemCategory>(arg0), staging.rows[0], staging.inventory);
            std::cout << ' ' << counted.count << ' ' << static_cast<int>(counted.status);
            break;
        }
        case kDifficultyMatch:
            std::cout << ' '
                      << VS::Menu::Loot::DropDifficultyMatch(
                             arg0, static_cast<std::uint8_t>(difficulty));
            break;
        case kStage:
            if (records.size() != 1)
                return 2;
            VS::Menu::Loot::StageDropRecord(records[0], 0, staging, check);
            PrintListing(staging, check);
            break;
        case kBuild:
            VS::Menu::Loot::BuildStagedInventory(records, staging, check);
            PrintListing(staging, check);
            break;
        case kKeyFlag: {
            const auto flag = VS::Menu::Loot::ClaimedKeyFlag(arg0);
            std::cout << ' ' << (flag ? 1 : 0) << ' ' << (flag ? *flag : 0);
            break;
        }
        default:
            return 2;
        }
        std::cout << '\n';
    }
    return 0;
}
