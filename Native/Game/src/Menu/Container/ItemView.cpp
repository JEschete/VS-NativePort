// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Feedback.h"
#include "Content/StringTables.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"

// decomp: local static of _gripNavigation, at file scope so the module reset reaches it
static char* Menu_Container_gemSlotCountText = "X     0";

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

// decomp: MENUD.PRG func_80103D50 (src/MENU/MENUD.PRG/234.c:659)
void Menu_Container_SetPanelVisibility(char panel_visibility)
{
    Menu_Container_categoryTitleVisible = panel_visibility;
    Menu_Container_categoryIconsVisible = panel_visibility;
    Menu_Container_controlsPanelMode = panel_visibility;
}

// decomp: MENUD.PRG _pollItemNavigation (src/MENU/MENUD.PRG/234.c:666)
int Menu_Container_PollDetailsPaging(int itemCategory, int currentSelection)
{
    int previousSelection;
    int count;
    int item_count_for_wrap;

    if ((Input_buttonsState & (PADL1 | PADR1)) != (PADL1 | PADR1)) {

        previousSelection = currentSelection;
        count = Menu_Container_CountItems(itemCategory, &Menu_Container_containerData->items);
        item_count_for_wrap = count;

        if (Input_buttonRepeat & PADL1) {
            int temp = currentSelection - 1;
            currentSelection = temp + count;
        }

        do {
            if (Input_buttonRepeat & PADR1) {
                ++currentSelection;
            }
            if (currentSelection >= item_count_for_wrap) {
                currentSelection -= item_count_for_wrap;
            }
        } while (0);

        if ((Input_buttonsPressed.allButtons & (PADL1 | PADR1))
            && (currentSelection == previousSelection)) {
            Menu_PlayInvalidSound();
        }
    }
    return currentSelection;
}

// decomp: MENUD.PRG func_80103E24 (src/MENU/MENUD.PRG/234.c:700)
void Menu_Container_DrawCategoryIcons(int arg0, int details_view_phase)
{
    int i;
    u_long* icon_sprite_packet;
    Menu_Framework_MenuRow* details_menu_row;

    int selected_category_icon = (Menu_Framework_menuMemory.opaque85[0x1B] - 1) & 7;

    if (arg0 == 4) {
        Menu_Framework_DrawTabNavigation(0);
    }

    arg0 = (arg0 * 8) - 16;

    for (i = 0; i < 8; ++i) {

        int icon_x = (((i + 1) & 7) * 0x10) + 0x20;

        if ((i == selected_category_icon) && (details_view_phase == 1)) {
            details_menu_row = Menu_Framework_MenuItem(0x1F);
            if (icon_x >= (details_menu_row->x - 0xC)) {
                details_menu_row->rowIcon = i + 0x18;
                continue;
            } else {
                icon_sprite_packet = Render_QueueDefaultSprite(0x100010, icon_x | 0x100000);
            }
        } else {
            icon_sprite_packet = Render_QueueSpriteWithTexturePage(
                0x80, icon_x | (arg0 << 0x10), 0x100010, (u_long *)(((u_char*)(Render_frameScratch.orderingTable)) - 0xC));
        }

        icon_sprite_packet[4] = (0x78 + i * 0x10) | 0x8000 | (i == selected_category_icon ? 0x37FD0000 : 0x37FE0000);
    }
}

// decomp: MENUD.PRG func_80103F64 (src/MENU/MENUD.PRG/234.c:735)
void Menu_Container_BeginDetailsTransition(int menu_row_id)
{
    Menu_Framework_MenuRow* menuItem;

    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

    menuItem = Menu_Framework_MenuItem(0x1F);
    menuItem->state = 3;
    menuItem->targetPosition0 = 0x12;

    menuItem = Menu_Framework_MenuItem(menu_row_id);
    menuItem->state = 2;
    menuItem->targetPosition0 = 0x9B;
    menuItem->selected = 1;
    menuItem->subText = NULL;
    menuItem->unselectable = 0;
}

// decomp: MENUD.PRG func_80104034 (src/MENU/MENUD.PRG/234.c:767)
void Menu_Container_InitializeDetailsLayout(int selected_menu_row_id, int stat_page_panel_command)
{
    Menu_Container_categoryIconsVisible = 0;
    Menu_Container_controlsPanelMode = 0;
    Menu_Framework_ContinueItemDetailsTransition(selected_menu_row_id);
    Menu_Framework_UpdateEquipmentStatPages(stat_page_panel_command);
    Menu_Framework_UpdatePrimaryStatPanel(1);
}

// decomp: MENUD.PRG func_80104114 (src/MENU/MENUD.PRG/234.c:789)
int Menu_Container_SelectDetailsItem(int category_id, int category_item_index)
{
    Menu_Framework_menuMemory.cursorMemories[(category_id + 0x51) * 2] = 0;
    Menu_Framework_menuMemory.cursorMemories[(category_id + 0x51) * 2 + 1] = category_item_index;
    return Menu_Container_CategoryIndices(category_id, Menu_Container_containerData)[category_item_index];
}

// decomp: MENUD.PRG func_80104170 (src/MENU/MENUD.PRG/234.c:796)
void Menu_Container_ExitDetailsView(int hide_dp_pp_bars)
{
    Menu_PlayLeaveSound();
    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
    Menu_Framework_FlyOutRowLeft(-1);
    Menu_Framework_UpdateEquipmentStatPages(-1);
    Menu_Framework_UpdatePrimaryStatPanel(2);
    Menu_Container_detailsViewPhase = 2;
    Menu_Container_detailsClosedWithTriangle = Input_buttonsPressed.allButtons & PADRup;
    if (hide_dp_pp_bars != 0) {
        Menu_Framework_UpdateDpPpBars(4);
    }
}

// decomp: MENUD.PRG _setWeaponUi (src/MENU/MENUD.PRG/234.c:810)
void Menu_Container_BuildWeaponRow(Items_ContainerData* container, char** rowStrings,
    int* rowTypes, char* buf, int index)
{
    Items_WeaponLoadout weapon;

    Menu_Container_BuildWeaponStats(&weapon, &container->weapons[index], container);
    Menu_Framework_FormatWeaponRow(&weapon, rowStrings, rowTypes, buf);
    rowStrings[0] = container->weapons[index].name;
}

// decomp: MENUD.PRG _setShieldUi (src/MENU/MENUD.PRG/234.c:820)
void Menu_Container_BuildShieldRow(Items_ContainerData* container, char** rowStrings,
    int* rowTypes, char* buf, int index)
{
    Items_ShieldLoadout shield;

    Menu_Container_BuildShieldStats(&shield, &container->shields[index], container);
    Menu_Framework_FormatShieldRow(&shield, rowStrings, rowTypes, buf);
}

