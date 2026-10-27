// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Setup/Assembly.h"
#include "Menu/Setup/Combination.h"
#include "Menu/Setup/SetupScreen.h"
#include "Menu/Sounds.h"
#include "State/Settings.h"

// decomp: local static of _combineBladeMenu, at file scope so the module reset reaches it
static char Menu_Setup_bladeCombinationCursorAnimation = 0;

// decomp: local static of _combineShieldMenu, at file scope so the module reset reaches it
static char Menu_Setup_shieldCombinationCursorAnimation = 0;

// decomp: local static of _combineArmorMenu, at file scope so the module reset reaches it
static char Menu_Setup_armorCombinationCursorAnimation = 0;

// decomp: _1 (_1@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_layoutPadding3[2];

// decomp: _2 (_2@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_layoutPadding5[2];

// decomp: _armorBuf (_armorBuf@src/MENU/MENUC.PRG/168.c)
Items_InventoryArmor Menu_Setup_armorBuf;

// decomp: _bladeBuf (_bladeBuf@src/MENU/MENUC.PRG/168.c)
Items_InventoryBlade Menu_Setup_bladeBuf;

// decomp: _bladeCombinationInitData (_bladeCombinationInitData@src/MENU/MENUC.PRG/168.c)
void* Menu_Setup_bladeCombinationInitData;

// decomp: _combinationInitData (_combinationInitData@src/MENU/MENUC.PRG/168.c)
Menu_Setup_CombinedArmorRecord* Menu_Setup_combinationInitData;

// decomp: _combinationResults (_combinationResults@src/MENU/MENUC.PRG/168.c)
char* Menu_Setup_combinationResults;

// decomp: _combiningArmorIds (_combiningArmorIds@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_combiningArmorIds[2];

// decomp: _combiningBladeIds (_combiningBladeIds@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_combiningBladeIds[2];

// decomp: _currentWorkshop (_currentWorkshop@src/MENU/MENUC.PRG/168.c)
u_char Menu_Setup_currentWorkshop;

// decomp: local static of _setClassAffinities, at file scope so the module reset reaches it
static short Menu_Setup_combineClassOrAffinityValuesDifferentMaterialStatFactors[] = { 3, 5, 7 };

// decomp: local static of _setTypeValues, at file scope so the module reset reaches it
static short Menu_Setup_combinePhysicalTypeValuesDifferentMaterialTypeFactors[] = { 4, 6 };

// decomp: _itemsToCombine (_itemsToCombine@src/MENU/MENUC.PRG/168.c)
u_char Menu_Setup_itemsToCombine[16];

// decomp: _materialResults (_materialResults@src/MENU/MENUC.PRG/168.c)
char* Menu_Setup_materialResults;

// decomp: local static of _setClassAffinities, at file scope so the module reset reaches it
static short Menu_Setup_combineClassOrAffinityValuesSameMaterialStatFactors[] = { 4, 6, 8 };

// decomp: local static of _setTypeValues, at file scope so the module reset reaches it
static short Menu_Setup_combinePhysicalTypeValuesSameMaterialTypeFactors[] = { 6, 8 };

// decomp: _shieldBuf (_shieldBuf@src/MENU/MENUC.PRG/168.c)
Items_InventoryShield Menu_Setup_shieldBuf;

// decomp: _shieldCombinationInitData (_shieldCombinationInitData@src/MENU/MENUC.PRG/168.c)
Menu_Setup_CombinedArmorRecord* Menu_Setup_shieldCombinationInitData;

// decomp: _sydData (_sydData@src/MENU/MENUC.PRG/168.c)
void* Menu_Setup_sydData = NULL;

// decomp: local static of _loadSyd, at file scope so the module reset reaches it
static int Menu_Setup_loadCombinationRecipesSydLbas[] = { VS_BLADE_SYD_LBA, VS_SHIELD_SYD_LBA, VS_ARMOR_SYD_LBA };

// decomp: local static of _loadSyd, at file scope so the module reset reaches it
static int Menu_Setup_loadCombinationRecipesSydSizes[] = { VS_BLADE_SYD_SIZE, VS_SHIELD_SYD_SIZE, VS_ARMOR_SYD_SIZE };

// decomp: _workshopMaterials (_workshopMaterials@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_workshopMaterials[] = { 0, 0xE, 0x18, 0x38, 0xC0, 0x3E, 0xFE };

// decomp: local static of _combineTopMenu, at file scope so the module reset reaches it
static int (*Menu_Setup_combineMenus[])(
        int) = { Menu_Setup_UpdateBladeCombination, Menu_Setup_UpdateShieldCombination, Menu_Setup_UpdateArmorCombination };

// decomp: MENUC.PRG _loadSyd (src/MENU/MENUC.PRG/168.c:2658)
int Menu_Setup_LoadCombinationRecipes(int id)
{

    static Content_DiscQueueSlot* Menu_Setup_loadCombinationRecipesSydCdQueueSlot;
    static char Menu_Setup_loadCombinationRecipesSydFileLoading;
    static char Menu_Setup_loadCombinationRecipesLayoutPadding[3] __attribute__((unused));

    Content_DiscFile cdFile;

    if (id != 0) {
        --id;
        Menu_Setup_sydData = Core_Memory_AllocateFromHighEnd(Menu_Setup_loadCombinationRecipesSydSizes[id]);
        cdFile.lba = Menu_Setup_loadCombinationRecipesSydLbas[id];
        cdFile.size = Menu_Setup_loadCombinationRecipesSydSizes[id];
        Menu_Setup_loadCombinationRecipesSydCdQueueSlot = Content_AllocateDiscRequest(&cdFile);
        Content_EnqueueDiscRequest(Menu_Setup_loadCombinationRecipesSydCdQueueSlot, Menu_Setup_sydData);
        Menu_Setup_loadCombinationRecipesSydFileLoading = 1;
        return 0;
    }
    if (Menu_Setup_loadCombinationRecipesSydFileLoading == 0) {
        return 1;
    }

    if (Menu_Setup_loadCombinationRecipesSydCdQueueSlot->state == 4) {
        Content_ReleaseDiscRequest(Menu_Setup_loadCombinationRecipesSydCdQueueSlot);
        Menu_Setup_loadCombinationRecipesSydFileLoading = 0;
        return 1;
    }
    return 0;
}

// decomp: MENUC.PRG _setItemRow (src/MENU/MENUC.PRG/168.c:2696)
Menu_Framework_MenuRow* Menu_Setup_SetCombinationResultRow(int itemType, int index)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(0, 155, 18, 165, 0,
        (index == 0
                ? (char*)&Menu_Framework_menu12Text
                      [Menu_Framework_menu12Text[itemType + Text_Menu_MENU12_BIN_Index_newBlade]]
                : Menu_Framework_itemNames[index]));
    menuItem->selected = 1;
    return menuItem;
}

// decomp: MENUC.PRG _initItemRow (src/MENU/MENUC.PRG/168.c:2710)
Menu_Framework_MenuRow* Menu_Setup_AnimateCombinationResult(int itemType, int index)
{
    Menu_Framework_MenuRow* menuItem;

    menuItem = Menu_Setup_SetCombinationResultRow(itemType, index);
    menuItem->state = 2;
    menuItem->targetPosition0 = 155;
    menuItem->x = 320;

    return menuItem;
}

// decomp: MENUC.PRG _setCombineBladeUi (src/MENU/MENUC.PRG/168.c:2735)
void Menu_Setup_PreviewBladeCombination(int preview_selection)
{
    int i;
    int ingredient_presence_mask;
    u_int selected_ingredient_index;
    int second_blade_id;
    Items_InventoryBlade* blade;

    i = Menu_Setup_combiningBladeIds[0];
    second_blade_id = Menu_Setup_combiningBladeIds[1];

    ingredient_presence_mask = i != 0;
    if (second_blade_id != 0) {
        ingredient_presence_mask += 2;
    }

    switch (ingredient_presence_mask) {
    case 0:
        Core_Memory_ZeroWordsBackward(&Menu_Setup_bladeBuf, sizeof Menu_Setup_bladeBuf);
        break;

    case 1:
    case 2:
        Core_Memory_CopyHalfwords(
            &Menu_Setup_bladeBuf, &Audio_Seq_unusedInventory.blades[i + second_blade_id - 1], sizeof Menu_Setup_bladeBuf);
        break;

    case 3:
        Core_Memory_ZeroWordsBackward(&Menu_Setup_bladeBuf, sizeof Menu_Setup_bladeBuf);
        Menu_Setup_BuildCombinedBlade(&Audio_Seq_unusedInventory.blades[i - 1],
            &Audio_Seq_unusedInventory.blades[second_blade_id - 1], &Menu_Setup_bladeBuf, Menu_Setup_sydData);
        break;
    }

    blade = &Menu_Setup_bladeBuf;
    blade->assembledWeaponIndex = 0;

    Menu_Framework_ResetEquipmentStats();
    Menu_Framework_SetRangeAndRisk(0, 0, 0, 1);

    selected_ingredient_index = preview_selection - 1;

    if (selected_ingredient_index >= 2) {
        if (Menu_Setup_bladeBuf.id != 0) {
            for (i = 0; i < 16; ++i) {
                Menu_Framework_equipmentStats[i] = blade->classes[i & 7];
                Menu_Framework_equipmentStats[16 + i] = blade->affinities[i & 7];
            }
            Menu_Framework_SetDpPp(
                blade->currentDp, blade->maxDp, blade->currentPp, blade->maxPp);
            Menu_Framework_SetPrimaryStats(
                blade->strength, blade->intelligence, blade->agility, 1);
            Menu_Framework_SetRangeAndRisk(blade->range.range, blade->cost, 0, 1);
            Menu_Framework_equipmentSubtype = 2;
        }
    } else if (ingredient_presence_mask & preview_selection) {
        Menu_Framework_SetBladeStats(Menu_Setup_combiningBladeIds[selected_ingredient_index]);
    }
}

