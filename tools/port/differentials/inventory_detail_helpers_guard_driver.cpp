#include <array>
#include <bit>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "Input/Pad.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Sounds.hpp"

extern "C" {
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"

Menu_Framework_MenuRow* Menu_Framework_menuItems { };
Menu_Framework_MenuMemory Menu_Framework_menuMemory { };
u_char* Menu_Inventory_inventoryIndices[7] { };
int Input_buttonsState { }, Input_buttonRepeat { };
union Input_PressedButtonsView Input_buttonsPressed { };
int Menu_Inventory_ashleyRenderState { }, Menu_Inventory_ashleyYOffset { },
    Menu_Inventory_exitToBattle { };
char Menu_Inventory_secondaryTitleEnabled { },
    Menu_Inventory_itemCategoryIconsEnabled { }, Menu_Inventory_statusCommandState { },
    Menu_Inventory_selectedItemCategoryIconOnTop { };
}

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using Bytes = std::array<std::uint8_t, sizeof(Menu_Framework_MenuMemory)>;
using Call = std::array<int, 2>;
enum Event {
    kCount = 1,
    kInvalid,
    kClear,
    kRow,
    kContinue,
    kPages,
    kPrimary,
    kLeave,
    kDismiss,
    kBars
};
constexpr int kSelected = 20;
std::string_view selected;
std::vector<Call> calls;
std::array<Menu_Framework_MenuRow, fw::kRowCount> rows { }, replacement_rows { };
std::array<std::uint8_t, 64> order { }, replacement_order { };
Bytes initial_memory { };
bool valid = true;
int row_reads = 0;

static_assert(sizeof(fw::MenuRow) == sizeof(Menu_Framework_MenuRow));
static_assert(
    offsetof(fw::MenuRow, target0) == offsetof(Menu_Framework_MenuRow, targetPosition0));
static_assert(
    offsetof(fw::MenuRow, sub_text) == offsetof(Menu_Framework_MenuRow, subText));

void Record(int event, int argument = 0) { calls.push_back({ event, argument }); }
bool SameCalls(std::initializer_list<Call> expected)
{
    return calls == std::vector<Call>(expected);
}
bool LayoutFields()
{
    return Menu_Inventory_ashleyRenderState == 1
        && Menu_Inventory_itemCategoryIconsEnabled == 0
        && Menu_Inventory_statusCommandState == 0
        && static_cast<std::uint8_t>(Menu_Inventory_secondaryTitleEnabled) == 0xA5
        && Menu_Inventory_ashleyYOffset == 5;
}
bool MemoryPair(int page)
{
    auto expected = initial_memory;
    expected[0x3C] = 0;
    expected[0x3D] = static_cast<std::uint8_t>(page);
    return std::bit_cast<Bytes>(Menu_Framework_menuMemory) == expected;
}
bool DiagnosticState()
{
    if (selected == "missing-rows-transition" || selected == "invalid-category-paging"
        || selected == "invalid-category-selection") {
        return calls.empty()
            && std::bit_cast<Bytes>(Menu_Framework_menuMemory) == initial_memory;
    }
    if (selected == "missing-rows-layout" || selected == "invalid-row-layout") {
        return calls.empty() && LayoutFields();
    }
    if (selected == "missing-rows-exit") {
        return SameCalls({ { kLeave, 0 } })
            && Menu_Inventory_selectedItemCategoryIconOnTop == 0x55
            && Menu_Inventory_exitToBattle == 0x12345678;
    }
    if (selected == "missing-after-clear") {
        return SameCalls({ { kClear, 40 } })
            && rows[31].state == Menu_Framework_MenuItemState_Idle;
    }
    if (selected == "missing-after-row" || selected == "invalid-row-transition") {
        return SameCalls({ { kClear, 40 }, { kRow, 31 } })
            && rows[31].state == Menu_Framework_MenuItemState_SlideY
            && rows[31].targetPosition0 == 18;
    }
    if (selected == "missing-order-selection")
        return calls.empty() && MemoryPair(7);
    if (selected == "invalid-page-selection")
        return calls.empty() && MemoryPair(8);
    if (selected == "negative-page-selection")
        return calls.empty() && MemoryPair(-1);
    return false;
}

