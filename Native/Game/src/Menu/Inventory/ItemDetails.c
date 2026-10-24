// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Feedback.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemDetails.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Sounds.h"
#include "Render/Font.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/UiPrimitives.h"

// decomp: local static of _itemStatusDispatcher, at file scope so the module reset reaches it
static int (*Menu_Inventory_itemDetailsStatusViewDispatch[])(int) = { Menu_Inventory_UpdateWeaponDetails, Menu_Inventory_UpdateBladeDetails,
        Menu_Inventory_UpdateGripDetails, Menu_Inventory_UpdateShieldDetails, Menu_Inventory_UpdateArmorDetails, Menu_Inventory_UpdateGemDetails };

// decomp: local static of _gripStatusView, at file scope so the module reset reaches it
static char* Menu_Inventory_gripDetailsGemSlotCountBuf = "X     0";

// decomp: MENU _toggleMenuUi (src/MENU/_toggleUi.h:9)
void Menu_Inventory_SetPanelVisibility(char state)
{
    Menu_Inventory_secondaryTitleEnabled = state;
    Menu_Inventory_itemCategoryIconsEnabled = state;
    Menu_Inventory_statusCommandState = state;
}

// decomp: MENU _itemPaging (src/MENU/items0.h:15)
int Menu_Inventory_PollDetailsPaging(int items, int page)
{
    if ((Input_buttonsState & (PADL1 | PADR1)) != (PADL1 | PADR1)) {

        int previousPage = page;

        items = Items_Inventory_CountItems(items, NULL);

        if (Input_buttonRepeat & PADL1) {
            page += items - 1;
        }

        if (Input_buttonRepeat & PADR1) {
            ++page;
        }

        if (page >= items) {
            page -= items;
        }

        if ((Input_buttonsPressed.allButtons & (PADL1 | PADR1)) && (page == previousPage)) {
            Menu_PlayInvalidSound();
        }
    }

    return page;
}

// decomp: MENU _renderItemCategory (src/MENU/items0.h:46)
void Menu_Inventory_DrawCategoryIcons(int animationStep, int highlightSelected)
{
    int i;
    int selectedCategory = (Menu_Framework_menuMemory.opaque3A.currentCategory - 1) & 7;

    if (animationStep == 4) {
        Menu_Framework_DrawTabNavigation(0);
    }

    animationStep = (animationStep * 8) - 16;

    for (i = 0; i < 8; ++i) {
        u_long* texPage;
        int x = (((i + 1) & 7) * 16) + 32;

        if (i == selectedCategory && highlightSelected == 1) {

            Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(31);

            if (x >= (menuItem->x - 12)) {
                menuItem->rowIcon = i + 24;
                continue;
            }

            texPage = Render_QueueDefaultSprite(vs_getWH(16, 16), x | (16 << 16));

        } else {
            texPage = Render_QueueSpriteWithTexturePage(
                128, x | (animationStep << 0x10), vs_getWH(16, 16), Render_frameScratch.orderingTable);
        }

        texPage[4] = (120 + i * 16) | (128 << 8)
                   | (i == selectedCategory ? vs_getUV0Clut(0, 0, 976, 223)
                                            : vs_getUV0Clut(0, 0, 992, 223));
    }
}

// decomp: MENU _startMenuTransition (src/MENU/items0.h:87)
void Menu_Inventory_BeginDetailsTransition(int id)
{
    Menu_Framework_MenuRow* menuItem;

    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

    menuItem = Menu_Framework_MenuItem(31);
    menuItem->state = Menu_Framework_MenuItemState_SlideY;
    menuItem->targetPosition0 = 18;

    menuItem = Menu_Framework_MenuItem(id);
    menuItem->state = Menu_Framework_MenuItemState_SlideX;
    menuItem->targetPosition0 = 155;
    menuItem->selected = 1;
    menuItem->subText = NULL;
}

// decomp: MENU _initMenuLayout (src/MENU/items1.h:21)
void Menu_Inventory_InitializeDetailsLayout(int selectedItem, int classAffinityType)
{
    Menu_Inventory_ashleyRenderState = 1;
    Menu_Inventory_itemCategoryIconsEnabled = 0;
    Menu_Inventory_statusCommandState = 0;

    Menu_Framework_ContinueItemDetailsTransition(selectedItem);
    Menu_Framework_UpdateEquipmentStatPages(classAffinityType);
    Menu_Framework_UpdatePrimaryStatPanel(1);
}

