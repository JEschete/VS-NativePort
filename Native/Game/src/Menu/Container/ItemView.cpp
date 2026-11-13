// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Menu/Container/ContainerCapacityLive.hpp"
#include "Menu/Container/ContainerDetailControllersLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Container/ContainerMenuDialogsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Content/StringTables.h"
#include "Input/ControllerLiveState.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/InventoryLiveState.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Inventory/ItemList.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: D_80109A08 (D_80109A08@src/MENU/MENUD.PRG/234.c)
char const* Menu_Container_kStorageNames[] = { "GADGET BAG", "CONTAINER" };

// decomp: D_80109A10 (D_80109A10@src/MENU/MENUD.PRG/234.c)
char const* Menu_Container_kCategoryLabels[] = { "$WEAPON", "$BLADE", "$GRIP", "$SHIELD", "$ARMOR",
    "$GEM", "$MISC" };

// decomp: local static of func_801089BC, at file scope so the module reset reaches it
static char Menu_Container_transferDirectionOkBlink = 0;

// decomp: local static of func_801089BC, at file scope so the module reset reaches it
static char Menu_Container_transferDirectionCursorAnimation = 0;

// decomp: local static of func_801089BC, at file scope so the module reset reaches it
static char Menu_Container_directionCursorResetPending = 1;

// decomp: local static of func_80107490, at file scope so the module reset reaches it
static char Menu_Container_itemActionIds[8];

// decomp: D_80109A7A (D_80109A7A@src/MENU/MENUD.PRG/234.c)
u_char Menu_Container_browsingGadgetBag;

// decomp: D_80109A7B (D_80109A7B@src/MENU/MENUD.PRG/234.c)
char Menu_Container_categoryTitleVisible;

// decomp: D_80109A7C (D_80109A7C@src/MENU/MENUD.PRG/234.c)
char Menu_Container_categoryIconsVisible;

// decomp: D_80109A7D (D_80109A7D@src/MENU/MENUD.PRG/234.c)
char Menu_Container_controlsPanelMode;

// decomp: D_80109A7E (D_80109A7E@src/MENU/MENUD.PRG/234.c)
char Menu_Container_categoryTitleSlideStep;

// decomp: D_80109A7F (D_80109A7F@src/MENU/MENUD.PRG/234.c)
char Menu_Container_categoryIconsSlideStep;

// decomp: D_80109A80 (D_80109A80@src/MENU/MENUD.PRG/234.c)
char Menu_Container_controlsPanelSlideStep;

// decomp: D_80109A81 (D_80109A81@src/MENU/MENUD.PRG/234.c)
char Menu_Container_detailsViewPhase;

// decomp: D_80109A84 (D_80109A84@src/MENU/MENUD.PRG/234.c)
int Menu_Container_detailsClosedWithTriangle;

// decomp: D_80109A88 (D_80109A88@src/MENU/MENUD.PRG/234.c)
Items_Container* Menu_Container_otherStorage;

// decomp: _menuText (_menuText@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_menuText[690]; /* contents: disc string table (ModuleData.cpp) */

// decomp: vs_menuD_containerData (vs_menuD_containerData@MENU/MENUD.PRG)
Items_Container* Menu_Container_containerData;

// decomp: MENUD.PRG _setWeaponUi (src/MENU/MENUD.PRG/234.c:810)
void Menu_Container_BuildWeaponRow(Items_ContainerData* container, char** rowStrings,
    int* rowTypes, char* buf, int index)
{
    Items_WeaponLoadout weapon;

    VS::Menu::container::BuildLiveWeaponStats(&weapon, &container->weapons[index], container);
    VS::Menu::Framework::FormatLiveWeaponRow(&weapon, rowStrings, rowTypes, buf);
    rowStrings[0] = container->weapons[index].name;
}

// decomp: MENUD.PRG _setShieldUi (src/MENU/MENUD.PRG/234.c:820)
void Menu_Container_BuildShieldRow(Items_ContainerData* container, char** rowStrings,
    int* rowTypes, char* buf, int index)
{
    Items_ShieldLoadout shield;

    VS::Menu::container::BuildLiveShieldStats(&shield, &container->shields[index], container);
    VS::Menu::Framework::FormatLiveShieldRow(&shield, rowStrings, rowTypes, buf);
}

