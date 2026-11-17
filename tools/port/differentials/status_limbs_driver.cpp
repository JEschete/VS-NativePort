#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "..\..\..\Native\Tests\include\Menu\StatusLimbControllersWorld.hpp"

namespace st = VS::Menu::Status;
namespace rd = VS::Render;
namespace fixture = status_limb_tests;

namespace {
using Bytes = std::vector<std::uint8_t>;
constexpr std::uint32_t kMenuBase = 0x80102800;
constexpr std::uint32_t kBattleBase = 0x80068800;
constexpr std::uint32_t kStatusText = 0x80107448;
constexpr std::uint32_t kBattleText = 0x800EA868;

Bytes Load(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot read original fixture image");
    return { std::istreambuf_iterator<char>(input), { } };
}
std::uint32_t ReadWord(
    std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t size = 4)
{
    if (offset > bytes.size() || size > bytes.size() - offset) {
        throw std::runtime_error("fixture table is outside image");
    }
    std::uint32_t word = 0;
    for (std::size_t index = 0; index < size; ++index) {
        word |= std::uint32_t { bytes[offset + index] } << (index * 8);
    }
    return word;
}
std::span<const std::uint8_t> Text(
    std::span<const std::uint8_t> image, std::size_t offset, bool encoded = true)
{
    const auto tail = image.subspan(offset);
    for (std::size_t index = 0; index < tail.size(); ++index) {
        if (tail[index] == (encoded ? 0xE7 : 0))
            return tail.first(index + 1);
        if (encoded && tail[index] >= 0xEC)
            ++index;
    }
    throw std::runtime_error("unterminated fixture string");
}
std::span<const std::uint8_t> Indexed(
    std::span<const std::uint8_t> bank, std::size_t index)
{
    return Text(bank, ReadWord(bank, index * 2, 2) * 2);
}
std::int64_t Number(std::istream& input)
{
    std::int64_t value = 0;
    if (!(input >> value))
        throw std::runtime_error("short fixture vector");
    return value;
}
template <typename T> T Read(std::istream& input)
{
    return static_cast<T>(Number(input));
}
template <typename T, std::size_t N>
void ReadArray(std::istream& input, std::array<T, N>& array)
{
    for (auto& value : array)
        value = Read<T>(input);
}

void LoadLabels(fixture::World& world, const Bytes& menu, const Bytes& battle)
{
    for (std::size_t index = 0; index < world.name_labels.size(); ++index) {
        const auto pointer = ReadWord(battle, 0x800EA620 - kBattleBase + index * 4);
        if (pointer == 0)
            continue;
        world.name_offsets[index] = pointer - kBattleBase;
        const auto label = Text(battle, world.name_offsets[index], false);
        world.name_labels[index].assign(label.begin(), label.end());
    }
    for (std::size_t index = 0; index < world.condition_labels.size(); ++index) {
        const auto pointer = ReadWord(menu, 0x80108084 - kMenuBase + index * 4);
        world.condition_offsets[index] = pointer - kMenuBase;
        const auto label = Text(menu, world.condition_offsets[index], false);
        world.condition_labels[index].assign(label.begin(), label.end());
    }
    const auto status_bank = std::span(menu).subspan(kStatusText - kMenuBase);
    for (std::size_t index = 0; index < world.descriptions.size(); ++index) {
        world.description_offsets[index] =
            static_cast<std::uint16_t>(ReadWord(status_bank, (18 + index) * 2, 2) * 2);
        const auto label = Text(status_bank, world.description_offsets[index]);
        world.descriptions[index].assign(label.begin(), label.end());
    }
}

struct Vector {
    int mode = 0;
    std::array<std::int32_t, 4> args { };
    std::array<std::int16_t, fixture::kEquipmentStats> equipment { };
    std::array<std::int16_t, st::kStatusLimbSlots * st::kLimbAffinitySlots>
        affinities { };
    std::array<std::int16_t, st::kStatusLimbSlots * st::kLimbDamageTypeSlots> types { };
    std::array<std::uint8_t, fixture::kTextBytes> output { };
    std::array<std::size_t, 2> context { };
    std::size_t alias = 0;
};

Vector Input(std::istream& input, fixture::World& world)
{
    Vector value;
    value.mode = Read<int>(input);
    ReadArray(input, value.args);
    auto& frame = world.navigation_frame;
    world.panel_frame.selected_actor = Read<std::int32_t>(input);
    frame.selected_element = Read<std::uint8_t>(input);
    frame.yaw = Read<std::int32_t>(input);
    frame.pitch = Read<std::int32_t>(input);
    world.panel_frame.fade_screen = Read<std::int32_t>(input);
    world.panel_frame.background_first = Read<std::int32_t>(input);
    frame.cursor_color = Read<std::uint8_t>(input);
    world.navigation.cursor_animation = Read<std::uint8_t>(input);
    world.navigation.selected = Read<std::uint8_t>(input);
    world.navigation.count = Read<std::uint8_t>(input);
    const int target = Read<int>(input);
    if (target >= 0)
        world.navigation.target_slot = target;
    ReadArray(input, world.navigation.active);
    world.panel.phase = Read<std::uint8_t>(input);
    world.panel.actor_slot = Read<std::uint8_t>(input);
    ReadArray(input, world.panel.animation_steps);
    world.panel_frame.selection_and_dimming = Read<std::uint8_t>(input);
    frame.pressed = Read<std::uint32_t>(input);
    frame.held = Read<std::uint32_t>(input);
    frame.repeat = Read<std::uint32_t>(input);
    frame.stick_x = Read<std::int32_t>(input);
    frame.stick_y = Read<std::int32_t>(input);
    world.cursor_return = Read<std::int32_t>(input);
    world.status_bits = Read<std::uint32_t>(input);
    ReadArray(input, world.upper_icons);
    ReadArray(input, world.hp);
    ReadArray(input, world.max_hp);
    ReadArray(input, world.names);
    ReadArray(input, world.bones);
    ReadArray(input, world.projections);
    ReadArray(input, world.palette);
    ReadArray(input, world.texture_offsets);
    ReadArray(input, world.row_offsets);
    ReadArray(input, value.equipment);
    ReadArray(input, value.affinities);
    ReadArray(input, value.types);
    ReadArray(input, value.output);
    ReadArray(input, value.context);
    value.alias = Read<std::size_t>(input);
    ReadArray(input, world.packets.heads);
    return value;
}

int Run(fixture::World& world, Vector& value, rd::EncodedTextArguments& arguments,
    std::span<const std::uint8_t> battle_text, std::span<const std::uint8_t> status_text)
{
    st::LimbControllerResult done;
    switch (value.mode) {
    case 0: {
        fixture::PacketChannel queue(world.packets, 0);
        done = st::DrawLimbConditionIndicator(queue, world.palette,
            { .condition = value.args[0],
                .position = value.args[1],
                .dimming = value.args[2] });
        break;
    }
    case 1: {
        fixture::PacketChannel queue(world.packets, 1);
        done = st::DrawLimbLeaderLine(queue, { .limb = value.args[0],
                                                 .position = value.args[1],
                                                 .opacity = value.args[3] });
        break;
    }
    case 2:
        done = st::DrawStatusEffectIcons(world,
            world.Flags(static_cast<std::uint8_t>(world.panel_frame.selected_actor - 1)),
            value.args[1], world.texture_offsets);
        break;
    case 3: {
        std::optional<st::StatusEffectTarget> target;
        if (value.args[0]) {
            target = st::StatusEffectTarget { .actor_slot =
                                                  world.panel_frame.selected_actor - 1,
                .flags = world.Flags(
                    static_cast<std::uint8_t>(world.panel_frame.selected_actor - 1)) };
        }
        const auto result = st::NavigateStatusEffects(
            world.navigation, world.navigation_frame, target, value.args[1] != 0, world);
        return result ? *result : -100 - static_cast<int>(result.error());
    }
    case 4: {
        const auto result =
            st::UpdateLimbPanel(world.panel, world.panel_frame, value.args[0], world);
        return result ? *result : -100 - static_cast<int>(result.error());
    }
    case 5: {
        const auto limb = world.navigation_frame.selected_element;
        const auto slot = static_cast<std::uint8_t>(world.panel_frame.selected_actor - 1);
        const auto condition = VS::Battle::LimbCondition(world.Health(slot, limb));
        done = st::ShowSelectedLimbCondition(limb,
            { .name = Indexed(battle_text, world.NameIndex(slot, limb)),
                .condition =
                    Indexed(status_text, 13 + static_cast<std::size_t>(condition)),
                .format = Text(status_text, 253 * 2) },
            arguments, value.output, world);
        break;
    }
    case 6: {
        const auto limb = world.navigation_frame.selected_element;
        const auto salt = static_cast<std::uint32_t>(world.panel_frame.selected_actor - 1)
                        * fixture::kActorStatSalt;
        for (auto& affinity : value.affinities) {
            affinity =
                static_cast<std::int16_t>(static_cast<std::uint32_t>(affinity) + salt);
        }
        for (auto& type : value.types) {
            type = static_cast<std::int16_t>(static_cast<std::uint32_t>(type) + salt);
        }
        std::span<const std::int16_t> affinities =
            std::span(value.affinities).subspan(limb * 8, 8);
        std::span<const std::int16_t> types = std::span(value.types).subspan(limb * 4, 4);
        if (value.alias) {
            affinities = std::span(value.equipment).subspan(value.alias, 8);
            types = std::span(value.equipment).subspan(value.alias - 4, 4);
        }
        done = st::PopulateSelectedLimbDefense(limb,
            { .affinities = affinities.first<8>(), .damage_types = types.first<4>() },
            value.equipment, world);
        break;
    }
    default:
        throw std::runtime_error("unknown operation");
    }
    return done ? 0 : -100 - static_cast<int>(done.error());
}

void Print(std::int64_t value) { std::cout << value << ' '; }
template <typename T> void PrintRange(const T& range)
{
    for (const auto value : range)
        Print(value);
}
void Output(int result, const fixture::World& world, const Vector& value,
    const rd::EncodedTextArguments& arguments, std::span<const std::uint8_t> battle_text,
    std::span<const std::uint8_t> status_text)
{
    Print(result);
    Print(world.navigation.cursor_animation);
    Print(world.navigation.selected);
    Print(world.navigation.count);
    Print(world.navigation.target_slot.value_or(-1));
    PrintRange(world.navigation.active);
    Print(world.panel.phase);
    Print(world.panel.actor_slot);
    PrintRange(world.panel.animation_steps);
    Print(world.navigation_frame.selected_element);
    Print(world.navigation_frame.yaw);
    Print(world.navigation_frame.pitch);
    Print(world.navigation_frame.cursor_color);
    Print(world.panel_frame.selection_and_dimming);
    Print(world.panel_frame.selected_actor);
    Print(world.panel_frame.fade_screen);
    Print(world.panel_frame.background_first);
    for (const auto field : value.equipment)
        Print(static_cast<std::uint16_t>(field));
    Print(arguments.strings[0].data() - battle_text.data());
    Print(arguments.strings[1].data() - status_text.data());
    PrintRange(value.output);
    Print(static_cast<std::int64_t>(world.packets.words.size()));
    PrintRange(world.packets.heads);
    PrintRange(world.packets.words);
    Print(static_cast<std::int64_t>(world.events.size()));
    for (const auto& event : world.events)
        PrintRange(event);
    std::cout << '\n';
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::filesystem::path menu_path(argv[1]);
    const Bytes menu = Load(menu_path);
    const Bytes battle =
        Load(menu_path.parent_path().parent_path() / "BATTLE" / "BATTLE.PRG");
    const auto status_text = std::span(menu).subspan(kStatusText - kMenuBase);
    const auto battle_text = std::span(battle).subspan(kBattleText - kBattleBase);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty())
            continue;
        fixture::World world;
        LoadLabels(world, menu, battle);
        std::istringstream input(line);
        Vector value = Input(input, world);
        rd::EncodedTextArguments arguments;
        arguments.strings[0] = Indexed(battle_text, value.context[0]);
        arguments.strings[1] = Indexed(status_text, 13 + value.context[1]);
        const int result = Run(world, value, arguments, battle_text, status_text);
        Output(result, world, value, arguments, battle_text, status_text);
    }
}
