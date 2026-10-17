// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Hud.h"
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Field/RoomRendering.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loot.h"
#include "Items/Weapons.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemDetails.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Loot/LootInventory.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "State/Settings.h"

// decomp: D_8010A6B7 (D_8010A6B7@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_previewedLoot;

// decomp: _controlsEnabled (_controlsEnabled@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_controlsEnabled = 0;

// decomp: _gradientStateIndex (_gradientStateIndex@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_gradientStateIndex = 0;

// decomp: _inventory (_inventory@src/MENU/MENUB.PRG/260.c)
Items_Inventory* Menu_Loot_inventory;

// decomp: _inventoryBackup (_inventoryBackup@src/MENU/MENUB.PRG/260.c)
Items_Inventory* Menu_Loot_inventoryBackup;

// decomp: _inventoryCapacityFlyinTimer (_inventoryCapacityFlyinTimer@src/MENU/MENUB.PRG/260.c)
u_char Menu_Loot_inventoryCapacityFlyinTimer;

// decomp: _itemCategoryHeaders (_itemCategoryHeaders@src/MENU/MENUB.PRG/260.c)
char* Menu_Loot_itemCategoryHeaders[] = { "WEAPON", "BLADE", "GRIP", "SHIELD", "ARMOR",
    "GEM", "MISC" };

// decomp: _lootClaimed (_lootClaimed@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_lootClaimed;

// decomp: _lootLeftEdge (_lootLeftEdge@src/MENU/MENUB.PRG/260.c)
u_char Menu_Loot_lootLeftEdge;

// decomp: _lootList (_lootList@src/MENU/MENUB.PRG/260.c)
Menu_Loot_LootListItem* Menu_Loot_lootList;

// decomp: _lootListCount (_lootListCount@src/MENU/MENUB.PRG/260.c)
u_char Menu_Loot_lootListCount;

// decomp: _lootListOffset (_lootListOffset@src/MENU/MENUB.PRG/260.c)
u_char Menu_Loot_lootListOffset;

// decomp: _lootListWorkingCopy (_lootListWorkingCopy@src/MENU/MENUB.PRG/260.c)
Menu_Loot_LootListItem* Menu_Loot_lootListWorkingCopy;

// decomp: _selectedLoot (_selectedLoot@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_selectedLoot;

// decomp: _showInventory (_showInventory@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_showInventory;

// decomp: _showLoot (_showLoot@src/MENU/MENUB.PRG/260.c)
char Menu_Loot_showLoot;

// decomp: local static of _organizeInventory, at file scope so the module reset reaches it
static char Menu_Loot_organizationMenuCursorAnimState = 0;

// decomp: local static of _discardItems, at file scope so the module reset reaches it
static char Menu_Loot_leaveConfirmationCursorAnimStep = 0;

// decomp: local static of _consolidateMiscItems, at file scope so the module reset reaches it
static char Menu_Loot_stackConsolidationCursorMemory = 0;

// decomp: local static of _disassembleItem, at file scope so the module reset reaches it
static char Menu_Loot_disassemblyCursorMemory = 0;

// decomp: local static of _processLootMenu, at file scope so the module reset reaches it
static char Menu_Loot_lootSelectionCursorMemory = 0;

// decomp: MENUB.PRG _topLevelMenuTransition (src/MENU/MENUB.PRG/260.c:61)
int Menu_Loot_UpdateInventoryTransition(int category)
{
    enum Menu_Framework_InitOnlyState { Menu_Framework_InitOnlyState_Init };

    static char Menu_Loot_inventoryTransitionState;
    static char Menu_Loot_hudWasHidden;

    int stepValue;
    Menu_Framework_MenuRow* menuItem;

    if (category != 0) {
        Menu_Framework_menuMemory.opaque3A.currentCategory = category;
        Menu_Loot_showInventory = 0;
        Menu_Loot_showLoot = 0;
        Menu_Loot_inventoryTransitionState = Menu_Framework_InitOnlyState_Init;
        return 0;
    }

    switch (Menu_Loot_inventoryTransitionState) {
    case Menu_Framework_InitOnlyState_Init:
        if (!Menu_Framework_AreMenuRowsIdle()) {
            break;
        }

        Menu_Framework_SetBackgroundFade(1);
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);

        menuItem = Menu_Framework_SetMenuItem(3, 320, 18, 0x7E, 8,
            (char*)&Menu_menuStrings
                [Menu_menuStrings[Text_Battle_menuStrings_Index_items]]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 180;
        menuItem->selected = 1;
        Menu_Loot_inventoryTransitionState = 1;
        break;

    case 1:
        Menu_Inventory_SetPanelVisibility(0);

        Menu_Inventory_secondaryTitleAnimationStep = 0;
        Menu_Inventory_itemCategoryIconsAnimationStep = 0;
        Menu_Inventory_statusCommandAnimationStep = 0;
        Menu_Inventory_selectedItemCategoryIconOnTop = 0;

        if (!Menu_Framework_AreMenuRowsIdle()) {
            break;
        }

        Field_SetFieldRenderingEnabled(0);

        Menu_Loot_hudWasHidden = Battle_hudHidden;

        if (!Menu_Loot_hudWasHidden) {
            Battle_SetHudHidden(1);
            Battle_hudSlideOffset = 0x200000;
        }

        Menu_Framework_RenderAshley(1);
        Menu_Inventory_UpdateItemList(1);

        Menu_Loot_inventoryTransitionState = 2;
        break;

    case 2:
        if (Menu_Inventory_UpdateItemList(0) == 0) {
            break;
        }

        Menu_Inventory_SetPanelVisibility(0);
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Field_SetFieldRenderingEnabled(1);

        if (Menu_Loot_hudWasHidden == 0) {
            Battle_SetHudHidden(0);
            Battle_hudSlideOffset = 0;
        }

        Menu_Framework_RenderAshley(2);

        Menu_Loot_inventoryTransitionState = 3;

        if (Input_settings.information == 0) {
            Menu_Framework_DismissInformationBox();
        }

        break;

    case 3:
        Menu_Framework_SetBackgroundFade(0);
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Loot_showInventory = 1;
            Menu_Loot_showLoot = 1;
            return 1;
        }

        break;
    }

    if (Menu_Inventory_secondaryTitleEnabled != 0) {
        if (Menu_Inventory_secondaryTitleAnimationStep < 10) {
            ++Menu_Inventory_secondaryTitleAnimationStep;
        }
        stepValue = Menu_Framework_rowAnimationSteps[10 - Menu_Inventory_secondaryTitleAnimationStep];
    } else {
        if (Menu_Inventory_secondaryTitleAnimationStep != 0) {
            --Menu_Inventory_secondaryTitleAnimationStep;
        }
        stepValue = (10 - Menu_Inventory_secondaryTitleAnimationStep) << 5;
    }

    if ((Menu_Inventory_secondaryTitleAnimationStep != 0) && (Menu_Inventory_selectedItemCategoryIconOnTop == 0)) {

        int itemCategory = (Menu_Framework_menuMemory.opaque3A.currentCategory - 1) & 7;

        Menu_Framework_MenuRow* menuItem =
            Menu_Framework_SetMenuItem(31, stepValue + 180, 34, 140, 8,
                (char*)&Menu_Inventory_menuText[Menu_Inventory_menuText[itemCategory + Text_Menub_menuText_Index_weapons]]);
        menuItem->selected = 1;

        if (itemCategory != 7) {
            menuItem->max = Items_Inventory_inventoryItemCapacities[itemCategory];
            menuItem->count = Items_Inventory_CountItems(itemCategory, NULL);
        }
    }

    Menu_Framework_RenderAshley(0);

    if (Menu_Inventory_itemCategoryIconsEnabled != 0) {
        if (Menu_Inventory_itemCategoryIconsAnimationStep < 4) {
            ++Menu_Inventory_itemCategoryIconsAnimationStep;
        }
    } else if (Menu_Inventory_itemCategoryIconsAnimationStep != 0) {
        --Menu_Inventory_itemCategoryIconsAnimationStep;
    }

    Menu_Inventory_DrawCategoryIcons(Menu_Inventory_itemCategoryIconsAnimationStep, Menu_Inventory_selectedItemCategoryIconOnTop);

    if (Menu_Inventory_statusCommandState != 0) {
        if (Menu_Inventory_statusCommandAnimationStep < 10) {
            ++Menu_Inventory_statusCommandAnimationStep;
        }
        stepValue = Menu_Framework_rowAnimationSteps[10 - Menu_Inventory_statusCommandAnimationStep];
    } else {
        if (Menu_Inventory_statusCommandAnimationStep != 0) {
            --Menu_Inventory_statusCommandAnimationStep;
        }
        stepValue = (10 - Menu_Inventory_statusCommandAnimationStep) << 5;
    }

    if (Menu_Inventory_statusCommandAnimationStep != 0) {
        Menu_Framework_DrawButtonPromptBackground(0x10 - stepValue, 0x26, 0x58, 10);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, 8 - stepValue, 0x24, NULL);
        Render_DrawCompactTextColor("STATUS", ((0x1C - stepValue) & 0xFFFF) | 0x260000,
            0x202020 << Menu_Inventory_statusCommandState, NULL);
    }

    return 0;
}

