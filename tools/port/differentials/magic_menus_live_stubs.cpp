// Isolated Live contract tests. Framework, battle-cost and render allocation
// boundaries are recording substitutes; the production Magic Live/core run.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/AbilitiesLive.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Core/Log.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/MagicCostsLive.hpp"
#include "Menu/MagicMenusLive.hpp"
#include "Menu/MagicTerminalLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Magic.h"
#include "Render/Text/CompactFont.h"

decltype(Battle_actionDefinitions) Battle_actionDefinitions { };
decltype(Battle_characterState) Battle_characterState { };
decltype(Input_settings) Input_settings { };
decltype(Input_buttonRepeat) Input_buttonRepeat { };
decltype(Input_buttonsPressed) Input_buttonsPressed { };
decltype(Audio_Seq_unusedStateFlags) Audio_Seq_unusedStateFlags { };
decltype(Menu_Framework_menuState) Menu_Framework_menuState { };
decltype(Menu_Framework_actionSelection) Menu_Framework_actionSelection { };
decltype(Menu_Framework_menuMemory) Menu_Framework_menuMemory { };
decltype(Menu_Framework_shortcutInvoked) Menu_Framework_shortcutInvoked { };
decltype(Menu_Framework_actionRestrictions) Menu_Framework_actionRestrictions { };
decltype(Menu_Framework_displayActionCost) Menu_Framework_displayActionCost { };
decltype(Menu_Framework_cursorBrightnessAnimation)
    Menu_Framework_cursorBrightnessAnimation { };
decltype(Menu_Magic_warlockSpellIds) Menu_Magic_warlockSpellIds { };
decltype(Menu_Magic_shamanSpellIds) Menu_Magic_shamanSpellIds { };
decltype(Menu_Magic_sorcererSpellIds) Menu_Magic_sorcererSpellIds { };
decltype(Menu_Magic_enchanterSpellIds) Menu_Magic_enchanterSpellIds { };
decltype(Menu_Magic_magicStrings) Menu_Magic_magicStrings { };
decltype(Menu_Magic_teleportationStrings) Menu_Magic_teleportationStrings { };
decltype(Menu_menuStrings) Menu_menuStrings { };
decltype(Render_compactGlyphWidths) Render_compactGlyphWidths { };
decltype(Render_compactGlyphLeftTrim) Render_compactGlyphLeftTrim { };
decltype(Render_compactGlyphIndices) Render_compactGlyphIndices { };
}

namespace {
namespace magic = VS::Menu::Magic;
namespace framework = VS::Menu::Framework;
using Entry = int (*)(unsigned int);
constexpr std::array<Entry, 4> kEntries { magic::UpdateWarlockMenuLive,
    magic::UpdateShamanMenuLive, magic::UpdateSorcererMenuLive,
    magic::UpdateEnchanterMenuLive };
struct BoundaryState {
    int selected = -1;
    int highlighted = 0;
    int row_calls = 0;
    int cost = 0;
    int log_calls = 0;
    int menu_during_idle = -1;
    bool idle = true;
    bool has_frame = true;
    int rows = 0;
    std::array<int, 18> types { };
    std::array<std::array<char, 26>, 18> names { };
    std::array<std::array<char, 92>, 18> descriptions { };
    std::array<framework::MenuRow, 40> menu_rows { };
    std::array<u_long, 2> chains { };
    std::vector<std::pair<int, int>> ordering_requests;
    std::vector<std::vector<std::uint32_t>> packets;
};
BoundaryState g_boundary;
Battle_Actor g_actor;
Battle_ActorStats g_stats;

template <std::size_t N> void FillIds(char (&ids)[N])
{
    for (std::size_t i = 0; i < N; ++i) {
        ids[i] = static_cast<char>(200 + i);
    }
}
void Reset()
{
    g_boundary = { };
    g_actor = { };
    g_stats = { };
    Battle_characterState = &g_actor;
    g_actor.stats = &g_stats;
    std::fill_n(Battle_actionDefinitions, 256, Battle_ActionDefinition { });
    Battle_actionDefinitions[200].unlocked = 1;
    Battle_actionDefinitions[200].name[0] = 'A';
    Battle_actionDefinitions[201].name[0] = 'B';
    FillIds(Menu_Magic_warlockSpellIds);
    FillIds(Menu_Magic_shamanSpellIds);
    FillIds(Menu_Magic_sorcererSpellIds);
    FillIds(Menu_Magic_enchanterSpellIds);
    std::fill_n(Menu_Magic_magicStrings, 2462, u_short { 0 });
    std::fill_n(Menu_Magic_magicStrings, 80, u_short { 100 });
    Menu_Magic_magicStrings[100] = 'W';
    Menu_Framework_menuMemory = { };
    Menu_Framework_menuState = { };
    Menu_Framework_actionSelection = { };
    Menu_Framework_actionRestrictions = 0;
    Menu_Framework_shortcutInvoked = 0;
    Input_settings = { };
    Input_buttonsPressed.allButtons = 0;
    Input_buttonRepeat = 0;
    std::fill_n(Render_compactGlyphWidths, 92, u_char { 0x55 });
    std::fill_n(Render_compactGlyphLeftTrim, 264, u_char { 0 });
    std::fill_n(Render_compactGlyphIndices, 60, u_char { 0 });
    std::fill_n(Menu_Framework_cursorBrightnessAnimation, 16, char { 128 });
    magic::ResetMagicMenusLiveState();
}
} // namespace

