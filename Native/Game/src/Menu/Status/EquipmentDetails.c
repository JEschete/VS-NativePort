// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Battle/Targeting.h"
#include "Content/StringTables.h"
#include "Core/Memory/WordCopy.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Sounds.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/EquipmentDetails.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "State/Settings.h"

// decomp: local static of _equipmentDetailScreen, at file scope so the module reset reaches it
static char Menu_Status_equipmentDetailsCursorAnimState = 0;

// decomp: MENU4.PRG _drawWeaponInfo (src/MENU/MENU4.PRG/120.c:26)
void Menu_Status_PopulateWeaponStats(Items_WeaponLoadout* weapon)
{
    Menu_Framework_equipmentSubtype = 1;

    Menu_Framework_ResetEquipmentStats();
    Menu_Framework_SetRangeAndRisk(0, 0, 0, 1);

    if (weapon->blade.id != 0) {
        int i;

        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats, &weapon->classAffinityCurrent,
            sizeof weapon->classAffinityCurrent);
        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats + 48, &weapon->classAffinityBaseline,
            sizeof weapon->classAffinityBaseline);

        Menu_Framework_equipmentStats[63] =
            weapon == &Battle_characterState->stats->weapon;

        for (i = 0; i < 4; ++i) {
            Menu_Framework_equipmentStats[i + 32] = weapon->grip.types[i];
        }

        Menu_Framework_equipmentStats[36] = weapon->damageType;

        Menu_Framework_SetDpPp(
            weapon->currentDp, weapon->maxDp, weapon->currentPp, weapon->maxPp);
        Menu_Framework_SetPrimaryStats(
            weapon->currentStr, weapon->currentInt, weapon->currentAgility, 1);

        Menu_Framework_strIntAgi[4] = weapon->baseStr;
        Menu_Framework_strIntAgi[5] = weapon->baseInt;
        Menu_Framework_strIntAgi[6] = weapon->baseAgility;

        Menu_Framework_SetRangeAndRisk(weapon->range.range, weapon->risk, 0, 1);
    }

    Menu_Framework_SetEnabledStatPages(7);
}

// decomp: MENU4.PRG _drawShieldInfo (src/MENU/MENU4.PRG/120.c:68)
void Menu_Status_PopulateShieldStats(Items_ShieldLoadout* shield)
{
    Menu_Framework_equipmentSubtype = 8;

    Menu_Framework_ResetEquipmentStats();

    if (shield->base.id != 0) {
        int i;

        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats, &shield->classAffinityCurrent,
            sizeof shield->classAffinityCurrent);
        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats + 48, &shield->classAffinityBaseline,
            sizeof shield->classAffinityBaseline);

        Menu_Framework_equipmentStats[63] =
            shield == &Battle_characterState->stats->shield;

        for (i = 0; i < 4; ++i) {
            Menu_Framework_equipmentStats[i + 32] = shield->types[i];
        }

        Menu_Framework_SetDpPp(
            shield->currentDp, shield->maxDp, shield->currentPp, shield->maxPp);
        Menu_Framework_SetPrimaryStats(
            shield->currentStr, shield->currentInt, shield->currentAgility, 1);

        Menu_Framework_strIntAgi[4] = shield->baseStr;
        Menu_Framework_strIntAgi[5] = shield->baseInt;
        Menu_Framework_strIntAgi[6] = shield->baseAgility;
    }
    Menu_Framework_SetEnabledStatPages(7);
}

// decomp: MENU4.PRG _drawArmorInfo (src/MENU/MENU4.PRG/120.c:104)
void Menu_Status_PopulateArmorStats(Items_ArmorLoadout* armor)
{
    Menu_Framework_equipmentSubtype = 16;

    Menu_Framework_ResetEquipmentStats();

    if (armor->armor.id != 0) {
        int i;

        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats, &armor->classAffinityCurrent,
            sizeof armor->classAffinityCurrent);
        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats + 48, &armor->classAffinityBaseline,
            sizeof armor->classAffinityBaseline);

        Menu_Framework_equipmentStats[63] = 0;

        for (i = 0; i < 5; ++i) {
            Menu_Framework_equipmentStats[63] |=
                armor == &Battle_characterState->stats->limbs[i].armor;
        }

        for (i = 0; i < 4; ++i) {
            Menu_Framework_equipmentStats[i + 32] = armor->types[i];
        }

        Menu_Framework_SetDpPp(armor->currentDp, armor->maxDp, 0, 0);
        Menu_Framework_SetPrimaryStats(
            armor->currentStr, armor->currentInt, armor->currentAgility, 1);

        Menu_Framework_strIntAgi[4] = armor->baseStr;
        Menu_Framework_strIntAgi[5] = armor->baseInt;
        Menu_Framework_strIntAgi[6] = armor->baseAgility;
    }

    Menu_Framework_SetEnabledStatPages(7);
}

// decomp: MENU4.PRG _drawAccessoryInfo (src/MENU/MENU4.PRG/120.c:144)
void Menu_Status_PopulateAccessoryStats(Items_AccessorySpawnLoadout* accessory)
{
    Menu_Framework_equipmentSubtype = 32;

    Menu_Framework_ResetEquipmentStats();

    if (accessory->accessory.id != 0) {
        int i;
        for (i = 0; i < 16; ++i) {
            Menu_Framework_equipmentStats[i] = accessory->classes[i & 7];
            Menu_Framework_equipmentStats[i + 16] = accessory->affinities[i & 7];
            Menu_Framework_equipmentStats[i + 32] =
                accessory->types[i & 7]; // BUG? Copies past end
        }

        Menu_Framework_SetPrimaryStats(
            accessory->currentStr, accessory->currentInt, accessory->currentAgility, 2);
    }

    Menu_Framework_SetEnabledStatPages(7);
}