// decomp: MENUB.PRG _disassembleItem (src/MENU/MENUB.PRG/260.c:232)
int Menu_Loot_UpdateDisassembly(int itemIndex)
{
    enum Menu_Loot_DisassemblyState {
        Menu_Loot_DisassemblyState_Init,
        Menu_Loot_DisassemblyState_InitOptionYes,
        Menu_Loot_DisassemblyState_InitOptionNo,
        Menu_Loot_DisassemblyState_WaitInit,
        Menu_Loot_DisassemblyState_ProcessCommand,
        Menu_Loot_DisassemblyState_Disassemble,
        Menu_Loot_DisassemblyState_Finalize,
        Menu_Loot_DisassemblyState_Leave
    };


    static char Menu_Loot_disassemblyState;
    static char Menu_Loot_disassemblyYesNoChoice;
    static char Menu_Loot_disassemblyLootCount;
    static u_char Menu_Loot_disassemblyTargetItemIndex;
    static char Menu_Loot_disassembleItemCategory;
    static char Menu_Loot_disassemblyLayoutPadding __attribute__((unused));

    int i;
    Menu_Framework_MenuRow* menuItem;

    if (itemIndex != 0) {

        Core_Memory_CopyHalfwordBlocks(
            Menu_Loot_lootListWorkingCopy, Menu_Loot_lootList, 160 * sizeof *Menu_Loot_lootListWorkingCopy);

        Menu_Loot_showLoot = 0;
        Menu_Loot_disassemblyLootCount = Menu_Loot_lootListCount;
        Menu_Loot_disassemblyTargetItemIndex = itemIndex - 1;
        Menu_Loot_disassembleItemCategory = Menu_Loot_lootList[Menu_Loot_disassemblyTargetItemIndex].itemCategory;
        Menu_Loot_selectedLoot = 0;
        Menu_Loot_lootListOffset = 0;
        Menu_Loot_disassemblyYesNoChoice = 0;
        Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_Init;

        return 0;
    }

    i = Menu_Loot_lootListWorkingCopy[Menu_Loot_disassemblyTargetItemIndex].itemIndex;

    switch (Menu_Loot_disassemblyState) {
    case Menu_Loot_DisassemblyState_Init:
        if (Menu_Loot_lootLeftEdge != 6) {
            break;
        }

        Core_Memory_ZeroWordsBackward(Menu_Loot_lootList, 0x280);

        Menu_Loot_lootList->itemCategory = Menu_Loot_lootListWorkingCopy[Menu_Loot_disassemblyTargetItemIndex].itemCategory;
        Menu_Loot_lootList->itemIndex = i;
        Menu_Loot_lootList->state = Menu_Loot_LootItemState_NoIndent;

        if (Menu_Loot_disassembleItemCategory != Items_ItemCategory_Weapon) {

            Items_InventoryShield* shield = &Menu_Loot_inventory->shields[i];
            Menu_Loot_lootList[1].itemCategory = Items_ItemCategory_Shield;
            Menu_Loot_lootList[1].itemIndex = i;
            Menu_Loot_lootList[1].state = Menu_Loot_LootItemState_Indent1;
            Menu_Loot_lootListCount = 2;

            for (i = 0; i < shield->base.gemSlots; ++i) {
                if (shield->gems[i] != 0) {
                    Menu_Loot_lootList[Menu_Loot_lootListCount].itemCategory = Items_ItemCategory_Gem;
                    Menu_Loot_lootList[Menu_Loot_lootListCount].itemIndex = shield->gems[i] - 1;
                    Menu_Loot_lootList[Menu_Loot_lootListCount].state = Menu_Loot_LootItemState_Indent3;
                    ++Menu_Loot_lootListCount;
                }
            }
        } else {
            Items_InventoryWeapon* weapon = &Menu_Loot_inventory->weapons[i];
            Menu_Loot_lootList[1].itemCategory = Items_ItemCategory_Blade;
            Menu_Loot_lootList[1].itemIndex = weapon->blade - 1;
            Menu_Loot_lootList[1].state = Menu_Loot_LootItemState_Indent2;
            Menu_Loot_lootList[2].itemCategory = Items_ItemCategory_Grip;
            Menu_Loot_lootList[2].itemIndex = weapon->grip - 1;
            Menu_Loot_lootList[2].state = Menu_Loot_LootItemState_Indent2;
            Menu_Loot_lootListCount = 3;

            for (i = 0; i < Menu_Loot_inventory->grips[weapon->grip - 1].gemSlots; ++i) {
                if (weapon->gems[i] != 0) {
                    Menu_Loot_lootList[Menu_Loot_lootListCount].itemCategory = Items_ItemCategory_Gem;
                    Menu_Loot_lootList[Menu_Loot_lootListCount].itemIndex = weapon->gems[i] - 1;
                    Menu_Loot_lootList[Menu_Loot_lootListCount].state = Menu_Loot_LootItemState_Indent3;
                    ++Menu_Loot_lootListCount;
                }
            }
        }

        Menu_Loot_lootLeftEdge = 10;
        Menu_Loot_showLoot = 1;
        Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_InitOptionYes;
        break;

    case Menu_Loot_DisassemblyState_InitOptionYes:
        menuItem = Menu_Framework_SetMenuItem(
            0x1E, 0x140, 0x92, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionYes]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 0xC2;
        Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_InitOptionNo;
        break;

    case Menu_Loot_DisassemblyState_InitOptionNo:
        menuItem = Menu_Framework_SetMenuItem(
            0x1F, 0x140, 0xA2, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionNo]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 0xC2;
        Battle_stringContext.strings[0] =
            Menu_Loot_disassembleItemCategory == 0
                ? Menu_Loot_inventory->weapons[i].name
                : Menu_Framework_itemNames[Menu_Loot_inventory->shields[i].base.id];
        Menu_InitInformationTextBox(1);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_disassembleConfirm]);
        Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_WaitInit;
        break;

    case Menu_Loot_DisassemblyState_WaitInit:
        Menu_Loot_disassemblyState += Menu_Framework_AreMenuRowsIdle();
        break;

    case Menu_Loot_DisassemblyState_ProcessCommand:
        Menu_Framework_MenuItem(30 + Menu_Loot_disassemblyYesNoChoice)->selected = 1;
        Menu_Framework_MenuItem(31 - Menu_Loot_disassemblyYesNoChoice)->selected = 0;

        if (Input_buttonsPressed.allButtons & (PADRright | PADRdown)) {
            Menu_Framework_FlyOutRowRight(30);
            Menu_Framework_FlyOutRowRight(31);
            Menu_Loot_showLoot = 0;
            if ((Menu_Loot_disassemblyYesNoChoice != 0) || (Input_buttonsPressed.allButtons & PADRdown)) {
                Menu_PlayLeaveSound();
                Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_Finalize;
            } else {
                Audio_PlaySfxCentered(0x7E, 0x19);
                Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_Disassemble;
            }
            if (Input_settings.information == 0) {
                Menu_Framework_DismissInformationBox();
            }
            break;
        }

        if (Input_buttonRepeat & (PADLup | PADLdown)) {
            Menu_PlayChangeSound();
            Menu_Loot_disassemblyYesNoChoice = (1 - Menu_Loot_disassemblyYesNoChoice);
        }

        Menu_Loot_disassemblyCursorMemory = Menu_Framework_DrawMainMenuCursor(Menu_Loot_disassemblyCursorMemory, Menu_Loot_disassemblyYesNoChoice + 8);
        break;

    case Menu_Loot_DisassemblyState_Disassemble:
        if (Menu_Loot_lootLeftEdge != 6) {
            break;
        }

        Core_Memory_CopyBytesBackward(&Menu_Loot_lootListWorkingCopy[Menu_Loot_disassemblyLootCount], &Menu_Loot_lootList[1],
            (Menu_Loot_lootListCount - 1) * sizeof Menu_Loot_lootList[1]);

        if (Menu_Loot_disassembleItemCategory != Items_ItemCategory_Weapon) {
            Items_InventoryShield* shield = &Menu_Loot_inventory->shields[i];
            for (i = 0; i < shield->base.gemSlots; ++i) {
                int gem = shield->gems[i];
                if (gem != 0) {
                    shield->gems[i] = 0;
                    Menu_Loot_inventory->gems[gem - 1].setItemIndex = 0;
                }
            }
        } else {
            Items_InventoryWeapon* weapon = &Menu_Loot_inventory->weapons[i];
            Menu_Loot_inventory->blades[weapon->blade - 1].assembledWeaponIndex = 0;
            Menu_Loot_inventory->grips[weapon->grip - 1].assembledWeaponIndex = 0;

            for (i = 0; i < Menu_Loot_inventory->grips[weapon->grip - 1].gemSlots; ++i) {
                int gem = weapon->gems[i];
                if (gem != 0) {
                    weapon->gems[i] = 0;
                    Menu_Loot_inventory->gems[gem - 1].setItemIndex = 0;
                }
            }
        }

        Menu_Loot_disassemblyLootCount = Menu_Loot_lootListCount + (Menu_Loot_disassemblyLootCount + 254);

        for (i = Menu_Loot_disassemblyTargetItemIndex; i < Menu_Loot_disassemblyLootCount; ++i) {
            Menu_Loot_lootListWorkingCopy[i] = Menu_Loot_lootListWorkingCopy[i + 1];
        }
        // Fallthrough

    case Menu_Loot_DisassemblyState_Finalize:
        if (Menu_Loot_lootLeftEdge != 6) {
            break;
        }

        Menu_Loot_lootListCount = Menu_Loot_disassemblyLootCount;

        Core_Memory_CopyHalfwordBlocks(
            Menu_Loot_lootList, Menu_Loot_lootListWorkingCopy, 160 * sizeof *Menu_Loot_lootListWorkingCopy);
        Menu_Loot_RefreshClaimableRows();

        Menu_Loot_lootLeftEdge = 10;
        Menu_Loot_showLoot = 1;
        Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_Leave;
        break;

    case Menu_Loot_DisassemblyState_Leave:
        if (Menu_Loot_lootLeftEdge != 0) {
            break;
        }
        return -1;
    }

    return 0;
}