// decomp: MENUD.PRG _weaponNavigation (src/MENU/MENUD.PRG/234.c:829)
int Menu_Container_UpdateWeaponDetails(int weaponIndex)
{
    static char Menu_Container_weaponDetailsStep;
    static char Menu_Container_weaponDetailsSlideStep;
    static char Menu_Container_weaponDetailsItemSource;
    static u_char Menu_Container_weaponDetailsSelectedWeapon;
    static char Menu_Container_unusedWeaponDetailsFlag;

    enum Menu_Container_EquipmentDetailsState { Menu_Container_EquipmentDetailsState_Init, Menu_Container_EquipmentDetailsState_Animate, Menu_Container_EquipmentDetailsState_HandleInput, Menu_Container_EquipmentDetailsState_Exit };

    char* text[2];
    int rowId;
    int selectedWeapon;
    int i;

    if (weaponIndex != 0) {
        Menu_Container_weaponDetailsItemSource = weaponIndex >> 8;
        Menu_Container_weaponDetailsSelectedWeapon = weaponIndex - 1;
        Menu_Container_BeginDetailsTransition(Menu_Container_weaponDetailsItemSource);
        Menu_Container_unusedWeaponDetailsFlag = 0;
        Menu_Container_weaponDetailsSlideStep = 0;
        Menu_Container_weaponDetailsStep = 0;
        return 0;
    }

    switch (Menu_Container_weaponDetailsStep) {
    case Menu_Container_EquipmentDetailsState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_InitializeDetailsLayout(Menu_Container_weaponDetailsItemSource, 7);
            Menu_Framework_SetWeaponStats(
                Menu_Container_containerData->indices.weapons[Menu_Container_weaponDetailsSelectedWeapon]);
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Container_weaponDetailsStep = Menu_Container_EquipmentDetailsState_Animate;
        }
        break;
    case Menu_Container_EquipmentDetailsState_Animate:
        if (Menu_Container_weaponDetailsSlideStep < 10) {
            ++Menu_Container_weaponDetailsSlideStep;
            if (Menu_Container_weaponDetailsSlideStep < 6) {
                Menu_Framework_CreateWeaponComponentRow(Menu_Container_weaponDetailsSlideStep,
                    Menu_Container_containerData->indices.weapons[Menu_Container_weaponDetailsSelectedWeapon], 1);
            }
            break;
        }
        Menu_Container_weaponDetailsStep = Menu_Container_EquipmentDetailsState_HandleInput;
        break;
    case Menu_Container_EquipmentDetailsState_HandleInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Container_ExitDetailsView(1);
            Menu_Container_weaponDetailsStep = Menu_Container_EquipmentDetailsState_Exit;
        } else {
            selectedWeapon = Menu_Container_PollDetailsPaging(0, Menu_Container_weaponDetailsSelectedWeapon);
            if (selectedWeapon != Menu_Container_weaponDetailsSelectedWeapon) {
                Menu_Container_weaponDetailsSelectedWeapon = selectedWeapon;
                i = Menu_Container_SelectDetailsItem(0, selectedWeapon);
                Menu_Container_BuildWeaponRow(&Menu_Container_containerData->items, text, &rowId,
                    Menu_Framework_stringBuf, i - 1);
                Menu_Framework_SetWeaponStats(i);

                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_weaponDetailsItemSource, text, rowId, selectedWeapon);
                for (i = 1; i < 6; ++i) {
                    Menu_Framework_CreateWeaponComponentRow(
                        i, Menu_Container_containerData->indices.weapons[selectedWeapon], 0);
                }
            }
        }
        break;
    case Menu_Container_EquipmentDetailsState_Exit:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG _bladeNavigation (src/MENU/MENUD.PRG/234.c:902)
int Menu_Container_UpdateBladeDetails(int bladeIndex)
{
    static char Menu_Container_bladeDetailsStep;
    static char Menu_Container_bladeDetailsDelay;
    static char Menu_Container_bladeDetailsItemSource;
    static u_char Menu_Container_bladeDetailsItemIndex;

    enum Menu_Container_EquipmentDetailsState { Menu_Container_EquipmentDetailsState_Init, Menu_Container_EquipmentDetailsState_Animate, Menu_Container_EquipmentDetailsState_HandleInput, Menu_Container_EquipmentDetailsState_Exit };

    char* text[2];
    int row_type;
    int selectedBlade;
    int blade_id;

    if (bladeIndex != 0) {
        Menu_Container_bladeDetailsItemSource = bladeIndex >> 8;
        Menu_Container_bladeDetailsItemIndex = bladeIndex - 1;
        Menu_Container_BeginDetailsTransition(Menu_Container_bladeDetailsItemSource);
        Menu_Container_bladeDetailsDelay = 10;
        Menu_Container_bladeDetailsStep = 0;
        return 0;
    }

    switch (Menu_Container_bladeDetailsStep) {
    case Menu_Container_EquipmentDetailsState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_InitializeDetailsLayout(Menu_Container_bladeDetailsItemSource, 3);
            Menu_Framework_SetBladeStats(
                Menu_Container_containerData->indices.blades[Menu_Container_bladeDetailsItemIndex]);
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Container_bladeDetailsStep = Menu_Container_EquipmentDetailsState_Animate;
        }
        break;
    case Menu_Container_EquipmentDetailsState_Animate:
        if (Menu_Container_bladeDetailsDelay != 0) {
            --Menu_Container_bladeDetailsDelay;
        } else {
            Menu_Container_bladeDetailsStep = Menu_Container_EquipmentDetailsState_HandleInput;
        }
        break;
    case Menu_Container_EquipmentDetailsState_HandleInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Container_ExitDetailsView(1);
            Menu_Container_bladeDetailsStep = Menu_Container_EquipmentDetailsState_Exit;
        } else {
            selectedBlade = Menu_Container_PollDetailsPaging(1, Menu_Container_bladeDetailsItemIndex);
            if (selectedBlade != Menu_Container_bladeDetailsItemIndex) {
                Menu_Container_bladeDetailsItemIndex = selectedBlade;
                blade_id = Menu_Container_SelectDetailsItem(1, selectedBlade);
                Menu_Framework_FormatBladeRow(
                    &Menu_Container_containerData->items.blades[blade_id - 1], text, &row_type,
                    Menu_Framework_stringBuf);
                Menu_Framework_SetBladeStats(blade_id);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_bladeDetailsItemSource, text, row_type, selectedBlade);
            }
        }
        break;
    case Menu_Container_EquipmentDetailsState_Exit:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG _gripNavigation (src/MENU/MENUD.PRG/234.c:965)
