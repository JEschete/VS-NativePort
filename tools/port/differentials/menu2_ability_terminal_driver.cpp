#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

#include "Menu/AbilityTerminalWorld.hpp"

namespace {
namespace a = VS::Menu::BattleAbilities;
namespace fixture = VS::Test::ability_terminal;
using Words = std::vector<std::int64_t>;
using Input = std::span<const std::int64_t>;
constexpr std::uint32_t kAbilityStrings = 0x80104690;
constexpr std::uint32_t kActions = 0x8004B9DC;
constexpr std::uint32_t kRowText = 0x80220000;

template <typename Range> void Append(Words& output, const Range& range)
{
    for (const auto value : range)
        output.push_back(value);
}

void AppendCalls(Words& output, const std::vector<Words>& calls)
{
    output.push_back(calls.size());
    for (const auto& call : calls) {
        output.push_back(call.size());
        Append(output, call);
    }
}

void CheckInput(Input input, std::size_t count)
{
    if (input.size() != count)
        throw std::runtime_error("wrong terminal vector field count");
}

void CheckRefusals(const std::vector<std::string>& refusals)
{
    if (!refusals.empty())
        throw std::runtime_error("native terminal refused: " + refusals.front());
}

struct Image {
    std::array<std::uint16_t, 1060> text { };
    explicit Image(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("cannot open canonical MENU2 image");
        const std::vector<std::uint8_t> bytes { std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>() };
        if (bytes.size() != 0x2920)
            throw std::runtime_error("wrong MENU2 image size");
        for (std::size_t i = 0; i < text.size(); ++i) {
            text[i] = static_cast<std::uint16_t>(
                bytes.at(0x1E90 + i * 2) | bytes.at(0x1E91 + i * 2) << 8);
        }
    }
};

struct AssignmentWorld : fixture::AssignmentWorld {
    void PlaySound(a::AssignmentSound sound) override
    {
        if (sound == a::AssignmentSound::kSet) {
            calls.push_back({ 2, 126, 35 });
        } else {
            calls.push_back({ 1 + static_cast<int>(sound) });
        }
        if (on_sound)
            on_sound(sound);
    }
    void ReleaseAbilityText() override
    {
        calls.push_back({ 9, kRowText });
        has_text = false;
    }
};

void SeedAssignment(AssignmentWorld& world, std::uint32_t seed)
{
    for (std::size_t i = 0; i < 16; ++i) {
        world.chain[i] = static_cast<std::uint8_t>(seed + i * 17);
        world.defense[i] = static_cast<std::uint8_t>(seed + 61 + i * 13);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        world.chain_mapping[i] = static_cast<std::uint8_t>(seed + (12 + i) * 13);
        world.defense_mapping[i] = static_cast<std::uint8_t>(seed + (16 + i) * 13);
    }
}

Words RunAssignment(Input input)
{
    CheckInput(input, 6);
    AssignmentWorld world;
    const auto type = static_cast<a::AbilityMenuType>(input[0]);
    const auto button = static_cast<a::AssignmentButton>(input[1]);
    const auto index = static_cast<int>(input[2]);
    const auto seed = static_cast<std::uint32_t>(input[3]);
    const auto mode = static_cast<int>(input[4]);
    SeedAssignment(world, seed);
    const auto action = type == a::AbilityMenuType::kChain ? world.chain.at(index)
                                                           : world.defense.at(index);
    auto mapping = world.Mappings(type);
    for (std::size_t slot = 0; slot < 3; ++slot) {
        mapping[slot] =
            static_cast<std::uint8_t>((mode & 1) != 0 ? action : action + slot + 1);
    }
    if ((mode & 2) != 0) {
        mapping[static_cast<std::size_t>(button)] =
            static_cast<std::uint8_t>(action + 73);
    }
    world.on_sound = [&](a::AssignmentSound sound) {
        if (input[5] != 0 && sound == a::AssignmentSound::kSet)
            mapping[2] = action;
    };
    a::ToggleButtonAssignment(world, type, button, index);
    CheckRefusals(world.refusals);
    Words output;
    Append(output, world.chain_mapping);
    Append(output, world.defense_mapping);
    AppendCalls(output, world.calls);
    return output;
}

void InitializeItems(AssignmentWorld& world, std::uint32_t seed)
{
    for (std::size_t row = 0; row < world.items.size(); ++row) {
        world.items[row] = { .state = static_cast<std::uint8_t>(seed + row),
            .gradient = static_cast<std::uint8_t>(seed + row * 19),
            .scroll = static_cast<std::uint8_t>(row % 3),
            .selected = static_cast<std::uint8_t>(row % 2),
            .target = 700 + static_cast<int>(row),
            .x = 100 + static_cast<int>(row),
            .y = 200 + static_cast<int>(row),
            .width = 90 + static_cast<int>(row),
            .text_offset = 500 + static_cast<int>(row) };
    }
    for (std::size_t i = 0; i < world.memories.size(); ++i) {
        world.memories[i] = static_cast<std::uint8_t>(seed + i * 7);
    }
}

a::AssignmentInput Buttons(std::uint32_t pressed, std::uint32_t repeat)
{
    return { .circle = (pressed & 0x20) != 0,
        .triangle = (pressed & 0x10) != 0,
        .square = (pressed & 0x80) != 0,
        .cancel = (pressed & 0x40) != 0,
        .repeat_up = (repeat & 0x1000) != 0,
        .repeat_down = (repeat & 0x4000) != 0,
        .pressed_up = (pressed & 0x1000) != 0,
        .pressed_down = (pressed & 0x4000) != 0 };
}

Words RunAssignments(Input input)
{
    CheckInput(input, 11);
    AssignmentWorld world;
    a::AbilityRowsState rows { .count = static_cast<int>(input[0]),
        .menu_state = static_cast<std::uint32_t>(input[4]),
        .type = static_cast<a::AbilityMenuType>(input[1]),
        .cursor = { .highlighted = static_cast<int>(input[3]),
            .first = static_cast<int>(input[2]) } };
    a::AssignmentState state { .cursor_animation = std::bit_cast<int>(
                                   static_cast<std::uint32_t>(input[8])) };
    const auto seed = static_cast<std::uint32_t>(input[9]);
    SeedAssignment(world, seed);
    InitializeItems(world, seed);
    world.ready = input[7] != 0;
    world.input = Buttons(
        static_cast<std::uint32_t>(input[5]), static_cast<std::uint32_t>(input[6]));
    world.cursor_result = seed ^ 0x81234567u;
    world.display_cost = static_cast<std::uint8_t>(seed);
    for (const auto type : { a::AbilityMenuType::kChain, a::AbilityMenuType::kDefense }) {
        auto mapping = world.Mappings(type);
        auto visible = type == a::AbilityMenuType::kChain
                         ? std::array<int, 3> { 0, 3, 7 }
                         : std::array<int, 3> { 1, 4, 6 };
        for (auto& row : visible)
            row = std::min(row, rows.count - 1);
        for (std::size_t button = 0; button < visible.size(); ++button) {
            const auto action =
                world.LearnedAction(type, visible[button] + rows.cursor.first);
            if (!action)
                throw std::runtime_error("invalid fixture learned-action index");
            mapping[button] = *action;
        }
    }
    world.on_sound = [&](a::AssignmentSound sound) {
        if (input[10] != 0 && sound == a::AssignmentSound::kSet)
            world.input.cancel = true;
    };
    const int result = a::UpdateAssignments(rows, state, world);
    CheckRefusals(world.refusals);
    Words output { result, rows.menu_state, rows.cursor.highlighted, rows.cursor.first,
        static_cast<std::uint32_t>(state.cursor_animation), world.has_text ? 1 : 0,
        world.display_cost };
    Append(output, world.chain_mapping);
    Append(output, world.defense_mapping);
    Append(output, world.memories);
    for (const auto& item : world.items) {
        Append(output, std::array<std::int64_t, 9> { item.state, item.gradient,
                           item.scroll, item.selected, item.target, item.x, item.y,
                           item.width, item.text_offset });
    }
    AppendCalls(output, world.calls);
    return output;
}

struct ListWorld : fixture::ListWorld {
    const Image& image;
    explicit ListWorld(const Image& data)
        : image(data)
    {
    }
    void CreateRows(a::AbilityKind kind, std::span<const a::LearnedAbility> rows) override
    {
        Words call { 21, static_cast<std::int64_t>(rows.size()),
            kind == a::AbilityKind::kChain ? 24 : 25 };
        for (const auto row : rows) {
            call.push_back(kActions + row.action * 52 + 28);
            const auto slot =
                (kind == a::AbilityKind::kChain ? 6 : 20) + row.description_slot;
            call.push_back(kAbilityStrings + image.text.at(slot) * 2);
        }
        calls.push_back(std::move(call));
        if (on_create)
            on_create();
    }
};

Words RunList(Input input, const Image& image, a::AbilityKind kind)
{
    CheckInput(input, 40);
    ListWorld world(image);
    a::AbilityListState state { .phase = static_cast<a::AbilityListPhase>(input[1]) };
    const auto seed = static_cast<std::uint32_t>(input[6]);
    for (std::size_t i = 0; i < 16; ++i) {
        world.chain[i] = static_cast<std::uint8_t>(input[8 + i]);
        world.defense[i] = static_cast<std::uint8_t>(input[24 + i]);
        world.learned_chain[i] = static_cast<std::uint8_t>(seed + i * 17);
        world.learned_defense[i] = static_cast<std::uint8_t>(seed + 61 + i * 13);
    }
    const auto& order = kind == a::AbilityKind::kChain ? world.chain : world.defense;
    for (std::size_t i = 0; i < 14; ++i) {
        world.unlocked.at(order[i]) = (input[4] & (std::int64_t { 1 } << i)) != 0;
    }
    world.shortcut = input[2] != 0;
    world.ready = input[3] != 0;
    world.assignment_result = static_cast<int>(input[5]);
    world.on_create = [&] { state.phase = static_cast<a::AbilityListPhase>(81); };
    const int result = kind == a::AbilityKind::kChain
                         ? a::UpdateChainAbilityMenu(state, world, input[0] != 0)
                         : a::UpdateDefenseAbilityMenu(state, world, input[0] != 0);
    CheckRefusals(world.refusals);
    Words output { result, static_cast<std::uint32_t>(state.phase) };
    Append(output, world.learned_chain);
    Append(output, world.learned_defense);
    AppendCalls(output, world.calls);
    return output;
}

Words RunPoints(Input input)
{
    CheckInput(input, 4);
    fixture::PointsWorld world;
    world.count = static_cast<int>(input[0]);
    world.kills = static_cast<std::uint16_t>(input[1]);
    if (world.count < 22) {
        world.requirements.at(world.count) = static_cast<std::uint16_t>(input[2]);
    }
    a::DrawNextUnlockPoints(static_cast<int>(input[3]), world);
    CheckRefusals(world.refusals);
    std::vector<Words> calls;
    for (std::size_t i = 0; i < world.texts.size(); ++i) {
        Words call { 40, world.positions[i], 0,
            static_cast<std::int64_t>(world.texts[i].size()) };
        Append(call, world.texts[i]);
        calls.push_back(std::move(call));
    }
    Words output;
    AppendCalls(output, calls);
    return output;
}

struct ScreenWorld : fixture::ScreenWorld {
    const Image& image;
    std::uint32_t seed;
    std::uint8_t cursor = 0;
    int header_selected = 0;
    std::uint32_t nested_state = 0;
    ScreenWorld(const Image& data, std::uint32_t value)
        : image(data)
        , seed(value)
    {
    }
    bool ShowShortcutCategory(const a::AbilityShortcutRow& row) override
    {
        calls.push_back({ 4, row.row, row.x, row.y, row.width, row.background_width,
            kAbilityStrings
                + image.text.at(row.kind == a::AbilityKind::kChain ? 0 : 3) * 2 });
        header_selected = 1;
        return true;
    }
    void ShowCategories(bool chain, bool defense, bool force_cursor) override
    {
        const auto saved = cursor;
        if (force_cursor)
            cursor = 1;
        calls.push_back(
            { 32, 2, 0x117, cursor, kAbilityStrings + 34 * 2 + (chain ? 0 : 1),
                kAbilityStrings + (chain ? 42 : 87) * 2, kAbilityStrings + 111 * 2,
                kAbilityStrings + (defense ? 120 : 166) * 2, 0, defense ? 0 : 1 });
        cursor = static_cast<std::uint8_t>(seed + 99);
        current_menu = static_cast<std::uint8_t>(seed + 77);
        cursor = saved;
    }
};

Words RunScreen(Input input, const Image& image)
{
    CheckInput(input, 12);
    const auto seed = static_cast<std::uint32_t>(input[10]);
    ScreenWorld world(image, seed);
    auto phase = static_cast<std::uint8_t>(input[0]);
    a::AbilityScreenAnimation animation { input[1] != 0,
        static_cast<std::uint8_t>(input[2]) };
    world.shortcut = static_cast<int>(input[3]);
    world.readiness = { (input[4] & 1) != 0, (input[4] & 2) != 0 };
    world.selected = static_cast<int>(input[5]);
    world.list_result = static_cast<int>(input[6]);
    world.chain_available = input[7] != 0;
    world.defense_available = input[8] != 0;
    world.cursor = static_cast<std::uint8_t>(input[9]);
    world.current_menu = static_cast<std::uint8_t>(seed);
    world.header_selected = seed & 1;
    world.nested_state = seed ^ 0x12345678u;
    for (std::size_t i = 0; i < world.offsets.size(); ++i) {
        world.offsets[i] = static_cast<std::uint8_t>(seed + i * 13);
    }
    world.on_list = [&](a::AbilityKind, bool) {
        if (input[11] != 0) {
            world.current_menu = static_cast<std::uint8_t>(seed + 19);
            world.nested_state = seed ^ 0xABCDEF01u;
        }
        if ((input[11] & 2) != 0) {
            const std::array<int, 3> shortcuts { 0, 7, 8 };
            world.shortcut = shortcuts[seed % 3];
        }
    };
    world.on_points = [&] {
        if (input[11] == 3)
            animation.points_step = static_cast<std::uint8_t>(seed % 11);
    };
    const int result = a::UpdateAbilityScreen(phase, animation, world);
    CheckRefusals(world.refusals);
    Words output { result, phase, animation.menu_animating ? 1 : 0, animation.points_step,
        world.current_menu, world.cursor, world.shortcut, world.header_selected,
        world.nested_state };
    AppendCalls(output, world.calls);
    return output;
}

Words Run(Input input, const Image& image)
{
    if (input.empty())
        throw std::runtime_error("empty terminal vector");
    switch (input.front()) {
    case 0:
        return RunAssignment(input.subspan(1));
    case 1:
        return RunAssignments(input.subspan(1));
    case 2:
        return RunList(input.subspan(1), image, a::AbilityKind::kChain);
    case 3:
        return RunList(input.subspan(1), image, a::AbilityKind::kDefense);
    case 4:
        return RunPoints(input.subspan(1));
    case 5:
        return RunScreen(input.subspan(1), image);
    default:
        throw std::runtime_error("unknown terminal vector mode");
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    try {
        const Image image(argv[1]);
        for (std::string line; std::getline(std::cin, line);) {
            std::istringstream input(line);
            Words values;
            for (std::int64_t value = 0; input >> value;)
                values.push_back(value);
            const auto output = Run(values, image);
            for (const auto value : output)
                std::cout << value << ' ';
            std::cout << '\n';
        }
    } catch (const std::runtime_error& error) {
        std::cerr << "terminal fixture failure: " << error.what() << '\n';
        return 3;
    } catch (const std::out_of_range& error) {
        std::cerr << "terminal fixture bounds failure: " << error.what() << '\n';
        return 4;
    }
}
