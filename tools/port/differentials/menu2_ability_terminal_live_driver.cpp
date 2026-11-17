#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/BattleAbilitiesTerminalLive.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/TextBoxStateLive.hpp"

extern "C" {
#include "State/Progress.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Battle/TargetingState.h"
#include "Input/ControllerLiveState.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/TextBox.h"
#include "Render/Render.h"

State_ArtsStatus Audio_Seq_unusedArtsStatus { };
Battle_Actor* Battle_characterState = nullptr;
u_short Battle_breakArtPointRequirements[15][4] { };
u_short Battle_battleAbilityPointRequirements[150] { };
Battle_ActionDefinition Battle_actionDefinitions[256] { };
Menu_StringContext Battle_stringContext { };
Input_PressedButtonsView Input_buttonsPressed { };
int Input_buttonRepeat = 0;
State_Settings Input_settings { };
Menu_TextBoxState Menu_textBoxes[8] { };
Menu_Framework_MenuMemory Menu_Framework_menuMemory { };
Menu_Framework_MenuControlState Menu_Framework_menuState { };
char Menu_Framework_unlockedBreakArt = 0;
char Menu_Framework_unlockedBattleAbility = 0;
char Menu_Framework_shortcutInvoked = 0;
char Menu_Framework_displayActionCost = 0;
char Menu_Framework_rowAnimationSteps[36] { };
char Battle_chainAbilityActionIds[16] { };
char Battle_defenseAbilityActionIds[16] { };
char Menu_BattleAbilities_abilityCount = 0;
u_int Menu_BattleAbilities_abilityMenuState = 0;
u_short* Menu_BattleAbilities_abilityText = nullptr;
u_short Menu_BattleAbilities_battleAbilityMenuStrings[292] { };
u_short Menu_BattleAbilities_battleAbilityStrings[1060] { };
char Menu_BattleAbilities_firstAbility = 0;
int Menu_BattleAbilities_highlightedAbility = 0;
char Menu_BattleAbilities_selectedAbilityType = 0;
char Menu_BattleAbilities_unlockedChainAbilities[16] { };
char Menu_BattleAbilities_unlockedDefenseAbilities[16] { };
}