// decomp: MENU4.PRG _drawBladeInfo (src/MENU/MENU4.PRG/120.c:169)
void Menu_Status_PopulateBladeStats(Items_WeaponLoadout* weapon)
{
    int i;

    Items_EquipmentPartStats* blade = &weapon->blade;
    Menu_Framework_equipmentSubtype = 2;

    for (i = 0; i < 16; ++i) {
        Menu_Framework_equipmentStats[i] = blade->classes[i & 7];
        Menu_Framework_equipmentStats[i + 16] = blade->affinities[i & 7];
    }

    Menu_Framework_SetDpPp(blade->currentDp, blade->maxDp, weapon->currentPp, weapon->maxPp);
    Menu_Framework_SetPrimaryStats(blade->strength, blade->intelligence, blade->agility, 1);
    Menu_Framework_SetRangeAndRisk(blade->range.range, blade->cost, 0, 1);
    Menu_Framework_SetEnabledStatPages(3);
    Menu_Framework_UpdateDpPpBars(11);
}

// decomp: MENU4.PRG _drawGripInfo (src/MENU/MENU4.PRG/120.c:191)
void Menu_Status_PopulateGripStats(Items_EquipmentPartStats* grip)
{
    int i;

    Menu_Framework_equipmentSubtype = 4;

    for (i = 0; i < 4; ++i) {
        Menu_Framework_equipmentStats[i + 32] = grip->types[i];
    }

    Menu_Framework_SetPrimaryStats(grip->strength, grip->intelligence, grip->agility, 1);
    Menu_Framework_SetEnabledStatPages(4);
    Menu_Framework_UpdateDpPpBars(8);
}

// decomp: MENU4.PRG _drawGemInfo (src/MENU/MENU4.PRG/120.c:209)
void Menu_Status_PopulateGemStats(Items_EquipmentPartStats* gem)
{
    int i;

    Menu_Framework_equipmentSubtype = 64;

    for (i = 0; i < 16; ++i) {
        Menu_Framework_equipmentStats[i] = gem->classes[i & 7];
        Menu_Framework_equipmentStats[i + 16] = gem->affinities[i & 7];
    }

    Menu_Framework_SetPrimaryStats(gem->strength, gem->intelligence, gem->agility, 1);
    Menu_Framework_SetEnabledStatPages(3);
    Menu_Framework_UpdateDpPpBars(8);
}

// decomp: MENU4.PRG _initBladeDescription (src/MENU/MENU4.PRG/120.c:228)
void Menu_Status_FormatBladeDescription(Items_EquipmentPartStats* blade, char* menuText[],
    int rowTypes[] __attribute__((unused)))
{
    char* str;

    Battle_stringContext.strings[9] =
        Menu_Framework_itemNames[blade->material - 1 + Text_Menu_ITEMNAME_BIN_Index_wood];
    Battle_stringContext.strings[8] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[blade->category + Text_Menu_ITEMHELP_BIN_Index_dagger - 1]];
    Battle_stringContext.strings[7] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[blade->damageType + Text_Menu_ITEMHELP_BIN_Index_blunt - 1]];
    Battle_stringContext.strings[6] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[Menu_Framework_weaponHands[blade->category - 1]
                              + Text_Menu_ITEMHELP_BIN_Index_oneHanded]];

    str = Render_FormatEncodedText(Menu_Framework_stringBuf,
        (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp[blade->id - 1]]);
    Render_FormatEncodedText(
        str, (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_weaponDescTemplate]);

    menuText[1] = Menu_Framework_stringBuf;
}

// decomp: MENU4.PRG _initGripInfo (src/MENU/MENU4.PRG/120.c:254)
void Menu_Status_FormatGripDescription(
    Items_EquipmentPartStats* grip, char* menuText[], int rowTypes[] __attribute__((unused)))
{
    char* str;

    u_short* gripId = &grip->id;

    Battle_stringContext.strings[5] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[grip->category + Text_Menu_ITEMHELP_BIN_Index_shortGrip - 1]];

    str = Render_FormatEncodedText(Menu_Framework_stringBuf,
        (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp[*gripId - 6]]);
    Render_FormatEncodedText(
        str, (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_gripTemplate]);

    menuText[1] = Menu_Framework_stringBuf;
}

// decomp: MENU4.PRG _initGemInfo (src/MENU/MENU4.PRG/120.c:275)
void Menu_Status_CopyGemDescription(
    Items_EquipmentPartStats* gem, char* menuText[], int rowTypes[] __attribute__((unused)))
{
    Core_Memory_CopyHalfwordBlocks(Menu_Framework_stringBuf,
        &Menu_Framework_itemHelp[Menu_Framework_itemHelp[gem->id - 261
                                                   + Text_Menu_ITEMHELP_BIN_Index_gemFeldspar]],
        96);

    menuText[1] = Menu_Framework_stringBuf;
}

