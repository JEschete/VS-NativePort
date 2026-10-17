// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Targeting.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
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
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/TextBoxes.h"
#include "Render/Font.h"

// decomp: local static of vs_mainMenu_getEquipmentMaxStats, at file scope so the module reset reaches it
static short Menu_Framework_equipmentTitleRanksWeaponTitleMaxThresholds[] = { 50, 95, SHRT_MAX,
        8192 };

// decomp: local static of vs_mainMenu_getEquipmentMaxStats, at file scope so the module reset reaches it
static short Menu_Framework_equipmentTitleRanksWeaponTitleSubmaxThresholds[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90,
        100, SHRT_MAX };

// decomp: vs_mainMenu_itemHelp (vs_mainMenu_itemHelp@MENU/MAINMENU.PRG)
u_short* Menu_Framework_itemHelp;

// decomp: vs_mainMenu_itemNames (vs_mainMenu_itemNames@MENU/MAINMENU.PRG)
char (*Menu_Framework_itemNames)[24] = NULL;

// decomp: vs_mainMenu_menu12Text (vs_mainMenu_menu12Text@MENU/MAINMENU.PRG)
u_short* Menu_Framework_menu12Text;

// decomp: vs_mainMenu_weaponHands (vs_mainMenu_weaponHands@MENU/MAINMENU.PRG)
u_char Menu_Framework_weaponHands[] = { 0, 0, 1, 0, 1, 0, 1, 1, 1, 0 };

// decomp: MAINMENU.PRG vs_mainMenu_initWeaponDetailsRow (src/MENU/MAINMENU.PRG/2D10.c:14)
Menu_Framework_MenuRow* Menu_Framework_CreateWeaponComponentRow(
    int row, int weaponIndex, int slide)
{
    char* menuText[2];
    int rowType;
    int menuItemId;

    Menu_Framework_MenuRow* menuItem = NULL;
    Items_InventoryWeapon* weapon = &Menu_Framework_weapons[weaponIndex - 1];
    int grip = weapon->grip;
    int gemSlots = Menu_Framework_grips[grip - 1].gemSlots;
    int xOffset = 158;

    if (row == 1) {
        Menu_Framework_FormatBladeRow(&Menu_Framework_blades[weapon->blade - 1], menuText,
            &rowType, Menu_Framework_stringBuf);
    } else if (row == 2) {
        Menu_Framework_FormatGripRow(
            &Menu_Framework_grips[grip - 1], menuText, &rowType, Menu_Framework_stringBuf);
    } else {

        xOffset = (row - 3);

        if (xOffset < gemSlots) {
            int gem;

            menuText[1] = NULL;
            rowType = 0x58000000;
            menuText[0] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_none];
            gem = weapon->gems[xOffset];

            if (gem != 0) {
                Menu_Framework_FormatGemRow(
                    &Menu_Framework_gems[gem - 1], menuText, &rowType, Menu_Framework_stringBuf);
            }

            xOffset = 151;
        } else {
            xOffset = 0;
        }
    }

    menuItemId = row + 10;

    Menu_Framework_DeactivateRow(menuItemId);

    if (xOffset != 0) {

        int x = 320 - xOffset;
        menuItem = Menu_Framework_SetMenuItem(
            menuItemId, x, (row * 16) + 18, xOffset, 0, menuText[0]);
        menuItem->unselectable = menuText[1] == NULL;

        if (slide & 1) {
            menuItem->x = 320;
            menuItem->state = Menu_Framework_MenuItemState_SlideX;
            menuItem->targetPosition0 = x;
        }

        menuItem->rowIcon = (u_int)rowType >> 0x1A;
        menuItem->material = ((u_int)rowType >> 16) & 7;
    }

    return menuItem;
}