namespace {
namespace a = VS::Menu::BattleAbilities;
namespace framework = VS::Menu::Framework;
using Words = std::vector<std::int64_t>;
std::array<std::uint16_t, 16 * 64> g_text { };
std::array<framework::MenuRow, 40> g_items { };
std::array<int, 40> g_text_offsets { };
std::array<std::byte, 64> g_ordering { };
Render_FrameScratch g_scratch { };
std::vector<Words> g_calls;
std::vector<std::uint8_t> g_description;
std::vector<std::uint32_t> g_point_positions;
std::size_t g_owned_bytes = 0;
bool g_owned = false;
bool g_ready = true;
bool g_nested_cancel = false;
std::uint32_t g_cursor_result = 0;
std::uint32_t g_last_cursor = 0;
int g_selected = -1;
int g_frees = 0;
bool g_self_check = false;
int g_refusals = 0;
std::array<std::intptr_t, 6> g_categories { };
int g_category_cursor = 0;

char Byte(std::int64_t value)
{
    return std::bit_cast<char>(static_cast<std::uint8_t>(value));
}
void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void Unexpected(const char* function)
{
    throw std::runtime_error(std::string("unexpected terminal dependency: ") + function);
}
template <typename Range> void Append(Words& output, const Range& range)
{
    for (const auto value : range)
        output.push_back(value);
}
template <std::size_t Size> void AppendBytes(Words& output, const char (&bytes)[Size])
{
    for (const char byte : bytes)
        output.push_back(static_cast<std::uint8_t>(byte));
}
void LoadTables(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    Require(file.good(), "cannot open live fixture MENU2 image");
    const std::vector<std::uint8_t> bytes { std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>() };
    Require(bytes.size() == 0x2920, "live fixture MENU2 size differs");
    for (std::size_t i = 0; i < std::size(Menu_BattleAbilities_battleAbilityStrings);
        ++i) {
        Menu_BattleAbilities_battleAbilityStrings[i] = static_cast<std::uint16_t>(
            bytes.at(0x1E90 + i * 2) | bytes.at(0x1E91 + i * 2) << 8);
    }
}
void InitializeCommon()
{
    g_scratch.overlayOrderingTable = g_ordering.data();
    for (std::size_t action = 0; action < std::size(Battle_actionDefinitions); ++action) {
        Battle_actionDefinitions[action] = { };
        Battle_actionDefinitions[action].cost = static_cast<std::uint8_t>(action);
    }
}
void PrepareRows(const Words& input)
{
    const auto seed = static_cast<std::uint32_t>(input.at(10));
    Menu_BattleAbilities_abilityCount = Byte(input.at(1));
    Menu_BattleAbilities_selectedAbilityType = Byte(input.at(2));
    Menu_BattleAbilities_firstAbility = Byte(input.at(3));
    Menu_BattleAbilities_highlightedAbility = static_cast<int>(input.at(4));
    Menu_BattleAbilities_abilityMenuState = static_cast<std::uint32_t>(input.at(5));
    Input_buttonsPressed.allButtons = static_cast<std::uint32_t>(input.at(6));
    Input_buttonRepeat = static_cast<int>(input.at(7));
    g_ready = input.at(8) != 0;
    g_nested_cancel = input.at(11) != 0;
    g_cursor_result = seed ^ 0x81234567u;
    Menu_Framework_displayActionCost = Byte(seed);
    g_owned = true;
    g_owned_bytes = static_cast<std::size_t>(input[1]) * 128;
    Menu_BattleAbilities_abilityText = g_text.data();
    for (std::size_t i = 0; i < g_text.size(); ++i)
        g_text[i] = static_cast<std::uint16_t>(i);
    for (std::size_t i = 0; i < 16; ++i) {
        Menu_BattleAbilities_unlockedChainAbilities[i] = Byte(seed + i * 17);
        Menu_BattleAbilities_unlockedDefenseAbilities[i] = Byte(seed + 61 + i * 13);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        Input_settings.mappedChainAbilities[i] = Byte(seed + (12 + i) * 13);
        Input_settings.mappedDefenseAbilities[i] = Byte(seed + (16 + i) * 13);
    }
    const int count = static_cast<int>(input[1]);
    const int first = static_cast<int>(input[3]);
    const std::array<int, 3> chain_rows { 0, 3, 7 };
    const std::array<int, 3> defense_rows { 1, 4, 6 };
    for (std::size_t button = 0; button < chain_rows.size(); ++button) {
        Input_settings.mappedChainAbilities[button] =
            Menu_BattleAbilities_unlockedChainAbilities[first
                                                        + std::min(chain_rows[button],
                                                            count - 1)];
        Input_settings.mappedDefenseAbilities[button] =
            Menu_BattleAbilities_unlockedDefenseAbilities[first
                                                          + std::min(defense_rows[button],
                                                              count - 1)];
    }
    static_assert(sizeof(Menu_Framework_menuMemory) >= 64);
    auto* memories = reinterpret_cast<std::uint8_t*>(&Menu_Framework_menuMemory);
    for (std::size_t i = 0; i < 64; ++i) {
        memories[i] = static_cast<std::uint8_t>(seed + i * 7);
    }
    for (std::size_t row = 0; row < 8; ++row) {
        auto& item = g_items[row + 20];
        item = { };
        item.state =
            static_cast<framework::RowState>(static_cast<std::uint8_t>(seed + row));
        item.gradient_state = static_cast<std::uint8_t>(seed + row * 19);
        item.is_scrollable = static_cast<std::uint8_t>(row % 3);
        item.selected = static_cast<std::uint8_t>(row % 2);
        item.target0 = static_cast<std::int16_t>(700 + row);
        item.x = static_cast<std::int16_t>(100 + row);
        item.y = static_cast<std::int16_t>(200 + row);
        item.w = static_cast<std::uint8_t>(90 + row);
        g_text_offsets[row + 20] = 500 + static_cast<int>(row);
    }
}
void RunVector(const Words& input)
{
    Require(input.size() == 12 && input.front() == 1, "wrong live assignment vector");
    // Independent vectors start cold; shared framework reset wiring is external.
    a::ResetLiveAbilityTerminalColdState();
    a::ResetLiveAbilityFoundationState();
    InitializeCommon();
    PrepareRows(input);
    g_calls.clear();
    g_refusals = 0;
    const int result = a::LiveUpdateAssignments();
    Require(g_refusals == 0, "live assignment refused a valid original vector");
    Words output { result, Menu_BattleAbilities_abilityMenuState,
        Menu_BattleAbilities_highlightedAbility,
        static_cast<std::uint8_t>(Menu_BattleAbilities_firstAbility),
        (Menu_BattleAbilities_abilityMenuState == 17 && result == 0) ? g_cursor_result
                                                                     : 0,
        Menu_BattleAbilities_abilityText != nullptr ? 1 : 0,
        static_cast<std::uint8_t>(Menu_Framework_displayActionCost) };
    // Wait-ready may transition to 17 without drawing the cursor.
    const bool drew_cursor = std::ranges::any_of(
        g_calls, [](const Words& call) { return !call.empty() && call.front() == 10; });
    output[4] = drew_cursor ? g_cursor_result : 0;
    AppendBytes(output, Input_settings.mappedChainAbilities);
    AppendBytes(output, Input_settings.mappedDefenseAbilities);
    const auto* memories =
        reinterpret_cast<const std::uint8_t*>(&Menu_Framework_menuMemory);
    for (std::size_t i = 0; i < 64; ++i)
        output.push_back(memories[i]);
    for (std::size_t row = 20; row < 28; ++row) {
        const auto& item = g_items[row];
        Append(
            output, std::array<std::int64_t, 9> { static_cast<std::uint8_t>(item.state),
                        item.gradient_state, item.is_scrollable, item.selected,
                        item.target0, item.x, item.y, item.w, g_text_offsets[row] });
    }
    output.push_back(g_calls.size());
    for (const auto& call : g_calls) {
        output.push_back(call.size());
        Append(output, call);
    }
    for (const auto value : output)
        std::cout << value << ' ';
    std::cout << '\n';
}
void CheckScreenAndOwnership()
{
    g_self_check = true;
    InitializeCommon();
    a::ResetLiveAbilityTerminalColdState();
    a::ResetLiveAbilityFoundationState();
    Menu_BattleAbilities_abilityText = nullptr;
    Menu_Framework_shortcutInvoked = 0;
    Input_settings.cursorMemory = Byte(0xA7);
    g_owned = false;
    g_ready = true;
    for (std::size_t i = 0; i < 16; ++i) {
        Battle_chainAbilityActionIds[i] = Byte(22 + i);
        Battle_defenseAbilityActionIds[i] = Byte(40 + i);
    }
    std::uint8_t phase = 3;
    Require(a::LiveUpdateAbilityScreen(&phase) == 0 && phase == 5,
        "live screen did not create its category selector");
    Require(g_categories == std::array<std::intptr_t, 6> { 69, 174, 222, 332, 0, 1 },
        "live count-dependent help, byte-shift quirk or disabled defense differs");
    Require(g_category_cursor == 0xA7
                && static_cast<std::uint8_t>(Input_settings.cursorMemory) == 0xA7
                && Menu_Framework_menuState.currentMenu == 0x55,
        "live category callback effects were overwritten");
    Input_settings.cursorMemory = 0;
    Battle_actionDefinitions[22].unlocked = 1;
    Battle_actionDefinitions[35].unlocked = 1;
    g_selected = 0;
    Require(a::LiveUpdateAbilityScreen(&phase) == 0 && phase == 6,
        "live category selection failed");
    Require(a::LiveUpdateAbilityScreen(&phase) == 0
                && Menu_BattleAbilities_abilityCount == 2
                && Menu_BattleAbilities_abilityMenuState == 0 && g_owned_bytes == 256
                && g_owned,
        "live learned list did not use foundation row creation");
    const auto* owned = Menu_BattleAbilities_abilityText;
    a::LiveUpdateChainAbilityMenu(1);
    a::LiveUpdateChainAbilityMenu(0);
    Require(Menu_BattleAbilities_abilityText == owned && g_owned_bytes == 256,
        "live repeated list entry replaced its owned rows");
    for (int tick = 0; tick < 3; ++tick)
        a::LiveUpdateAbilityScreen(&phase);
    Require(Menu_BattleAbilities_abilityMenuState == 17,
        "live assignment rows did not become ready");
    g_cursor_result = 29;
    Input_buttonsPressed.allButtons = 0;
    Input_buttonRepeat = 0;
    a::LiveUpdateAssignments();
    a::LiveUpdateAssignments();
    Require(g_last_cursor == 29 && Menu_Framework_displayActionCost == 1,
        "live cursor or nested Risk publication did not persist");
    a::LiveUpdateDefenseAbilityMenu(0);
    Menu_BattleAbilities_unlockedChainAbilities[15] = Byte(0x5E);
    Menu_BattleAbilities_unlockedDefenseAbilities[15] = Byte(0xB7);
    const std::array<char, std::size(Menu_BattleAbilities_unlockedChainAbilities)>
        reloaded_actions { };
    a::ResetLiveAbilityTerminalModuleState();
    Require(
        std::ranges::equal(reloaded_actions, Menu_BattleAbilities_unlockedChainAbilities)
            && std::ranges::equal(
                reloaded_actions, Menu_BattleAbilities_unlockedDefenseAbilities),
        "module reset retained image-backed learned actions");
    g_calls.clear();
    a::LiveUpdateChainAbilityMenu(0);
    a::LiveUpdateDefenseAbilityMenu(0);
    Require(std::ranges::count_if(g_calls,
                [](const Words& call) { return !call.empty() && call.front() == 7; })
                == 2,
        "module reset did not restart both initialized list phases");
    a::LiveUpdateAssignments();
    Require(g_last_cursor == 0 && Menu_BattleAbilities_abilityText == owned
                && Menu_BattleAbilities_abilityCount == 2 && g_owned_bytes == 256,
        "module reset did not reset cursor while preserving the outer row owner");
    g_frees = 0;
    Input_buttonsPressed.allButtons = 0x40;
    Require(a::LiveUpdateAssignments() == 1 && Menu_BattleAbilities_abilityText == nullptr
                && !g_owned && g_frees == 1 && g_description.size() == 96,
        "live cancel lost copied help or failed to release its sole row owner");
    const auto copied_help = g_description;
    g_text.fill(0xABCD);
    Require(g_description == copied_help, "information box retained borrowed freed rows");
    const auto refusals = g_refusals;
    a::LiveUpdateAssignments();
    Require(
        g_frees == 1 && g_refusals > refusals, "repeat cancel double-freed absent rows");

    Audio_Seq_unusedArtsStatus.kills = { };
    Battle_battleAbilityPointRequirements[0] = 100;
    for (std::size_t i = 0; i < 36; ++i)
        Menu_Framework_rowAnimationSteps[i] = Byte(i * 13);
    phase = 3;
    Menu_Framework_shortcutInvoked = 0;
    a::LiveUpdateAbilityScreen(&phase);
    g_selected = -1;
    a::LiveUpdateAbilityScreen(&phase);
    a::ResetLiveAbilityTerminalModuleState();
    g_point_positions.clear();
    phase = 0;
    a::LiveUpdateAbilityScreen(&phase);
    Require(!g_point_positions.empty() && g_point_positions.front() == 0x004200EE,
        "module reload retained image-backed screen animation");
    a::ResetLiveAbilityTerminalColdState();
    g_point_positions.clear();
    a::LiveUpdateAbilityScreen(&phase);
    Require(!g_point_positions.empty() && g_point_positions.front() == 0x004200EE,
        "cold reset retained image-backed screen animation");
    g_self_check = false;
}
} // namespace