namespace VS::Core {
bool IsLogEnabled(LogLevel, std::string_view) { return true; }
void LogWrite(LogLevel, std::string_view, std::string_view) { ++g_boundary.log_calls; }
} // namespace VS::Core
namespace VS::Battle {
int LiveActorActionRestrictionFlags(int, int) { return 0; }
int LiveHasUnlockedSpellClass(int) { return 1; }
} // namespace VS::Battle
int Field_CurrentSavePointIdEntry() { return 1; }
namespace VS::Menu {
void PlayLeaveSound() { }
void PlaySelectSound() { }
void PlayInvalidSound() { }
void PlayChangeSound() { }
} // namespace VS::Menu
namespace VS::Menu::Magic {
void SetSpellMpCostLive(int action) { g_boundary.cost = action; }
void CreateHeaderLive() { }
int UpdateTeleportMenuLive(int) { return 0; }
} // namespace VS::Menu::Magic
namespace VS::Menu::Framework {
void FlyOutLiveRowsAndHoistSelection(int, int) { }
void FlyOutLiveRowRight(int) { }
void ClearLiveMenuExcept(int) { }
void DismissLiveInformationBox() { }
void InitializeLiveInformationBox() { }
void SetLiveCommandLabel(int) { }
int AreLiveMenuRowsIdle()
{
    if (g_boundary.menu_during_idle >= 0) {
        Menu_Framework_menuState.currentMenu =
            static_cast<char>(g_boundary.menu_during_idle);
    }
    return g_boundary.idle ? 1 : 0;
}
int LiveSelectedRow() { return g_boundary.selected; }
int LiveHighlightedRow() { return g_boundary.highlighted; }
std::span<std::uint8_t> LiveMenuCursorMemory()
{
    return { reinterpret_cast<std::uint8_t*>(Menu_Framework_menuMemory.cursorMemories),
        std::size(Menu_Framework_menuMemory.cursorMemories) };
}
void CreateLiveMenuRows(int rows, int, char* strings[], int types[])
{
    ++g_boundary.row_calls;
    g_boundary.rows = rows;
    for (int i = 0; i < rows; ++i) {
        g_boundary.types[i] = types[i];
        std::copy_n(strings[i * 2], 26, g_boundary.names[i].begin());
        std::copy_n(strings[i * 2 + 1], 92, g_boundary.descriptions[i].begin());
    }
}
MenuRow& SetLiveMenuItemText(
    const RowPlacement& placement, std::span<const std::uint8_t> text)
{
    auto& row = g_boundary.menu_rows[placement.row];
    row.x = static_cast<std::int16_t>(placement.x);
    row.y = static_cast<std::int16_t>(placement.y);
    row.w = static_cast<std::uint8_t>(placement.w);
    row.background_width = static_cast<std::uint8_t>(placement.background_width);
    std::copy_n(text.begin(), std::min(text.size(), row.text.size()), row.text.begin());
    return row;
}
} // namespace VS::Menu::Framework
namespace VS::Render {
u_long* LiveFrameOrderingEntry(LiveOrderingBase base, int slot)
{
    g_boundary.ordering_requests.emplace_back(static_cast<int>(base), slot);
    return g_boundary.has_frame ? &g_boundary.chains[static_cast<int>(base)] : nullptr;
}
std::uint32_t LivePacketQueue::ChainLink() const
{
    return chain_ != nullptr ? *chain_ : 0;
}
bool LivePacketQueue::CanEmit(std::size_t) const { return chain_ != nullptr; }
bool LivePacketQueue::Emit(std::span<const std::uint32_t> packet)
{
    if (chain_ == nullptr)
        return false;
    g_boundary.packets.emplace_back(packet.begin(), packet.end());
    *chain_ = static_cast<u_long>(g_boundary.packets.size());
    return true;
}
} // namespace VS::Render

TEST_CASE("Magic live school owners reset every retained selection and list",
    "[magic-terminal-live][F02604][F02605][F02606][F02607]")
{
    const auto school = GENERATE(0U, 1U, 2U, 3U);
    Reset();
    CHECK(kEntries[school](1) == 0);
    CHECK(kEntries[school](0) == 0);
    g_boundary.selected = 0;
    CHECK(kEntries[school](0) == 0);
    CHECK(kEntries[school](0) == 200);
    magic::ResetMagicMenusLiveState();
    Battle_actionDefinitions[200].unlocked = 0;
    Battle_actionDefinitions[201].unlocked = 1;
    CHECK(kEntries[school](0) == 0);
    CHECK(g_boundary.row_calls == 2);
    CHECK(kEntries[school](0) == 0);
    CHECK(kEntries[school](0) == 201);
}