// decomp: MAINMENU.PRG vs_mainMenu_initShieldDetailsRow (src/MENU/MAINMENU.PRG/2D10.c:80)
Menu_Framework_MenuRow* Menu_Framework_CreateShieldGemRow(
    int row, int shieldIndex, int slide)
{
    char* menuText[2];
    int rowType;

    Menu_Framework_MenuRow* menuItem = NULL;
    int xOffset = row - 1;
    Items_InventoryShield* shield = &Menu_Framework_shields[shieldIndex - 1];

    if (xOffset < shield->base.gemSlots) {
        char gem;

        menuText[1] = 0;
        rowType = 0x58000000;
        menuText[0] = (char*)&Menu_Framework_itemHelp[0x0340B];
        gem = shield->gems[xOffset];

        if (gem != 0) {
            Menu_Framework_FormatGemRow(&Menu_Framework_gems[gem - 1], (char**)&menuText, &rowType,
                Menu_Framework_stringBuf);
        }

        xOffset = 151;
    } else {
        xOffset = 0;
    }

    Menu_Framework_DeactivateRow(row + 10);

    if (xOffset != 0) {

        menuItem = Menu_Framework_SetMenuItem(
            row + 10, 0x140 - xOffset, (row * 16) + 18, xOffset, 0, (char*)menuText[0]);
        menuItem->unselectable = menuText[1] == 0;

        if (slide & 1) {
            menuItem->x = 320;
            menuItem->state = Menu_Framework_MenuItemState_SlideX;
            menuItem->targetPosition0 = 320 - xOffset;
        }

        menuItem->rowIcon = 0x16;
    }

    return menuItem;
}

// decomp: MAINMENU.PRG vs_mainMenu_setWeaponRow (src/MENU/MAINMENU.PRG/2D10.c:130)
void Menu_Framework_FormatWeaponRow(
    Items_WeaponLoadout* weapon, char* menuText[], int* rowType, void* buf)
{
    int maxStatRank;

    Items_EquipmentPartStats* blade = &weapon->blade;
    int stats = Menu_Framework_EquipmentTitleRanks(weapon, 0);

    Battle_stringContext.strings[0] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
            [(stats & 0xFF) + Text_Menu_ITEMHELP_BIN_Index_warriorEquipment - 1]];

    maxStatRank = ((stats >> 8) & 0xFF);

    Battle_stringContext.strings[1] = (char*)&Menu_Framework_itemHelp
        [maxStatRank != 0
                ? Menu_Framework_itemHelp[maxStatRank + (stats >> 15) - 1
                                       + Text_Menu_ITEMHELP_BIN_Index_weaponPrestige]
                : Menu_Framework_itemHelp[blade->category - 1
                                       + Text_Menu_ITEMHELP_BIN_Index_weaponBasePrestige]];
    Battle_stringContext.strings[9] =
        (char*)&Menu_Framework_itemNames[blade->material - 1 + Text_Menu_ITEMNAME_BIN_Index_wood];
    Battle_stringContext.strings[8] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[blade->category - 1 + Text_Menu_ITEMHELP_BIN_Index_dagger]];
    Battle_stringContext.strings[7] =
        (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp[blade->damageType - 1
                                                          + Text_Menu_ITEMHELP_BIN_Index_blunt]];
    Battle_stringContext.strings[6] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[Menu_Framework_weaponHands[blade->category - 1]
                              + Text_Menu_ITEMHELP_BIN_Index_oneHanded]];

    Render_FormatEncodedText(
        Render_FormatEncodedText(
            buf, (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_classTemplate]),
        (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_weaponDescTemplate]);

    menuText[1] = buf;
    *rowType = (blade->category << 0x1A) + (blade->material << 0x10);
}

// decomp: MAINMENU.PRG vs_mainMenu_setWeaponRowFromInventory (src/MENU/MAINMENU.PRG/2D10.c:169)
void Menu_Framework_FormatInventoryWeaponRow(
    Items_InventoryWeapon* weapon, char* menuText[], int* rowType, void* buf)
{
    Items_WeaponLoadout uiWeapon;

    if (Menu_Framework_weapons == Audio_Seq_unusedInventory.weapons) {
        Items_LoadInventoryWeaponStats(&uiWeapon, weapon);
    } else {
        Menu_Container_BuildWeaponStats(&uiWeapon, weapon, &Menu_Container_containerData->items);
    }

    Menu_Framework_FormatWeaponRow(&uiWeapon, menuText, rowType, buf);

    menuText[0] = weapon->name;
}

