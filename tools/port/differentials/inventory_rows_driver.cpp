// Native side of the inventory_rows_* differentials: reads one vector per line and
// prints the typed cores' observable results for _getParentWeaponIndex,
// _getParentShieldIndex, _populateItemRows and _restoreItemMenu.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <type_traits>
#include <vector>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Inventory/ItemListRows.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;
namespace items = VS::Items;
namespace framework = VS::Menu::Framework;
using InventoryBytes = std::array<std::uint8_t, sizeof(items::InventoryContents)>;
static_assert(sizeof(items::InventoryContents) == 0xF00);

constexpr std::size_t kRows = 64;
constexpr std::size_t kUnlockable = 140;
constexpr std::size_t kListRows = 20;
constexpr std::size_t kMemory = 128;

std::int64_t Next()
{
    std::int64_t number = 0;
    std::cin >> number;
    return number;
}

template <typename Range> void Read(Range& values)
{
    for (auto& value : values)
        value = static_cast<std::remove_reference_t<decltype(value)>>(Next());
}

items::InventoryContents ReadInventory()
{
    InventoryBytes bytes { };
    Read(bytes);
    return std::bit_cast<items::InventoryContents>(bytes);
}

struct Call {
    int category;
    std::size_t slot;
    std::size_t row;
};

class RowsWorld final : public inventory::ItemRowsWorld {
public:
    std::array<std::uint8_t, kUnlockable> unlocked { };
    std::array<std::uint32_t, kRows> formatted { };
    std::array<std::uint32_t, kRows> row_types { };
    std::array<std::uint8_t, kRows> actions { };
    std::vector<Call> calls;

    bool ActionUnlocked(std::uint32_t action) override
    {
        return action < unlocked.size() && unlocked[action] != 0;
    }
    void FormatRow(items::ItemCategory category, std::size_t slot, std::size_t row) override
    {
        calls.push_back({ static_cast<int>(category), slot, row });
        row_types.at(row) = formatted.at(row);
    }
    void AddRowFlags(std::size_t row, std::uint32_t flags) override
    {
        row_types.at(row) |= flags;
    }
    void StoreMiscAction(std::size_t row, std::uint8_t action) override
    {
        actions.at(row) = action;
    }
};

void Rows()
{
    const auto context = Next();
    const auto category = Next();
    const auto capacity = Next();
    std::vector<std::uint8_t> order(static_cast<std::size_t>(Next()));
    Read(order);
    std::array<std::uint8_t, inventory::kMiscActionMapEntries> map { };
    Read(map);
    RowsWorld world;
    Read(world.unlocked);
    Read(world.formatted);
    Read(world.row_types);
    Read(world.actions);
    const auto contents = ReadInventory();
    const inventory::ItemRowsInput input {
        .category = static_cast<items::ItemCategory>(category),
        .context = context == 0 ? inventory::ItemRowsContext::kItemMenu
                                : inventory::ItemRowsContext::kLoot,
        .order = order,
        .capacity = static_cast<std::uint8_t>(capacity),
        .misc_action_map = map,
        .row_capacity = kRows,
    };
    const auto result = inventory::BuildItemRows(input, contents, world);
    std::cout << (result ? *result : -1 - static_cast<int>(result.error())) << ' ';
    for (const auto value : world.row_types)
        std::cout << value << ' ';
    for (const auto value : world.actions)
        std::cout << +value << ' ';
    std::cout << world.calls.size() << ' ';
    for (const auto& call : world.calls)
        std::cout << call.category << ' ' << call.slot << ' ' << call.row << ' ' << 1 << ' ';
}

class RestoreWorld final : public inventory::ItemRowsRestoreWorld {
public:
    std::uint8_t animation = 0;
    std::uint8_t step = 0;
    std::int64_t advances = 0;
    std::size_t mutate_at = 0;
    std::uint8_t mutate_value = 0;
    std::size_t memory_index = 0;
    std::size_t row_calls = 0;
    std::array<std::uint8_t, kMemory> memory { };
    std::array<std::size_t, kListRows> row_map { };
    std::array<framework::MenuRow, kListRows> rows { };

    std::uint8_t ListAnimationRow() override { return animation; }
    void AdvanceListRows() override
    {
        animation = static_cast<std::uint8_t>(animation + step);
        ++advances;
    }
    framework::MenuRow* Row(std::int32_t index) override
    {
        // The first kListRows calls are the core's up-front check; the original
        // looks each row up once, in the settling pass.
        if (row_calls >= kListRows && row_calls - kListRows == mutate_at)
            memory[memory_index] = mutate_value;
        ++row_calls;
        return &rows.at(row_map.at(static_cast<std::size_t>(index) - 20));
    }
    std::span<const std::uint8_t> CursorMemory() override { return memory; }
};

void Restore()
{
    RestoreWorld world;
    const auto category = static_cast<std::int32_t>(Next());
    world.animation = static_cast<std::uint8_t>(Next());
    world.step = static_cast<std::uint8_t>(Next());
    world.mutate_at = static_cast<std::size_t>(Next());
    world.mutate_value = static_cast<std::uint8_t>(Next());
    world.memory_index = static_cast<std::size_t>(Next());
    Read(world.memory);
    Read(world.row_map);
    for (auto& row : world.rows) {
        row.state = static_cast<framework::RowState>(Next());
        row.selected = static_cast<std::uint8_t>(Next());
        row.x = static_cast<std::int16_t>(Next());
        row.target0 = static_cast<std::int16_t>(Next());
    }
    const auto result = inventory::RestoreItemRows(world, category);
    std::cout << (result ? 0 : 1 + static_cast<int>(result.error())) << ' ' << +world.animation
              << ' ' << world.advances << ' ';
    for (const auto value : world.memory)
        std::cout << +value << ' ';
    for (const auto& row : world.rows) {
        std::cout << static_cast<int>(row.state) << ' ' << +row.selected << ' '
                  << static_cast<std::uint16_t>(row.x) << ' '
                  << static_cast<std::uint16_t>(row.target0) << ' ';
    }
}
} // namespace

int main()
{
    std::int64_t kind = 0;
    while (std::cin >> kind) {
        if (kind < 2) {
            const auto category = static_cast<std::int32_t>(Next());
            const auto index = static_cast<std::int32_t>(Next());
            const auto contents = ReadInventory();
            if (kind == 0) {
                const auto value = inventory::ParentWeaponIndex(category, index, contents);
                std::cout << (value ? static_cast<std::int64_t>(*value) : -1);
            } else {
                const auto value = inventory::ParentShieldIndex(category, index, contents);
                std::cout << (value ? static_cast<std::int64_t>(*value) : -1);
            }
        } else if (kind == 2) {
            Rows();
        } else {
            Restore();
        }
        std::cout << '\n';
    }
}