// decomp: MENU _getItemIndex (src/MENU/items1.h:54)
int Menu_Inventory_SelectDetailsItem(int itemCategory, int itemId)
{
    // Actually accesses struct at unk3A, might originally have been raw
    // pointer manipulation.
    Menu_Framework_menuMemory.cursorMemories[(itemCategory + 30) * 2] = 0;
    Menu_Framework_menuMemory.cursorMemories[(itemCategory + 30) * 2 + 1] = itemId;
    return Menu_Inventory_inventoryIndices[itemCategory][itemId];
}

// decomp: MENU _exitStatusView (src/MENU/items1.h:74)
void Menu_Inventory_ExitDetailsView(int clearDpPp)
{
    Menu_PlayLeaveSound();
    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
    Menu_Framework_FlyOutRowLeft(-1);
    Menu_Framework_UpdateEquipmentStatPages(-1);
    Menu_Framework_UpdatePrimaryStatPanel(2);

    Menu_Inventory_selectedItemCategoryIconOnTop = 2;
    Menu_Inventory_exitToBattle = Input_buttonsPressed.allButtons & PADRup;

    if (clearDpPp != 0) {
        Menu_Framework_UpdateDpPpBars(4);
    }
}

// decomp: MENU _weaponStatusView (src/MENU/items1.h:98)
int Menu_Inventory_UpdateWeaponDetails(int itemInfo)
{
    enum Menu_Inventory_WeaponDetailsState { Menu_Inventory_WeaponDetailsState_Init, Menu_Inventory_WeaponDetailsState_InitWeapon, Menu_Inventory_WeaponDetailsState_ManageInput, Menu_Inventory_WeaponDetailsState_ReturnMenuReady };

    static char Menu_Inventory_weaponDetailsState;
    static char Menu_Inventory_weaponDetailsInitRow;
    static char Menu_Inventory_weaponDetailsItemRow;
    static u_char Menu_Inventory_weaponDetailsItemIndex;
    static char Menu_Inventory_weaponDetailsLayoutPadding;

    char* menuText[2];
    int rowType;
    int newIndex;
    int i;

    if (itemInfo != 0) {
        Menu_Inventory_weaponDetailsItemRow = itemInfo >> 8;
        Menu_Inventory_weaponDetailsItemIndex = (itemInfo - 1) & 0xFF;

        Menu_Inventory_BeginDetailsTransition(Menu_Inventory_weaponDetailsItemRow);

        Menu_Inventory_weaponDetailsLayoutPadding = 0;
        Menu_Inventory_weaponDetailsInitRow = 0;
        Menu_Inventory_weaponDetailsState = Menu_Inventory_WeaponDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_weaponDetailsState) {
    case Menu_Inventory_WeaponDetailsState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Inventory_InitializeDetailsLayout(Menu_Inventory_weaponDetailsItemRow, 7);
            Menu_Framework_SetWeaponStats(Audio_Seq_unusedInventoryIndices.weapons[Menu_Inventory_weaponDetailsItemIndex]);
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Inventory_weaponDetailsState = Menu_Inventory_WeaponDetailsState_InitWeapon;
        }
        break;

    case Menu_Inventory_WeaponDetailsState_InitWeapon:
        if (Menu_Inventory_weaponDetailsInitRow < 10) {
            if (++Menu_Inventory_weaponDetailsInitRow < 6) {
                Menu_Framework_CreateWeaponComponentRow(
                    Menu_Inventory_weaponDetailsInitRow, Audio_Seq_unusedInventoryIndices.weapons[Menu_Inventory_weaponDetailsItemIndex], 1);
                return 0;
            }
            break;
        }

        Menu_Inventory_weaponDetailsState = Menu_Inventory_WeaponDetailsState_ManageInput;
        break;

    case Menu_Inventory_WeaponDetailsState_ManageInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Inventory_ExitDetailsView(1);
            Menu_Inventory_weaponDetailsState = 3;
            break;
        }

        newIndex = Menu_Inventory_PollDetailsPaging(0, Menu_Inventory_weaponDetailsItemIndex);

        if (newIndex != Menu_Inventory_weaponDetailsItemIndex) {
            int weaponToDisplay;

            Menu_Inventory_weaponDetailsItemIndex = newIndex;
            weaponToDisplay = Menu_Inventory_SelectDetailsItem(0, newIndex);

            Menu_Framework_FormatInventoryWeaponRow(
                &Audio_Seq_unusedInventory.weapons[weaponToDisplay - 1], menuText, &rowType,
                Menu_Framework_stringBuf);

            do {
                Menu_Framework_SetWeaponStats(weaponToDisplay);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Inventory_weaponDetailsItemRow, menuText, rowType, newIndex);

                for (i = 1; i < 6; ++i) {
                    Menu_Framework_CreateWeaponComponentRow(
                        i, Audio_Seq_unusedInventoryIndices.weapons[newIndex], 0);
                }
            } while (0);
        }
        break;

    case Menu_Inventory_WeaponDetailsState_ReturnMenuReady:
        return Menu_Framework_AreMenuRowsIdle();
    }

    return 0;
}

