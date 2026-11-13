// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/SfxLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Menu/Container/ContainerSortingLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/InventoryLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Container_discardMenuCursorAnimation = 0;

// decomp: D_80109A82 (D_80109A82@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_transferredMiscCount;

// decomp: _containerItemCapacities (_containerItemCapacities@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_containerItemCapacities[] = { 32, 64, 64, 32, 64, 192, 256, 2 };

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Container_discardMenuCountTemplate[] = "|>48|000/000\0";

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Container_discardSteps[] = { 1, 10, 100 };

// decomp: MENUD.PRG _displaySortMenu (src/MENU/MENUD.PRG/234.c:1601)
int Menu_Container_UpdateSortMenu(int category_selection)
{
    static char Menu_Container_kWeaponSortStats[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23 };
    static char Menu_Container_kGripSortStats[] = { 0, 8, 9, 10, 24, 25, 26 };
    static char Menu_Container_kShieldSortStats[] = { 1, 4, 5, 6, 7, 8, 9, 10, 40, 41, 42, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39 };
    static char Menu_Container_kArmorSortStats[] = { 0, 1, 4, 5, 8, 9, 10, 40, 41, 42, 27, 28, 29, 30, 31, 32,
        33, 34, 35, 36, 37, 38, 39 };
    static char Menu_Container_kGemSortStats[] = { 8, 9, 10, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39 };
    static char Menu_Container_kMiscSortStats[] = { 0, 56 };
    static char Menu_Container_kSortStatCounts[] = { sizeof Menu_Container_kWeaponSortStats, sizeof Menu_Container_kWeaponSortStats, sizeof Menu_Container_kGripSortStats,
        sizeof Menu_Container_kShieldSortStats, sizeof Menu_Container_kArmorSortStats, sizeof Menu_Container_kGemSortStats, sizeof Menu_Container_kMiscSortStats };
    static char* Menu_Container_kSortStatLists[] = { Menu_Container_kWeaponSortStats, Menu_Container_kWeaponSortStats, Menu_Container_kGripSortStats, Menu_Container_kShieldSortStats,
        Menu_Container_kArmorSortStats, Menu_Container_kGemSortStats, Menu_Container_kMiscSortStats };

    static char Menu_Container_sortMenuStep;
    static u_char Menu_Container_sortCategory;

    enum Menu_Container_SortMenuState { Menu_Container_SortMenuState_Init, Menu_Container_SortMenuState_Display };

    char* sort_row_text[48];
    int sort_row_types[24];
    int i;
    char* sort_stat_ids;

    if (category_selection != 0) {
        Menu_Container_sortCategory = category_selection - 1;
        Menu_Container_sortMenuStep = 0;
        return 0;
    }

    switch (Menu_Container_sortMenuStep) {
    case Menu_Container_SortMenuState_Init:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            sort_stat_ids = Menu_Container_kSortStatLists[Menu_Container_sortCategory];

            for (i = 0; i < Menu_Container_kSortStatCounts[Menu_Container_sortCategory]; ++i) {
                sort_row_text[i * 2] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [sort_stat_ids[i] * 2 + Text_Menu_ITEMHELP_BIN_Index_sortMenu]];
                sort_row_text[i * 2 + 1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [sort_stat_ids[i] * 2 + Text_Menu_ITEMHELP_BIN_Index_sortMenuDesc]];
                sort_row_types[i] = 0;
            }
            VS::Menu::Framework::CreateLiveActionRows(i, Menu_Container_sortCategory + 96, sort_row_text, sort_row_types);
            Menu_Container_sortMenuStep = Menu_Container_SortMenuState_Display;
        }
        break;
    case Menu_Container_SortMenuState_Display:
        VS::Menu::Framework::UpdateLiveActionMenu();
        i = VS::Menu::Framework::LiveActionSelectionResult();
        if ((i + 1) != 0) {
            if (i >= 0) {
                VS::Menu::PlaySelectSound();
                switch (Menu_Container_sortCategory) {
                case 0:
                    VS::Menu::container::SortLiveWeapons(Menu_Container_kWeaponSortStats[i]);
                    break;
                case 3:
                    VS::Menu::container::SortLiveShields(Menu_Container_kShieldSortStats[i]);
                    break;
                case 6:
                    VS::Menu::container::SortLiveMisc(Menu_Container_kMiscSortStats[i]);
                    break;
                default:
                    VS::Menu::container::SortLiveEquipment(Menu_Container_sortCategory, Menu_Container_kSortStatLists[Menu_Container_sortCategory][i]);
                    break;
                }
            } else {
                VS::Menu::PlayLeaveSound();
            }
            return i + 1;
        }
        break;
    }
    return 0;
}

