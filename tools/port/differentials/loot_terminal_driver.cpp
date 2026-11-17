#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <type_traits>

#include "Menu/LootTerminalWorld.hpp"

namespace {
namespace Test = VS::tests::loot_terminal;
namespace Loot = VS::Menu::Loot;
using Category = VS::Items::ItemCategory;
constexpr std::size_t kOriginalRows = 160;
constexpr std::size_t kInventoryWords = 0xF00 / sizeof(std::uint32_t);
constexpr std::size_t kMenuRowWords = 60 / sizeof(std::uint32_t);
using InventoryWords = std::array<std::uint32_t, kInventoryWords>;
using MenuRowWords = std::array<std::uint32_t, kMenuRowWords>;

struct Controllers {
    Loot::InventoryTransitionState transition;
    Loot::DisassemblyState disassembly;
    Loot::StackConsolidationState consolidation;
    Loot::LootSelectionState selection;
    std::uint8_t screen = 0;
};

struct Reader {
    template <typename... Values> void operator()(Values&... values) const
    {
        (Read(values), ...);
    }
    template <typename Value> static void Read(Value& target)
    {
        std::int64_t value = 0;
        if (!(std::cin >> value))
            throw std::runtime_error("incomplete loot vector");
        target = static_cast<Value>(value);
    }
    template <typename Values> void Range(Values& values) const
    {
        for (auto& value : values)
            Read(value);
    }
};
struct Writer {
    template <typename... Values> void operator()(Values... values) const
    {
        ((std::cout << static_cast<std::int64_t>(values) << ' '), ...);
    }
    template <typename Values> void Range(const Values& values) const
    {
        for (auto value : values)
            (*this)(value);
    }
};
template <typename Io> void TransferControllers(Io io, Controllers& state)
{
    auto& dis = state.disassembly;
    auto& con = state.consolidation;
    io(state.transition.step, state.transition.hud_was_hidden, dis.step, dis.choice,
        dis.saved_count, dis.target_row, dis.category, dis.cursor_animation,
        con.remaining, con.stack_room, con.item_id, con.step, con.choice, con.saved_count,
        con.cursor_animation, state.selection.step, state.selection.removal_delay,
        state.selection.cursor_animation, state.screen);
}
template <typename Io>
void TransferPresentation(Io io, VS::Menu::Inventory::TerminalPresentation& ui)
{
    io(ui.secondary_title, ui.category_icons, ui.status_command, ui.secondary_step,
        ui.icons_step, ui.status_step, ui.icon_on_top, ui.ashley_layout);
}
template <typename Io> void TransferRuntime(Io io, Loot::LootTerminalRuntime& runtime)
{
    io(runtime.cursor.count, runtime.cursor.offset, runtime.cursor.selected);
    TransferPresentation(io, runtime.inventory_ui);
    io(runtime.inventory_category, runtime.show_inventory, runtime.inventory_step,
        runtime.show_loot, runtime.loot_step, runtime.controls_enabled, runtime.previewed,
        runtime.claimed, runtime.information_shown, runtime.hud_hidden,
        runtime.hud_slide_offset, runtime.result_kind);
}
void ReadServices(Test::World& world)
{
    Reader io;
    io.Range(world.ready_values);
    io(world.cursor_result, world.inventory_result, world.leave_result,
        world.organize_result, world.transition_result, world.disassembly_result,
        world.consolidation_result, world.selection_result, world.staged_count,
        world.selection_preview, world.information_done, world.text_loaded,
        world.text_unloaded, world.dismiss_pending, world.have_room_loot,
        world.active_workspace, world.change_inventory_ui, world.nested_category);
    TransferPresentation(io, world.nested_ui);
    bool refresh = false;
    Loot::LootCursor cursor;
    io(refresh, cursor.count, cursor.offset, cursor.selected);
    if (refresh)
        world.refreshed_cursor = cursor;
    io.Range(world.animation);
    io.Range(world.counts);
    io.Range(world.claim_results);
}
void ReadInventory(VS::Items::InventoryContents& inventory)
{
    InventoryWords words;
    Reader { }.Range(words);
    static_assert(sizeof(words) == sizeof(inventory));
    inventory = std::bit_cast<VS::Items::InventoryContents>(words);
}
void WriteInventory(const VS::Items::InventoryContents& inventory)
{
    Writer { }.Range(std::bit_cast<InventoryWords>(inventory));
}
void ReadRows(std::span<Loot::LootRow> rows)
{
    for (auto& row : rows) {
        std::uint32_t value = 0;
        Reader { }(value);
        row = std::bit_cast<Loot::LootRow>(value);
    }
}
void WriteRows(std::span<const Loot::LootRow> rows)
{
    for (const auto& row : rows.first(kOriginalRows)) {
        Writer { }(std::bit_cast<std::uint32_t>(row));
    }
}
void ReadWorld(Test::World& world, Controllers& state)
{
    Reader io;
    TransferControllers(io, state);
    TransferRuntime(io, world.runtime);
    io(world.input.pressed, world.input.repeat, world.name_token);
    ReadServices(world);
    ReadInventory(world.workspace.inventory);
    ReadInventory(world.workspace.inventory_backup);
    ReadInventory(world.ashley);
    ReadRows(world.workspace.rows);
    ReadRows(world.workspace.working_rows);
    for (auto& row : world.menu_rows) {
        MenuRowWords words;
        io.Range(words);
        const auto bytes = std::as_bytes(std::span(words));
        std::ranges::copy(bytes, std::as_writable_bytes(std::span(&row, 1)).begin());
    }
}
void WriteWorld(Test::World& world, Controllers& state, std::int32_t result)
{
    Writer io;
    io(result);
    TransferControllers(io, state);
    TransferRuntime(io, world.runtime);
    io(world.name_token, world.active_workspace);
    WriteInventory(world.workspace.inventory);
    WriteInventory(world.workspace.inventory_backup);
    WriteRows(world.workspace.rows);
    WriteRows(world.workspace.working_rows);
    for (const auto& row : world.menu_rows) {
        MenuRowWords words { };
        std::ranges::copy(std::as_bytes(std::span(&row, 1)).first(sizeof(words)),
            std::as_writable_bytes(std::span(words)).begin());
        io.Range(words);
    }
    io(world.calls.size());
    io.Range(world.calls);
    std::cout << '\n';
}
std::int32_t Execute(int kind, int initialize, Controllers& state, Test::World& world)
{
    const auto row =
        initialize != 0 ? std::optional<std::size_t>(initialize - 1) : std::nullopt;
    switch (kind) {
    case 0:
        return Loot::UpdateInventoryTransition(state.transition, world,
            initialize != 0 ? std::optional(static_cast<Category>(initialize - 1))
                            : std::nullopt);
    case 1:
        return Loot::UpdateDisassembly(state.disassembly, world, row);
    case 2:
        return Loot::UpdateStackConsolidation(state.consolidation, world, row);
    case 3:
        return Loot::UpdateLootSelection(state.selection, world, initialize != 0);
    case 4:
        return Loot::UpdateLootScreen(state.screen, world);
    default:
        throw std::runtime_error("unknown loot terminal vector");
    }
}
} // namespace

int main()
{
    try {
        int kind = 0;
        while (std::cin >> kind) {
            int initialize = 0;
            Reader { }(initialize);
            Test::World world;
            Controllers state;
            ReadWorld(world, state);
            const auto result = Execute(kind, initialize, state, world);
            WriteWorld(world, state, result);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
