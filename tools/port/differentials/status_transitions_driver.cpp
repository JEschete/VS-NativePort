#include <array>
#include <bit>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Status/DetailTransitions.hpp"
#include "Menu/StatusTransitionsWorld.hpp"

namespace {
namespace fixture = vs_test::status_transitions;
namespace status = VS::Menu::Status;
namespace Framework = VS::Menu::Framework;
namespace Items = VS::Items;

enum class CaseKind : unsigned {
    kNavigate,
    kHoist,
    kBegin,
    kInitialize,
    kWeapon,
    kShield,
    kExit
};
constexpr int kUsage = 2;
constexpr int kInvalidInput = 3;
constexpr int kInvalidNavigation = 4;
constexpr int kInvalidCase = 5;
constexpr int kTruncatedRecord = 6;
constexpr int kAllocationFailure = 7;
constexpr int kInvalidSize = 8;
constexpr int kInvalidIndex = 9;
constexpr int kIoFailure = 10;
constexpr int kLocaleFailure = 11;

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
void Print(fixture::World& world, std::uint32_t result, const Record& record)
{
    std::cout << world.text.refusals.size() << ' ' << result;
    for (const auto& row : world.rows) {
        for (auto value : fixture::RowBytes(row)) {
            std::cout << ' ' << static_cast<unsigned>(value);
        }
    }
    for (auto value : world.text.buffer) {
        std::cout << ' ' << static_cast<unsigned>(value);
    }
    for (auto value : world.text.arguments.strings) {
        std::cout << ' ' << world.text.Address(value);
    }
    if constexpr (!std::is_same_v<Record, std::nullptr_t>) {
        const auto bytes =
            std::bit_cast<std::array<std::uint8_t, sizeof(Record)>>(record);
        for (auto value : bytes) {
            std::cout << ' ' << static_cast<unsigned>(value);
        }
    }
    std::cout << ' ' << world.events.size();
    for (auto value : world.events) {
        std::cout << ' ' << value;
    }
    std::cout << '\n';
}
int RunDriver(int argc)
{
    if (argc != 2) {
        return kUsage;
    }
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        unsigned kind = 0, seed = 0, page = 0;
        int row = 0, command = 0;
        std::uint32_t flags = 0, repeat = 0;
        if (!(input >> kind >> seed >> row >> flags >> repeat >> page >> command)) {
            return kInvalidInput;
        }
        const auto links = Read<Framework::StatusViewNavigation>(input);
        fixture::World world(seed);
        switch (static_cast<CaseKind>(kind)) {
        case CaseKind::kNavigate: {
            const auto result = status::NavigateDetailElements(
                { .selected = static_cast<std::uint32_t>(row),
                    .flags = flags,
                    .repeat = repeat,
                    .stat_page = static_cast<std::uint8_t>(page),
                    .links = links });
            if (!result) {
                return kInvalidNavigation;
            }
            Print(world, *result, nullptr);
            break;
        }
        case CaseKind::kHoist:
            static_cast<void>(status::HoistEquipmentRow(world, row));
            Print(world, 0, nullptr);
            break;
        case CaseKind::kBegin:
            static_cast<void>(status::BeginDetailTransition(world, row));
            Print(world, 0, nullptr);
            break;
        case CaseKind::kInitialize:
            static_cast<void>(status::InitializeDetailPanel(world, row, command));
            Print(world, 0, nullptr);
            break;
        case CaseKind::kWeapon: {
            const auto weapon = Read<Items::WeaponLoadout>(input);
            status::SetWeaponComponentRow(world, row, weapon, (command & 1) != 0);
            Print(world, 0, weapon);
            break;
        }
        case CaseKind::kShield: {
            const auto shield = Read<Items::ShieldLoadout>(input);
            status::SetShieldGemRow(world, row, shield, (command & 1) != 0);
            Print(world, 0, shield);
            break;
        }
        case CaseKind::kExit:
            status::ExitEquipmentDetails(world, command != 0);
            Print(world, 0, nullptr);
            break;
        default:
            return kInvalidCase;
        }
        if (!input) {
            return kTruncatedRecord;
        }
    }
    return 0;
}
} // namespace

int main(int argc, char**)
{
    try {
        return RunDriver(argc);
    } catch (const std::bad_alloc&) {
        std::fputs("status transition driver: allocation failed\n", stderr);
        return kAllocationFailure;
    } catch (const std::length_error&) {
        std::fputs("status transition driver: invalid storage size\n", stderr);
        return kInvalidSize;
    } catch (const std::out_of_range&) {
        std::fputs("status transition driver: invalid fixture index\n", stderr);
        return kInvalidIndex;
    } catch (const std::ios_base::failure&) {
        std::fputs("status transition driver: input/output failed\n", stderr);
        return kIoFailure;
    } catch (const std::bad_cast&) {
        std::fputs("status transition driver: invalid stream locale\n", stderr);
        return kLocaleFailure;
    }
}