extern "C" void* Core_ScratchBase() { return &g_scratch; }

namespace VS::Core {
bool IsLogEnabled(LogLevel, std::string_view) { return true; }
void LogWrite(LogLevel, std::string_view, std::string_view) { ++g_refusals; }
} // namespace VS::Core

namespace VS::Core::Memory {
void* AllocateLiveFromHighEnd(std::size_t bytes, std::source_location)
{
    Require(!g_owned && bytes <= sizeof(g_text), "live allocation reused a live owner");
    g_owned = true;
    g_owned_bytes = bytes;
    return g_text.data();
}
std::expected<std::size_t, HeapError> LivePayloadCapacity(const void* payload)
{
    if (payload != g_text.data() || !g_owned)
        return std::unexpected(HeapError::kNotAllocated);
    return g_owned_bytes;
}
void FreeLiveFromHighEnd(const void* payload, std::source_location)
{
    Require(payload == g_text.data() && g_owned,
        "live free released borrowed or absent text");
    g_calls.push_back({ 9, 0x80220000 });
    g_owned = false;
    ++g_frees;
}
} // namespace VS::Core::Memory

namespace VS::Audio {
void LivePlaySfxCentered(int group, int effect)
{
    g_calls.push_back({ 2, group, effect });
    if (g_nested_cancel)
        Input_buttonsPressed.allButtons |= 0x40;
}
} // namespace VS::Audio