// decomp: MAINMENU.PRG vs_mainMenu_setBladeRow (src/MENU/MAINMENU.PRG/2D10.c:185)
void Menu_Framework_FormatBladeRow(
    Items_InventoryBlade* blade, char* menuText[], int* rowType, void* buf)
{
    Battle_stringContext.strings[9] =
        (char*)&Menu_Framework_itemNames[blade->material - 1 + Text_Menu_ITEMNAME_BIN_Index_wood];
    Battle_stringContext.strings[8] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[blade->category - 1 + Text_Menu_ITEMHELP_BIN_Index_dagger]];
    Battle_stringContext.strings[7] =
        (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp[blade->damageType - 1
                                                          + Text_Menu_ITEMHELP_BIN_Index_blunt]];
    Battle_stringContext.strings[6] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[Menu_Framework_weaponHands[blade->category - 1]
                              + Text_Menu_ITEMHELP_BIN_Index_oneHanded]];
    Render_FormatEncodedText(
        Render_FormatEncodedText(
            buf, (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                         [blade->id - 1 + Text_Menu_ITEMHELP_BIN_Index_battleKnifeDesc]]),
        (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_weaponDescTemplate]);

    menuText[0] = Menu_Framework_itemNames[blade->id + Text_Menu_ITEMNAME_BIN_Index_item0];
    menuText[1] = buf;
    *rowType = (blade->category << 0x1A) + (blade->material << 0x10);
}

// decomp: MAINMENU.PRG vs_mainMenu_setGripRow (src/MENU/MAINMENU.PRG/2D10.c:209)
void Menu_Framework_FormatGripRow(
    Items_InventoryGrip* grip, char* menuText[], int* rowType, void* buf)
{
    u_short* id;
    Battle_stringContext.strings[5] = (char*)&Menu_Framework_itemHelp
        [Menu_Framework_itemHelp[grip->category - 1 + Text_Menu_ITEMHELP_BIN_Index_shortGrip]];
    id = &grip->id;

    Render_FormatEncodedText(Render_FormatEncodedText(buf,
                         (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp[*id - 6]]),
        (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_gripTemplate]);

    menuText[0] = Menu_Framework_itemNames[grip->id + Text_Menu_ITEMNAME_BIN_Index_item0];
    menuText[1] = buf;
    *rowType = (grip->category + 0xA) << 0x1A;
}

// decomp: MAINMENU.PRG vs_mainMenu_setShieldRow (src/MENU/MAINMENU.PRG/2D10.c:226)
void Menu_Framework_FormatShieldRow(
    Items_ShieldLoadout* shield, char* menuText[], int* rowType, void* buf)
{
    int maxStatRank;

    Items_EquipmentPartStats* shieldBase = &shield->base;
    int stats = Menu_Framework_EquipmentTitleRanks(shield, 1);

    Battle_stringContext.strings[0] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
            [(stats & 0xFF) - 1 + Text_Menu_ITEMHELP_BIN_Index_warriorEquipment]];

    maxStatRank = (stats >> 8) & 0xFF;

    Battle_stringContext.strings[1] = (char*)&Menu_Framework_itemHelp
        [maxStatRank != 0 ? Menu_Framework_itemHelp[maxStatRank + (stats >> 0xF) - 1
                                                 + Text_Menu_ITEMHELP_BIN_Index_armorPrestige]
                          : Text_Menu_ITEMHELP_BIN_Offset_armorBasePrestige];
    Battle_stringContext.strings[8] =
        (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_armorCategory];
    Battle_stringContext.strings[9] =
        (char*)&Menu_Framework_itemNames[shieldBase->material - 1
                                      + Text_Menu_ITEMNAME_BIN_Index_wood];

    Render_FormatEncodedText(
        Render_FormatEncodedText(
            buf, (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_classTemplate]),
        (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_armorTemplate]);

    menuText[0] = Menu_Framework_itemNames[shieldBase->id + Text_Menu_ITEMNAME_BIN_Index_item0];
    menuText[1] = buf;
    *rowType = (shieldBase->material << 0x10) + 0x3C000000;
}

// decomp: MAINMENU.PRG vs_mainMenu_setShieldRowFromInventory (src/MENU/MAINMENU.PRG/2D10.c:259)
void Menu_Framework_FormatInventoryShieldRow(
    Items_InventoryShield* shield, char* menuText[], int* rowType, void* buf)
{
    Items_ShieldLoadout uiShield;

    if (Menu_Framework_shields == Audio_Seq_unusedInventory.shields) {
        Items_LoadInventoryShieldStats(&uiShield, shield);
    } else {
        Menu_Container_BuildShieldStats(&uiShield, shield, &Menu_Container_containerData->items);
    }

    Menu_Framework_FormatShieldRow(&uiShield, menuText, rowType, buf);
}