// decomp: MENUD.PRG _discardMenu (src/MENU/MENUD.PRG/234.c:1680)
int Menu_Container_UpdateDiscardMenu(int packed_discard_selection)
{
    enum Menu_Inventory_DiscardMenuState {
        Menu_Inventory_DiscardMenuState_Init,
        Menu_Inventory_DiscardMenuState_DiscardOneInit,
        Menu_Inventory_DiscardMenuState_DiscardMultipleInit,
        Menu_Inventory_DiscardMenuState_DiscardOne,
        Menu_Inventory_DiscardMenuState_ClearTemplate,
        Menu_Inventory_DiscardMenuState_DetermineDiscardAmount,
        Menu_Inventory_DiscardMenuState_DiscardMultipleConfirm,
        Menu_Inventory_DiscardMenuState_DiscardMultiple
    };

#pragma vsstring(start)
#pragma vsstring(end)


    static char Menu_Container_discardMenuState;
    static char Menu_Container_discardMenuItemCategory;
    static u_char Menu_Container_discardMenuSelectedItem;
    static char Menu_Container_discardType;
    static u_char Menu_Container_discardStep;
    static char Menu_Container_discardCount;
    static char Menu_Container_discardMenuMaxAmount;
    static char Menu_Container_discardMenuLayoutPadding[3] __attribute__((unused));

    char* discardOneText[4];
    int discardOneRowTypes[2];
    char* discardMultipleText[4];
    int discardMultipleRowTypes[2];
    int i;
    int packed_discard_digits;
    int var_v0;
    int previous_discard_count;
    Menu_Framework_MenuRow* menuItem;
    int a0;

    if (packed_discard_selection != 0) {
        Menu_Container_discardType = packed_discard_selection >> 0x10;
        Menu_Container_discardMenuItemCategory = (packed_discard_selection >> 0xC) & 0xF;
        Menu_Container_discardMenuSelectedItem = packed_discard_selection - 1;
        Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(1));
        VS::Menu::Framework::SetLiveInformationMessage(
            (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [Menu_Container_discardType + Text_Menu_ITEMHELP_BIN_Index_discardConfirm]]);
        Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_Init;
        return 0;
    }
    switch (Menu_Container_discardMenuState) {
    case Menu_Inventory_DiscardMenuState_Init:
        var_v0 = 0;
        if (Menu_textBoxes[7].flags.done) {
            if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
                Menu_Container_discardMenuState = ((Menu_Container_discardType & 1) + 1);
            }
        }
        break;
    case Menu_Inventory_DiscardMenuState_DiscardOneInit:
        for (i = 0; i < 2; ++i) {
            discardOneText[i * 2] = (char*)&Menu_Framework_itemHelp
                [Menu_Framework_itemHelp[i + Text_Menu_ITEMHELP_BIN_Index_confirmYes]];
            discardOneText[i * 2 + 1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [Menu_Container_discardType + Text_Menu_ITEMHELP_BIN_Index_discardConfirm]];
            discardOneRowTypes[i] = 0;
        }

        VS::Menu::Framework::CreateLiveActionRows(
            2, Menu_Framework_ActionsMenuAction_Discard, discardOneText, discardOneRowTypes);
        Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardOne;
        break;
    case Menu_Inventory_DiscardMenuState_DiscardMultipleInit:
        Menu_Container_discardMenuMaxAmount = Menu_Container_containerData->items.misc[Menu_Container_discardMenuSelectedItem].count;
        i = VS::Core::Math::SplitDecimalDigit(Menu_Container_discardMenuMaxAmount);
        Menu_Container_discardMenuCountTemplate[8] = i & 0xF;
        i = VS::Core::Math::SplitDecimalDigit(i >> 4);

        Menu_Container_discardMenuCountTemplate[7] = i & 0xF;
        Menu_Container_discardMenuCountTemplate[6] = i >> 4;

        for (i = 2; i < 5; ++i) {
            Menu_Container_discardMenuCountTemplate[i] = 0;
        }

        menuItem = VS::Menu::Framework::SetLiveMenuItem(0x22, -0x7E, 0x82, 0x7E, 0, Menu_Container_discardMenuCountTemplate);
        menuItem->state = 5;
        menuItem->targetPosition0 = 0;
        Menu_Container_discardStep = 0;
        Menu_Container_discardCount = 0;
        Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_ClearTemplate;
        break;
    case Menu_Inventory_DiscardMenuState_DiscardOne:
        VS::Menu::Framework::UpdateLiveActionMenu();
        i = VS::Menu::Framework::LiveActionSelectionResult() + 1;
        if (i != 0) {
            if (i == 1) {
                VS::Audio::LivePlaySfxCentered(0x7E, 0x1C);
                VS::Items::ClearLiveContainerItem(
                    Menu_Container_discardMenuItemCategory, Menu_Container_discardMenuSelectedItem, &Menu_Container_containerData->items);
            } else {
                VS::Menu::PlayLeaveSound();
            }
            return i;
        }
        break;
    case Menu_Inventory_DiscardMenuState_ClearTemplate:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
            break;
        }

        for (i = 2; i < 5; ++i) {
#pragma vsstring(start)
            Menu_Container_discardMenuCountTemplate[i] = ' ';
#pragma vsstring(end)
        }

        VS::Menu::Framework::SetLiveMenuItem(0x22, 0, 0x82, 0x7E, 0, Menu_Container_discardMenuCountTemplate);
        Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_DetermineDiscardAmount;
        // Fallthrough
    case Menu_Inventory_DiscardMenuState_DetermineDiscardAmount:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRright | PADRdown)) {
            i = VS::Core::Math::SplitDecimalDigit(Menu_Container_discardCount);
            Menu_Container_discardMenuCountTemplate[4] = i & 0xF;
            i = VS::Core::Math::SplitDecimalDigit(i >> 4);
            Menu_Container_discardMenuCountTemplate[3] = i & 0xF;
            Menu_Container_discardMenuCountTemplate[2] = i >> 4;
            VS::Menu::Framework::SetLiveMenuItem(0x22, 0, 0x82, 0x7E, 0, Menu_Container_discardMenuCountTemplate);
            if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
                VS::Menu::PlayLeaveSound();
                if ((Input_buttonsPressed.allButtons & PADRup)) {
                    return -2;
                }
                return -1;
            }
            if (Menu_Container_discardCount == 0) {
                VS::Menu::PlayLeaveSound();
                return -1;
            }
            VS::Menu::PlaySelectSound();
            Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardMultipleConfirm;
            break;
        }

        i = 0;
        a0 = previous_discard_count = Menu_Container_discardCount;
        if (Input_buttonRepeat & PADLup) {
            i = 1;
        }
        if (Input_buttonRepeat & PADLdown) {
            --i;
        }
        if (i == 1) {
            a0 = Menu_Container_discardCount + Menu_Container_discardSteps[Menu_Container_discardStep];
        } else if (i == -1) {
            a0 = Menu_Container_discardCount - Menu_Container_discardSteps[Menu_Container_discardStep];
        }
        if (a0 >= 0) {
            if (Menu_Container_discardMenuMaxAmount >= a0) {
                Menu_Container_discardCount = a0;
            }
        }
        i = 0;
        a0 = Menu_Container_discardStep;
        if (Input_buttonRepeat & PADLright) {
            i = -1;
        }
        if (Input_buttonRepeat & PADLleft) {
            ++i;
        }
        if (i == 1) {
            if ((a0 & 0xFF) == 2) {
                if (Menu_Container_discardCount != Menu_Container_discardMenuMaxAmount) {
                    Menu_Container_discardCount = Menu_Container_discardMenuMaxAmount;
                }
            } else {
                Menu_Container_discardStep = a0 + 1;
            }
        } else if (i == -1) {
            if (!(a0 & 0xFF)) {
                if (Menu_Container_discardCount != 0) {
                    Menu_Container_discardCount = 0;
                }
            } else {
                Menu_Container_discardStep = a0 - 1;
            }
        }
        if ((a0 != Menu_Container_discardStep) || (previous_discard_count != Menu_Container_discardCount)) {
            VS::Menu::PlayChangeSound();
        }
        VS::Render::SetLiveMenuFontStyle(5);
        packed_discard_digits = VS::Core::Math::SplitDecimalDigit(Menu_Container_discardCount);
        VS::Render::DrawLiveVariableWidthGlyph(
            packed_discard_digits & 0xF, 0x42, 0x82, (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) - 0xC));
        packed_discard_digits = VS::Core::Math::SplitDecimalDigit(packed_discard_digits >> 4);
        VS::Render::DrawLiveVariableWidthGlyph(
            packed_discard_digits & 0xF, 0x3C, 0x82, (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) - 0xC));
        VS::Render::DrawLiveVariableWidthGlyph(
            packed_discard_digits >> 4, 0x36, 0x82, (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) - 0xC));
        Menu_Container_discardMenuCursorAnimation =
            VS::Menu::Framework::DrawLiveCursor(Menu_Container_discardMenuCursorAnimation, (0x36 - (Menu_Container_discardStep * 6)) | 0x720000);
        break;
    case Menu_Inventory_DiscardMenuState_DiscardMultipleConfirm:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                discardMultipleText[i * 2] = (char*)&Menu_Framework_itemHelp
                    [Menu_Framework_itemHelp[i + Text_Menu_ITEMHELP_BIN_Index_confirmYes]];
                discardMultipleText[i * 2 + 1] =
                    (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_discardConfirm];
                discardMultipleRowTypes[i] = 0;
            }

            VS::Menu::Framework::CreateLiveActionRows(
                2, Menu_Framework_ActionsMenuAction_Discard, discardMultipleText, discardMultipleRowTypes);
            Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardMultiple;
        }
        break;
    case Menu_Inventory_DiscardMenuState_DiscardMultiple:
        VS::Menu::Framework::UpdateLiveActionMenu();
        i = VS::Menu::Framework::LiveActionSelectionResult() + 1;
        if (i != 0) {
            if (i == 1) {
                VS::Audio::LivePlaySfxCentered(0x7E, 0x1C);
                if (Menu_Container_containerData->items.misc[Menu_Container_discardMenuSelectedItem].count
                    == Menu_Container_discardCount) {
                    VS::Items::ClearLiveContainerItem(
                        Menu_Container_discardMenuItemCategory, Menu_Container_discardMenuSelectedItem, &Menu_Container_containerData->items);
                } else {
                    Menu_Container_containerData->items.misc[Menu_Container_discardMenuSelectedItem].count -= Menu_Container_discardCount;
                }
            } else {
                VS::Menu::PlayLeaveSound();
            }
            return i;
        }
        break;
    }
    return 0;
}

