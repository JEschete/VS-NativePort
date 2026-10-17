// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Targeting.h"
#include "Content/StringTables.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Text/TextCode.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Setup/Assembly.h"
#include "Menu/Setup/SetupScreen.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Settings.h"

// decomp: local static of _assembleMenu, at file scope so the module reset reaches it
static char Menu_Setup_assemblyCursorAnimation = 0;

// decomp: local static of _assembleMenu, at file scope so the module reset reaches it
static char Menu_Setup_assemblyOkBlinkPhase = 0;

// decomp: local static of _attachGemsMenu, at file scope so the module reset reaches it
static char Menu_Setup_gemAttachmentCursorAnimation = 0;

// decomp: local static of _attachGemsMenu, at file scope so the module reset reaches it
static char Menu_Setup_gemAttachmentOkBlinkPhase = 0;

// decomp: local static of _attachGemsMenu, at file scope so the module reset reaches it
static char Menu_Setup_stagedGems[4];

// decomp: _ (_@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_layoutPadding6[2];

// decomp: _availableItems (_availableItems@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_availableItems[64];

// decomp: _combiningItem (_combiningItem@src/MENU/MENUC.PRG/168.c)
u_char Menu_Setup_combiningItem;

// decomp: local static of _assembleMenu, at file scope so the module reset reaches it
static int Menu_Setup_assemblyCursorPositions[] = { vs_getXY(141, 10), vs_getXY(148, 26),
        vs_getXY(148, 42), vs_getXY(155, 58), vs_getXY(155, 74), vs_getXY(155, 90),
        vs_getXY(155, 106) };

// decomp: _slotSelectionHistory (_slotSelectionHistory@src/MENU/MENUC.PRG/168.c)
u_int Menu_Setup_slotSelectionHistory[5];

// decomp: local static of _isValidGrip, at file scope so the module reset reaches it
static u_short Menu_Setup_isGripCompatibleValidGripFlags[] = { 0, 0xE, 0xF0, 0x100, 0x200 };

// decomp: local static of _assembleMenu, at file scope so the module reset reaches it
static char Menu_Setup_assemblyGemInfo[4];

// decomp: local static of _attachGemsMenu, at file scope so the module reset reaches it
static char Menu_Setup_gemAttachmentTargetGems[4];

// decomp: MENUC.PRG _pushSelectionHistory (src/MENU/MENUC.PRG/168.c:56)
void Menu_Setup_RecordSlotSelection(int index)
{
    u_int i;
    u_int max = 0;

    if (index < 5) {

        for (i = 0; i < 5; ++i) {
            if (max < Menu_Setup_slotSelectionHistory[i]) {
                max = Menu_Setup_slotSelectionHistory[i];
            }
        }

        Menu_Setup_slotSelectionHistory[index] = max + 1;

        return;
    }

    Core_Memory_ZeroWordsBackward(Menu_Setup_slotSelectionHistory, sizeof Menu_Setup_slotSelectionHistory);
}

// decomp: MENUC.PRG _popSelectionHistory (src/MENU/MENUC.PRG/168.c:83)
int Menu_Setup_PopSlotSelection(int index)
{
    int i;
    int max = 0;

    if (index < 5) {
        Menu_Setup_slotSelectionHistory[index] = 0;
    } else {

        for (i = 0; i < 5; ++i) {
            if (Menu_Setup_slotSelectionHistory[max] < Menu_Setup_slotSelectionHistory[i]) {
                max = i;
            }
        }

        if (Menu_Setup_slotSelectionHistory[max] != 0) {
            Menu_Setup_slotSelectionHistory[max] = 0;
            return max + 1;
        }
    }

    return 0;
}

// decomp: MENUC.PRG _disassembleWeapon (src/MENU/MENUC.PRG/168.c:310)
void Menu_Setup_DisassembleWeapon(int weaponIndex)
{
    int i;
    int index;
    Items_InventoryWeapon* weapon = &Audio_Seq_unusedInventory.weapons[weaponIndex];

    if (weapon->blade == 0) {
        return;
    }

    for (i = 0; i < Audio_Seq_unusedInventory.grips[weapon->grip - 1].gemSlots; ++i) {
        index = weapon->gems[i];
        if (index != 0) {
            Audio_Seq_unusedInventory.gems[index - 1].setItemIndex = 0;
            weapon->gems[i] = 0;
        }
    }

    Audio_Seq_unusedInventory.grips[weapon->grip - 1].assembledWeaponIndex = 0;
    Audio_Seq_unusedInventory.blades[weapon->blade - 1].assembledWeaponIndex = 0;

    index = weapon->index;
    Core_Memory_ZeroWordsBackward(weapon, sizeof *weapon);
    weapon->index = index;

    Items_Inventory_RebuildDisplayIndices(0);
}

// decomp: MENUC.PRG _assembleWeapon (src/MENU/MENUC.PRG/168.c:345)
void Menu_Setup_AssembleWeapon(int bladeIndex, int gripIndex, int gemInfo)
{
    int i;
    int weaponIndex;

    Items_InventoryWeapon* weapon = &Audio_Seq_unusedInventory.weapons[Menu_Setup_combiningItem];
    Items_InventoryBlade* blade = &Audio_Seq_unusedInventory.blades[bladeIndex];
    Items_InventoryGrip* grip = &Audio_Seq_unusedInventory.grips[gripIndex];
    int gemSlots = grip->gemSlots;

    Menu_Setup_DisassembleWeapon(Menu_Setup_combiningItem);

    weapon->blade = bladeIndex + 1;
    weapon->grip = gripIndex + 1;

    for (i = 0; i < gemSlots; ++i) {
        weapon->gems[i] = gemInfo;
        gemInfo >>= 8;
    }

    weaponIndex = Menu_Setup_combiningItem + 1;
    blade->assembledWeaponIndex = weaponIndex;
    grip->assembledWeaponIndex = weaponIndex;

    for (i = 0; i < gemSlots; ++i) {
        gemInfo = weapon->gems[i];
        if (gemInfo != 0) {
            Audio_Seq_unusedInventory.gems[gemInfo - 1].setItemIndex = weaponIndex;
        }
    }

    memset(weapon->name, Core_TextCode_Terminator, sizeof weapon->name);
    Items_Inventory_RebuildDisplayIndices(0);
}

// decomp: MENUC.PRG _disassembleShield (src/MENU/MENUC.PRG/168.c:384)
void Menu_Setup_DetachShieldGems(int index)
{
    int i;
    Items_InventoryShield* shield = &Audio_Seq_unusedInventory.shields[index];
    int slots = shield->base.gemSlots;

    for (i = 0; i < slots; ++i) {
        int gemIndex = shield->gems[i];
        if (gemIndex != 0) {
            Audio_Seq_unusedInventory.gems[gemIndex - 1].setItemIndex = 0;
            shield->gems[i] = 0;
        }
    }
}

// decomp: MENUC.PRG _isValidGrip (src/MENU/MENUC.PRG/168.c:403)
int Menu_Setup_IsGripCompatible(Items_InventoryBlade* blade, Items_InventoryGrip* grip)
{

    if ((blade == NULL) || (grip == NULL)) {
        return 1;
    }
    return (1 << blade->category) & Menu_Setup_isGripCompatibleValidGripFlags[grip->category];
}

// decomp: MENUC.PRG _setUiStats (src/MENU/MENUC.PRG/168.c:730)
void Menu_Setup_SetAssemblyStats(int blade, int grip)
{
    int var_v1;

    Menu_Framework_ResetEquipmentStats();
    Menu_Framework_SetRangeAndRisk(0, 0, 0, 1);
    Menu_Framework_equipmentSubtype = 0;

    var_v1 = blade != 0;
    if (grip != 0) {
        var_v1 += 2;
    }

    switch (var_v1) {
    case 1:
        Menu_Framework_SetBladeStats(blade);
        break;
    case 2:
        Menu_Framework_SetGripStats(grip);
        break;
    case 3:
        Menu_Framework_SetWeaponStats(Menu_Setup_combiningItem + 1);
        break;
    }

    Menu_Framework_SetEnabledStatPages(7);
    Menu_Framework_UpdateDpPpBars(11);
}