namespace VS::Battle {
int LiveHasUnlockedBattleAbility(int defense)
{
    const auto* order =
        defense != 0 ? Battle_defenseAbilityActionIds : Battle_chainAbilityActionIds;
    for (std::size_t i = 0; i < 14; ++i) {
        if (Battle_actionDefinitions[static_cast<std::uint8_t>(order[i])].unlocked != 0)
            return 1;
    }
    return 0;
}
} // namespace VS::Battle

namespace VS::Render {
void DrawLiveCompactText(const char*, int xy, void* chain)
{
    Require(chain == nullptr, "points text used a nondefault ordering table");
    g_point_positions.push_back(static_cast<std::uint32_t>(xy));
}
} // namespace VS::Render

namespace VS::Menu {
void PlayLeaveSound() { g_calls.push_back({ 1 }); }
void PlayChangeSound() { g_calls.push_back({ 3 }); }
void InitLiveInformationTextBox(Menu_InformationBoxHeader)
{
    Unexpected("unlock header");
}
int DismissLiveTextBox(int)
{
    Unexpected("unlock dismiss");
    return 0;
}
} // namespace VS::Menu

namespace VS::Menu::Framework {
void InitializeLiveInformationBox() { }
void DismissLiveInformationBox() { }
void SetLiveCommandLabel(int) { }
void SetLiveInformationMessage(char*) { Unexpected("unbounded information text"); }
void SetLiveInformationMessageText(std::span<const std::uint8_t> text)
{
    const auto base = reinterpret_cast<std::uintptr_t>(g_text.data());
    const auto address = reinterpret_cast<std::uintptr_t>(text.data());
    Require(address >= base && address - base + text.size() <= g_owned_bytes,
        "live assignment description outside its owner");
    g_calls.push_back({ 6, static_cast<std::int64_t>((address - base) / 2) });
    g_description.assign(text.begin(), text.end());
}
int LiveSelectedRow() { return g_selected; }
Menu_Framework_MenuRow* SetLiveMenuItem(int, int, int, int, int, char*)
{
    Unexpected("unlock category");
    return nullptr;
}
MenuRow& SetLiveMenuItemText(
    const RowPlacement& placement, std::span<const std::uint8_t> text)
{
    Require(placement.row >= 0 && placement.row < 40, "live menu row outside table");
    auto& item = g_items.at(placement.row);
    item = { };
    item.state = RowState::kIdle;
    item.x = static_cast<std::int16_t>(placement.x);
    item.y = static_cast<std::int16_t>(placement.y);
    item.w = static_cast<std::uint8_t>(placement.w);
    item.background_width = static_cast<std::uint8_t>(placement.background_width);
    if (placement.row >= 20 && placement.row < 28) {
        const auto address = reinterpret_cast<std::uintptr_t>(text.data());
        const auto base = reinterpret_cast<std::uintptr_t>(g_text.data());
        Require(address >= base && address - base + text.size() <= g_owned_bytes,
            "live assignment name outside row owner");
        const auto offset = static_cast<int>((address - base) / 2);
        g_text_offsets.at(placement.row) = offset;
        g_calls.push_back({ 4, placement.row, placement.x, placement.y, placement.w,
            placement.background_width, offset });
    }
    return item;
}
Menu_Framework_MenuRow* LiveMenuItem(int row)
{
    Require(row >= 0 && row < 40, "live menu getter outside table");
    g_calls.push_back({ 5, row });
    static_assert(sizeof(MenuRow) == sizeof(Menu_Framework_MenuRow));
    return reinterpret_cast<Menu_Framework_MenuRow*>(&g_items.at(row));
}
void CreateLiveMenuRows(int count, int info, char** text, int* types)
{
    Require(g_self_check && count == 2 && info == 0x117,
        "unexpected live category row request");
    const auto base =
        reinterpret_cast<std::uintptr_t>(Menu_BattleAbilities_battleAbilityStrings);
    for (std::size_t i = 0; i < 4; ++i) {
        g_categories[i] =
            static_cast<std::intptr_t>(reinterpret_cast<std::uintptr_t>(text[i]) - base);
    }
    g_categories[4] = types[0];
    g_categories[5] = types[1];
    g_category_cursor = static_cast<std::uint8_t>(Input_settings.cursorMemory);
    Input_settings.cursorMemory = Byte(0x6E);
    Menu_Framework_menuState.currentMenu = 0x55;
}
void ClearLiveMenuExcept(int) { }
void FlyOutLiveRowsAndHoistSelection(int, int) { }
int AreLiveMenuRowsIdle()
{
    g_calls.push_back({ 7 });
    return g_ready ? 1 : 0;
}
void DrawLiveButtonIcon(int icon, int x, int y, u_long* chain)
{
    Require(chain == reinterpret_cast<u_long*>(g_ordering.data() + 8),
        "live button ordering table was advanced by native pointer elements");
    g_calls.push_back({ 8, icon, x, y });
}
int DrawLiveCursor(u_int step, int position)
{
    g_calls.push_back({ 10, step, position });
    g_last_cursor = step;
    return std::bit_cast<int>(g_cursor_result);
}
void SetLiveActionCostText(int index, const char* text, int x, int disabled)
{
    Require(disabled == 0, "live Risk text unexpectedly disabled");
    Menu_Framework_displayActionCost = 1;
    if (index == 1 && x == 8 && std::string_view(text) == "RISK")
        return;
    Require(index == 0 && x == 72 && text[0] == '#', "live Risk layout differs");
    unsigned action = 0;
    const auto length = std::strlen(text);
    const auto result = std::from_chars(text + 1, text + length, action);
    Require(result.ec == std::errc { } && result.ptr == text + length && action <= 255,
        "live Risk value cannot identify its action");
    g_calls.push_back({ 11, action });
}
} // namespace VS::Menu::Framework

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    try {
        LoadTables(argv[1]);
        CheckScreenAndOwnership();
        for (std::string line; std::getline(std::cin, line);) {
            std::istringstream input(line);
            Words values;
            for (std::int64_t word = 0; input >> word;)
                values.push_back(word);
            RunVector(values);
        }
    } catch (const std::runtime_error& error) {
        std::cerr << "live terminal contract: " << error.what() << '\n';
        return 3;
    } catch (const std::out_of_range& error) {
        std::cerr << "live terminal fixture bounds: " << error.what() << '\n';
        return 4;
    }
}