int Menu_Container_UpdateGripDetails(int packed_details_selection)
{

    static char Menu_Container_gripDetailsStep;
    static char Menu_Container_gripDetailsSlideStep;
    static char Menu_Container_gripDetailsItemSource;
    static u_char Menu_Container_gripDetailsItemIndex;
    static char Menu_Container_unusedGripDetailsFlag;

    char* row_text[2];
    int row_type;
    int selected_grip_index;
    int grip_id;

    if (packed_details_selection != 0) {
        Menu_Container_gripDetailsItemSource = packed_details_selection >> 8;
        Menu_Container_gripDetailsItemIndex = packed_details_selection - 1;
        Menu_Container_BeginDetailsTransition(Menu_Container_gripDetailsItemSource);
        Menu_Container_unusedGripDetailsFlag = 0;
        Menu_Container_gripDetailsSlideStep = 0;
        Menu_Container_gripDetailsStep = 0;
        return 0;
    }
    switch (Menu_Container_gripDetailsStep) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_InitializeDetailsLayout(Menu_Container_gripDetailsItemSource, 4);
            Menu_Framework_SetGripStats(
                Menu_Container_containerData->indices.grips[Menu_Container_gripDetailsItemIndex]);
            Menu_Container_gripDetailsStep = 1U;
        }
        break;
    case 1:
        if (Menu_Container_gripDetailsSlideStep < 0xAU) {
            Menu_Container_gripDetailsSlideStep = (Menu_Container_gripDetailsSlideStep + 1);
        } else {
            Menu_Container_gripDetailsStep = 2U;
        }
        break;
    case 2:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Container_ExitDetailsView(0);
            Menu_Container_gripDetailsStep = 3U;
        } else {
            selected_grip_index = Menu_Container_PollDetailsPaging(2, Menu_Container_gripDetailsItemIndex);
            if (selected_grip_index != Menu_Container_gripDetailsItemIndex) {
                Menu_Container_gripDetailsItemIndex = selected_grip_index;
                grip_id = Menu_Container_SelectDetailsItem(2, selected_grip_index);
                Menu_Framework_FormatGripRow(&Menu_Container_containerData->items.grips[grip_id - 1],
                    row_text, &row_type, Menu_Framework_stringBuf);
                Menu_Framework_SetGripStats(grip_id);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_gripDetailsItemSource, row_text, row_type, selected_grip_index);
            }
        }
        // BUG: write to .rodata
        Menu_Container_gemSlotCountText[6] = (Menu_Container_containerData->items
                             .grips[Menu_Container_containerData->indices.grips[Menu_Container_gripDetailsItemIndex] - 1]
                             .gemSlots
                         + 0x30);

        Menu_Framework_DrawRowIcon(Menu_Framework_MainMenuIcon_Gem | (1 << 8), 256, 32);
        Render_DrawCompactText(Menu_Container_gemSlotCountText, vs_getXY(280, 36), NULL);
        break;

    case 3:
        return Menu_Framework_AreMenuRowsIdle();
    }

    return 0;
}

// decomp: MENUD.PRG _shieldNavigation (src/MENU/MENUD.PRG/234.c:1037)
int Menu_Container_UpdateShieldDetails(int packed_details_selection)
{
    static char Menu_Container_shieldDetailsStep;
    static char Menu_Container_shieldDetailsSlideStep;
    static char Menu_Container_shieldDetailsItemSource;
    static u_char Menu_Container_shieldDetailsItemIndex;
    static char Menu_Container_unusedShieldDetailsFlag;

    char* row_text[2];
    int row_type;
    int selected_shield_index;
    int i;

    if (packed_details_selection != 0) {
        Menu_Container_shieldDetailsItemSource = packed_details_selection >> 8;
        Menu_Container_shieldDetailsItemIndex = packed_details_selection - 1;
        Menu_Container_BeginDetailsTransition(Menu_Container_shieldDetailsItemSource);
        Menu_Container_unusedShieldDetailsFlag = 0;
        Menu_Container_shieldDetailsSlideStep = 0;
        Menu_Container_shieldDetailsStep = 0;
        return 0;
    }

    switch (Menu_Container_shieldDetailsStep) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_InitializeDetailsLayout(Menu_Container_shieldDetailsItemSource, 7);
            Menu_Framework_SetShieldStats(
                Menu_Container_containerData->indices.shields[Menu_Container_shieldDetailsItemIndex]);
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Container_shieldDetailsStep = 1;
        }
        break;
    case 1:
        if (Menu_Container_shieldDetailsSlideStep < 10) {
            ++Menu_Container_shieldDetailsSlideStep;
            if (Menu_Container_shieldDetailsSlideStep < 4) {
                Menu_Framework_CreateShieldGemRow(
                    Menu_Container_shieldDetailsSlideStep, Menu_Container_containerData->indices.shields[Menu_Container_shieldDetailsItemIndex], 1);
            }
            break;
        }
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_shieldDetailsStep = 2;
        }
        break;
    case 2:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Container_ExitDetailsView(1);
            Menu_Container_shieldDetailsStep = 3;
        } else {
            selected_shield_index = Menu_Container_PollDetailsPaging(3, Menu_Container_shieldDetailsItemIndex);
            if (selected_shield_index != Menu_Container_shieldDetailsItemIndex) {
                Menu_Container_shieldDetailsItemIndex = selected_shield_index;
                i = Menu_Container_SelectDetailsItem(3, selected_shield_index);
                Menu_Container_BuildShieldRow(&Menu_Container_containerData->items, row_text, &row_type,
                    Menu_Framework_stringBuf, i - 1);
                Menu_Framework_SetShieldStats(i);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_shieldDetailsItemSource, row_text, row_type, selected_shield_index);
                for (i = 1; i < 4; ++i) {
                    Menu_Framework_CreateShieldGemRow(
                        i, Menu_Container_containerData->indices.shields[selected_shield_index], 0);
                }
                return 0;
            }
        }
        break;
    case 3:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG _armorNavigation (src/MENU/MENUD.PRG/234.c:1110)
int Menu_Container_UpdateArmorDetails(int packed_details_selection)
{
    static char Menu_Container_armorDetailsStep;
    static char Menu_Container_armorDetailsDelay;
    static char Menu_Container_armorDetailsItemSource;
    static u_char Menu_Container_armorDetailsItemIndex;

    char* row_text[2];
    int row_type;
    int selected_armor_index;
    int armor_id;

    if (packed_details_selection != 0) {
        Menu_Container_armorDetailsItemSource = packed_details_selection >> 8;
        Menu_Container_armorDetailsItemIndex = (packed_details_selection - 1);
        Menu_Container_BeginDetailsTransition(Menu_Container_armorDetailsItemSource);
        Menu_Container_armorDetailsDelay = 10;
        Menu_Container_armorDetailsStep = 0;
        return 0;
    }

    switch (Menu_Container_armorDetailsStep) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_InitializeDetailsLayout(Menu_Container_armorDetailsItemSource, 7);
            Menu_Framework_SetArmorStats(
                Menu_Container_containerData->indices.armor[Menu_Container_armorDetailsItemIndex]);
            if (Audio_Seq_unusedInventory
                    .armor[Menu_Container_containerData->indices.armor[Menu_Container_armorDetailsItemIndex] - 1]
                    .category
                != 7) {
                Menu_Framework_UpdateDpPpBars(1);
            }
            Menu_Container_armorDetailsStep = 1;
        }
        break;
    case 1:
        if (Menu_Container_armorDetailsDelay != 0) {
            Menu_Container_armorDetailsDelay = (Menu_Container_armorDetailsDelay - 1);
        } else {
            Menu_Container_armorDetailsStep = 2;
        }
        break;
    case 2:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Container_ExitDetailsView(1);
            Menu_Container_armorDetailsStep = 3;
        } else {
            selected_armor_index = Menu_Container_PollDetailsPaging(4, Menu_Container_armorDetailsItemIndex);
            if (selected_armor_index != Menu_Container_armorDetailsItemIndex) {
                Menu_Container_armorDetailsItemIndex = selected_armor_index;
                armor_id = Menu_Container_SelectDetailsItem(4, selected_armor_index);
                Menu_Framework_FormatInventoryArmorRow(
                    &Menu_Container_containerData->items.armor[armor_id - 1], row_text, &row_type,
                    Menu_Framework_stringBuf);
                Menu_Framework_SetArmorStats(armor_id);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_armorDetailsItemSource, row_text, row_type, selected_armor_index);
            }
        }
        break;
    case 3:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG _gemNavigation (src/MENU/MENUD.PRG/234.c:1176)
