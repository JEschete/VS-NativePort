// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Content/StringTables.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/WordCopy.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Sounds.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"

// decomp: local static of vs_mainMenu_drawClassAffinityType, at file scope so the module reset reaches it
static int Menu_Framework_statPagePanelDirection = 0;

// decomp: local static of vs_mainMenu_drawClassAffinityType, at file scope so the module reset reaches it
static u_char Menu_Framework_statPagePanelSlideStep = 0;

// decomp: local static of vs_mainMenu_renderEquipStats, at file scope so the module reset reaches it
static u_char Menu_Framework_primaryStatPanelSlideStep = 0;

// decomp: D_80102488 (D_80102488@MENU/MAINMENU.PRG)
short Menu_Framework_comparisonStatDeltas[4];

// decomp: D_80102490 (D_80102490@MENU/MAINMENU.PRG)
short Menu_Framework_comparisonStatTotals[4];

// decomp: D_80102498 (D_80102498@MENU/MAINMENU.PRG)
short Menu_Framework_comparisonAccessoryStats[4];

// decomp: _currentDp (_currentDp@src/MENU/MAINMENU.PRG/C48.c)
u_short Menu_Framework_currentDp = 0;

// decomp: _currentPp (_currentPp@src/MENU/MAINMENU.PRG/C48.c)
u_short Menu_Framework_currentPp = 0;

// decomp: _maxDp (_maxDp@src/MENU/MAINMENU.PRG/C48.c)
u_short Menu_Framework_maxDp = 0;

// decomp: _maxPp (_maxPp@src/MENU/MAINMENU.PRG/C48.c)
u_short Menu_Framework_maxPp = 0;

// decomp: _rangeRiskData (_rangeRiskData@MENU/MAINMENU.PRG)
char Menu_Framework_rangeRiskData[8];

// decomp: _uIElement15Enabled (_uIElement15Enabled@src/MENU/MAINMENU.PRG/2D10.c)
char Menu_Framework_uiElement15Enabled = 0;

// decomp: local static of vs_mainMenu_renderStatusView, at file scope so the module reset reaches it
static char Menu_Framework_itemDetailsCursorAnimStep = 0;

// decomp: dpPpBarAnimState (dpPpBarAnimState@src/MENU/MAINMENU.PRG/C48.c)
u_short Menu_Framework_dpPpBarAnimState = 0;

// decomp: local static of _renderStatPage, at file scope so the module reset reaches it
static int Menu_Framework_equipmentStatPageNegativeStatColors[] = { vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 110, 63, 105),
        vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 220, 120, 159), vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 220, 120, 159),
        vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 110, 63, 105) };

// decomp: local static of _renderStatPage, at file scope so the module reset reaches it
static int Menu_Framework_equipmentStatPagePositiveStatColors[] = { vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 24, 115, 129),
        vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 48, 224, 208), vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 48, 224, 208),
        vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 24, 115, 129) };

// decomp: local static of vs_mainMenu_renderStatusView, at file scope so the module reset reaches it
static char Menu_Framework_itemDetailsPreviousItem = 0;

// decomp: local static of vs_mainMenu_renderDpPpBars, at file scope so the module reset reaches it
static char Menu_Framework_dpPpBarsState = 1;

// decomp: local static of vs_mainMenu_renderEquipStats, at file scope so the module reset reaches it
static char Menu_Framework_primaryStatPanelStatsAnimation = 0;

// decomp: local static of vs_mainMenu_renderStatusView, at file scope so the module reset reaches it
static char* (*Menu_Framework_itemDetailsStatusViewDispatch[])(int row) = { Menu_Framework_SelectWeaponDetailRow, Menu_Framework_BladeDetailDescription,
        Menu_Framework_GripDetailDescription, Menu_Framework_SelectShieldDetailRow, Menu_Framework_ArmorDetailDescription, Menu_Framework_ArmorDetailDescription,
        Menu_Framework_GemDetailDescription };

// decomp: local static of vs_mainMenu_renderDpPpBars, at file scope so the module reset reaches it
static char Menu_Framework_dpPpBarsTarget = 0;

// decomp: vs_mainMenu_armor (vs_mainMenu_armor@MENU/MAINMENU.PRG)
Items_InventoryArmor* Menu_Framework_armor;

// decomp: vs_mainMenu_blades (vs_mainMenu_blades@MENU/MAINMENU.PRG)
Items_InventoryBlade* Menu_Framework_blades;

// decomp: vs_mainMenu_currentStatusViewItem (vs_mainMenu_currentStatusViewItem@MENU/MAINMENU.PRG)
char Menu_Framework_currentStatusViewItem;

// decomp: vs_mainMenu_enabledStatPages (vs_mainMenu_enabledStatPages@MENU/MAINMENU.PRG)
char Menu_Framework_enabledStatPages;

// decomp: vs_mainMenu_equipmentStats (vs_mainMenu_equipmentStats@MENU/MAINMENU.PRG)
short Menu_Framework_equipmentStats[64];

// decomp: vs_mainMenu_equipmentSubtype (vs_mainMenu_equipmentSubtype@MENU/MAINMENU.PRG)
char Menu_Framework_equipmentSubtype;

// decomp: vs_mainMenu_gems (vs_mainMenu_gems@MENU/MAINMENU.PRG)
Items_InventoryGem* Menu_Framework_gems;

// decomp: vs_mainMenu_grips (vs_mainMenu_grips@MENU/MAINMENU.PRG)
Items_InventoryGrip* Menu_Framework_grips;

// decomp: vs_mainMenu_itemStatPage (vs_mainMenu_itemStatPage@MENU/MAINMENU.PRG)
char Menu_Framework_itemStatPage;

// decomp: vs_mainMenu_selectedStatusViewElement (vs_mainMenu_selectedStatusViewElement@MENU/MAINMENU.PRG)
u_char Menu_Framework_selectedStatusViewElement;

// decomp: vs_mainMenu_shields (vs_mainMenu_shields@MENU/MAINMENU.PRG)
Items_InventoryShield* Menu_Framework_shields;

// decomp: vs_mainMenu_statusViewCursorPositions (vs_mainMenu_statusViewCursorPositions@MENU/MAINMENU.PRG)
int Menu_Framework_statusViewCursorPositions[] = { vs_getXY(66, 28), vs_getXY(-4, 28),
    vs_getXY(0, 50), vs_getXY(0, 66), vs_getXY(0, 82), vs_getXY(0, 98), vs_getXY(0, 114),
    vs_getXY(0, 130), vs_getXY(0, 146), vs_getXY(141, 10), vs_getXY(148, 26),
    vs_getXY(148, 42), vs_getXY(155, 58), vs_getXY(155, 74), vs_getXY(155, 90),
    vs_getXY(180, 24), vs_getXY(192, 126), vs_getXY(192, 136), vs_getXY(152, 146),
    vs_getXY(152, 156), vs_getXY(152, 166) };