// decomp: MENUB.PRG _consolidateMiscItems (src/MENU/MENUB.PRG/260.c:458)
int Menu_Loot_UpdateStackConsolidation(int lootIndex)
{

    static u_short Menu_Loot_stackConsolidationTotalCount;
    static u_short Menu_Loot_stackConsolidationAmountToAdd;
    static u_short Menu_Loot_stackConsolidationItemId;
    static char Menu_Loot_stackConsolidationState;
    static char Menu_Loot_stackConsolidationYesNoChoice;
    static u_char Menu_Loot_stackConsolidationLootCount;

    Menu_Loot_LootListItem* loot;
    int i;
    int j;
    int var_v0;
    Items_InventoryMisc* item;
    Menu_Framework_MenuRow* menuItem;

    if (lootIndex != 0) {

        Core_Memory_CopyHalfwordBlocks(
            Menu_Loot_lootListWorkingCopy, Menu_Loot_lootList, 160 * sizeof *Menu_Loot_lootListWorkingCopy);
        Core_Memory_CopyHalfwordBlocks(Menu_Loot_inventoryBackup, Menu_Loot_inventory, sizeof *Menu_Loot_inventory);

        Menu_Loot_showLoot = 0;
        Menu_Loot_stackConsolidationLootCount = Menu_Loot_lootListCount;
        Menu_Loot_stackConsolidationItemId = Menu_Loot_inventory->misc[Menu_Loot_lootList[lootIndex - 1].itemIndex].id;
        Menu_Loot_selectedLoot = 0;
        Menu_Loot_lootListOffset = 0;
        Menu_Loot_stackConsolidationYesNoChoice = 0;
        Menu_Loot_stackConsolidationState = 0;
        return 0;
    }

    switch (Menu_Loot_stackConsolidationState) {
    case 0:
        if (Menu_Loot_lootLeftEdge == 6) {

            Menu_Loot_stackConsolidationAmountToAdd = 0;
            item = Audio_Seq_unusedInventory.misc;

            for (i = 0; i < 64; ++i, ++item) {
                if (item->id == Menu_Loot_stackConsolidationItemId) {
                    int v0 = Menu_Loot_stackConsolidationAmountToAdd + 100;
                    v0 -= item->count;
                    Menu_Loot_stackConsolidationAmountToAdd = v0;
                }
            }

            Menu_Loot_stackConsolidationTotalCount = 0;

            for (i = 0; i < Menu_Loot_lootListCount; ++i) {
                if (Menu_Loot_lootList[i].itemCategory == Items_ItemCategory_Misc) {
                    item = &Menu_Loot_inventory->misc[Menu_Loot_lootList[i].itemIndex];
                    if (item->id == Menu_Loot_stackConsolidationItemId) {

                        Menu_Loot_stackConsolidationTotalCount += item->count;

                        Core_Memory_ZeroWordsBackward(item, sizeof *item);

                        --Menu_Loot_lootListCount;

                        for (j = i; j < Menu_Loot_lootListCount; ++j) {
                            Menu_Loot_lootList[j] = Menu_Loot_lootList[j + 1];
                        }
                        --i;
                    }
                }
            }

            Menu_Loot_lootListCount = 0;

            while (Menu_Loot_stackConsolidationTotalCount != 0) {

                for (i = 0; i < 64; ++i) {
                    if (Menu_Loot_inventory->misc[i].id == 0) {
                        break;
                    }
                }

                if (i == 64) {
                    break;
                }

                loot = &Menu_Loot_lootList[Menu_Loot_lootListCount++];
                loot->itemCategory = Items_ItemCategory_Misc;
                loot->itemIndex = i;
                loot->opaque2 = 0;
                Menu_Loot_inventory->misc[i].id = Menu_Loot_stackConsolidationItemId;
                loot->state = Menu_Loot_stackConsolidationAmountToAdd != 0;

                if (loot->state != Menu_Loot_LootItemState_None) {
                    if (Menu_Loot_stackConsolidationAmountToAdd > 100) {
                        Menu_Loot_inventory->misc[i].count = 100;
                        Menu_Loot_stackConsolidationTotalCount -= 100;
                        Menu_Loot_stackConsolidationAmountToAdd -= 100;
                    } else {
                        Menu_Loot_inventory->misc[i].count = Menu_Loot_stackConsolidationAmountToAdd;
                        Menu_Loot_stackConsolidationTotalCount = Menu_Loot_stackConsolidationTotalCount - Menu_Loot_stackConsolidationAmountToAdd;
                        Menu_Loot_stackConsolidationAmountToAdd = 0;
                    }
                } else if (Menu_Loot_stackConsolidationTotalCount > 100) {
                    Menu_Loot_inventory->misc[i].count = 100;
                    Menu_Loot_stackConsolidationTotalCount -= 100;
                } else {
                    Menu_Loot_inventory->misc[i].count = Menu_Loot_stackConsolidationTotalCount;
                    Menu_Loot_stackConsolidationTotalCount = 0;
                }
            }

            Menu_Loot_lootLeftEdge = 10;
            Menu_Loot_showLoot = 1;
            Menu_Loot_stackConsolidationState = 1;
        }
        break;

    case 1:
        menuItem = Menu_Framework_SetMenuItem(
            30, 320, 146, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionYes]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        Menu_Loot_stackConsolidationState = 2;
        break;

    case 2:
        menuItem = Menu_Framework_SetMenuItem(
            31, 320, 162, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionNo]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        Battle_stringContext.strings[0] = Menu_Framework_itemNames[Menu_Loot_stackConsolidationItemId];

        Menu_InitInformationTextBox(1);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_organizeConfirm]);

        Menu_Loot_stackConsolidationState = 3;
        break;

    case 3:
        Menu_Loot_stackConsolidationState += Menu_Framework_AreMenuRowsIdle();
        break;

    case 4:
        Menu_Framework_MenuItem(30 + Menu_Loot_stackConsolidationYesNoChoice)->selected = 1;
        Menu_Framework_MenuItem(31 - Menu_Loot_stackConsolidationYesNoChoice)->selected = 0;

        if (Input_buttonsPressed.allButtons & 0x60) {

            Menu_Framework_FlyOutRowRight(30);
            Menu_Framework_FlyOutRowRight(31);

            Menu_Loot_showLoot = 0;

            if ((Menu_Loot_stackConsolidationYesNoChoice != 0) || (Input_buttonsPressed.allButtons & PADRdown)) {
                Menu_PlayLeaveSound();
                Menu_Loot_stackConsolidationState = 6;
            } else {
                Menu_PlaySelectSound();
                Menu_Loot_stackConsolidationState = 5;
            }

            var_v0 = 0;

            if (Input_settings.information == 0) {
                Menu_Framework_DismissInformationBox();
            }

            break;
        }

        if (Input_buttonRepeat & (PADLup | PADLdown)) {
            Menu_PlayChangeSound();
            Menu_Loot_stackConsolidationYesNoChoice = 1 - Menu_Loot_stackConsolidationYesNoChoice;
        }

        Menu_Loot_stackConsolidationCursorMemory = Menu_Framework_DrawMainMenuCursor(Menu_Loot_stackConsolidationCursorMemory, Menu_Loot_stackConsolidationYesNoChoice + 8);
        break;

    case 5:
        if (Menu_Loot_lootLeftEdge != 6) {
            break;
        }

        for (i = 0; i < Menu_Loot_stackConsolidationLootCount; ++i) {
            if (Menu_Loot_lootListWorkingCopy[i].itemCategory == 6) {

                item = &Menu_Loot_inventoryBackup->misc[Menu_Loot_lootListWorkingCopy[i].itemIndex];

                if (item->id == Menu_Loot_stackConsolidationItemId) {

                    Core_Memory_ZeroWordsBackward(item, 4);

                    --Menu_Loot_stackConsolidationLootCount;

                    for (j = i; j < Menu_Loot_stackConsolidationLootCount; ++j) {
                        Menu_Loot_lootListWorkingCopy[j] = Menu_Loot_lootListWorkingCopy[j + 1];
                    }
                    --i;
                }
            }
        }

        for (i = 0; i < Menu_Loot_lootListCount; ++i) {
            for (j = 0; j < 64; ++j) {
                if (Menu_Loot_inventoryBackup->misc[j].id == 0) {
                    break;
                }
            }

            if (j == 0x40) {
                break;
            }

            loot = &Menu_Loot_lootListWorkingCopy[Menu_Loot_stackConsolidationLootCount++];
            loot->itemCategory = Items_ItemCategory_Misc;
            loot->itemIndex = j;
            loot->opaque2 = 0;
            loot->state = Menu_Loot_lootList[i].state;
            item = &Menu_Loot_inventoryBackup->misc[j];
            item->id = Menu_Loot_stackConsolidationItemId;
            item->count = Menu_Loot_inventory->misc[Menu_Loot_lootList[i].itemIndex].count;
        }
        // Fallthrough

    case 6:
        if (Menu_Loot_lootLeftEdge == 6) {

            Menu_Loot_lootListCount = Menu_Loot_stackConsolidationLootCount;

            Core_Memory_CopyHalfwordBlocks(Menu_Loot_lootList, Menu_Loot_lootListWorkingCopy, sizeof *Menu_Loot_lootList * 160);
            Core_Memory_CopyHalfwordBlocks(Menu_Loot_inventory, Menu_Loot_inventoryBackup, sizeof *Menu_Loot_inventory);

            Menu_Loot_lootLeftEdge = 10;
            Menu_Loot_showLoot = 1;
            Menu_Loot_stackConsolidationState = 7;
        }
        break;

    case 7:
        if (Menu_Loot_lootLeftEdge != 0) {
            break;
        }
        return -1;
    }

    return 0;
}

