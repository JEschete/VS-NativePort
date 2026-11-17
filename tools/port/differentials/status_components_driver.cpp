#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

#include "Items/ItemRecords.hpp"
#include "Menu/Status/ComponentDescriptions.hpp"
#include "Menu/StatusComponentsWorld.hpp"

namespace {
namespace Fixture = vs_test::status_components;
namespace Status = VS::Menu::Status;
namespace Items = VS::Items;

template <typename Record> Record Read(std::istream& input)
{
    std::array<std::uint8_t, sizeof(Record)> bytes { };
    unsigned value = 0;
    for (auto& byte : bytes) {
        input >> value;
        byte = static_cast<std::uint8_t>(value);
    }
    return std::bit_cast<Record>(bytes);
}

template <typename Record>
void Print(Fixture::World& world, std::uint32_t result, const Record& record)
{
    std::cout << world.refusals.size() << ' ' << result << ' ' << 0x81234560U << ' '
              << 0xA5B6C7D8U;
    for (auto value : world.buffer)
        std::cout << ' ' << static_cast<unsigned>(value);
    for (auto value : world.arguments.strings)
        std::cout << ' ' << world.Address(value);
    for (auto value : world.selected)
        std::cout << ' ' << static_cast<unsigned>(value);
    if constexpr (!std::is_same_v<Record, std::nullptr_t>) {
        const auto bytes =
            std::bit_cast<std::array<std::uint8_t, sizeof(Record)>>(record);
        for (auto value : bytes)
            std::cout << ' ' << static_cast<unsigned>(value);
    }
    std::cout << ' ' << world.events.size();
    for (auto value : world.events)
        std::cout << ' ' << value;
    std::cout << '\n';
}
} // namespace

int main(int argc, char**)
{
    if (argc != 2)
        return 2;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        int kind = 0;
        std::uint32_t seed = 0;
        int row = 0;
        std::uint32_t flags = 0;
        unsigned page = 0;
        if (!(input >> kind >> seed >> row >> flags >> page))
            return 3;
        Fixture::World world(seed);
        auto context = world.Context();
        switch (kind) {
        case 0:
        case 1:
        case 2: {
            const auto part = Read<Items::EquipmentPart>(input);
            const auto result =
                kind == 0   ? Status::FormatBladeDescription(world, part, world.buffer)
                : kind == 1 ? Status::FormatGripDescription(world, part, world.buffer)
                            : Status::CopyGemDescription(world, part, world.buffer);
            Print(world, world.Address(result), part);
            break;
        }
        case 3: {
            const auto weapon = Read<Items::WeaponLoadout>(input);
            world.origin = &weapon;
            const auto result = Status::SelectWeaponComponent(context, row, weapon);
            Print(world, world.Address(result), weapon);
            break;
        }
        case 4: {
            const auto shield = Read<Items::ShieldLoadout>(input);
            world.origin = &shield;
            const auto result = Status::SelectShieldComponent(context, row, shield);
            Print(world, world.Address(result), shield);
            break;
        }
        case 5: {
            const auto armor = Read<Items::ArmorLoadout>(input);
            Print(world,
                world.Address(Status::ArmorDescription(world, armor, world.buffer)),
                armor);
            break;
        }
        case 6: {
            const auto accessory = Read<Items::AccessorySpawnLoadout>(input);
            Print(world,
                world.Address(
                    Status::AccessoryDescription(world, accessory, world.buffer)),
                accessory);
            break;
        }
        case 7:
            Print(world,
                Status::IsDetailElementSelectable(
                    row, flags, static_cast<std::uint8_t>(page)),
                nullptr);
            break;
        default:
            return 4;
        }
        if (!input)
            return 5;
    }
}