// decomp: vs_mainMenu_statusViewNavigationMap (vs_mainMenu_statusViewNavigationMap@MENU/MAINMENU.PRG)
u_char Menu_Framework_statusViewNavigationMap[][4] = { { 0, 2, 1, 10 }, { 1, 2, 1, 0 },
    { 1, 3, 2, 11 }, { 2, 4, 3, 12 }, { 3, 5, 4, 13 }, { 4, 6, 5, 14 }, { 5, 7, 6, 16 },
    { 6, 8, 7, 17 }, { 7, 8, 8, 18 }, { 9, 10, 0, 9 }, { 9, 11, 0, 10 },
    { 10, 12, 2, 11 }, { 11, 13, 3, 12 }, { 12, 14, 4, 13 }, { 13, 15, 5, 14 },
    { 14, 16, 5, 15 }, { 15, 17, 6, 16 }, { 16, 18, 7, 17 }, { 17, 19, 8, 18 },
    { 18, 20, 8, 19 }, { 19, 20, 8, 20 } };

// decomp: vs_mainMenu_strIntAgi (vs_mainMenu_strIntAgi@MENU/MAINMENU.PRG)
short Menu_Framework_strIntAgi[8];

// decomp: vs_mainMenu_weapons (vs_mainMenu_weapons@MENU/MAINMENU.PRG)
Items_InventoryWeapon* Menu_Framework_weapons;