// decomp: MAINMENU.PRG vs_mainMenu_setArmorRow (src/MENU/MAINMENU.PRG/2D10.c:273)
void Menu_Framework_FormatArmorRow(
    Items_ArmorLoadout* armor, char* menuText[], int* rowType, void* buf)
{
    int stats;
    int maxStatRank;
    Items_EquipmentPartStats* armorBase = &armor->armor;

    menuText[0] = Menu_Framework_itemNames[armorBase->id];
    *rowType = ((armorBase->category + 14) << 0x1A) + (armorBase->material << 0x10);

    if (armorBase->category == 7) {
        menuText[1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                [armorBase->id - 223 + Text_Menu_ITEMHELP_BIN_Index_roodNecklace]];
        return;
    }

    stats = Menu_Framework_EquipmentTitleRanks(armor, 2);

    Battle_stringContext.strings[0] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
            [(stats & 0xFF) - 1 + Text_Menu_ITEMHELP_BIN_Index_warriorEquipment]];

    maxStatRank = (stats >> 8) & 0xFF;

    Battle_stringContext.strings[1] = (char*)&Menu_Framework_itemHelp
        [maxStatRank != 0
                ? Menu_Framework_itemHelp[maxStatRank + (stats >> 0xF) - 1
                                       + Text_Menu_ITEMHELP_BIN_Index_armorPrestige]
                : Menu_Framework_itemHelp[armorBase->category - 1
                                       + Text_Menu_ITEMHELP_BIN_Index_armorBasePrestige]];
    Battle_stringContext.strings[8] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
            [armorBase->category - 1 + Text_Menu_ITEMHELP_BIN_Index_armorCategory]];
    Battle_stringContext.strings[9] =
        (char*)&Menu_Framework_itemNames[armorBase->material - 1
                                      + Text_Menu_ITEMNAME_BIN_Index_wood];

    Render_FormatEncodedText(
        Render_FormatEncodedText(
            buf, (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_classTemplate]),
        (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_armorTemplate]);

    menuText[1] = (char*)buf;
}

// decomp: MAINMENU.PRG vs_mainMenu_setArmorRowFromInventory (src/MENU/MAINMENU.PRG/2D10.c:316)
void Menu_Framework_FormatInventoryArmorRow(
    Items_InventoryArmor* armor, char* menuText[], int* rowType, void* buf)
{
    Items_ArmorLoadout uiArmor;

    Items_LoadInventoryArmorStats(&uiArmor, armor);
    Menu_Framework_FormatArmorRow(&uiArmor, menuText, rowType, buf);
}

// decomp: MAINMENU.PRG vs_mainMenu_setGemRow (src/MENU/MAINMENU.PRG/2D10.c:325)
void Menu_Framework_FormatGemRow(
    Items_InventoryGem* gem, char* menuText[], int* rowType, void* buf)
{
    Core_Memory_CopyHalfwordBlocks(buf,
        Menu_Framework_itemHelp
            + Menu_Framework_itemHelp[gem->id - 261 + Text_Menu_ITEMHELP_BIN_Index_gemFeldspar],
        96);

    menuText[0] = Menu_Framework_itemNames[gem->id];
    menuText[1] = buf;
    *rowType = 0x58000000;
}

// decomp: MAINMENU.PRG vs_mainMenu_setMiscRow (src/MENU/MAINMENU.PRG/2D10.c:338)
void Menu_Framework_FormatMiscItemRow(
    Items_InventoryMisc* misc, char* menuText[], int* rowType, void* buf)
{
    Core_Memory_CopyHalfwordBlocks(buf,
        Menu_Framework_itemHelp
            + Menu_Framework_itemHelp[misc->id - 323 + Text_Menu_ITEMHELP_BIN_Index_cureRootDesc],
        96);

    menuText[0] = Menu_Framework_itemNames[misc->id];
    menuText[1] = buf;
    *rowType = misc->count << 9;
}