// decomp: MENU _bladeStatusView (src/MENU/items1.h:195)
int Menu_Inventory_UpdateBladeDetails(int itemInfo)
{
    enum Menu_Inventory_EquipmentDetailsState { Menu_Inventory_EquipmentDetailsState_Init, Menu_Inventory_EquipmentDetailsState_Wait, Menu_Inventory_EquipmentDetailsState_ManageInput, Menu_Inventory_EquipmentDetailsState_ReturnMenuReady };

    static char Menu_Inventory_bladeDetailsState;
    static char Menu_Inventory_bladeDetailsAnimationTimer;
    static char Menu_Inventory_bladeDetailsItemRow;
    static u_char Menu_Inventory_bladeDetailsItemIndex;

    char* meunText[2];
    int rowType;
    int newIndex;

    if (itemInfo != 0) {
        Menu_Inventory_bladeDetailsItemRow = itemInfo >> 8;
        Menu_Inventory_bladeDetailsItemIndex = (itemInfo - 1) & 0xFF;

        Menu_Inventory_BeginDetailsTransition(Menu_Inventory_bladeDetailsItemRow);

        Menu_Inventory_bladeDetailsAnimationTimer = 10;
        Menu_Inventory_bladeDetailsState = Menu_Inventory_EquipmentDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_bladeDetailsState) {
    case Menu_Inventory_EquipmentDetailsState_Init:
        if ((Menu_Inventory_bladeDetailsState == 0) && (Menu_Framework_AreMenuRowsIdle() != 0)) {
            Menu_Inventory_InitializeDetailsLayout(Menu_Inventory_bladeDetailsItemRow, 3);
            Menu_Framework_SetBladeStats(Audio_Seq_unusedInventoryIndices.blades[Menu_Inventory_bladeDetailsItemIndex]);
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Inventory_bladeDetailsState = Menu_Inventory_EquipmentDetailsState_Wait;
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_Wait:
        if (Menu_Inventory_bladeDetailsAnimationTimer != 0) {
            --Menu_Inventory_bladeDetailsAnimationTimer;
        } else {
            Menu_Inventory_bladeDetailsState = Menu_Inventory_EquipmentDetailsState_ManageInput;
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_ManageInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Inventory_ExitDetailsView(1);
            Menu_Inventory_bladeDetailsState = Menu_Inventory_EquipmentDetailsState_ReturnMenuReady;
            break;
        }

        newIndex = Menu_Inventory_PollDetailsPaging(1, (int)Menu_Inventory_bladeDetailsItemIndex);

        if (newIndex != Menu_Inventory_bladeDetailsItemIndex) {
            int bladeToDisplay;

            Menu_Inventory_bladeDetailsItemIndex = newIndex;
            bladeToDisplay = Menu_Inventory_SelectDetailsItem(1, newIndex);

            Menu_Framework_FormatBladeRow(&Audio_Seq_unusedInventory.blades[bladeToDisplay - 1],
                meunText, &rowType, Menu_Framework_stringBuf);
            Menu_Framework_SetBladeStats(bladeToDisplay);
            Menu_Framework_UpdateItemDetailsSelection(Menu_Inventory_bladeDetailsItemRow, meunText, rowType, newIndex);
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_ReturnMenuReady:
        return Menu_Framework_AreMenuRowsIdle();
    }

    return 0;
}

// decomp: MENU _gripStatusView (src/MENU/items1.h:275)
int Menu_Inventory_UpdateGripDetails(int itemInfo)
{
    enum Menu_Inventory_EquipmentDetailsState { Menu_Inventory_EquipmentDetailsState_Init, Menu_Inventory_EquipmentDetailsState_Wait, Menu_Inventory_EquipmentDetailsState_ManageInput, Menu_Inventory_EquipmentDetailsState_ReturnMenuReady };


    static char Menu_Inventory_gripDetailsState;
    static char Menu_Inventory_gripDetailsAnimationTimer;
    static char Menu_Inventory_gripDetailsItemRow;
    static u_char Menu_Inventory_gripDetailsItemIndex;
    static char Menu_Inventory_gripDetailsLayoutPadding;

    char* menuText[2];
    int rowType;

    if (itemInfo != 0) {
        Menu_Inventory_gripDetailsItemRow = itemInfo >> 8;
        Menu_Inventory_gripDetailsItemIndex = (itemInfo - 1) & 0xFF;

        Menu_Inventory_BeginDetailsTransition(Menu_Inventory_gripDetailsItemRow);

        Menu_Inventory_gripDetailsLayoutPadding = 0;
        Menu_Inventory_gripDetailsAnimationTimer = 0;
        Menu_Inventory_gripDetailsState = Menu_Inventory_EquipmentDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_gripDetailsState) {
    case Menu_Inventory_EquipmentDetailsState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Inventory_InitializeDetailsLayout(Menu_Inventory_gripDetailsItemRow, 4);
            Menu_Framework_SetGripStats(Audio_Seq_unusedInventoryIndices.grips[Menu_Inventory_gripDetailsItemIndex]);
            Menu_Inventory_gripDetailsState = Menu_Inventory_EquipmentDetailsState_Wait;
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_Wait:
        if (Menu_Inventory_gripDetailsAnimationTimer < 10) {
            ++Menu_Inventory_gripDetailsAnimationTimer;
        } else {
            Menu_Inventory_gripDetailsState = Menu_Inventory_EquipmentDetailsState_ManageInput;
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_ManageInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Inventory_ExitDetailsView(0);
            Menu_Inventory_gripDetailsState = Menu_Inventory_EquipmentDetailsState_ReturnMenuReady;
        } else {
            int newIndex = Menu_Inventory_PollDetailsPaging(2, Menu_Inventory_gripDetailsItemIndex);

            if (newIndex != Menu_Inventory_gripDetailsItemIndex) {
                int gripToDisplay;
                Menu_Inventory_gripDetailsItemIndex = newIndex;
                gripToDisplay = Menu_Inventory_SelectDetailsItem(2, newIndex);

                Menu_Framework_FormatGripRow(&Audio_Seq_unusedInventory.grips[gripToDisplay - 1],
                    menuText, &rowType, Menu_Framework_stringBuf);

                Menu_Framework_SetGripStats(gripToDisplay);
                Menu_Framework_UpdateItemDetailsSelection(Menu_Inventory_gripDetailsItemRow, menuText, rowType, newIndex);
            }
        }

        // BUG: rodata .write
        Menu_Inventory_gripDetailsGemSlotCountBuf[6] =
            Audio_Seq_unusedInventory.grips[Audio_Seq_unusedInventoryIndices.grips[Menu_Inventory_gripDetailsItemIndex] - 1]
                .gemSlots
            + '0';

        Menu_Framework_DrawRowIcon(Menu_Framework_MainMenuIcon_Gem | (1 << 8), 256, 32);
        Render_DrawCompactText(Menu_Inventory_gripDetailsGemSlotCountBuf, vs_getXY(280, 36), NULL);

        break;

    case 3:
        return Menu_Framework_AreMenuRowsIdle();
    }
    return 0;
}

// decomp: MENU _shieldStatusView (src/MENU/items1.h:365)
int Menu_Inventory_UpdateShieldDetails(int itemInfo)
{
    enum Menu_Inventory_ShieldDetailsState { Menu_Inventory_ShieldDetailsState_Init, Menu_Inventory_ShieldDetailsState_InitShield, Menu_Inventory_ShieldDetailsState_ManageInput, Menu_Inventory_ShieldDetailsState_ReturnMenuReady };

    static char Menu_Inventory_shieldDetailsState;
    static char Menu_Inventory_shieldDetailsInitRow;
    static char Menu_Inventory_shieldDetailsItemRow;
    static u_char Menu_Inventory_shieldDetailsItemIndex;
    static char Menu_Inventory_shieldDetailsLayoutPadding;

    char* menuText[2];
    int rowType;
    int newIndex;
    int i;

    if (itemInfo != 0) {
        Menu_Inventory_shieldDetailsItemRow = itemInfo >> 8;
        Menu_Inventory_shieldDetailsItemIndex = (itemInfo - 1) & 0xFF;

        Menu_Inventory_BeginDetailsTransition(Menu_Inventory_shieldDetailsItemRow);

        Menu_Inventory_shieldDetailsLayoutPadding = 0;
        Menu_Inventory_shieldDetailsInitRow = 0;
        Menu_Inventory_shieldDetailsState = Menu_Inventory_ShieldDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_shieldDetailsState) {
    case Menu_Inventory_ShieldDetailsState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Inventory_InitializeDetailsLayout(Menu_Inventory_shieldDetailsItemRow, 7);
            Menu_Framework_SetShieldStats(Audio_Seq_unusedInventoryIndices.shields[Menu_Inventory_shieldDetailsItemIndex]);
            Menu_Framework_UpdateDpPpBars(3);

            Menu_Inventory_shieldDetailsState = Menu_Inventory_ShieldDetailsState_InitShield;
        }
        break;

    case Menu_Inventory_ShieldDetailsState_InitShield:
        if (Menu_Inventory_shieldDetailsInitRow < 10) {

            ++Menu_Inventory_shieldDetailsInitRow;

            if (Menu_Inventory_shieldDetailsInitRow < 4) {
                Menu_Framework_CreateShieldGemRow(
                    Menu_Inventory_shieldDetailsInitRow, Audio_Seq_unusedInventoryIndices.shields[Menu_Inventory_shieldDetailsItemIndex], 1);
            }
            break;
        }

        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Inventory_shieldDetailsState = Menu_Inventory_ShieldDetailsState_ManageInput;
        }

        break;

    case Menu_Inventory_ShieldDetailsState_ManageInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Inventory_ExitDetailsView(1);
            Menu_Inventory_shieldDetailsState = Menu_Inventory_ShieldDetailsState_ReturnMenuReady;
            break;
        }

        newIndex = Menu_Inventory_PollDetailsPaging(3, Menu_Inventory_shieldDetailsItemIndex);

        if (newIndex != Menu_Inventory_shieldDetailsItemIndex) {
            Menu_Inventory_shieldDetailsItemIndex = newIndex;
            i = Menu_Inventory_SelectDetailsItem(3, newIndex);

            Menu_Framework_FormatInventoryShieldRow(&Audio_Seq_unusedInventory.shields[i - 1],
                menuText, &rowType, Menu_Framework_stringBuf);

            Menu_Framework_SetShieldStats(i);
            Menu_Framework_UpdateItemDetailsSelection(Menu_Inventory_shieldDetailsItemRow, menuText, rowType, newIndex);

            for (i = 1; i < 4; ++i) {
                Menu_Framework_CreateShieldGemRow(
                    i, Audio_Seq_unusedInventoryIndices.shields[newIndex], 0);
            }
        }

        break;

    case Menu_Inventory_ShieldDetailsState_ReturnMenuReady:
        return Menu_Framework_AreMenuRowsIdle();
    }

    return 0;
}