// decomp: MAINMENU.PRG vs_mainMenu_resetStats (src/MENU/MAINMENU.PRG/2D10.c:351)
void Menu_Framework_ResetEquipmentStats(void)
{
    Core_Memory_ZeroWordsBackward(&Menu_Framework_equipmentStats, sizeof Menu_Framework_equipmentStats);
    Menu_Framework_SetDpPp(0, 0, 0, 0);
    Menu_Framework_SetPrimaryStats(0, 0, 0, 1);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromWeapon (src/MENU/MAINMENU.PRG/2D10.c:358)
void Menu_Framework_SetWeaponStats(int weaponIndex)
{
    Items_WeaponLoadout uiWeapon;
    Items_WeaponLoadout* weapon = &uiWeapon;

    Menu_Framework_ResetEquipmentStats();
    Menu_Framework_SetRangeAndRisk(0, 0, 0, 1);

    if (weaponIndex != 0) {
        int i;

        if (Menu_Framework_weapons[weaponIndex - 1].isEquipped != 0) {
            weapon = &Battle_characterState->stats->weapon;
        } else if (Menu_Framework_weapons == Audio_Seq_unusedInventory.weapons) {
            Items_LoadInventoryWeaponStats(weapon, &Menu_Framework_weapons[weaponIndex - 1]);
        } else {
            Menu_Container_BuildWeaponStats(weapon, &Menu_Framework_weapons[weaponIndex - 1],
                &Menu_Container_containerData->items);
        }

        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats, &weapon->classAffinityCurrent,
            sizeof weapon->classAffinityCurrent);

        for (i = 0; i < 4; ++i) {
            Menu_Framework_equipmentStats[32 + i] = weapon->grip.types[i];
        }

        Menu_Framework_equipmentStats[36] = weapon->damageType;

        do {
        } while (0);

        Menu_Framework_SetDpPp(
            weapon->currentDp, weapon->maxDp, weapon->currentPp, weapon->maxPp);
        Menu_Framework_SetPrimaryStats(
            weapon->currentStr, weapon->currentInt, weapon->currentAgility, 1);
        Menu_Framework_SetRangeAndRisk(weapon->range.range, weapon->risk, 0, 1);

        Menu_Framework_strIntAgi[4] = weapon->baseStr;
        Menu_Framework_strIntAgi[5] = weapon->baseInt;
        Menu_Framework_strIntAgi[6] = weapon->baseAgility;
    }

    Menu_Framework_equipmentSubtype = 1;
    Menu_Framework_currentStatusViewItem = weaponIndex;

    Menu_Framework_SetEnabledStatPages(Menu_Framework_StatPageEnabled_All);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromBlade (src/MENU/MAINMENU.PRG/2D10.c:407)
void Menu_Framework_SetBladeStats(int bladeIndex)
{
    int i;
    Items_InventoryBlade* blade = &Menu_Framework_blades[bladeIndex - 1];

    for (i = 0; i < 16; ++i) {
        Menu_Framework_equipmentStats[i] = blade->classes[i & 7];
        Menu_Framework_equipmentStats[16 + i] = blade->affinities[i & 7];
    }

    Menu_Framework_SetDpPp(blade->currentDp, blade->maxDp, blade->currentPp, blade->maxPp);
    Menu_Framework_SetPrimaryStats(blade->strength, blade->intelligence, blade->agility, 1);
    Menu_Framework_SetRangeAndRisk(blade->range.range, blade->cost, 0, 1);

    Menu_Framework_equipmentSubtype = 2;
    Menu_Framework_currentStatusViewItem = bladeIndex;

    Menu_Framework_SetEnabledStatPages(Menu_Framework_StatPageEnabled_Class | Menu_Framework_StatPageEnabled_Affinity);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromGrip (src/MENU/MAINMENU.PRG/2D10.c:427)
void Menu_Framework_SetGripStats(int gripIndex)
{
    int i;
    Items_InventoryGrip* grip = &Menu_Framework_grips[gripIndex - 1];

    for (i = 0; i < 4; ++i) {
        Menu_Framework_equipmentStats[i + 32] = grip->types[i];
    }
    Menu_Framework_SetPrimaryStats(grip->strength, grip->intelligence, grip->agility, 1);

    Menu_Framework_equipmentSubtype = 4;
    Menu_Framework_currentStatusViewItem = gripIndex;

    Menu_Framework_SetEnabledStatPages(Menu_Framework_StatPageEnabled_Type);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromShield (src/MENU/MAINMENU.PRG/2D10.c:443)
void Menu_Framework_SetShieldStats(int shieldIndex)
{
    Items_ShieldLoadout uiShield;
    int i;
    Items_ShieldLoadout* shield = &uiShield;

    Menu_Framework_ResetEquipmentStats();

    if (shieldIndex != 0) {

        if (Menu_Framework_shields[shieldIndex - 1].isEquipped != 0) {
            shield = &Battle_characterState->stats->shield;
        } else if (Menu_Framework_shields == Audio_Seq_unusedInventory.shields) {
            Items_LoadInventoryShieldStats(shield, &Menu_Framework_shields[shieldIndex - 1]);
        } else {
            Menu_Container_BuildShieldStats(shield, &Menu_Framework_shields[shieldIndex - 1],
                &Menu_Container_containerData->items);
        }

        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats, &shield->classAffinityCurrent, 0x40);

        for (i = 0; i < 4; ++i) {
            Menu_Framework_equipmentStats[32 + i] = shield->types[i];
        }

        Menu_Framework_SetDpPp(
            shield->currentDp, shield->maxDp, shield->currentPp, shield->maxPp);
        Menu_Framework_SetPrimaryStats(
            shield->currentStr, shield->currentInt, shield->currentAgility, 1);

        Menu_Framework_strIntAgi[4] = shield->baseStr;
        Menu_Framework_strIntAgi[5] = shield->baseInt;
        Menu_Framework_strIntAgi[6] = shield->baseAgility;
    }

    Menu_Framework_equipmentSubtype = 8;
    Menu_Framework_currentStatusViewItem = shieldIndex;

    Menu_Framework_SetEnabledStatPages(Menu_Framework_StatPageEnabled_All);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromArmor (src/MENU/MAINMENU.PRG/2D10.c:484)
void Menu_Framework_SetArmorStats(int armorIndex)
{
    Items_ArmorLoadout uiArmor;
    int i;
    Items_ArmorLoadout* armor = &uiArmor;
    int isAccessory = 0;

    Menu_Framework_ResetEquipmentStats();

    if (armorIndex != 0) {

        u_int limb = Menu_Framework_armor[armorIndex - 1].limb - 1;

        if (limb < 5) {
            armor = &Battle_characterState->stats->limbs[limb].armor;
        } else {
            Items_LoadInventoryArmorStats(armor, &Menu_Framework_armor[armorIndex - 1]);
        }

        isAccessory = armor->armor.category == 7;

        Core_Memory_CopyHalfwordBlocks(Menu_Framework_equipmentStats, &armor->classAffinityCurrent, 0x40);

        for (i = 0; i < 4; ++i) {
            Menu_Framework_equipmentStats[32 + i] = armor->types[i];
        }

        Menu_Framework_SetDpPp(armor->currentDp, armor->maxDp, 0, 0);
        Menu_Framework_SetPrimaryStats(
            armor->currentStr, armor->currentInt, armor->currentAgility, isAccessory + 1);

        Menu_Framework_strIntAgi[4] = armor->baseStr;
        Menu_Framework_strIntAgi[5] = armor->baseInt;
        Menu_Framework_strIntAgi[6] = armor->baseAgility;
    }

    if (isAccessory != 0) {
        Menu_Framework_equipmentSubtype = 32;
    } else {
        Menu_Framework_equipmentSubtype = 16;
    }

    Menu_Framework_currentStatusViewItem = armorIndex;

    Menu_Framework_SetEnabledStatPages(Menu_Framework_StatPageEnabled_All);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromGem (src/MENU/MAINMENU.PRG/2D10.c:531)
void Menu_Framework_SetGemStats(int gemIndex)
{
    int i;
    Items_InventoryGem* temp_a2 = &Menu_Framework_gems[gemIndex - 1];

    for (i = 0; i < 16; ++i) {
        Menu_Framework_equipmentStats[i] = temp_a2->classes[i & 7];
        Menu_Framework_equipmentStats[i + 16] = temp_a2->affinities[i & 7];
    }

    Menu_Framework_SetPrimaryStats(
        temp_a2->strength, temp_a2->intelligence, temp_a2->agility, 1);

    Menu_Framework_equipmentSubtype = 64;
    Menu_Framework_currentStatusViewItem = gemIndex;

    Menu_Framework_SetEnabledStatPages(Menu_Framework_StatPageEnabled_Class | Menu_Framework_StatPageEnabled_Affinity);
}

// decomp: MAINMENU.PRG _weaponStatusView (src/MENU/MAINMENU.PRG/2D10.c:556)
char* Menu_Framework_SelectWeaponDetailRow(int row)
{
    char* menuText[2];
    int rowType;
    int gem;
    int weaponId;
    Items_InventoryWeapon* weapon;

    menuText[1] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_noGems];
    weaponId = Menu_Framework_currentStatusViewItem;
    weapon = &Menu_Framework_weapons[weaponId - 1];

    switch (row) {
    case 0:
        Menu_Framework_FormatInventoryWeaponRow(
            weapon, menuText, &rowType, Menu_Framework_stringBuf);
        break;

    case 1:
        Menu_Framework_UpdateDpPpBars(Menu_Framework_DpPpBarsTarget_Both | Menu_Framework_DpPpBarsState_Static);
        Menu_Framework_FormatBladeRow(&Menu_Framework_blades[weapon->blade - 1], menuText,
            &rowType, Menu_Framework_stringBuf);
        Menu_Framework_SetBladeStats(weapon->blade);
        break;

    case 2:
        Menu_Framework_UpdateDpPpBars(Menu_Framework_DpPpBarsTarget_None | Menu_Framework_DpPpBarsState_Static);
        Menu_Framework_FormatGripRow(&Menu_Framework_grips[weapon->grip - 1], menuText, &rowType,
            Menu_Framework_stringBuf);
        Menu_Framework_SetGripStats(weapon->grip);
        break;

    case 3:
    case 4:
    case 5:
        Menu_Framework_UpdateDpPpBars(Menu_Framework_DpPpBarsTarget_None | Menu_Framework_DpPpBarsState_Static);
        gem = weapon->gems[row - 3];

        if (gem != 0) {
            Menu_Framework_FormatGemRow(
                &Menu_Framework_gems[gem - 1], menuText, &rowType, Menu_Framework_stringBuf);
            Menu_Framework_SetGemStats(gem);
        } else {
            Menu_Framework_ResetEquipmentStats();
        }

        break;
    }

    Menu_Framework_equipmentSubtype = 1;
    Menu_Framework_currentStatusViewItem = weaponId;

    Menu_Framework_MenuItem(row + 10)->selected = 1;

    // BUG: Returns stack temporary
    return menuText[1];
}

// decomp: MAINMENU.PRG _bladeStatusView (src/MENU/MAINMENU.PRG/2D10.c:620)
char* Menu_Framework_BladeDetailDescription(int arg0 __attribute__((unused)))
{
    char* menuText[2];
    int rowType;

    Menu_Framework_FormatBladeRow(&Menu_Framework_blades[Menu_Framework_currentStatusViewItem - 1],
        menuText, &rowType, Menu_Framework_stringBuf);

    // BUG: Returns stack temporary
    return menuText[1];
}

// decomp: MAINMENU.PRG _gripStatusView (src/MENU/MAINMENU.PRG/2D10.c:638)
char* Menu_Framework_GripDetailDescription(int arg0 __attribute__((unused)))
{
    char* menuText[2];
    int rowType;

    Menu_Framework_FormatGripRow(&Menu_Framework_grips[Menu_Framework_currentStatusViewItem - 1],
        menuText, &rowType, Menu_Framework_stringBuf);

    // BUG: Returns stack temporary
    return menuText[1];
}

// decomp: MAINMENU.PRG _shieldStatusView (src/MENU/MAINMENU.PRG/2D10.c:656)
char* Menu_Framework_SelectShieldDetailRow(int arg0)
{
    char* menuText[2];
    int rowType;
    int gem;
    int temp_s2;
    Items_InventoryShield* shield;

    menuText[1] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_noGems];
    shield = &Menu_Framework_shields[Menu_Framework_currentStatusViewItem - 1];
    temp_s2 = Menu_Framework_currentStatusViewItem;

    if (arg0 == 0) {
        Menu_Framework_FormatInventoryShieldRow(
            shield, menuText, &rowType, Menu_Framework_stringBuf);
    } else if (arg0 >= 0) {
        if (arg0 < 4) {

            Menu_Framework_UpdateDpPpBars(8);

            gem = shield->gems[arg0 - 1];

            if (gem != 0) {
                Menu_Framework_FormatGemRow(
                    &Menu_Framework_gems[gem - 1], menuText, &rowType, Menu_Framework_stringBuf);
                Menu_Framework_SetGemStats(gem);
            } else {
                Menu_Framework_ResetEquipmentStats();
            }
        }
    }

    Menu_Framework_equipmentSubtype = 8;
    Menu_Framework_currentStatusViewItem = temp_s2;

    Menu_Framework_MenuItem(arg0 + 10)->selected = 1;

    // BUG: Returns stack temporary
    return menuText[1];
}

// decomp: MAINMENU.PRG _armorStatusView (src/MENU/MAINMENU.PRG/2D10.c:703)
char* Menu_Framework_ArmorDetailDescription(int arg0 __attribute__((unused)))
{
    char* menuText[2];
    int rowType;

    Menu_Framework_FormatInventoryArmorRow(
        &Menu_Framework_armor[Menu_Framework_currentStatusViewItem - 1], menuText, &rowType,
        Menu_Framework_stringBuf);

    // BUG: Returns stack temporary
    return menuText[1];
}

// decomp: MAINMENU.PRG _gemStatusView (src/MENU/MAINMENU.PRG/2D10.c:722)
char* Menu_Framework_GemDetailDescription(int arg0 __attribute__((unused)))
{
    char* menuText[2];
    int rowType;

    Menu_Framework_FormatGemRow(&Menu_Framework_gems[Menu_Framework_currentStatusViewItem - 1],
        menuText, &rowType, Menu_Framework_stringBuf);

    // BUG: Returns stack temporary
    return menuText[1];
}

// decomp: MAINMENU.PRG vs_mainMenu_resetStatusViewCursor (src/MENU/MAINMENU.PRG/2D10.c:734)
void Menu_Framework_ResetItemDetailCursor(void)
{
    Menu_Framework_selectedStatusViewElement = 9;
}

// decomp: MAINMENU.PRG _isUiElementReachable (src/MENU/MAINMENU.PRG/2D10.c:750)
int Menu_Framework_IsItemDetailElementReachable(int selectedElement)
{
    int var_a0 = 0;
    int subType = Menu_Framework_equipmentSubtype;

    switch (selectedElement) {
    case 0:
        var_a0 = subType & 0xB;
        break;
    case 1:
        var_a0 = subType & 0x1B;
        break;
    case 2:
    case 3:
    case 4:
        var_a0 = 1;
        break;

    case 5:
    case 6:
    case 7:
        var_a0 = Menu_Framework_itemStatPage != Menu_Framework_StatPage_Type;
        break;

    case 8:
        var_a0 = Menu_Framework_itemStatPage == Menu_Framework_StatPage_Affinity;
        break;

    case 9:
        var_a0 = 1;
        // Fallthrough

    case 10:
    case 11:
    case 12:
        if ((subType & 8) && (Menu_Framework_currentStatusViewItem != 0)) {
            var_a0 = (selectedElement - 10)
                   < Menu_Framework_shields[Menu_Framework_currentStatusViewItem - 1]
                         .base.gemSlots;
            break;
        }
        // Fallthrough

    case 13:
    case 14:
        if ((subType & 1) && (Menu_Framework_currentStatusViewItem != 0)) {
            var_a0 = (selectedElement - 12)
                   < Menu_Framework_grips
                         [Menu_Framework_weapons[Menu_Framework_currentStatusViewItem - 1].grip
                             - 1]
                             .gemSlots;
        }
        break;

    case 15:
        var_a0 = Menu_Framework_uiElement15Enabled;
        break;

    case 16:
    case 17:
        var_a0 = subType & 3;
        break;

    case 18:
    case 19:
    case 20:
        var_a0 = 1;
        break;
    }

    return var_a0;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderStatusView (src/MENU/MAINMENU.PRG/2D10.c:837)
void Menu_Framework_UpdateItemDetails(void)
{


    char* infoMsg;
    u_int selectedElement = Menu_Framework_selectedStatusViewElement;
    int direction = 0;
    int i = selectedElement;

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
        Menu_Framework_selectedStatusViewElement =
            Menu_Framework_statusViewNavigationMap[selectedElement][direction - 1];
    }

    while (Menu_Framework_IsItemDetailElementReachable(Menu_Framework_selectedStatusViewElement) == 0) {

        if (selectedElement == Menu_Framework_selectedStatusViewElement) {

            if ((selectedElement - 2) < 14) {
                do {
                    --Menu_Framework_selectedStatusViewElement;
                } while (
                    Menu_Framework_IsItemDetailElementReachable(Menu_Framework_selectedStatusViewElement) == 0);
            } else {
                do {
                    ++Menu_Framework_selectedStatusViewElement;
                } while (
                    Menu_Framework_IsItemDetailElementReachable(Menu_Framework_selectedStatusViewElement) == 0);
            }

            break;

        } else {
            selectedElement = Menu_Framework_selectedStatusViewElement;
            Menu_Framework_selectedStatusViewElement =
                Menu_Framework_statusViewNavigationMap[selectedElement][direction - 1];
        }
    }

    if (Menu_Framework_itemDetailsPreviousItem == Menu_Framework_currentStatusViewItem) {
        if (i != Menu_Framework_selectedStatusViewElement) {
            Menu_PlayChangeSound();
        }
    } else {
        Menu_Framework_itemDetailsPreviousItem = Menu_Framework_currentStatusViewItem;
    }

    for (i = 11; i < 16; ++i) {
        Menu_Framework_MenuItem(i)->selected = 0;
    }

    if (Menu_Framework_equipmentSubtype & 1) {
        Menu_Framework_UpdateDpPpBars(11);
        Menu_Framework_SetWeaponStats(Menu_Framework_currentStatusViewItem);
    }

    if (Menu_Framework_equipmentSubtype & 8) {
        Menu_Framework_UpdateDpPpBars(11);
        Menu_Framework_SetShieldStats(Menu_Framework_currentStatusViewItem);
    }

    if (Menu_Framework_equipmentSubtype & 0x10) {
        Menu_Framework_UpdateDpPpBars(9);
    }

    if (Menu_Framework_equipmentSubtype & 0x20) {
        Menu_Framework_UpdateDpPpBars(8);
    }

    if (Menu_Framework_selectedStatusViewElement < (Menu_Framework_StatusUiId_UiDp + 1)) {

        infoMsg = (char*)&Menu_Framework_itemHelp
            [Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Index_phantomPointsDesc
                                  - Menu_Framework_selectedStatusViewElement]];

    } else if (Menu_Framework_selectedStatusViewElement
               < (Menu_Framework_StatusUiId_UistatPageLastElement + 1)) {

        switch (Menu_Framework_itemStatPage) {
        case Menu_Framework_StatPage_Class:
            infoMsg = (char*)&Menu_Framework_itemHelp
                [Menu_Framework_itemHelp[Menu_Framework_selectedStatusViewElement - 2
                                      + Text_Menu_ITEMHELP_BIN_Index_humanClassDesc]];
            break;

        case Menu_Framework_StatPage_Affinity:
            infoMsg = (char*)&Menu_Framework_itemHelp
                [Menu_Framework_itemHelp[Menu_Framework_selectedStatusViewElement - 2
                                      + Text_Menu_ITEMHELP_BIN_Index_physicalAffinityDesc]];
            break;

        case Menu_Framework_StatPage_Type:
            infoMsg = (char*)&Menu_Framework_itemHelp
                [Menu_Framework_itemHelp[Menu_Framework_selectedStatusViewElement - 2
                                      + Text_Menu_ITEMHELP_BIN_Index_bluntTypeDesc]];
            break;
        }

    } else {

        if (Menu_Framework_selectedStatusViewElement < 15) {

            for (i = 0; i < 7; ++i) {
                if ((Menu_Framework_equipmentSubtype >> i) & 1) {
                    // BUG: The methods in statusViewDispatch return stack temporaries.
                    infoMsg =
                        Menu_Framework_itemDetailsStatusViewDispatch[i](Menu_Framework_selectedStatusViewElement - 9);
                }
            }

        } else if (Menu_Framework_selectedStatusViewElement == 15) {
            // Unused, see `_uIElement15Enabled`
            infoMsg = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_endsSetup];
        } else {

            infoMsg = (char*)&Menu_Framework_itemHelp
                [Menu_Framework_itemHelp[Menu_Framework_selectedStatusViewElement - 16
                                      + Text_Menu_ITEMHELP_BIN_Index_rangeDesc]];

            if (Menu_Framework_selectedStatusViewElement >= 18) {

                if (Menu_Framework_equipmentSubtype & 7) {
                    infoMsg = (char*)&Menu_Framework_itemHelp
                        [Menu_Framework_itemHelp[Menu_Framework_selectedStatusViewElement - 18
                                              + Text_Menu_ITEMHELP_BIN_Index_attackStr]];
                }

                if (Menu_Framework_equipmentSubtype & 0x18) {
                    infoMsg = (char*)&Menu_Framework_itemHelp
                        [Menu_Framework_itemHelp[Menu_Framework_selectedStatusViewElement - 18
                                              + Text_Menu_ITEMHELP_BIN_Index_defStr]];
                }
            }
        }
    }

    // BUG: If `infoMsg` resulted from an invocation of one of the statusViewDispatch
    // methods, the only reason this works is due to proximity.
    Menu_Framework_SetInformationMessage(infoMsg);

    i = Menu_Framework_statusViewCursorPositions[Menu_Framework_selectedStatusViewElement];

    if (Menu_Framework_selectedStatusViewElement == 15) {
        i += Menu_Framework_uiElement15Enabled << 0x14;
    }

    if (Menu_Framework_equipmentSubtype & 8) {
        if ((Menu_Framework_selectedStatusViewElement - 10u) < 2u) {
            i += 7;
        }
    }

    Menu_Framework_itemDetailsCursorAnimStep = Menu_Framework_DrawCursor(Menu_Framework_itemDetailsCursorAnimStep, i);
    Menu_Framework_uiElement15Enabled = 0;
}