// decomp: MAINMENU.PRG vs_mainMenu_loadItemText (src/MENU/MAINMENU.PRG/2D10.c:1119)
int Menu_Framework_LoadItemText(int load)
{
    static int Menu_Framework_unusedBss2 __attribute__((unused)); // Garbage, patched in MAINMENU_PRG.py
    static Content_DiscQueueSlot* Menu_Framework_loadItemTextItemNamesCdQueueSlot;
    static char Menu_Framework_loadItemTextItemNamesLoading;

    Content_DiscFile cdFile;

    if (load != 0) {

        Menu_Framework_itemNames = Core_Memory_AllocateFromHighEnd(
            VS_ITEMNAME_BIN_SIZE + VS_ITEMHELP_BIN_SIZE + VS_MENU12_BIN_SIZE);
        Menu_Framework_itemHelp = (void*)Menu_Framework_itemNames + VS_ITEMNAME_BIN_SIZE;
        Menu_Framework_menu12Text = (void*)Menu_Framework_itemHelp + VS_ITEMHELP_BIN_SIZE;

        // These three files must be contiguous on disk
        cdFile.lba = VS_ITEMNAME_BIN_LBA;
        cdFile.size = VS_ITEMNAME_BIN_SIZE + VS_ITEMHELP_BIN_SIZE + VS_MENU12_BIN_SIZE;

        Menu_Framework_loadItemTextItemNamesCdQueueSlot = Content_AllocateDiscRequest(&cdFile);

        Content_EnqueueDiscRequest(Menu_Framework_loadItemTextItemNamesCdQueueSlot, Menu_Framework_itemNames);

        Menu_Framework_loadItemTextItemNamesLoading = 1;
        return 0;
    }

    if (Menu_Framework_loadItemTextItemNamesLoading == 0) {
        return 1;
    }

    if (Menu_Framework_loadItemTextItemNamesCdQueueSlot->state == 4) {

        Content_ReleaseDiscRequest(Menu_Framework_loadItemTextItemNamesCdQueueSlot);

        Menu_Framework_loadItemTextItemNamesLoading = 0;
        return 1;
    }

    return 0;
}

// decomp: MAINMENU.PRG vs_mainMenu_ensureItemTextUnloaded (src/MENU/MAINMENU.PRG/2D10.c:1161)
int Menu_Framework_UnloadItemTextWhenReady(void)
{
    if (Menu_Framework_itemNames == NULL) {
        return 1;
    }

    if (Menu_Framework_LoadItemText(0) != 0) {

        Core_Memory_FreeFromHighEnd(Menu_Framework_itemNames);

        Menu_Framework_itemNames = NULL;
        return 1;
    }

    return 0;
}

