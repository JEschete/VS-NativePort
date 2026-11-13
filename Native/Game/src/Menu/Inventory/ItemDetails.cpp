// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/EquipmentDetailControllersLive.hpp"
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
static int (*Menu_Inventory_itemDetailsStatusViewDispatch[])(int) = { VS::Menu::Inventory::UpdateLiveWeaponDetails, VS::Menu::Inventory::UpdateLiveBladeDetails,
        VS::Menu::Inventory::UpdateLiveGripDetails, VS::Menu::Inventory::UpdateLiveShieldDetails, Menu_Inventory_UpdateArmorDetails, Menu_Inventory_UpdateGemDetails };

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
        static int (*const initial[])(int) = { VS::Menu::Inventory::UpdateLiveWeaponDetails, VS::Menu::Inventory::UpdateLiveBladeDetails,
        VS::Menu::Inventory::UpdateLiveGripDetails, VS::Menu::Inventory::UpdateLiveShieldDetails, Menu_Inventory_UpdateArmorDetails, Menu_Inventory_UpdateGemDetails };
        memcpy((void*)&Menu_Inventory_itemDetailsStatusViewDispatch, &initial, sizeof Menu_Inventory_itemDetailsStatusViewDispatch);
    }
}

}  // extern "C"