bool PositiveState()
{
    if (selected == "panels-unused-owners") {
        return calls.empty()
            && static_cast<std::uint8_t>(Menu_Inventory_secondaryTitleEnabled) == 0xFF
            && static_cast<std::uint8_t>(Menu_Inventory_itemCategoryIconsEnabled) == 0xFF
            && static_cast<std::uint8_t>(Menu_Inventory_statusCommandState) == 0xFF
            && Menu_Inventory_ashleyYOffset == 5
            && std::bit_cast<Bytes>(Menu_Framework_menuMemory) == initial_memory;
    }
    if (selected == "paging-unused-category")
        return calls.empty();
    if (selected == "paging-current-input")
        return SameCalls({ { kCount, 0 } });
    if (selected == "paging-invalid-feedback")
        return SameCalls({ { kCount, 0 }, { kInvalid, 0 } });
    if (selected == "transition-clear-owner") {
        return SameCalls({ { kClear, 40 }, { kRow, 31 }, { kRow, kSelected } })
            && replacement_rows[31].state == Menu_Framework_MenuItemState_SlideY
            && replacement_rows[kSelected].targetPosition0 == 155
            && replacement_rows[kSelected].selected == 1
            && replacement_rows[kSelected].subText == nullptr
            && rows[kSelected].selected == 0;
    }
    if (selected == "transition-row-owner") {
        return SameCalls({ { kClear, 40 }, { kRow, 31 }, { kRow, kSelected } })
            && rows[31].targetPosition0 == 18
            && replacement_rows[kSelected].targetPosition0 == 155
            && rows[kSelected].selected == 0;
    }
    if (selected == "layout-callback-writes") {
        return SameCalls({ { kContinue, kSelected }, { kPages, 7 }, { kPrimary, 1 } })
            && Menu_Inventory_itemCategoryIconsEnabled == 9
            && Menu_Inventory_statusCommandState == 10;
    }
    if (selected == "selection-current-order") {
        return calls.empty() && MemoryPair(7) && Menu_Framework_menuItems == nullptr;
    }
    if (selected == "exit-current-input") {
        return SameCalls({ { kLeave, 0 }, { kClear, 40 }, { kDismiss, -1 },
                   { kPages, -1 }, { kPrimary, 2 }, { kBars, 4 } })
            && Menu_Inventory_selectedItemCategoryIconOnTop == 2
            && Menu_Inventory_exitToBattle == static_cast<int>(VS::Input::Triangle);
    }
    if (selected == "exit-unused-bars") {
        return SameCalls({ { kLeave, 0 }, { kClear, 40 }, { kDismiss, -1 },
                   { kPages, -1 }, { kPrimary, 2 } })
            && Menu_Inventory_selectedItemCategoryIconOnTop == 2
            && Menu_Inventory_exitToBattle == 0;
    }
    return false;
}

void Initialize()
{
    initial_memory.fill(0xA5);
    Menu_Framework_menuMemory = std::bit_cast<Menu_Framework_MenuMemory>(initial_memory);
    Menu_Framework_menuItems = rows.data();
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<std::uint8_t>(i + 1);
        replacement_order[i] = static_cast<std::uint8_t>(64 - i);
    }
    for (auto& pointer : Menu_Inventory_inventoryIndices)
        pointer = order.data();
    for (auto* table : { &rows, &replacement_rows }) {
        for (auto& row : *table) {
            row.state = Menu_Framework_MenuItemState_Idle;
            row.w = 80;
        }
    }
    Menu_Inventory_ashleyRenderState = 0;
    Menu_Inventory_ashleyYOffset = 5;
    Menu_Inventory_secondaryTitleEnabled = static_cast<char>(0xA5);
    Menu_Inventory_itemCategoryIconsEnabled = 6;
    Menu_Inventory_statusCommandState = 7;
    Menu_Inventory_selectedItemCategoryIconOnTop = 0x55;
    Menu_Inventory_exitToBattle = 0x12345678;
    Input_buttonsPressed.allButtons = VS::Input::Circle;
    Input_buttonRepeat = VS::Input::R1;
}