// decomp: MENU4.PRG _drawWeaponInfoRow (src/MENU/MENU4.PRG/120.c:293)
char* Menu_Status_SelectWeaponComponent(int row, Items_WeaponLoadout* weapon)
{
    char* menuText[2];
    int rowType;

    menuText[1] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_noGems];

    switch (row) {
    case 0:
        Menu_Framework_FormatWeaponRow(weapon, menuText, &rowType, Menu_Framework_stringBuf);
        menuText[0] = weapon->name;
        break;

    case 1:
        Menu_Status_FormatBladeDescription(&weapon->blade, menuText, &rowType);
        Menu_Status_PopulateBladeStats(weapon);
        break;

    case 2:
        Menu_Status_FormatGripDescription(&weapon->grip, menuText, &rowType);
        Menu_Status_PopulateGripStats(&weapon->grip);
        break;

    case 3:
    case 4:
    case 5:
        if (weapon->gems[row - 3].id != 0) {
            Menu_Status_CopyGemDescription(&weapon->gems[row - 3], menuText, &rowType);
            Menu_Status_PopulateGemStats(&weapon->gems[row - 3]);
        } else {
            Menu_Framework_UpdateDpPpBars(8);
            Menu_Framework_ResetEquipmentStats();
        }
        break;
    }

    Menu_Framework_MenuItem(row + 0x14)->selected = 1;

    return menuText[1];
}

// decomp: MENU4.PRG _drawShieldInfoRow (src/MENU/MENU4.PRG/120.c:339)
char* Menu_Status_SelectShieldComponent(int row, Items_ShieldLoadout* shield)
{
    char* menuText[2];
    int rowType;

    menuText[1] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_noGems];

    if (row == 0) {
        Menu_Framework_FormatShieldRow(shield, menuText, &rowType, Menu_Framework_stringBuf);
    } else if (row < 0) {

    } else if (row < 4) {
        if (shield->gems[row - 1].id != 0) {
            Menu_Status_CopyGemDescription(&shield->gems[row - 1], (char**)&menuText, &rowType);
            Menu_Status_PopulateGemStats(&shield->gems[row - 1]);
        } else {
            Menu_Framework_UpdateDpPpBars(8);
            Menu_Framework_ResetEquipmentStats();
        }
    }
    Menu_Framework_MenuItem(row + 20)->selected = 1;
    return menuText[1];
}

// decomp: MENU4.PRG _drawArmorInfoRow (src/MENU/MENU4.PRG/120.c:368)
char* Menu_Status_ArmorDescription(Items_ArmorLoadout* armor_loadout)
{
    char* menuText[2];
    int rowType;

    Menu_Framework_FormatArmorRow(armor_loadout, menuText, &rowType, Menu_Framework_stringBuf);

    return menuText[1];
}

// decomp: MENU4.PRG _drawAccessoryInfoRow (src/MENU/MENU4.PRG/120.c:383)
char* Menu_Status_AccessoryDescription(Items_AccessorySpawnLoadout* accessory_loadout)
{
    char* menuText[2];
    Items_InventoryArmor armor;
    int rowType[2];

    Items_StoreInventoryAccessoryStats(&armor, accessory_loadout);
    Menu_Framework_FormatInventoryArmorRow(&armor, menuText, rowType, Menu_Framework_stringBuf);

    return menuText[1];
}

// decomp: MENU4.PRG _equipmentDetailElementSelectable (src/MENU/MENU4.PRG/120.c:401)
int Menu_Status_IsDetailElementSelectable(int newSelection, int flags)
{
    int maximum_component_selection;
    int component_selection_index;
    int ret = 0;

    switch (newSelection) {
    case 0:
        ret = (flags >> 5) & 1;
        break;

    case 1:
        ret = (flags >> 6) & 1;
        break;

    case 5:
    case 6:
    case 7:
        ret = Menu_Framework_itemStatPage != Menu_Framework_StatPage_Type;
        break;

    case 8:
        ret = Menu_Framework_itemStatPage == Menu_Framework_StatPage_Affinity;
        break;

    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        component_selection_index = newSelection - 9;
        maximum_component_selection = flags & 0xF;
        ret = (maximum_component_selection < component_selection_index) ^ 1;
        break;

    case 16:
    case 17:
        ret = (flags >> 4) & 1;
        break;

    case 2:
    case 3:
    case 4:
    case 18:
    case 19:
    case 20:
        ret = 1;
        break;
    }

    return ret;
}

// decomp: MENU4.PRG _updateEquipmentDetailSelection (src/MENU/MENU4.PRG/120.c:462)
int Menu_Status_NavigateDetailElements(u_int newSelection, int flags)
{
    int direction = 0;
    int previousSelection = newSelection;

    if (Input_buttonRepeat & PADLup) {
        direction = 1;
    } else if (Input_buttonRepeat & PADLdown) {
        direction = 2;
    } else if (Input_buttonRepeat & PADLleft) {
        direction = 3;
    } else if (Input_buttonRepeat & PADLright) {
        direction = 4;
    }
    if (direction != 0) {
        newSelection =
            Menu_Framework_statusViewNavigationMap[previousSelection][direction - 1];
    }

    while (1) {
        if (Menu_Status_IsDetailElementSelectable(newSelection, flags)) {
            break;
        }

        if (previousSelection == newSelection) {

            if ((newSelection - 2) < 14) {
                do {
                    --newSelection;
                } while (!Menu_Status_IsDetailElementSelectable(newSelection, flags));

                return newSelection;
            }

            do {
                ++newSelection;
            } while (!Menu_Status_IsDetailElementSelectable(newSelection, flags));

            return newSelection;
        }

        previousSelection = newSelection;
        newSelection =
            Menu_Framework_statusViewNavigationMap[previousSelection][direction - 1];
    }

    return newSelection;
}

// decomp: MENU4.PRG _getEquipmentListRowCount (src/MENU/MENU4.PRG/120.c:1132)
int Menu_Status_EquipmentRowCount(void)
{
    return Menu_Status_LimbCount(Menu_Status_selectedActor - 1) + 2
         + (Battle_actors[Menu_Status_selectedActor - 1]->stats->accessory.accessory.id != 0);
}

