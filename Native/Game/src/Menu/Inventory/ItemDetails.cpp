// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Feedback.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemDetails.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: local static of _itemStatusDispatcher, at file scope so the module reset reaches it
static int (*Menu_Inventory_itemDetailsStatusViewDispatch[])(int) = { Menu_Inventory_UpdateWeaponDetails, Menu_Inventory_UpdateBladeDetails,
        Menu_Inventory_UpdateGripDetails, Menu_Inventory_UpdateShieldDetails, Menu_Inventory_UpdateArmorDetails, Menu_Inventory_UpdateGemDetails };

// decomp: local static of _gripStatusView, at file scope so the module reset reaches it
static char* Menu_Inventory_gripDetailsGemSlotCountBuf = "X     0";

// decomp: MENU _renderItemCategory (src/MENU/items0.h:46)
void Menu_Inventory_DrawCategoryIcons(int animationStep, int highlightSelected)
{
    int i;
    int selectedCategory = (Menu_Framework_menuMemory.opaque3A.currentCategory - 1) & 7;

    if (animationStep == 4) {
        VS::Menu::Framework::DrawLiveTabNavigation(0);
    }

    animationStep = (animationStep * 8) - 16;

    for (i = 0; i < 8; ++i) {
        u_long* texPage;
        int x = (((i + 1) & 7) * 16) + 32;

        if (i == selectedCategory && highlightSelected == 1) {

            Menu_Framework_MenuRow* menuItem = VS::Menu::Framework::LiveMenuItem(31);

            if (x >= (menuItem->x - 12)) {
                menuItem->rowIcon = i + 24;
                continue;
            }

            texPage = VS::Render::QueueLiveDefaultSprite(vs_getWH(16, 16), x | (16 << 16));

        } else {
            texPage = VS::Render::QueueLiveSpriteWithTexturePage(
                128, x | (animationStep << 0x10), vs_getWH(16, 16), (u_long *)(Render_frameScratch.orderingTable));
        }

        texPage[4] = (120 + i * 16) | (128 << 8)
                   | (i == selectedCategory ? vs_getUV0Clut(0, 0, 976, 223)
                                            : vs_getUV0Clut(0, 0, 992, 223));
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

        VS::Menu::Inventory::BeginLiveDetailsTransition(Menu_Inventory_weaponDetailsItemRow);

        Menu_Inventory_weaponDetailsLayoutPadding = 0;
        Menu_Inventory_weaponDetailsInitRow = 0;
        Menu_Inventory_weaponDetailsState = Menu_Inventory_WeaponDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_weaponDetailsState) {
    case Menu_Inventory_WeaponDetailsState_Init:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            VS::Menu::Inventory::InitializeLiveDetailsLayout(Menu_Inventory_weaponDetailsItemRow, 7);
            VS::Menu::Framework::SetLiveWeaponStats(Audio_Seq_unusedInventoryIndices.weapons[Menu_Inventory_weaponDetailsItemIndex]);
            VS::Menu::Framework::UpdateLiveDpPpBars(3);
            Menu_Inventory_weaponDetailsState = Menu_Inventory_WeaponDetailsState_InitWeapon;
        }
        break;

    case Menu_Inventory_WeaponDetailsState_InitWeapon:
        if (Menu_Inventory_weaponDetailsInitRow < 10) {
            if (++Menu_Inventory_weaponDetailsInitRow < 6) {
                VS::Menu::Framework::CreateLiveWeaponComponentRow(
                    Menu_Inventory_weaponDetailsInitRow, Audio_Seq_unusedInventoryIndices.weapons[Menu_Inventory_weaponDetailsItemIndex], 1);
                return 0;
            }
            break;
        }

        Menu_Inventory_weaponDetailsState = Menu_Inventory_WeaponDetailsState_ManageInput;
        break;

    case Menu_Inventory_WeaponDetailsState_ManageInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            VS::Menu::Inventory::ExitLiveDetailsView(1);
            Menu_Inventory_weaponDetailsState = 3;
            break;
        }

        newIndex = VS::Menu::Inventory::PollLiveDetailsPaging(0, Menu_Inventory_weaponDetailsItemIndex);

        if (newIndex != Menu_Inventory_weaponDetailsItemIndex) {
            int weaponToDisplay;

            Menu_Inventory_weaponDetailsItemIndex = newIndex;
            weaponToDisplay = VS::Menu::Inventory::SelectLiveDetailsItem(0, newIndex);

            VS::Menu::Framework::FormatLiveInventoryWeaponRow(
                &Audio_Seq_unusedInventory.weapons[weaponToDisplay - 1], menuText, &rowType,
                Menu_Framework_stringBuf);

            do {
                VS::Menu::Framework::SetLiveWeaponStats(weaponToDisplay);
                VS::Menu::Framework::UpdateLiveItemDetailsSelection(Menu_Inventory_weaponDetailsItemRow, menuText, rowType, newIndex);

                for (i = 1; i < 6; ++i) {
                    VS::Menu::Framework::CreateLiveWeaponComponentRow(
                        i, Audio_Seq_unusedInventoryIndices.weapons[newIndex], 0);
                }
            } while (0);
        }
        break;

    case Menu_Inventory_WeaponDetailsState_ReturnMenuReady:
        return VS::Menu::Framework::AreLiveMenuRowsIdle();
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

        VS::Menu::Inventory::BeginLiveDetailsTransition(Menu_Inventory_bladeDetailsItemRow);

        Menu_Inventory_bladeDetailsAnimationTimer = 10;
        Menu_Inventory_bladeDetailsState = Menu_Inventory_EquipmentDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_bladeDetailsState) {
    case Menu_Inventory_EquipmentDetailsState_Init:
        if ((Menu_Inventory_bladeDetailsState == 0) && (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0)) {
            VS::Menu::Inventory::InitializeLiveDetailsLayout(Menu_Inventory_bladeDetailsItemRow, 3);
            VS::Menu::Framework::SetLiveBladeStats(Audio_Seq_unusedInventoryIndices.blades[Menu_Inventory_bladeDetailsItemIndex]);
            VS::Menu::Framework::UpdateLiveDpPpBars(3);
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
            VS::Menu::Inventory::ExitLiveDetailsView(1);
            Menu_Inventory_bladeDetailsState = Menu_Inventory_EquipmentDetailsState_ReturnMenuReady;
            break;
        }

        newIndex = VS::Menu::Inventory::PollLiveDetailsPaging(1, (int)Menu_Inventory_bladeDetailsItemIndex);

        if (newIndex != Menu_Inventory_bladeDetailsItemIndex) {
            int bladeToDisplay;

            Menu_Inventory_bladeDetailsItemIndex = newIndex;
            bladeToDisplay = VS::Menu::Inventory::SelectLiveDetailsItem(1, newIndex);

            VS::Menu::Framework::FormatLiveBladeRow(&Audio_Seq_unusedInventory.blades[bladeToDisplay - 1],
                meunText, &rowType, Menu_Framework_stringBuf);
            VS::Menu::Framework::SetLiveBladeStats(bladeToDisplay);
            VS::Menu::Framework::UpdateLiveItemDetailsSelection(Menu_Inventory_bladeDetailsItemRow, meunText, rowType, newIndex);
        }
        break;

    case Menu_Inventory_EquipmentDetailsState_ReturnMenuReady:
        return VS::Menu::Framework::AreLiveMenuRowsIdle();
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

        VS::Menu::Inventory::BeginLiveDetailsTransition(Menu_Inventory_gripDetailsItemRow);

        Menu_Inventory_gripDetailsLayoutPadding = 0;
        Menu_Inventory_gripDetailsAnimationTimer = 0;
        Menu_Inventory_gripDetailsState = Menu_Inventory_EquipmentDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_gripDetailsState) {
    case Menu_Inventory_EquipmentDetailsState_Init:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            VS::Menu::Inventory::InitializeLiveDetailsLayout(Menu_Inventory_gripDetailsItemRow, 4);
            VS::Menu::Framework::SetLiveGripStats(Audio_Seq_unusedInventoryIndices.grips[Menu_Inventory_gripDetailsItemIndex]);
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
            VS::Menu::Inventory::ExitLiveDetailsView(0);
            Menu_Inventory_gripDetailsState = Menu_Inventory_EquipmentDetailsState_ReturnMenuReady;
        } else {
            int newIndex = VS::Menu::Inventory::PollLiveDetailsPaging(2, Menu_Inventory_gripDetailsItemIndex);

            if (newIndex != Menu_Inventory_gripDetailsItemIndex) {
                int gripToDisplay;
                Menu_Inventory_gripDetailsItemIndex = newIndex;
                gripToDisplay = VS::Menu::Inventory::SelectLiveDetailsItem(2, newIndex);

                VS::Menu::Framework::FormatLiveGripRow(&Audio_Seq_unusedInventory.grips[gripToDisplay - 1],
                    menuText, &rowType, Menu_Framework_stringBuf);

                VS::Menu::Framework::SetLiveGripStats(gripToDisplay);
                VS::Menu::Framework::UpdateLiveItemDetailsSelection(Menu_Inventory_gripDetailsItemRow, menuText, rowType, newIndex);
            }
        }

        // BUG: rodata .write
        Menu_Inventory_gripDetailsGemSlotCountBuf[6] =
            Audio_Seq_unusedInventory.grips[Audio_Seq_unusedInventoryIndices.grips[Menu_Inventory_gripDetailsItemIndex] - 1]
                .gemSlots
            + '0';

        VS::Menu::Framework::DrawLiveRowIcon(Menu_Framework_MainMenuIcon_Gem | (1 << 8), 256, 32);
        VS::Render::DrawLiveCompactText(Menu_Inventory_gripDetailsGemSlotCountBuf, vs_getXY(280, 36), NULL);

        break;

    case 3:
        return VS::Menu::Framework::AreLiveMenuRowsIdle();
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

        VS::Menu::Inventory::BeginLiveDetailsTransition(Menu_Inventory_shieldDetailsItemRow);

        Menu_Inventory_shieldDetailsLayoutPadding = 0;
        Menu_Inventory_shieldDetailsInitRow = 0;
        Menu_Inventory_shieldDetailsState = Menu_Inventory_ShieldDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_shieldDetailsState) {
    case Menu_Inventory_ShieldDetailsState_Init:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            VS::Menu::Inventory::InitializeLiveDetailsLayout(Menu_Inventory_shieldDetailsItemRow, 7);
            VS::Menu::Framework::SetLiveShieldStats(Audio_Seq_unusedInventoryIndices.shields[Menu_Inventory_shieldDetailsItemIndex]);
            VS::Menu::Framework::UpdateLiveDpPpBars(3);

            Menu_Inventory_shieldDetailsState = Menu_Inventory_ShieldDetailsState_InitShield;
        }
        break;

    case Menu_Inventory_ShieldDetailsState_InitShield:
        if (Menu_Inventory_shieldDetailsInitRow < 10) {

            ++Menu_Inventory_shieldDetailsInitRow;

            if (Menu_Inventory_shieldDetailsInitRow < 4) {
                VS::Menu::Framework::CreateLiveShieldGemRow(
                    Menu_Inventory_shieldDetailsInitRow, Audio_Seq_unusedInventoryIndices.shields[Menu_Inventory_shieldDetailsItemIndex], 1);
            }
            break;
        }

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            Menu_Inventory_shieldDetailsState = Menu_Inventory_ShieldDetailsState_ManageInput;
        }

        break;

    case Menu_Inventory_ShieldDetailsState_ManageInput:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
            VS::Menu::Inventory::ExitLiveDetailsView(1);
            Menu_Inventory_shieldDetailsState = Menu_Inventory_ShieldDetailsState_ReturnMenuReady;
            break;
        }

        newIndex = VS::Menu::Inventory::PollLiveDetailsPaging(3, Menu_Inventory_shieldDetailsItemIndex);

        if (newIndex != Menu_Inventory_shieldDetailsItemIndex) {
            Menu_Inventory_shieldDetailsItemIndex = newIndex;
            i = VS::Menu::Inventory::SelectLiveDetailsItem(3, newIndex);

            VS::Menu::Framework::FormatLiveInventoryShieldRow(&Audio_Seq_unusedInventory.shields[i - 1],
                menuText, &rowType, Menu_Framework_stringBuf);

            VS::Menu::Framework::SetLiveShieldStats(i);
            VS::Menu::Framework::UpdateLiveItemDetailsSelection(Menu_Inventory_shieldDetailsItemRow, menuText, rowType, newIndex);

            for (i = 1; i < 4; ++i) {
                VS::Menu::Framework::CreateLiveShieldGemRow(
                    i, Audio_Seq_unusedInventoryIndices.shields[newIndex], 0);
            }
        }

        break;

    case Menu_Inventory_ShieldDetailsState_ReturnMenuReady:
        return VS::Menu::Framework::AreLiveMenuRowsIdle();
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

        VS::Menu::Inventory::BeginLiveDetailsTransition(Menu_Inventory_armorDetailsItemRow);

        Menu_Inventory_armorDetailsAnimationTimer = 10;
        Menu_Inventory_armorDetailsStatus = 0;

        return 0;
    }

    switch (Menu_Inventory_armorDetailsStatus) {
    case 0:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            VS::Menu::Inventory::InitializeLiveDetailsLayout(Menu_Inventory_armorDetailsItemRow, 7);
            VS::Menu::Framework::SetLiveArmorStats(Audio_Seq_unusedInventoryIndices.armor[Menu_Inventory_armorDetailsItemIndex]);
            if (Audio_Seq_unusedInventory.armor[Audio_Seq_unusedInventoryIndices.armor[Menu_Inventory_armorDetailsItemIndex] - 1]
                    .category
                != 7) {
                VS::Menu::Framework::UpdateLiveDpPpBars(1);
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
            VS::Menu::Inventory::ExitLiveDetailsView(1);
            Menu_Inventory_armorDetailsStatus = Menu_Inventory_EquipmentDetailsState_ReturnMenuReady;

            break;
        }

        newIndex = VS::Menu::Inventory::PollLiveDetailsPaging(4, Menu_Inventory_armorDetailsItemIndex);

        if (newIndex != Menu_Inventory_armorDetailsItemIndex) {
            int armorToDisplay;

            Menu_Inventory_armorDetailsItemIndex = newIndex;
            armorToDisplay = VS::Menu::Inventory::SelectLiveDetailsItem(4, newIndex);

            VS::Menu::Framework::FormatLiveInventoryArmorRow(
                &Audio_Seq_unusedInventory.armor[armorToDisplay - 1], menuText, &rowType,
                Menu_Framework_stringBuf);
            VS::Menu::Framework::SetLiveArmorStats(armorToDisplay);
            VS::Menu::Framework::UpdateLiveItemDetailsSelection(Menu_Inventory_armorDetailsItemRow, menuText, rowType, newIndex);

            return 0;
        }

        break;

    case Menu_Inventory_EquipmentDetailsState_ReturnMenuReady:
        return VS::Menu::Framework::AreLiveMenuRowsIdle();
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

        VS::Menu::Inventory::BeginLiveDetailsTransition(Menu_Inventory_gemDetailsItemRow);

        Menu_Inventory_gemDetailsAnimationTimer = 10;
        Menu_Inventory_gemDetailsState = Menu_Inventory_EquipmentDetailsState_Init;

        return 0;
    }

    switch (Menu_Inventory_gemDetailsState) {
    case Menu_Inventory_EquipmentDetailsState_Init:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            VS::Menu::Inventory::InitializeLiveDetailsLayout(Menu_Inventory_gemDetailsItemRow, 3);
            VS::Menu::Framework::SetLiveGemStats(Audio_Seq_unusedInventoryIndices.gems[Menu_Inventory_gemDetailsItemIndex]);

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
            VS::Menu::Inventory::ExitLiveDetailsView(0);

            Menu_Inventory_gemDetailsState = Menu_Inventory_EquipmentDetailsState_ReturnMenuReady;
            Menu_Inventory_exitToBattle = Input_buttonsPressed.allButtons & PADRup;

            break;
        }

        newIndex = VS::Menu::Inventory::PollLiveDetailsPaging(5, Menu_Inventory_gemDetailsItemIndex);

        if (newIndex != Menu_Inventory_gemDetailsItemIndex) {
            int gemToDisplay;

            Menu_Inventory_gemDetailsItemIndex = newIndex;
            gemToDisplay = VS::Menu::Inventory::SelectLiveDetailsItem(5, newIndex);

            VS::Menu::Framework::FormatLiveGemRow(&Audio_Seq_unusedInventory.gems[gemToDisplay - 1], menuText,
                &rowType, Menu_Framework_stringBuf);
            VS::Menu::Framework::SetLiveGemStats(gemToDisplay);
            VS::Menu::Framework::UpdateLiveItemDetailsSelection(Menu_Inventory_gemDetailsItemRow, menuText, rowType, newIndex);

            return 0;
        }

        break;

    case Menu_Inventory_EquipmentDetailsState_ReturnMenuReady:
        return VS::Menu::Framework::AreLiveMenuRowsIdle();
    }

    return 0;
}