int Menu_Container_UpdateGemDetails(int packed_details_selection)
{
    static u_char Menu_Container_gemDetailsStep;
    static u_char Menu_Container_gemDetailsDelay;
    static u_char Menu_Container_gemDetailsItemSource;
    static u_char Menu_Container_gemDetailsItemIndex;

    char* row_text[2];
    int row_type;
    int selected_gem_index;
    int gem_id;

    if (packed_details_selection != 0) {
        Menu_Container_gemDetailsItemSource = packed_details_selection >> 8;
        Menu_Container_gemDetailsItemIndex = packed_details_selection - 1;
        Menu_Container_BeginDetailsTransition(Menu_Container_gemDetailsItemSource);
        Menu_Container_gemDetailsDelay = 10;
        Menu_Container_gemDetailsStep = 0;
        return 0;
    }

    switch (Menu_Container_gemDetailsStep) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_InitializeDetailsLayout(Menu_Container_gemDetailsItemSource, 3);
            Menu_Framework_SetGemStats(Menu_Container_containerData->indices.gems[Menu_Container_gemDetailsItemIndex]);
            Menu_Container_gemDetailsStep = 1;
        }
        break;
    case 1:
        if (Menu_Container_gemDetailsDelay != 0) {
            Menu_Container_gemDetailsDelay -= 1;
        } else {
            Menu_Container_gemDetailsStep = 2;
        }
        break;
    case 2:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Container_ExitDetailsView(0);
            Menu_Container_gemDetailsStep = 3;
            Menu_Container_detailsClosedWithTriangle = Input_buttonsPressed.allButtons & 0x10;
        } else {
            selected_gem_index = Menu_Container_PollDetailsPaging(5, Menu_Container_gemDetailsItemIndex);
            if (selected_gem_index != Menu_Container_gemDetailsItemIndex) {
                Menu_Container_gemDetailsItemIndex = selected_gem_index;
                gem_id = Menu_Container_SelectDetailsItem(5, selected_gem_index);
                Menu_Framework_FormatGemRow(&Menu_Container_containerData->items.gems[gem_id - 1],
                    row_text, &row_type, Menu_Framework_stringBuf);
                Menu_Framework_SetGemStats(gem_id);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_gemDetailsItemSource, row_text, row_type, selected_gem_index);
            }
        }
        break;
    case 3:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG func_80105008 (src/MENU/MENUD.PRG/234.c:1235)
int Menu_Container_UpdateItemDetails(int packed_category_selection)
{
    static int (*Menu_Container_kItemDetailHandlers[])(int) = { Menu_Container_UpdateWeaponDetails, Menu_Container_UpdateBladeDetails,
        Menu_Container_UpdateGripDetails, Menu_Container_UpdateShieldDetails, Menu_Container_UpdateArmorDetails, Menu_Container_UpdateGemDetails };

    static u_char Menu_Container_detailsCategory;

    int detail_handler_result;
    int packed_details_selection = 0;

    if (packed_category_selection != 0) {
        packed_details_selection = packed_category_selection >> 4;
        Menu_Container_detailsCategory = packed_category_selection & 0xF;
        Menu_Container_detailsViewPhase = 1;
        Menu_Container_detailsClosedWithTriangle = 0;
        Menu_Framework_ResetItemDetailCursor();
        Menu_Framework_MenuItem(0x1F)->itemPage = packed_details_selection & 0xFF;
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
    }

    detail_handler_result = Menu_Container_kItemDetailHandlers[Menu_Container_detailsCategory](packed_details_selection);

    if (detail_handler_result != 0) {
        if (Menu_Container_detailsClosedWithTriangle != 0) {
            return -2;
        }
        Menu_Container_categoryIconsVisible = 1;
        Menu_Container_detailsViewPhase = 0;
        Menu_Container_controlsPanelMode = 1;
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
    } else if (Menu_Framework_AreMenuRowsIdle() != 0) {
        Menu_Framework_cursorColor = Menu_Framework_selectedStatusViewElement != 9;
        Menu_Framework_DrawTabNavigation(1);
        Menu_Framework_UpdateItemDetails();
    }
    return detail_handler_result;
}

// decomp: MENUD.PRG func_80106464 (src/MENU/MENUD.PRG/234.c:1925)
void Menu_Container_DrawCapacityCount(int capacity_limit, int packed_count_position, int occupied_count)
{
    int used_count_color;
    int packed_used_count_position;

    used_count_color = 0x808080;
    Menu_Framework_DrawColoredInteger(capacity_limit, packed_count_position, 0x808080, 0);
    packed_used_count_position = packed_count_position - 0x20;
    Render_DrawCompactTextColor("/", packed_used_count_position, 0x808080, NULL);
    if ((occupied_count * 2) < capacity_limit) {
        used_count_color = 0x804020;
    }
    Menu_Framework_DrawColoredInteger(occupied_count, packed_used_count_position, used_count_color, 0);
}

