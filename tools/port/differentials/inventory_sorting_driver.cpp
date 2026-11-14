#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <type_traits>

#include "Menu/InventorySortingWorld.hpp"
#include "container_sorting_keys.hpp"

namespace {
namespace sorting = VS::Menu::Inventory;
namespace fixture = sorting::sorting_test;
namespace items = VS::Items;
using RecordBytes = std::array<std::uint8_t, sizeof(items::InventoryContents)>;
using IndexBytes = std::array<std::uint8_t, sizeof(items::InventoryIndices)>;

class World final : public fixture::World {
public:
    std::int32_t WeaponValue(
        std::int32_t stat, const items::WeaponLoadout& weapon) override
    {
        return Observe(fixture::Operation::kWeapon, stat, keys_.Weapon(stat, weapon));
    }
    std::int32_t ShieldValue(
        std::int32_t stat, const items::ShieldLoadout& shield) override
    {
        return Observe(fixture::Operation::kShield, stat, keys_.Shield(stat, shield));
    }
    std::int32_t MiscValue(std::int32_t stat, const items::InventoryMisc& item) override
    {
        return Observe(fixture::Operation::kMisc, stat, keys_.Misc(stat, item));
    }
    std::int32_t EquipmentValue(
        std::int32_t stat, const items::EquipmentPart& part) override
    {
        return Observe(fixture::Operation::kEquipment, stat, keys_.Equipment(stat, part));
    }

private:
    VS::Menu::container::sort_test::ProductionKeys keys_;
};

template <typename Range> void Read(Range& values)
{
    for (auto& value : values) {
        std::int64_t number = 0;
        std::cin >> number;
        value = static_cast<std::remove_reference_t<decltype(value)>>(number);
    }
}

template <typename Range> void Print(const Range& values)
{
    for (const auto value : values)
        std::cout << +value << ' ';
}

std::uint32_t Checksum(const items::InventoryContents& inventory)
{
    const auto bytes = std::bit_cast<RecordBytes>(inventory);
    std::uint32_t result = 0;
    for (std::size_t row = 0; row < bytes.size(); ++row) {
        result += static_cast<std::uint32_t>(row + 1) * bytes[row];
    }
    return result;
}

void Run(World& world, int kind, int category, int stat, int index,
    items::EquipmentPart& target)
{
    switch (kind) {
    case 0:
        sorting::SortWeapons(world, stat);
        return;
    case 1:
        sorting::SortShields(world, stat);
        return;
    case 2:
        sorting::SortMisc(world, stat);
        return;
    case 3:
        sorting::CopyEquipmentStats(target, category, index, world);
        return;
    default:
        sorting::SortEquipment(world, category, stat);
        return;
    }
}
} // namespace

int main()
{
    int kind = 0;
    int category = 0;
    int stat = 0;
    int index = 0;
    int mode = 0;
    std::size_t count = 0;
    while (std::cin >> kind >> category >> stat >> index >> mode >> count) {
        World world;
        Read(world.capacities);
        for (auto& route : world.routes) {
            int value = 0;
            std::cin >> value;
            route = static_cast<items::ItemCategory>(value);
        }
        RecordBytes initial { };
        RecordBytes replacement { };
        Read(initial);
        Read(replacement);
        world.inventory = std::bit_cast<items::InventoryContents>(initial);
        const auto next = std::bit_cast<items::InventoryContents>(replacement);
        IndexBytes indices { };
        Read(indices);
        world.indices = std::bit_cast<items::InventoryIndices>(indices);
        std::array<std::uint8_t, sizeof(items::EquipmentPart)> target_bytes { };
        Read(target_bytes);
        auto target = std::bit_cast<items::EquipmentPart>(target_bytes);
        world.trace_count = count;
        std::size_t keys = 0;
        world.hook = [&](auto& state, auto operation) {
            if (operation < fixture::Operation::kWeapon || ++keys != 1)
                return;
            if (mode == 1) {
                state.inventory = next;
                state.version = 1;
            } else if (mode == 2) {
                state.routes[category] = items::ItemCategory::kMisc;
            } else if (mode == 3) {
                state.capacities[category] = 4;
            } else if (mode == 4) {
                state.captured[count - 1] = 0;
            }
        };
        Run(world, kind, category, stat, index, target);
        std::cout << world.version << ' ';
        Print(world.capacities);
        std::cout << Checksum(world.inventory) << ' ';
        Print(std::bit_cast<std::array<std::uint8_t, sizeof(items::EquipmentPart)>>(
            target));
        Print(std::bit_cast<IndexBytes>(world.indices));
        for (const auto route : world.routes)
            std::cout << static_cast<int>(route) << ' ';
        std::cout << keys << ' ';
        for (const auto& event : world.events) {
            if (event.operation >= fixture::Operation::kWeapon) {
                std::cout << kind << ' ' << event.stat << ' ' << event.value << ' '
                          << event.hash << ' ' << event.version << ' ';
            }
        }
        std::cout << '\n';
    }
}
