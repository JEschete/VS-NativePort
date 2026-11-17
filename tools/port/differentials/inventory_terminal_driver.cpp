#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

#include "Menu/InventoryTerminalWorld.hpp"

namespace {
namespace Menu = VS::Menu::Inventory;
using VS::tests::inventory_terminal::World;

template <typename... Values> void Read(std::istream& input, Values&... values)
{
    const auto one = [&input](auto& value) {
        std::int64_t number = 0;
        if (!(input >> number))
            throw std::runtime_error("short input");
        value = static_cast<std::remove_reference_t<decltype(value)>>(number);
    };
    (one(values), ...);
}
template <typename Range> void ReadRange(std::istream& input, Range& values)
{
    for (auto& value : values)
        Read(input, value);
}
template <typename... Values> void Print(const Values&... values)
{
    ((std::cout << static_cast<std::int64_t>(values) << ' '), ...);
}
template <typename Range> void PrintRange(const Range& values)
{
    for (const auto value : values)
        Print(value);
}
void ReadRuntime(std::istream& input, Menu::TerminalRuntime& runtime)
{
    Read(input, runtime.cursor.category, runtime.cursor.reserved);
    for (auto& position : runtime.cursor.positions) {
        Read(input, position.row, position.page);
    }
    auto& p = runtime.presentation;
    Read(input, p.secondary_title, p.category_icons, p.status_command, p.secondary_step,
        p.icons_step, p.status_step, p.icon_on_top, p.ashley_layout);
    Read(input, runtime.control.current_menu, runtime.control.return_state,
        runtime.control.execute_ability_type, runtime.control.loading,
        runtime.use.command, runtime.use.kind, runtime.use.action, runtime.use.item,
        runtime.use.slot, runtime.cursor_memory_enabled, runtime.freeze_tabs,
        runtime.restrictions, runtime.hud_slide_offset);
}
void PrintRuntime(const Menu::TerminalRuntime& runtime)
{
    Print(runtime.cursor.category, runtime.cursor.reserved);
    for (const auto position : runtime.cursor.positions) {
        Print(position.row, position.page);
    }
    const auto& p = runtime.presentation;
    Print(p.secondary_title, p.category_icons, p.status_command, p.secondary_step,
        p.icons_step, p.status_step, p.icon_on_top, p.ashley_layout);
    Print(runtime.control.current_menu, runtime.control.return_state,
        runtime.control.execute_ability_type, runtime.control.loading,
        runtime.use.command, runtime.use.kind, runtime.use.action, runtime.use.item,
        runtime.use.slot, runtime.cursor_memory_enabled, runtime.freeze_tabs,
        runtime.restrictions, runtime.hud_slide_offset);
}
void ReadWorld(std::istream& input, World& world)
{
    ReadRuntime(input, world.runtime);
    Read(input, world.input.pressed, world.input.repeat, world.input.held);
    ReadRange(input, world.ready_values);
    Read(input, world.information_done, world.names_loaded, world.text_loaded,
        world.text_unloaded, world.tarnished, world.intro, world.action_result,
        world.selected_result, world.cursor_result, world.details_result,
        world.discard_result, world.sort_result, world.navigation_result,
        world.equipment_result, world.parent_weapon, world.parent_shield,
        world.find_result, world.populate_count, world.cursor_animation, world.fade_step,
        world.item_count, world.nested_navigation_changes_panels);
    ReadRange(input, world.animation);
    std::array<std::uint8_t, sizeof(VS::Items::InventoryContents)> inventory { };
    ReadRange(input, inventory);
    world.inventory = std::bit_cast<VS::Items::InventoryContents>(inventory);
    for (auto& order : world.order)
        ReadRange(input, order);
    ReadRange(input, world.misc_actions);
    ReadRange(input, world.action_map);
    ReadRange(input, world.unlocked);
    ReadRange(input, world.action_flags);
    ReadRange(input, world.formatted_flags);
    for (auto& row : world.rows) {
        Read(input, row.state, row.selected, row.unselectable, row.outset_icon, row.x,
            row.y, row.target0, row.max, row.count);
    }
}
void PrintWorld(const World& world)
{
    PrintRuntime(world.runtime);
    PrintRange(
        std::bit_cast<std::array<std::uint8_t, sizeof(VS::Items::InventoryContents)>>(
            world.inventory));
    for (const auto& order : world.order)
        PrintRange(order);
    PrintRange(world.misc_actions);
    for (const auto& row : world.rows) {
        Print(row.state, row.selected, row.unselectable, row.outset_icon, row.x, row.y,
            row.target0, row.max, row.count);
    }
    Print(world.rows_active, world.calls.size());
    PrintRange(world.calls);
}
void Run(std::istream& input)
{
    int kind = 0;
    Menu::ItemRowsContext context { };
    int initialize = 0;
    std::uint8_t menu_step = 0;
    Menu::InventorySortState sort;
    Menu::InventoryDiscardState discard;
    Menu::InventoryDiscardPresentation presentation;
    Menu::InventoryNavigationState navigation;
    Menu::MiscShortcutState shortcut;
    Menu::InventoryScreenState screen;
    Read(input, kind, context, initialize, menu_step);
    Read(input, sort.step, sort.category);
    Read(input, discard.step, discard.item.category, discard.item.one_based_slot,
        discard.item.kind, discard.place, discard.quantity, discard.maximum);
    ReadRange(input, presentation.text);
    Read(input, presentation.cursor_animation);
    ReadRange(input, navigation.actions);
    Read(input, navigation.step, navigation.no_items, navigation.ignore_cursor_memory,
        navigation.selected_row);
    ReadRange(input, shortcut.actions);
    Read(input, shortcut.selected_row, shortcut.step, shortcut.no_items,
        shortcut.no_force_cursor_memory);
    Read(input, screen.resume_without_cursor_reset, screen.quit_countdown);
    World world;
    ReadWorld(input, world);
    int result = 0;
    switch (kind) {
    case 0:
        result = Menu::UpdateSortMenu(sort, world,
            initialize == 0 ? std::optional<Menu::TerminalCategory> { }
                            : static_cast<Menu::TerminalCategory>(initialize - 1));
        break;
    case 1: {
        std::optional<Menu::InventoryDiscardEntry> entry;
        if (initialize != 0) {
            entry = Menu::InventoryDiscardEntry { static_cast<Menu::TerminalCategory>(
                                                      (initialize >> 8) & 255),
                static_cast<std::uint8_t>(initialize),
                static_cast<std::uint8_t>(initialize >> 16) };
        }
        result = Menu::UpdateDiscardMenu(discard, presentation, world, entry);
        break;
    }
    case 2:
        result = Menu::UpdateItemList(navigation, world, context, initialize);
        break;
    case 3:
        Menu::UpdateMiscItemShortcut(shortcut, world, initialize);
        break;
    case 4:
        result = Menu::UpdateInventoryScreen(screen, menu_step, world);
        break;
    default:
        throw std::runtime_error("unknown mode");
    }
    Print(result, menu_step, sort.step, sort.category);
    Print(discard.step, discard.item.category, discard.item.one_based_slot,
        discard.item.kind, discard.place, discard.quantity, discard.maximum);
    PrintRange(presentation.text);
    Print(presentation.cursor_animation);
    PrintRange(navigation.actions);
    Print(navigation.step, navigation.no_items, navigation.ignore_cursor_memory,
        navigation.selected_row);
    PrintRange(shortcut.actions);
    Print(shortcut.selected_row, shortcut.step, shortcut.no_items,
        shortcut.no_force_cursor_memory);
    Print(screen.resume_without_cursor_reset, screen.quit_countdown);
    PrintWorld(world);
    std::cout << '\n';
}
} // namespace

int main()
{
    std::string line;
    std::size_t number = 0;
    try {
        while (std::getline(std::cin, line)) {
            std::istringstream input(line);
            Run(input);
            ++number;
        }
    } catch (const std::exception& error) {
        std::cerr << "vector " << number << ": " << error.what() << '\n';
        return 1;
    }
}