// decomp: MENUD.PRG func_80106504 (src/MENU/MENUD.PRG/234.c:1940)
void Menu_Container_DrawStorageCapacities(void)
{
    int i;
    int var_s1_2;

    for (i = 0; i < 2; ++i) {
        Render_DrawCompactText(Menu_Container_kStorageNames[i], (0x3A + i * 0x80) | 0x580000, NULL);
    }

    for (i = 0; i < 7; ++i, var_s1_2 += 0xC) {
        Menu_Container_DrawCapacityCount(Items_Inventory_inventoryItemCapacities[i],
            ((0x64 + 0xC * i) << 16) | 0x70,
            Menu_Container_CountItems(i, &Menu_Framework_unusedInventoryStorage->gadgetBag.items));
        Render_DrawCompactText(Menu_Container_kCategoryLabels[i], ((0x64 + 0xC * i) << 16) | 0xA0, NULL);
        Menu_Container_DrawCapacityCount(Menu_Container_containerItemCapacities[i], ((0x64 + 0xC * i) << 16) | 0xF8,
            Menu_Container_CountItems(i, &Menu_Framework_unusedInventoryStorage->container.items));
    }
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
                Menu_Framework_FormatBladeRow(
                    &container->blades[itemIndex], &text[i * 2], &rowTypes[i], c);
                break;
            case 2:
                Menu_Framework_FormatGripRow(
                    &container->grips[itemIndex], &text[i * 2], &rowTypes[i], c);
                break;
            case 3:
                Menu_Container_BuildShieldRow(container, &text[i * 2], &rowTypes[i], c, itemIndex);
                break;
            case 4:
                Menu_Framework_FormatInventoryArmorRow(
                    &container->armor[itemIndex], &text[i * 2], &rowTypes[i], c);
                break;
            case 5:
                Menu_Framework_FormatGemRow(
                    &container->gems[itemIndex], &text[i * 2], &rowTypes[i], c);
                if (Menu_Container_ParentShieldIndex(5, itemIndex, container) != 0) {
                    rowType = 0xCC01;
                }
                break;
            case 6:
                Menu_Framework_FormatMiscItemRow(
                    &container->misc[itemIndex], &text[i * 2], &rowTypes[i], c);
                break;
            }
            if (Menu_Container_ParentWeaponIndex(itemCategory, itemIndex, container) != 0) {
                rowType = 0xCC01;
            }
            ++itemCount;
            rowTypes[i] |= rowType;
        }
    } else {
        char** t;
        int* r;
        indices = Menu_Container_CategoryIndices(itemCategory, Menu_Container_containerData);
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

                Menu_Framework_FormatMiscItemRow(
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
                    Menu_Framework_FormatBladeRow(
                        &Menu_Container_containerData->items.blades[itemIndex], &text[i * 2],
                        &rowTypes[i], c);
                    break;
                case 2:
                    Menu_Framework_FormatGripRow(&Menu_Container_containerData->items.grips[itemIndex],
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
                    Menu_Framework_FormatInventoryArmorRow(
                        &Menu_Container_containerData->items.armor[itemIndex], &text[i * 2],
                        &rowTypes[i], c);
                    if (Menu_Container_containerData->items.armor[itemIndex].limb != 0) {
                        rowType = 0xCA00;
                    }
                    break;
                case 5:
                    Menu_Framework_FormatGemRow(&Menu_Container_containerData->items.gems[itemIndex],
                        &text[i * 2], &rowTypes[i], c);
                    break;
                }
                parentIndex = Menu_Container_ParentWeaponIndex(
                    itemCategory, itemIndex, &Menu_Container_containerData->items);
                if (parentIndex != 0) {
                    rowType = 0xCA00;
                    if (Menu_Container_containerData->items.weapons[parentIndex - 1].isEquipped
                        == 0) {
                        rowType = 0xCC00;
                    }
                }
                parentIndex = Menu_Container_ParentShieldIndex(
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

// decomp: MENUD.PRG func_801071D8 (src/MENU/MENUD.PRG/234.c:2309)
void Menu_Container_RestoreItemRows(int category_id)
{
    int selected_row_mismatch;
    int i;

    while (Menu_Framework_itemsListRow < 16) {
        Menu_Framework_UpdateRowSelection();
    }

    for (i = 20; i < 40; ++i) {

        Menu_Framework_MenuRow* restored_menu_row = Menu_Framework_MenuItem(i);

        if (restored_menu_row->state == 2) {
            restored_menu_row->state = 1;
            restored_menu_row->x = restored_menu_row->targetPosition0;
        }

        selected_row_mismatch = i ^ (Menu_Framework_menuMemory.cursorMemories[(category_id + 81) * 2] + 20);
        restored_menu_row->selected = selected_row_mismatch == 0;
    }
}

// decomp: MENUD.PRG func_801072B0 (src/MENU/MENUD.PRG/234.c:2332)
int Menu_Container_UpdateTransferConfirmation(int initialize_confirmation)
{
    static int Menu_Container_transferConfirmationSelectedAction;
    static char Menu_Container_transferConfirmationStep;
    static char Menu_Container_transferConfirmationLayoutPadding[3] __attribute__((unused));

    char* confirmation_row_text[4];
    int confirmation_row_types[2];
    int i;
    int confirmation_step;

    if (initialize_confirmation != 0) {
        Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(1));
        Menu_Container_controlsPanelMode = 5;
        Menu_Inventory_freezeTabArrows = 0;
        Menu_Container_transferConfirmationSelectedAction = 0;
        Menu_Container_transferConfirmationStep = 0;
        return 0;
    }

    confirmation_step = Menu_Container_transferConfirmationStep;

    switch (confirmation_step) {
    case 0:
        for (i = 0; i < 2; ++i) {
            confirmation_row_text[i * 2] = (char*)&Menu_Container_menuText[Menu_Container_menuText[8 + i]];
            confirmation_row_text[i * 2 + 1] = (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag + 10]];
            confirmation_row_types[i] = 0;
        }

        Menu_Framework_CreateActionRows(2, 3, confirmation_row_text, confirmation_row_types);
        Menu_Container_transferConfirmationStep = 1;
        break;
    case 1:
        Menu_Framework_UpdateActionMenu();
        Menu_Container_transferConfirmationSelectedAction = Menu_Framework_ActionSelectionResult() + 1;

        if (Menu_Container_transferConfirmationSelectedAction != 0) {
            Menu_Framework_FlyOutRowLeft(0);
            Menu_Framework_FlyOutRowLeft(1);
            if (Menu_Container_transferConfirmationSelectedAction == confirmation_step) {
                Menu_PlaySelectSound();
                Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                Menu_Container_SetPanelVisibility(0);
            } else {
                Menu_PlayLeaveSound();
                if (Menu_Container_transferConfirmationSelectedAction == 2) {
                    Menu_Container_transferConfirmationSelectedAction = -1;
                }
            }
            Menu_Container_transferConfirmationStep = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Container_transferConfirmationSelectedAction;
        }
    }
    return 0;
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
        Menu_Container_SetPanelVisibility(1);
        Menu_Container_categoryTitleSlideStep = 0;
        if (Input_settings.cursorMemory == 0) {
            memset(Menu_Framework_menuMemory.opaqueA0, 0, sizeof Menu_Framework_menuMemory.opaqueA0);
        }
        Core_Memory_ZeroWordsBackward(
            &Menu_Framework_unusedInventoryStorage->stagedTransfer, sizeof Menu_Framework_unusedInventoryStorage->stagedTransfer);
        Core_Memory_ZeroWordsBackward(Menu_Framework_unusedInventoryStorage->itemsToTransfer,
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
    temp_s1 = Menu_Container_CategoryIndices(selected_category_id, Menu_Container_containerData);

    switch (Menu_Container_itemListStep) { /* switch 1 */
    case 0:
        Menu_Framework_InitializeInformationBox();
        /* fallthrough */
    case 1:
        temp_s1 = (u_short *)(Core_Memory_AllocateFromHighEnd(0x6C00));
        initial_row_types = (int *)(((u_char*)((void*)temp_s1)) + 0x6000);
        initial_row_text = (char **)(((u_char*)((void*)temp_s1)) + 0x6400);
        Menu_Inventory_freezeTabArrows = 1;

        for (temp_s3 = 0; temp_s3 < 7; ++temp_s3) {
            Menu_Container_RebuildCategoryIndices(temp_s3, Menu_Container_containerData);
        }

        Menu_Container_RebuildPendingTransfers();
        temp_s2 = Menu_Container_BuildItemRows(selected_category_id, initial_row_text, initial_row_types, (char*)temp_s1);
        Menu_Container_categoryEmpty = temp_s2 == 0;
        if (Menu_Container_categoryEmpty != 0) {
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag + 27 + (selected_category_id == 7) * 2]]);
        } else {
            temp_s3 = Input_settings.cursorMemory;
            if (Menu_Container_itemListFirstEntry == 0) {
                Input_settings.cursorMemory = 1;
            }
            Menu_Container_itemListFirstEntry = 0;
            Menu_Framework_CreateMenuRows(temp_s2, (selected_category_id + 0x51) | 0x19200, initial_row_text, initial_row_types);
            if (Menu_Container_itemListStep == 0) {
                Menu_Container_RestoreItemRows(selected_category_id);
            }
            Input_settings.cursorMemory = temp_s3;
        }
        Core_Memory_FreeFromHighEnd(temp_s1);
        Menu_Container_itemListStep = 2;
        break;
    case 2:
        Menu_Container_controlsPanelMode = 1;
        if (Menu_Container_categoryEmpty != 0) {
            if (Input_buttonsPressed.allButtons & 0x10) {
                Menu_PlayLeaveSound();
                return -2;
            }
            if (Input_buttonsPressed.allButtons & 0x40) {
                Menu_PlayLeaveSound();
                if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[0] == 0) {
                    return -1;
                }
                if (selected_category_id == 7) {
                    Menu_Container_UpdateTransferConfirmation(1);
                    Menu_Container_itemListStep = 0xF;
                } else {
                    Menu_Framework_menuMemory.opaqueA0[0] = 0;
                    Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                    Menu_Container_itemListStep = 0xD;
                }
                break;
            }
            if (Input_buttonsPressed.allButtons & 0xA0) {
                Menu_PlayInvalidSound();
            }
        } else {
            int details_index_bias = 1;
            temp_s0 = selected_category_id < 6;
            Menu_Container_controlsPanelMode = temp_s0 + 1;
            Menu_Container_itemListSelectedRow = Menu_Framework_GetSelectedRow() + 1;
            if (Menu_Container_itemListSelectedRow == 0) {
                if (Input_buttonsPressed.allButtons & 0x80) {
                    if ((temp_s0 != 0) && (Menu_Framework_AreMenuRowsIdle() != 0)) {
                        Menu_PlaySelectSound();
                        temp_s0 = Menu_Framework_CloseRowsAndRememberCursor();
                        Menu_Framework_MenuItem(temp_s0 >> 8)->outsetIcon = 0;
                        Menu_Container_UpdateItemDetails(selected_category_id | ((temp_s0 + details_index_bias) * 0x10));
                        Menu_Container_categoryTitleVisible = 0;
                        Menu_Container_itemListStep = 7;
                        break;
                    }
                    Menu_PlayInvalidSound();
                }
            } else if (Menu_Container_itemListSelectedRow > 0) {
                if (selected_category_id == 7) {
                    Menu_Container_UndoPendingTransfer(Menu_Container_itemListSelectedRow - 1);
                    Menu_Container_RefreshMiscTransferCount();
                    Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                    Menu_Container_itemListStep = 4;
                } else {
                    rebuilt_rows_allocation = Core_Memory_AllocateFromHighEnd(0x6C00U);
                    rebuilt_row_text = (char **)(((u_char*)(rebuilt_rows_allocation)) + 0x6400);
                    rebuilt_row_types = (int *)(((u_char*)(rebuilt_rows_allocation)) + 0x6000);
                    temp_s0 = Menu_Container_BuildItemRows(selected_category_id, rebuilt_row_text, rebuilt_row_types, (char *)(rebuilt_rows_allocation));
                    rebuilt_row_types[Menu_Container_itemListSelectedRow - 1] |= 8;
                    temp_s3 = Input_settings.cursorMemory;
                    Input_settings.cursorMemory = 1;
                    Menu_Framework_CreateMenuRows(
                        temp_s0, (selected_category_id + 0x51) | 0x19200, rebuilt_row_text, rebuilt_row_types);
                    Input_settings.cursorMemory = temp_s3;
                    Menu_Container_RestoreItemRows(selected_category_id);
                    Menu_Container_controlsPanelMode = 1;
                    Menu_Inventory_freezeTabArrows = 0;
                    Core_Memory_FreeFromHighEnd(rebuilt_rows_allocation);
                    Menu_Container_itemListStep = 6;
                }
            } else if (Menu_Container_itemListSelectedRow == -2) {
                return -2;
            } else {
                if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[0] == 0) {
                    Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                    return -1;
                }
                if (selected_category_id != 7) {
                    Menu_Framework_menuMemory.opaqueA0[0] = 0;
                    Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                    Menu_Container_itemListStep = 0xD;
                } else {
                    Menu_Container_UpdateTransferConfirmation(1);
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
                Menu_PlayChangeSound();
                Menu_Framework_menuMemory.opaqueA0[0] = (selected_category_id + 1) & 7;
                if (Menu_Container_itemListStep == 2) {
                    if (Menu_Container_categoryEmpty == 0) {
                        Menu_Framework_CloseRowsAndRememberCursor();
                        Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                    }
                    Menu_Container_itemListStep = 3;
                }
            }
        }
        if (Menu_Container_itemListStep == 3) {
            if (Menu_Framework_AreMenuRowsIdle() != 0) {
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
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_itemListStep = 1;
        }
        break;
    case 5:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_itemListStep = 0;
        }
        break;
    case 6:
        if (Input_buttonsPressed.allButtons & 0x80) {
            Menu_PlayInvalidSound();
        }
        temp_s3 = Menu_Framework_GetSelectedRow() + 1;
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
                        Menu_Container_ClearItem(
                            6, temp_s0 - 1, &Menu_Container_containerData->items);
                    }
                } else {
                    temp_s1[temp_s3 - 1] = var_a2;
                    temp_s1[temp_s2 - 1] = temp_s0;
                }
                Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
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
        temp_s3 = Menu_Container_UpdateItemDetails(0);
        if (temp_s3 != 0) {
            if (temp_s3 == -2) {
                return -2;
            }
            Menu_Container_categoryTitleVisible = 1;
            category_title_row = Menu_Framework_MenuItem(0);
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
        Core_Memory_ZeroWordsBackward(Menu_Framework_rowTypeBuf, 0x10);
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

        var_a2 = Menu_Container_ParentWeaponIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items);
        var_a1 = 8;

        if (var_a2 != 0) {
            temp_s0 = 1;
            temp_s2 =
                Menu_Container_menuText[Menu_Container_containerData->items.weapons[var_a2 - 1].isEquipped == 0
                              ? 5
                              : 4];
        }

        var_a2 = Menu_Container_ParentShieldIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items);
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
                    Menu_Container_CountItems(temp_s2, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
            }

            Menu_Container_PreviewTransferredMisc();
            var_a2 = Menu_Container_CopyItem(6, &Menu_Framework_unusedInventoryStorage->transferPreview, temp_s3,
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
            var_a2 = Menu_Container_TransferCapacityError(selected_category_id, temp_s3);
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
            if ((Menu_Container_ParentWeaponIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items) == 0)
                && (Menu_Container_ParentShieldIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items)
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
        var_a2 = Menu_Container_ParentWeaponIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items);
        if (var_a2 != 0) {
            temp_s0 = 1;
            if (Menu_Container_containerData->items.weapons[var_a2 - 1].isEquipped == 0) {
                temp_s0 = 2;
            }
        }
        var_a2 = Menu_Container_ParentShieldIndex(selected_category_id, temp_s3, &Menu_Container_containerData->items);
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
        Menu_Framework_CreateActionRows(
            temp_s2, selected_category_id + 0x59, (char**)&action_row_text, (int*)Menu_Framework_rowTypeBuf);
        Menu_Container_itemListStep = 9;
        break;
    case 9:
        Menu_Framework_UpdateInformationVisibility(
            Menu_Framework_HighlightedActionRow(), Menu_Framework_actionMenuState);
        Menu_Framework_UpdateActionMenu();
        temp_s3 = Menu_Framework_ActionSelectionResult() + 1;
        if (temp_s3 != 0) {
            Menu_Framework_FlyOutRowsLeftExcept(0x28);
            if (temp_s3 > 0) {
                Menu_PlaySelectSound();
                switch (Menu_Container_itemActionIds[temp_s3]) {
                case 1:
                    Menu_Framework_InitializeInformationBox();
                    Menu_Container_StageItemTransfer(selected_category_id, Menu_Container_CategoryIndices(selected_category_id,
                                              Menu_Container_containerData)[Menu_Container_itemListSelectedRow - 1]
                                              - 1);
                    Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                    Menu_Container_itemListStep = 4;
                    break;
                case 2:
                    Menu_Framework_InitializeInformationBox();
                    Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
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
                    Menu_Container_UpdateDiscardMenu(temp_s2);
                    Menu_Container_itemListStep = 0xB;
                    break;
                case 4:
                    Menu_Container_UpdateDiscardMenu(temp_s1[Menu_Container_itemListSelectedRow - 1] | (selected_category_id << 0xC) | 0x20000);
                    Menu_Container_itemListStep = 0xB;
                    break;
                case 5:
                    Menu_Framework_InitializeInformationBox();
                    Menu_Container_UpdateSortMenu(selected_category_id + 1);
                    Menu_Container_itemListStep = 0xC;
                    break;
                }
                break;
            }
            Menu_PlayLeaveSound();
            if (temp_s3 == -2) {
                Menu_Framework_DismissInformationBox();
                return -2;
            }
            Menu_Container_itemListStep = 0;
        }
        break;
    case 10:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            temp_s3 = Menu_Container_ParentWeaponIndex(
                selected_category_id, temp_s1[Menu_Container_itemListSelectedRow - 1] - 1, &Menu_Container_containerData->items);
            if (temp_s3 != 0) {
                selected_category_id = 0;
            } else {
                temp_s3 = Menu_Container_ParentShieldIndex(
                    selected_category_id, temp_s1[Menu_Container_itemListSelectedRow - 1] - 1, &Menu_Container_containerData->items);
                selected_category_id = 3;
            }
            temp_s2 = Menu_Container_CountItems(selected_category_id, &Menu_Container_containerData->items);
            parent_item_row = Menu_Container_FindItemRow(selected_category_id, temp_s3 - 1,
                Menu_Container_CategoryIndices(selected_category_id, Menu_Container_containerData));
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
        temp_s3 = Menu_Container_UpdateDiscardMenu(0);
        if (temp_s3 != 0) {
            Menu_Framework_InitializeInformationBox();
            Menu_Framework_FlyOutRowsLeftExcept(0x28);
            Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
            if (temp_s3 == -2) {
                return -2;
            }
            Menu_Container_itemListStep = 4;
        }
        break;
    case 12:
        temp_s3 = Menu_Container_UpdateSortMenu(0);
        if (temp_s3 != 0) {
            Menu_Framework_FlyOutRowsLeftExcept(0x28);
            Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
            if (temp_s3 == -2) {
                return -2;
            }
            Menu_Container_itemListStep = 4;
        }
        break;
    case 13:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            pending_rows_allocation = Core_Memory_AllocateFromHighEnd(0x6C00U);
            pending_row_text = (char **)(((u_char*)(pending_rows_allocation)) + 0x6400);
            pending_row_types = (int *)(((u_char*)(pending_rows_allocation)) + 0x6000);
            temp_s2 = Menu_Container_BuildItemRows(7, pending_row_text, pending_row_types, (char *)(pending_rows_allocation));
            Menu_Container_categoryEmpty = temp_s2 == 0;
            if (Menu_Container_categoryEmpty != 0) {
                Menu_Framework_SetInformationMessage(
                    (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag + 0x1D]]);
            } else {
                Menu_Framework_CreateMenuRows(
                    temp_s2, (selected_category_id + 0x51) | 0x19200, pending_row_text, pending_row_types);
            }
            Core_Memory_FreeFromHighEnd(pending_rows_allocation);
            Menu_Container_itemListStep = 0xE;
        }
        break;
    case 14:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_CloseRowsAndRememberCursor();
            Menu_Container_UpdateTransferConfirmation(1);
            Menu_Container_itemListStep = 0xF;
        }
        break;
    case 15:
        temp_s3 = Menu_Container_UpdateTransferConfirmation(0);
        if (temp_s3 != 0) {
            Menu_Framework_InitializeInformationBox();
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
                        Menu_Container_CopyItem((temp_s2 >> 8) | 0x10, &Menu_Container_otherStorage->items,
                            (temp_s2 - 1) & 0xFF, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
                        Menu_Container_ClearItem((temp_s2 >> 8), (temp_s2 - 1) & 0xFF,
                            &Menu_Framework_unusedInventoryStorage->stagedTransfer);
                        Menu_Container_RebuildPendingTransfers();
                    }
                } else {
                    while ((temp_s2 = Menu_Framework_unusedInventoryStorage->itemsToTransfer[0])) {
                        Menu_Container_CopyItem((temp_s2 >> 8) | 0x10, &Menu_Container_otherStorage->items,
                            (temp_s2 - 1) & 0xFF, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
                        Menu_Container_ClearItem((temp_s2 >> 8), (temp_s2 - 1) & 0xFF,
                            &Menu_Framework_unusedInventoryStorage->stagedTransfer);
                        Menu_Container_RebuildPendingTransfers();
                    }
                }
                for (temp_s3 = 0; temp_s3 < 7; ++temp_s3) {
                    Menu_Container_RebuildCategoryIndices(temp_s3, Menu_Container_otherStorage);
                }
                Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                Menu_Container_itemListStep = 17;
            }
        }
        break;
    case 17:
        return Menu_Framework_AreMenuRowsIdle();
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
            Menu_Framework_SetMenuItem(Menu_Container_transferDirectionStep + 0xA, 0x140, (Menu_Container_transferDirectionStep * 0x10) + 0x22,
                0x7E, 0, (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_transferDirectionStep * 2 + 12]]);
        selected_direction_row->state = 2;
        selected_direction_row->targetPosition0 = 0xC2;
        ++Menu_Container_transferDirectionStep;
        break;
    case 2:
        Menu_Container_transferDirectionStep += Menu_Framework_AreMenuRowsIdle();
        break;
    case 3:
        selected_direction = Menu_Framework_menuMemory.opaque9E;

        for (i = 0; i < 2; ++i) {
            Menu_Framework_MenuItem(i + 0xA)->selected = i == selected_direction;
        }
        if (Input_buttonsPressed.allButtons & 0x10) {
            Menu_PlayLeaveSound();
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            return -2;
        }
        if (Input_buttonsPressed.allButtons & 0x40) {
            Menu_PlayLeaveSound();
            return -1;
        }
        if (Input_buttonsPressed.allButtons & 0x20) {
            Menu_PlaySelectSound();
            return selected_direction + 1;
        }
        Menu_Container_transferDirectionOkBlink += 16;
        Render_DrawCompactTextColor("OK", 0x4200C8,
            ((Core_Gte_Sin((Menu_Container_transferDirectionOkBlink) * 8) >> 5) + 0x40) * 0x10101, (u_long *)(((u_char*)(Render_frameScratch.orderingTable)) - 0xC));
        v = 0x1A;
        Menu_Container_transferDirectionCursorAnimation = Menu_Framework_DrawCursor(
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
            Menu_PlayChangeSound();
            ((u_short*)&Menu_Framework_menuMemory)[0x4F] = selected_direction;
        }
        if (selected_direction != 2) {
            direction_help_text = (char*)&Menu_Container_menuText[Menu_Container_menuText[selected_direction * 2 + 13]];
        } else {
            direction_help_text = (char*)&Menu_Container_menuText[Text_Menud_menuText_Offset_endTransfer];
        }
        Menu_Framework_SetInformationMessage(direction_help_text);
        break;
    }
    Menu_Container_DrawStorageCapacities();
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
        Menu_Framework_LoadItemText(1);
        Menu_Framework_InitializeInformationBox();
        Menu_Container_SetPanelVisibility(0);
        Core_Memory_CopyHalfwordBlocks(&Menu_Framework_unusedInventoryStorage->container.items,
            &Menu_Framework_unusedInventoryStorage->storedContainer,
            sizeof Menu_Framework_unusedInventoryStorage->container.items);
        Menu_Container_SnapshotGadgetBag();
        Menu_Container_capacityListSlideStep = 0;
        Menu_Container_directionReopenDelay = 0;
        Menu_Container_categoryTitleSlideStep = 0;
        Menu_Container_categoryIconsSlideStep = 0;
        Menu_Container_controlsPanelSlideStep = 0;
        *screen_step = 1;
        return 0;
    case 1:
        *screen_step += Menu_Framework_LoadItemText(0);
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
                Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
                Menu_Framework_SetMenuItem(0, 0xB4, 0x12, 0x8C, 8,
                    (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_screenResult + 0xF]])
                    ->selected = 1;
                *screen_step = 4;
            } else {
                *screen_step = 6;
            }
        }
        break;
    case 4:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_UpdateItemList(Menu_Container_screenResult);
            *screen_step = 5;
        }
        break;
    case 5:
        Menu_Container_screenResult = Menu_Container_UpdateItemList(0);
        if (Menu_Container_screenResult != 0) {
            Menu_Container_SetPanelVisibility(0);
            if (Menu_Container_screenResult == -2) {
                *screen_step = 6;
            } else {
                Menu_Framework_SetMenuItem(0, 0xB4, 0x12, 0x8C, 8,
                    (char*)&Menu_Container_menuText[Text_Menud_menuText_Offset_container])
                    ->selected = 1;
                Menu_Container_directionReopenDelay = 5;
                *screen_step = 2;
            }
        }
        break;
    case 6:
        if (Menu_Framework_UnloadItemTextWhenReady() != 0) {
            Menu_Container_CommitGadgetBag();
            for (i = 0; i < 7; ++i) {
                Menu_Container_RebuildCategoryIndices(i, &Menu_Framework_unusedInventoryStorage->container);
            }
            for (i = 0; i < 7; ++i) {
                if (Menu_Container_CountItems(i, &Menu_Framework_unusedInventoryStorage->container.items)
                    != 0) {
                    break;
                }
            }
            Input_settings.containerEmpty = i == 7;
            Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(0));
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
                menuItem = Menu_Framework_SetMenuItem(0x1F, i + 0xB4, 0x22, 0x8C, 8,
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
                        Menu_Container_CountItems(itemCategory, &Menu_Container_containerData->items);
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

    Menu_Container_DrawCategoryIcons(Menu_Container_categoryIconsSlideStep, Menu_Container_detailsViewPhase);

    if (Menu_Container_controlsPanelMode != 0) {
        if (Menu_Container_controlsPanelSlideStep == 0) {
            Menu_Container_RefreshMiscTransferCount();
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
        Menu_Framework_DrawButtonPromptBackground(i, 0x26, 0x90, 0xA);
        Menu_Framework_DrawButtonPromptBackground(i, 0x36, 0x48, 0xA);
        control_icon_x = i - 8;
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, control_icon_x, 0x24, NULL);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Triangle, i + 0x40, 0x24, NULL);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Cross, control_icon_x, 0x34, NULL);
        s3 = (i + 0xC) & 0xFFFF;
        Render_DrawCompactTextColor(
            "STATUS", s3 | 0x260000, 0x202020 << (Menu_Container_controlsPanelMode & 3), NULL);
        Render_DrawCompactText("CANCEL", (s3 + 0x48) | 0x260000, NULL);
        Render_DrawCompactTextColor(
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
        Render_DrawCompactText(Menu_Container_kStorageNames[Menu_Container_browsingGadgetBag], vs_getXY_4(i, 0x4C), NULL);
        s3 = 0;
        packed_capacity_row_position = vs_getXY_4(0x70, 0x58);

        for (;; ++s3) {
            Render_DrawCompactText(
                Menu_Container_kCategoryLabels[s3] + 1, vs_getXY_4(8 + i, 0x58 + s3 * 12), NULL);
            if (s3 == 6) {
                break;
            }
            destination_category_count = Menu_Container_CountItems(s3, &Menu_Container_otherStorage->items);
            projected_category_count =
                destination_category_count + Menu_Container_CountItems(s3, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
            if (Menu_Container_browsingGadgetBag != 0) {
                category_capacity = Menu_Container_containerItemCapacities[s3];
            } else {
                category_capacity = Items_Inventory_inventoryItemCapacities[s3];
            }
            Menu_Container_DrawCapacityCount(category_capacity, i + packed_capacity_row_position, projected_category_count);
            packed_capacity_row_position += vs_getXY_4(0, 12);
        }
        Menu_Container_DrawCapacityCount(Menu_Container_browsingGadgetBag != 0 ? 0x100 : 0x40, i + 0xA00070, Menu_Container_transferredMiscCount);
    }
    return 0;
}
#undef vs_getXY_4

// MENUD.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Container_ItemView_ResetMenudStatics(void)
{
    {
        static const char* initial = "X     0";
        memcpy((void*)&Menu_Container_gemSlotCountText, &initial, sizeof Menu_Container_gemSlotCountText);
    }
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