TEST_CASE(
    "Magic live rows copy bounded text and initialize all visible restriction flags",
    "[magic-terminal-live][F02604][F02605][F02606][F02607]")
{
    const auto school = GENERATE(0U, 1U, 2U, 3U);
    Reset();
    Menu_Framework_actionRestrictions = 0xB7;
    Input_settings.cursorMemory = static_cast<char>(255);
    CHECK(kEntries[school](2) == 0);
    CHECK(kEntries[school](0) == 0);
    CHECK(g_boundary.rows == 1);
    CHECK(g_boundary.types[0] == 1);
    CHECK(g_boundary.names[0][0] == (school == 0 ? 'W' : 'A'));
    CHECK(g_boundary.names[0][25] == 0);
    CHECK(g_boundary.descriptions[0][91] == 0);
    CHECK(static_cast<unsigned char>(Input_settings.cursorMemory) == 255);
}

TEST_CASE("Magic live schools refuse missing player and out-of-range text",
    "[magic-terminal-live][F02604][F02605][F02606][F02607]")
{
    const auto school = GENERATE(0U, 1U, 2U, 3U);
    const auto bad_source = GENERATE(0, 1, 2);
    Reset();
    const auto before = magic::RefusedMagicMenuCount();
    if (bad_source == 0)
        Battle_characterState = nullptr;
    if (bad_source == 1)
        g_actor.stats = nullptr;
    if (bad_source == 2) {
        std::fill_n(Menu_Magic_magicStrings, 2462, u_short { 0x4141 });
    }
    CHECK(kEntries[school](2) == 0);
    CHECK(kEntries[school](0) == 0);
    CHECK(magic::RefusedMagicMenuCount() == before + 1);
    CHECK(g_boundary.row_calls == 0);
}

TEST_CASE("Magic live text never reads beyond bounded legacy arrays",
    "[magic-terminal-live][F02604][F02605][F02606][F02607]")
{
    const auto school = GENERATE(0U, 1U, 2U, 3U);
    Reset();
    const auto before = magic::RefusedMagicMenuCount();
    if (school == 0) {
        Menu_Magic_magicStrings[62] = 2461;
        Menu_Magic_magicStrings[2461] = 0x4141;
    } else {
        std::fill_n(Battle_actionDefinitions[200].name, 24, 'A');
    }
    Input_settings.cursorMemory = static_cast<char>(255);
    CHECK(kEntries[school](2) == 0);
    CHECK(kEntries[school](0) == 0);
    CHECK(magic::RefusedMagicMenuCount() == before + 1);
    CHECK(g_boundary.row_calls == 0);
    CHECK(static_cast<unsigned char>(Input_settings.cursorMemory) == 255);
}

TEST_CASE("F02604 live rank binding uses checked original ordering buckets",
    "[magic-terminal-live][F02604]")
{
    Reset();
    Menu_Magic_warlockSpellIds[0] = 106;
    Battle_actionDefinitions[200].unlocked = 0;
    Battle_actionDefinitions[106].unlocked = 1;
    Battle_actionDefinitions[107].unlocked = 1;
    g_boundary.selected = 0;
    const bool ready = GENERATE(false, true);
    g_boundary.has_frame = ready;
    const auto before = magic::RefusedMagicMenuCount();
    CHECK(magic::UpdateWarlockMenuLive(1) == 0);
    CHECK(magic::UpdateWarlockMenuLive(0) == 0);
    CHECK(magic::UpdateWarlockMenuLive(0) == 0);
    CHECK(magic::UpdateWarlockMenuLive(0) == 0);
    CHECK(magic::UpdateWarlockMenuLive(0) == 0);
    CHECK((g_boundary.ordering_requests
           == std::vector<std::pair<int, int>> { { 1, 0 }, { 0, -1 } }));
    CHECK(g_boundary.packets.size() == (ready ? 20U : 0U));
    CHECK(g_boundary.cost == 106);
    CHECK(magic::RefusedMagicMenuCount() == before + (ready ? 0 : 1));
}

TEST_CASE("F02600 live title publishes a bounded selected native row",
    "[magic-terminal-live][F02600]")
{
    Reset();
    const auto school = GENERATE(0, 1, 2, 3);
    Menu_Framework_shortcutInvoked = static_cast<char>(school + 1);
    std::uint8_t state = 3;
    CHECK(magic::UpdateLive(&state) == 0);
    const auto& row = g_boundary.menu_rows[school + 10];
    CHECK(row.x == 320);
    CHECK(row.y == 34);
    CHECK(row.w == 140);
    CHECK(row.selected == 1);
    CHECK(row.text[0] == 'W');
    CHECK(state == 7 + school * 2);
}

TEST_CASE("F02600 live execution preserves nested framework writes",
    "[magic-terminal-live][F02600]")
{
    Reset();
    g_boundary.menu_during_idle = 255;
    std::uint8_t state = 16;
    CHECK(magic::UpdateLive(&state) == 1);
    CHECK(state == 0);
    CHECK(Menu_Framework_actionSelection.selectionKind == 7);
    CHECK(Menu_Framework_menuState.currentMenu == 1);
    CHECK(static_cast<unsigned char>(Menu_Framework_menuState.returnState) == 255);
}

int main(int argc, char* argv[]) { return Catch::Session().run(argc, argv); }