// decomp: MENUC.PRG _assembleBladeMenuHeader (src/MENU/MENUC.PRG/168.c:764)
void Menu_Setup_SetAssemblyHeader(int bladeGrip)
{
    Menu_Framework_MenuRow* menuItem;

    int blade = (bladeGrip >> 4) & 0x3F;
    int grip = (bladeGrip >> 10) & 0x3F;
    bladeGrip &= 0xF;
    menuItem = Menu_Framework_SetMenuItem(32, 16, 18, 164, 8,
        (char*)(&Menu_Framework_menu12Text
                [Menu_Framework_menu12Text[bladeGrip + Text_Menu_MENU12_BIN_Index_assemble]]));
    menuItem->rowIcon = 24;
    menuItem->selected = 1;
    if (bladeGrip == 0) {
        Menu_Setup_SetAssemblyStats(blade, grip);
    }
}

// decomp: MENUC.PRG _assembleBladeMenu (src/MENU/MENUC.PRG/168.c:802)
int Menu_Setup_SelectAssemblyBlade(int params)
{
    static int Menu_Setup_assemblyBladeSelectedRow;
    static char Menu_Setup_assemblyBladeState;
    static char Menu_Setup_assemblyBladeToAssemble;
    static char Menu_Setup_assemblyBladeGripToAssemble;
    static char Menu_Setup_assemblyBladeLayoutPadding[1] __attribute__((unused));

    int bladeCount;
    int i;
    void* textBuf;
    char** menuText;
    int itemId;
    Items_InventoryBlade* blade = NULL;
    Items_InventoryGrip* grip = NULL;

    if (params != 0) {
        Menu_Setup_assemblyBladeToAssemble = params & 0x7F;
        Menu_Setup_assemblyBladeGripToAssemble = (params >> 8);
        Menu_Setup_AnimateItemHeader(11);
        Menu_Setup_assemblyBladeState = 0;
        return 0;
    }

    if (Menu_Setup_assemblyBladeToAssemble != 0) {
        blade = &Audio_Seq_unusedInventory.blades[Menu_Setup_assemblyBladeToAssemble - 1];
    }
    if (Menu_Setup_assemblyBladeGripToAssemble != 0) {
        grip = &Audio_Seq_unusedInventory.grips[Menu_Setup_assemblyBladeGripToAssemble - 1];
    }

    switch (Menu_Setup_assemblyBladeState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            textBuf = Core_Memory_AllocateFromHighEnd(0x72C);
            menuText = textBuf + 0x6A4;
            if (blade == NULL) {
                menuText[0] = (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_blades];
                menuText[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_bladesDesc];
                *Menu_Framework_rowTypeBuf = 1;
            } else {
                Menu_Framework_FormatBladeRow(blade, menuText, Menu_Framework_rowTypeBuf, textBuf);
            }

            bladeCount = 1;
            Menu_Setup_availableItems[0] = Menu_Setup_assemblyBladeToAssemble;

            for (i = 0; i < 16; ++i) {
                itemId = Audio_Seq_unusedInventoryIndices.blades[i];

                if ((itemId != 0) && (itemId != Menu_Setup_assemblyBladeToAssemble)) {

                    blade = &Audio_Seq_unusedInventory.blades[itemId - 1];

                    if (blade->assembledWeaponIndex == 0) {

                        Menu_Framework_FormatBladeRow(blade, menuText + bladeCount * 2,
                            &Menu_Framework_rowTypeBuf[bladeCount], textBuf + bladeCount * 96);

                        if (Menu_Setup_IsGripCompatible(blade, grip) == 0) {
                            menuText[bladeCount * 2 + 1] = (char*)&Menu_Framework_menu12Text
                                [Text_Menu_MENU12_BIN_Offset_invalidBlade];
                            Menu_Framework_rowTypeBuf[bladeCount] |= 1;
                        }

                        Menu_Setup_availableItems[bladeCount] = itemId;
                        ++bladeCount;
                    }
                }
            }

            Menu_Setup_BuildItemList(bladeCount, menuText, Menu_Framework_rowTypeBuf);
            Core_Memory_FreeFromHighEnd(textBuf);
            Menu_Setup_assemblyBladeState = 1;
        }
        break;
    case 1:
        itemId = Menu_Setup_availableItems[Menu_Setup_itemsListWindow + Menu_Setup_itemsListSelection];
        if (itemId != 0) {
            Menu_Framework_SetBladeStats(itemId);
        } else {
            Menu_Framework_SetEnabledStatPages(3);
            Menu_Framework_ResetEquipmentStats();
            Menu_Framework_SetRangeAndRisk(0, 0, 0, 1);
        }

        Menu_Framework_UpdateInformationVisibility(
            Menu_Setup_itemsListSelection + Menu_Setup_itemsListWindow, Menu_Setup_itemsListState);
        Menu_Setup_UpdateItemList(2);

        Menu_Setup_assemblyBladeSelectedRow = Menu_Setup_GetSelectedItemRow() + 1;

        if (Menu_Setup_assemblyBladeSelectedRow != 0) {
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            if (Menu_Setup_assemblyBladeSelectedRow == -2) {
                return -2;
            }
            if (Menu_Setup_assemblyBladeSelectedRow > 0) {
                Menu_Setup_assemblyBladeSelectedRow = Menu_Setup_availableItems[Menu_Setup_assemblyBladeSelectedRow - 1];
            }
            Menu_Setup_assemblyBladeState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Setup_assemblyBladeSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _assembleGripMenu (src/MENU/MENUC.PRG/168.c:923)
int Menu_Setup_SelectAssemblyGrip(int params)
{
    static int Menu_Setup_assemblyGripSelectedRow;
    static char Menu_Setup_assemblyGripState;
    static char Menu_Setup_assemblyGripBladeToAssemble;
    static char Menu_Setup_assemblyGripToAssemble;
    static char Menu_Setup_assemblyGripLayoutPadding __attribute__((unused));

    int gripCount;
    int i;
    void* textBuf;
    char** menuText;
    int itemId;

    Items_InventoryGrip* grip = NULL;
    Items_InventoryBlade* blade = NULL;

    if (params != 0) {
        Menu_Setup_assemblyGripToAssemble = params & 0x7F;
        Menu_Setup_assemblyGripBladeToAssemble = params >> 8;
        Menu_Setup_AnimateItemHeader(0xC);
        Menu_Setup_assemblyGripState = 0;
        return 0;
    }
    if (Menu_Setup_assemblyGripBladeToAssemble != 0) {
        blade = &Audio_Seq_unusedInventory.blades[Menu_Setup_assemblyGripBladeToAssemble - 1];
    }
    if (Menu_Setup_assemblyGripToAssemble != 0) {
        grip = &Audio_Seq_unusedInventory.grips[Menu_Setup_assemblyGripToAssemble - 1];
    }

    switch (Menu_Setup_assemblyGripState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            textBuf = Core_Memory_AllocateFromHighEnd(0x72CU);
            menuText = textBuf + 0x6A4;
            if (grip == NULL) {
                menuText[0] = (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_grips];
                menuText[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_gripsDesc];
                *Menu_Framework_rowTypeBuf = 1;
            } else {
                Menu_Framework_FormatGripRow(grip, menuText, Menu_Framework_rowTypeBuf, textBuf);
            }

            gripCount = 1;
            Menu_Setup_availableItems[0] = Menu_Setup_assemblyGripToAssemble;

            for (i = 0; i < 16; ++i) {

                itemId = Audio_Seq_unusedInventoryIndices.grips[i];

                if ((itemId != 0) && (itemId != Menu_Setup_assemblyGripToAssemble)) {

                    grip = &Audio_Seq_unusedInventory.grips[itemId - 1];

                    if (grip->assembledWeaponIndex == 0) {

                        Menu_Framework_FormatGripRow(grip, menuText + gripCount * 2,
                            &Menu_Framework_rowTypeBuf[gripCount], textBuf + gripCount * 0x60);

                        if (Menu_Setup_IsGripCompatible(blade, grip) == 0) {
                            menuText[gripCount * 2 + 1] = (char*)&Menu_Framework_menu12Text
                                [Text_Menu_MENU12_BIN_Offset_invalidGrip];
                            Menu_Framework_rowTypeBuf[gripCount] |= 1;
                        }

                        Menu_Setup_availableItems[gripCount] = itemId;
                        ++gripCount;
                    }
                }
            }

            Menu_Setup_BuildItemList(gripCount, menuText, Menu_Framework_rowTypeBuf);
            Core_Memory_FreeFromHighEnd(textBuf);
            Menu_Setup_assemblyGripState = 1;
        }
        break;
    case 1:
        itemId = Menu_Setup_availableItems[Menu_Setup_itemsListWindow + Menu_Setup_itemsListSelection];
        Menu_Framework_UpdateDpPpBars(8);

        if (itemId != 0) {
            Menu_Framework_SetGripStats((int)itemId);
        } else {
            Menu_Framework_SetEnabledStatPages(4);
            Menu_Framework_ResetEquipmentStats();
        }

        Menu_Framework_UpdateInformationVisibility(
            Menu_Setup_itemsListSelection + Menu_Setup_itemsListWindow, Menu_Setup_itemsListState);
        Menu_Setup_UpdateItemList(2);

        Menu_Setup_assemblyGripSelectedRow = Menu_Setup_GetSelectedItemRow() + 1;

        if (Menu_Setup_assemblyGripSelectedRow != 0) {
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            if (Menu_Setup_assemblyGripSelectedRow == -2) {
                return -2;
            }
            if (Menu_Setup_assemblyGripSelectedRow > 0) {
                Menu_Setup_assemblyGripSelectedRow = Menu_Setup_availableItems[Menu_Setup_assemblyGripSelectedRow - 1];
            }
            Menu_Setup_assemblyGripState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Setup_assemblyGripSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _attachGem (src/MENU/MENUC.PRG/168.c:1045)
int Menu_Setup_SelectGem(int params)
{
    static int Menu_Setup_gemSelectedRow;
    static char Menu_Setup_gemState;
    static char Menu_Setup_gemToAttach;

    int i;
    int itemId;
    int gemCount;
    void* textBuf;
    char** menuText;

    Items_InventoryGem* gem = NULL;

    if (params != 0) {
        i = params & 0x7;
        gemCount = params & 0xF8;
        Menu_Setup_gemToAttach = params >> gemCount;
        Menu_Setup_AnimateItemHeader(i + 10);
        Menu_Setup_gemState = 0;
        return 0;
    }

    if (Menu_Setup_gemToAttach != 0) {
        gem = &Audio_Seq_unusedInventory.gems[Menu_Setup_gemToAttach - 1];
    }

    switch (Menu_Setup_gemState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            textBuf = Core_Memory_AllocateFromHighEnd(0x14AC);
            menuText = textBuf + 0x1324;

            if (gem == NULL) {
                menuText[0] = (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_gems];
                menuText[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_gemsDesc];
                *Menu_Framework_rowTypeBuf = 1;
            } else {
                Menu_Framework_FormatGemRow(gem, menuText, Menu_Framework_rowTypeBuf, textBuf);
            }

            gemCount = 1;
            Menu_Setup_availableItems[0] = Menu_Setup_gemToAttach;

            for (i = 0; i < 48; ++i) {

                itemId = Audio_Seq_unusedInventoryIndices.gems[i];

                if ((itemId != 0) && (itemId != Menu_Setup_gemToAttach)) {

                    gem = &Audio_Seq_unusedInventory.gems[itemId - 1];

                    if (gem->setItemIndex == 0) {
                        Menu_Framework_FormatGemRow(gem, menuText + gemCount * 2,
                            &Menu_Framework_rowTypeBuf[gemCount], textBuf + gemCount * 96);
                        Menu_Setup_availableItems[gemCount] = itemId;
                        ++gemCount;
                    }
                }
            }
            Menu_Setup_BuildItemList(gemCount, menuText, Menu_Framework_rowTypeBuf);
            Core_Memory_FreeFromHighEnd(textBuf);
            Menu_Setup_gemState = 1;
        }
        break;
    case 1:
        itemId = Menu_Setup_availableItems[Menu_Setup_itemsListWindow + Menu_Setup_itemsListSelection];
        Menu_Framework_UpdateDpPpBars(8);
        if (itemId != 0) {
            Menu_Framework_SetGemStats(itemId);
        } else {
            Menu_Framework_SetEnabledStatPages(3);
            Menu_Framework_ResetEquipmentStats();
        }

        Menu_Framework_UpdateInformationVisibility(
            Menu_Setup_itemsListSelection + Menu_Setup_itemsListWindow, Menu_Setup_itemsListState);
        Menu_Setup_UpdateItemList(2);

        Menu_Setup_gemSelectedRow = Menu_Setup_GetSelectedItemRow() + 1;

        if (Menu_Setup_gemSelectedRow != 0) {
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            if (Menu_Setup_gemSelectedRow == -2) {
                return -2;
            }
            if (Menu_Setup_gemSelectedRow > 0) {
                Menu_Setup_gemSelectedRow = Menu_Setup_availableItems[Menu_Setup_gemSelectedRow - 1];
            }
            Menu_Setup_gemState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Setup_gemSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _leaveAssembleMenu (src/MENU/MENUC.PRG/168.c:1150)
int Menu_Setup_CancelAssembly(void)
{
    Menu_Setup_DisassembleWeapon(Menu_Setup_combiningItem);
    return Menu_Setup_LeaveItemSelection(1);
}

// decomp: MENUC.PRG _assembleMenu (src/MENU/MENUC.PRG/168.c:1164)
int Menu_Setup_UpdateAssembly(int arg0)
{


    static char Menu_Setup_assemblyState;
    static char Menu_Setup_assemblySelectedOption;
    static char Menu_Setup_assemblyBladeToAssemble;
    static char Menu_Setup_assemblyGripToAssemble;
    static char Menu_Setup_assemblyLayoutPadding[2] __attribute__((unused));

    char* menuText[2];
    int sp20;
    int itemInfo;
    int itemSlot;
    int submenuResult;
    int temp_v1;
    int i;
    int gemSlots;
    Items_InventoryBlade* blade;
    Items_InventoryGrip* grip;
    Menu_Framework_MenuRow* menuItem;

    if (arg0 != 0) {
        Menu_InitInformationTextBox(1);
        Menu_Setup_RecordSlotSelection(5);
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Setup_combiningItem = Items_Inventory_FirstEmptySlot(0, NULL);
        Menu_Setup_assemblyBladeToAssemble = 0;
        Menu_Setup_assemblyGripToAssemble = 0;
        *(int*)Menu_Setup_assemblyGemInfo = 0;
        Menu_Setup_assemblySelectedOption = 0;
        Menu_Setup_assemblyState = 0;
        return 0;
    }

    gemSlots = 0;
    itemInfo = Menu_Setup_assemblyBladeToAssemble > 0;

    if (itemInfo != 0) {
        blade = &Audio_Seq_unusedInventory.blades[Menu_Setup_assemblyBladeToAssemble - 1];
    }

    itemSlot = Menu_Setup_assemblyGripToAssemble != 0;

    if (itemSlot != 0) {
        grip = &Audio_Seq_unusedInventory.grips[Menu_Setup_assemblyGripToAssemble - 1];
    }

    itemInfo &= itemSlot;

    if (itemInfo != 0) {
        gemSlots = grip->gemSlots;
    }

    Menu_Setup_assemblyOkBlinkPhase += 16;

    if (Menu_Setup_assemblyState != 2) {
        Menu_Setup_assemblyOkBlinkPhase = 0;
    }

    switch (Menu_Setup_assemblyState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Setup_InitializeEquipmentHeader(Text_Menu_MENU12_BIN_Offset_assemble, 0x18);
            Menu_Framework_equipmentSubtype = 0;
            Menu_Setup_assemblyState = 1;
        }
        break;
    case 1:
        menuItem = Menu_Framework_SetMenuItem(0xA, 0x140, 0x12, 0xA5, 0,
            (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_weapon]);
        menuItem->state = 2;
        menuItem->selected = 1;
        menuItem->targetPosition0 = 155;
        menuItem = Menu_Framework_SetMenuItem(11, 327, 34, 158, 0,
            Menu_Setup_assemblyBladeToAssemble == 0
                ? (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_blades]
                : Menu_Framework_itemNames[blade->id]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 162;
        if (Menu_Setup_assemblyBladeToAssemble != 0) {
            menuItem->rowIcon = blade->category;
            menuItem->material = blade->material;
        } else {
            menuItem->fontColor = 1;
        }
        menuItem = Menu_Framework_SetMenuItem(0xC, 0x147, 0x32, 0x9E, 0,
            Menu_Setup_assemblyGripToAssemble == 0
                ? (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_grips]
                : Menu_Framework_itemNames[Audio_Seq_unusedInventory.grips[Menu_Setup_assemblyGripToAssemble - 1].id]);
        menuItem->state = 2;
        menuItem->fontColor = Menu_Setup_assemblyGripToAssemble == 0;
        menuItem->targetPosition0 = 0xA2;
        if (Menu_Setup_assemblyGripToAssemble != 0) {
            menuItem->rowIcon = grip->category + 0xA;
        } else {
            menuItem->fontColor = 1;
        }

        for (i = 0; i < gemSlots; ++i) {
            int v = Menu_Setup_assemblyGemInfo[i];
            menuItem = Menu_Framework_SetMenuItem(i + 0xD, 0x14E, 0x42 + i * 0x10, 0x97, 0,
                v == 0 ? (char*)(Menu_Framework_menu12Text + 0x421)
                       : Menu_Framework_itemNames[Audio_Seq_unusedInventory.gems[v - 1].id]);
            menuItem->state = 2;
            menuItem->targetPosition0 = 0xA9;
            if (v != 0) {
                menuItem->rowIcon = 0x16;
            } else {
                menuItem->fontColor = 1;
            }
        }
        Menu_Setup_assemblyState = 2;
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            if (!(Input_buttonsPressed.allButtons & PADRup)) {

                if (itemInfo != 0) {
                    Menu_Setup_AssembleWeapon(
                        Menu_Setup_assemblyBladeToAssemble - 1, Menu_Setup_assemblyGripToAssemble - 1, *(int*)Menu_Setup_assemblyGemInfo);
                    Menu_Setup_DrawConfirmationLabel(gemSlots + 2, Menu_Setup_assemblyOkBlinkPhase);
                }

                if (Input_buttonsPressed.allButtons & PADRdown) {

                    itemSlot = Menu_Setup_PopSlotSelection(5);
                    Menu_Setup_DisassembleWeapon(Menu_Setup_combiningItem);

                    switch (itemSlot) {
                    case 0:
                        return Menu_Setup_CancelAssembly();
                    case 1:
                        Menu_Setup_assemblyBladeToAssemble = 0;
                        *(int*)Menu_Setup_assemblyGemInfo = 0;
                        for (itemInfo = 13; itemInfo < 16; ++itemInfo) {
                            Menu_Framework_FlyOutRowRight(itemInfo);
                        }
                        itemInfo = Text_Menu_MENU12_BIN_Offset_blades;
                        break;
                    case 2:
                        Menu_Setup_assemblyGripToAssemble = 0;
                        *(int*)Menu_Setup_assemblyGemInfo = 0;
                        for (itemInfo = 13; itemInfo < 16; ++itemInfo) {
                            Menu_Framework_FlyOutRowRight(itemInfo);
                        }
                        itemInfo = Text_Menu_MENU12_BIN_Offset_grips;
                        break;
                    default:
                        Menu_Setup_assemblyGemInfo[itemSlot - 3] = 0;
                        Menu_Setup_AssembleWeapon(
                            Menu_Setup_assemblyBladeToAssemble - 1, Menu_Setup_assemblyGripToAssemble - 1, *(int*)Menu_Setup_assemblyGemInfo);
                        itemInfo = Text_Menu_MENU12_BIN_Offset_gems;
                        break;
                    }

                    Menu_PlayLeaveSound();
                    Menu_Setup_assemblySelectedOption = itemSlot - 1;
                    menuItem = Menu_Framework_SetMenuItem(itemSlot + 10,
                        itemSlot < 3 ? 162 : 169, (itemSlot * 16) + 18, 151, 0,
                        (char*)&Menu_Framework_menu12Text[itemInfo]);
                    menuItem->selected = 1;
                    menuItem->fontColor = 1;
                    Menu_Setup_SetAssemblyStats(Menu_Setup_assemblyBladeToAssemble, Menu_Setup_assemblyGripToAssemble);
                    break;
                }

                if ((Input_buttonsPressed.allButtons & PADstart) && (itemInfo != 0)) {
                    Menu_PlaySelectSound();
                    Menu_Framework_MenuItem(Menu_Setup_assemblySelectedOption + 11)->selected = 0;
                    Menu_Setup_assemblySelectedOption = gemSlots + 2;
                    Menu_Setup_assemblyState = 6;
                    break;
                }

                i = Menu_Setup_assemblySelectedOption;

                if (Input_buttonsPressed.allButtons & PADRright) {
                    Menu_PlaySelectSound();
                    if (i == (gemSlots + 2)) {
                        Menu_Setup_assemblyState = 6;
                    } else {
                        int a0;
                        int a0_2;
                        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                        switch (i) {
                        case 0:
                            Menu_Setup_SetAssemblyHeader(1);
                            a0 = Menu_Setup_assemblyGripToAssemble << 8;
                            a0 += 128;
                            Menu_Setup_SelectAssemblyBlade(Menu_Setup_assemblyBladeToAssemble + a0);
                            Menu_Setup_assemblyState = 3;
                            break;
                        case 1:
                            Menu_Setup_SetAssemblyHeader(2);
                            a0_2 = Menu_Setup_assemblyBladeToAssemble << 8;
                            a0_2 += 128;
                            Menu_Setup_SelectAssemblyGrip(Menu_Setup_assemblyGripToAssemble + a0_2);
                            Menu_Setup_assemblyState = 4;
                            break;
                        default:
                            Menu_Setup_SetAssemblyHeader(3);
                            Menu_Setup_SelectGem((i + 1) | ((i - 1) * 8) | (*(int*)Menu_Setup_assemblyGemInfo << 8));
                            Menu_Setup_assemblyState = 5;
                            break;
                        }
                        break;
                    }
                }
                if (Input_buttonRepeat & PADLup) {
                    if (i == 0) {
                        i = gemSlots + itemInfo + 1;
                    } else {
                        --i;
                    }
                }
                if (Input_buttonRepeat & PADLdown) {
                    if (i == (gemSlots + itemInfo + 1)) {
                        i = 0;
                    } else {
                        ++i;
                    }
                }
                if (i != Menu_Setup_assemblySelectedOption) {
                    Menu_PlayChangeSound();
                    Menu_Setup_assemblySelectedOption = i;
                }

                for (i = 0; i < 5; ++i) {
                    Menu_Framework_MenuItem(i + 11)->selected = i == Menu_Setup_assemblySelectedOption;
                }

                Menu_Framework_rowTypeBuf[0] = Menu_Setup_assemblyBladeToAssemble == 0;
                Menu_Framework_rowTypeBuf[1] = Menu_Setup_assemblyGripToAssemble == 0;

                for (i = 0; i < gemSlots; ++i) {
                    Menu_Framework_rowTypeBuf[i + 2] = Menu_Setup_assemblyGemInfo[i] == 0;
                }

                Menu_Framework_rowTypeBuf[i + 2] = 1;

                Menu_Framework_UpdateInformationVisibility(Menu_Setup_assemblySelectedOption, 16);

                if (Menu_Setup_assemblySelectedOption >= 2) {
                    i = Text_Menu_MENU12_BIN_Index_selectGem;
                    if (Menu_Setup_assemblySelectedOption == (gemSlots + 2)) {
                        i = Text_Menu_MENU12_BIN_Index_pressOk;
                    }
                } else {
                    i = Menu_Setup_assemblySelectedOption + Text_Menu_MENU12_BIN_Index_selectBladeToAssemble;
                }

                menuText[1] = (char*)&Menu_Framework_menu12Text[Menu_Framework_menu12Text[i]];

                switch (Menu_Setup_assemblySelectedOption) {
                case 0:
                    if (Menu_Setup_assemblyBladeToAssemble != 0) {
                        Menu_Framework_FormatBladeRow(
                            &Audio_Seq_unusedInventory.blades[Menu_Setup_assemblyBladeToAssemble - 1], menuText,
                            &sp20, Menu_Framework_stringBuf);
                    }
                    break;
                case 1:
                    if (Menu_Setup_assemblyGripToAssemble != 0) {
                        Menu_Framework_FormatGripRow(
                            &Audio_Seq_unusedInventory.grips[Menu_Setup_assemblyGripToAssemble - 1], menuText, &sp20,
                            Menu_Framework_stringBuf);
                    }
                    break;
                case 2:
                case 3:
                case 4:
                    itemSlot = Menu_Setup_assemblyGemInfo[Menu_Setup_assemblySelectedOption - 2];
                    if (itemSlot != 0) {
                        Menu_Framework_FormatGemRow(&Audio_Seq_unusedInventory.gems[itemSlot - 1],
                            menuText, &sp20, Menu_Framework_stringBuf);
                    }
                    break;
                }

                Menu_Framework_SetInformationMessage(menuText[1]);

                i = Menu_Setup_assemblyCursorPositions[Menu_Setup_assemblySelectedOption + 1];

                if (Menu_Setup_assemblySelectedOption == (gemSlots + 2)) {
                    i += 0xFFFC0000;
                }

                Menu_Setup_assemblyCursorAnimation = Menu_Framework_DrawCursor(Menu_Setup_assemblyCursorAnimation, i);

            } else {
                return Menu_Setup_CancelAssembly();
            }
        } else {
            Menu_Setup_assemblyOkBlinkPhase = 0;
        }
        break;
    case 3:
        submenuResult = Menu_Setup_SelectAssemblyBlade(0);
        if (submenuResult == 0) {
            break;
        }
        if (submenuResult != -2) {
            if (submenuResult > 0) {

                *(int*)Menu_Setup_assemblyGemInfo = 0;

                for (itemInfo = 13; itemInfo < 16; ++itemInfo) {
                    Menu_Framework_FlyOutRowRight(itemInfo);
                }

                if (submenuResult == Menu_Setup_assemblyBladeToAssemble) {
                    Menu_Setup_assemblyBladeToAssemble = 0;
                    Menu_Setup_PopSlotSelection(0);
                    Menu_Setup_DisassembleWeapon(Menu_Setup_combiningItem);
                } else {
                    Menu_Setup_assemblyBladeToAssemble = submenuResult;
                    Menu_Setup_RecordSlotSelection(0);
                    Menu_Setup_assemblySelectedOption = 1;
                    if (Menu_Setup_assemblyGripToAssemble != 0) {
                        Menu_Setup_AssembleWeapon(
                            Menu_Setup_assemblyBladeToAssemble - 1, Menu_Setup_assemblyGripToAssemble - 1, *(int*)Menu_Setup_assemblyGemInfo);
                    }
                }
            }
            Menu_Setup_SetAssemblyHeader(Menu_Setup_assemblyBladeToAssemble * 0x10 | (Menu_Setup_assemblyGripToAssemble << 0xA));
            Menu_Setup_assemblyState = 1;
            break;
        }
        return Menu_Setup_CancelAssembly();
    case 4:
        submenuResult = Menu_Setup_SelectAssemblyGrip(0);
        if (submenuResult == 0) {
            break;
        }
        if (submenuResult != -2) {
            if (submenuResult > 0) {
                *(int*)Menu_Setup_assemblyGemInfo = 0;
                for (itemInfo = 13; itemInfo < 16; ++itemInfo) {
                    Menu_Framework_FlyOutRowRight(itemInfo);
                }
                if (submenuResult == Menu_Setup_assemblyGripToAssemble) {
                    Menu_Setup_assemblyGripToAssemble = 0;
                    Menu_Setup_PopSlotSelection(1);
                    Menu_Setup_DisassembleWeapon(Menu_Setup_combiningItem);
                } else {
                    Menu_Setup_assemblyGripToAssemble = submenuResult;
                    Menu_Setup_RecordSlotSelection(1);
                    Menu_Setup_assemblySelectedOption = ((Menu_Setup_assemblyBladeToAssemble != 0) * 2);
                    if (Menu_Setup_assemblyBladeToAssemble != 0) {
                        Menu_Setup_AssembleWeapon(
                            Menu_Setup_assemblyBladeToAssemble - 1, Menu_Setup_assemblyGripToAssemble - 1, *(int*)Menu_Setup_assemblyGemInfo);
                    }
                }
            }
            Menu_Setup_SetAssemblyHeader(Menu_Setup_assemblyBladeToAssemble * 0x10 | (Menu_Setup_assemblyGripToAssemble << 0xA));
            Menu_Setup_assemblyState = 1;
            break;
        }
        return Menu_Setup_CancelAssembly();
    case 5:
        submenuResult = Menu_Setup_SelectGem(0);
        if (submenuResult == 0) {
            break;
        }
        if (submenuResult != -2) {
            if (submenuResult > 0) {
                if (submenuResult == Menu_Setup_assemblyGemInfo[Menu_Setup_assemblySelectedOption - 2]) {
                    Menu_Setup_assemblyGemInfo[Menu_Setup_assemblySelectedOption - 2] = 0;
                    Menu_Setup_PopSlotSelection(Menu_Setup_assemblySelectedOption);
                } else {
                    Menu_Setup_assemblyGemInfo[Menu_Setup_assemblySelectedOption - 2] = submenuResult;
                    Menu_Setup_RecordSlotSelection(Menu_Setup_assemblySelectedOption);
                    ++Menu_Setup_assemblySelectedOption;
                }
            }
            Menu_Setup_AssembleWeapon(Menu_Setup_assemblyBladeToAssemble - 1, Menu_Setup_assemblyGripToAssemble - 1, *(int*)Menu_Setup_assemblyGemInfo);
            Menu_Setup_SetAssemblyHeader(Menu_Setup_assemblyBladeToAssemble * 0x10 | (Menu_Setup_assemblyGripToAssemble << 0xA));
            Menu_Setup_assemblyState = 1;
            break;
        }
        return Menu_Setup_CancelAssembly();
    case 6:
        Menu_Framework_UpdatePrimaryStatPanel(2);
        Menu_Setup_UpdateThreeChoicePrompt(3);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_attachmentConfirm]);
        Menu_Setup_assemblyState = 7;
        break;
    case 7:
        itemSlot = Menu_Setup_UpdateThreeChoicePrompt(0);

        if (itemSlot == 0) {
            break;
        }

        switch (itemSlot) {
        case 1:
            Menu_PlaySelectSound();
            Menu_Framework_ClearMenuExcept(10);
            Menu_Framework_UpdateEquipmentStatPages(-1);
            Menu_Framework_UpdateDpPpBars(4);
            Menu_Setup_assemblyState = 8;
            break;
        case 2:
            Menu_Framework_UpdatePrimaryStatPanel(1);
            Menu_Setup_assemblyState = itemSlot;
            break;
        case 3:
            return Menu_Setup_CancelAssembly();
        }
        break;
    case 8:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_cursorColor = 0;
            Menu_Framework_stringBuf[0] = 0;
            Menu_Framework_stringBuf[1] = Menu_Setup_combiningItem;
            Menu_Setup_SetAssemblyHeader(4);
            Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
            return 1;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _addGemSelector (src/MENU/MENUC.PRG/168.c:1599)
void Menu_Setup_CreateEmptyGemSlotRow(int arg0 __attribute__((unused)), int arg1)
{
    Menu_Framework_MenuRow* temp_v0 =
        Menu_Framework_SetMenuItem(arg1 + 10, 320, (arg1 * 16) + 18, 320 - 169, 0,
            (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_gems]);
    temp_v0->state = 2;
    temp_v0->targetPosition0 = 169;
    temp_v0->fontColor = 1;
}

// decomp: MENUC.PRG _attachGems (src/MENU/MENUC.PRG/168.c:1616)
int Menu_Setup_RestoreGemsAndLeaveSelection(char* gems, int isShield)
{
    int i;
    int gem;
    u_char* attachedGems;
    Items_InventoryShield* shield;
    Items_InventoryWeapon* weapon;

    weapon = &Audio_Seq_unusedInventory.weapons[Menu_Setup_combiningItem];
    shield = &Audio_Seq_unusedInventory.shields[Menu_Setup_combiningItem];

    if (isShield != 0) {
        attachedGems = shield->gems;
    } else {
        attachedGems = weapon->gems;
    }

    for (i = 0; i < 3; ++i) {
        gem = attachedGems[i];
        if (gem != 0) {
            Audio_Seq_unusedInventory.gems[gem - 1].setItemIndex = 0;
            attachedGems[i] = 0;
        }
    }

    for (i = 0; i < 3; ++i) {
        gem = gems[i];
        if (gem != 0) {
            Audio_Seq_unusedInventory.gems[gem - 1].setItemIndex =
                Menu_Setup_combiningItem + ((isShield << 7) + 1);
            attachedGems[i] = gem;
        }
    }
    if (isShield != 0) {
        if (shield->isEquipped != 0) {
            Items_EquipAshleyShield(shield);
        }
    } else if (weapon->isEquipped != 0) {
        Items_EquipAshleyWeapon(weapon);
    }
    return Menu_Setup_LeaveItemSelection(1);
}

// decomp: MENUC.PRG _attachGemsMenu (src/MENU/MENUC.PRG/168.c:1666)
int Menu_Setup_UpdateGemAttachment(int arg0)
{

    static int Menu_Setup_gemAttachmentSelectedRow;
    static char Menu_Setup_gemAttachmentState;
    static char Menu_Setup_gemAttachmentAppearStep;
    static u_char Menu_Setup_selectedGemSlot;
    static char Menu_Setup_gemAttachmentGemSlots;
    static char Menu_Setup_gemAttachmentIsShield;
    static char Menu_Setup_gemAttachmentLayoutPadding[3] __attribute__((unused));
    static u_char* Menu_Setup_gemAttachmentAttachedGems;

    char textBuf[0x300];
    char* menuText[16];
    int rowTypes[8];
    int var_s2;
    int j;
    int i;
    int var_a2;
    int itemInfo;
    Items_InventoryShield* shield;
    Items_InventoryWeapon* weapon;
    Menu_Framework_MenuRow* menuItem;

    if (arg0 != 0) {
        Menu_InitInformationTextBox(1);
        Menu_Setup_gemAttachmentIsShield = arg0 - 1;
        Menu_Setup_gemAttachmentState = 0;
        return 0;
    }

    switch (Menu_Setup_gemAttachmentState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Setup_SetTitleRow(0, Menu_Framework_menu12Text[Menu_Setup_gemAttachmentIsShield + 32]);
            Menu_Setup_gemAttachmentState = 1;
        }
        break;
    case 1:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        i = 0;

        for (j = 0; j < 8; ++j) {
            char* new_var = textBuf;
            itemInfo = 0;

            if (Menu_Setup_gemAttachmentIsShield != 0) {
                var_s2 = Audio_Seq_unusedInventoryIndices.shields[j];
                if (var_s2 != 0) {
                    shield = &Audio_Seq_unusedInventory.shields[var_s2 - 1];
                    Menu_Framework_FormatInventoryShieldRow(
                        shield, &menuText[i * 2], &rowTypes[i], textBuf + i * 96);
                    itemInfo = shield->base.gemSlots;
                }
            } else {
                var_s2 = Audio_Seq_unusedInventoryIndices.weapons[j];
                if (var_s2 != 0) {
                    weapon = &Audio_Seq_unusedInventory.weapons[var_s2 - 1];
                    Menu_Framework_FormatInventoryWeaponRow(
                        weapon, &menuText[i * 2], &rowTypes[i], textBuf + i * 96);
                    itemInfo = Audio_Seq_unusedInventory.grips[weapon->grip - 1].gemSlots;
                }
            }

            if (var_s2 != 0) {
                Battle_stringContext.integers[0] = itemInfo;
                Render_FormatEncodedText(textBuf + i * 96,
                    (char*)&Menu_Framework_menu12Text[Menu_Framework_menu12Text
                            [Menu_Setup_gemAttachmentIsShield * 2
                                + (itemInfo == 0 ? Text_Menu_MENU12_BIN_Index_weaponNoGemSlots
                                                 : Text_Menu_MENU12_BIN_Index_weaponGemSlots)]]);
                menuText[i * 2 + 1] = new_var + i * 96;
                rowTypes[i] |= itemInfo == 0;
                ++i;
                Menu_Setup_availableItems[i] = var_s2;
            }
        }

        Menu_Framework_CreateMenuRows(i, (Menu_Setup_gemAttachmentIsShield + 55) | 0x19100, menuText, rowTypes);
        Menu_Setup_gemAttachmentState = 2;
        break;
    case 2:
        Menu_Setup_gemAttachmentSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Setup_gemAttachmentSelectedRow != 0) {
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            if (Menu_Setup_gemAttachmentSelectedRow < 0) {
                return Menu_Setup_gemAttachmentSelectedRow;
            }
            menuItem = Menu_Framework_MenuItem(0);
            menuItem->state = 2;
            menuItem->w = 164;
            menuItem->targetPosition0 = 16;
            menuItem = Menu_Framework_MenuItem(Menu_Setup_gemAttachmentSelectedRow + 9);
            menuItem->state = 4;
            menuItem->targetPosition0 = 155;
            menuItem->targetPosition1 = 18;
            Menu_Setup_gemAttachmentState = 3;
        }
        break;
    case 3:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(0);

            var_a2 = 24;
            if (Menu_Setup_gemAttachmentIsShield != 0) {
                var_a2 = 27;
            }
            menuItem->rowIcon = var_a2;

            Menu_Setup_MoveMenuRow(0, 32);
            Menu_Setup_MoveMenuRow(Menu_Setup_gemAttachmentSelectedRow + 9, 10);

            var_s2 = Menu_Setup_availableItems[Menu_Setup_gemAttachmentSelectedRow];
            Menu_Setup_combiningItem = var_s2 - 1;

            if (Menu_Setup_gemAttachmentIsShield != 0) {
                Menu_Framework_SetShieldStats(var_s2);
                shield = &Audio_Seq_unusedInventory.shields[var_s2 - 1];
                Menu_Setup_gemAttachmentGemSlots = shield->base.gemSlots;
                Menu_Setup_gemAttachmentAttachedGems = shield->gems;
            } else {
                Menu_Framework_SetWeaponStats(var_s2);
                weapon = &Audio_Seq_unusedInventory.weapons[var_s2 - 1];
                Menu_Setup_gemAttachmentGemSlots = Audio_Seq_unusedInventory.grips[weapon->grip - 1].gemSlots;
                Menu_Setup_gemAttachmentAttachedGems = weapon->gems;
            }
            Menu_Framework_UpdateEquipmentStatPages(7);
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Framework_UpdatePrimaryStatPanel(1);

            *(int*)Menu_Setup_stagedGems = 0;
            for (itemInfo = 0; itemInfo < Menu_Setup_gemAttachmentGemSlots; ++itemInfo) {
                Menu_Setup_stagedGems[itemInfo] = Menu_Setup_gemAttachmentAttachedGems[itemInfo];
            }
            Menu_Setup_gemAttachmentAppearStep = 0;
            Menu_Setup_selectedGemSlot = 0;
            Menu_Setup_gemAttachmentState = 4;
            *(int*)Menu_Setup_gemAttachmentTargetGems = *(int*)Menu_Setup_stagedGems;
        }
        break;
    case 4:
        if (Menu_Setup_gemAttachmentAppearStep < 5) {
            ++Menu_Setup_gemAttachmentAppearStep;
            if (Menu_Setup_gemAttachmentIsShield != 0) {
                menuItem =
                    Menu_Framework_CreateShieldGemRow(Menu_Setup_gemAttachmentAppearStep, Menu_Setup_combiningItem + 1, 1);
            } else {
                menuItem =
                    Menu_Framework_CreateWeaponComponentRow(Menu_Setup_gemAttachmentAppearStep, Menu_Setup_combiningItem + 1, 1);
            }
            if (menuItem != NULL) {
                if (menuItem->unselectable != 0) {
                    Menu_Setup_CreateEmptyGemSlotRow(Menu_Setup_gemAttachmentAppearStep - ((1 - Menu_Setup_gemAttachmentIsShield) * 2), Menu_Setup_gemAttachmentAppearStep);
                    break;
                }
            }
        } else {
            Menu_Setup_gemAttachmentState = 5;
        }
        break;
    case 5:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            if (!(Input_buttonsPressed.allButtons & PADRup)) {
                Menu_Setup_gemAttachmentOkBlinkPhase += 16;
                Menu_Setup_DrawConfirmationLabel(Menu_Setup_gemAttachmentGemSlots + ((1 - Menu_Setup_gemAttachmentIsShield) * 2), Menu_Setup_gemAttachmentOkBlinkPhase);
                if (Input_buttonsPressed.allButtons & PADRdown) {
                    Menu_Setup_RestoreGemsAndLeaveSelection(Menu_Setup_gemAttachmentTargetGems, Menu_Setup_gemAttachmentIsShield);
                    Menu_Setup_gemAttachmentState = 0;
                } else if (Input_buttonsPressed.allButtons & PADstart) {
                    int new_var2 = 11;
                    Menu_PlaySelectSound();
                    Menu_Framework_MenuItem(((1 - Menu_Setup_gemAttachmentIsShield) * 2) + (Menu_Setup_selectedGemSlot + new_var2))
                        ->selected = 0;
                    Menu_Setup_gemAttachmentState = 7;
                    Menu_Setup_selectedGemSlot = Menu_Setup_gemAttachmentGemSlots;
                } else {
                    itemInfo = Menu_Setup_selectedGemSlot;
                    if (Input_buttonsPressed.allButtons & PADRright) {
                        Menu_PlaySelectSound();
                        if (itemInfo == Menu_Setup_gemAttachmentGemSlots) {
                            Menu_Setup_gemAttachmentState = 7;
                        } else {
                            int new_var3;
                            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                            new_var3 = ((1 - Menu_Setup_gemAttachmentIsShield) * 2) + 1;
                            Menu_Setup_SelectGem((itemInfo + new_var3) | ((itemInfo + 1) * 8)
                                       | (*(int*)Menu_Setup_stagedGems << 8));
                            Menu_Setup_gemAttachmentState = 6;
                        }
                    } else {
                        if (Input_buttonRepeat & 0x1000) {
                            if (itemInfo == 0) {
                                itemInfo = Menu_Setup_gemAttachmentGemSlots;
                            } else {
                                --itemInfo;
                            }
                        }
                        if (Input_buttonRepeat & 0x4000) {
                            if (itemInfo == Menu_Setup_gemAttachmentGemSlots) {
                                itemInfo = 0;
                            } else {
                                ++itemInfo;
                            }
                        }
                        if (itemInfo != Menu_Setup_selectedGemSlot) {
                            Menu_PlayChangeSound();
                            Menu_Setup_selectedGemSlot = itemInfo;
                        }

                        for (i = 0; i < Menu_Setup_gemAttachmentGemSlots; ++i) {
                            Menu_Framework_MenuItem(11 + ((1 - Menu_Setup_gemAttachmentIsShield) * 2) + i)
                                ->selected = i == Menu_Setup_selectedGemSlot;
                            Menu_Framework_rowTypeBuf[i] = Menu_Setup_stagedGems[i] == 0;
                        }

                        Menu_Framework_rowTypeBuf[i] = 1;
                        Menu_Framework_UpdateInformationVisibility(itemInfo, 16);
                        menuText[1] = (char*)&Menu_Framework_menu12Text
                            [Text_Menu_MENU12_BIN_Offset_selectGem];
                        if (itemInfo == Menu_Setup_gemAttachmentGemSlots) {
                            menuText[1] = (char*)&Menu_Framework_menu12Text
                                [Text_Menu_MENU12_BIN_Offset_pressOk];
                        } else {
                            i = *(itemInfo + Menu_Setup_stagedGems);
                            if (i != 0) {
                                Menu_Framework_FormatGemRow(&Audio_Seq_unusedInventory.gems[i - 1],
                                    menuText, rowTypes, &textBuf);
                            }
                        }
                        Menu_Framework_SetInformationMessage(menuText[1]);
                        i = 26;
                        i = ((((1 - Menu_Setup_gemAttachmentIsShield) << 5) + ((itemInfo * 16) + i)) << 0x10)
                          | 155;
                        if (itemInfo == Menu_Setup_gemAttachmentGemSlots) {
                            i += 0xFFFC0000;
                        }
                        Menu_Setup_gemAttachmentCursorAnimation = Menu_Framework_DrawCursor(Menu_Setup_gemAttachmentCursorAnimation, i);
                    }
                }
            } else {
                return Menu_Setup_RestoreGemsAndLeaveSelection(Menu_Setup_gemAttachmentTargetGems, Menu_Setup_gemAttachmentIsShield);
            }
        } else {
            Menu_Setup_gemAttachmentOkBlinkPhase = 0;
        }
        break;
    case 6:
        itemInfo = Menu_Setup_SelectGem(0);
        if (itemInfo != 0) {
            if (itemInfo != -2) {
                var_s2 = Menu_Setup_combiningItem + 1;
                weapon = &Audio_Seq_unusedInventory.weapons[var_s2 - 1];
                shield = &Audio_Seq_unusedInventory.shields[var_s2 - 1];
                Menu_Setup_gemAttachmentAttachedGems = Menu_Setup_gemAttachmentIsShield != 0 ? shield->gems : weapon->gems;
                if (itemInfo > 0) {
                    i = Menu_Setup_stagedGems[Menu_Setup_selectedGemSlot];
                    if (itemInfo == i) {
                        Audio_Seq_unusedInventory.gems[itemInfo - 1].setItemIndex = 0;
                        Menu_Setup_gemAttachmentAttachedGems[Menu_Setup_selectedGemSlot] = 0;
                        Menu_Setup_stagedGems[Menu_Setup_selectedGemSlot] = 0;
                    } else {
                        if (i != 0) {
                            Audio_Seq_unusedInventory.gems[i - 1].setItemIndex = 0;
                        }
                        Audio_Seq_unusedInventory.gems[itemInfo - 1].setItemIndex =
                            var_s2 + (Menu_Setup_gemAttachmentIsShield << 7);
                        Menu_Setup_gemAttachmentAttachedGems[Menu_Setup_selectedGemSlot] = itemInfo;
                        Menu_Setup_stagedGems[Menu_Setup_selectedGemSlot] = itemInfo;
                        ++Menu_Setup_selectedGemSlot;
                    }
                    if (Menu_Setup_gemAttachmentIsShield != 0) {
                        if (shield->isEquipped != 0) {
                            Items_EquipAshleyShield(shield);
                        }
                    } else if (weapon->isEquipped != 0) {
                        Items_EquipAshleyWeapon(weapon);
                    }
                }
                Menu_Framework_SetEnabledStatPages(7);
                Menu_Framework_UpdateDpPpBars(11);
                if (Menu_Setup_gemAttachmentIsShield != 0) {
                    Menu_Framework_SetShieldStats(var_s2);
                    Menu_Framework_FormatInventoryShieldRow(
                        shield, menuText, rowTypes, textBuf);
                } else {
                    Menu_Framework_SetWeaponStats(var_s2);
                    Menu_Framework_FormatInventoryWeaponRow(
                        weapon, menuText, rowTypes, textBuf);
                }
                menuItem = Menu_Framework_SetMenuItem(10, 320, 18, 137, 0, menuText[0]);
                menuItem->state = 2;
                menuItem->targetPosition0 = 155;
                menuItem->selected = 1;
                i = rowTypes[0];
                menuItem->rowIcon = i >> 0x1A;
                menuItem->material = (i >> 0x10) & 7;
                Menu_Setup_gemAttachmentAppearStep = 0;
                Menu_Setup_gemAttachmentState = 4;
            } else {
                return Menu_Setup_RestoreGemsAndLeaveSelection(Menu_Setup_gemAttachmentTargetGems, Menu_Setup_gemAttachmentIsShield);
            }
        }
        break;
    case 7:
        Menu_Framework_UpdatePrimaryStatPanel(2);
        Menu_Setup_UpdateThreeChoicePrompt(3);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_attachmentConfirm]);
        Menu_Setup_gemAttachmentState = 8;
        break;
    case 8:
        itemInfo = Menu_Setup_UpdateThreeChoicePrompt(0);
        if (itemInfo == 0) {
            break;
        }
        switch (itemInfo) {
        case 1:
            Audio_PlaySfxCentered(0x7E, 0x18);
            Menu_Framework_InitializeInformationBox();
            Menu_Framework_UpdateEquipmentStatPages(-1);
            Menu_Framework_UpdateDpPpBars(4);
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Menu_Framework_FlyOutRowLeft(0);
            return 1;
        case 2:
            Menu_PlayLeaveSound();
            Menu_Framework_UpdatePrimaryStatPanel(1);
            Menu_Setup_gemAttachmentState = 5;
            break;
        case 3:
            return Menu_Setup_RestoreGemsAndLeaveSelection(Menu_Setup_gemAttachmentTargetGems, Menu_Setup_gemAttachmentIsShield);
            break;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _attachGemsTopMenu (src/MENU/MENUC.PRG/168.c:2016)
int Menu_Setup_UpdateGemCategoryMenu(int arg0)
{
    static char Menu_Setup_gemCategoryMenuState;
    static char Menu_Setup_gemCategoryMenuNoForceCursorMemory;
    static char Menu_Setup_gemCategoryMenuLayoutPadding[2] __attribute__((unused));

    char* text[4];
    int rowTypes[4];
    int i;

    if (arg0 != 0) {
        Menu_Setup_gemCategoryMenuNoForceCursorMemory = 1;
        Menu_Framework_FlyOutRowsAndHoistSelection(1, 1);
        Menu_Setup_gemCategoryMenuState = 0;
        return 0;
    }

    switch (Menu_Setup_gemCategoryMenuState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }
        for (i = 0; i < 4; ++i) {
            text[i] = (char*)&Menu_Framework_menu12Text
                [Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Index_weapons + i]];
            rowTypes[i] = 0;
        }
        if (Items_Inventory_CountItems(0, NULL) == 0) {
            rowTypes[0] = 1;
            text[1] = (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_noItems];
        }
        if (Items_Inventory_CountItems(3, NULL) == 0) {
            rowTypes[1] = 1;
            text[3] = (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_noItems];
        }
        i = Input_settings.cursorMemory;
        if (Menu_Setup_gemCategoryMenuNoForceCursorMemory == 0) {
            Input_settings.cursorMemory = 1;
        }
        Menu_Setup_gemCategoryMenuNoForceCursorMemory = 0;
        Menu_Framework_CreateMenuRows(2, 566, text, rowTypes);
        Input_settings.cursorMemory = i;
        Menu_Setup_gemCategoryMenuState = 1;
        break;
    case 1:
        i = Menu_Framework_GetSelectedRow() + 1;
        if (i != 0) {
            if (i < 0) {
                Menu_Framework_ClearMenuExcept(0);
                return i;
            }
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Menu_Setup_UpdateGemAttachment(i);
            Menu_Setup_gemCategoryMenuState = 2;
        }
        break;
    case 2:
        i = Menu_Setup_UpdateGemAttachment(0);
        if (i != 0) {
            if (i != -1) {
                return i;
            }
            Menu_Setup_gemCategoryMenuState = 3;
        }
        break;
    case 3:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_InitializeInformationBox();
            Menu_Setup_SetTitleRow(0, Text_Menu_MENU12_BIN_Offset_setup);
            Menu_Setup_SetTitleRow(11, Text_Menu_MENU12_BIN_Offset_attachGems);
            Menu_Setup_gemCategoryMenuState = 0;
        }
        break;
    }
    return 0;
}