void Execute()
{
    if (selected == "panels-unused-owners") {
        Menu_Framework_menuItems = nullptr;
        inventory::SetLivePanelVisibility(static_cast<char>(0xFF));
    } else if (selected == "paging-unused-category") {
        Menu_Framework_menuItems = nullptr;
        Input_buttonsState = VS::Input::L1 | VS::Input::R1;
        valid &= inventory::PollLiveDetailsPaging(-1, 17) == 17;
    } else if (selected == "paging-current-input") {
        valid &= inventory::PollLiveDetailsPaging(0, 0) == 7;
    } else if (selected == "paging-invalid-feedback") {
        valid &= inventory::PollLiveDetailsPaging(0, 0) == 0;
    } else if (selected == "invalid-category-paging") {
        (void)inventory::PollLiveDetailsPaging(-1, 0);
    } else if (selected == "missing-rows-layout") {
        Menu_Framework_menuItems = nullptr;
        inventory::InitializeLiveDetailsLayout(kSelected, 7);
    } else if (selected == "invalid-row-layout") {
        inventory::InitializeLiveDetailsLayout(40, 7);
    } else if (selected == "layout-callback-writes") {
        inventory::InitializeLiveDetailsLayout(kSelected, 7);
    } else if (selected == "missing-rows-exit") {
        Menu_Framework_menuItems = nullptr;
        inventory::ExitLiveDetailsView(0);
    } else if (selected == "exit-current-input" || selected == "exit-unused-bars") {
        inventory::ExitLiveDetailsView(selected == "exit-current-input" ? 1 : 0);
    } else if (selected == "selection-current-order") {
        Menu_Framework_menuItems = nullptr;
        valid &= inventory::SelectLiveDetailsItem(0, 7) == 8;
        Menu_Inventory_inventoryIndices[0] = replacement_order.data();
        valid &= inventory::SelectLiveDetailsItem(0, 7) == 57;
    } else if (selected == "invalid-category-selection") {
        (void)inventory::SelectLiveDetailsItem(7, 0);
    } else if (selected == "missing-order-selection") {
        Menu_Inventory_inventoryIndices[0] = nullptr;
        (void)inventory::SelectLiveDetailsItem(0, 7);
    } else if (selected == "invalid-page-selection") {
        (void)inventory::SelectLiveDetailsItem(0, 8);
    } else if (selected == "negative-page-selection") {
        (void)inventory::SelectLiveDetailsItem(0, -1);
    } else {
        if (selected == "missing-rows-transition")
            Menu_Framework_menuItems = nullptr;
        inventory::BeginLiveDetailsTransition(
            selected == "invalid-row-transition" ? -1 : kSelected);
    }
}
} // namespace

extern "C" void Core_Errorf(const char*, const char* format, ...)
{
    std::va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
    const bool expected = valid && DiagnosticState();
    std::fprintf(
        stderr, "guard-state-order-lazy-boundaries=%s\n", expected ? "PASS" : "FAIL");
    std::_Exit(expected ? 85 : 86);
}

namespace VS::Items::Inventory {
int CountLiveItems(int category, const Items_Inventory* source)
{
    valid &= category == 0 && source == nullptr;
    Record(kCount, category);
    Input_buttonRepeat = VS::Input::L1;
    Input_buttonsPressed.allButtons =
        selected == "paging-invalid-feedback" ? VS::Input::L1 : 0;
    return selected == "paging-invalid-feedback" ? 1 : 8;
}
} // namespace VS::Items::Inventory

namespace VS::Menu {
void PlayLeaveSound() { Record(kLeave); }
void PlayInvalidSound() { Record(kInvalid); }
} // namespace VS::Menu

namespace VS::Menu::Framework {
void ClearLiveMenuExcept(int row)
{
    Record(kClear, row);
    if (selected == "transition-clear-owner")
        Menu_Framework_menuItems = replacement_rows.data();
    if (selected == "missing-after-clear")
        Menu_Framework_menuItems = nullptr;
}
MenuRow& LiveMenuRow(int row)
{
    Record(kRow, row);
    auto& result = reinterpret_cast<MenuRow&>(Menu_Framework_menuItems[row]);
    ++row_reads;
    if (row_reads == 1 && selected == "transition-row-owner") {
        Menu_Framework_menuItems = replacement_rows.data();
    }
    if (row_reads == 1 && selected == "missing-after-row")
        Menu_Framework_menuItems = nullptr;
    return result;
}
void ContinueLiveItemDetailsTransition(int row)
{
    valid &= LayoutFields();
    Record(kContinue, row);
}
void UpdateLiveEquipmentStatPages(int pages) { Record(kPages, pages); }
void UpdateLivePrimaryStatPanel(int motion)
{
    Record(kPrimary, motion);
    if (selected == "exit-current-input")
        Input_buttonsPressed.allButtons = VS::Input::Triangle;
    if (selected == "layout-callback-writes") {
        Menu_Inventory_itemCategoryIconsEnabled = 9;
        Menu_Inventory_statusCommandState = 10;
    }
}
void FlyOutLiveRowLeft(int row) { Record(kDismiss, row); }
void UpdateLiveDpPpBars(int flags)
{
    valid &= Menu_Inventory_exitToBattle == static_cast<int>(VS::Input::Triangle)
          && Menu_Inventory_selectedItemCategoryIconOnTop == 2;
    Record(kBars, flags);
}
} // namespace VS::Menu::Framework

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    selected = argv[1];
    Initialize();
    Execute();
    valid &= PositiveState();
    std::printf("positive-fields-order-lazy-boundaries=%s\n", valid ? "PASS" : "FAIL");
    return valid ? 0 : 1;
}