// decomp: MENU _armorStatusView (src/MENU/items1.h:464)
int Menu_Inventory_UpdateArmorDetails(int itemInfo)
{
    enum Menu_Inventory_EquipmentDetailsState { Menu_Inventory_EquipmentDetailsState_Init, Menu_Inventory_EquipmentDetailsState_Wait, Menu_Inventory_EquipmentDetailsState_ManageInput, Menu_Inventory_EquipmentDetailsState_ReturnMenuReady };

    static char Menu_Inventory_armorDetailsStatus;
    static char Menu_Inventory_armorDetailsAnimationTimer;
    static char Menu_Inventory_armorDetailsItemRow;
    static u_char Menu_Inventory_armorDetailsItemIndex;

    char* menuText[2];
    int rowType;
    int newIndex;

    if (itemInfo != 0) {
        Menu_Inventory_armorDetailsItemRow = itemInfo >> 8;
        Menu_Inventory_armorDetailsItemIndex = (itemInfo - 1) & 0xFF;

        Menu_Inventory_BeginDetailsTransition(Menu_Inventory_armorDetailsItemRow);

        Menu_Inventory_armorDetailsAnimationTimer = 10;
        Menu_Inventory_armorDetailsStatus = 0;

        return 0;
    }

    switch (Menu_Inventory_armorDetailsStatus) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Inventory_InitializeDetailsLayout(Menu_Inventory_armorDetailsItemRow, 7);
            Menu_Framework_SetArmorStats(Audio_Seq_unusedInventoryIndices.armor[Menu_Inventory_armorDetailsItemIndex]);
            if (Audio_Seq_unusedInventory.armor[Audio_Seq_unusedInventoryIndices.armor[Menu_Inventory_armorDetailsItemIndex] - 1]
                    .category
                != 7) {
                Menu_Framework_UpdateDpPpBars(1);
            }

            Menu_Inventory_armorDetailsStatus = Menu_Inventory_EquipmentDetailsState_Wait;
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_Wait:
        if (Menu_Inventory_armorDetailsAnimationTimer != 0) {
            --Menu_Inventory_armorDetailsAnimationTimer;
        } else {
            Menu_Inventory_armorDetailsStatus = Menu_Inventory_EquipmentDetailsState_ManageInput;
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_ManageInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Inventory_ExitDetailsView(1);
            Menu_Inventory_armorDetailsStatus = Menu_Inventory_EquipmentDetailsState_ReturnMenuReady;

            break;
        }

        newIndex = Menu_Inventory_PollDetailsPaging(4, Menu_Inventory_armorDetailsItemIndex);

        if (newIndex != Menu_Inventory_armorDetailsItemIndex) {
            int armorToDisplay;

            Menu_Inventory_armorDetailsItemIndex = newIndex;
            armorToDisplay = Menu_Inventory_SelectDetailsItem(4, newIndex);

            Menu_Framework_FormatInventoryArmorRow(
                &Audio_Seq_unusedInventory.armor[armorToDisplay - 1], menuText, &rowType,
                Menu_Framework_stringBuf);
            Menu_Framework_SetArmorStats(armorToDisplay);
            Menu_Framework_UpdateItemDetailsSelection(Menu_Inventory_armorDetailsItemRow, menuText, rowType, newIndex);

            return 0;
        }

        break;

    case Menu_Inventory_EquipmentDetailsState_ReturnMenuReady:
        return Menu_Framework_AreMenuRowsIdle();
    }

    return 0;
}