// decomp: MENUD.PRG _getWeaponsubItemCount (src/MENU/MENUD.PRG/234.c:1959)
int Menu_Container_CountWeaponGems(int weapon_index)
{
    int i;
    Items_InventoryWeapon* temp = &Menu_Container_containerData->items.weapons[weapon_index];
    int count = 0;

    for (i = 0; i < Menu_Container_containerData->items.grips[temp->grip - 1].gemSlots; ++i) {
        count += temp->gems[i] != 0;
    }
    return count;
}

// decomp: MENUD.PRG _getShieldsubItemCount (src/MENU/MENUD.PRG/234.c:1971)
int Menu_Container_CountShieldGems(int shield_index)
{
    int i;
    Items_InventoryShield* shield = &Menu_Container_containerData->items.shields[shield_index];
    int count = 0;

    for (i = 0; i < shield->base.gemSlots; ++i) {
        count += shield->gems[i] != 0;
    }
    return count;
}

// decomp: MENUD.PRG func_801066DC (src/MENU/MENUD.PRG/234.c:1983)
int Menu_Container_WouldExceedCapacity(int category_id, int additional_item_count)
{
    int destination_item_count = VS::Items::CountLiveContainerItems(category_id, &Menu_Container_otherStorage->items);
    int projected_item_count =
        additional_item_count + (destination_item_count + VS::Items::CountLiveContainerItems(category_id, &Menu_Framework_unusedInventoryStorage->stagedTransfer));

    if (Menu_Container_browsingGadgetBag != 0) {
        return Menu_Container_containerItemCapacities[category_id] < projected_item_count;
    }
    return Items_Inventory_inventoryItemCapacities[category_id] < projected_item_count;
}