// decomp: MENU4.PRG _setActiveRow (src/MENU/MENU4.PRG/120.c:1299)
void Menu_Status_HoistEquipmentRow(int row)
{
    Menu_Framework_MenuRow* menuItem;

    Menu_Framework_FlyOutRowRight(4);

    menuItem = Menu_Framework_MenuItem(row + 10);
    menuItem->state = Menu_Framework_MenuItemState_SlideX;
    menuItem->targetPosition0 = 155;
    menuItem->selected = 1;
    menuItem->subText = NULL;
}

// decomp: MENU4.PRG _animateEquipmentDetailTransition (src/MENU/MENU4.PRG/120.c:1318)
void Menu_Status_BeginDetailTransition(int selectedRow)
{
    char icon;
    Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(32, -164, 18, 164, 8,
        (char*)&Menu_Status_statusStrings[Menu_Status_statusStrings[selectedRow < 2
                                                  ? selectedRow + Text_Menu4_status_Index_weapon
                                                  : Text_Menu4_status_Index_armor]]);

    menuItem->state = 5;
    menuItem->targetPosition0 = 16;
    menuItem->selected = 1;

    icon = 28;

    if (selectedRow == 0) {
        icon = 24;
    } else if (selectedRow == 1) {
        icon = 27;
    }

    menuItem->rowIcon = icon;

    menuItem = Menu_Framework_MenuItem(selectedRow + 10);
    menuItem->state = 3;
    menuItem->targetPosition0 = 18;
}

// decomp: MENU4.PRG _initUi (src/MENU/MENU4.PRG/120.c:1348)
void Menu_Status_InitializeDetailPanel(int selectedRow, int classAffinity)
{
    Menu_Status_BeginDetailTransition(selectedRow);
    Menu_Framework_UpdateEquipmentStatPages(classAffinity);
    Menu_Framework_UpdatePrimaryStatPanel(1);
}

// decomp: MENU4.PRG _setWeaponRow (src/MENU/MENU4.PRG/120.c:1358)
void Menu_Status_SetWeaponComponentRow(int row, Items_WeaponLoadout* weapon, int init)
{
    char* menuText[2];
    u_int rowType;

    int xOffset = 158;
    int gemSlots = weapon->grip.gemSlots;

    if (row == 1) {
        Items_InventoryBlade blade;
        Items_WriteInventoryBladeStats(&blade, &weapon->blade);
        Menu_Framework_FormatBladeRow(&blade, menuText, &rowType, Menu_Framework_stringBuf);
    } else if (row == 2) {
        Items_InventoryGrip grip;
        Items_WriteInventoryGripStats(&grip, &weapon->grip);
        Menu_Framework_FormatGripRow(&grip, menuText, &rowType, Menu_Framework_stringBuf);
    } else {

        // xOffset temporarily reused as the gem index
        xOffset = row - 3;

        if (xOffset < gemSlots) {
            menuText[1] = 0;
            menuText[0] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_none];
            rowType = 0x58000000;

            if (weapon->gems[xOffset].id != 0) {
                Items_InventoryGem gem;
                Items_WriteInventoryGemStats(&gem, &weapon->gems[xOffset]);
                Menu_Framework_FormatGemRow(&gem, menuText, &rowType, Menu_Framework_stringBuf);
            }

            xOffset = 151;
        } else {
            xOffset = 0;
        }
    }

    Menu_Framework_DeactivateRow(row + 20);

    if (xOffset != 0) {
        Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(
            row + 20, 320 - xOffset, (row * 16) + 18, xOffset, 0, menuText[0]);

        menuItem->unselectable = menuText[1] == 0;

        if (init & 1) {
            menuItem->x = 320;
            menuItem->state = 2;
            menuItem->targetPosition0 = 320 - xOffset;
        }

        menuItem->rowIcon = rowType >> 26;
        menuItem->material = ((rowType & 0xFFFF0000) >> 16) & 7;
    }
}

// decomp: MENU4.PRG _setShieldRow (src/MENU/MENU4.PRG/120.c:1418)
void Menu_Status_SetShieldGemRow(int row, Items_ShieldLoadout* shield, int init)
{
    char* menuText[2];
    int rowType;
    int gemSlot = row - 1;

    if (gemSlot < shield->base.gemSlots) {

        menuText[1] = NULL;
        menuText[0] = (char*)(&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_none]);
        rowType = 0x58000000;

        if (shield->gems[gemSlot].id != 0) {
            Items_InventoryGem gem;
            Items_WriteInventoryGemStats(&gem, &shield->gems[gemSlot]);
            Menu_Framework_FormatGemRow(&gem, menuText, &rowType, Menu_Framework_stringBuf);
        }

        gemSlot = 151;
    } else {
        gemSlot = 0;
    }

    Menu_Framework_DeactivateRow(row + 20);

    if (gemSlot != 0) {
        Menu_Framework_MenuRow* meuItem = Menu_Framework_SetMenuItem(
            row + 20, 320 - gemSlot, (row * 16) + 18, gemSlot, 0, menuText[0]);
        meuItem->unselectable = menuText[1] == 0;

        if (init & 1) {
            meuItem->x = 320;
            meuItem->state = 2;
            meuItem->targetPosition0 = 320 - gemSlot;
        }

        meuItem->rowIcon = 22;
    }
}

// decomp: MENU4.PRG _exitEquipmentDetail (src/MENU/MENU4.PRG/120.c:1463)
void Menu_Status_ExitEquipmentDetails(int clearDpPp)
{
    Menu_PlayLeaveSound();
    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
    Menu_Framework_FlyOutRowLeft(0);
    Menu_Framework_UpdateEquipmentStatPages(-1);
    Menu_Framework_UpdatePrimaryStatPanel(2);

    if (clearDpPp != 0) {
        Menu_Framework_UpdateDpPpBars(4);
    }
}