// decomp: MENUC.PRG _selectBlade (src/MENU/MENUC.PRG/168.c:2801)
int Menu_Setup_SelectCombinationBlade(int ingredient_slot)
{
    static int Menu_Setup_combinationBladeSelectedRow;
    static char Menu_Setup_combinationBladeState;
    static char Menu_Setup_combinationBladeSlot;

    int count;
    int i;
    char* textBuf;
    char* menuText[2 * 17];
    int parent_weapon_id;
    int itemId;
    Items_InventoryBlade* blade = NULL;

    if (ingredient_slot != 0) {
        Menu_Setup_combinationBladeSlot = ingredient_slot;
        Menu_Setup_AnimateItemHeader(ingredient_slot + 10);
        Menu_Setup_combinationBladeState = 0;
        return 0;
    }

    itemId = Menu_Setup_combiningBladeIds[Menu_Setup_combinationBladeSlot - 1];
    if (itemId != 0) {
        blade = &Audio_Seq_unusedInventory.blades[itemId - 1];
    }

    switch (Menu_Setup_combinationBladeState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        textBuf = Core_Memory_AllocateFromHighEnd(0x72C);


        if (blade == NULL) {
            menuText[0] = (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_blade];
            menuText[1] =
                (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_selectBladeToCombine];
            *Menu_Framework_rowTypeBuf = 1;
        } else {
            Menu_Framework_FormatBladeRow(blade, menuText, Menu_Framework_rowTypeBuf, textBuf);
        }

        Menu_Setup_availableItems[0] = itemId;
        count = 1;

        for (i = 0; i < 16; ++i) {

            itemId = Audio_Seq_unusedInventoryIndices.blades[i];

            if ((itemId != 0) && (itemId != Menu_Setup_combiningBladeIds[0])
                && (itemId != Menu_Setup_combiningBladeIds[1])) {

                int available_blade_id = itemId;
                blade = &Audio_Seq_unusedInventory.blades[itemId - 1];

                Menu_Framework_FormatBladeRow(blade, menuText + count * 2,
                    &Menu_Framework_rowTypeBuf[count], textBuf + count * 96);

                if (!((Menu_Setup_workshopMaterials[Menu_Setup_currentWorkshop] >> blade->material) & 1)) {
                    menuText[count * 2 + 1] = (char*)&Menu_Framework_menu12Text
                        [Text_Menu_MENU12_BIN_Offset_invalidBladeMaterial];
                    Menu_Framework_rowTypeBuf[count] |= 1;
                }

                parent_weapon_id = blade->assembledWeaponIndex;

                if (parent_weapon_id != 0) {
                    Menu_Framework_rowTypeBuf[count] |=
                        (Audio_Seq_unusedInventory.weapons[parent_weapon_id - 1].isEquipped == 0 ? 0xCC00
                                                                                : 0xCA00);
                }

                Menu_Setup_availableItems[count] = available_blade_id;
                ++count;
            }
        }

        Menu_Setup_BuildItemList(count, menuText, Menu_Framework_rowTypeBuf);
        Core_Memory_FreeFromHighEnd(textBuf);
        Menu_Setup_combinationBladeState = 1;
        break;
    case 1:
        itemId = Menu_Setup_availableItems[Menu_Setup_itemsListWindow + Menu_Setup_itemsListSelection];
        if (itemId != 0) {
            Menu_Framework_SetBladeStats(itemId);
        } else {
            Menu_Framework_ResetEquipmentStats();
            Menu_Framework_SetRangeAndRisk(0, 0, 0, 1);
        }
        Menu_Framework_UpdateInformationVisibility(
            Menu_Setup_itemsListSelection + Menu_Setup_itemsListWindow, Menu_Setup_itemsListState);
        Menu_Setup_UpdateItemList(2);
        Menu_Setup_combinationBladeSelectedRow = Menu_Setup_GetSelectedItemRow() + 1;
        if (Menu_Setup_combinationBladeSelectedRow != 0) {
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            if (Menu_Setup_combinationBladeSelectedRow == -2) {
                return -2;
            }
            if (Menu_Setup_combinationBladeSelectedRow > 0) {
                Menu_Setup_combinationBladeSelectedRow = Menu_Setup_availableItems[Menu_Setup_combinationBladeSelectedRow - 1];
            }
            Menu_Setup_combinationBladeState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Setup_combinationBladeSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _setBladeMenuItem (src/MENU/MENUC.PRG/168.c:2919)
void Menu_Setup_SetCombinationBladeRow(
    Menu_Framework_MenuRow* menuItem, Items_InventoryBlade* blade)
{
    int assembledWeapon;

    menuItem->rowIcon = blade->category;
    menuItem->material = blade->material;
    assembledWeapon = blade->assembledWeaponIndex;
    if (assembledWeapon != 0) {
        menuItem->outsetIcon =
            Audio_Seq_unusedInventory.weapons[assembledWeapon - 1].isEquipped == 0 ? 2 : 1;
    }
}

// decomp: MENUC.PRG _combineBladeMenu (src/MENU/MENUC.PRG/168.c:2939)
int Menu_Setup_UpdateBladeCombination(int initialize_blade_combination)
{

    static char Menu_Setup_bladeCombinationState;
    static char Menu_Setup_bladeCombinationRow;
    static char Menu_Setup_bladeCombinationOkBlinkPhase;
    static char Menu_Setup_bladeCombinationLayoutPadding[3] __attribute__((unused));

    char* menuText[2];
    int rowType;
    int itemId;
    int var_s1;
    int temp_v0_2;
    int i;
    char* combination_help_text;
    int i_2;
    Items_InventoryBlade* blade;
    Menu_Framework_MenuRow* menuItem;

    if (initialize_blade_combination != 0) {
        Menu_Setup_RecordSlotSelection(5);

        *(u_short*)&Menu_Setup_combiningBladeIds = 0;

        Menu_Setup_PreviewBladeCombination(0);

        Menu_Setup_bladeCombinationRow = 1;
        Menu_Setup_bladeCombinationState = 0;
        return 0;
    }

    var_s1 = 0;

    if (Menu_Setup_combiningBladeIds[0] != 0) {
        var_s1 = Menu_Setup_combiningBladeIds[1] > 0;
    }

    Menu_Setup_bladeCombinationOkBlinkPhase += 16;

    if (Menu_Setup_bladeCombinationState != 2) {
        Menu_Setup_bladeCombinationOkBlinkPhase = 0;
    }

    switch (Menu_Setup_bladeCombinationState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Menu_Setup_InitializeEquipmentHeader(Text_Menu_MENU12_BIN_Offset_combineBlade, 24);
        // Fallthrough

    case 1:
        var_s1 = Menu_Setup_bladeBuf.id;
        menuItem = Menu_Setup_AnimateCombinationResult(0, var_s1);

        if (var_s1 != 0) {
            Menu_Setup_SetCombinationBladeRow(menuItem, &Menu_Setup_bladeBuf);
        }

        for (i = 0; i < 2; ++i) {

            var_s1 = Menu_Setup_combiningBladeIds[i];

            if (var_s1 != 0) {
                blade = &Audio_Seq_unusedInventory.blades[var_s1 - 1];
            }

            menuItem = Menu_Framework_SetMenuItem(i + 11, 334, 34 + i * 16, 151, 0,
                var_s1 == 0 ? (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_blade]
                            : Menu_Framework_itemNames[blade->id]);
            menuItem->state = 2;
            menuItem->targetPosition0 = 169;

            if (var_s1 == 0) {
                menuItem->fontColor = 1;
            } else {
                Menu_Setup_SetCombinationBladeRow(menuItem, blade);
            }
        }

        Menu_Setup_bladeCombinationState = 2;
        break;

    case 2:
        if (Menu_Framework_AreMenuRowsIdle()) {

            if (Input_buttonsPressed.allButtons & PADRup) {
                return Menu_Setup_LeaveItemSelection(1);
            }

            if (var_s1 != 0) {
                Menu_Setup_DrawConfirmationLabel(2, Menu_Setup_bladeCombinationOkBlinkPhase);
            }

            if (Input_buttonsPressed.allButtons & PADRdown) {

                temp_v0_2 = Menu_Setup_PopSlotSelection(5);

                if (temp_v0_2 != 0) {

                    Menu_PlayLeaveSound();

                    Menu_Setup_bladeCombinationRow = temp_v0_2;
                    Menu_Setup_combiningBladeIds[temp_v0_2 - 1] = 0;

                    Menu_Setup_PreviewBladeCombination(temp_v0_2);

                    menuItem = Menu_Framework_SetMenuItem(temp_v0_2 + 10, 169,
                        (temp_v0_2 * 16) + 18, 151, 0,
                        (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_blade]);
                    menuItem->selected = 1;
                    menuItem->fontColor = 1;

                    var_s1 = Menu_Setup_bladeBuf.id;

                    menuItem = Menu_Setup_SetCombinationResultRow(0, var_s1);

                    if (var_s1 != 0) {
                        Menu_Setup_SetCombinationBladeRow(menuItem, &Menu_Setup_bladeBuf);
                    }

                    break;
                }

                return Menu_Setup_LeaveItemSelection(1);
            }

            if ((Input_buttonsPressed.allButtons & PADstart) && (var_s1 != 0)) {

                Menu_PlaySelectSound();
                Menu_Framework_MenuItem(Menu_Setup_bladeCombinationRow + 10)->selected = 0;

                Menu_Setup_bladeCombinationRow = 3;
                Menu_Setup_bladeCombinationState = 4;

                Menu_Setup_PreviewBladeCombination(3);
                return 0;
            }

            i_2 = Menu_Setup_bladeCombinationRow;

            if (Input_buttonsPressed.allButtons & PADRright) {
                if (i_2 == 0) {
                    Menu_PlayInvalidSound();
                } else {

                    Menu_PlaySelectSound();

                    if (i_2 == 3) {
                        Menu_Setup_bladeCombinationState = 4;
                    } else {
                        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                        Menu_Setup_SelectCombinationBlade(i_2);
                        Menu_Setup_bladeCombinationState = 3;
                    }

                    break;
                }
            }

            if (Input_buttonRepeat & PADLup) {
                if (i_2 == 0) {
                    i_2 = var_s1 + 2;
                } else {
                    --i_2;
                }
            }

            if (Input_buttonRepeat & PADLdown) {
                if (i_2 == (var_s1 + 2)) {
                    i_2 = 0;
                } else {
                    ++i_2;
                }
            }

            if (i_2 != Menu_Setup_bladeCombinationRow) {

                Menu_PlayChangeSound();

                Menu_Setup_bladeCombinationRow = i_2;

                Menu_Setup_PreviewBladeCombination(Menu_Setup_bladeCombinationRow);
            }

            Menu_Framework_rowTypeBuf[0] = Menu_Setup_bladeBuf.id < 1;

            for (var_s1 = 1; var_s1 < 3; ++var_s1) {

                Menu_Framework_MenuItem(var_s1 + 10)->selected = var_s1 == Menu_Setup_bladeCombinationRow;

                Menu_Framework_rowTypeBuf[var_s1] = Menu_Setup_combiningBladeIds[var_s1 - 1] == 0;
            }

            Menu_Framework_rowTypeBuf[3] = 1;

            Menu_Framework_UpdateInformationVisibility(i_2, 16);

            menuText[1] =
                (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_selectBladeToCombine];

            switch (i_2) {
            case 0:
                if (Menu_Setup_bladeBuf.id != 0) {
                    Menu_Framework_FormatBladeRow(
                        &Menu_Setup_bladeBuf, menuText, &rowType, Menu_Framework_stringBuf);
                }
                break;

            case 1:
            case 2:
                var_s1 = Menu_Setup_combiningBladeIds[i_2 - 1];
                if (var_s1 != 0) {
                    Menu_Framework_FormatBladeRow(&Audio_Seq_unusedInventory.blades[var_s1 - 1],
                        menuText, &rowType, Menu_Framework_stringBuf);
                }
                break;

            case 3:
                menuText[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_pressOk];
                break;
            }

            Menu_Framework_SetInformationMessage(menuText[1]);

            var_s1 = (((i_2 * 16) + 10) << 0x10) | 0x9B;

            if (i_2 == 0) {
                var_s1 -= 14;
            }

            if (i_2 == 3) {
                var_s1 += 0xFFFC0000;
            }

            Menu_Setup_bladeCombinationCursorAnimation = Menu_Framework_DrawCursor(Menu_Setup_bladeCombinationCursorAnimation, var_s1);
            break;
        }
        Menu_Setup_bladeCombinationOkBlinkPhase = 0;
        break;

    case 3:
        itemId = Menu_Setup_SelectCombinationBlade(0);

        if (itemId == 0) {
            break;
        }

        if (itemId == -2) {
            return Menu_Setup_LeaveItemSelection(1);
        }

        if (itemId > 0) {

            var_s1 = Menu_Setup_bladeCombinationRow - 1;

            if (itemId == Menu_Setup_combiningBladeIds[Menu_Setup_bladeCombinationRow - 1]) {

                Menu_Setup_combiningBladeIds[Menu_Setup_bladeCombinationRow - 1] = 0;

                Menu_Setup_PopSlotSelection(var_s1);

            } else {

                Menu_Setup_combiningBladeIds[Menu_Setup_bladeCombinationRow - 1] = itemId;

                Menu_Setup_RecordSlotSelection(var_s1);

                Menu_Setup_bladeCombinationRow = (Menu_Setup_bladeCombinationRow + 1);

                if (var_s1 == 1) {
                    if (Menu_Setup_combiningBladeIds[0] == 0) {
                        Menu_Setup_bladeCombinationRow = var_s1;
                    }
                }
            }

            Menu_Setup_PreviewBladeCombination(Menu_Setup_bladeCombinationRow);
        }

        Menu_Setup_bladeCombinationState = 1;
        break;

    case 4:
        Menu_Framework_UpdatePrimaryStatPanel(2);
        Menu_Setup_UpdateThreeChoicePrompt(2);

        var_s1 = 0;

        for (i = 0; i < 2; ++i) {
            var_s1 |=
                Audio_Seq_unusedInventory.blades[Menu_Setup_combiningBladeIds[i] - 1].assembledWeaponIndex;
        }

        if (var_s1 == 0) {
            combination_help_text =
                (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_attachmentConfirm];
        } else {
            combination_help_text =
                (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_cancelCombineConfirm];
        }

        Menu_Framework_SetInformationMessage(combination_help_text);

        Menu_Setup_bladeCombinationState = 5;
        break;

    case 5:
        temp_v0_2 = Menu_Setup_UpdateThreeChoicePrompt(0);

        if (temp_v0_2 == 0) {
            break;
        }

        switch (temp_v0_2) {
        case 1:
            Audio_PlaySfxCentered(0x7E, 0x18);
            Menu_Framework_InitializeInformationBox();
            Menu_Framework_UpdateEquipmentStatPages(-1);
            Menu_Framework_UpdateDpPpBars(4);
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Menu_Framework_FlyOutRowLeft(0);

            for (i = 0; i < 2; ++i) {

                blade = &Audio_Seq_unusedInventory.blades[Menu_Setup_combiningBladeIds[i] - 1];
                var_s1 = blade->assembledWeaponIndex;

                if (var_s1 != 0) {
                    if (Audio_Seq_unusedInventory.weapons[var_s1 - 1].isEquipped != 0) {
                        Items_Inventory_UnequipWeapon();
                        Menu_Setup_bladeCombinationState = 6;
                    }
                    Menu_Setup_DisassembleWeapon(var_s1 - 1);
                }

                Items_Inventory_ClearItemSlot(Items_ItemCategory_Blade, Menu_Setup_combiningBladeIds[i]);
            }

            itemId = Menu_Setup_combiningBladeIds[0];
            blade = &Audio_Seq_unusedInventory.blades[itemId - 1];

            Core_Memory_CopyHalfwords(blade, &Menu_Setup_bladeBuf, sizeof *blade);

            blade->index = itemId;

            Items_Inventory_RebuildDisplayIndices(1);

            return Menu_Setup_bladeCombinationState != 6;

        case 2:
            Menu_PlayLeaveSound();
            Menu_Framework_UpdatePrimaryStatPanel(1);

            Menu_Setup_bladeCombinationState = 2;
            break;

        case 3:
            return Menu_Setup_LeaveItemSelection(1);
        }
        break;

    case 6:
        if (Menu_Setup_ReloadAshleyEquipment(0) == 0) {
            return -1;
        }
        break;
    }

    return 0;
}

// decomp: MENUC.PRG _setShieldUi (src/MENU/MENUC.PRG/168.c:3317)
void Menu_Setup_SetShieldBaseStats(Items_InventoryShield* shield)
{
    int i;
    Items_InventoryArmor* base = &shield->base;

    Menu_Framework_ResetEquipmentStats();

    for (i = 0; i < 16; ++i) {
        Menu_Framework_equipmentStats[i] = base->classes[i & 7];
        Menu_Framework_equipmentStats[16 + i] = base->affinities[i & 7];
    }

    for (i = 0; i < 4; ++i) {
        Menu_Framework_equipmentStats[32 + i] = base->types[i];
    }

    Menu_Framework_SetDpPp(base->currentDp, base->maxDp, 0, 0);
    Menu_Framework_SetPrimaryStats(base->strength, base->intelligence, base->agility, 1);
    Menu_Framework_strIntAgi[4] = base->strength;
    Menu_Framework_strIntAgi[5] = base->intelligence;
    Menu_Framework_strIntAgi[6] = base->agility;
    Menu_Framework_equipmentSubtype = 8;
}

// decomp: MENUC.PRG _initCombineShields (src/MENU/MENUC.PRG/168.c:3353)
void Menu_Setup_PreviewShieldCombination(int preview_selection)
{
    u_int selected_ingredient_index;
    Items_InventoryShield* shield;

    int first = Menu_Setup_itemsToCombine[0];
    int second = Menu_Setup_itemsToCombine[1];

    int ingredient_presence_mask = first > 0;

    if (second != 0) {
        ingredient_presence_mask += 2;
    }

    switch (ingredient_presence_mask) {
    case 0:
        Core_Memory_ZeroWordsBackward(&Menu_Setup_shieldBuf, sizeof Menu_Setup_shieldBuf);
        break;

    case 1:
    case 2:
        Core_Memory_CopyHalfwords(&Menu_Setup_shieldBuf, &Audio_Seq_unusedInventory.shields[first + second - 1],
            sizeof Menu_Setup_shieldBuf);
        break;

    case 3:
        Core_Memory_ZeroWordsBackward(&Menu_Setup_shieldBuf, sizeof Menu_Setup_shieldBuf);
        Menu_Setup_BuildCombinedShield(&Audio_Seq_unusedInventory.shields[first - 1].base,
            &Audio_Seq_unusedInventory.shields[second - 1].base, &Menu_Setup_shieldBuf.base, Menu_Setup_sydData);
        break;
    }

    shield = &Menu_Setup_shieldBuf;
    shield->isEquipped = 0;

    Menu_Framework_ResetEquipmentStats();

    selected_ingredient_index = preview_selection - 1;

    if (selected_ingredient_index > 1) {
        if (shield->base.id != 0) {
            Menu_Setup_SetShieldBaseStats(shield);
        }
    } else if (ingredient_presence_mask & preview_selection) {
        Menu_Setup_SetShieldBaseStats(&Audio_Seq_unusedInventory.shields[Menu_Setup_itemsToCombine[selected_ingredient_index] - 1]);
    }
}

// decomp: MENUC.PRG _initUiShield (src/MENU/MENUC.PRG/168.c:3404)
void Menu_Setup_SetShieldRowWithoutGems(
    Items_InventoryShield* shield, char** menuText, int* rowTypes, char* stringBuf)
{
    int gems = *(int*)shield->gems;
    *(int*)shield->gems = 0;

    Menu_Framework_FormatInventoryShieldRow(shield, menuText, rowTypes, stringBuf);

    *(int*)shield->gems = gems;
}

// decomp: MENUC.PRG _selectShields (src/MENU/MENUC.PRG/168.c:3421)
int Menu_Setup_SelectCombinationShield(int ingredient_slot)
{
    static int Menu_Setup_combinationShieldSelectedRow;
    static char Menu_Setup_combinationShieldState;
    static char Menu_Setup_combinationShieldSlot;

    char* menuText[2 * 9];
    int rowType;
    int i;
    void* stringBuf;
    int itemId;
    Items_InventoryShield* shield;

    shield = NULL;

    if (ingredient_slot != 0) {

        Menu_Setup_combinationShieldSlot = ingredient_slot;

        Menu_Setup_AnimateItemHeader(ingredient_slot + 10);
        Menu_Setup_combinationShieldState = 0;
        return 0;
    }

    itemId = Menu_Setup_itemsToCombine[Menu_Setup_combinationShieldSlot - 1];

    if (itemId != 0) {
        shield = &Audio_Seq_unusedInventory.shields[itemId - 1];
    }

    switch (Menu_Setup_combinationShieldState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {

            stringBuf = Core_Memory_AllocateFromHighEnd(0x3CC);



            if (shield == NULL) {
                menuText[0] = (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_shield];
                menuText[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_selectShield];
                *Menu_Framework_rowTypeBuf = 1;
            } else {
                Menu_Setup_SetShieldRowWithoutGems(shield, menuText, Menu_Framework_rowTypeBuf, stringBuf);
            }

            Menu_Setup_availableItems[0] = itemId;
            rowType = 1;

            for (i = 0; i < 8; ++i) {

                itemId = Audio_Seq_unusedInventoryIndices.shields[i];

                if ((itemId != 0) && (itemId != Menu_Setup_itemsToCombine[0])
                    && (itemId != Menu_Setup_itemsToCombine[1])) {

                    shield = &Audio_Seq_unusedInventory.shields[itemId - 1];

                    Menu_Setup_SetShieldRowWithoutGems(shield, menuText + rowType * 2,
                        &Menu_Framework_rowTypeBuf[rowType], stringBuf + rowType * 0x60);

                    if (!((Menu_Setup_workshopMaterials[Menu_Setup_currentWorkshop] >> shield->base.material)
                            & 1)) {
                        menuText[rowType * 2 + 1] = (char*)&Menu_Framework_menu12Text
                            [Text_Menu_MENU12_BIN_Offset_invalidShieldMaterial];
                        Menu_Framework_rowTypeBuf[rowType] |= 1;
                    }

                    if (shield->isEquipped != 0) {
                        Menu_Framework_rowTypeBuf[rowType] |= 0xCA00;
                    }

                    Menu_Setup_availableItems[rowType] = itemId;
                    ++rowType;
                }
            }

            Menu_Setup_BuildItemList(rowType, menuText, Menu_Framework_rowTypeBuf);
            Core_Memory_FreeFromHighEnd(stringBuf);

            Menu_Setup_combinationShieldState = 1;
        }
        break;

    case 1:
        itemId = Menu_Setup_availableItems[Menu_Setup_itemsListWindow + Menu_Setup_itemsListSelection];

        if (itemId != 0) {
            Menu_Setup_SetShieldBaseStats(&Audio_Seq_unusedInventory.shields[itemId - 1]);
        } else {
            Menu_Framework_ResetEquipmentStats();
        }

        Menu_Framework_UpdateInformationVisibility(
            Menu_Setup_itemsListSelection + Menu_Setup_itemsListWindow, Menu_Setup_itemsListState);
        Menu_Setup_UpdateItemList(2);

        Menu_Setup_combinationShieldSelectedRow = Menu_Setup_GetSelectedItemRow() + 1;

        if (Menu_Setup_combinationShieldSelectedRow != 0) {

            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

            if (Menu_Setup_combinationShieldSelectedRow == -2) {
                return -2;
            }
            if (Menu_Setup_combinationShieldSelectedRow > 0) {
                Menu_Setup_combinationShieldSelectedRow = Menu_Setup_availableItems[Menu_Setup_combinationShieldSelectedRow - 1];
            }
            Menu_Setup_combinationShieldState = 2;
        }
        break;

    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Setup_combinationShieldSelectedRow;
        }
        break;
    }

    return 0;
}

// decomp: MENUC.PRG _setShieldMenuItem (src/MENU/MENUC.PRG/168.c:3548)
void Menu_Setup_SetCombinationShieldRow(
    Menu_Framework_MenuRow* menuItem, Items_InventoryShield* shield)
{
    menuItem->rowIcon = 15;
    menuItem->material = shield->base.material;
    menuItem->outsetIcon = shield->isEquipped != 0;
}

// decomp: MENUC.PRG _combineShieldMenu (src/MENU/MENUC.PRG/168.c:3562)
int Menu_Setup_UpdateShieldCombination(int initialize_shield_combination)
{

    static char Menu_Setup_shieldCombinationState;
    static char Menu_Setup_shieldCombinationRow;
    static char Menu_Setup_shieldCombinationOkBlinkPhase;
    static char Menu_Setup_shieldCombinationLayoutPadding[3] __attribute__((unused));

    char* menuText[2];
    int rowType;
    int itemId_3;
    int gem_slot_index;
    int var_s1;
    int i;
    u_short* combination_help_text;
    int temp_s1;
    int temp_s2;
    int shield_gem_slot_count;
    Items_InventoryShield* shield1;
    Items_InventoryShield* shield0;
    Menu_Framework_MenuRow* combination_menu_row;

    if (initialize_shield_combination != 0) {

        Menu_Setup_RecordSlotSelection(5);

        *(u_short*)Menu_Setup_itemsToCombine = 0;

        Menu_Setup_PreviewShieldCombination(0);

        Menu_Setup_shieldCombinationRow = 1;
        Menu_Setup_shieldCombinationState = 0;
        return 0;
    }

    var_s1 = 0;

    if (Menu_Setup_itemsToCombine[0] != 0) {
        var_s1 = Menu_Setup_itemsToCombine[1] > 0;
    }

    Menu_Setup_shieldCombinationOkBlinkPhase += 0x10;

    if (Menu_Setup_shieldCombinationState != 2) {
        Menu_Setup_shieldCombinationOkBlinkPhase = 0;
    }

    switch (Menu_Setup_shieldCombinationState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }
        Menu_Setup_InitializeEquipmentHeader(Text_Menu_MENU12_BIN_Offset_combineShield, 0x1B);
        // Fallthrough

    case 1:
        temp_s1 = Menu_Setup_shieldBuf.base.id;
        combination_menu_row = Menu_Setup_AnimateCombinationResult(1, temp_s1);

        if (temp_s1 != 0) {
            Menu_Setup_SetCombinationShieldRow(combination_menu_row, &Menu_Setup_shieldBuf);
        }

        for (i = 0; i < 2; ++i) {

            temp_s1 = Menu_Setup_itemsToCombine[i];

            if (temp_s1 != 0) {
                shield0 = &Audio_Seq_unusedInventory.shields[temp_s1 - 1];
            }

            combination_menu_row = Menu_Framework_SetMenuItem(i + 0xB, 0x14E, 0x22 + i * 0x10, 0x97, 0,
                (temp_s1 == 0
                        ? (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_shield]
                        : Menu_Framework_itemNames[shield0->base.id]));
            combination_menu_row->state = 2;
            combination_menu_row->targetPosition0 = 0xA9;

            if (temp_s1 == 0) {
                combination_menu_row->fontColor = 1;
            } else {
                Menu_Setup_SetCombinationShieldRow(combination_menu_row, shield0);
            }
        }

        Menu_Setup_shieldCombinationState = 2;
        break;

    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            if (!(Input_buttonsPressed.allButtons & PADRup)) {

                if (var_s1 != 0) {
                    Menu_Setup_DrawConfirmationLabel(2, Menu_Setup_shieldCombinationOkBlinkPhase);
                }

                if (Input_buttonsPressed.allButtons & PADRdown) {

                    temp_s2 = Menu_Setup_PopSlotSelection(5);

                    if (temp_s2 != 0) {

                        Menu_PlayLeaveSound();

                        Menu_Setup_shieldCombinationRow = temp_s2;
                        Menu_Setup_itemsToCombine[temp_s2 - 1] = 0;

                        Menu_Setup_PreviewShieldCombination(temp_s2);
                        combination_menu_row = Menu_Framework_SetMenuItem(temp_s2 + 0xA, 0xA9,
                            (temp_s2 * 0x10) + 0x12, 0x97, 0,
                            (char*)(Menu_Framework_menu12Text + 0x439));

                        combination_menu_row->selected = 1;
                        combination_menu_row->fontColor = 1;
                        var_s1 = Menu_Setup_shieldBuf.base.id;
                        combination_menu_row = Menu_Setup_SetCombinationResultRow(1, var_s1);

                        if (var_s1 != 0) {
                            Menu_Setup_SetCombinationShieldRow(combination_menu_row, &Menu_Setup_shieldBuf);
                            return 0;
                        }

                        break;
                    }

                    return Menu_Setup_LeaveItemSelection(1);
                }

                if ((Input_buttonsPressed.allButtons & PADstart) && (var_s1 != 0)) {
                    Menu_PlaySelectSound();
                    Menu_Framework_MenuItem(Menu_Setup_shieldCombinationRow + 0xA)->selected = 0;

                    Menu_Setup_shieldCombinationRow = 3;
                    Menu_Setup_shieldCombinationState = 4;

                    Menu_Setup_PreviewShieldCombination(3);
                    return 0;
                }

                temp_s2 = Menu_Setup_shieldCombinationRow;

                if (Input_buttonsPressed.allButtons & PADRright) {

                    if (temp_s2 == 0) {
                        Menu_PlayInvalidSound();
                    } else {

                        Menu_PlaySelectSound();

                        if (temp_s2 == 3) {
                            Menu_Setup_shieldCombinationState = 4;
                        } else {
                            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                            Menu_Setup_SelectCombinationShield(temp_s2);
                            Menu_Setup_shieldCombinationState = 3;
                        }

                        break;
                    }
                }

                if (Input_buttonRepeat & PADLup) {
                    if (temp_s2 == 0) {
                        temp_s2 = var_s1 + 2;
                    } else {
                        --temp_s2;
                    }
                }

                if (Input_buttonRepeat & PADLdown) {
                    if (temp_s2 == (var_s1 + 2)) {
                        temp_s2 = 0;
                    } else {
                        ++temp_s2;
                    }
                }

                if (temp_s2 != Menu_Setup_shieldCombinationRow) {
                    Menu_PlayChangeSound();

                    Menu_Setup_shieldCombinationRow = temp_s2;

                    Menu_Setup_PreviewShieldCombination(Menu_Setup_shieldCombinationRow);
                }

                Menu_Framework_rowTypeBuf[0] = Menu_Setup_shieldBuf.base.id < 1;

                for (var_s1 = 1; var_s1 < 3; ++var_s1) {
                    Menu_Framework_MenuItem(var_s1 + 0xA)->selected = var_s1 == Menu_Setup_shieldCombinationRow;
                    Menu_Framework_rowTypeBuf[var_s1] = Menu_Setup_itemsToCombine[var_s1 - 1] == 0;
                }

                Menu_Framework_rowTypeBuf[3] = 1;

                Menu_Framework_UpdateInformationVisibility(temp_s2, 0x10);

                menuText[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_selectShield];

                switch (temp_s2) {
                case 0:
                    if (Menu_Setup_shieldBuf.base.id != 0) {
                        Menu_Setup_SetShieldRowWithoutGems(
                            &Menu_Setup_shieldBuf, menuText, &rowType, Menu_Framework_stringBuf);
                    }
                    break;

                case 1:
                case 2:
                    var_s1 = Menu_Setup_itemsToCombine[temp_s2 - 1];

                    if (var_s1 != 0) {

                        shield1 = &Audio_Seq_unusedInventory.shields[var_s1 - 1];
                        var_s1 = *(int*)shield1->gems;
                        *(int*)shield1->gems = 0;

                        Menu_Setup_SetShieldRowWithoutGems(shield1, menuText, &rowType, Menu_Framework_stringBuf);

                        *(int*)shield1->gems = var_s1;
                    }
                    break;

                case 3:
                    menuText[1] = (char*)(Menu_Framework_menu12Text + 0x39E);
                    break;
                }

                Menu_Framework_SetInformationMessage((char*)menuText[1]);

                var_s1 = (((temp_s2 * 0x10) + 0xA) << 0x10) | 0x9B;

                if (temp_s2 == 0) {
                    var_s1 -= 0xE;
                }

                if (temp_s2 == 3) {
                    var_s1 += 0xFFFC0000;
                }

                Menu_Setup_shieldCombinationCursorAnimation = Menu_Framework_DrawCursor(Menu_Setup_shieldCombinationCursorAnimation, var_s1);
                break;
            }

            return Menu_Setup_LeaveItemSelection(1);
        }

        Menu_Setup_shieldCombinationOkBlinkPhase = 0;
        break;

    case 3:
        temp_s2 = Menu_Setup_SelectCombinationShield(0);

        if (temp_s2 == 0) {
            break;
        }

        if (temp_s2 != -2) {
            if (temp_s2 > 0) {
                var_s1 = Menu_Setup_shieldCombinationRow - 1;
                if (temp_s2 == Menu_Setup_itemsToCombine[Menu_Setup_shieldCombinationRow - 1]) {
                    Menu_Setup_itemsToCombine[Menu_Setup_shieldCombinationRow - 1] = 0;
                    Menu_Setup_PopSlotSelection(var_s1);
                } else {
                    Menu_Setup_itemsToCombine[Menu_Setup_shieldCombinationRow - 1] = temp_s2;
                    Menu_Setup_RecordSlotSelection(var_s1);
                    Menu_Setup_shieldCombinationRow = (Menu_Setup_shieldCombinationRow + 1);
                    if (var_s1 == 1) {
                        if (Menu_Setup_itemsToCombine[0] == 0) {
                            Menu_Setup_shieldCombinationRow = var_s1;
                        }
                    }
                }
                Menu_Setup_PreviewShieldCombination(Menu_Setup_shieldCombinationRow);
            }
            Menu_Setup_shieldCombinationState = 1;
            break;
        }
        return Menu_Setup_LeaveItemSelection(1);

    case 4:
        Menu_Framework_UpdatePrimaryStatPanel(2);
        Menu_Setup_UpdateThreeChoicePrompt(2);

        var_s1 = 0;

        for (i = 0; i < 2; ++i) {

            shield0 = &Audio_Seq_unusedInventory.shields[Menu_Setup_itemsToCombine[i] - 1];
            shield_gem_slot_count = shield0->base.gemSlots;
            var_s1 |= shield0->isEquipped;

            for (gem_slot_index = 0; gem_slot_index < shield_gem_slot_count; ++gem_slot_index) {
                var_s1 |= shield0->gems[gem_slot_index];
            }
        }

        if (var_s1 == 0) {
            combination_help_text = Menu_Framework_menu12Text + 0x42B;
        } else {
            combination_help_text = Menu_Framework_menu12Text + 0x4A7;
        }

        Menu_Framework_SetInformationMessage((char*)combination_help_text);

        Menu_Setup_shieldCombinationState = 5;
        break;

    case 5:
        temp_s2 = Menu_Setup_UpdateThreeChoicePrompt(0);

        if (temp_s2 == 0) {
            break;
        }

        switch (temp_s2) {
        case 1:
            Audio_PlaySfxCentered(0x7E, 0x18);
            Menu_Framework_InitializeInformationBox();
            Menu_Framework_UpdateEquipmentStatPages(-1);
            Menu_Framework_UpdateDpPpBars(4);
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Menu_Framework_FlyOutRowLeft(0);

            for (i = 0; i < 2; ++i) {

                temp_s1 = Menu_Setup_itemsToCombine[i];
                itemId_3 = temp_s1 - 1;

                if (Audio_Seq_unusedInventory.shields[itemId_3].isEquipped != 0) {
                    Items_Inventory_UnequipShield();
                    Menu_Setup_shieldCombinationState = 6;
                }

                Menu_Setup_DetachShieldGems(itemId_3);
                Items_Inventory_ClearItemSlot(Items_ItemCategory_Shield, temp_s1);
            }

            temp_s2 = Menu_Setup_itemsToCombine[0];
            shield1 = &Audio_Seq_unusedInventory.shields[temp_s2 - 1];

            Core_Memory_CopyHalfwords(shield1, &Menu_Setup_shieldBuf, 0x30);

            shield1->index = temp_s2;

            Items_Inventory_RebuildDisplayIndices(3);

            return Menu_Setup_shieldCombinationState != 6;

        case 2:
            Menu_PlayLeaveSound();
            Menu_Framework_UpdatePrimaryStatPanel(1);

            Menu_Setup_shieldCombinationState = temp_s2;
            break;

        case 3:
            return Menu_Setup_LeaveItemSelection(1);
        }
        break;

    case 6:
        if (Menu_Setup_ReloadAshleyEquipment(1) == 0) {
            return -1;
        }
        break;
    }

    return 0;
}

