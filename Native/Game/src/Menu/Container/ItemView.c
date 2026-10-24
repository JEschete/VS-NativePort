// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
u_short Menu_Container_menuText[690]; /* contents: disc string table (ModuleData.c) */

// decomp: vs_menuD_containerData (vs_menuD_containerData@MENU/MENUD.PRG)
Items_Container* Menu_Container_containerData;

// decomp: MENUD.PRG func_80103D50 (src/MENU/MENUD.PRG/234.c:659)
void Menu_Container_SetPanelVisibility(char arg0)
{
    Menu_Container_categoryTitleVisible = arg0;
    Menu_Container_categoryIconsVisible = arg0;
    Menu_Container_controlsPanelMode = arg0;
}

// decomp: MENUD.PRG _pollItemNavigation (src/MENU/MENUD.PRG/234.c:666)
int Menu_Container_PollDetailsPaging(int itemCategory, int currentSelection)
{
    int previousSelection;
    int count;
    int temp_a0;

    if ((Input_buttonsState & (PADL1 | PADR1)) != (PADL1 | PADR1)) {

        previousSelection = currentSelection;
        count = Menu_Container_CountItems(itemCategory, &Menu_Container_containerData->items);
        temp_a0 = count;

        if (Input_buttonRepeat & PADL1) {
            int temp = currentSelection - 1;
            currentSelection = temp + count;
        }

        do {
            if (Input_buttonRepeat & PADR1) {
                ++currentSelection;
            }
            if (currentSelection >= temp_a0) {
                currentSelection -= temp_a0;
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
void Menu_Container_DrawCategoryIcons(int arg0, int arg1)
{
    int i;
    u_long* var_v1;
    Menu_Framework_MenuRow* temp_v0;

    int temp_s4 = (Menu_Framework_menuMemory.opaque85[0x1B] - 1) & 7;

    if (arg0 == 4) {
        Menu_Framework_DrawTabNavigation(0);
    }

    arg0 = (arg0 * 8) - 16;

    for (i = 0; i < 8; ++i) {

        int temp_s0 = (((i + 1) & 7) * 0x10) + 0x20;

        if ((i == temp_s4) && (arg1 == 1)) {
            temp_v0 = Menu_Framework_MenuItem(0x1F);
            if (temp_s0 >= (temp_v0->x - 0xC)) {
                temp_v0->rowIcon = i + 0x18;
                continue;
            } else {
                var_v1 = Render_QueueDefaultSprite(0x100010, temp_s0 | 0x100000);
            }
        } else {
            var_v1 = Render_QueueSpriteWithTexturePage(
                0x80, temp_s0 | (arg0 << 0x10), 0x100010, Render_frameScratch.orderingTable - 0xC);
        }

        var_v1[4] = (0x78 + i * 0x10) | 0x8000 | (i == temp_s4 ? 0x37FD0000 : 0x37FE0000);
    }
}

// decomp: MENUD.PRG func_80103F64 (src/MENU/MENUD.PRG/234.c:735)
void Menu_Container_BeginDetailsTransition(int arg0)
{
    Menu_Framework_MenuRow* menuItem;

    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

    menuItem = Menu_Framework_MenuItem(0x1F);
    menuItem->state = 3;
    menuItem->targetPosition0 = 0x12;

    menuItem = Menu_Framework_MenuItem(arg0);
    menuItem->state = 2;
    menuItem->targetPosition0 = 0x9B;
    menuItem->selected = 1;
    menuItem->subText = NULL;
    menuItem->unselectable = 0;
}

// decomp: MENUD.PRG func_80104034 (src/MENU/MENUD.PRG/234.c:767)
void Menu_Container_InitializeDetailsLayout(int arg0, int arg1)
{
    Menu_Container_categoryIconsVisible = 0;
    Menu_Container_controlsPanelMode = 0;
    Menu_Framework_ContinueItemDetailsTransition(arg0);
    Menu_Framework_UpdateEquipmentStatPages(arg1);
    Menu_Framework_UpdatePrimaryStatPanel(1);
}

// decomp: MENUD.PRG func_80104114 (src/MENU/MENUD.PRG/234.c:789)
int Menu_Container_SelectDetailsItem(int arg0, int arg1)
{
    Menu_Framework_menuMemory.cursorMemories[(arg0 + 0x51) * 2] = 0;
    Menu_Framework_menuMemory.cursorMemories[(arg0 + 0x51) * 2 + 1] = arg1;
    return Menu_Container_CategoryIndices(arg0, Menu_Container_containerData)[arg1];
}

// decomp: MENUD.PRG func_80104170 (src/MENU/MENUD.PRG/234.c:796)
void Menu_Container_ExitDetailsView(int arg0)
{
    Menu_PlayLeaveSound();
    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
    Menu_Framework_FlyOutRowLeft(-1);
    Menu_Framework_UpdateEquipmentStatPages(-1);
    Menu_Framework_UpdatePrimaryStatPanel(2);
    Menu_Container_detailsViewPhase = 2;
    Menu_Container_detailsClosedWithTriangle = Input_buttonsPressed.allButtons & PADRup;
    if (arg0 != 0) {
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
    int sp18;
    int selectedBlade;
    int temp_v0_3;

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
                temp_v0_3 = Menu_Container_SelectDetailsItem(1, selectedBlade);
                Menu_Framework_FormatBladeRow(
                    &Menu_Container_containerData->items.blades[temp_v0_3 - 1], text, &sp18,
                    Menu_Framework_stringBuf);
                Menu_Framework_SetBladeStats(temp_v0_3);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_bladeDetailsItemSource, text, sp18, selectedBlade);
            }
        }
        break;
    case Menu_Container_EquipmentDetailsState_Exit:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG _gripNavigation (src/MENU/MENUD.PRG/234.c:965)
int Menu_Container_UpdateGripDetails(int arg0)
{

    static char Menu_Container_gripDetailsStep;
    static char Menu_Container_gripDetailsSlideStep;
    static char Menu_Container_gripDetailsItemSource;
    static u_char Menu_Container_gripDetailsItemIndex;
    static char Menu_Container_unusedGripDetailsFlag;

    char* sp10[2];
    int sp18;
    int temp_v0_2;
    int temp_v0_3;

    if (arg0 != 0) {
        Menu_Container_gripDetailsItemSource = arg0 >> 8;
        Menu_Container_gripDetailsItemIndex = arg0 - 1;
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
            temp_v0_2 = Menu_Container_PollDetailsPaging(2, Menu_Container_gripDetailsItemIndex);
            if (temp_v0_2 != Menu_Container_gripDetailsItemIndex) {
                Menu_Container_gripDetailsItemIndex = temp_v0_2;
                temp_v0_3 = Menu_Container_SelectDetailsItem(2, temp_v0_2);
                Menu_Framework_FormatGripRow(&Menu_Container_containerData->items.grips[temp_v0_3 - 1],
                    sp10, &sp18, Menu_Framework_stringBuf);
                Menu_Framework_SetGripStats(temp_v0_3);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_gripDetailsItemSource, sp10, sp18, temp_v0_2);
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
int Menu_Container_UpdateShieldDetails(int arg0)
{
    static char Menu_Container_shieldDetailsStep;
    static char Menu_Container_shieldDetailsSlideStep;
    static char Menu_Container_shieldDetailsItemSource;
    static u_char Menu_Container_shieldDetailsItemIndex;
    static char Menu_Container_unusedShieldDetailsFlag;

    char* sp18[2];
    int sp20;
    int temp_v0_2;
    int i;

    if (arg0 != 0) {
        Menu_Container_shieldDetailsItemSource = arg0 >> 8;
        Menu_Container_shieldDetailsItemIndex = arg0 - 1;
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
            temp_v0_2 = Menu_Container_PollDetailsPaging(3, Menu_Container_shieldDetailsItemIndex);
            if (temp_v0_2 != Menu_Container_shieldDetailsItemIndex) {
                Menu_Container_shieldDetailsItemIndex = temp_v0_2;
                i = Menu_Container_SelectDetailsItem(3, temp_v0_2);
                Menu_Container_BuildShieldRow(&Menu_Container_containerData->items, sp18, &sp20,
                    Menu_Framework_stringBuf, i - 1);
                Menu_Framework_SetShieldStats(i);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_shieldDetailsItemSource, sp18, sp20, temp_v0_2);
                for (i = 1; i < 4; ++i) {
                    Menu_Framework_CreateShieldGemRow(
                        i, Menu_Container_containerData->indices.shields[temp_v0_2], 0);
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
int Menu_Container_UpdateArmorDetails(int arg0)
{
    static char Menu_Container_armorDetailsStep;
    static char Menu_Container_armorDetailsDelay;
    static char Menu_Container_armorDetailsItemSource;
    static u_char Menu_Container_armorDetailsItemIndex;

    char* sp10[2];
    int sp18;
    int temp_v0_2;
    int temp_v0_3;

    if (arg0 != 0) {
        Menu_Container_armorDetailsItemSource = arg0 >> 8;
        Menu_Container_armorDetailsItemIndex = (arg0 - 1);
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
            temp_v0_2 = Menu_Container_PollDetailsPaging(4, Menu_Container_armorDetailsItemIndex);
            if (temp_v0_2 != Menu_Container_armorDetailsItemIndex) {
                Menu_Container_armorDetailsItemIndex = temp_v0_2;
                temp_v0_3 = Menu_Container_SelectDetailsItem(4, temp_v0_2);
                Menu_Framework_FormatInventoryArmorRow(
                    &Menu_Container_containerData->items.armor[temp_v0_3 - 1], sp10, &sp18,
                    Menu_Framework_stringBuf);
                Menu_Framework_SetArmorStats(temp_v0_3);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_armorDetailsItemSource, sp10, sp18, temp_v0_2);
            }
        }
        break;
    case 3:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG _gemNavigation (src/MENU/MENUD.PRG/234.c:1176)
int Menu_Container_UpdateGemDetails(int arg0)
{
    static u_char Menu_Container_gemDetailsStep;
    static u_char Menu_Container_gemDetailsDelay;
    static u_char Menu_Container_gemDetailsItemSource;
    static u_char Menu_Container_gemDetailsItemIndex;

    char* sp10[2];
    int sp18;
    int temp_v0_2;
    int temp_v0_3;

    if (arg0 != 0) {
        Menu_Container_gemDetailsItemSource = arg0 >> 8;
        Menu_Container_gemDetailsItemIndex = arg0 - 1;
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
            temp_v0_2 = Menu_Container_PollDetailsPaging(5, Menu_Container_gemDetailsItemIndex);
            if (temp_v0_2 != Menu_Container_gemDetailsItemIndex) {
                Menu_Container_gemDetailsItemIndex = temp_v0_2;
                temp_v0_3 = Menu_Container_SelectDetailsItem(5, temp_v0_2);
                Menu_Framework_FormatGemRow(&Menu_Container_containerData->items.gems[temp_v0_3 - 1],
                    sp10, &sp18, Menu_Framework_stringBuf);
                Menu_Framework_SetGemStats(temp_v0_3);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Container_gemDetailsItemSource, sp10, sp18, temp_v0_2);
            }
        }
        break;
    case 3:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENUD.PRG func_80105008 (src/MENU/MENUD.PRG/234.c:1235)
int Menu_Container_UpdateItemDetails(int arg0)
{
    static int (*Menu_Container_kItemDetailHandlers[])(int) = { Menu_Container_UpdateWeaponDetails, Menu_Container_UpdateBladeDetails,
        Menu_Container_UpdateGripDetails, Menu_Container_UpdateShieldDetails, Menu_Container_UpdateArmorDetails, Menu_Container_UpdateGemDetails };

    static u_char Menu_Container_detailsCategory;

    int temp_v0;
    int var_s0 = 0;

    if (arg0 != 0) {
        var_s0 = arg0 >> 4;
        Menu_Container_detailsCategory = arg0 & 0xF;
        Menu_Container_detailsViewPhase = 1;
        Menu_Container_detailsClosedWithTriangle = 0;
        Menu_Framework_ResetItemDetailCursor();
        Menu_Framework_MenuItem(0x1F)->itemPage = var_s0 & 0xFF;
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
    }

    temp_v0 = Menu_Container_kItemDetailHandlers[Menu_Container_detailsCategory](var_s0);

    if (temp_v0 != 0) {
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
    return temp_v0;
}

// decomp: MENUD.PRG func_80106464 (src/MENU/MENUD.PRG/234.c:1925)
void Menu_Container_DrawCapacityCount(int arg0, int arg1, int arg2)
{
    int var_s2;
    int temp_s0;

    var_s2 = 0x808080;
    Menu_Framework_DrawColoredInteger(arg0, arg1, 0x808080, 0);
    temp_s0 = arg1 - 0x20;
    Render_DrawCompactTextColor("/", temp_s0, 0x808080, NULL);
    if ((arg2 * 2) < arg0) {
        var_s2 = 0x804020;
    }
    Menu_Framework_DrawColoredInteger(arg2, temp_s0, var_s2, 0);
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
void Menu_Container_RestoreItemRows(int arg0)
{
    int temp_v0_2;
    int i;

    while (Menu_Framework_itemsListRow < 16) {
        Menu_Framework_UpdateRowSelection();
    }

    for (i = 20; i < 40; ++i) {

        Menu_Framework_MenuRow* temp_v0 = Menu_Framework_MenuItem(i);

        if (temp_v0->state == 2) {
            temp_v0->state = 1;
            temp_v0->x = temp_v0->targetPosition0;
        }

        temp_v0_2 = i ^ (Menu_Framework_menuMemory.cursorMemories[(arg0 + 81) * 2] + 20);
        temp_v0->selected = temp_v0_2 == 0;
    }
}

// decomp: MENUD.PRG func_801072B0 (src/MENU/MENUD.PRG/234.c:2332)
int Menu_Container_UpdateTransferConfirmation(int arg0)
{
    static int Menu_Container_transferConfirmationSelectedAction;
    static char Menu_Container_transferConfirmationStep;
    static char Menu_Container_transferConfirmationLayoutPadding[3] __attribute__((unused));

    char* sp10[4];
    int sp20[2];
    int i;
    int temp_s0;

    if (arg0 != 0) {
        Menu_InitInformationTextBox(1);
        Menu_Container_controlsPanelMode = 5;
        Menu_Inventory_freezeTabArrows = 0;
        Menu_Container_transferConfirmationSelectedAction = 0;
        Menu_Container_transferConfirmationStep = 0;
        return 0;
    }

    temp_s0 = Menu_Container_transferConfirmationStep;

    switch (temp_s0) {
    case 0:
        for (i = 0; i < 2; ++i) {
            sp10[i * 2] = (char*)&Menu_Container_menuText[Menu_Container_menuText[8 + i]];
            sp10[i * 2 + 1] = (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag + 10]];
            sp20[i] = 0;
        }

        Menu_Framework_CreateActionRows(2, 3, sp10, sp20);
        Menu_Container_transferConfirmationStep = 1;
        break;
    case 1:
        Menu_Framework_UpdateActionMenu();
        Menu_Container_transferConfirmationSelectedAction = Menu_Framework_ActionSelectionResult() + 1;

        if (Menu_Container_transferConfirmationSelectedAction != 0) {
            Menu_Framework_FlyOutRowLeft(0);
            Menu_Framework_FlyOutRowLeft(1);
            if (Menu_Container_transferConfirmationSelectedAction == temp_s0) {
                Menu_PlaySelectSound();
                Menu_Framework_ClearMenuExcept(0);
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
int Menu_Container_UpdateItemList(int arg0)
{
    static char Menu_Container_itemListStep;
    static char Menu_Container_categoryEmpty;
    static char Menu_Container_itemListFirstEntry;
    static char Menu_Container_itemListLayoutPadding __attribute__((unused));
    static int Menu_Container_itemListSelectedRow;

    u_short* sp10[8];
    int temp_a0;
    int temp_s0;
    int temp_s2;
    int temp_s3;
    int temp_v0_17;
    int temp_v0_18;
    int temp_v0_8;
    int var_a1;
    int var_a2;
    int var_s4;
    int var_v0_3;
    int* temp_s1_3;
    int* temp_s1_4;
    int* temp_s6;
    char** temp_s0_2;
    char** temp_s3_7;
    char** temp_s5;
    u_short* temp_s1;
    void* temp_v0_19;
    void* temp_v0_7;
    Menu_Framework_MenuRow* temp_v0_11;

loop_1:
    if (arg0 != 0) {
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
        Menu_Container_browsingGadgetBag = arg0 - 1;
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
    var_s4 = (Menu_Framework_menuMemory.opaqueA0[0] - 1) & 7;
    temp_s1 = Menu_Container_CategoryIndices(var_s4, Menu_Container_containerData);

    switch (Menu_Container_itemListStep) { /* switch 1 */
    case 0:
        Menu_Framework_InitializeInformationBox();
        /* fallthrough */
    case 1:
        temp_s1 = Core_Memory_AllocateFromHighEnd(0x6C00);
        temp_s6 = (void*)temp_s1 + 0x6000;
        temp_s5 = (void*)temp_s1 + 0x6400;
        Menu_Inventory_freezeTabArrows = 1;

        for (temp_s3 = 0; temp_s3 < 7; ++temp_s3) {
            Menu_Container_RebuildCategoryIndices(temp_s3, Menu_Container_containerData);
        }

        Menu_Container_RebuildPendingTransfers();
        temp_s2 = Menu_Container_BuildItemRows(var_s4, temp_s5, temp_s6, (char*)temp_s1);
        Menu_Container_categoryEmpty = temp_s2 == 0;
        if (Menu_Container_categoryEmpty != 0) {
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag + 27 + (var_s4 == 7) * 2]]);
        } else {
            temp_s3 = Input_settings.cursorMemory;
            if (Menu_Container_itemListFirstEntry == 0) {
                Input_settings.cursorMemory = 1;
            }
            Menu_Container_itemListFirstEntry = 0;
            Menu_Framework_CreateMenuRows(temp_s2, (var_s4 + 0x51) | 0x19200, temp_s5, temp_s6);
            if (Menu_Container_itemListStep == 0) {
                Menu_Container_RestoreItemRows(var_s4);
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
                if (var_s4 == 7) {
                    Menu_Container_UpdateTransferConfirmation(1);
                    Menu_Container_itemListStep = 0xF;
                } else {
                    Menu_Framework_menuMemory.opaqueA0[0] = 0;
                    Menu_Framework_ClearMenuExcept(0);
                    Menu_Container_itemListStep = 0xD;
                }
                break;
            }
            if (Input_buttonsPressed.allButtons & 0xA0) {
                Menu_PlayInvalidSound();
            }
        } else {
            int s6 = 1;
            temp_s0 = var_s4 < 6;
            Menu_Container_controlsPanelMode = temp_s0 + 1;
            Menu_Container_itemListSelectedRow = Menu_Framework_GetSelectedRow() + 1;
            if (Menu_Container_itemListSelectedRow == 0) {
                if (Input_buttonsPressed.allButtons & 0x80) {
                    if ((temp_s0 != 0) && (Menu_Framework_AreMenuRowsIdle() != 0)) {
                        Menu_PlaySelectSound();
                        temp_s0 = Menu_Framework_CloseRowsAndRememberCursor();
                        Menu_Framework_MenuItem(temp_s0 >> 8)->outsetIcon = 0;
                        Menu_Container_UpdateItemDetails(var_s4 | ((temp_s0 + s6) * 0x10));
                        Menu_Container_categoryTitleVisible = 0;
                        Menu_Container_itemListStep = 7;
                        break;
                    }
                    Menu_PlayInvalidSound();
                }
            } else if (Menu_Container_itemListSelectedRow > 0) {
                if (var_s4 == 7) {
                    Menu_Container_UndoPendingTransfer(Menu_Container_itemListSelectedRow - 1);
                    Menu_Container_RefreshMiscTransferCount();
                    Menu_Framework_ClearMenuExcept(0);
                    Menu_Container_itemListStep = 4;
                } else {
                    temp_v0_7 = Core_Memory_AllocateFromHighEnd(0x6C00U);
                    temp_s0_2 = temp_v0_7 + 0x6400;
                    temp_s1_3 = temp_v0_7 + 0x6000;
                    temp_s0 = Menu_Container_BuildItemRows(var_s4, temp_s0_2, temp_s1_3, temp_v0_7);
                    temp_s1_3[Menu_Container_itemListSelectedRow - 1] |= 8;
                    temp_s3 = Input_settings.cursorMemory;
                    Input_settings.cursorMemory = 1;
                    Menu_Framework_CreateMenuRows(
                        temp_s0, (var_s4 + 0x51) | 0x19200, temp_s0_2, temp_s1_3);
                    Input_settings.cursorMemory = temp_s3;
                    Menu_Container_RestoreItemRows(var_s4);
                    Menu_Container_controlsPanelMode = 1;
                    Menu_Inventory_freezeTabArrows = 0;
                    Core_Memory_FreeFromHighEnd(temp_v0_7);
                    Menu_Container_itemListStep = 6;
                }
            } else if (Menu_Container_itemListSelectedRow == -2) {
                return -2;
            } else {
                if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[0] == 0) {
                    Menu_Framework_ClearMenuExcept(0);
                    return -1;
                }
                if (var_s4 != 7) {
                    Menu_Framework_menuMemory.opaqueA0[0] = 0;
                    Menu_Framework_ClearMenuExcept(0);
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
        temp_v0_8 = (temp_s3 != 0);
        if (temp_s2 != 0) {
            temp_s0 = temp_v0_8 ^ 1;
        } else {
            temp_s0 = temp_v0_8 & 1;
        }
        if (temp_s0 != 0) {
            var_a2 = 0x8004;
            if (temp_s3 == 0x8004) {
                var_a2 = 0x8000;
            }
            if (Input_buttonRepeat & var_a2) {
                var_s4 = (var_s4 - 1) & 7;
            }
            var_a2 = 0x2008;
            if (temp_s2 == 0x2008) {
                var_a2 = 0x2000;
            }
            var_v0_3 = Input_buttonRepeat & var_a2;
            if (var_v0_3 != 0) {
                var_s4 = (var_s4 + 1) & 7;
            }
            if (var_s4 != ((Menu_Framework_menuMemory.opaqueA0[0] - 1) & 7)) {
                Menu_PlayChangeSound();
                Menu_Framework_menuMemory.opaqueA0[0] = (var_s4 + 1) & 7;
                if (Menu_Container_itemListStep == 2) {
                    if (Menu_Container_categoryEmpty == 0) {
                        Menu_Framework_CloseRowsAndRememberCursor();
                        Menu_Framework_ClearMenuExcept(0);
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
                arg0 = 0;
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
                if ((var_s4 == 6)
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
                Menu_Framework_ClearMenuExcept(0);
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
            temp_v0_11 = Menu_Framework_MenuItem(0);
            if (temp_v0_11->x >= 0xB5) {
                if (temp_v0_11->x >= 0xCD) {
                    temp_v0_11->x = (u_short)temp_v0_11->x - 0x18;
                }
                temp_v0_11->state = 2;
                temp_v0_11->targetPosition0 = 0xB4;
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
        switch (var_s4) {
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

        var_a2 = Menu_Container_ParentWeaponIndex(var_s4, temp_s3, &Menu_Container_containerData->items);
        var_a1 = 8;

        if (var_a2 != 0) {
            temp_s0 = 1;
            temp_s2 =
                Menu_Container_menuText[Menu_Container_containerData->items.weapons[var_a2 - 1].isEquipped == 0
                              ? 5
                              : 4];
        }

        var_a2 = Menu_Container_ParentShieldIndex(var_s4, temp_s3, &Menu_Container_containerData->items);
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
            sp10[1] = &Menu_Container_menuText[temp_s2];
        } else if (var_s4 == 6) {
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
            sp10[1] = &Menu_Container_menuText[temp_s2];
        } else {
            var_a2 = Menu_Container_TransferCapacityError(var_s4, temp_s3);
            sp10[1] = &Menu_Container_menuText[Menu_Container_menuText[var_a2 + 5]];
        }

        sp10[0] = &Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag * 2 + 0x1F]];

        if (var_a2 != 0) {
            Menu_Framework_rowTypeBuf[0] = 1;
        } else {
            sp10[1] = &Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag * 2 + 0x20]];
        }
        Menu_Container_itemActionIds[1] = 1;
        temp_s2 = 1;
        if (((u_int)(var_s4 - 1) < 2) || ((var_s4 == 5))) {
            sp10[2] = Menu_Framework_itemHelp + 0x3527;
            sp10[3] = Menu_Framework_itemHelp + 0x352C;
            if ((Menu_Container_ParentWeaponIndex(var_s4, temp_s3, &Menu_Container_containerData->items) == 0)
                && (Menu_Container_ParentShieldIndex(var_s4, temp_s3, &Menu_Container_containerData->items)
                    == 0)) {
                sp10[3] = Menu_Framework_itemHelp + 0x35B2;
                Menu_Framework_rowTypeBuf[1] = 1;
            }
            ++temp_s2;
            Menu_Container_itemActionIds[temp_s2] = 2;
        }
        temp_s0 = 0;
        sp10[temp_s2 * 2] = Menu_Framework_itemHelp + 0x354A;
        sp10[temp_s2 * 2 + 1] = Menu_Framework_itemHelp + 0x354F;
        switch (var_s4) {
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
        var_a2 = Menu_Container_ParentWeaponIndex(var_s4, temp_s3, &Menu_Container_containerData->items);
        if (var_a2 != 0) {
            temp_s0 = 1;
            if (Menu_Container_containerData->items.weapons[var_a2 - 1].isEquipped == 0) {
                temp_s0 = 2;
            }
        }
        var_a2 = Menu_Container_ParentShieldIndex(var_s4, temp_s3, &Menu_Container_containerData->items);
        if (var_a2 != 0) {
            temp_s0 = 1;
            if (Menu_Container_containerData->items.shields[var_a2 - 1].isEquipped == 0) {
                temp_s0 = 2;
            }
        }
        if (temp_s0 != 0) {
            sp10[temp_s2 * 2 + 1] =
                &Menu_Framework_itemHelp[Menu_Framework_itemHelp[temp_s0 + 663]];
            temp_s0 = 1;
        }
        Menu_Framework_rowTypeBuf[temp_s2++] = temp_s0;
        Menu_Container_itemActionIds[temp_s2] = 3;
        if (var_s4 == 6) {
            sp10[temp_s2 * 2] = Menu_Framework_itemHelp + 0x356D;
            sp10[temp_s2 * 2 + 1] = Menu_Framework_itemHelp + 0x3574;
            if (Menu_Container_containerData->items.misc[temp_s3].id >= 0x1CA) {
                sp10[temp_s2 * 2 + 1] = Menu_Framework_itemHelp + 0x350E;
                Menu_Framework_rowTypeBuf[temp_s2] = 1;
            }
            ++temp_s2;
            Menu_Container_itemActionIds[temp_s2] = 4;
        }

        sp10[temp_s2 * 2] = Menu_Framework_itemHelp + 0x3586;
        sp10[temp_s2 * 2 + 1] = Menu_Framework_itemHelp + 0x358A;
        ++temp_s2;
        Menu_Container_itemActionIds[temp_s2] = 5;
        Menu_Framework_CreateActionRows(
            temp_s2, var_s4 + 0x59, (char**)&sp10, (int*)Menu_Framework_rowTypeBuf);
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
                    Menu_Container_StageItemTransfer(var_s4, Menu_Container_CategoryIndices(var_s4,
                                              Menu_Container_containerData)[Menu_Container_itemListSelectedRow - 1]
                                              - 1);
                    Menu_Framework_ClearMenuExcept(0);
                    Menu_Container_itemListStep = 4;
                    break;
                case 2:
                    Menu_Framework_InitializeInformationBox();
                    Menu_Framework_ClearMenuExcept(0);
                    Menu_Container_itemListStep = 10;
                    break;
                case 3:
                    temp_s2 = temp_s1[Menu_Container_itemListSelectedRow - 1] | (var_s4 << 0xC);
                    if ((var_s4 == 6)
                        && (Menu_Container_containerData->items.misc[temp_s1[Menu_Container_itemListSelectedRow - 1] - 1]
                                .count
                            >= 2)) {
                        temp_s2 |= 0x10000;
                    }
                    Menu_Container_UpdateDiscardMenu(temp_s2);
                    Menu_Container_itemListStep = 0xB;
                    break;
                case 4:
                    Menu_Container_UpdateDiscardMenu(temp_s1[Menu_Container_itemListSelectedRow - 1] | (var_s4 << 0xC) | 0x20000);
                    Menu_Container_itemListStep = 0xB;
                    break;
                case 5:
                    Menu_Framework_InitializeInformationBox();
                    Menu_Container_UpdateSortMenu(var_s4 + 1);
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
                var_s4, temp_s1[Menu_Container_itemListSelectedRow - 1] - 1, &Menu_Container_containerData->items);
            if (temp_s3 != 0) {
                var_s4 = 0;
            } else {
                temp_s3 = Menu_Container_ParentShieldIndex(
                    var_s4, temp_s1[Menu_Container_itemListSelectedRow - 1] - 1, &Menu_Container_containerData->items);
                var_s4 = 3;
            }
            temp_s2 = Menu_Container_CountItems(var_s4, &Menu_Container_containerData->items);
            temp_v0_17 = Menu_Container_FindItemRow(var_s4, temp_s3 - 1,
                Menu_Container_CategoryIndices(var_s4, Menu_Container_containerData));
            temp_s3 = temp_v0_17 - 1;
            if ((temp_s2 < 9) || (temp_s3 < 8)) {
                temp_s2 = 0;
            } else {
                temp_a0 = (temp_s2 - 8);
                if (temp_a0 < temp_s3) {
                    temp_s2 = temp_a0;
                    temp_s3 -= temp_s2;
                } else {
                    temp_s2 = temp_v0_17 - 2;
                    temp_s3 = 1;
                }
            }
            Menu_Framework_menuMemory.opaqueA0[0] = (var_s4 + 1) & 7;
            temp_v0_18 = (var_s4 + 0x51) * 2;
            Menu_Framework_menuMemory.cursorMemories[temp_v0_18] = temp_s3;
            Menu_Framework_menuMemory.cursorMemories[temp_v0_18 + 1] = temp_s2;
            Menu_Container_itemListStep = 1;
        }
        break;
    case 11:
        temp_s3 = Menu_Container_UpdateDiscardMenu(0);
        if (temp_s3 != 0) {
            Menu_Framework_InitializeInformationBox();
            Menu_Framework_FlyOutRowsLeftExcept(0x28);
            Menu_Framework_ClearMenuExcept(0);
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
            Menu_Framework_ClearMenuExcept(0);
            if (temp_s3 == -2) {
                return -2;
            }
            Menu_Container_itemListStep = 4;
        }
        break;
    case 13:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            temp_v0_19 = Core_Memory_AllocateFromHighEnd(0x6C00U);
            temp_s3_7 = temp_v0_19 + 0x6400;
            temp_s1_4 = temp_v0_19 + 0x6000;
            temp_s2 = Menu_Container_BuildItemRows(7, temp_s3_7, temp_s1_4, temp_v0_19);
            Menu_Container_categoryEmpty = temp_s2 == 0;
            if (Menu_Container_categoryEmpty != 0) {
                Menu_Framework_SetInformationMessage(
                    (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_browsingGadgetBag + 0x1D]]);
            } else {
                Menu_Framework_CreateMenuRows(
                    temp_s2, (var_s4 + 0x51) | 0x19200, temp_s3_7, temp_s1_4);
            }
            Core_Memory_FreeFromHighEnd(temp_v0_19);
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
                Menu_Framework_ClearMenuExcept(0);
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
int Menu_Container_UpdateTransferDirection(int arg0)
{
    static char Menu_Container_transferDirectionLayoutPadding __attribute__((unused)) = 1;

    static char Menu_Container_transferDirectionStep;
    static char Menu_Container_transferDirectionLayoutPadding2[3] __attribute__((unused));

    int i;
    int var_s1;
    Menu_Framework_MenuRow* temp_v0_2;
    int v;
    char* a0;

    if (arg0 != 0) {
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
        temp_v0_2 =
            Menu_Framework_SetMenuItem(Menu_Container_transferDirectionStep + 0xA, 0x140, (Menu_Container_transferDirectionStep * 0x10) + 0x22,
                0x7E, 0, (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_transferDirectionStep * 2 + 12]]);
        temp_v0_2->state = 2;
        temp_v0_2->targetPosition0 = 0xC2;
        ++Menu_Container_transferDirectionStep;
        break;
    case 2:
        Menu_Container_transferDirectionStep += Menu_Framework_AreMenuRowsIdle();
        break;
    case 3:
        var_s1 = Menu_Framework_menuMemory.opaque9E;

        for (i = 0; i < 2; ++i) {
            Menu_Framework_MenuItem(i + 0xA)->selected = i == var_s1;
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
            return var_s1 + 1;
        }
        Menu_Container_transferDirectionOkBlink += 16;
        Render_DrawCompactTextColor("OK", 0x4200C8,
            ((Core_Gte_Sin((Menu_Container_transferDirectionOkBlink) * 8) >> 5) + 0x40) * 0x10101, Render_frameScratch.orderingTable - 0xC);
        v = 0x1A;
        Menu_Container_transferDirectionCursorAnimation = Menu_Framework_DrawCursor(
            Menu_Container_transferDirectionCursorAnimation, (((var_s1 * 0x10) - (((var_s1 * 2) & 4) - v)) << 0x10) | 0xB4);
        if (Input_buttonRepeat & 0x1000) {
            var_s1 += 2;
        }
        if (Input_buttonRepeat & 0x4000) {
            ++var_s1;
        }
        if (var_s1 >= 3) {
            var_s1 -= 3;
        }
        if (var_s1 != ((u_short*)&Menu_Framework_menuMemory)[0x4F]) {
            Menu_PlayChangeSound();
            ((u_short*)&Menu_Framework_menuMemory)[0x4F] = var_s1;
        }
        if (var_s1 != 2) {
            a0 = (char*)&Menu_Container_menuText[Menu_Container_menuText[var_s1 * 2 + 13]];
        } else {
            a0 = (char*)&Menu_Container_menuText[Text_Menud_menuText_Offset_endTransfer];
        }
        Menu_Framework_SetInformationMessage(a0);
        break;
    }
    Menu_Container_DrawStorageCapacities();
    return 0;
}

// decomp: src/MENU/MENUD.PRG/234.c:3128
#define vs_getXY_4(x, y) (((x)) + ((y) << 16))
// decomp: MENUD.PRG vs_menuD_exec (src/MENU/MENUD.PRG/234.c:3130)
int Menu_Container_UpdateScreen(u_char* arg0)
{
    static u_int Menu_Container_screenResult;
    static char Menu_Container_directionReopenDelay;
    static u_char Menu_Container_capacityListSlideStep;

    int temp_a2;
    int temp_s0;
    int temp_s0_2;
    int i;
    int var_s6;
    int var_a0;
    int var_v0;
    Menu_Framework_MenuRow* menuItem;

    Menu_Framework_weapons = Menu_Container_containerData->items.weapons;
    Menu_Framework_blades = Menu_Container_containerData->items.blades;
    Menu_Framework_grips = Menu_Container_containerData->items.grips;
    Menu_Framework_shields = Menu_Container_containerData->items.shields;
    Menu_Framework_armor = Menu_Container_containerData->items.armor;
    Menu_Framework_gems = Menu_Container_containerData->items.gems;

    switch (*arg0) {
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
        *arg0 = 1;
        return 0;
    case 1:
        *arg0 += Menu_Framework_LoadItemText(0);
        break;
    case 2:
        if (Menu_Container_directionReopenDelay != 0) {
            --Menu_Container_directionReopenDelay;
        } else {
            Menu_Container_UpdateTransferDirection(1);
            *arg0 = 3;
        }
        break;
    case 3:
        Menu_Container_screenResult = Menu_Container_UpdateTransferDirection(0);
        if (Menu_Container_screenResult != 0) {
            if (Menu_Container_screenResult < 3) {
                Menu_Framework_ClearMenuExcept(0);
                Menu_Framework_SetMenuItem(0, 0xB4, 0x12, 0x8C, 8,
                    (char*)&Menu_Container_menuText[Menu_Container_menuText[Menu_Container_screenResult + 0xF]])
                    ->selected = 1;
                *arg0 = 4;
            } else {
                *arg0 = 6;
            }
        }
        break;
    case 4:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Container_UpdateItemList(Menu_Container_screenResult);
            *arg0 = 5;
        }
        break;
    case 5:
        Menu_Container_screenResult = Menu_Container_UpdateItemList(0);
        if (Menu_Container_screenResult != 0) {
            Menu_Container_SetPanelVisibility(0);
            if (Menu_Container_screenResult == -2) {
                *arg0 = 6;
            } else {
                Menu_Framework_SetMenuItem(0, 0xB4, 0x12, 0x8C, 8,
                    (char*)&Menu_Container_menuText[Text_Menud_menuText_Offset_container])
                    ->selected = 1;
                Menu_Container_directionReopenDelay = 5;
                *arg0 = 2;
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
            Menu_Framework_ClearMenuExcept(0);
            Menu_Framework_menuState.currentMenu = Menu_Framework_menuState.returnState;
            Menu_Framework_menuState.returnState = Menu_Container_screenResult + 2;
            *arg0 = 0;
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
            if (*arg0 != 6) {
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
        temp_s0 = i - 8;
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, temp_s0, 0x24, NULL);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Triangle, i + 0x40, 0x24, NULL);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Cross, temp_s0, 0x34, NULL);
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
            var_v0 = Menu_Container_capacityListSlideStep << 5;
        } else {
            if (Menu_Container_capacityListSlideStep != 0) {
                --Menu_Container_capacityListSlideStep;
            }
            var_v0 = Menu_Framework_rowAnimationSteps[Menu_Container_capacityListSlideStep];
        }

        i -= var_v0;
        Render_DrawCompactText(Menu_Container_kStorageNames[Menu_Container_browsingGadgetBag], vs_getXY_4(i, 0x4C), NULL);
        s3 = 0;
        var_s6 = vs_getXY_4(0x70, 0x58);

        for (;; ++s3) {
            Render_DrawCompactText(
                Menu_Container_kCategoryLabels[s3] + 1, vs_getXY_4(8 + i, 0x58 + s3 * 12), NULL);
            if (s3 == 6) {
                break;
            }
            temp_s0_2 = Menu_Container_CountItems(s3, &Menu_Container_otherStorage->items);
            temp_a2 =
                temp_s0_2 + Menu_Container_CountItems(s3, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
            if (Menu_Container_browsingGadgetBag != 0) {
                var_a0 = Menu_Container_containerItemCapacities[s3];
            } else {
                var_a0 = Items_Inventory_inventoryItemCapacities[s3];
            }
            Menu_Container_DrawCapacityCount(var_a0, i + var_s6, temp_a2);
            var_s6 += vs_getXY_4(0, 12);
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
        static const char initial[8];
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