// decomp: MENUD.PRG func_80106C64 (src/MENU/MENUD.PRG/234.c:2145)
int Menu_Container_BuildItemRows(int itemCategory, char** text, int* rowTypes, char* textBuf)
{
    int itemCount;
    u_short* indices;
    int itemIndex;
    int parentIndex;
    int i;
    int rowType;
    Items_ContainerData* container = &Menu_Framework_unusedInventoryStorage->stagedTransfer;
    char* c;
    itemCount = 0;
    c = textBuf;

    if (itemCategory == 7) {

        for (i = 0; i < 64; ++i, c += 0x60) {
            itemIndex = Menu_Framework_unusedInventoryStorage->itemsToTransfer[i];
            itemCategory = itemIndex >> 8;
            itemIndex &= 0xFF;

            if (itemIndex == 0) {
                break;
            }

            --itemIndex;
            rowType = 0;

            switch (itemCategory) {
            case 0:
                Menu_Container_BuildWeaponRow(container, &text[i * 2], &rowTypes[i], c, itemIndex);
                break;
            case 1:
                VS::Menu::Framework::FormatLiveBladeRow(
                    &container->blades[itemIndex], &text[i * 2], &rowTypes[i], c);
                break;
            case 2:
                VS::Menu::Framework::FormatLiveGripRow(
                    &container->grips[itemIndex], &text[i * 2], &rowTypes[i], c);
                break;
            case 3:
                Menu_Container_BuildShieldRow(container, &text[i * 2], &rowTypes[i], c, itemIndex);
                break;
            case 4:
                VS::Menu::Framework::FormatLiveInventoryArmorRow(
                    &container->armor[itemIndex], &text[i * 2], &rowTypes[i], c);
                break;
            case 5:
                VS::Menu::Framework::FormatLiveGemRow(
                    &container->gems[itemIndex], &text[i * 2], &rowTypes[i], c);
                if (VS::Items::LiveContainerParentShieldIndex(5, itemIndex, container) != 0) {
                    rowType = 0xCC01;
                }
                break;
            case 6:
                VS::Menu::Framework::FormatLiveMiscItemRow(
                    &container->misc[itemIndex], &text[i * 2], &rowTypes[i], c);
                break;
            }
            if (VS::Items::LiveContainerParentWeaponIndex(itemCategory, itemIndex, container) != 0) {
                rowType = 0xCC01;
            }
            ++itemCount;
            rowTypes[i] |= rowType;
        }
    } else {
        char** t;
        int* r;
        indices = VS::Items::LiveContainerCategoryIndices(itemCategory, Menu_Container_containerData);
        if (itemCategory == 6) {
            int j;
            for (i = 0, j = 0; i < 256;) {
                t = &text[j * 2];
                r = &rowTypes[j];
                itemIndex = indices[i];

                if (itemIndex == 0) {
                    break;
                }

                --itemIndex;

                VS::Menu::Framework::FormatLiveMiscItemRow(
                    &Menu_Container_containerData->items.misc[itemIndex], t, r, c);
                ++itemCount;
                ++j;
                ++i;
                c += 0x60;
            }
        } else {

            for (i = 0; i < Menu_Container_containerItemCapacities[itemCategory]; ++i, c += 0x60) {
                itemIndex = indices[i];

                if (itemIndex == 0) {
                    break;
                }

                --itemIndex;
                rowType = 0;

                switch (itemCategory) {
                case 0:
                    Menu_Container_BuildWeaponRow(&Menu_Container_containerData->items, &text[i * 2],
                        &rowTypes[i], c, itemIndex);
                    if (Menu_Container_containerData->items.weapons[itemIndex].isEquipped != 0) {
                        rowType = 0xCA00;
                    }
                    break;
                case 1:
                    VS::Menu::Framework::FormatLiveBladeRow(
                        &Menu_Container_containerData->items.blades[itemIndex], &text[i * 2],
                        &rowTypes[i], c);
                    break;
                case 2:
                    VS::Menu::Framework::FormatLiveGripRow(&Menu_Container_containerData->items.grips[itemIndex],
                        &text[i * 2], &rowTypes[i], c);
                    break;
                case 3:
                    Menu_Container_BuildShieldRow(&Menu_Container_containerData->items, &text[i * 2],
                        &rowTypes[i], c, itemIndex);
                    if (Menu_Container_containerData->items.shields[itemIndex].isEquipped != 0) {
                        rowType = 0xCA00;
                    }
                    break;
                case 4:
                    VS::Menu::Framework::FormatLiveInventoryArmorRow(
                        &Menu_Container_containerData->items.armor[itemIndex], &text[i * 2],
                        &rowTypes[i], c);
                    if (Menu_Container_containerData->items.armor[itemIndex].limb != 0) {
                        rowType = 0xCA00;
                    }
                    break;
                case 5:
                    VS::Menu::Framework::FormatLiveGemRow(&Menu_Container_containerData->items.gems[itemIndex],
                        &text[i * 2], &rowTypes[i], c);
                    break;
                }
                parentIndex = VS::Items::LiveContainerParentWeaponIndex(
                    itemCategory, itemIndex, &Menu_Container_containerData->items);
                if (parentIndex != 0) {
                    rowType = 0xCA00;
                    if (Menu_Container_containerData->items.weapons[parentIndex - 1].isEquipped
                        == 0) {
                        rowType = 0xCC00;
                    }
                }
                parentIndex = VS::Items::LiveContainerParentShieldIndex(
                    itemCategory, itemIndex, &Menu_Container_containerData->items);
                if (parentIndex != 0) {
                    rowType = 0xCA00;
                    if (Menu_Container_containerData->items.shields[parentIndex - 1].isEquipped
                        == 0) {
                        rowType = 0xCC00;
                    }
                }

                ++itemCount;
                rowTypes[i] |= rowType;
            }
        }
    }
    return itemCount;
}