// decomp: MENU _itemStatusDispatcher (src/MENU/items1.h:640)
int Menu_Inventory_UpdateItemDetails(int itemInfo)
{

    static u_char Menu_Inventory_itemDetailsItemCategory;

    int page = 0;
    int has_initial_page;

    if (itemInfo != 0) {
        page = itemInfo >> 4;
        Menu_Inventory_itemDetailsItemCategory = itemInfo & 0xF;
        Menu_Inventory_selectedItemCategoryIconOnTop = 1;
        Menu_Inventory_exitToBattle = 0;

        VS::Menu::Framework::ResetLiveItemDetailCursor();
        VS::Menu::Framework::LiveMenuItem(31)->itemPage = page & 0xFF;
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
    }

    page = Menu_Inventory_itemDetailsStatusViewDispatch[Menu_Inventory_itemDetailsItemCategory](page);
    has_initial_page = page != 0;

    if (has_initial_page) {
        if (Menu_Inventory_exitToBattle == 0) {
            Menu_Inventory_itemCategoryIconsEnabled = 1;
            Menu_Inventory_selectedItemCategoryIconOnTop = 0;
            Menu_Inventory_statusCommandState = 1;
            Menu_Inventory_ashleyRenderState = 0;

            VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_Menu);

            return page;
        }

        return -2;
    }

    if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {

        Menu_Framework_cursorColor = Menu_Framework_selectedStatusViewElement != 9;

        VS::Menu::Framework::DrawLiveTabNavigation(1);
        VS::Menu::Framework::UpdateLiveItemDetails();
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
        static const int (*initial[])(int) = { (const int (*)(int))(Menu_Inventory_UpdateWeaponDetails), (const int (*)(int))(Menu_Inventory_UpdateBladeDetails),
        (const int (*)(int))(Menu_Inventory_UpdateGripDetails), (const int (*)(int))(Menu_Inventory_UpdateShieldDetails), (const int (*)(int))(Menu_Inventory_UpdateArmorDetails), (const int (*)(int))(Menu_Inventory_UpdateGemDetails) };
        memcpy((void*)&Menu_Inventory_itemDetailsStatusViewDispatch, &initial, sizeof Menu_Inventory_itemDetailsStatusViewDispatch);
    }
}

}  // extern "C"