// decomp: MENU4.PRG _equipmentDetailScreen (src/MENU/MENU4.PRG/120.c:1482)
int Menu_Status_UpdateEquipmentDetails(int row)
{
    enum Menu_Framework_InitOnlyState { Menu_Framework_InitOnlyState_Init };

    static char Menu_Status_equipmentDetailState;
    static char Menu_Status_equipmentDetailsSelectedEquipmentRow;
    static char Menu_Status_equipmentDetailRowToRender;
    static char Menu_Status_equipmentDetailSelectedElement;

    char* equipment_row_text[2];
    Items_InventoryArmor armor;
    u_int equipment_row_flags;
    int limbs;
    int equipmentCount;
    int dp_pp_panel_command;
    int equipment_row_advance;
    int i;
    int selected_equipment_row;
    int newSelection;
    char* detail_help_text;
    int previousSelection;
    Menu_Framework_MenuRow* menuItem;
    unsigned char state;

    Battle_ActorStats* actor = Battle_actors[Menu_Status_selectedActor - 1]->stats;

    if (row != 0) {
        Menu_Status_equipmentDetailsSelectedEquipmentRow = row - 1;
        Menu_Status_renderBasicStatsState = 0;

        Battle_SetHudHidden(1);
        Menu_Status_HoistEquipmentRow(Menu_Status_equipmentDetailsSelectedEquipmentRow);
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        Menu_Status_equipmentDetailSelectedElement = 9;
        Menu_Status_equipmentDetailRowToRender = 0;
        Menu_Status_equipmentDetailState = Menu_Framework_InitOnlyState_Init;

        return 0;
    }

    limbs = Menu_Status_LimbCount(Menu_Status_selectedActor - 1);

    state = Menu_Status_equipmentDetailState;

    switch (state) {
    case Menu_Framework_InitOnlyState_Init:
        if (Menu_Status_basicStatsAnimState != 0) {
            --Menu_Status_basicStatsAnimState;
        }

        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Menu_Status_InitializeDetailPanel(Menu_Status_equipmentDetailsSelectedEquipmentRow, 7);

        switch (Menu_Status_equipmentDetailsSelectedEquipmentRow) {
        case 0:
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Status_PopulateWeaponStats(&actor->weapon);
            break;

        case 1:
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Status_PopulateShieldStats(&actor->shield);
            break;

        default:
            if ((Menu_Status_equipmentDetailsSelectedEquipmentRow - 2) < limbs) {
                Menu_Framework_UpdateDpPpBars(1);
                Menu_Status_PopulateArmorStats(&actor->limbs[Menu_Status_equipmentDetailsSelectedEquipmentRow - 2].armor);
            } else {
                Menu_Framework_UpdateDpPpBars(0);
                Menu_Status_PopulateAccessoryStats(&actor->accessory);
            }
            break;
        }

        Menu_Status_equipmentDetailState = 1;
        break;

    case 1:
        if (Menu_Status_equipmentDetailRowToRender < 10) {

            ++Menu_Status_equipmentDetailRowToRender;
            Menu_Status_animationStep =
                128 - Menu_Framework_rowAnimationSteps[10 - Menu_Status_equipmentDetailRowToRender];

            if (Menu_Status_equipmentDetailRowToRender < 6) {
                if (Menu_Status_basicStatsAnimState != 0) {
                    --Menu_Status_basicStatsAnimState;
                }

                Menu_Status_renderBasicStatsState = 0;

                Battle_SetHudHidden(1);

                if (Menu_Status_equipmentDetailsSelectedEquipmentRow != 0 && Menu_Status_equipmentDetailsSelectedEquipmentRow != state) {
                    return 0;
                }

                if (Menu_Status_equipmentDetailsSelectedEquipmentRow == 0) {
                    Menu_Status_SetWeaponComponentRow(Menu_Status_equipmentDetailRowToRender, &actor->weapon, 1);
                } else if (Menu_Status_equipmentDetailsSelectedEquipmentRow == 1) {
                    Menu_Status_SetShieldGemRow(Menu_Status_equipmentDetailRowToRender, &actor->shield, 1);
                }
            }
            break;
        }

        Menu_Status_equipmentDetailState = 2;
        break;

    case 2:
        if ((Input_buttonsState & (PADL1 | PADR1)) != (PADL1 | PADR1)) {
            int previous_equipment_row;
            equipment_row_advance = 0;
            equipmentCount = Menu_Status_EquipmentRowCount();
            selected_equipment_row = Menu_Status_equipmentDetailsSelectedEquipmentRow;

            if (Input_buttonRepeat & PADL1) {
                equipment_row_advance = equipmentCount - 1;
            }

            if (Input_buttonRepeat & PADR1) {
                equipment_row_advance = 1;
            }

            previous_equipment_row = selected_equipment_row;

            while (1) {
                if ((selected_equipment_row += equipment_row_advance) >= equipmentCount) {
                    selected_equipment_row -= equipmentCount;
                }

                if (selected_equipment_row == previous_equipment_row) {
                    goto here;
                }

                switch (selected_equipment_row) {
                case 0:
                    if (actor->weapon.blade.id != 0) {
                        equipmentCount = 0;
                    }
                    break;

                case 1:
                    if (actor->shield.base.id != 0) {
                        equipmentCount = 0;
                    }
                    break;

                default:
                    if ((selected_equipment_row - 2) >= limbs
                        || (actor->limbs[selected_equipment_row - 2].armor.armor.id != 0)) {
                        equipmentCount = 0;
                    }
                }

                if (equipmentCount == 0) {
                    break;
                }
            }

            if (selected_equipment_row != Menu_Status_equipmentDetailsSelectedEquipmentRow) {

                Menu_PlayChangeSound();

                for (i = 10; i < 26; ++i) {
                    Menu_Framework_DeactivateRow(i);
                }

                Menu_Status_equipmentDetailsSelectedEquipmentRow = selected_equipment_row;

                switch (selected_equipment_row) {
                case 0:
                    Menu_Framework_UpdateDpPpBars(11);
                    Menu_Status_PopulateWeaponStats(&actor->weapon);

                    for (i = 1; i < 6; ++i) {
                        Menu_Status_SetWeaponComponentRow(i, &actor->weapon, 0);
                    }

                    Menu_Framework_FormatWeaponRow(
                        &actor->weapon, equipment_row_text, &equipment_row_flags, Menu_Framework_stringBuf);

                    equipment_row_text[0] = (char*)&actor->weapon;
                    break;

                case 1:
                    Menu_Framework_UpdateDpPpBars(11);
                    Menu_Status_PopulateShieldStats(&actor->shield);

                    for (i = 1; i < 4; ++i) {
                        Menu_Status_SetShieldGemRow(i, &actor->shield, 0);
                    }

                    Menu_Framework_FormatShieldRow(
                        &actor->shield, equipment_row_text, &equipment_row_flags, Menu_Framework_stringBuf);

                    break;

                default:
                    if ((selected_equipment_row - 2) < limbs) {
                        Menu_Framework_UpdateDpPpBars(
                            Menu_Framework_DpPpBarsTarget_Dp | Menu_Framework_DpPpBarsState_Static);
                        Menu_Status_PopulateArmorStats(&actor->limbs[selected_equipment_row - 2].armor);
                        Menu_Framework_FormatArmorRow(&actor->limbs[selected_equipment_row - 2].armor, equipment_row_text,
                            &equipment_row_flags, Menu_Framework_stringBuf);
                    } else {
                        Menu_Framework_UpdateDpPpBars(
                            Menu_Framework_DpPpBarsTarget_None | Menu_Framework_DpPpBarsState_Static);
                        Menu_Status_PopulateAccessoryStats((Items_AccessorySpawnLoadout*)&actor->accessory);
                        Items_StoreInventoryAccessoryStats(&armor, &actor->accessory);
                        Menu_Framework_FormatInventoryArmorRow(
                            &armor, equipment_row_text, &equipment_row_flags, Menu_Framework_stringBuf);
                    }

                    break;
                }

                menuItem = Menu_Framework_SetMenuItem(selected_equipment_row + 10, 155, 18, 165, 0, equipment_row_text[0]);
                menuItem->selected = 1;
                menuItem->rowIcon = (equipment_row_flags >> 0x1A);
                menuItem->material = ((equipment_row_flags & 0xFFFF0000) >> 16) & 7;

                Menu_Framework_SetInformationMessage(equipment_row_text[1]);

                menuItem = Menu_Framework_SetMenuItem(0x20, 0x10, 0x12, 0xA4, 8,
                    (char*)&Menu_Status_statusStrings
                        [Menu_Status_statusStrings[selected_equipment_row < 2 ? (selected_equipment_row + Text_Menu4_status_Index_weapon)
                                                   : Text_Menu4_status_Index_armor]]);
                menuItem->selected = 1;
                i = 0x1C;

                if (selected_equipment_row == 0) {
                    i = 0x18;
                } else if (selected_equipment_row == 1) {
                    i = 0x1B;
                }

                menuItem->rowIcon = i;
                Menu_Status_equipmentDetailSelectedElement = 9;
                Menu_Framework_cursorColor = 0;
                Menu_Status_equipmentDetailsCursorAnimState = Menu_Framework_DrawCursor(
                    Menu_Status_equipmentDetailsCursorAnimState, Menu_Framework_statusViewCursorPositions[9]);
                Menu_Inventory_freezeTabArrows = 1;

                Menu_Framework_DrawTabNavigation(1);

                return 0;
            }
        }

    here:
        for (i = 21; i < 26; ++i) {
            Menu_Framework_MenuItem(i)->selected = 0;
        }

        previousSelection = Menu_Status_equipmentDetailSelectedElement;

        switch (Menu_Status_equipmentDetailsSelectedEquipmentRow) {
        case 0:
            Menu_Framework_UpdateDpPpBars(11);
            Menu_Status_PopulateWeaponStats(&actor->weapon);

            newSelection = Menu_Status_NavigateDetailElements(
                previousSelection, actor->weapon.grip.gemSlots + 114);

            break;

        case 1:
            Menu_Framework_UpdateDpPpBars(11);
            Menu_Status_PopulateShieldStats(&actor->shield);

            newSelection = Menu_Status_NavigateDetailElements(
                previousSelection, actor->shield.base.gemSlots + 96);

            break;

        default:
            dp_pp_panel_command = 8;
            if ((Menu_Status_equipmentDetailsSelectedEquipmentRow - 2) < limbs) {
                dp_pp_panel_command = 9;
            }

            Menu_Framework_UpdateDpPpBars(dp_pp_panel_command);
            newSelection = Menu_Status_NavigateDetailElements(
                previousSelection, ((Menu_Status_equipmentDetailsSelectedEquipmentRow - 2) < limbs) << 6);

            break;
        }

        if (newSelection != Menu_Status_equipmentDetailSelectedElement) {
            Menu_PlayChangeSound();
            Menu_Status_equipmentDetailSelectedElement = newSelection;
        }

        if (newSelection < 2) {
            detail_help_text = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [Text_Menu_ITEMHELP_BIN_Index_phantomPointsDesc - newSelection]];
        } else if (newSelection < 9) {
            switch (Menu_Framework_itemStatPage) {
            case Menu_Framework_StatPage_Class:
                detail_help_text = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [newSelection + Text_Menu_ITEMHELP_BIN_Index_damagePointsDesc]];
                break;

            case Menu_Framework_StatPage_Affinity:
                detail_help_text = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [newSelection + Text_Menu_ITEMHELP_BIN_Index_evilClassDesc - 1]];
                break;

            case Menu_Framework_StatPage_Type:
                detail_help_text = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [newSelection + Text_Menu_ITEMHELP_BIN_Index_darkAffinityDesc - 1]];
                break;
            }
        } else if (newSelection < 16) {
            switch (Menu_Status_equipmentDetailsSelectedEquipmentRow) {
            case 0:
                detail_help_text = Menu_Status_SelectWeaponComponent(newSelection - 9, &actor->weapon);
                break;

            case 1:
                detail_help_text = Menu_Status_SelectShieldComponent(newSelection - 9, &actor->shield);
                break;

            default:
                if ((Menu_Status_equipmentDetailsSelectedEquipmentRow - 2) < limbs) {
                    detail_help_text =
                        Menu_Status_ArmorDescription(&actor->limbs[Menu_Status_equipmentDetailsSelectedEquipmentRow - 2].armor);
                } else {
                    detail_help_text = Menu_Status_AccessoryDescription(&actor->accessory);
                }
                break;
            }
        } else {

            detail_help_text = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [newSelection + Text_Menu_ITEMHELP_BIN_Index_humanClassDesc]];

            if (newSelection >= 18) {
                if (Menu_Status_equipmentDetailsSelectedEquipmentRow == 0) {
                    detail_help_text = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                            [newSelection + Text_Menu_ITEMHELP_BIN_Index_shortGrip - 1]];
                } else if ((Menu_Status_equipmentDetailsSelectedEquipmentRow - 2) < limbs) {
                    detail_help_text = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                            [newSelection + Text_Menu_ITEMHELP_BIN_Index_polearmGrip]];
                }
            }
        }

        Menu_Framework_SetInformationMessage(detail_help_text);

        i = Menu_Framework_statusViewCursorPositions[newSelection];

        if ((Menu_Status_equipmentDetailsSelectedEquipmentRow == 1)
            && ((Menu_Status_equipmentDetailSelectedElement - 10) < 2U)) {
            i += 7;
        }

        Menu_Framework_cursorColor = Menu_Status_equipmentDetailSelectedElement != 9;
        Menu_Status_equipmentDetailsCursorAnimState = Menu_Framework_DrawCursor(Menu_Status_equipmentDetailsCursorAnimState, i);

        if (Input_buttonsPressed.allButtons & PADRup) {
            Menu_Status_ExitEquipmentDetails(1);
            return -1;
        }

        if (Input_buttonsPressed.allButtons & PADRdown) {
            Menu_Status_BeginVitalsTransition(Menu_Status_selectedActor - 1);
            Menu_Status_ExitEquipmentDetails(1);
            Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
            return Menu_Status_equipmentDetailsSelectedEquipmentRow + 1;
        }

        Menu_Inventory_freezeTabArrows = 1;

        Menu_Framework_DrawTabNavigation(1);
        break;
    }

    return 0;
}