// decomp: MENUB.PRG _displayInventoryCapacities (src/MENU/MENUB.PRG/260.c:1015)
void Menu_Loot_RenderBagCapacities(int edgeX)
{
    int itemCountPos;
    int buttonBgPos;
    int var_a2;
    int var_fp;
    int i;
    int y;
    Menu_Loot_LootListItem* item;
    unsigned int xy;
    char* str;

    item = NULL;
    var_fp = 1;

    if (Menu_Loot_previewedLoot != 0) {

        item = (Menu_Loot_LootListItem*)1;

        if (Menu_Loot_previewedLoot & 0xF) {

            var_fp = 0;
            item = &Menu_Loot_lootList[Menu_Loot_lootListOffset + Menu_Loot_previewedLoot - 1];

            for (i = 0; i < Menu_Loot_lootListCount; ++i) {
                var_fp += Menu_Loot_lootList[i].state;
            }
        }
    }

    Render_DrawCompactText("GADGET BAG", (edgeX + 176) | (12 << 16), NULL);

    itemCountPos = edgeX + 272;

    for (i = 0; i < 7; ++i) {
        int color = vs_getRGB(128, 128, 128);
        int capacity = Items_Inventory_inventoryItemCapacities[i];
        int itemCount = Items_Inventory_CountItems(i, NULL);

        if (item != NULL) {
            color = vs_getRGB(64, 64, 64)
                 << ((item == (Menu_Loot_LootListItem*)1)
                        || (Menu_Loot_CountContainedCategory(i, item) != 0));
        }

        y = (24 + i * 12) << 16;

        Render_DrawCompactTextColor("/", itemCountPos | y, color, NULL);
        Render_DrawCompactTextColor(
            Menu_Loot_itemCategoryHeaders[i], (edgeX + 192) | y, color, NULL);
        Menu_Framework_DrawColoredInteger(capacity, (edgeX + 296) | y, color, NULL);

        if (((u_int)color >> 23) != 0) {

            if ((itemCount * 2) < capacity) {
                color = vs_getRGB(32, 64, 128);
            }

            if (((uintptr_t)item >> 1) != 0) {
                if (capacity < (Menu_Loot_CountContainedCategory(i, item) + itemCount)) {
                    color = vs_getRGB(128, 64, 32);
                }
            }
        }

        Menu_Framework_DrawColoredInteger(itemCount, itemCountPos | y, color, NULL);
    }

    buttonBgPos = edgeX + 184;

    Menu_Framework_DrawButtonPromptBackground(buttonBgPos, 112, 112, 10);
    Menu_Framework_DrawButtonPromptBackground(buttonBgPos, 128, 64, 10);
    Render_DrawCompactTextColor("GET", (edgeX + 196) | (112 << 16),
        vs_getRGB(64, 64, 64) << Menu_Loot_controlsEnabled, NULL);
    Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Circle, edgeX + 176, 110, NULL);

    str = "ALL";
    xy = (edgeX + 252) | (112 << 16);

    if (var_fp != 0) {
        var_a2 = vs_getRGB(64, 64, 64) << (Menu_Loot_controlsEnabled & 1);
    } else {
        var_a2 = vs_getRGB(64, 64, 64);
    }

    Render_DrawCompactTextColor(str, xy, var_a2, NULL);
    Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, edgeX + 232, 110, NULL);
    Render_DrawCompactTextColor("END", (edgeX + 196) | (128 << 16),
        vs_getRGB(64, 64, 64) << Menu_Loot_controlsEnabled, NULL);
    Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Cross, edgeX + 176, 126, NULL);
}