// decomp: MENUD.PRG func_80106784 (src/MENU/MENUD.PRG/234.c:1995)
int Menu_Container_TransferCapacityError(int category_id, int item_index)
{
    int i;

    for (i = 0; i < 64; ++i) {
        if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[i] == 0) {
            break;
        }
    }

    if (category_id == 0) {
        i += 2 + Menu_Container_CountWeaponGems(item_index);
    }

    if (category_id == 3) {
        i += Menu_Container_CountShieldGems(item_index);
    }

    if (i >= 64) {
        return 1;
    }

    if (Menu_Container_WouldExceedCapacity(category_id, 1) != 0) {
        return 2;
    }

    if (category_id == 0) {
        if (Menu_Container_WouldExceedCapacity(1, 1) != 0) {
            return 2;
        }
        if (Menu_Container_WouldExceedCapacity(2, 1) != 0) {
            return 2;
        }
        if (Menu_Container_WouldExceedCapacity(5, Menu_Container_CountWeaponGems(item_index)) != 0) {
            return 2;
        }
    }

    if (category_id == 3 && Menu_Container_WouldExceedCapacity(5, Menu_Container_CountShieldGems(item_index)) != 0) {
        return 2;
    }

    return 0;
}

// decomp: MENUD.PRG func_801068BC (src/MENU/MENUD.PRG/234.c:2040)
void Menu_Container_UndoPendingTransfer(int pending_transfer_index)
{
    int staged_item_index;
    int category_id;
    int packed_transfer_item;

    packed_transfer_item = Menu_Framework_unusedInventoryStorage->itemsToTransfer[pending_transfer_index];
    category_id = packed_transfer_item >> 8;
    staged_item_index = (packed_transfer_item - 1) & 0xFF;
    VS::Items::CopyLiveContainerItem(category_id | 0x10, &Menu_Container_containerData->items, staged_item_index,
        &Menu_Framework_unusedInventoryStorage->stagedTransfer);
    VS::Items::ClearLiveContainerItem(category_id, staged_item_index, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
}

// decomp: MENUD.PRG func_80106948 (src/MENU/MENUD.PRG/234.c:2054)
void Menu_Container_StageItemTransfer(int category_id, int item_index)
{
    VS::Items::CopyLiveContainerItem(category_id | 0x10, &Menu_Framework_unusedInventoryStorage->stagedTransfer, item_index,
        &Menu_Container_containerData->items);
    VS::Items::ClearLiveContainerItem(category_id, item_index, &Menu_Container_containerData->items);
}

// decomp: MENUD.PRG func_801069B0 (src/MENU/MENUD.PRG/234.c:2061)
int Menu_Container_FindPendingTransfer(int category_id, int item_index)
{
    int i;

    for (i = 0; i < 64; ++i) {
        if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[i] == ((category_id << 8) | (item_index + 1))) {
            return i + 1;
        }
    }
    return 0;
}