// decomp: MENUC.PRG _initCombineArmor (src/MENU/MENUC.PRG/168.c:3947)
void Menu_Setup_PreviewArmorCombination(int preview_selection)
{
    int var_a0;
    u_int selected_ingredient_index;
    Items_InventoryArmor* armor;

    int i = Menu_Setup_combiningArmorIds[0];
    int second_armor_id = Menu_Setup_combiningArmorIds[1];
    int ingredient_presence_mask = i != 0;

    if (second_armor_id != 0) {
        ingredient_presence_mask += 2;
    }

    switch (ingredient_presence_mask) {
    case 0:
        Core_Memory_ZeroWordsBackward(&Menu_Setup_armorBuf, sizeof Menu_Setup_armorBuf);
        break;

    case 1:
    case 2:
        Core_Memory_CopyHalfwords(
            &Menu_Setup_armorBuf, &Audio_Seq_unusedInventory.armor[i + second_armor_id - 1], sizeof Menu_Setup_armorBuf);
        break;

    case 3:
        Core_Memory_ZeroWordsBackward(&Menu_Setup_armorBuf, sizeof Menu_Setup_armorBuf);
        Menu_Setup_BuildCombinedArmor(&Audio_Seq_unusedInventory.armor[i - 1],
            &Audio_Seq_unusedInventory.armor[second_armor_id - 1], &Menu_Setup_armorBuf, Menu_Setup_sydData);
        break;
    }

    armor = &Menu_Setup_armorBuf;
    armor->limb = 0;

    Menu_Framework_ResetEquipmentStats();

    selected_ingredient_index = preview_selection - 1;

    if (selected_ingredient_index >= 2) {
        if (Menu_Setup_armorBuf.id != 0) {

            for (i = 0; i < 16; ++i) {
                Menu_Framework_equipmentStats[i] = armor->classes[i & 7];
                Menu_Framework_equipmentStats[16 + i] = armor->affinities[i & 7];
            }

            for (i = 0; i < 4; ++i) {
                Menu_Framework_equipmentStats[32 + i] = armor->types[i];
                var_a0 += 2;
            }

            Menu_Framework_SetDpPp(armor->currentDp, armor->maxDp, 0, 0);
            Menu_Framework_SetPrimaryStats(
                armor->strength, armor->intelligence, armor->agility, 1);

            Menu_Framework_equipmentSubtype = 0x10;
            Menu_Framework_strIntAgi[4] = armor->strength;
            Menu_Framework_strIntAgi[5] = armor->intelligence;
            Menu_Framework_strIntAgi[6] = armor->agility;
        }
    } else if (ingredient_presence_mask & preview_selection) {
        Menu_Framework_SetArmorStats(Menu_Setup_combiningArmorIds[selected_ingredient_index]);
    }
}