// decomp: MENU4.PRG _equipmentScreen (src/MENU/MENU4.PRG/120.c:1875)
int Menu_Status_UpdateEquipmentList(int element)
{
    enum Menu_Status_EquipmentListState {
        Menu_Status_EquipmentListState_Init,
        Menu_Status_EquipmentListState_AnimWait,
        Menu_Status_EquipmentListState_HandleInput,
        Menu_Status_EquipmentListState_WaitChangeActor,
        Menu_Status_EquipmentListState_WaitActorInit,
        Menu_Status_EquipmentListState_EquipDetailScreen
    };

    static char Menu_Status_equipmentScreenState;
    static u_char Menu_Status_equipmentListTimer;
    static char Menu_Status_equipmentListLayoutPadding[2] __attribute__((unused));

    char* rowStrings[18];
    int rowTypes[9];
    char equipmentDescriptions[9][96];
    Items_InventoryArmor accessory_inventory_view;
    int rowCount;
    int limbCount;
    int rowType;
    int temp_s5;
    int i;
    int temp_s1_2;
    Menu_Framework_MenuRow* menuItem;
    int weapon_row_style_flags;

    Battle_ActorStats* selected_actor_stats = Battle_actors[Menu_Status_selectedActor - 1]->stats;
    Battle_LimbStats* limbs = selected_actor_stats->limbs;

    limbCount = Menu_Status_LimbCount(Menu_Status_selectedActor - 1);
    rowCount = Menu_Status_EquipmentRowCount();

    if (element != 0) {
        Menu_Framework_menuMemory.selectedEquipment = element + 1;
        Menu_Status_equipmentScreenState = Menu_Status_EquipmentListState_Init;
        return 0;
    }

    switch (Menu_Status_equipmentScreenState) {
    case Menu_Status_EquipmentListState_Init:
        for (i = 0; i < 9; ++i) {
            rowStrings[i * 2] = (char*)&Menu_Status_statusStrings[Text_Menu4_status_Offset_none];
            rowStrings[i * 2 + 1] =
                (char*)&Menu_Status_statusStrings[Text_Menu4_status_Offset_nothingEquipped];
            rowTypes[i] = 1;
        }

        if (selected_actor_stats->weapon.blade.id != 0) {
            Menu_Framework_FormatWeaponRow(
                &selected_actor_stats->weapon, rowStrings, rowTypes, equipmentDescriptions[0]);
            rowStrings[0] = selected_actor_stats->weapon.name;
        }

        if (selected_actor_stats->shield.base.id != 0) {
            Menu_Framework_FormatShieldRow(
                &selected_actor_stats->shield, &rowStrings[2], rowTypes + 1, equipmentDescriptions[1]);
        }

        temp_s5 = (selected_actor_stats->isAnalyzed == 0) & (Menu_Status_selectedActor != 1); // Disable row
        weapon_row_style_flags = 0xF000;
        temp_s1_2 = 0xF200;
        rowTypes[0] |= temp_s5 | weapon_row_style_flags;
        rowTypes[1] |= temp_s5 | temp_s1_2;

        for (i = 2; i < rowCount; ++i, ++limbs) {
            Items_InventoryArmor* p = &accessory_inventory_view;
            rowType = temp_s5 | 0xF400;

            if ((i - 2) < limbCount) {
                if (limbs->armor.armor.id != 0) {
                    Menu_Framework_FormatArmorRow(&limbs->armor, &rowStrings[i * 2],
                        &rowTypes[i], equipmentDescriptions[i]);
                }

                rowTypes[i] |= ((limbs->nameIndex + 103) << 9) + temp_s5;

            } else {
                Items_StoreInventoryAccessoryStats(p, &selected_actor_stats->accessory);
                Menu_Framework_FormatInventoryArmorRow(
                    p, &rowStrings[i * 2], &rowTypes[i], equipmentDescriptions[i]);

                rowTypes[i] |= rowType;
            }
        }

        temp_s1_2 = Input_settings.cursorMemory;
        Input_settings.cursorMemory = 1;

        Menu_Framework_CreateMenuRows(rowCount, 0x19142, (char**)rowStrings, rowTypes);

        Input_settings.cursorMemory = temp_s1_2;

        if (Menu_Status_animationStep != 0) {
            Menu_Status_equipmentListTimer = 10;
        } else {
            Menu_Status_equipmentListTimer = 0;
        }

        Menu_Status_equipmentScreenState = Menu_Status_EquipmentListState_AnimWait;
        break;

    case Menu_Status_EquipmentListState_AnimWait:
        if (Menu_Status_equipmentListTimer != 0) {
            --Menu_Status_equipmentListTimer;
            Menu_Status_animationStep = Menu_Framework_rowAnimationSteps[Menu_Status_equipmentListTimer];
        } else {
            Menu_Status_equipmentScreenState = Menu_Status_EquipmentListState_HandleInput;
        }
        break;

    case Menu_Status_EquipmentListState_HandleInput:
        i = Menu_Framework_GetSelectedRow() + 1;

        if (i != 0) {

            Menu_Framework_SetActorComparisonStats(Menu_Status_selectedActor + 15);
            Menu_Framework_ClearMenuExcept(4);

            if (i < 0) {
                temp_s5 = Menu_Framework_menuMemory.selectedEquipment - 2;
                if (temp_s5 < 0) {
                    temp_s5 = 0;
                }
                if (temp_s5 >= limbCount) {
                    temp_s5 = limbCount - 1;
                }
                Menu_Status_selectedElement = temp_s5;
                return i;
            }

            Menu_Status_UpdateEquipmentDetails(i);

            Menu_Status_equipmentScreenState = Menu_Status_EquipmentListState_EquipDetailScreen;
            break;
        }

        if ((Input_buttonsState & (PADL1 | PADR1)) != (PADL1 | PADR1)) {

            i = Menu_Status_selectedActor - 1;

            if (Input_buttonsPressed.allButtons & PADL1) {
                i = Menu_Status_FindNextInspectableActor(i, 14);
                temp_s5 = 1;
            }

            if (Input_buttonsPressed.allButtons & PADR1) {
                i = Menu_Status_FindNextInspectableActor(i, 1);
                temp_s5 = 1 | (1 << 4);
            }

            if (i != (Menu_Status_selectedActor - 1)) {

                Menu_Status_previousActor = Menu_Status_selectedActor;

                Menu_Status_BeginVitalsTransition(i);
                Menu_Status_AnimateActorSwitch(i + temp_s5);
                Menu_Framework_CloseRowsAndRememberCursor();
                Menu_Framework_ClearMenuExcept(4);

                Menu_Status_equipmentScreenState = Menu_Status_EquipmentListState_WaitChangeActor;
            }
        }
        break;

    case Menu_Status_EquipmentListState_WaitChangeActor:
        if (Menu_Status_AnimateActorSwitch(0)) {
            Menu_Status_equipmentScreenState = Menu_Status_EquipmentListState_WaitActorInit;
        }
        break;

    case Menu_Status_EquipmentListState_WaitActorInit:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {

            menuItem = Menu_Framework_MenuItem(4);

            if (menuItem->x >= 181) {
                menuItem->state = 2;
                menuItem->targetPosition0 = 180;
                menuItem->selected = 1;
            }

            Menu_Status_equipmentScreenState = Menu_Status_EquipmentListState_Init;
        }
        break;

    case Menu_Status_EquipmentListState_EquipDetailScreen:
        i = Menu_Status_UpdateEquipmentDetails(0);

        if (i != 0) {
            if (i < 0) {
                return -2;
            }
            Menu_Framework_menuMemory.selectedEquipment = i - 1;
            Menu_Status_equipmentScreenState = Menu_Status_EquipmentListState_WaitActorInit;
        }

        break;
    }
    return 0;
}

// MENU4.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Status_EquipmentDetails_ResetMenu4Statics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_equipmentDetailsCursorAnimState, &initial, sizeof Menu_Status_equipmentDetailsCursorAnimState);
    }
}