// decomp: MENUB.PRG _displayCurrentLoot (src/MENU/MENUB.PRG/260.c:1192)
void Menu_Loot_RenderLootRows(int x)
{
    char gradientStates[256];
    int i;
    int index;
    int temp_v1_3;
    Menu_Framework_MenuRow* menuItem;

    int count = Menu_Loot_lootListCount;
    Menu_Loot_LootListItem* loot = &Menu_Loot_lootList[Menu_Loot_lootListOffset];

    Core_Memory_ZeroWordsBackward(&gradientStates, sizeof gradientStates);

    for (i = 0; i < 8; ++i) {
        menuItem = Menu_Framework_MenuItem(i + 32);
        gradientStates[i + Menu_Loot_gradientStateIndex] = menuItem->gradientState;
        menuItem->state = 0;
    }

    if (count > 8) {
        count = 8;
    }

    for (i = 0; i < count;) {
        index = loot->itemIndex;
        menuItem = Menu_Framework_MenuItem(32 + i);

        switch (loot->itemCategory) {
        case Items_ItemCategory_Weapon: {
            Items_InventoryWeapon* weapon = &Menu_Loot_inventory->weapons[index];
            Items_InventoryBlade* blade = &Menu_Loot_inventory->blades[weapon->blade - 1];
            menuItem =
                Menu_Framework_SetMenuItem(32 + i, 24 - x, 50 + i * 16, 152, 0, weapon->name);
            menuItem->rowIcon = blade->category;
            menuItem->material = blade->material;
            break;
        }
        case Items_ItemCategory_Blade: {
            Items_InventoryBlade* blade2 = &Menu_Loot_inventory->blades[index];
            menuItem = Menu_Framework_SetMenuItem(
                32 + i, 24 - x, 50 + i * 16, 152, 0, Menu_Framework_itemNames[blade2->id]);
            menuItem->rowIcon = blade2->category;
            menuItem->material = blade2->material;
            break;
        }
        case Items_ItemCategory_Grip: {
            Items_InventoryGrip* grip = &Menu_Loot_inventory->grips[index];
            menuItem = Menu_Framework_SetMenuItem(
                32 + i, 24 - x, 50 + i * 16, 152, 0, Menu_Framework_itemNames[grip->id]);
            menuItem->rowIcon = grip->category + 10;
            break;
        }
        case Items_ItemCategory_Shield: {
            Items_InventoryShield* shield = &Menu_Loot_inventory->shields[index];
            menuItem = Menu_Framework_SetMenuItem(32 + i, 24 - x, 50 + i * 16, 152, 0,
                Menu_Framework_itemNames[shield->base.id]);
            menuItem->rowIcon = 15;
            menuItem->material = shield->base.material;
            break;
        }
        case Items_ItemCategory_Armor: {
            Items_InventoryArmor* armor = &Menu_Loot_inventory->armor[index];
            menuItem = Menu_Framework_SetMenuItem(
                32 + i, 24 - x, 50 + i * 16, 152, 0, Menu_Framework_itemNames[armor->id]);
            menuItem->rowIcon = armor->category + 0xE;
            menuItem->material = armor->material;
            break;
        }
        case Items_ItemCategory_Gem: {
            Items_InventoryGem* gem = &Menu_Loot_inventory->gems[index];
            menuItem = Menu_Framework_SetMenuItem(
                32 + i, 24 - x, 50 + i * 16, 152, 0, Menu_Framework_itemNames[gem->id]);
            menuItem->rowIcon = 22;
            break;
        }
        case Items_ItemCategory_Misc: {
            Items_InventoryMisc* misc = &Menu_Loot_inventory->misc[index];
            menuItem = Menu_Framework_SetMenuItem(
                32 + i, 24 - x, 50 + i * 16, 152, 0, Menu_Framework_itemNames[misc->id]);
            menuItem->count = misc->count;
            menuItem->fontColor = (misc->id < 458) ^ 1;
            break;
        }
        }

        menuItem->gradientState = gradientStates[i + Menu_Loot_lootListOffset];
        menuItem->unselectable = loot->state == 0;

        if (loot->opaque2 != 0) {
            menuItem->x -= loot->opaque2 * 48;
            ++loot->opaque2;
            if (loot->opaque2 == 4) {
                menuItem->gradientState = 0;
            }
        }

        if (i == 0) {
            if (Menu_Loot_lootListOffset != 0) {
                menuItem->isScrollable = 1;
            }
        }

        if ((i == 7) && (Menu_Loot_lootListOffset != (Menu_Loot_lootListCount - 8))) {
            menuItem->isScrollable = 2;
        }

        temp_v1_3 = loot->state;

        if (temp_v1_3 == 2) {
            menuItem->backgroundWidth = 0x18;
        }

        if (temp_v1_3 > 2) {
            menuItem->x += (temp_v1_3 - 3) * 7;
        }

        ++i;
        ++loot;
    }

    Menu_Loot_gradientStateIndex = Menu_Loot_lootListOffset;
}

// decomp: MENUB.PRG _populateLootItem (src/MENU/MENUB.PRG/260.c:1319)
void Menu_Loot_ShowLootDescription(int lootIndex)
{
    char* menuText[2];
    int rowType;
    int itemIndex = Menu_Loot_lootList[lootIndex].itemIndex;

    Core_Memory_CopyHalfwordBlocks(Menu_Loot_inventoryBackup, &Audio_Seq_unusedInventory, sizeof *Menu_Loot_inventoryBackup);
    Core_Memory_CopyHalfwordBlocks(&Audio_Seq_unusedInventory, Menu_Loot_inventory, sizeof Audio_Seq_unusedInventory);

    switch (Menu_Loot_lootList[lootIndex].itemCategory) {
    case Items_ItemCategory_Weapon:
        Menu_Framework_FormatInventoryWeaponRow(&Audio_Seq_unusedInventory.weapons[itemIndex],
            menuText, &rowType, Menu_Framework_stringBuf);
        break;

    case Items_ItemCategory_Blade:
        Menu_Framework_FormatBladeRow(&Audio_Seq_unusedInventory.blades[itemIndex], menuText, &rowType,
            Menu_Framework_stringBuf);
        break;

    case Items_ItemCategory_Grip:
        Menu_Framework_FormatGripRow(
            &Audio_Seq_unusedInventory.grips[itemIndex], menuText, &rowType, Menu_Framework_stringBuf);
        break;

    case Items_ItemCategory_Shield:
        Menu_Framework_FormatInventoryShieldRow(&Audio_Seq_unusedInventory.shields[itemIndex],
            menuText, &rowType, Menu_Framework_stringBuf);
        break;

    case Items_ItemCategory_Armor:
        Menu_Framework_FormatInventoryArmorRow(
            &Audio_Seq_unusedInventory.armor[itemIndex], menuText, &rowType, Menu_Framework_stringBuf);
        break;

    case Items_ItemCategory_Gem:
        Menu_Framework_FormatGemRow(
            &Audio_Seq_unusedInventory.gems[itemIndex], menuText, &rowType, Menu_Framework_stringBuf);
        break;

    case Items_ItemCategory_Misc:
        Menu_Framework_FormatMiscItemRow(
            &Audio_Seq_unusedInventory.misc[itemIndex], menuText, &rowType, Menu_Framework_stringBuf);
        break;
    }

    Menu_Framework_SetInformationMessage(menuText[1]);
    Core_Memory_CopyHalfwordBlocks(&Audio_Seq_unusedInventory, Menu_Loot_inventoryBackup, sizeof Audio_Seq_unusedInventory);
}