// decomp: MAINMENU.PRG vs_mainMenu_getEquipmentMaxStats (src/MENU/MAINMENU.PRG/C48.c:87)
int Menu_Framework_EquipmentTitleRanks(void* item, u_int itemCategory)
{

 // Last one junk?

    int i;
    int subMaxRank;
    int maxRank;
    int isAffinity = 0;
    int param = 0;
    int maxParam = 0;
    int subMaxParam = 0;

    switch (itemCategory) {
    case 0:
        for (i = 0; i < 6; ++i) {
            if (maxParam
                < ((Items_WeaponLoadout*)(item))->classAffinityCurrent.classes[1][i]) {
                subMaxParam = maxParam;
                maxParam =
                    ((Items_WeaponLoadout*)(item))->classAffinityCurrent.classes[1][i];
                param = i;
            } else if (subMaxParam < ((Items_WeaponLoadout*)(item))
                           ->classAffinityCurrent.classes[1][i]) {
                subMaxParam =
                    ((Items_WeaponLoadout*)(item))->classAffinityCurrent.classes[1][i];
            }
        }

        for (i = 0; i < 7; ++i) {
            if (maxParam
                < ((Items_WeaponLoadout*)(item))->classAffinityCurrent.affinities[1][i]) {
                subMaxParam = maxParam;
                maxParam =
                    ((Items_WeaponLoadout*)(item))->classAffinityCurrent.affinities[1][i];
                param = i;
                isAffinity = 1;
            } else if (subMaxParam < ((Items_WeaponLoadout*)(item))
                           ->classAffinityCurrent.affinities[1][i]) {
                subMaxParam =
                    ((Items_WeaponLoadout*)(item))->classAffinityCurrent.affinities[1][i];
            }
        }

        break;

    case 1:
        for (i = 0; i < 6; ++i) {
            if (maxParam
                < ((Items_ShieldLoadout*)(item))->classAffinityCurrent.classes[1][i]) {
                subMaxParam = maxParam;
                maxParam =
                    ((Items_ShieldLoadout*)(item))->classAffinityCurrent.classes[1][i];
                param = i;
            } else if (subMaxParam < ((Items_ShieldLoadout*)(item))
                           ->classAffinityCurrent.classes[1][i]) {
                subMaxParam =
                    ((Items_ShieldLoadout*)(item))->classAffinityCurrent.classes[1][i];
            }
        }

        for (i = 0; i < 7; ++i) {
            if (maxParam
                < ((Items_ShieldLoadout*)(item))->classAffinityCurrent.affinities[1][i]) {
                subMaxParam = maxParam;
                maxParam =
                    ((Items_ShieldLoadout*)(item))->classAffinityCurrent.affinities[1][i];
                param = i;
                isAffinity = 1;
            } else if (subMaxParam < ((Items_ShieldLoadout*)(item))
                           ->classAffinityCurrent.affinities[1][i]) {
                subMaxParam =
                    ((Items_ShieldLoadout*)(item))->classAffinityCurrent.affinities[1][i];
            }
        }

        break;

    case 2:
    default:
        for (i = 0; i < 6; ++i) {
            if (maxParam
                < ((Items_ArmorLoadout*)(item))->classAffinityCurrent.classes[1][i]) {
                subMaxParam = maxParam;
                maxParam = ((Items_ArmorLoadout*)(item))->classAffinityCurrent.classes[1][i];
                param = i;
            } else if (subMaxParam
                       < ((Items_ArmorLoadout*)(item))->classAffinityCurrent.classes[1][i]) {
                subMaxParam =
                    ((Items_ArmorLoadout*)(item))->classAffinityCurrent.classes[1][i];
            }
        }

        for (i = 0; i < 7; ++i) {
            if (maxParam
                < ((Items_ArmorLoadout*)(item))->classAffinityCurrent.affinities[1][i]) {
                subMaxParam = maxParam;
                maxParam =
                    ((Items_ArmorLoadout*)(item))->classAffinityCurrent.affinities[1][i];
                param = i;
                isAffinity = 1;
            } else if (subMaxParam < ((Items_ArmorLoadout*)(item))
                           ->classAffinityCurrent.affinities[1][i]) {
                subMaxParam =
                    ((Items_ArmorLoadout*)(item))->classAffinityCurrent.affinities[1][i];
            }
        }

        break;
    }

    subMaxRank = 0;

    for (i = 0; i < 11; ++i) {
        if (subMaxParam < Menu_Framework_equipmentTitleRanksWeaponTitleSubmaxThresholds[i]) {
            subMaxRank = i;
            break;
        }
    }

    maxRank = 0;

    for (i = 0; i < 3; ++i) {
        if (maxParam < Menu_Framework_equipmentTitleRanksWeaponTitleMaxThresholds[i]) {
            maxRank = i;
            break;
        }
    }

    {
        int a = (isAffinity * 6) + param;
        int b = subMaxRank + (maxRank << 8);
        return b + (a << 16);
    }
}

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Framework_ItemRows_ResetMainMenuStatics(void)
{
    {
        static const char (*initial)[24] = NULL;
        memcpy((void*)&Menu_Framework_itemNames, &initial, sizeof Menu_Framework_itemNames);
    }
    {
        static const u_char initial[] = { 0, 0, 1, 0, 1, 0, 1, 1, 1, 0 };
        memcpy((void*)&Menu_Framework_weaponHands, &initial, sizeof Menu_Framework_weaponHands);
    }
    {
        static const short initial[] = { 50, 95, SHRT_MAX,
        8192 };
        memcpy((void*)&Menu_Framework_equipmentTitleRanksWeaponTitleMaxThresholds, &initial, sizeof Menu_Framework_equipmentTitleRanksWeaponTitleMaxThresholds);
    }
    {
        static const short initial[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90,
        100, SHRT_MAX };
        memcpy((void*)&Menu_Framework_equipmentTitleRanksWeaponTitleSubmaxThresholds, &initial, sizeof Menu_Framework_equipmentTitleRanksWeaponTitleSubmaxThresholds);
    }
}