// decomp: MENUD.PRG func_80106A04 (src/MENU/MENUD.PRG/234.c:2073)
void Menu_Container_AppendPendingTransfer(int category_id, int item_index)
{
    int i;
    u_short* pending_transfer_items;

    i = 0;
    pending_transfer_items = (u_short*)Menu_Framework_unusedInventoryStorage + 0x61D8;
    for (i = 0; i < 64; ++i) {
        if (pending_transfer_items[i] == 0) {
            pending_transfer_items[i] = ((category_id << 8) | (item_index + 1));
            return;
        }
    }
}

// decomp: MENUD.PRG func_80106A50 (src/MENU/MENUD.PRG/234.c:2088)
void Menu_Container_RebuildPendingTransfers(void)
{
    int j;
    int next_occupied_transfer_index;
    int i;
    int k;
    u_short* pending_transfer_items = Menu_Framework_unusedInventoryStorage->itemsToTransfer;

    for (i = 0; i < 64; ++i) {
        int source = pending_transfer_items[i];
        if ((source != 0)
            && (VS::Items::LiveContainerItemId((source >> 8), (source - 1) & 0xFF,
                    &Menu_Framework_unusedInventoryStorage->stagedTransfer)
                == 0)) {
            pending_transfer_items[i] = 0;
        }
    }

    i = 0;

    while (1) {
        if (pending_transfer_items[i] != 0) {
            ++i;
            if (i == 0x3F) {
                break;
            }
        } else {
            for (next_occupied_transfer_index = i + 1; next_occupied_transfer_index < 0x40; ++next_occupied_transfer_index) {
                if (pending_transfer_items[next_occupied_transfer_index] != 0) {
                    break;
                }
            }

            if (next_occupied_transfer_index == 0x40) {
                break;
            }

            for (j = next_occupied_transfer_index; j < 0x40; ++j) {
                pending_transfer_items[j + i - next_occupied_transfer_index] = pending_transfer_items[j];
            }

            for (j = j + (i - next_occupied_transfer_index); j < 64; ++j) {
                pending_transfer_items[j] = 0;
            }
        }
    }

    for (k = 0; k < 7; ++k) {
        for (i = 0; i < Menu_Container_containerItemCapacities[k]; ++i) {
            if ((VS::Items::LiveContainerItemId(k, i, &Menu_Framework_unusedInventoryStorage->stagedTransfer) != 0)
                && (Menu_Container_FindPendingTransfer(k, i) == 0)) {
                Menu_Container_AppendPendingTransfer(k, i);
            }
        }
    }
}

// MENUD.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Container_Storage_ResetMenudStatics(void)
{
    {
        static const u_short initial[] = { 32, 64, 64, 32, 64, 192, 256, 2 };
        memcpy((void*)&Menu_Container_containerItemCapacities, &initial, sizeof Menu_Container_containerItemCapacities);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Container_discardMenuCursorAnimation, &initial, sizeof Menu_Container_discardMenuCursorAnimation);
    }
    {
        static const char initial[] = "|>48|000/000\0";
        memcpy((void*)&Menu_Container_discardMenuCountTemplate, &initial, sizeof Menu_Container_discardMenuCountTemplate);
    }
    {
        static const char initial[] = { 1, 10, 100 };
        memcpy((void*)&Menu_Container_discardSteps, &initial, sizeof Menu_Container_discardSteps);
    }
}

}  // extern "C"