// decomp: MENUD.PRG func_80107490 (src/MENU/MENUD.PRG/234.c:2393)
int Menu_Container_UpdateItemList(int transfer_direction_selection)
{
    static char Menu_Container_itemListStep;
    static char Menu_Container_categoryEmpty;
    static char Menu_Container_itemListFirstEntry;
    static char Menu_Container_itemListLayoutPadding __attribute__((unused));
    static int Menu_Container_itemListSelectedRow;

    u_short* action_row_text[8];
    int temp_a0;
    int temp_s0;
    int temp_s2;
    int temp_s3;
    int parent_item_row;
    int category_cursor_memory_offset;
    int previous_category_buttons_held;
    int var_a1;
    int var_a2;
    int selected_category_id;
    int next_category_buttons_repeated;
    int* rebuilt_row_types;
    int* pending_row_types;
    int* initial_row_types;
    char** rebuilt_row_text;
    char** pending_row_text;
    char** initial_row_text;
    u_short* temp_s1;
    void* pending_rows_allocation;
    void* rebuilt_rows_allocation;
    Menu_Framework_MenuRow* category_title_row;

loop_1:
    if (transfer_direction_selection != 0) {
        Menu_Container_itemListFirstEntry = 1;
        VS::Menu::container::SetLivePanelVisibility(1);
        Menu_Container_categoryTitleSlideStep = 0;
        if (Input_settings.cursorMemory == 0) {
            memset(Menu_Framework_menuMemory.opaqueA0, 0, sizeof Menu_Framework_menuMemory.opaqueA0);
        }
        VS::Core::Memory::ZeroLiveWordsBackward(
            &Menu_Framework_unusedInventoryStorage->stagedTransfer, sizeof Menu_Framework_unusedInventoryStorage->stagedTransfer);
        VS::Core::Memory::ZeroLiveWordsBackward(Menu_Framework_unusedInventoryStorage->itemsToTransfer,
            sizeof Menu_Framework_unusedInventoryStorage->itemsToTransfer);
        Menu_Container_browsingGadgetBag = transfer_direction_selection - 1;
        if (Menu_Container_browsingGadgetBag) {
            Menu_Container_containerData = &Menu_Framework_unusedInventoryStorage->gadgetBag;
            Menu_Container_otherStorage = &Menu_Framework_unusedInventoryStorage->container;
        } else {
            Menu_Container_containerData = &Menu_Framework_unusedInventoryStorage->container;
            Menu_Container_otherStorage = &Menu_Framework_unusedInventoryStorage->gadgetBag;
        }
        Menu_Container_itemListStep = 1;
        return 0;
    }
    selected_category_id = (Menu_Framework_menuMemory.opaqueA0[0] - 1) & 7;
    temp_s1 = VS::Items::LiveContainerCategoryIndices(selected_category_id, Menu_Container_containerData);

    switch (Menu_Container_itemListStep) { /* switch 1 */
    case 0:
        VS::Menu::Framework::InitializeLiveInformationBox();
        /* fallthrough */
    case 1:
        temp_s1 = (u_short *)(VS::Core::Memory::AllocateLiveFromHighEnd(0x6C00));
        initial_row_types = (int *)(((u_char*)((void*)temp_s1)) + 0x6000);
        initial_row_text = (char **)(((u_char*)((void*)temp_s1)) + 0x6400);
        Menu_Inventory_freezeTabArrows = 1;

        for (temp_s3 = 0; temp_s3 < 7; ++temp_s3) {
            VS::Items::RebuildLiveContainerCategoryIndices(temp_s3, Menu_Container_containerData);
        }

        Menu_Container_RebuildPendingTransfers();
        temp_s2 = Menu_Container_BuildItemRows(selected_category_id, initial_row_text, initial_row_types, (char*)temp_s1);
        Menu_Container_categoryEmpty = temp_s2 == 0;
        if (Menu_Container_categoryEmpty != 0) {
            VS::Menu::Framework::SetLiveInformationMessage(
                (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag + 27 + (selected_category_id == 7) * 2]]);
        } else {
            temp_s3 = Input_settings.cursorMemory;
            if (Menu_Container_itemListFirstEntry == 0) {
                Input_settings.cursorMemory = 1;
            }
            Menu_Container_itemListFirstEntry = 0;
            VS::Menu::Framework::CreateLiveMenuRows(temp_s2, (selected_category_id + 0x51) | 0x19200, initial_row_text, initial_row_types);
            if (Menu_Container_itemListStep == 0) {
                VS::Menu::container::RestoreLiveItemRows(selected_category_id);
            }
            Input_settings.cursorMemory = temp_s3;
        }
        VS::Core::Memory::FreeLiveFromHighEnd(temp_s1);
        Menu_Container_itemListStep = 2;
        break;
    case 2:
        Menu_Container_controlsPanelMode = 1;
        if (Menu_Container_categoryEmpty != 0) {
            if (Input_buttonsPressed.allButtons & 0x10) {
                VS::Menu::PlayLeaveSound();
                return -2;
            }
            if (Input_buttonsPressed.allButtons & 0x40) {
                VS::Menu::PlayLeaveSound();
                if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[0] == 0) {
                    return -1;
                }
                if (selected_category_id == 7) {
                    VS::Menu::container::UpdateLiveTransferConfirmation(1);
                    Menu_Container_itemListStep = 0xF;
                } else {
                    Menu_Framework_menuMemory.opaqueA0[0] = 0;
                    VS::Menu::Framework::ClearLiveMenuExcept(0);
                    Menu_Container_itemListStep = 0xD;
                }
                break;
            }
            if (Input_buttonsPressed.allButtons & 0xA0) {
                VS::Menu::PlayInvalidSound();
            }
        } else {
            int details_index_bias = 1;
            temp_s0 = selected_category_id < 6;
            Menu_Container_controlsPanelMode = temp_s0 + 1;
            Menu_Container_itemListSelectedRow = VS::Menu::Framework::LiveSelectedRow() + 1;
            if (Menu_Container_itemListSelectedRow == 0) {
                if (Input_buttonsPressed.allButtons & 0x80) {
                    if ((temp_s0 != 0) && (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0)) {
                        VS::Menu::PlaySelectSound();
                        temp_s0 = VS::Menu::Framework::CloseLiveRowsAndRememberCursor();
                        VS::Menu::Framework::LiveMenuItem(temp_s0 >> 8)->outsetIcon = 0;
                        VS::Menu::container::UpdateLiveItemDetails(selected_category_id | ((temp_s0 + details_index_bias) * 0x10));
                        Menu_Container_categoryTitleVisible = 0;
                        Menu_Container_itemListStep = 7;
                        break;
                    }
                    VS::Menu::PlayInvalidSound();
                }
            } else if (Menu_Container_itemListSelectedRow > 0) {
                if (selected_category_id == 7) {
                    VS::Menu::container::UndoLivePendingTransfer(Menu_Container_itemListSelectedRow - 1);
                    VS::Items::RefreshLiveMiscTransferCount();
                    VS::Menu::Framework::ClearLiveMenuExcept(0);
                    Menu_Container_itemListStep = 4;
                } else {
                    rebuilt_rows_allocation = VS::Core::Memory::AllocateLiveFromHighEnd(0x6C00U);
                    rebuilt_row_text = (char **)(((u_char*)(rebuilt_rows_allocation)) + 0x6400);
                    rebuilt_row_types = (int *)(((u_char*)(rebuilt_rows_allocation)) + 0x6000);
                    temp_s0 = Menu_Container_BuildItemRows(selected_category_id, rebuilt_row_text, rebuilt_row_types, (char *)(rebuilt_rows_allocation));
                    rebuilt_row_types[Menu_Container_itemListSelectedRow - 1] |= 8;
                    temp_s3 = Input_settings.cursorMemory;
                    Input_settings.cursorMemory = 1;
                    VS::Menu::Framework::CreateLiveMenuRows(
                        temp_s0, (selected_category_id + 0x51) | 0x19200, rebuilt_row_text, rebuilt_row_types);
                    Input_settings.cursorMemory = temp_s3;
                    VS::Menu::container::RestoreLiveItemRows(selected_category_id);
                    Menu_Container_controlsPanelMode = 1;
                    Menu_Inventory_freezeTabArrows = 0;
                    VS::Core::Memory::FreeLiveFromHighEnd(rebuilt_rows_allocation);
                    Menu_Container_itemListStep = 6;
                }
            } else if (Menu_Container_itemListSelectedRow == -2) {
                return -2;
            } else {
                if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[0] == 0) {
                    VS::Menu::Framework::ClearLiveMenuExcept(0);
                    return -1;
                }
                if (selected_category_id != 7) {
                    Menu_Framework_menuMemory.opaqueA0[0] = 0;
                    VS::Menu::Framework::ClearLiveMenuExcept(0);
                    Menu_Container_itemListStep = 0xD;
                } else {
                    VS::Menu::container::UpdateLiveTransferConfirmation(1);
                    Menu_Container_itemListStep = 0xF;
                }
                break;
            }
        }
        // fallthrough
    case 3:
        temp_s3 = Input_buttonsState & 0x8004;
        temp_s2 = Input_buttonsState & 0x2008;
        previous_category_buttons_held = (temp_s3 != 0);
        if (temp_s2 != 0) {
            temp_s0 = previous_category_buttons_held ^ 1;
        } else {
            temp_s0 = previous_category_buttons_held & 1;
        }
        if (temp_s0 != 0) {
            var_a2 = 0x8004;
            if (temp_s3 == 0x8004) {
                var_a2 = 0x8000;
            }
            if (Input_buttonRepeat & var_a2) {
                selected_category_id = (selected_category_id - 1) & 7;
            }
            var_a2 = 0x2008;
            if (temp_s2 == 0x2008) {
                var_a2 = 0x2000;
            }
            next_category_buttons_repeated = Input_buttonRepeat & var_a2;
            if (next_category_buttons_repeated != 0) {
                selected_category_id = (selected_category_id + 1) & 7;
            }
            if (selected_category_id != ((Menu_Framework_menuMemory.opaqueA0[0] - 1) & 7)) {
                VS::Menu::PlayChangeSound();
                Menu_Framework_menuMemory.opaqueA0[0] = (selected_category_id + 1) & 7;
                if (Menu_Container_itemListStep == 2) {
                    if (Menu_Container_categoryEmpty == 0) {
                        VS::Menu::Framework::CloseLiveRowsAndRememberCursor();
                        VS::Menu::Framework::ClearLiveMenuExcept(0);
                    }
                    Menu_Container_itemListStep = 3;
                }
            }
        }
        if (Menu_Container_itemListStep == 3) {
            if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
                if (Input_buttonsState & 0xA00C) {
                    if (temp_s0 != 0) {
                        break;
                    }
                }
                Menu_Container_itemListStep = 1;
                transfer_direction_selection = 0;
                goto loop_1;
            }
        }
        break;
    case 4:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            Menu_Container_itemListStep = 1;
        }
        break;
    case 5:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            Menu_Container_itemListStep = 0;
        }
        break;
    case 6:
        if (Input_buttonsPressed.allButtons & 0x80) {
            VS::Menu::PlayInvalidSound();
        }
        temp_s3 = VS::Menu::Framework::LiveSelectedRow() + 1;
        if (temp_s3 != 0) {
            temp_s2 = Menu_Container_itemListSelectedRow;
            if (temp_s2 == temp_s3) {
                Menu_Container_itemListStep = 8;
                break;
            }
            if (temp_s3 > 0) {
                temp_s0 = temp_s1[temp_s3 - 1];
                var_a2 = temp_s1[temp_s2 - 1];
                if ((selected_category_id == 6)
                    && (Menu_Container_containerData->items.misc[temp_s0 - 1].id
                        == Menu_Container_containerData->items.misc[var_a2 - 1].id)) {
                    Menu_Container_containerData->items.misc[var_a2 - 1].count +=
                        Menu_Container_containerData->items.misc[temp_s0 - 1].count;
                    if (Menu_Container_containerData->items.misc[var_a2 - 1].count > 0x64) {
                        Menu_Container_containerData->items.misc[temp_s0 - 1].count =
                            Menu_Container_containerData->items.misc[var_a2 - 1].count - 0x64;
                        Menu_Container_containerData->items.misc[var_a2 - 1].count = 0x64;
                    } else {
                        VS::Items::ClearLiveContainerItem(
                            6, temp_s0 - 1, &Menu_Container_containerData->items);
                    }
                } else {
                    temp_s1[temp_s3 - 1] = var_a2;
                    temp_s1[temp_s2 - 1] = temp_s0;
                }
                VS::Menu::Framework::ClearLiveMenuExcept(0);
                Menu_Container_itemListStep = 4;
                break;
            }
            if (temp_s3 != -1) {
                return -2;
            }
            Menu_Container_itemListStep = 0;
        }
        break;
    case 7:
        temp_s3 = VS::Menu::container::UpdateLiveItemDetails(0);
        if (temp_s3 != 0) {
            if (temp_s3 == -2) {
                return -2;
            }
            Menu_Container_categoryTitleVisible = 1;
            category_title_row = VS::Menu::Framework::LiveMenuItem(0);
            if (category_title_row->x >= 0xB5) {
                if (category_title_row->x >= 0xCD) {
                    category_title_row->x = (u_short)category_title_row->x - 0x18;
                }
                category_title_row->state = 2;
                category_title_row->targetPosition0 = 0xB4;
            }
            Menu_Container_itemListStep = 1;
        }
        break;
    case 8:
        Menu_Container_controlsPanelMode = 0;
        temp_s3 = temp_s1[Menu_Container_itemListSelectedRow - 1] - 1;
        VS::Core::Memory::ZeroLiveWordsBackward(Menu_Framework_rowTypeBuf, 0x10);
        temp_s0 = 0;
        temp_s2 = 0x33;
        switch (selected_category_id) {
        case 0:
            temp_s0 = Menu_Container_containerData->items.weapons[temp_s3].isEquipped;
            temp_s2 = 0x4A;
            break;
        case 3:
            temp_s0 = Menu_Container_containerData->items.shields[temp_s3].isEquipped;
            temp_s2 = 0x4A;
            break;
        case 4:
            temp_s0 = Menu_Container_containerData->items.armor[temp_s3].limb;
            temp_s2 = 0x4A;
            break;
        case 6:
            temp_s0 = (Menu_Container_containerData->items.misc[temp_s3].id < 0x1CA) ^ 1;
            break;
        }

        var_a2 = VS::Items::LiveContainerParentWeaponIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items);
        var_a1 = 8;

        if (var_a2 != 0) {
            temp_s0 = 1;
            temp_s2 =
                Menu_Container_menuText[Menu_Container_containerData->items.weapons[var_a2 - 1].isEquipped == 0
                              ? 5
                              : 4];
        }

        var_a2 = VS::Items::LiveContainerParentShieldIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items);
        var_a1 = 8;

        if (var_a2 != 0) {
            temp_s0 = 1;
            temp_s2 =
                Menu_Container_menuText[Menu_Container_containerData->items.shields[var_a2 - 1].isEquipped == 0
                              ? 5
                              : 4];
        }

        var_a2 = temp_s0;

        if (temp_s0 != 0) {
            action_row_text[1] = &Menu_Container_menuText[temp_s2];
        } else if (selected_category_id == 6) {
            int temp_s0 = 0;

            for (temp_s2 = 0; temp_s2 < 7; ++temp_s2) {
                temp_s0 +=
                    VS::Items::CountLiveContainerItems(temp_s2, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
            }

            VS::Items::PreviewLiveTransferredMisc();
            var_a2 = VS::Items::CopyLiveContainerItem(6, &Menu_Framework_unusedInventoryStorage->transferPreview, temp_s3,
                &Menu_Container_containerData->items);
            if ((Menu_Container_browsingGadgetBag == 0) && (var_a2 > 0x40)) {
                var_a2 = 0;
            }
            temp_s2 = 0xAB;
            if (var_a2 != 0) {
                if (temp_s0 == 0x40) {
                    int i;
                    int temp_a0 = Menu_Container_containerData->items.misc[temp_s3].count;
                    for (i = 0; i < 256; ++i) {
                        if (Menu_Framework_unusedInventoryStorage->stagedTransfer.misc[i].id
                            == Menu_Container_containerData->items.misc[temp_s3].id) {
                            temp_a0 +=
                                Menu_Framework_unusedInventoryStorage->stagedTransfer.misc[i].count - 0x64;
                        }
                    }
                    var_a2 = temp_a0 < 1;
                    temp_s2 = 0x82;
                }
            }
            var_a2 = var_a2 == 0;
            action_row_text[1] = &Menu_Container_menuText[temp_s2];
        } else {
            var_a2 = VS::Menu::container::LiveTransferCapacityError(selected_category_id, temp_s3);
            action_row_text[1] = &Menu_Container_menuText[Menu_Container_menuText[var_a2 + 5]];
        }

        action_row_text[0] = &Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag * 2 + 0x1F]];

        if (var_a2 != 0) {
            Menu_Framework_rowTypeBuf[0] = 1;
        } else {
            action_row_text[1] = &Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag * 2 + 0x20]];
        }
        Menu_Container_itemActionIds[1] = 1;
        temp_s2 = 1;
        if (((u_int)(selected_category_id - 1) < 2) || ((selected_category_id == 5))) {
            action_row_text[2] = Menu_Framework_itemHelp + 0x3527;
            action_row_text[3] = Menu_Framework_itemHelp + 0x352C;
            if ((VS::Items::LiveContainerParentWeaponIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items) == 0)
                && (VS::Items::LiveContainerParentShieldIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items)
                    == 0)) {
                action_row_text[3] = Menu_Framework_itemHelp + 0x35B2;
                Menu_Framework_rowTypeBuf[1] = 1;
            }
            ++temp_s2;
            Menu_Container_itemActionIds[temp_s2] = 2;
        }
        temp_s0 = 0;
        action_row_text[temp_s2 * 2] = Menu_Framework_itemHelp + 0x354A;
        action_row_text[temp_s2 * 2 + 1] = Menu_Framework_itemHelp + 0x354F;
        switch (selected_category_id) {
        case 0:
            temp_s0 = Menu_Container_containerData->items.weapons[temp_s3].isEquipped != 0;
            break;
        case 3:
            temp_s0 = Menu_Container_containerData->items.shields[temp_s3].isEquipped != 0;
            break;
        case 4:
            temp_s0 = Menu_Container_containerData->items.armor[temp_s3].limb != 0;
            break;
        case 6:
            if (Menu_Container_containerData->items.misc[temp_s3].id >= 0x1CA) {
                temp_s0 = 3;
            }
            break;
        }
        var_a2 = VS::Items::LiveContainerParentWeaponIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items);
        if (var_a2 != 0) {
            temp_s0 = 1;
            if (Menu_Container_containerData->items.weapons[var_a2 - 1].isEquipped == 0) {
                temp_s0 = 2;
            }
        }
        var_a2 = VS::Items::LiveContainerParentShieldIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items);
        if (var_a2 != 0) {
            temp_s0 = 1;
            if (Menu_Container_containerData->items.shields[var_a2 - 1].isEquipped == 0) {
                temp_s0 = 2;
            }
        }
        if (temp_s0 != 0) {
            action_row_text[temp_s2 * 2 + 1] =
                &Menu_Framework_itemHelp[Menu_Framework_itemHelp[temp_s0 + 663]];
            temp_s0 = 1;
        }
        Menu_Framework_rowTypeBuf[temp_s2++] = temp_s0;
        Menu_Container_itemActionIds[temp_s2] = 3;
        if (selected_category_id == 6) {
            action_row_text[temp_s2 * 2] = Menu_Framework_itemHelp + 0x356D;
            action_row_text[temp_s2 * 2 + 1] = Menu_Framework_itemHelp + 0x3574;
            if (Menu_Container_containerData->items.misc[temp_s3].id >= 0x1CA) {
                action_row_text[temp_s2 * 2 + 1] = Menu_Framework_itemHelp + 0x350E;
                Menu_Framework_rowTypeBuf[temp_s2] = 1;
            }
            ++temp_s2;
            Menu_Container_itemActionIds[temp_s2] = 4;
        }

        action_row_text[temp_s2 * 2] = Menu_Framework_itemHelp + 0x3586;
        action_row_text[temp_s2 * 2 + 1] = Menu_Framework_itemHelp + 0x358A;
        ++temp_s2;
        Menu_Container_itemActionIds[temp_s2] = 5;
        VS::Menu::Framework::CreateLiveActionRows(
            temp_s2, selected_category_id + 0x59, (char**)&action_row_text, (int*)Menu_Framework_rowTypeBuf);
        Menu_Container_itemListStep = 9;
        break;
    case 9:
        VS::Menu::Framework::UpdateLiveInformationVisibility(
            VS::Menu::Framework::LiveHighlightedActionRow(), Menu_Framework_actionMenuState);
        VS::Menu::Framework::UpdateLiveActionMenu();
        temp_s3 = VS::Menu::Framework::LiveActionSelectionResult() + 1;
        if (temp_s3 != 0) {
            VS::Menu::Framework::FlyOutLiveRowsLeftExcept(0x28);
            if (temp_s3 > 0) {
                VS::Menu::PlaySelectSound();
                switch (Menu_Container_itemActionIds[temp_s3]) {
                case 1:
                    VS::Menu::Framework::InitializeLiveInformationBox();
                    VS::Menu::container::StageLiveItemTransfer(selected_category_id, VS::Items::LiveContainerCategoryIndices(selected_category_id,
                                              Menu_Container_containerData)[Menu_Container_itemListSelectedRow - 1]
                                              - 1);
                    VS::Menu::Framework::ClearLiveMenuExcept(0);
                    Menu_Container_itemListStep = 4;
                    break;
                case 2:
                    VS::Menu::Framework::InitializeLiveInformationBox();
                    VS::Menu::Framework::ClearLiveMenuExcept(0);
                    Menu_Container_itemListStep = 10;
                    break;
                case 3:
                    temp_s2 = temp_s1[Menu_Container_itemListSelectedRow - 1] | (selected_category_id << 0xC);
                    if ((selected_category_id == 6)
                        && (Menu_Container_containerData->items.misc[temp_s1[Menu_Container_itemListSelectedRow - 1] - 1]
                                .count
                            >= 2)) {
                        temp_s2 |= 0x10000;
                    }
                    VS::Menu::container::UpdateLiveDiscardMenu(temp_s2);
                    Menu_Container_itemListStep = 0xB;
                    break;
                case 4:
                    VS::Menu::container::UpdateLiveDiscardMenu(temp_s1[Menu_Container_itemListSelectedRow - 1] | (selected_category_id << 0xC) | 0x20000);
                    Menu_Container_itemListStep = 0xB;
                    break;
                case 5:
                    VS::Menu::Framework::InitializeLiveInformationBox();
                    VS::Menu::container::UpdateLiveSortMenu(selected_category_id + 1);
                    Menu_Container_itemListStep = 0xC;
                    break;
                }
                break;
            }
            VS::Menu::PlayLeaveSound();
            if (temp_s3 == -2) {
                VS::Menu::Framework::DismissLiveInformationBox();
                return -2;
            }
            Menu_Container_itemListStep = 0;
        }
        break;
    case 10:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            temp_s3 = VS::Items::LiveContainerParentWeaponIndex(
                selected_category_id, temp_s1[Menu_Container_itemListSelectedRow - 1] - 1, &Menu_Container_containerData->items);
            if (temp_s3 != 0) {
                selected_category_id = 0;
            } else {
                temp_s3 = VS::Items::LiveContainerParentShieldIndex(
                    selected_category_id, temp_s1[Menu_Container_itemListSelectedRow - 1] - 1, &Menu_Container_containerData->items);
                selected_category_id = 3;
            }
            temp_s2 = VS::Items::CountLiveContainerItems(selected_category_id, &Menu_Container_containerData->items);
            parent_item_row = VS::Items::FindLiveContainerItemRow(selected_category_id, temp_s3 - 1,
                VS::Items::LiveContainerCategoryIndices(selected_category_id, Menu_Container_containerData));
            temp_s3 = parent_item_row - 1;
            if ((temp_s2 < 9) || (temp_s3 < 8)) {
                temp_s2 = 0;
            } else {
                temp_a0 = (temp_s2 - 8);
                if (temp_a0 < temp_s3) {
                    temp_s2 = temp_a0;
                    temp_s3 -= temp_s2;
                } else {
                    temp_s2 = parent_item_row - 2;
                    temp_s3 = 1;
                }
            }
            Menu_Framework_menuMemory.opaqueA0[0] = (selected_category_id + 1) & 7;
            category_cursor_memory_offset = (selected_category_id + 0x51) * 2;
            Menu_Framework_menuMemory.cursorMemories[category_cursor_memory_offset] = temp_s3;
            Menu_Framework_menuMemory.cursorMemories[category_cursor_memory_offset + 1] = temp_s2;
            Menu_Container_itemListStep = 1;
        }
        break;
    case 11:
        temp_s3 = VS::Menu::container::UpdateLiveDiscardMenu(0);
        if (temp_s3 != 0) {
            VS::Menu::Framework::InitializeLiveInformationBox();
            VS::Menu::Framework::FlyOutLiveRowsLeftExcept(0x28);
            VS::Menu::Framework::ClearLiveMenuExcept(0);
            if (temp_s3 == -2) {
                return -2;
            }
            Menu_Container_itemListStep = 4;
        }
        break;
    case 12:
        temp_s3 = VS::Menu::container::UpdateLiveSortMenu(0);
        if (temp_s3 != 0) {
            VS::Menu::Framework::FlyOutLiveRowsLeftExcept(0x28);
            VS::Menu::Framework::ClearLiveMenuExcept(0);
            if (temp_s3 == -2) {
                return -2;
            }
            Menu_Container_itemListStep = 4;
        }
        break;
    case 13:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            pending_rows_allocation = VS::Core::Memory::AllocateLiveFromHighEnd(0x6C00U);
            pending_row_text = (char **)(((u_char*)(pending_rows_allocation)) + 0x6400);
            pending_row_types = (int *)(((u_char*)(pending_rows_allocation)) + 0x6000);
            temp_s2 = Menu_Container_BuildItemRows(7, pending_row_text, pending_row_types, (char *)(pending_rows_allocation));
            Menu_Container_categoryEmpty = temp_s2 == 0;
            if (Menu_Container_categoryEmpty != 0) {
                VS::Menu::Framework::SetLiveInformationMessage(
                    (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag + 0x1D]]);
            } else {
                VS::Menu::Framework::CreateLiveMenuRows(
                    temp_s2, (selected_category_id + 0x51) | 0x19200, pending_row_text, pending_row_types);
            }
            VS::Core::Memory::FreeLiveFromHighEnd(pending_rows_allocation);
            Menu_Container_itemListStep = 0xE;
        }
        break;
    case 14:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            VS::Menu::Framework::CloseLiveRowsAndRememberCursor();
            VS::Menu::container::UpdateLiveTransferConfirmation(1);
            Menu_Container_itemListStep = 0xF;
        }
        break;
    case 15:
        temp_s3 = VS::Menu::container::UpdateLiveTransferConfirmation(0);
        if (temp_s3 != 0) {
            VS::Menu::Framework::InitializeLiveInformationBox();
            if (temp_s3 < 0) {
                if (temp_s3 == -2) {
                    return -2;
                } else {
                    Menu_Container_itemListStep = 5;
                    break;
                }
            } else {
                // Pointless check and duplicated loops
                if (Menu_Container_browsingGadgetBag != 0) {
                    while ((temp_s2 = Menu_Framework_unusedInventoryStorage->itemsToTransfer[0])) {
                        VS::Items::CopyLiveContainerItem((temp_s2 >> 8) | 0x10, &Menu_Container_otherStorage->items,
                            (temp_s2 - 1) & 0xFF, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
                        VS::Items::ClearLiveContainerItem((temp_s2 >> 8), (temp_s2 - 1) & 0xFF,
                            &Menu_Framework_unusedInventoryStorage->stagedTransfer);
                        Menu_Container_RebuildPendingTransfers();
                    }
                } else {
                    while ((temp_s2 = Menu_Framework_unusedInventoryStorage->itemsToTransfer[0])) {
                        VS::Items::CopyLiveContainerItem((temp_s2 >> 8) | 0x10, &Menu_Container_otherStorage->items,
                            (temp_s2 - 1) & 0xFF, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
                        VS::Items::ClearLiveContainerItem((temp_s2 >> 8), (temp_s2 - 1) & 0xFF,
                            &Menu_Framework_unusedInventoryStorage->stagedTransfer);
                        Menu_Container_RebuildPendingTransfers();
                    }
                }
                for (temp_s3 = 0; temp_s3 < 7; ++temp_s3) {
                    VS::Items::RebuildLiveContainerCategoryIndices(temp_s3, Menu_Container_otherStorage);
                }
                VS::Menu::Framework::ClearLiveMenuExcept(0);
                Menu_Container_itemListStep = 17;
            }
        }
        break;
    case 17:
        return VS::Menu::Framework::AreLiveMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG func_801089BC (src/MENU/MENUD.PRG/234.c:3037)
int Menu_Container_UpdateTransferDirection(int initialize_direction_menu)
{
    static char Menu_Container_transferDirectionLayoutPadding __attribute__((unused)) = 1;

    static char Menu_Container_transferDirectionStep;
    static char Menu_Container_transferDirectionLayoutPadding2[3] __attribute__((unused));

    int i;
    int selected_direction;
    Menu_Framework_MenuRow* selected_direction_row;
    int v;
    char* direction_help_text;

    if (initialize_direction_menu != 0) {
        if (Menu_Container_directionCursorResetPending != 0) {
            Menu_Container_directionCursorResetPending = 0;
            if (Input_settings.cursorMemory == 0) {
                Menu_Framework_menuMemory.opaque9E = 0;
            }
        }
        Menu_Container_transferDirectionOkBlink = 0;
        Menu_Container_transferDirectionStep = 0;
        return 0;
    }

    switch (Menu_Container_transferDirectionStep) {
    case 0:
    case 1:
        selected_direction_row =
            VS::Menu::Framework::SetLiveMenuItem(Menu_Container_transferDirectionStep + 0xA, 0x140, (Menu_Container_transferDirectionStep * 0x10) + 0x22,
                0x7E, 0, (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_transferDirectionStep * 2 + 12]]);
        selected_direction_row->state = 2;
        selected_direction_row->targetPosition0 = 0xC2;
        ++Menu_Container_transferDirectionStep;
        break;
    case 2:
        Menu_Container_transferDirectionStep += VS::Menu::Framework::AreLiveMenuRowsIdle();
        break;
    case 3:
        selected_direction = Menu_Framework_menuMemory.opaque9E;

        for (i = 0; i < 2; ++i) {
            VS::Menu::Framework::LiveMenuItem(i + 0xA)->selected = i == selected_direction;
        }
        if (Input_buttonsPressed.allButtons & 0x10) {
            VS::Menu::PlayLeaveSound();
            VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
            return -2;
        }
        if (Input_buttonsPressed.allButtons & 0x40) {
            VS::Menu::PlayLeaveSound();
            return -1;
        }
        if (Input_buttonsPressed.allButtons & 0x20) {
            VS::Menu::PlaySelectSound();
            return selected_direction + 1;
        }
        Menu_Container_transferDirectionOkBlink += 16;
        VS::Render::DrawLiveCompactTextColor("OK", 0x4200C8,
            ((VS::Core::Math::Sine((Menu_Container_transferDirectionOkBlink) * 8) >> 5) + 0x40) * 0x10101, (u_long *)(((u_char*)(Render_frameScratch.orderingTable)) - 0xC));
        v = 0x1A;
        Menu_Container_transferDirectionCursorAnimation = VS::Menu::Framework::DrawLiveCursor(
            Menu_Container_transferDirectionCursorAnimation, (((selected_direction * 0x10) - (((selected_direction * 2) & 4) - v)) << 0x10) | 0xB4);
        if (Input_buttonRepeat & 0x1000) {
            selected_direction += 2;
        }
        if (Input_buttonRepeat & 0x4000) {
            ++selected_direction;
        }
        if (selected_direction >= 3) {
            selected_direction -= 3;
        }
        if (selected_direction != ((u_short*)&Menu_Framework_menuMemory)[0x4F]) {
            VS::Menu::PlayChangeSound();
            ((u_short*)&Menu_Framework_menuMemory)[0x4F] = selected_direction;
        }
        if (selected_direction != 2) {
            direction_help_text = (char*)&Menu_Container_menuText[Menu_Container_menuText[selected_direction * 2 + 13]];
        } else {
            direction_help_text = (char*)&Menu_Container_menuText[Text_Menud_menuText_Offset_endTransfer];
        }
        VS::Menu::Framework::SetLiveInformationMessage(direction_help_text);
        break;
    }
    VS::Menu::container::DrawLiveStorageCapacities();
    return 0;
}

// decomp: src/MENU/MENUD.PRG/234.c:3128
#define vs_getXY_4(x, y) (((x)) + ((y) << 16))
// decomp: MENUD.PRG vs_menuD_exec (src/MENU/MENUD.PRG/234.c:3130)
int Menu_Container_UpdateScreen(u_char* screen_step)
{
    static u_int Menu_Container_screenResult;
    static char Menu_Container_directionReopenDelay;
    static u_char Menu_Container_capacityListSlideStep;

    int projected_category_count;
    int control_icon_x;
    int destination_category_count;
    int i;
    int packed_capacity_row_position;
    int category_capacity;
    int capacity_slide_offset;
    Menu_Framework_MenuRow* menuItem;

    Menu_Framework_weapons = Menu_Container_containerData->items.weapons;
    Menu_Framework_blades = Menu_Container_containerData->items.blades;
    Menu_Framework_grips = Menu_Container_containerData->items.grips;
    Menu_Framework_shields = Menu_Container_containerData->items.shields;
    Menu_Framework_armor = Menu_Container_containerData->items.armor;
    Menu_Framework_gems = Menu_Container_containerData->items.gems;

    switch (*screen_step) {
    case 0:
        VS::Menu::Framework::LoadLiveItemText(1);
        VS::Menu::Framework::InitializeLiveInformationBox();
        VS::Menu::container::SetLivePanelVisibility(0);
        VS::Core::Memory::CopyLiveHalfwordBlocks(&Menu_Framework_unusedInventoryStorage->container.items,
            &Menu_Framework_unusedInventoryStorage->storedContainer,
            sizeof Menu_Framework_unusedInventoryStorage->container.items);
        VS::Items::SnapshotLiveGadgetBag();
        Menu_Container_capacityListSlideStep = 0;
        Menu_Container_directionReopenDelay = 0;
        Menu_Container_categoryTitleSlideStep = 0;
        Menu_Container_categoryIconsSlideStep = 0;
        Menu_Container_controlsPanelSlideStep = 0;
        *screen_step = 1;
        return 0;
    case 1:
        *screen_step += VS::Menu::Framework::LoadLiveItemText(0);
        break;
    case 2:
        if (Menu_Container_directionReopenDelay != 0) {
            --Menu_Container_directionReopenDelay;
        } else {
            Menu_Container_UpdateTransferDirection(1);
            *screen_step = 3;
        }
        break;
    case 3:
        Menu_Container_screenResult = Menu_Container_UpdateTransferDirection(0);
        if (Menu_Container_screenResult != 0) {
            if (Menu_Container_screenResult < 3) {
                VS::Menu::Framework::ClearLiveMenuExcept(0);
                VS::Menu::Framework::SetLiveMenuItem(0, 0xB4, 0x12, 0x8C, 8,
                    (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_screenResult + 0xF]])
                    ->selected = 1;
                *screen_step = 4;
            } else {
                *screen_step = 6;
            }
        }
        break;
    case 4:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            Menu_Container_UpdateItemList(Menu_Container_screenResult);
            *screen_step = 5;
        }
        break;
    case 5:
        Menu_Container_screenResult = Menu_Container_UpdateItemList(0);
        if (Menu_Container_screenResult != 0) {
            VS::Menu::container::SetLivePanelVisibility(0);
            if (Menu_Container_screenResult == -2) {
                *screen_step = 6;
            } else {
                VS::Menu::Framework::SetLiveMenuItem(0, 0xB4, 0x12, 0x8C, 8,
                    (char*)&Menu_Container_menuText[Text_Menud_menuText_Offset_container])
                    ->selected = 1;
                Menu_Container_directionReopenDelay = 5;
                *screen_step = 2;
            }
        }
        break;
    case 6:
        if (VS::Menu::Framework::UnloadLiveItemTextWhenReady() != 0) {
            VS::Items::CommitLiveGadgetBag();
            for (i = 0; i < 7; ++i) {
                VS::Items::RebuildLiveContainerCategoryIndices(i, &Menu_Framework_unusedInventoryStorage->container);
            }
            for (i = 0; i < 7; ++i) {
                if (VS::Items::CountLiveContainerItems(i, &Menu_Framework_unusedInventoryStorage->container.items)
                    != 0) {
                    break;
                }
            }
            Input_settings.containerEmpty = i == 7;
            VS::Menu::Framework::ClearLiveMenuExcept(0);
            Menu_Framework_menuState.currentMenu = Menu_Framework_menuState.returnState;
            Menu_Framework_menuState.returnState = Menu_Container_screenResult + 2;
            *screen_step = 0;
            return 0;
        }
        break;
    }

    if (Menu_Container_categoryTitleVisible != 0) {
        if (Menu_Container_categoryTitleSlideStep < 10) {
            ++Menu_Container_categoryTitleSlideStep;
        }
        i = Menu_Framework_rowAnimationSteps[0xA - Menu_Container_categoryTitleSlideStep];
    } else {
        if (Menu_Container_categoryTitleSlideStep != 0) {
            --Menu_Container_categoryTitleSlideStep;
        }
        i = (10 - Menu_Container_categoryTitleSlideStep) << 5;
    }
    if (Menu_Container_categoryTitleSlideStep != 0) {
        if (Menu_Container_detailsViewPhase == 0) {
            if (*screen_step != 6) {
                int itemCategory = ((Menu_Framework_menuMemory.opaqueA0[0] - 1) & 7);
                menuItem = VS::Menu::Framework::SetLiveMenuItem(0x1F, i + 0xB4, 0x22, 0x8C, 8,
                    (char*)&Menu_Container_menuText[Menu_Container_menuText[itemCategory + 0x12
                                                + (itemCategory == 7
                                                    && Menu_Container_browsingGadgetBag != 0)]]);
                menuItem->selected = 1;
                if (itemCategory == 7) {
                    for (i = 0; i < 64; ++i) {
                        if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[i] == 0) {
                            break;
                        }
                    }
                    menuItem->max = 64;
                    menuItem->count = i;
                } else {
                    menuItem->max = Menu_Container_browsingGadgetBag != 0
                                      ? Items_Inventory_inventoryItemCapacities[itemCategory]
                                      : Menu_Container_containerItemCapacities[itemCategory];
                    menuItem->count =
                        VS::Items::CountLiveContainerItems(itemCategory, &Menu_Container_containerData->items);
                }
            }
        }
    }

    if (Menu_Container_categoryIconsVisible != 0) {
        if (Menu_Container_categoryIconsSlideStep < 4) {
            ++Menu_Container_categoryIconsSlideStep;
        }
    } else if (Menu_Container_categoryIconsSlideStep != 0) {
        --Menu_Container_categoryIconsSlideStep;
    }

    VS::Menu::container::DrawLiveCategoryIcons(Menu_Container_categoryIconsSlideStep, Menu_Container_detailsViewPhase);

    if (Menu_Container_controlsPanelMode != 0) {
        if (Menu_Container_controlsPanelSlideStep == 0) {
            VS::Items::RefreshLiveMiscTransferCount();
        }
        if (Menu_Container_controlsPanelSlideStep < 0xAU) {
            ++Menu_Container_controlsPanelSlideStep;
        }
        i = Menu_Framework_rowAnimationSteps[0xA - Menu_Container_controlsPanelSlideStep];
    } else {
        if (Menu_Container_controlsPanelSlideStep != 0) {
            --Menu_Container_controlsPanelSlideStep;
        }
        i = (10 - Menu_Container_controlsPanelSlideStep) << 5;
    }
    if (Menu_Container_controlsPanelSlideStep != 0) {
        int s3;
        i = 0x10 - i;
        VS::Menu::Framework::DrawLiveButtonPromptBackground(i, 0x26, 0x90, 0xA);
        VS::Menu::Framework::DrawLiveButtonPromptBackground(i, 0x36, 0x48, 0xA);
        control_icon_x = i - 8;
        VS::Menu::Framework::DrawLiveButtonIcon(Menu_Framework_ButtonId_Square, control_icon_x, 0x24, NULL);
        VS::Menu::Framework::DrawLiveButtonIcon(Menu_Framework_ButtonId_Triangle, i + 0x40, 0x24, NULL);
        VS::Menu::Framework::DrawLiveButtonIcon(Menu_Framework_ButtonId_Cross, control_icon_x, 0x34, NULL);
        s3 = (i + 0xC) & 0xFFFF;
        VS::Render::DrawLiveCompactTextColor(
            "STATUS", s3 | 0x260000, 0x202020 << (Menu_Container_controlsPanelMode & 3), NULL);
        VS::Render::DrawLiveCompactText("CANCEL", (s3 + 0x48) | 0x260000, NULL);
        VS::Render::DrawLiveCompactTextColor(
            "END", s3 | 0x360000, 0x808080 >> (Menu_Container_controlsPanelMode >> 2), NULL);

        if ((Menu_Container_controlsPanelMode >> 2) != 0) {
            if (Menu_Container_capacityListSlideStep < 0xAU) {
                ++Menu_Container_capacityListSlideStep;
            }
            capacity_slide_offset = Menu_Container_capacityListSlideStep << 5;
        } else {
            if (Menu_Container_capacityListSlideStep != 0) {
                --Menu_Container_capacityListSlideStep;
            }
            capacity_slide_offset = Menu_Framework_rowAnimationSteps[Menu_Container_capacityListSlideStep];
        }

        i -= capacity_slide_offset;
        VS::Render::DrawLiveCompactText(Menu_Container_kStorageNames[Menu_Container_browsingGadgetBag], vs_getXY_4(i, 0x4C), NULL);
        s3 = 0;
        packed_capacity_row_position = vs_getXY_4(0x70, 0x58);

        for (;; ++s3) {
            VS::Render::DrawLiveCompactText(
                Menu_Container_kCategoryLabels[s3] + 1, vs_getXY_4(8 + i, 0x58 + s3 * 12), NULL);
            if (s3 == 6) {
                break;
            }
            destination_category_count = VS::Items::CountLiveContainerItems(s3, &Menu_Container_otherStorage->items);
            projected_category_count =
                destination_category_count + VS::Items::CountLiveContainerItems(s3, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
            if (Menu_Container_browsingGadgetBag != 0) {
                category_capacity = Menu_Container_containerItemCapacities[s3];
            } else {
                category_capacity = Items_Inventory_inventoryItemCapacities[s3];
            }
            VS::Menu::container::DrawLiveCapacityCount(category_capacity, i + packed_capacity_row_position, projected_category_count);
            packed_capacity_row_position += vs_getXY_4(0, 12);
        }
        VS::Menu::container::DrawLiveCapacityCount(Menu_Container_browsingGadgetBag != 0 ? 0x100 : 0x40, i + 0xA00070, Menu_Container_transferredMiscCount);
    }
    return 0;
}
#undef vs_getXY_4

// MENUD.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Container_ItemView_ResetMenudStatics(void)
{
    {
        static const char initial[8] = {};
        memcpy((void*)&Menu_Container_itemActionIds, &initial, sizeof Menu_Container_itemActionIds);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Container_transferDirectionOkBlink, &initial, sizeof Menu_Container_transferDirectionOkBlink);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Container_transferDirectionCursorAnimation, &initial, sizeof Menu_Container_transferDirectionCursorAnimation);
    }
    {
        static const char initial = 1;
        memcpy((void*)&Menu_Container_directionCursorResetPending, &initial, sizeof Menu_Container_directionCursorResetPending);
    }
}

}  // extern "C"