// decomp: MENU _gemStatusView (src/MENU/items1.h:554)
int Menu_Inventory_UpdateGemDetails(int itemInfo)
{
    enum Menu_Inventory_EquipmentDetailsState { Menu_Inventory_EquipmentDetailsState_Init, Menu_Inventory_EquipmentDetailsState_Wait, Menu_Inventory_EquipmentDetailsState_ManageInput, Menu_Inventory_EquipmentDetailsState_ReturnMenuReady };

    static char Menu_Inventory_gemDetailsState;
    static char Menu_Inventory_gemDetailsAnimationTimer;
    static char Menu_Inventory_gemDetailsItemRow;
    static u_char Menu_Inventory_gemDetailsItemIndex;

    char* menuText[2];
    int rowType;
    int newIndex;

    if (itemInfo != 0) {
        Menu_Inventory_gemDetailsItemRow = itemInfo >> 8;
        Menu_Inventory_gemDetailsItemIndex = (itemInfo - 1) & 0xFF;

        Menu_Inventory_BeginDetailsTransition(Menu_Inventory_gemDetailsItemRow);

        Menu_Inventory_gemDetailsAnimationTimer = 10;
        Menu_Inventory_gemDetailsState = Menu_Inventory_EquipmentDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_gemDetailsState) {
    case Menu_Inventory_EquipmentDetailsState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Inventory_InitializeDetailsLayout(Menu_Inventory_gemDetailsItemRow, 3);
            Menu_Framework_SetGemStats(Audio_Seq_unusedInventoryIndices.gems[Menu_Inventory_gemDetailsItemIndex]);

            Menu_Inventory_gemDetailsState = Menu_Inventory_EquipmentDetailsState_Wait;
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_Wait:
        if (Menu_Inventory_gemDetailsAnimationTimer != 0) {
            --Menu_Inventory_gemDetailsAnimationTimer;
        } else {
            Menu_Inventory_gemDetailsState = Menu_Inventory_EquipmentDetailsState_ManageInput;
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_ManageInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            Menu_Inventory_ExitDetailsView(0);

            Menu_Inventory_gemDetailsState = Menu_Inventory_EquipmentDetailsState_ReturnMenuReady;
            Menu_Inventory_exitToBattle = Input_buttonsPressed.allButtons & PADRup;

            break;
        }

        newIndex = Menu_Inventory_PollDetailsPaging(5, Menu_Inventory_gemDetailsItemIndex);

        if (newIndex != Menu_Inventory_gemDetailsItemIndex) {
            int gemToDisplay;

            Menu_Inventory_gemDetailsItemIndex = newIndex;
            gemToDisplay = Menu_Inventory_SelectDetailsItem(5, newIndex);

            Menu_Framework_FormatGemRow(&Audio_Seq_unusedInventory.gems[gemToDisplay - 1], menuText,
                &rowType, Menu_Framework_stringBuf);
            Menu_Framework_SetGemStats(gemToDisplay);
            Menu_Framework_UpdateItemDetailsSelection(Menu_Inventory_gemDetailsItemRow, menuText, rowType, newIndex);

            return 0;
        }

        break;

    case Menu_Inventory_EquipmentDetailsState_ReturnMenuReady:
        return Menu_Framework_AreMenuRowsIdle();
    }

    return 0;
}

