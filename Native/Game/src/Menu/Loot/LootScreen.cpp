// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/SfxLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/LootLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Inventory/ItemDetailDispatchLive.hpp"
#include "Menu/Loot/LootPoliciesLive.hpp"
#include "Menu/Loot/LootRowsLive.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/GameFontLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Hud.h"
#include "Battle/TargetingState.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/InventoryLiveState.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Loot/LootInventory.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Loot/Paging.h"
#include "Menu/TextBox.h"

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
        if (!VS::Menu::Framework::AreLiveMenuRowsIdle()) {
            break;
        }

        VS::Menu::Framework::SetLiveBackgroundFade(1);
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_Menu);

        menuItem = VS::Menu::Framework::SetLiveMenuItem(3, 320, 18, 0x7E, 8,
            (char*)&Menu_menuStrings
                [Menu_menuStrings[Text_Battle_menuStrings_Index_items]]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 180;
        menuItem->selected = 1;
        Menu_Loot_inventoryTransitionState = 1;
        break;

    case 1:
        VS::Menu::Inventory::SetLivePanelVisibility(0);

        Menu_Inventory_secondaryTitleAnimationStep = 0;
        Menu_Inventory_itemCategoryIconsAnimationStep = 0;
        Menu_Inventory_statusCommandAnimationStep = 0;
        Menu_Inventory_selectedItemCategoryIconOnTop = 0;

        if (!VS::Menu::Framework::AreLiveMenuRowsIdle()) {
            break;
        }

        VS::Field::LiveSetFieldRenderingEnabled(0);

        Menu_Loot_hudWasHidden = Battle_hudHidden;

        if (!Menu_Loot_hudWasHidden) {
            VS::Battle::SetLiveHudHidden(1);
            Battle_hudSlideOffset = 0x200000;
        }

        VS::Menu::Framework::RenderLiveAshley(1);
        VS::Menu::Inventory::UpdateLiveLootItemList(1);

        Menu_Loot_inventoryTransitionState = 2;
        break;

    case 2:
        if (VS::Menu::Inventory::UpdateLiveLootItemList(0) == 0) {
            break;
        }

        VS::Menu::Inventory::SetLivePanelVisibility(0);
        VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
        VS::Field::LiveSetFieldRenderingEnabled(1);

        if (Menu_Loot_hudWasHidden == 0) {
            VS::Battle::SetLiveHudHidden(0);
            Battle_hudSlideOffset = 0;
        }

        VS::Menu::Framework::RenderLiveAshley(2);

        Menu_Loot_inventoryTransitionState = 3;

        if (Input_settings.information == 0) {
            VS::Menu::Framework::DismissLiveInformationBox();
        }

        break;

    case 3:
        VS::Menu::Framework::SetLiveBackgroundFade(0);
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
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
            VS::Menu::Framework::SetLiveMenuItem(31, stepValue + 180, 34, 140, 8,
                (char*)&Menu_Inventory_menuText[Menu_Inventory_menuText[itemCategory + Text_Menub_menuText_Index_weapons]]);
        menuItem->selected = 1;

        if (itemCategory != 7) {
            menuItem->max = Items_Inventory_inventoryItemCapacities[itemCategory];
            menuItem->count = VS::Items::Inventory::CountLiveItems(itemCategory, NULL);
        }
    }

    VS::Menu::Framework::RenderLiveAshley(0);

    if (Menu_Inventory_itemCategoryIconsEnabled != 0) {
        if (Menu_Inventory_itemCategoryIconsAnimationStep < 4) {
            ++Menu_Inventory_itemCategoryIconsAnimationStep;
        }
    } else if (Menu_Inventory_itemCategoryIconsAnimationStep != 0) {
        --Menu_Inventory_itemCategoryIconsAnimationStep;
    }

    VS::Menu::Inventory::DrawLiveItemCategoryIcons(Menu_Inventory_itemCategoryIconsAnimationStep, Menu_Inventory_selectedItemCategoryIconOnTop);

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
        VS::Menu::Framework::DrawLiveButtonPromptBackground(0x10 - stepValue, 0x26, 0x58, 10);
        VS::Menu::Framework::DrawLiveButtonIcon(Menu_Framework_ButtonId_Square, 8 - stepValue, 0x24, NULL);
        VS::Render::DrawLiveCompactTextColor("STATUS", ((0x1C - stepValue) & 0xFFFF) | 0x260000,
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

        VS::Core::Memory::CopyLiveHalfwordBlocks(
            Menu_Loot_lootListWorkingCopy, Menu_Loot_lootList, kMenuLootRowCapacity * sizeof *Menu_Loot_lootListWorkingCopy);

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

        VS::Core::Memory::ZeroLiveWordsBackward(Menu_Loot_lootList, 0x280);

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
        menuItem = VS::Menu::Framework::SetLiveMenuItem(
            0x1E, 0x140, 0x92, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionYes]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 0xC2;
        Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_InitOptionNo;
        break;

    case Menu_Loot_DisassemblyState_InitOptionNo:
        menuItem = VS::Menu::Framework::SetLiveMenuItem(
            0x1F, 0x140, 0xA2, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionNo]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 0xC2;
        Battle_stringContext.strings[0] =
            Menu_Loot_disassembleItemCategory == 0
                ? Menu_Loot_inventory->weapons[i].name
                : Menu_Framework_itemNames[Menu_Loot_inventory->shields[i].base.id];
        VS::Menu::InitLiveInformationTextBox((enum Menu_InformationBoxHeader)(1));
        VS::Menu::Framework::SetLiveInformationMessage(
            (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_disassembleConfirm]);
        Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_WaitInit;
        break;

    case Menu_Loot_DisassemblyState_WaitInit:
        Menu_Loot_disassemblyState += VS::Menu::Framework::AreLiveMenuRowsIdle();
        break;

    case Menu_Loot_DisassemblyState_ProcessCommand:
        VS::Menu::Framework::LiveMenuItem(30 + Menu_Loot_disassemblyYesNoChoice)->selected = 1;
        VS::Menu::Framework::LiveMenuItem(31 - Menu_Loot_disassemblyYesNoChoice)->selected = 0;

        if (Input_buttonsPressed.allButtons & (PADRright | PADRdown)) {
            VS::Menu::Framework::FlyOutLiveRowRight(30);
            VS::Menu::Framework::FlyOutLiveRowRight(31);
            Menu_Loot_showLoot = 0;
            if ((Menu_Loot_disassemblyYesNoChoice != 0) || (Input_buttonsPressed.allButtons & PADRdown)) {
                VS::Menu::PlayLeaveSound();
                Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_Finalize;
            } else {
                VS::Audio::LivePlaySfxCentered(0x7E, 0x19);
                Menu_Loot_disassemblyState = Menu_Loot_DisassemblyState_Disassemble;
            }
            if (Input_settings.information == 0) {
                VS::Menu::Framework::DismissLiveInformationBox();
            }
            break;
        }

        if (Input_buttonRepeat & (PADLup | PADLdown)) {
            VS::Menu::PlayChangeSound();
            Menu_Loot_disassemblyYesNoChoice = (1 - Menu_Loot_disassemblyYesNoChoice);
        }

        Menu_Loot_disassemblyCursorMemory = VS::Menu::Framework::DrawLiveMainMenuCursor(Menu_Loot_disassemblyCursorMemory, Menu_Loot_disassemblyYesNoChoice + 8);
        break;

    case Menu_Loot_DisassemblyState_Disassemble:
        if (Menu_Loot_lootLeftEdge != 6) {
            break;
        }

        VS::Core::Memory::CopyLiveBytesBackward(&Menu_Loot_lootListWorkingCopy[Menu_Loot_disassemblyLootCount], &Menu_Loot_lootList[1],
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

        VS::Core::Memory::CopyLiveHalfwordBlocks(
            Menu_Loot_lootList, Menu_Loot_lootListWorkingCopy, kMenuLootRowCapacity * sizeof *Menu_Loot_lootListWorkingCopy);
        VS::Menu::Loot::Live::RefreshClaimableRows();

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

        VS::Core::Memory::CopyLiveHalfwordBlocks(
            Menu_Loot_lootListWorkingCopy, Menu_Loot_lootList, kMenuLootRowCapacity * sizeof *Menu_Loot_lootListWorkingCopy);
        VS::Core::Memory::CopyLiveHalfwordBlocks(Menu_Loot_inventoryBackup, Menu_Loot_inventory, sizeof *Menu_Loot_inventory);

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
                    int remaining_stack_capacity = Menu_Loot_stackConsolidationAmountToAdd + 100;
                    remaining_stack_capacity -= item->count;
                    Menu_Loot_stackConsolidationAmountToAdd = remaining_stack_capacity;
                }
            }

            Menu_Loot_stackConsolidationTotalCount = 0;

            for (i = 0; i < Menu_Loot_lootListCount; ++i) {
                if (Menu_Loot_lootList[i].itemCategory == Items_ItemCategory_Misc) {
                    item = &Menu_Loot_inventory->misc[Menu_Loot_lootList[i].itemIndex];
                    if (item->id == Menu_Loot_stackConsolidationItemId) {

                        Menu_Loot_stackConsolidationTotalCount += item->count;

                        VS::Core::Memory::ZeroLiveWordsBackward(item, sizeof *item);

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
        menuItem = VS::Menu::Framework::SetLiveMenuItem(
            30, 320, 146, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionYes]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        Menu_Loot_stackConsolidationState = 2;
        break;

    case 2:
        menuItem = VS::Menu::Framework::SetLiveMenuItem(
            31, 320, 162, 0x7E, 0, (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_optionNo]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        Battle_stringContext.strings[0] = Menu_Framework_itemNames[Menu_Loot_stackConsolidationItemId];

        VS::Menu::InitLiveInformationTextBox((enum Menu_InformationBoxHeader)(1));
        VS::Menu::Framework::SetLiveInformationMessage(
            (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_organizeConfirm]);

        Menu_Loot_stackConsolidationState = 3;
        break;

    case 3:
        Menu_Loot_stackConsolidationState += VS::Menu::Framework::AreLiveMenuRowsIdle();
        break;

    case 4:
        VS::Menu::Framework::LiveMenuItem(30 + Menu_Loot_stackConsolidationYesNoChoice)->selected = 1;
        VS::Menu::Framework::LiveMenuItem(31 - Menu_Loot_stackConsolidationYesNoChoice)->selected = 0;

        if (Input_buttonsPressed.allButtons & 0x60) {

            VS::Menu::Framework::FlyOutLiveRowRight(30);
            VS::Menu::Framework::FlyOutLiveRowRight(31);

            Menu_Loot_showLoot = 0;

            if ((Menu_Loot_stackConsolidationYesNoChoice != 0) || (Input_buttonsPressed.allButtons & PADRdown)) {
                VS::Menu::PlayLeaveSound();
                Menu_Loot_stackConsolidationState = 6;
            } else {
                VS::Menu::PlaySelectSound();
                Menu_Loot_stackConsolidationState = 5;
            }

            var_v0 = 0;

            if (Input_settings.information == 0) {
                VS::Menu::Framework::DismissLiveInformationBox();
            }

            break;
        }

        if (Input_buttonRepeat & (PADLup | PADLdown)) {
            VS::Menu::PlayChangeSound();
            Menu_Loot_stackConsolidationYesNoChoice = 1 - Menu_Loot_stackConsolidationYesNoChoice;
        }

        Menu_Loot_stackConsolidationCursorMemory = VS::Menu::Framework::DrawLiveMainMenuCursor(Menu_Loot_stackConsolidationCursorMemory, Menu_Loot_stackConsolidationYesNoChoice + 8);
        break;

    case 5:
        if (Menu_Loot_lootLeftEdge != 6) {
            break;
        }

        for (i = 0; i < Menu_Loot_stackConsolidationLootCount; ++i) {
            if (Menu_Loot_lootListWorkingCopy[i].itemCategory == 6) {

                item = &Menu_Loot_inventoryBackup->misc[Menu_Loot_lootListWorkingCopy[i].itemIndex];

                if (item->id == Menu_Loot_stackConsolidationItemId) {

                    VS::Core::Memory::ZeroLiveWordsBackward(item, 4);

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

            VS::Core::Memory::CopyLiveHalfwordBlocks(Menu_Loot_lootList, Menu_Loot_lootListWorkingCopy, sizeof *Menu_Loot_lootList * kMenuLootRowCapacity);
            VS::Core::Memory::CopyLiveHalfwordBlocks(Menu_Loot_inventory, Menu_Loot_inventoryBackup, sizeof *Menu_Loot_inventory);

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
    int nested_menu_result;

    if (initialize != 0) {
        Menu_Loot_selectedLoot = 0;
        Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Init;
        return 0;
    }

    switch (Menu_Loot_lootSelectionState) {
    case Menu_Loot_LootSelectionState_Init:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
            break;
        }

        VS::Menu::Framework::InitializeLiveInformationBox();

        Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_ProcessInput;
        // Fallthrough

    case Menu_Loot_LootSelectionState_ProcessInput:
        Menu_Loot_controlsEnabled = 1;

        if (Input_buttonsPressed.allButtons & PADRdown) {

            VS::Menu::PlayLeaveSound();

            Menu_Loot_controlsEnabled = 0;

            VS::Menu::Loot::Live::UpdateLeaveConfirmation(1);

            Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Discard;
            break;

        } else if (Input_buttonsPressed.allButtons & PADRleft) {

            temp_s2 = Menu_Loot_lootClaimed;

            for (i = 0; i < Menu_Loot_lootListCount; ++i) {

                loot = &Menu_Loot_lootList[i];

                if (VS::Items::Inventory::CopyLiveItem(loot->itemCategory | Menu_Framework_CopyItemFlag_Write,
                        &Audio_Seq_unusedInventory, loot->itemIndex, Menu_Loot_inventory)
                    != 0) {
                    if (loot->itemCategory == Items_ItemCategory_Misc) {

                        int miscId = Menu_Loot_inventory->misc[loot->itemIndex].id;

                        if (miscId > 457) {
                            VS::Menu::Loot::Live::RecordClaimedKey(miscId);
                        }
                    }

                    loot->opaque2 = 1;
                    ++Menu_Loot_lootClaimed;
                }
            }

            if (temp_s2 != Menu_Loot_lootClaimed) {

                VS::Audio::LivePlaySfxCentered(0x7E, 0xD);

                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_UpdateLoot;
                break;
            }

            VS::Menu::PlayInvalidSound();
        }

        lootIndex = Menu_Loot_selectedLoot + Menu_Loot_lootListOffset;
        loot = &Menu_Loot_lootList[lootIndex];

        if (Input_buttonsPressed.allButtons & PADRright) {
            if (loot->state == 0) {

                VS::Menu::PlaySelectSound();

                Menu_Loot_controlsEnabled = 0;

                VS::Menu::Loot::Live::UpdateOrganizationMenu(loot);

                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Organize;
            } else {

                VS::Audio::LivePlaySfxCentered(0x7E, 0xD);

                if (loot->itemCategory == Items_ItemCategory_Misc) {

                    i = Menu_Loot_inventory->misc[loot->itemIndex].id;

                    if (i > 457) {
                        VS::Menu::Loot::Live::RecordClaimedKey(i);
                    }
                }

                VS::Items::Inventory::CopyLiveItem(loot->itemCategory | 0x10, &Audio_Seq_unusedInventory,
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
                VS::Menu::PlayChangeSound();
            }

            VS::Menu::Loot::Live::ShowLootDescription(Menu_Loot_selectedLoot + Menu_Loot_lootListOffset);

            Menu_Loot_lootSelectionCursorMemory = VS::Menu::Framework::DrawLiveCursor(
                Menu_Loot_lootSelectionCursorMemory, (((Menu_Loot_selectedLoot * 0x10) + 0x2A) << 0x10) | 0xA);
        }
        break;

    case Menu_Loot_LootSelectionState_Discard:
        i = VS::Menu::Loot::Live::UpdateLeaveConfirmation(0);

        if (i != 0) {
            if (i < 0) {
                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_WaitForTopMenu;
            } else if (Menu_Loot_AdvanceStagedInventory()) {
                Menu_Loot_controlsEnabled = 0;
                Menu_Loot_showLoot = 1;
                Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_Init;
            } else {
                return i;
            }
        }
        break;

    case Menu_Loot_LootSelectionState_Organize:
        i = VS::Menu::Loot::Live::UpdateOrganizationMenu(NULL);

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
        nested_menu_result = Menu_Loot_UpdateInventoryTransition(0);

        if (nested_menu_result != 0) {
            Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_WaitForTopMenu;
        }

        break;

    case Menu_Loot_LootSelectionState_Dissasemble:
        nested_menu_result = Menu_Loot_UpdateDisassembly(0);

        if (nested_menu_result != 0) {
            Menu_Loot_lootSelectionState = Menu_Loot_LootSelectionState_WaitForTopMenu;
        }

        break;

    case Menu_Loot_LootSelectionState_ConsolidateItems:
        nested_menu_result = Menu_Loot_UpdateStackConsolidation(0);

        if (nested_menu_result != 0) {
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
            VS::Menu::Loot::Live::RefreshClaimableRows();
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

            VS::Menu::Loot::Live::RefreshClaimableRows();

            if (Menu_Loot_lootListCount == 0) {

                Menu_Loot_controlsEnabled = 0;

                VS::Menu::Loot::Live::UpdateLeaveConfirmation(1);

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

        VS::Menu::PlayOpenSound();
        VS::Menu::Framework::SetLiveActorComparisonStats(16);

        for (i = 0; i < 7; ++i) {
            VS::Items::Inventory::RebuildLiveDisplayIndices(i);
        }

        buf = VS::Core::Memory::AllocateLiveFromHighEnd(
            sizeof *Menu_Loot_inventory + sizeof *Menu_Loot_inventoryBackup + sizeof *Menu_Loot_lootList * kMenuLootRowCapacity * 2);

        Menu_Loot_inventory = (Items_Inventory *)(buf);
        Menu_Loot_inventoryBackup = (Items_Inventory *)(((u_char*)(buf)) + sizeof *Menu_Loot_inventory);
        Menu_Loot_lootList = (Menu_Loot_LootListItem *)(((u_char*)(buf)) + sizeof *Menu_Loot_inventory + sizeof *Menu_Loot_inventoryBackup);
        Menu_Loot_lootListWorkingCopy =
            (Menu_Loot_LootListItem *)(((u_char*)(buf)) + sizeof *Menu_Loot_inventory + sizeof *Menu_Loot_inventoryBackup + sizeof *Menu_Loot_lootList * kMenuLootRowCapacity);
        Menu_Loot_lootClaimed = 0;
        Menu_Loot_lootListCount = 0;
        Menu_Loot_lootListOffset = 0;
        Menu_Loot_updateLayoutPadding = 0;

        VS::Menu::Framework::LoadLiveItemText(1);
        VS::Menu::Loot::Live::BuildStagedInventory(Menu_Framework_lootLists);

        Menu_Loot_controlsEnabled = 0;
        Menu_Loot_showInventory = 1;
        Menu_Loot_inventoryCapacityFlyinTimer = 10;
        Menu_Loot_showLoot = 0;
        Menu_Loot_lootLeftEdge = 10;
        *state = 1;
        break;

    case 1:
        if (VS::Menu::Framework::LoadLiveItemText(0) != 0) {

            Menu_Loot_showLoot = 1;

            if (Menu_Loot_lootListCount == 0) {

                VS::Menu::InitLiveInformationTextBox((enum Menu_InformationBoxHeader)(1));
                VS::Menu::Framework::SetLiveInformationMessage(
                    (char*)&Menu_Inventory_menuText[Text_Menub_menuText_Offset_emptyChest]);

                *state = 4;

            } else {
                VS::Menu::Framework::InitializeLiveInformationBox();
                *state = 2;
            }
        }
        break;

    case 2:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
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

        if ((VS::Menu::DismissLiveTextBox(7) != 0)
            || (VS::Menu::Framework::UnloadLiveItemTextWhenReady() == 0)) {
            break;
        }

        Menu_Loot_ResetPaging();
        VS::Core::Memory::FreeLiveFromHighEnd(Menu_Loot_inventory);

        if (Menu_Framework_lootLists != 0) {
            VS::Items::ClearLiveRoomLootList();
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
        VS::Menu::Loot::Live::RenderBagCapacities(i);
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

        VS::Menu::Loot::Live::RenderLootRows(i);

        if (Menu_Loot_previewedLoot & 0xF) {
            VS::Menu::Framework::LiveMenuItem(Menu_Loot_previewedLoot + 31)->selected = 1;
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
        memcpy((void*)&Menu_Loot_lootSelectionCursorMemory, &initial, sizeof Menu_Loot_lootSelectionCursorMemory);
    }
}

}  // extern "C"