// decomp: MENUB.PRG _discardItems (src/MENU/MENUB.PRG/260.c:1377)
int Menu_Loot_UpdateLeaveConfirmation(int init)
{

    static char Menu_Loot_leaveConfirmationState;
    static char Menu_Loot_leaveConfirmationYesNoChoice;
    static char Menu_Loot_leaveConfirmationLayoutPadding __attribute__((unused));
    static char* Menu_Loot_leaveConfirmationItemName;

    Menu_Framework_MenuRow* menuItem;

    if (init != 0) {
        int i;

        Menu_Loot_leaveConfirmationYesNoChoice = 1;
        Menu_Loot_leaveConfirmationState = 0;

        if (Menu_Loot_lootListCount == 0) {
            Menu_Loot_leaveConfirmationState = 4;
            return 0;
        }

        if ((Menu_Loot_lootClaimed == 0) && (Menu_Framework_lootEntry != NULL)) {
            Menu_Loot_leaveConfirmationState = 5;
            return 0;
        }

        for (i = 0; i < Menu_Loot_lootListCount; ++i) {
            if ((Menu_Loot_lootList[i].itemCategory == Items_ItemCategory_Misc)
                && (Menu_Loot_inventory->misc[Menu_Loot_lootList[i].itemIndex].id > 457)) {
                Menu_Loot_leaveConfirmationItemName =
                    Menu_Framework_itemNames[Menu_Loot_inventory->misc[Menu_Loot_lootList[i].itemIndex].id];
                Menu_Loot_leaveConfirmationState = 6;
            }
        }
        return 0;
    }

    switch (Menu_Loot_leaveConfirmationState) {
    case 0:
        menuItem = Menu_Framework_SetMenuItem(
            30, 0x140, 0x92, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionYes]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 0xC2;
        Menu_Loot_leaveConfirmationState = 1;
        break;

    case 1:
        menuItem = Menu_Framework_SetMenuItem(
            31, 0x140, 0xA2, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionNo]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 0xC2;

        Menu_InitInformationTextBox(1);
        Menu_Framework_SetInformationMessage(
            Menu_Loot_lootClaimed == 0 ? (char*)(&Menu_Inventory_menuText[Text_Menub_menuText_Offset_discardAllConfirm])
                              : (char*)(&Menu_Inventory_menuText[Text_Menub_menuText_Offset_itemsRemaining]));
        Menu_Loot_leaveConfirmationState = 2;
        break;

    case 2:
        Menu_Loot_leaveConfirmationState = (Menu_Loot_leaveConfirmationState + Menu_Framework_AreMenuRowsIdle());
        break;

    case 3:
        Menu_Framework_MenuItem(30 + Menu_Loot_leaveConfirmationYesNoChoice)->selected = 1;
        Menu_Framework_MenuItem(31 - Menu_Loot_leaveConfirmationYesNoChoice)->selected = 0;

        if (Input_buttonsPressed.allButtons & (PADRright | PADRdown)) {

            Menu_Framework_FlyOutRowRight(30);
            Menu_Framework_FlyOutRowRight(31);

            if (Input_settings.information == 0) {
                Menu_Framework_DismissInformationBox();
            }

            if ((Menu_Loot_leaveConfirmationYesNoChoice == 0) && !(Input_buttonsPressed.allButtons & PADRdown)) {
                int var_v1;
                Audio_PlaySfxCentered(0x7E, 0x1C);
                var_v1 = 1;
                if (Menu_Loot_lootClaimed == 0) {
                    var_v1 = 2;
                }
                return var_v1;
            }

            Menu_PlayLeaveSound();

            return -1;
        }

        if (Input_buttonRepeat & (PADLup | PADLdown)) {
            Menu_PlayChangeSound();
            Menu_Loot_leaveConfirmationYesNoChoice = (1 - Menu_Loot_leaveConfirmationYesNoChoice);
        }

        Menu_Loot_leaveConfirmationCursorAnimStep = Menu_Framework_DrawMainMenuCursor(Menu_Loot_leaveConfirmationCursorAnimStep, Menu_Loot_leaveConfirmationYesNoChoice + 8);
        break;

    case 4:
    case 5:
        return Menu_Loot_leaveConfirmationState - 3;

    case 6:
        Battle_stringContext.strings[0] = Menu_Loot_leaveConfirmationItemName;

        Menu_InitInformationTextBox(1);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_invalidDiscard]);

        Menu_Loot_leaveConfirmationState = 7;
        /* fallthrough */

    case 7:
        if (Input_buttonsPressed.ports[0].lowByte != 0) {
            Menu_PlayLeaveSound();
            return -1;
        }
        break;
    }

    return 0;
}

// decomp: MENUB.PRG _organizeInventory (src/MENU/MENUB.PRG/260.c:1510)
int Menu_Loot_UpdateOrganizationMenu(Menu_Loot_LootListItem* loot)
{

    static char Menu_Loot_organizationMenuState;
    static char Menu_Loot_organizationMenuItemCategory;
    static char Menu_Loot_organizationMenuSelectedRow;
    static char Menu_Loot_organizationMenuAvailableAction;

    Menu_Framework_MenuRow* menuItem;

    if (loot != NULL) {
        Menu_Framework_InitializeInformationBox();

        Menu_Loot_organizationMenuItemCategory = loot->itemCategory;
        Menu_Loot_organizationMenuState = 0;
        Menu_Loot_organizationMenuSelectedRow = 0;
        Menu_Loot_organizationMenuAvailableAction = 0;

        if (Menu_Loot_organizationMenuItemCategory == Items_ItemCategory_Weapon) {
            Menu_Loot_organizationMenuAvailableAction = 1;
        } else if (Menu_Loot_organizationMenuItemCategory == Items_ItemCategory_Shield) {
            Menu_Loot_organizationMenuAvailableAction = 1;
        } else if ((Menu_Loot_organizationMenuItemCategory == Items_ItemCategory_Misc)
                   && (Menu_Loot_inventory->misc[loot->itemIndex].id < 458)) {
            Menu_Loot_organizationMenuAvailableAction = 2;
        } else {
            Menu_Loot_organizationMenuSelectedRow = 1;
        }

        if (Menu_Loot_organizationMenuAvailableAction != 0) {
            menuItem = Menu_Framework_SetMenuItem(30, 0x140, 0x92, 0x7E, 0,
                (char*)&Menu_Inventory_menuText[Menu_Inventory_menuText[(Menu_Loot_organizationMenuAvailableAction + 4) * 2]]);
            menuItem->state = 2;
            menuItem->targetPosition0 = 0xC2;
        }

        return 0;
    }

    switch (Menu_Loot_organizationMenuState) {
    case 0:
        menuItem = Menu_Framework_SetMenuItem(31, 0x140, 0xA2, 0x7E, 0,
            (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_organizeInventory]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 0xC2;
        Menu_Loot_organizationMenuState = 1;
        break;

    case 1:
        Menu_Loot_organizationMenuState += Menu_Framework_AreMenuRowsIdle();
        break;

    case 2:
        Menu_Framework_MenuItem(30 + Menu_Loot_organizationMenuSelectedRow)->selected = 1;
        Menu_Framework_MenuItem(31 - Menu_Loot_organizationMenuSelectedRow)->selected = 0;

        if (Input_buttonsPressed.allButtons & (PADRright | PADRdown)) {

            Menu_Framework_FlyOutRowRight(30);
            Menu_Framework_FlyOutRowRight(31);

            if (Input_buttonsPressed.allButtons & PADRdown) {
                Menu_PlayLeaveSound();
                return -1;
            }

            Menu_PlaySelectSound();

            if (Menu_Loot_organizationMenuSelectedRow == 0) {
                Menu_Loot_organizationMenuSelectedRow = Menu_Loot_organizationMenuAvailableAction + 1;
            }

            return Menu_Loot_organizationMenuSelectedRow;
        }

        if (Menu_Loot_organizationMenuAvailableAction != 0) {
            if (Input_buttonRepeat & (PADLup | PADLdown)) {
                Menu_PlayChangeSound();
                Menu_Loot_organizationMenuSelectedRow = 1 - Menu_Loot_organizationMenuSelectedRow;
            }
        }

        if (Menu_Loot_organizationMenuSelectedRow != 0) {
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_organizeInventoryInfo]);
        } else if (Menu_Loot_organizationMenuAvailableAction == 1) {
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_disassembleInfo]);
        } else {
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_organizeInfo]);
        }

        Menu_Loot_organizationMenuCursorAnimState = Menu_Framework_DrawMainMenuCursor(Menu_Loot_organizationMenuCursorAnimState, Menu_Loot_organizationMenuSelectedRow + 8);
        break;
    }
    return 0;
}