// decomp: MAINMENU.PRG _subtract (src/MENU/MAINMENU.PRG/C48.c:668)
int Menu_Framework_OffsetStatBarCoordinate(int a0, int a1, int a2) { return (u_short)(a0 - (a1 - a2)); }

// decomp: MAINMENU.PRG _renderStatPage (src/MENU/MAINMENU.PRG/C48.c:673)
void Menu_Framework_DrawEquipmentStatPage(int xOffset)
{
    static char const* Menu_Framework_kEquipmentStatPageStatHeaders[] = { "#HUMAN", "#BEAST", "#UNDEAD", "#PHANTOM",
        "#DRAGON", "#EVIL", NULL, NULL, "#PHYSICAL", "#AIR", "#FIRE", "#EARTH", "#WATER",
        "#LIGHT", "#DARK", NULL, "#BLUNT", "#EDGED", "#PIERCING" };
#pragma vsstring(start)
    static char Menu_Framework_equipmentStatPageHeader[] = "\xFB\x00\x0C\x15\x0A\x1C\x1C\xFB\x00\xA1\xFB\x00\x0A\x0F\x0F\x12\x17\x12\x1D\x22\xFB\x00\xA1\xFB\x00\x1D\x22\x19\x0E";
#pragma vsstring(end)

    int statPage;
    int sp14;
    int sp18;
    int sp1C;
    short* sp20;
    short* sp24;
    int sp28;
    short* var_s7;
    int temp_a0;
    int temp_a0_2;
    int i;
    int i_2;
    int j;
    int temp_v0_2;
    int temp_v0_3;
    int temp_v1_3;
    int var_s6;
    u_long* prim;
    int a0;

    u_long* before = Render_frameScratch.orderingTable - 0xC;
    statPage = Menu_Framework_itemStatPage;

    switch (statPage) {
    case Menu_Framework_StatPage_Class:
        var_s7 = Menu_Framework_equipmentStats;
        sp20 = &var_s7[8];
        sp24 = &var_s7[0x30];
        sp18 = 0x60;
        break;

    case Menu_Framework_StatPage_Affinity:
        var_s7 = Menu_Framework_equipmentStats + 16;
        sp20 = &var_s7[8];
        sp24 = &var_s7[0x28];
        sp18 = 0x70;
        sp1C = var_s7[0];
        for (i = 0; i < 7; ++i) {
            if (sp1C < var_s7[i]) {
                sp1C = var_s7[i];
            }
        }
        break;

    case Menu_Framework_StatPage_Type:
        var_s7 = Menu_Framework_equipmentStats + 33;
        sp20 = var_s7;
        sp18 = 48;
        break;
    }

    for (var_s6 = 0; var_s6 < sp18; var_s6 += 16) {

        sp14 = (xOffset + 116) & 0xFFFF;
        sp28 = (xOffset + 88) & 0xFFFF;

        j = var_s6 >> 4;

        if ((Menu_Framework_equipmentStats[63] != 0)
            && ((Menu_Framework_enabledStatPages == 7) && (statPage != 2))) {

            i = Menu_Framework_DrawComparedInteger(var_s7[j] - sp24[j],
                ((xOffset + 0xA4) & 0xFFFF) | (0x400000 + var_s6 * 0x10000), 0, before);

            if (var_s7[j] > sp24[j]) {
                Render_DrawCompactTextColor("#+", i + 1, 0x804020, before);
            }

            if (var_s7[j] == sp24[j]) {
                Render_DrawCompactTextColor("#*", i + 1, 0x808080, before);
            }
        }

        i = 0x808080;
        a0 = Menu_Framework_equipmentSubtype;

        if ((a0 == 1) && ((Menu_Framework_enabledStatPages == 7) && (statPage != 0))) {
            if (statPage == a0) {
                if (sp20[j] != var_s7[j]) {
                    if (sp20[j] < var_s7[j]) {
                        i = 0x804020;
                    } else {
                        i = 0x204080;
                    }
                } else {
                    i = 0x808080;
                    if (sp20[j] != sp1C) {
                        i = 0x404040;
                    }
                }
                Menu_Framework_DrawColoredInteger(
                    var_s7[j], sp28 | (0x400000 + var_s6 * 0x10000), i, before);
                i = 0x404040;
                if (var_s7[j] == sp1C) {
                    i = 0x808080;
                    sp1C = 0xFFFF;
                }
            } else {
                if (j != (Menu_Framework_equipmentStats[36] - 1)) {
                    i = 0x404040;
                }
                Menu_Framework_DrawColoredInteger(
                    var_s7[j], sp28 | (0x400000 + var_s6 * 0x10000), i, before);
            }
        } else {
            Menu_Framework_DrawComparedInteger(
                var_s7[j], sp28 | (0x400000 + var_s6 * 0x10000), sp20[j], before);
        }

        Render_DrawCompactTextColor(Menu_Framework_kEquipmentStatPageStatHeaders[j + (statPage * 8)],
            ((xOffset + 0x40) & 0xFFFF) | (0x400000 + var_s6 * 0x10000), i, before);
        Render_InsertThreeWordPrimitive(before, 0x40D0E030, sp14 | ((var_s6 + 0x42) << 0x10),
            sp14 | ((var_s6 + 0x45) << 0x10));
        Render_InsertThreeWordPrimitive(before, 0x40978918, sp14 | (0x400000 + var_s6 * 0x10000),
            sp14 | ((var_s6 + 0x47) << 0x10));

        j = (var_s7[j] * 24) / 100;

        if (j > 24) {
            j = 24;
        }

        if (j < -24) {
            j = -24;
        }

        if (j >= 0) {
            for (i = 0; i < 4; ++i) {

                temp_v0_2 = (var_s6 + i + 66) << 0x10;

                Render_InsertThreeWordPrimitive(before, Menu_Framework_equipmentStatPagePositiveStatColors[i], sp14 | temp_v0_2,
                    ((xOffset + j + 116) & 0xFFFF) | temp_v0_2);

                if (j == (24 - i)) {
                    --j;
                }
            }
        } else {
            for (i = 0; i < 4; ++i) {

                temp_v0_3 = ((var_s6 - i) + 0x45) << 0x10;

                Render_InsertThreeWordPrimitive(before, Menu_Framework_equipmentStatPageNegativeStatColors[i], sp14 | temp_v0_3,
                    ((xOffset + j + 0x74) & 0xFFFF) | temp_v0_3);

                if (j == (i - 24)) {
                    j = i - 23;
                }
            }
        }

        for (j = 2; j < 6; ++j) {
            Render_InsertThreeWordPrimitive(before, 0x40330500,
                Menu_Framework_OffsetStatBarCoordinate(xOffset, j, 97) | ((var_s6 + j + 64) << 0x10),
                Menu_Framework_OffsetStatBarCoordinate(xOffset, j, 142) | ((var_s6 + j + 64) << 0x10));
        }

        for (j = 0; j < 8; ++j) {
            Render_InsertThreeWordPrimitive(before, 0x405D3200,
                Menu_Framework_OffsetStatBarCoordinate(xOffset, j, 97) | ((var_s6 + j + 64) << 0x10),
                Menu_Framework_OffsetStatBarCoordinate(xOffset, j, 145) | ((var_s6 + j + 64) << 0x10));
        }

        Render_InsertDrawModeCommand(
            vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off), before);

        prim = Render_frameScratch.primitiveCursor;

        for (j = 0; j < 11; ++j) {
            temp_a0 = var_s6 + j;
            temp_v1_3 = j - 0x60;
            temp_a0_2 = (temp_a0 + 0x40) << 0x10;
            prim[0] = (*before & 0xFFFFFF) | 0x04000000;
            prim[1] = 0x50330500;
            prim[2] = ((xOffset & 0xFFFF) | temp_a0_2);
            prim[3] = 0x5D3200;
            prim[4] = (((xOffset - temp_v1_3) & 0xFFFF) | temp_a0_2);
            *before = LegacyGpu_Handle(prim);
            prim += 5;
        }

        Render_frameScratch.primitiveCursor = prim;

        Render_InsertDrawModeCommand(
            vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_On), before);
    }

    Menu_Framework_DrawButtonPromptBackground(xOffset + 24, 48, 135, 12);
    Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, xOffset + 16, 46, before - 1);

    xOffset = (xOffset & 0xFFFF) + 0x22;

    for (j = 0; j < 3; ++j) {
        Menu_Framework_equipmentStatPageHeader[(0x180B01 >> (j * 8)) & 0xFF] =
            ((Menu_Framework_enabledStatPages >> j) & 1) ? 0 : 3;
    }

    Menu_Framework_equipmentStatPageHeader[(0x180B01 >> (Menu_Framework_itemStatPage * 8)) & 0xFF] = 1;

    for (i_2 = 0; i_2 < 29; ++i_2) {
        i = Menu_Framework_equipmentStatPageHeader[i_2];
        if (i == 0xFB) {
            ++i_2;
            Render_SetMenuFontStyle(Menu_Framework_equipmentStatPageHeader[i_2] + 4);
        } else {
            xOffset = Render_DrawVariableWidthGlyph(i, xOffset, 0x30, before - 4);
        }
    }
}