// MENUC.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Setup_Assembly_ResetMenucStatics(void)
{
    {
        static const u_short initial[] = { 0, 0xE, 0xF0, 0x100, 0x200 };
        memcpy((void*)&Menu_Setup_isGripCompatibleValidGripFlags, &initial, sizeof Menu_Setup_isGripCompatibleValidGripFlags);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_assemblyCursorAnimation, &initial, sizeof Menu_Setup_assemblyCursorAnimation);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_assemblyOkBlinkPhase, &initial, sizeof Menu_Setup_assemblyOkBlinkPhase);
    }
    {
        static const int initial[] = { vs_getXY(141, 10), vs_getXY(148, 26),
        vs_getXY(148, 42), vs_getXY(155, 58), vs_getXY(155, 74), vs_getXY(155, 90),
        vs_getXY(155, 106) };
        memcpy((void*)&Menu_Setup_assemblyCursorPositions, &initial, sizeof Menu_Setup_assemblyCursorPositions);
    }
    {
        static const char initial[4];
        memcpy((void*)&Menu_Setup_assemblyGemInfo, &initial, sizeof Menu_Setup_assemblyGemInfo);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_gemAttachmentCursorAnimation, &initial, sizeof Menu_Setup_gemAttachmentCursorAnimation);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_gemAttachmentOkBlinkPhase, &initial, sizeof Menu_Setup_gemAttachmentOkBlinkPhase);
    }
    {
        static const char initial[4];
        memcpy((void*)&Menu_Setup_stagedGems, &initial, sizeof Menu_Setup_stagedGems);
    }
    {
        static const char initial[4];
        memcpy((void*)&Menu_Setup_gemAttachmentTargetGems, &initial, sizeof Menu_Setup_gemAttachmentTargetGems);
    }
}