// decomp: MENUC.PRG _initUiArmor (src/MENU/MENUC.PRG/168.c:4019)
int Menu_Setup_SelectCombinationArmor(int ingredient_slot)
{
    static int Menu_Setup_combinationArmorSelectedRow;
    static char Menu_Setup_combinationArmorStep;
    static char Menu_Setup_combinationArmorSlot;

    char* armor_row_text[2 * 17];
    int armor_row_count;
    int i;
    char* armor_text_allocation;
    int itemId;
    int s0_2;
    Items_InventoryArmor* armor_item;

    armor_item = NULL;
    if (ingredient_slot != 0) {
        Menu_Setup_combinationArmorSlot = ingredient_slot;
        Menu_Setup_AnimateItemHeader(ingredient_slot + 0xA);
        Menu_Setup_combinationArmorStep = 0;
        return 0;
    }

    itemId = Menu_Setup_combiningArmorIds[Menu_Setup_combinationArmorSlot - 1];
    if (itemId != 0) {
        armor_item = &Audio_Seq_unusedInventory.armor[itemId - 1];
    }

    switch (Menu_Setup_combinationArmorStep) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            s0_2 = itemId;
            armor_text_allocation = Core_Memory_AllocateFromHighEnd(0x72CU);

            if (armor_item == NULL) {
                armor_row_text[0] = (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_armor];
                armor_row_text[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_selectArmor];
                *Menu_Framework_rowTypeBuf = 1;
            } else {
                Menu_Framework_FormatInventoryArmorRow(
                    armor_item, armor_row_text, Menu_Framework_rowTypeBuf, armor_text_allocation);
            }

            *Menu_Setup_availableItems = s0_2;
            armor_row_count = 1;

            for (i = 0; i < 16; ++i) {
                itemId = Audio_Seq_unusedInventoryIndices.armor[i];
                if ((itemId != 0) && (itemId != Menu_Setup_combiningArmorIds[0])
                    && (itemId != Menu_Setup_combiningArmorIds[1])) {
                    armor_item = &Audio_Seq_unusedInventory.armor[itemId - 1];
                    if (armor_item->category != 7) {
                        Menu_Framework_FormatInventoryArmorRow(armor_item, armor_row_text + armor_row_count * 2,
                            &Menu_Framework_rowTypeBuf[armor_row_count], armor_text_allocation + armor_row_count * 0x60);
                        if (!((Menu_Setup_workshopMaterials[Menu_Setup_currentWorkshop] >> armor_item->material)
                                & 1)) {
                            armor_row_text[armor_row_count * 2 + 1] = (char*)&Menu_Framework_menu12Text
                                [Text_Menu_MENU12_BIN_Offset_invalidArmorMaterial];
                            Menu_Framework_rowTypeBuf[armor_row_count] |= 1;
                        }
                        if (armor_item->limb != 0) {
                            Menu_Framework_rowTypeBuf[armor_row_count] |= 0xCA00;
                        }
                        Menu_Setup_availableItems[armor_row_count] = itemId;
                        ++armor_row_count;
                    }
                }
            }
            Menu_Setup_BuildItemList(armor_row_count, armor_row_text, Menu_Framework_rowTypeBuf);
            Core_Memory_FreeFromHighEnd(armor_text_allocation);
            Menu_Setup_combinationArmorStep = 1;
        }
        break;
    case 1:
        itemId = Menu_Setup_availableItems[Menu_Setup_itemsListWindow + Menu_Setup_itemsListSelection];
        if (itemId != 0) {
            Menu_Framework_SetArmorStats(itemId);
        } else {
            Menu_Framework_ResetEquipmentStats();
        }
        Menu_Framework_UpdateInformationVisibility(
            Menu_Setup_itemsListSelection + Menu_Setup_itemsListWindow, Menu_Setup_itemsListState);
        Menu_Setup_UpdateItemList(2);
        Menu_Setup_combinationArmorSelectedRow = Menu_Setup_GetSelectedItemRow() + 1;
        if (Menu_Setup_combinationArmorSelectedRow != 0) {
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            if (Menu_Setup_combinationArmorSelectedRow == -2) {
                return -2;
            }
            if (Menu_Setup_combinationArmorSelectedRow > 0) {
                Menu_Setup_combinationArmorSelectedRow = Menu_Setup_availableItems[Menu_Setup_combinationArmorSelectedRow - 1];
            }
            Menu_Setup_combinationArmorStep = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Setup_combinationArmorSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _setArmorMenuItem (src/MENU/MENUC.PRG/168.c:4126)
void Menu_Setup_SetCombinationArmorRow(
    Menu_Framework_MenuRow* menuItem, Items_InventoryArmor* armor)
{
    menuItem->rowIcon = armor->category + 14;
    menuItem->material = armor->material;
    menuItem->outsetIcon = armor->limb != 0;
}

// decomp: MENUC.PRG _combineArmorMenu (src/MENU/MENUC.PRG/168.c:4140)
int Menu_Setup_UpdateArmorCombination(int initialize_armor_combination)
{

    static char Menu_Setup_armorCombinationState;
    static char Menu_Setup_armorCombinationRow;
    static char Menu_Setup_armorCombinationOkBlinkPhase;

    char* menuText[2];
    int rowType;
    int itemId;
    int confirmation_result;
    int i;
    int var_s1;
    char* combination_help_text;
    int i_2;
    Items_InventoryArmor* armor;
    Menu_Framework_MenuRow* result_menu_row;
    Menu_Framework_MenuRow* ingredient_menu_row;

    if (initialize_armor_combination != 0) {
        Menu_Setup_RecordSlotSelection(5);
        *(u_short*)Menu_Setup_combiningArmorIds = 0;
        Menu_Setup_PreviewArmorCombination(0);
        Menu_Setup_armorCombinationRow = 1;
        Menu_Setup_armorCombinationState = 0;
        return 0;
    }

    var_s1 = 0;
    if (Menu_Setup_combiningArmorIds[0] != 0) {
        var_s1 = Menu_Setup_combiningArmorIds[1] > 0;
    }

    Menu_Setup_armorCombinationOkBlinkPhase += 16;

    if (Menu_Setup_armorCombinationState != 2) {
        Menu_Setup_armorCombinationOkBlinkPhase = 0;
    }

    switch (Menu_Setup_armorCombinationState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }
        Menu_Setup_InitializeEquipmentHeader(Text_Menu_MENU12_BIN_Offset_combineArmor, 0x1C);
        // Fallthrough
    case 1:
        var_s1 = Menu_Setup_armorBuf.id;
        result_menu_row = Menu_Setup_AnimateCombinationResult(2, var_s1);
        if (var_s1 != 0) {
            Menu_Setup_SetCombinationArmorRow(result_menu_row, &Menu_Setup_armorBuf);
        }

        for (i = 0; i < 2; ++i) {
            var_s1 = Menu_Setup_combiningArmorIds[i];
            if (var_s1 != 0) {
                armor = &Audio_Seq_unusedInventory.armor[var_s1 - 1];
            }
            ingredient_menu_row = Menu_Framework_SetMenuItem(i + 11, 334, 34 + i * 16, 151, 0,
                var_s1 == 0 ? (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_armor]
                            : Menu_Framework_itemNames[armor->id]);
            ingredient_menu_row->state = 2;
            ingredient_menu_row->targetPosition0 = 169;

            if (var_s1 == 0) {
                ingredient_menu_row->fontColor = 1;
            } else {
                Menu_Setup_SetCombinationArmorRow(ingredient_menu_row, armor);
            }
        }

        Menu_Setup_armorCombinationState = 2;
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            if (!(Input_buttonsPressed.allButtons & PADRup)) {
                if (var_s1 != 0) {
                    Menu_Setup_DrawConfirmationLabel(2, Menu_Setup_armorCombinationOkBlinkPhase);
                }
                if (Input_buttonsPressed.allButtons & PADRdown) {
                    int restored_ingredient_slot = Menu_Setup_PopSlotSelection(5);
                    if (restored_ingredient_slot != 0) {
                        Menu_PlayLeaveSound();
                        Menu_Setup_armorCombinationRow = restored_ingredient_slot;
                        Menu_Setup_combiningArmorIds[restored_ingredient_slot - 1] = 0;
                        Menu_Setup_PreviewArmorCombination(restored_ingredient_slot);
                        ingredient_menu_row = Menu_Framework_SetMenuItem(restored_ingredient_slot + 10, 169,
                            (restored_ingredient_slot * 16) + 18, 151, 0,
                            (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_armor]);
                        ingredient_menu_row->selected = 1;
                        ingredient_menu_row->fontColor = 1;
                        var_s1 = Menu_Setup_armorBuf.id;
                        result_menu_row = Menu_Setup_SetCombinationResultRow(2, var_s1);
                        if (var_s1 != 0) {
                            Menu_Setup_SetCombinationArmorRow(result_menu_row, &Menu_Setup_armorBuf);
                        }
                        break;
                    }
                    return Menu_Setup_LeaveItemSelection(1);
                }
                if (Input_buttonsPressed.allButtons & PADstart) {
                    if (var_s1 != 0) {
                        Menu_PlaySelectSound();
                        Menu_Framework_MenuItem(Menu_Setup_armorCombinationRow + 10)->selected = 0;
                        Menu_Setup_armorCombinationRow = 3;
                        Menu_Setup_armorCombinationState = 4;
                        Menu_Setup_PreviewArmorCombination(3);
                        return 0;
                    }
                }
                i_2 = Menu_Setup_armorCombinationRow;
                if (Input_buttonsPressed.allButtons & PADRright) {
                    if (i_2 == 0) {
                        Menu_PlayInvalidSound();
                    } else {
                        Menu_PlaySelectSound();
                        if (i_2 == 3) {
                            Menu_Setup_armorCombinationState = 4;
                        } else {
                            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                            Menu_Setup_SelectCombinationArmor(i_2);
                            Menu_Setup_armorCombinationState = 3;
                        }
                        break;
                    }
                }
                if (Input_buttonRepeat & PADLup) {
                    if (i_2 == 0) {
                        i_2 = var_s1 + 2;
                    } else {
                        --i_2;
                    }
                }
                if (Input_buttonRepeat & PADLdown) {
                    if (i_2 == (var_s1 + 2)) {
                        i_2 = 0;
                    } else {
                        ++i_2;
                    }
                }

                if (i_2 != Menu_Setup_armorCombinationRow) {
                    Menu_PlayChangeSound();
                    Menu_Setup_armorCombinationRow = i_2;
                    Menu_Setup_PreviewArmorCombination(Menu_Setup_armorCombinationRow);
                }
                Menu_Framework_rowTypeBuf[0] = Menu_Setup_armorBuf.id < 1;

                for (var_s1 = 1; var_s1 < 3; ++var_s1) {
                    Menu_Framework_MenuItem(var_s1 + 10)->selected = var_s1 == Menu_Setup_armorCombinationRow;
                    Menu_Framework_rowTypeBuf[var_s1] = Menu_Setup_combiningArmorIds[var_s1 - 1] == 0;
                }

                Menu_Framework_rowTypeBuf[3] = 1;
                Menu_Framework_UpdateInformationVisibility(i_2, 16);
                menuText[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_selectArmor];
                switch (i_2) {
                case 0:
                    if (Menu_Setup_armorBuf.id != 0) {
                        Menu_Framework_FormatInventoryArmorRow(
                            &Menu_Setup_armorBuf, menuText, &rowType, Menu_Framework_stringBuf);
                    }
                    break;
                case 1:
                case 2:
                    var_s1 = Menu_Setup_combiningArmorIds[i_2 - 1];
                    if (var_s1 != 0) {
                        Menu_Framework_FormatInventoryArmorRow(
                            &Audio_Seq_unusedInventory.armor[var_s1 - 1], menuText, &rowType,
                            Menu_Framework_stringBuf);
                    }
                    break;
                case 3:
                    menuText[1] =
                        (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_pressOk];
                    break;
                }
                Menu_Framework_SetInformationMessage((char*)menuText[1]);
                var_s1 = (((i_2 * 16) + 10) << 0x10) | 0x9B;
                if (i_2 == 0) {
                    var_s1 -= 14;
                }
                if (i_2 == 3) {
                    var_s1 += 0xFFFC0000;
                }
                Menu_Setup_armorCombinationCursorAnimation = Menu_Framework_DrawCursor(Menu_Setup_armorCombinationCursorAnimation, var_s1);
                break;
            }
            return Menu_Setup_LeaveItemSelection(1);
        }
        Menu_Setup_armorCombinationOkBlinkPhase = 0;
        break;
    case 3:
        itemId = Menu_Setup_SelectCombinationArmor(0);
        if (itemId == 0) {
            break;
        }
        if (itemId == -2) {
            return Menu_Setup_LeaveItemSelection(1);
        }
        if (itemId > 0) {
            var_s1 = Menu_Setup_armorCombinationRow - 1;
            if (itemId == Menu_Setup_combiningArmorIds[Menu_Setup_armorCombinationRow - 1]) {
                Menu_Setup_combiningArmorIds[Menu_Setup_armorCombinationRow - 1] = 0;
                Menu_Setup_PopSlotSelection(var_s1);
            } else {
                Menu_Setup_combiningArmorIds[Menu_Setup_armorCombinationRow - 1] = itemId;
                Menu_Setup_RecordSlotSelection(var_s1);
                Menu_Setup_armorCombinationRow = (Menu_Setup_armorCombinationRow + 1);
                if (var_s1 == 1) {
                    if (Menu_Setup_combiningArmorIds[0] == 0) {
                        Menu_Setup_armorCombinationRow = var_s1;
                    }
                }
            }
            Menu_Setup_PreviewArmorCombination(Menu_Setup_armorCombinationRow);
        }
        Menu_Setup_armorCombinationState = 1;
        break;
    case 4:
        Menu_Framework_UpdatePrimaryStatPanel(2);
        Menu_Setup_UpdateThreeChoicePrompt(2);

        var_s1 = 0;

        for (i = 0; i < 2; ++i) {
            var_s1 |= Audio_Seq_unusedInventory.armor[Menu_Setup_combiningArmorIds[i] - 1].limb;
        }
        if (var_s1 == 0) {
            combination_help_text =
                (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_attachmentConfirm];
        } else {
            combination_help_text =
                (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_cancelCombineConfirm];
        }
        Menu_Framework_SetInformationMessage(combination_help_text);
        Menu_Setup_armorCombinationState = 5;
        break;
    case 5:
        confirmation_result = Menu_Setup_UpdateThreeChoicePrompt(0);

        if (confirmation_result == 0) {
            break;
        }

        switch (confirmation_result) {
        case 1:
            Audio_PlaySfxCentered(0x7E, 0x18);

            Menu_Framework_InitializeInformationBox();
            Menu_Framework_UpdateEquipmentStatPages(-1);
            Menu_Framework_UpdateDpPpBars(4);
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Menu_Framework_FlyOutRowLeft(0);
            for (i = 0; i < 2; ++i) {
                armor = &Audio_Seq_unusedInventory.armor[Menu_Setup_combiningArmorIds[i] - 1];
                var_s1 = armor->limb;
                if (var_s1 != 0) {
                    Items_EquipAshleyArmor(var_s1 - 1, NULL);
                    armor->limb = 0;
                }
                Items_Inventory_ClearItemSlot(Items_ItemCategory_Armor, Menu_Setup_combiningArmorIds[i]);
            }
            itemId = Menu_Setup_combiningArmorIds[0];
            armor = &Audio_Seq_unusedInventory.armor[itemId - 1];
            Core_Memory_CopyHalfwords(armor, &Menu_Setup_armorBuf, sizeof *armor);
            armor->index = itemId;
            Items_Inventory_RebuildDisplayIndices(4);
            return 1;
        case 2:
            Menu_PlayLeaveSound();
            Menu_Framework_UpdatePrimaryStatPanel(1);
            Menu_Setup_armorCombinationState = confirmation_result;
            break;
        case 3:
            return Menu_Setup_LeaveItemSelection(1);
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _countItems (src/MENU/MENUC.PRG/168.c:4428)
int Menu_Setup_CountCombinationIngredients(int category)
{
    int i;

    Items_InventoryArmor* armor = Audio_Seq_unusedInventory.armor;
    int count = 0;

    if (category == 2) {
        for (i = 0; i < 16; ++i, ++armor) {
            int is_combinable_armor = 0;
            if (armor->id != 0) {
                is_combinable_armor = armor->category != 7;
            }
            count += is_combinable_armor;
        }
    } else {
        count = Items_Inventory_CountItems((category * 2) | 1, NULL);
    }
    return count;
}

// decomp: MENUC.PRG _combineTopMenu (src/MENU/MENUC.PRG/168.c:4455)
int Menu_Setup_UpdateCombinationCategory(int initialize_combination_category)
{

    static char Menu_Setup_combinationCategoryState;
    static char Menu_Setup_combinationCategoryForceNoCursorMemory;

    char* text[6];
    int selectedRow;
    int i;
    int saved_cursor_memory;

    if (initialize_combination_category != 0) {
        Menu_Setup_combinationCategoryForceNoCursorMemory = 1;
        Menu_Framework_FlyOutRowsAndHoistSelection(5, 1);
        Menu_Setup_combinationCategoryState = 0;
        return 0;
    }

    switch (Menu_Setup_combinationCategoryState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 3; ++i) {
                int count = Menu_Setup_CountCombinationIngredients(i);
                text[i * 2] = (char*)&Menu_Framework_menu12Text
                    [Menu_Framework_menu12Text[i + Text_Menu_MENU12_BIN_Index_blade]];
                text[i * 2 + 1] =
                    count < 2 ? (char*)&Menu_Framework_menu12Text[Menu_Framework_menu12Text
                                        [Text_Menu_MENU12_BIN_Index_noItems - (count * 4)]]
                              : (char*)&Menu_Framework_menu12Text[Menu_Framework_menu12Text
                                        [i + Text_Menu_MENU12_BIN_Index_combineBlades]];
                Menu_Framework_rowTypeBuf[i] = count < 2;
            }

            saved_cursor_memory = Input_settings.cursorMemory;
            if (Menu_Setup_combinationCategoryForceNoCursorMemory == 0) {
                Input_settings.cursorMemory = 1;
            }
            Menu_Setup_combinationCategoryForceNoCursorMemory = 0;
            Menu_Framework_CreateMenuRows(3, 0x23D, text, Menu_Framework_rowTypeBuf);
            Input_settings.cursorMemory = saved_cursor_memory;
            Menu_Setup_combinationCategoryState = 1;
        }
        break;
    case 1:
        Menu_Framework_UpdateInformationVisibility(
            Menu_Framework_HighlightedRow(), Menu_Framework_itemsListRow);
        selectedRow = Menu_Framework_GetSelectedRow();
        i = selectedRow + 1;
        if (i != 0) {
            if (i < 0) {
                Menu_Framework_ClearMenuExcept(0);
                return i;
            }
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Menu_Setup_combineMenus[selectedRow](1);
            Menu_Setup_LoadCombinationRecipes(i);
            Menu_Setup_combinationCategoryState = selectedRow + 2;
        }
        break;
    case 2:
    case 3:
    case 4:
        if (Menu_Setup_LoadCombinationRecipes(0) != 0) {
            i = Menu_Setup_combineMenus[Menu_Setup_combinationCategoryState - 2](0);
            if (i != 0) {
                Core_Memory_FreeFromHighEnd(Menu_Setup_sydData);
                Menu_Setup_sydData = NULL;
                if (i == -1) {
                    Menu_Setup_combinationCategoryState = 5;
                    break;
                }
                return i;
            }
        }
        break;
    case 5:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_InitializeInformationBox();
            Menu_Setup_SetTitleRow(0, Text_Menu_MENU12_BIN_Offset_setup);
            Menu_Setup_SetTitleRow(15, Text_Menu_MENU12_BIN_Offset_combine);
            Menu_Setup_combinationCategoryState = 0U;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _setClassAffinities (src/MENU/MENUC.PRG/168.c:4759)
void Menu_Setup_CombineClassOrAffinityValues(signed char* first, signed char* second,
    signed char* result, int materialsDifferent, int setAffinities)
{
    static char Menu_Setup_combineClassOrAffinityValuesClassFactorLookup[][6] = { { 2, 0, 1, 1, 1, 0 }, { 0, 2, 0, 1, 1, 1 },
        { 1, 0, 2, 0, 1, 1 }, { 1, 1, 0, 2, 0, 1 }, { 1, 1, 1, 0, 2, 0 },
        { 0, 1, 1, 1, 0, 2 } };

    static char Menu_Setup_combineClassOrAffinityValuesAffinityFactorLookup[][7] = { { 2, 1, 1, 1, 1, 1, 1 },
        { 1, 2, 1, 0, 1, 1, 1 }, { 1, 1, 2, 1, 0, 1, 1 }, { 1, 0, 1, 2, 1, 1, 1 },
        { 1, 1, 0, 1, 2, 1, 1 }, { 1, 1, 1, 1, 1, 2, 0 }, { 1, 1, 1, 1, 1, 0, 2 } };


    short statSum;
    short statValue;
    int i;
    int nStats;
    u_int firstMaxStat;
    u_int secondMaxStat;
    signed char firstMaxVal;
    signed char secondMaxVal;
    signed char minStatVal;
    signed char maxStatValue;
    short factorLookup;

    nStats = 6;
    if (setAffinities != 0) {
        nStats = 7;
    }
    secondMaxVal = -128;
    firstMaxVal = -128;
    maxStatValue = -128;
    secondMaxStat = 0;
    firstMaxStat = 0;

    for (i = 0; i < nStats; ++i) {
        if (firstMaxVal < first[i]) {
            firstMaxVal = first[i];
            firstMaxStat = i;
        }
        if (maxStatValue < first[i]) {
            maxStatValue = first[i];
        }
    }

    for (i = 0; i < nStats; ++i) {
        if (secondMaxVal < second[i]) {
            secondMaxVal = second[i];
            secondMaxStat = i;
        }
        if (maxStatValue < second[i]) {
            maxStatValue = second[i];
        }
    }

    minStatVal = 127;

    for (i = 0; i < nStats; ++i) {
        if (first[i] < minStatVal) {
            minStatVal = first[i];
        }
    }

    for (i = 0; i < nStats; ++i) {
        if (second[i] < minStatVal) {
            minStatVal = second[i];
        }
    }

    for (i = 0; i < nStats; ++i) {
        if ((first[i] == maxStatValue) || ((second[i] == maxStatValue))) {
            result[i] = maxStatValue;
        } else if ((first[i] == minStatVal) || (second[i] == minStatVal)) {
            result[i] = minStatVal;
        } else {
            if (setAffinities == 0) {
                factorLookup = Menu_Setup_combineClassOrAffinityValuesClassFactorLookup[firstMaxStat][secondMaxStat];
            } else {
                factorLookup = Menu_Setup_combineClassOrAffinityValuesAffinityFactorLookup[firstMaxStat][secondMaxStat];
            }
            statSum = first[i] + second[i];
            if (materialsDifferent == 0) {
                statValue = (statSum * Menu_Setup_combineClassOrAffinityValuesSameMaterialStatFactors[factorLookup]) / 10;
            } else {
                statValue =
                    (statSum * *(factorLookup + Menu_Setup_combineClassOrAffinityValuesDifferentMaterialStatFactors)) / 10;
            }
            if (statValue > 100) {
                statValue = 100;
            } else if (statValue < -100) {
                statValue = -100;
            }
            result[i] = statValue;
        }
    }
}

// decomp: MENUC.PRG _setTypeValues (src/MENU/MENUC.PRG/168.c:4864)
void Menu_Setup_CombinePhysicalTypeValues(
    signed char* first, signed char* second, signed char* result, int materialsDifferent)
{

    short typeValue;
    short typeValueSum;
    int sameMaxType;
    int i;
    u_int firstMaxType;
    u_int secondMaxType;
    signed char firstMaxVal;
    signed char secondMaxVal;

    secondMaxVal = -128;
    firstMaxVal = -128;
    secondMaxType = 1;
    firstMaxType = 1;

    for (i = 1; i < 4; ++i) {
        if (firstMaxVal < first[i]) {
            firstMaxVal = first[i];
            firstMaxType = i;
        }
    }

    for (i = 1; i < 4; ++i) {
        if (secondMaxVal < second[i]) {
            secondMaxVal = second[i];
            secondMaxType = i;
        }
    }

    for (i = 1; i < 4; ++i) {
        sameMaxType = firstMaxType == secondMaxType;
        typeValueSum = first[i] + second[i];
        if (materialsDifferent == 0) {
            typeValue = (typeValueSum * Menu_Setup_combinePhysicalTypeValuesSameMaterialTypeFactors[sameMaxType]) / 10;
        } else {
            typeValue = (typeValueSum * Menu_Setup_combinePhysicalTypeValuesDifferentMaterialTypeFactors[sameMaxType]) / 10;
        }
        if (typeValue > 100) {
            typeValue = 100;
        } else if (typeValue < -100) {
            typeValue = -100;
        }
        result[i] = typeValue;
    }
}

// decomp: MENUC.PRG _combineBlades (src/MENU/MENUC.PRG/168.c:4934)
Items_InventoryBlade* Menu_Setup_BuildCombinedBlade(Items_InventoryBlade* first,
    Items_InventoryBlade* second, Items_InventoryBlade* result, void* sydData)
{
    struct {
        char reserved0;
        char wepId;
        char category;
        char damageType;
        char costType;
        char cost;
        char opaque6;
        char reserved7;
        char strength;
        char intelligence;
        char agility;
        char reservedB;
        Items_WeaponRange range;
    }* info;

    Menu_Setup_combinationResults = sydData + ((Menu_Setup_SydHeader*)sydData)->combinationsOffset;
    Menu_Setup_materialResults = sydData + ((Menu_Setup_SydHeader*)sydData)->materialsOffset;
    Menu_Setup_bladeCombinationInitData = sydData + ((Menu_Setup_SydHeader*)sydData)->initDataOffset;

    Core_Memory_CopyAlignedWords(result, first, sizeof *result);

    result->material = Menu_Setup_materialResults[(first->material - 3) * 500 + first->category * 50
                                        + (second->material - 3) * 10 + second->category];
    result->subId = Menu_Setup_combinationResults[(first->id * 96) + second->id];
    result->id = result->subId;
    info = Menu_Setup_bladeCombinationInitData + (result->id * 16);
    result->wepId = info->wepId;
    result->category = info->category;
    result->cost = info->cost;
    result->damageType = info->damageType & 3;
    result->costType = info->costType & 7;
    result->opaque12 = info->opaque6;
    result->range = info->range;
    result->strength = info->strength + Audio_Seq_unusedMaterialModifiers[result->material][24];
    result->intelligence =
        info->intelligence + Audio_Seq_unusedMaterialModifiers[result->material][26];
    result->agility = info->agility + Audio_Seq_unusedMaterialModifiers[result->material][28];
    result->maxDp = result->currentDp = (first->maxDp + second->maxDp) >> 1;
    result->maxPp = (first->maxPp + second->maxPp) >> 1;
    result->currentPp = (first->currentPp + second->currentPp) >> 1;

    if (first->material == second->material) {
        Menu_Setup_CombineClassOrAffinityValues(first->classes, second->classes, result->classes, 0, 0);
        Menu_Setup_CombineClassOrAffinityValues(
            first->affinities, second->affinities, result->affinities, 0, 1);
    } else {
        Menu_Setup_CombineClassOrAffinityValues(first->classes, second->classes, result->classes, 1, 0);
        Menu_Setup_CombineClassOrAffinityValues(
            first->affinities, second->affinities, result->affinities, 1, 1);
    }
    return result;
}

// decomp: MENUC.PRG _combineShields (src/MENU/MENUC.PRG/168.c:5012)
Items_InventoryArmor* Menu_Setup_BuildCombinedShield(Items_InventoryArmor* first,
    Items_InventoryArmor* second, Items_InventoryArmor* result, void* sydData)
{
    Menu_Setup_CombinedArmorRecord* info;

    Menu_Setup_combinationResults = sydData + ((Menu_Setup_SydHeader*)sydData)->combinationsOffset;
    Menu_Setup_materialResults = sydData + ((Menu_Setup_SydHeader*)sydData)->materialsOffset;
    Menu_Setup_shieldCombinationInitData = sydData + ((Menu_Setup_SydHeader*)sydData)->initDataOffset;

    Core_Memory_CopyAlignedWords(result, first, sizeof *result);

    result->material = Menu_Setup_materialResults[(first->material * 8) + second->material];
    result->subId = Menu_Setup_combinationResults[(first->subId * 17) + second->subId];
    result->id = result->subId + 126;
    info = &Menu_Setup_shieldCombinationInitData[result->subId];
    result->wepId = info->wepId;
    result->category = info->category;
    result->gemSlots = info->gemSlots;
    result->strength = info->strength + Audio_Seq_unusedMaterialModifiers[result->material][24];
    result->intelligence =
        info->intelligence + Audio_Seq_unusedMaterialModifiers[result->material][26];
    result->agility = info->agility + Audio_Seq_unusedMaterialModifiers[result->material][28];
    result->maxDp = result->currentDp = (first->maxDp + second->maxDp) / 2;
    result->maxPp = ((first->maxPp + second->maxPp) >> 1);
    result->currentPp = ((first->currentPp + second->currentPp) >> 1);
    if (first->material == second->material) {
        Menu_Setup_CombineClassOrAffinityValues(first->classes, second->classes, result->classes, 0, 0);
        Menu_Setup_CombineClassOrAffinityValues(
            first->affinities, second->affinities, result->affinities, 0, 1);
        Menu_Setup_CombinePhysicalTypeValues(first->types, second->types, result->types, 0);
    } else {
        Menu_Setup_CombineClassOrAffinityValues(first->classes, second->classes, result->classes, 1, 0);
        Menu_Setup_CombineClassOrAffinityValues(
            first->affinities, second->affinities, result->affinities, 1, 1);
        Menu_Setup_CombinePhysicalTypeValues(first->types, second->types, result->types, 1);
    }
    return result;
}

// decomp: MENUC.PRG _combineArmor (src/MENU/MENUC.PRG/168.c:5061)
Items_InventoryArmor* Menu_Setup_BuildCombinedArmor(Items_InventoryArmor* first,
    Items_InventoryArmor* second, Items_InventoryArmor* result, void* sydData)
{
    Menu_Setup_CombinedArmorRecord* info;

    Menu_Setup_combinationResults = sydData + ((Menu_Setup_SydHeader*)sydData)->combinationsOffset;
    Menu_Setup_materialResults = sydData + ((Menu_Setup_SydHeader*)sydData)->materialsOffset;
    Menu_Setup_combinationInitData = sydData + ((Menu_Setup_SydHeader*)sydData)->initDataOffset;

    Core_Memory_CopyAlignedWords(result, first, sizeof *result);

    result->material = Menu_Setup_materialResults[first->material * 128 + (first->category - 2) * 32
                                        + second->material * 4 + (second->category - 2)];
    result->subId = Menu_Setup_combinationResults[(first->subId - 16) * 65 + (second->subId - 16)];
    result->id = result->subId + 126;
    info = &Menu_Setup_combinationInitData[result->subId];
    result->category = info->category;
    result->strength = info->strength + Audio_Seq_unusedMaterialModifiers[result->material][24];
    result->intelligence =
        info->intelligence + Audio_Seq_unusedMaterialModifiers[result->material][26];
    result->agility = info->agility + Audio_Seq_unusedMaterialModifiers[result->material][28];
    result->maxDp = result->currentDp = (first->maxDp + second->maxDp) / 2;
    if (first->material == second->material) {
        Menu_Setup_CombineClassOrAffinityValues(first->classes, second->classes, result->classes, 0, 0);
        Menu_Setup_CombineClassOrAffinityValues(
            first->affinities, second->affinities, result->affinities, 0, 1);
        Menu_Setup_CombinePhysicalTypeValues(first->types, second->types, result->types, 0);
    } else {
        Menu_Setup_CombineClassOrAffinityValues(first->classes, second->classes, result->classes, 1, 0);
        Menu_Setup_CombineClassOrAffinityValues(
            first->affinities, second->affinities, result->affinities, 1, 1);
        Menu_Setup_CombinePhysicalTypeValues(first->types, second->types, result->types, 1);
    }
    return result;
}

// MENUC.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Setup_Combination_ResetMenucStatics(void)
{
    {
        static const void* initial = NULL;
        memcpy((void*)&Menu_Setup_sydData, &initial, sizeof Menu_Setup_sydData);
    }
    {
        static const char initial[] = { 0, 0xE, 0x18, 0x38, 0xC0, 0x3E, 0xFE };
        memcpy((void*)&Menu_Setup_workshopMaterials, &initial, sizeof Menu_Setup_workshopMaterials);
    }
    {
        static const int initial[] = { VS_BLADE_SYD_LBA, VS_SHIELD_SYD_LBA, VS_ARMOR_SYD_LBA };
        memcpy((void*)&Menu_Setup_loadCombinationRecipesSydLbas, &initial, sizeof Menu_Setup_loadCombinationRecipesSydLbas);
    }
    {
        static const int initial[] = { VS_BLADE_SYD_SIZE, VS_SHIELD_SYD_SIZE, VS_ARMOR_SYD_SIZE };
        memcpy((void*)&Menu_Setup_loadCombinationRecipesSydSizes, &initial, sizeof Menu_Setup_loadCombinationRecipesSydSizes);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_bladeCombinationCursorAnimation, &initial, sizeof Menu_Setup_bladeCombinationCursorAnimation);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_shieldCombinationCursorAnimation, &initial, sizeof Menu_Setup_shieldCombinationCursorAnimation);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_armorCombinationCursorAnimation, &initial, sizeof Menu_Setup_armorCombinationCursorAnimation);
    }
    {
        static const int (*initial[])(
        int) = { Menu_Setup_UpdateBladeCombination, Menu_Setup_UpdateShieldCombination, Menu_Setup_UpdateArmorCombination };
        memcpy((void*)&Menu_Setup_combineMenus, &initial, sizeof Menu_Setup_combineMenus);
    }
    {
        static const short initial[] = { 3, 5, 7 };
        memcpy((void*)&Menu_Setup_combineClassOrAffinityValuesDifferentMaterialStatFactors, &initial, sizeof Menu_Setup_combineClassOrAffinityValuesDifferentMaterialStatFactors);
    }
    {
        static const short initial[] = { 4, 6, 8 };
        memcpy((void*)&Menu_Setup_combineClassOrAffinityValuesSameMaterialStatFactors, &initial, sizeof Menu_Setup_combineClassOrAffinityValuesSameMaterialStatFactors);
    }
    {
        static const short initial[] = { 4, 6 };
        memcpy((void*)&Menu_Setup_combinePhysicalTypeValuesDifferentMaterialTypeFactors, &initial, sizeof Menu_Setup_combinePhysicalTypeValuesDifferentMaterialTypeFactors);
    }
    {
        static const short initial[] = { 6, 8 };
        memcpy((void*)&Menu_Setup_combinePhysicalTypeValuesSameMaterialTypeFactors, &initial, sizeof Menu_Setup_combinePhysicalTypeValuesSameMaterialTypeFactors);
    }
}