// decomp: MAINMENU.PRG _getNextStatPage (src/MENU/MAINMENU.PRG/C48.c:902)
int Menu_Framework_NextEnabledStatPage(int currentPage)
{
    do {
        currentPage = (currentPage + 1) & 3;
    } while (!((Menu_Framework_enabledStatPages >> currentPage) & 1));

    return currentPage;
}

// decomp: MAINMENU.PRG vs_mainMenu_updateStatPage (src/MENU/MAINMENU.PRG/C48.c:911)
void Menu_Framework_SetEnabledStatPages(int enabledPage)
{
    Menu_Framework_enabledStatPages = enabledPage;

    if (!((enabledPage >> Menu_Framework_itemStatPage) & 1)) {
        Menu_Framework_itemStatPage = Menu_Framework_NextEnabledStatPage(Menu_Framework_itemStatPage);
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_drawClassAffinityType (src/MENU/MAINMENU.PRG/C48.c:920)
void Menu_Framework_UpdateEquipmentStatPages(int arg0)
{

    int var_s1;
    int statPage;

    var_s1 = 1;

    if (arg0 != 0) {

        if (arg0 > 0) {
            Menu_Framework_itemStatPage = Menu_Framework_StatPage_None;
            Menu_Framework_SetEnabledStatPages(arg0);
        }

        Menu_Framework_statPagePanelDirection = arg0;
        Menu_Framework_statPagePanelSlideStep = 10;

        return;
    }

    statPage = Menu_Framework_itemStatPage;

    if (Menu_Framework_statPagePanelDirection > 0) {

        var_s1 = -Menu_Framework_rowAnimationSteps[Menu_Framework_statPagePanelSlideStep];

        if (Menu_Framework_statPagePanelSlideStep != 0) {
            --Menu_Framework_statPagePanelSlideStep;
        } else if (Input_buttonsPressed.allButtons & PADRleft) {
            statPage = Menu_Framework_NextEnabledStatPage(statPage);
            if (statPage != Menu_Framework_itemStatPage) {
                Audio_PlaySfxCentered(0x7E, 14);
            } else {
                Menu_PlayInvalidSound();
            }
        }
    } else {
        if (Menu_Framework_statPagePanelSlideStep != 0) {
            --Menu_Framework_statPagePanelSlideStep;
            var_s1 = (Menu_Framework_statPagePanelSlideStep - 10) * 24;
        }
    }

    if (var_s1 <= 0) {
        Menu_Framework_DrawEquipmentStatPage(var_s1);
    }

    Menu_Framework_itemStatPage = statPage;
}

// decomp: MAINMENU.PRG vs_mainMenu_setRangeRisk (src/MENU/MAINMENU.PRG/C48.c:973)
void Menu_Framework_SetRangeAndRisk(int range, int risk, int arg2, int arg3)
{
    Menu_Framework_rangeRiskData[0] = range;
    Menu_Framework_rangeRiskData[1] = risk;
    Menu_Framework_rangeRiskData[2] = arg2;
    Menu_Framework_rangeRiskData[3] = arg3;
}

// decomp: MAINMENU.PRG vs_mainMenu_setStrIntAgi (src/MENU/MAINMENU.PRG/C48.c:981)
void Menu_Framework_SetPrimaryStats(
    int strength, int intelligence, int agility, int valuesKnown)
{
    Menu_Framework_strIntAgi[0] = strength;
    Menu_Framework_strIntAgi[1] = intelligence;
    Menu_Framework_strIntAgi[2] = agility;
    Menu_Framework_strIntAgi[3] = valuesKnown;
    Menu_Framework_SetRangeAndRisk(0, 0, 0, 0);
    Core_Memory_CopyHalfwordBlocks(&Menu_Framework_strIntAgi[4], &Menu_Framework_strIntAgi[0], 8);
}

// decomp: MAINMENU.PRG vs_mainMenu_populateBaseStats (src/MENU/MAINMENU.PRG/C48.c:992)
void Menu_Framework_SetActorComparisonStats(int actorId)
{
    Battle_ActorStats* temp_t0 = Battle_actors[actorId & 0xF]->stats;
    Menu_Framework_comparisonStatDeltas[3] = actorId & 0xF;
    Menu_Framework_comparisonAccessoryStats[0] = temp_t0->accessory.currentStr;
    Menu_Framework_comparisonAccessoryStats[1] = temp_t0->accessory.currentInt;
    Menu_Framework_comparisonAccessoryStats[2] = temp_t0->accessory.currentAgility;
    Menu_Framework_comparisonStatDeltas[0] = temp_t0->strength;
    Menu_Framework_comparisonStatDeltas[1] = temp_t0->intelligence;
    Menu_Framework_comparisonStatDeltas[2] = temp_t0->agility;
    Menu_Framework_comparisonStatTotals[0] = temp_t0->totalStrength;
    Menu_Framework_comparisonStatTotals[1] = temp_t0->totalIntelligence;
    Menu_Framework_equipmentSubtype = 0;
    Menu_Framework_comparisonStatTotals[2] = temp_t0->totalAgility;

    if (actorId < 16) {
        int i;

        for (i = 0; i < 3; ++i) {
            Menu_Framework_comparisonStatDeltas[i] -= Menu_Framework_comparisonStatTotals[i];
        }

        Menu_Framework_SetPrimaryStats(temp_t0->totalStrength, temp_t0->totalIntelligence,
            temp_t0->totalAgility, temp_t0->isAnalyzed | (actorId == 0));
        Core_Memory_ZeroWordsBackward(&Menu_Framework_comparisonStatTotals, 8);
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_renderEquipStats (src/MENU/MAINMENU.PRG/C48.c:1025)
void Menu_Framework_UpdatePrimaryStatPanel(int animate)
{
    static char const* Menu_Framework_kPrimaryStatPanelRangeRiskStr[] = { "#RANGE", "#RISK" };
    static char const* Menu_Framework_kPrimaryStatPanelStatStr[] = { "#STRENGTH", "#INTELLIGENCE", "#AGILITY", NULL,
        "#ATTACK/STR", "#ATTACK/INT", "#AGILITY", NULL, "#DEFENSE/STR", "#DEFENSE/INT",
        "#AGILITY" };

    int valuesKnown;
    int stat;
    int var_a3;
    int statName;
    int i;
    u_long* temp_s4;
    int position;
    Battle_ActorStats* actor;

    position = vs_getXY(248, 126);
    actor = Battle_actors[Menu_Framework_comparisonStatDeltas[3]]->stats;
    temp_s4 = Render_frameScratch.orderingTable - 0x18;

    if (animate != 0) {
        if (Menu_Framework_primaryStatPanelStatsAnimation != animate) {
            Menu_Framework_primaryStatPanelStatsAnimation = animate;
            Menu_Framework_primaryStatPanelSlideStep = 15 - (animate * 4);
        }
        return;
    }

    valuesKnown = Menu_Framework_strIntAgi[3];

    if (Menu_Framework_primaryStatPanelStatsAnimation == 1) {
        if (Menu_Framework_primaryStatPanelSlideStep != 0) {
            position += Menu_Framework_rowAnimationSteps[Menu_Framework_primaryStatPanelSlideStep];
            --Menu_Framework_primaryStatPanelSlideStep;
        }
    } else if (Menu_Framework_primaryStatPanelSlideStep == 0) {
        return;
    } else {
        position += ((8 - Menu_Framework_primaryStatPanelSlideStep) << 5);
        --Menu_Framework_primaryStatPanelSlideStep;
    }

    if (Menu_Framework_rangeRiskData[3] != 0) {
        Render_DrawCompactText("ORG/EQP", position + 4, temp_s4);

        for (i = 0; i < 2; ++i) {
            position += vs_getXY(0, 10);
            Render_DrawCompactText(Menu_Framework_kPrimaryStatPanelRangeRiskStr[i], position, temp_s4);
            if (valuesKnown != 0) {
                stat = Menu_Framework_rangeRiskData[i];
                Menu_Framework_DrawNeutralInteger(stat, position + 28, temp_s4);
                if (i == 0) {
                    stat += actor->currentRange - actor->weapon.range.range;
                }
                Menu_Framework_DrawNeutralInteger(stat, position + 60, temp_s4);
            } else {
                Render_DrawCompactText("#??", position + 28, temp_s4);
                Render_DrawCompactText("#??", position + 60, temp_s4);
            }
        }
    } else {
        position += vs_getXY(0, 20);
        Render_DrawCompactText("ORG/EQP", position + 4, temp_s4);
    }

    for (i = 0; i < 3; ++i) {
        position += vs_getXY(0, 10);
        statName = ((Menu_Framework_equipmentSubtype & 7) != 0) * 4;
        Render_DrawCompactText(
            Menu_Framework_kPrimaryStatPanelStatStr[Menu_Framework_equipmentSubtype & 0x18 ? 8 + statName + i
                                                        : statName + i],
            position, temp_s4);

        if (valuesKnown != 0) {
            int new_var2;
            stat = Menu_Framework_strIntAgi[i];
            Menu_Framework_DrawComparedInteger(
                stat, position + 28, Menu_Framework_strIntAgi[i + 4], temp_s4);
            new_var2 = stat + Menu_Framework_comparisonStatTotals[i];
            stat += Menu_Framework_comparisonStatDeltas[i];
            var_a3 = valuesKnown & 1;
            if (var_a3 != 0) {
                var_a3 = Menu_Framework_comparisonAccessoryStats[i];
            }
            Menu_Framework_DrawComparedInteger(
                stat + var_a3, position + 60, new_var2 + var_a3, temp_s4);
        } else {
            Render_DrawCompactText("#??", position + 28, temp_s4);
            Render_DrawCompactText("#??", position + 60, temp_s4);
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_setDpPp (src/MENU/MAINMENU.PRG/C48.c:1126)
void Menu_Framework_SetDpPp(int currentDp, int maxDp, int currentPp, int maxPp)
{
    Menu_Framework_currentPp = currentPp;
    Menu_Framework_maxPp = maxPp;
    Menu_Framework_currentDp = (currentDp + 99) / 100;
    Menu_Framework_maxDp = (maxDp + 99) / 100;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderDpPpBars (src/MENU/MAINMENU.PRG/C48.c:1134)
void Menu_Framework_UpdateDpPpBars(int flags)
{

    void* before = Render_frameScratch.orderingTable - 0xC;

    if (flags != 0) {

        Menu_Framework_dpPpBarsState = flags >> 2;

        if (!(Menu_Framework_dpPpBarsState & 1)) {
            Menu_Framework_dpPpBarsTarget = flags & 3;
        }
    } else {
        int temp = Menu_Framework_dpPpBarAnimState;

        switch (Menu_Framework_dpPpBarsState) {
        case 0:
            if (temp < 5) {
                ++Menu_Framework_dpPpBarAnimState;
            }
            break;

        case 1:
            if (temp != 0) {
                --Menu_Framework_dpPpBarAnimState;
            }
            break;

        case 2:
            Menu_Framework_dpPpBarAnimState = 5;
            break;
        }

        if (Menu_Framework_dpPpBarAnimState != 0) {

            int temp_s1 = ((Menu_Framework_dpPpBarAnimState * 8) - 8) << 0x10;

            if (Menu_Framework_dpPpBarsTarget & 1) {
                Render_DrawCompactText("DP", (temp_s1 + vs_getXY(0, -1)) | 10, before);

                temp = Render_DrawIntegerRightAligned(0, temp_s1 | 66, Menu_Framework_maxDp, before);
                Render_DrawIntegerRightAligned(2, temp, 0, before);
                Render_DrawIntegerRightAligned(1, temp + vs_getXY(-7, -2), Menu_Framework_currentDp, before);
            }

            if (Menu_Framework_dpPpBarsTarget & 2) {
                Render_DrawCompactText("PP", (temp_s1 + vs_getXY(0, -1)) | 80, before);

                temp = Render_DrawIntegerRightAligned(0, temp_s1 | 136, Menu_Framework_maxPp, before);
                Render_DrawIntegerRightAligned(2, temp, 0, before);
                Render_DrawIntegerRightAligned(1, temp + vs_getXY(-7, -2), Menu_Framework_currentPp, before);
            }

            temp_s1 += 8 << 16;

            if (Menu_Framework_dpPpBarsTarget & 1) {

                temp = Menu_Framework_maxDp;

                if (temp == 0) {
                    temp = 1;
                }

                Render_DrawStatGauge(
                    0, (((Menu_Framework_currentDp << 6) + temp) - 1) / temp, before, temp_s1 | 10);
            }
            if (Menu_Framework_dpPpBarsTarget & 2) {

                temp = Menu_Framework_maxPp;

                if (temp == 0) {
                    temp = 1;
                }

                Render_DrawStatGauge(
                    1, (((Menu_Framework_currentPp << 6) + temp) - 1) / temp, before, temp_s1 | 80);
            }
        }
    }
}

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Framework_EquipmentDetails_ResetMainMenuStatics(void)
{
    {
        static const u_short initial = 0;
        memcpy((void*)&Menu_Framework_currentDp, &initial, sizeof Menu_Framework_currentDp);
    }
    {
        static const u_short initial = 0;
        memcpy((void*)&Menu_Framework_currentPp, &initial, sizeof Menu_Framework_currentPp);
    }
    {
        static const u_short initial = 0;
        memcpy((void*)&Menu_Framework_maxDp, &initial, sizeof Menu_Framework_maxDp);
    }
    {
        static const u_short initial = 0;
        memcpy((void*)&Menu_Framework_maxPp, &initial, sizeof Menu_Framework_maxPp);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_uiElement15Enabled, &initial, sizeof Menu_Framework_uiElement15Enabled);
    }
    {
        static const u_short initial = 0;
        memcpy((void*)&Menu_Framework_dpPpBarAnimState, &initial, sizeof Menu_Framework_dpPpBarAnimState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_itemDetailsCursorAnimStep, &initial, sizeof Menu_Framework_itemDetailsCursorAnimStep);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_itemDetailsPreviousItem, &initial, sizeof Menu_Framework_itemDetailsPreviousItem);
    }
    {
        static const char* (*initial[])(int row) = { Menu_Framework_SelectWeaponDetailRow, Menu_Framework_BladeDetailDescription,
        Menu_Framework_GripDetailDescription, Menu_Framework_SelectShieldDetailRow, Menu_Framework_ArmorDetailDescription, Menu_Framework_ArmorDetailDescription,
        Menu_Framework_GemDetailDescription };
        memcpy((void*)&Menu_Framework_itemDetailsStatusViewDispatch, &initial, sizeof Menu_Framework_itemDetailsStatusViewDispatch);
    }
    {
        static const int initial[] = { vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 110, 63, 105),
        vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 220, 120, 159), vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 220, 120, 159),
        vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 110, 63, 105) };
        memcpy((void*)&Menu_Framework_equipmentStatPageNegativeStatColors, &initial, sizeof Menu_Framework_equipmentStatPageNegativeStatColors);
    }
    {
        static const int initial[] = { vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 24, 115, 129),
        vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 48, 224, 208), vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 48, 224, 208),
        vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 24, 115, 129) };
        memcpy((void*)&Menu_Framework_equipmentStatPagePositiveStatColors, &initial, sizeof Menu_Framework_equipmentStatPagePositiveStatColors);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Framework_statPagePanelDirection, &initial, sizeof Menu_Framework_statPagePanelDirection);
    }
    {
        static const u_char initial = 0;
        memcpy((void*)&Menu_Framework_statPagePanelSlideStep, &initial, sizeof Menu_Framework_statPagePanelSlideStep);
    }
    {
        static const u_char initial = 0;
        memcpy((void*)&Menu_Framework_primaryStatPanelSlideStep, &initial, sizeof Menu_Framework_primaryStatPanelSlideStep);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_primaryStatPanelStatsAnimation, &initial, sizeof Menu_Framework_primaryStatPanelStatsAnimation);
    }
    {
        static const char initial = 1;
        memcpy((void*)&Menu_Framework_dpPpBarsState, &initial, sizeof Menu_Framework_dpPpBarsState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_dpPpBarsTarget, &initial, sizeof Menu_Framework_dpPpBarsTarget);
    }
}