// decomp: MENUB.PRG _processLootMenu (src/MENU/MENUB.PRG/260.c:1622)
int Menu_Loot_UpdateLootSelection(int initialize)
{
    enum Menu_Loot_LootSelectionState {
        Menu_Loot_LootSelectionState_Init,
        Menu_Loot_LootSelectionState_ProcessInput,
        Menu_Loot_LootSelectionState_Discard,
        Menu_Loot_LootSelectionState_Organize,
        Menu_Loot_LootSelectionState_TopMenu,
        Menu_Loot_LootSelectionState_Dissasemble,
        Menu_Loot_LootSelectionState_ConsolidateItems,
        Menu_Loot_LootSelectionState_WaitForInput,
        Menu_Loot_LootSelectionState_WaitForTopMenu,
        Menu_Loot_LootSelectionState_UpdateLoot
    };


    static char Menu_Loot_lootSelectionState;
    static char Menu_Loot_lootSelectionUpdateLootDelay;

    Menu_Loot_LootListItem* loot;
    int lootIndex;
    int temp_s2;
    int i;
    int var_v0_4;

    if (initialize != 0) {
        Menu_Loot_selectedLoot = 0;
        Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Init;
        return 0;
    }

    switch (Menu_Loot_lootSelectionState) {
    case Menu_Loot_LootSelectionState_Init:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Menu_Framework_InitializeInformationBox();

        Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_ProcessInput;
        // Fallthrough

    case Menu_Loot_LootSelectionState_ProcessInput:
        Menu_Loot_controlsEnabled = 1;

        if (Input_buttonsPressed.allButtons & PADRdown) {

            Menu_PlayLeaveSound();

            Menu_Loot_controlsEnabled = 0;

            Menu_Loot_UpdateLeaveConfirmation(1);

            Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Discard;
            break;

        } else if (Input_buttonsPressed.allButtons & PADRleft) {

            temp_s2 = Menu_Loot_lootClaimed;

            for (i = 0; i < Menu_Loot_lootListCount; ++i) {

                loot = &Menu_Loot_lootList[i];

                if (Items_Inventory_CopyItem(loot->itemCategory | Menu_Framework_CopyItemFlag_Write,
                        &Audio_Seq_unusedInventory, loot->itemIndex, Menu_Loot_inventory)
                    != 0) {
                    if (loot->itemCategory == Items_ItemCategory_Misc) {

                        int miscId = Menu_Loot_inventory->misc[loot->itemIndex].id;

                        if (miscId > 457) {
                            Menu_Loot_RecordClaimedKey(miscId);
                        }
                    }

                    loot->opaque2 = 1;
                    ++Menu_Loot_lootClaimed;
                }
            }

            if (temp_s2 != Menu_Loot_lootClaimed) {

                Audio_PlaySfxCentered(0x7E, 0xD);

                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_UpdateLoot;
                break;
            }

            Menu_PlayInvalidSound();
        }

        lootIndex = Menu_Loot_selectedLoot + Menu_Loot_lootListOffset;
        loot = &Menu_Loot_lootList[lootIndex];

        if (Input_buttonsPressed.allButtons & PADRright) {
            if (loot->state == 0) {

                Menu_PlaySelectSound();

                Menu_Loot_controlsEnabled = 0;

                Menu_Loot_UpdateOrganizationMenu(loot);

                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Organize;
            } else {

                Audio_PlaySfxCentered(0x7E, 0xD);

                if (loot->itemCategory == Items_ItemCategory_Misc) {

                    i = Menu_Loot_inventory->misc[loot->itemIndex].id;

                    if (i > 457) {
                        Menu_Loot_RecordClaimedKey(i);
                    }
                }

                Items_Inventory_CopyItem(loot->itemCategory | 0x10, &Audio_Seq_unusedInventory,
                    loot->itemIndex, Menu_Loot_inventory);

                loot->opaque2 = 1;
                Menu_Loot_lootSelectionUpdateLootDelay = 3;
                ++Menu_Loot_lootClaimed;
                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_UpdateLoot;
            }
        } else {
            i = lootIndex;

            if (Input_buttonRepeat & PADLup) {
                if (Menu_Loot_lootListCount < 9) {
                    if (Menu_Loot_selectedLoot == 0) {
                        Menu_Loot_selectedLoot = Menu_Loot_lootListCount - 1;
                    } else {
                        --Menu_Loot_selectedLoot;
                    }
                } else {
                    if (Menu_Loot_lootListOffset == 0) {
                        if (Menu_Loot_selectedLoot == 0) {
                            if (Input_buttonsPressed.allButtons & PADLup) {
                                Menu_Loot_selectedLoot = 7;
                                Menu_Loot_lootListOffset = Menu_Loot_lootListCount - 8;
                            }
                        } else {
                            --Menu_Loot_selectedLoot;
                        }
                    } else {
                        if (Menu_Loot_selectedLoot == 1) {
                            --Menu_Loot_lootListOffset;
                        } else {
                            --Menu_Loot_selectedLoot;
                        }
                    }
                }
            }

            if (Input_buttonRepeat & PADLdown) {
                if (Menu_Loot_lootListCount < 9) {
                    if (Menu_Loot_selectedLoot == (Menu_Loot_lootListCount - 1)) {
                        Menu_Loot_selectedLoot = 0;
                    } else {
                        ++Menu_Loot_selectedLoot;
                    }
                } else if (Menu_Loot_lootListOffset == (Menu_Loot_lootListCount - 8)) {
                    if (Menu_Loot_selectedLoot == 7) {
                        if (Input_buttonsPressed.allButtons & PADLdown) {
                            Menu_Loot_selectedLoot = 0;
                            Menu_Loot_lootListOffset = 0;
                        }
                    } else {
                        ++Menu_Loot_selectedLoot;
                    }
                } else if (Menu_Loot_selectedLoot == 6) {
                    ++Menu_Loot_lootListOffset;
                } else {
                    ++Menu_Loot_selectedLoot;
                }
            }

            if (i != (Menu_Loot_selectedLoot + Menu_Loot_lootListOffset)) {
                Menu_PlayChangeSound();
            }

            Menu_Loot_ShowLootDescription(Menu_Loot_selectedLoot + Menu_Loot_lootListOffset);

            Menu_Loot_lootSelectionCursorMemory = Menu_Framework_DrawCursor(
                Menu_Loot_lootSelectionCursorMemory, (((Menu_Loot_selectedLoot * 0x10) + 0x2A) << 0x10) | 0xA);
        }
        break;

    case Menu_Loot_LootSelectionState_Discard:
        i = Menu_Loot_UpdateLeaveConfirmation(0);

        if (i != 0) {
            if (i < 0) {
                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_WaitForTopMenu;
            } else {
                return i;
            }
        }
        break;

    case Menu_Loot_LootSelectionState_Organize:
        i = Menu_Loot_UpdateOrganizationMenu(NULL);

        if (i != 0) {

            if (i < 0) {
                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Init;
                break;
            }

            temp_s2 = Menu_Loot_selectedLoot + Menu_Loot_lootListOffset;

            switch (i) {
            case 1:
                Menu_Loot_UpdateInventoryTransition(Menu_Loot_lootList[temp_s2].itemCategory + 1);

                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_TopMenu;
                break;

            case 2:
                Menu_Loot_UpdateDisassembly(temp_s2 + 1);

                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Dissasemble;
                break;

            case 3:
                Menu_Loot_UpdateStackConsolidation(temp_s2 + 1);

                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_ConsolidateItems;
                break;
            }
        }

        break;

    case Menu_Loot_LootSelectionState_TopMenu:
        var_v0_4 = Menu_Loot_UpdateInventoryTransition(0);

        if (var_v0_4 != 0) {
            Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_WaitForTopMenu;
        }

        break;

    case Menu_Loot_LootSelectionState_Dissasemble:
        var_v0_4 = Menu_Loot_UpdateDisassembly(0);

        if (var_v0_4 != 0) {
            Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_WaitForTopMenu;
        }

        break;

    case Menu_Loot_LootSelectionState_ConsolidateItems:
        var_v0_4 = Menu_Loot_UpdateStackConsolidation(0);

        if (var_v0_4 != 0) {
            Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_WaitForTopMenu;
        }

        break;

    case Menu_Loot_LootSelectionState_WaitForInput:
        if (Input_buttonsPressed.allButtons == 0) {
            break;
        }
        // Fallthrough

    case Menu_Loot_LootSelectionState_WaitForTopMenu:
        if (Menu_Loot_lootLeftEdge == 0) {
            Menu_Loot_controlsEnabled = 1;
            Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Init;
            Menu_Loot_RefreshClaimableRows();
        }
        break;

    case Menu_Loot_LootSelectionState_UpdateLoot:
        if (Menu_Loot_lootSelectionUpdateLootDelay != 0) {
            --Menu_Loot_lootSelectionUpdateLootDelay;
        } else {

            while (1) {
                for (i = 0; i < Menu_Loot_lootListCount; ++i) {
                    if (Menu_Loot_lootList[i].opaque2 != 0) {
                        break;
                    }
                }

                if (i == Menu_Loot_lootListCount) {
                    break;
                }

                --Menu_Loot_lootListCount;

                for (; i < Menu_Loot_lootListCount; ++i) {
                    Menu_Loot_lootList[i] = Menu_Loot_lootList[i + 1];
                }
            }

            Menu_Loot_RefreshClaimableRows();

            if (Menu_Loot_lootListCount == 0) {

                Menu_Loot_controlsEnabled = 0;

                Menu_Loot_UpdateLeaveConfirmation(1);

                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Discard;
            } else {
                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Init;
            }
        }
        break;
    }

    Menu_Loot_previewedLoot = (Menu_Loot_lootSelectionState & 7) == 1 ? Menu_Loot_selectedLoot + 1 : -128;

    return 0;
}

