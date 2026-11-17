#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <type_traits>

#include "Menu/MagicMenusTestWorld.hpp"
#include "Menu/MagicTerminal.hpp"

namespace {
namespace magic = VS::Menu::Magic;
using magic::test::World;
using Entry = std::optional<int> (*)(
    unsigned int, magic::SpellMenuState&, magic::MenuHost&);
constexpr std::array<Entry, 4> kEntries { magic::UpdateWarlockMenu,
    magic::UpdateShamanMenu, magic::UpdateSorcererMenu, magic::UpdateEnchanterMenu };

template <typename T> void Read(T& value)
{
    std::int64_t input = 0;
    std::cin >> input;
    value = static_cast<T>(input);
}
template <typename T> void Block(T& block, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
        Read(block[i]);
}
template <typename T> void Print(T value)
{
    std::cout << static_cast<std::int64_t>(value) << ' ';
}
void PrintPackets(const magic::test::Packets& queue)
{
    Print(queue.records.size());
    for (auto packet = queue.records.rbegin(); packet != queue.records.rend(); ++packet) {
        Print(packet->size() - 1);
        for (std::size_t i = 1; i < packet->size(); ++i)
            Print((*packet)[i]);
    }
}
void PrintCalls(const World& world)
{
    Print(world.calls.size());
    for (const auto& call : world.calls) {
        Print(call.size());
        for (auto word : call)
            Print(word);
    }
}
std::size_t PacketWords(const magic::test::Packets& queue)
{
    std::size_t words = 0;
    for (const auto& packet : queue.records)
        words += packet.size();
    return words;
}

void Run(unsigned school)
{
    World world;
    magic::SpellMenuState state;
    unsigned init = 0;
    unsigned pressed = 0;
    unsigned repeat = 0;
    std::uint8_t terminal_state = 0;
    std::uint8_t title_selected = 0;
    int hook = 0;
    std::uint8_t hook_state = 0;
    std::uint8_t hook_current = 0;
    std::int32_t hook_action = 0;
    Read(init);
    Read(state.step);
    Read(state.selection);
    Read(state.remember_cursor);
    Read(state.blink);
    Read(world.shortcut);
    Read(world.idle);
    Read(world.cursor_setting);
    Read(world.cursor.selected);
    Read(world.cursor.count);
    Read(world.cursor.scroll);
    Read(world.selected_row);
    Read(world.highlighted_row);
    Read(pressed);
    Read(repeat);
    Read(world.display_cost);
    Read(terminal_state);
    Read(world.teleport_availability);
    Read(world.child_result);
    Read(world.teleport_result);
    Read(world.action);
    Read(world.selection_kind);
    Read(world.ability_type);
    Read(world.current_menu);
    Read(world.return_menu);
    Read(hook);
    Read(hook_state);
    Read(hook_current);
    Read(hook_action);
    Read(world.action_restrictions);
    for (auto& spell : state.available.spells)
        Read(spell.action_id);
    Block(world.ids, 18);
    Block(world.unlocked, 256);
    Block(world.restrictions, 256);
    Block(world.strings, 128);
    Block(world.schools, 4);
    Block(world.save_points, 48);
    Block(world.at_save_point, 3);
    Block(world.widths, 28);
    Block(world.trims, 172);
    Block(world.glyphs, 60);
    Block(world.brightness, 16);
    world.input = { .quit = (pressed & 0x10) != 0,
        .back = (pressed & 0x40) != 0,
        .confirm = (pressed & 0x20) != 0,
        .right = (repeat & 0x2000) != 0,
        .left = (repeat & 0x8000) != 0 };
    world.caller_state = &terminal_state;
    world.callback = [&](int code) {
        if (code == 15)
            title_selected = 1;
        if (code == hook) {
            terminal_state = hook_state;
            world.current_menu = hook_current;
            world.action = hook_action;
        }
    };
    if (school < 4)
        state.available.count = magic::kSpellCounts[school];
    const auto result = school == 4 ? magic::Update(terminal_state, world)
                                    : kEntries[school](init, state, world);
    Print(world.refusals.size());
    Print(result ? *result : 0);
    if (school < 4) {
        Print(state.step);
        Print(state.selection);
        Print(state.remember_cursor);
        Print(state.blink);
        Print(world.cursor.selected);
        Print(world.cursor.count);
        Print(world.cursor.scroll);
        Print(world.cursor_setting);
        Print(world.display_cost);
        for (std::size_t i = 0; i < magic::kSpellCounts[school]; ++i)
            Print(state.available.spells[i].action_id);
        PrintPackets(world.text);
        PrintPackets(world.icons);
        Print(PacketWords(world.text) + PacketWords(world.icons));
    } else {
        Print(terminal_state);
        Print(world.action);
        Print(world.selection_kind);
        Print(world.ability_type);
        Print(world.current_menu);
        Print(world.return_menu);
        Print(world.cursor_setting);
        Print(title_selected);
    }
    PrintCalls(world);
    std::cout << '\n';
}
} // namespace

int main(int argc, char**)
{
    if (argc != 2)
        return 2;
    unsigned school = 0;
    while (std::cin >> school) {
        if (school > 4)
            return 3;
        Run(school);
    }
}