// decomp: MENU _itemStatusDispatcher (src/MENU/items1.h:640)
int Menu_Inventory_UpdateItemDetails(int itemInfo)
{

    static u_char Menu_Inventory_itemDetailsItemCategory;

    int page = 0;
    int new_var;

    if (itemInfo != 0) {
        page = itemInfo >> 4;
        Menu_Inventory_itemDetailsItemCategory = itemInfo & 0xF;
        Menu_Inventory_selectedItemCategoryIconOnTop = 1;
        Menu_Inventory_exitToBattle = 0;

        Menu_Framework_ResetItemDetailCursor();
        Menu_Framework_MenuItem(31)->itemPage = page & 0xFF;
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
    }

    page = Menu_Inventory_itemDetailsStatusViewDispatch[Menu_Inventory_itemDetailsItemCategory](page);
    new_var = page != 0;

    if (new_var) {
        if (Menu_Inventory_exitToBattle == 0) {
            Menu_Inventory_itemCategoryIconsEnabled = 1;
            Menu_Inventory_selectedItemCategoryIconOnTop = 0;
            Menu_Inventory_statusCommandState = 1;
            Menu_Inventory_ashleyRenderState = 0;

            Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);

            return page;
        }

        return -2;
    }

    if (Menu_Framework_AreMenuRowsIdle() != 0) {

        Menu_Framework_cursorColor = Menu_Framework_selectedStatusViewElement != 9;

        Menu_Framework_DrawTabNavigation(1);
        Menu_Framework_UpdateItemDetails();
    }

    return page;
}

// MENU3.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ItemDetails_ResetMenu3Statics(void)
{
    {
        static const char* initial = "X     0";
        memcpy((void*)&Menu_Inventory_gripDetailsGemSlotCountBuf, &initial, sizeof Menu_Inventory_gripDetailsGemSlotCountBuf);
    }
    {
        static const int (*initial[])(int) = { Menu_Inventory_UpdateWeaponDetails, Menu_Inventory_UpdateBladeDetails,
        Menu_Inventory_UpdateGripDetails, Menu_Inventory_UpdateShieldDetails, Menu_Inventory_UpdateArmorDetails, Menu_Inventory_UpdateGemDetails };
        memcpy((void*)&Menu_Inventory_itemDetailsStatusViewDispatch, &initial, sizeof Menu_Inventory_itemDetailsStatusViewDispatch);
    }
}