// decomp: MENUB.PRG vs_menuB_exec (src/MENU/MENUB.PRG/260.c:1951)
int Menu_Loot_Update(u_char* state)
{
    static char Menu_Loot_updateLayoutPadding __attribute__((unused));
    static char Menu_Loot_updateLayoutPadding2 __attribute__((unused));

    int i;
    void* buf;

    Menu_Loot_previewedLoot = 0;

    switch (*state) {
    case 0:

        Menu_PlayOpenSound();
        Menu_Framework_SetActorComparisonStats(16);

        for (i = 0; i < 7; ++i) {
            Items_Inventory_RebuildDisplayIndices(i);
        }

        buf = Core_Memory_AllocateFromHighEnd(
            sizeof *Menu_Loot_inventory + sizeof *Menu_Loot_inventoryBackup + sizeof *Menu_Loot_lootList * 160 * 2);

        Menu_Loot_inventory = buf;
        Menu_Loot_inventoryBackup = buf + sizeof *Menu_Loot_inventory;
        Menu_Loot_lootList = buf + sizeof *Menu_Loot_inventory + sizeof *Menu_Loot_inventoryBackup;
        Menu_Loot_lootListWorkingCopy =
            buf + sizeof *Menu_Loot_inventory + sizeof *Menu_Loot_inventoryBackup + sizeof *Menu_Loot_lootList * 160;
        Menu_Loot_lootClaimed = 0;
        Menu_Loot_lootListCount = 0;
        Menu_Loot_lootListOffset = 0;
        Menu_Loot_updateLayoutPadding = 0;

        Menu_Framework_LoadItemText(1);
        Menu_Loot_BuildStagedInventory(Menu_Framework_lootLists);

        Menu_Loot_controlsEnabled = 0;
        Menu_Loot_showInventory = 1;
        Menu_Loot_inventoryCapacityFlyinTimer = 10;
        Menu_Loot_showLoot = 0;
        Menu_Loot_lootLeftEdge = 10;
        *state = 1;
        break;

    case 1:
        if (Menu_Framework_LoadItemText(0) != 0) {

            Menu_Loot_showLoot = 1;

            if (Menu_Loot_lootListCount == 0) {

                Menu_InitInformationTextBox(1);
                Menu_Framework_SetInformationMessage(
                    (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_emptyChest]);

                *state = 4;

            } else {
                Menu_Framework_InitializeInformationBox();
                *state = 2;
            }
        }
        break;

    case 2:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        if (Menu_Loot_lootLeftEdge != 0) {
            break;
        }

        Menu_Loot_UpdateLootSelection(1);

        *state = 3;
        // Fallthrough

    case 3:
        i = Menu_Loot_UpdateLootSelection(0);
        if (i != 0) {
            Menu_Framework_actionSelection.selectionKind = i == 1;
            *state = 5;
        }
        break;

    case 4:
        if ((Menu_textBoxes[7].flags.done)
            && (Input_buttonsPressed.ports[0].lowByte != 0)) {
            *state = 5;
        }
        break;

    case 5:
        Menu_Loot_showInventory = 0;
        Menu_Loot_showLoot = 0;

        if ((Menu_DismissTextBox(7) != 0)
            || (Menu_Framework_UnloadItemTextWhenReady() == 0)) {
            break;
        }

        Core_Memory_FreeFromHighEnd(Menu_Loot_inventory);

        if (Menu_Framework_lootLists != 0) {
            Items_ClearRoomLootList();
        }

        *state = 0;
        return 1;
    }

    if (Menu_Loot_showInventory != 0) {
        if (Menu_Loot_inventoryCapacityFlyinTimer != 0) {
            --Menu_Loot_inventoryCapacityFlyinTimer;
        }
        i = Menu_Framework_rowAnimationSteps[Menu_Loot_inventoryCapacityFlyinTimer];
    } else {
        if (Menu_Loot_inventoryCapacityFlyinTimer < 10) {
            ++Menu_Loot_inventoryCapacityFlyinTimer;
        }
        i = Menu_Loot_inventoryCapacityFlyinTimer << 5;
    }

    if (Menu_Loot_inventoryCapacityFlyinTimer < 10) {
        Menu_Loot_RenderBagCapacities(i);
    }

    if (Menu_Loot_showLoot != 0) {
        if (Menu_Loot_lootLeftEdge != 0) {
            --Menu_Loot_lootLeftEdge;
        }
        i = Menu_Framework_rowAnimationSteps[Menu_Loot_lootLeftEdge];
    } else {
        if (Menu_Loot_lootLeftEdge < 10) {
            ++Menu_Loot_lootLeftEdge;
        }
        i = Menu_Loot_lootLeftEdge << 5;
    }

    if (Menu_Loot_lootLeftEdge < 10) {

        Menu_Loot_RenderLootRows(i);

        if (Menu_Loot_previewedLoot & 0xF) {
            Menu_Framework_MenuItem(Menu_Loot_previewedLoot + 31)->selected = 1;
        }
    }

    return 0;
}

// MENUB.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Loot_LootScreen_ResetMenubStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_controlsEnabled, &initial, sizeof Menu_Loot_controlsEnabled);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_gradientStateIndex, &initial, sizeof Menu_Loot_gradientStateIndex);
    }
    {
        static const char* initial[] = { "WEAPON", "BLADE", "GRIP", "SHIELD", "ARMOR",
    "GEM", "MISC" };
        memcpy((void*)&Menu_Loot_itemCategoryHeaders, &initial, sizeof Menu_Loot_itemCategoryHeaders);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_disassemblyCursorMemory, &initial, sizeof Menu_Loot_disassemblyCursorMemory);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_stackConsolidationCursorMemory, &initial, sizeof Menu_Loot_stackConsolidationCursorMemory);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_leaveConfirmationCursorAnimStep, &initial, sizeof Menu_Loot_leaveConfirmationCursorAnimStep);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_organizationMenuCursorAnimState, &initial, sizeof Menu_Loot_organizationMenuCursorAnimState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Loot_lootSelectionCursorMemory, &initial, sizeof Menu_Loot_lootSelectionCursorMemory);
    }
}

