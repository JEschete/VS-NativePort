// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/StatusEffects.h"
#include "Battle/TargetVolume.h"
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Overlays.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Effects/ScreenEffects.h"
#include "Event/Splash.h"
#include "Field/RoomLoading.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/Weapons.h"
#include "Menu/BattleAbilities.h"
#include "Menu/BreakArts.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Files.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Framework/AshleyPreview.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Magic.h"
#include "Menu/Manual.h"
#include "Menu/Map.h"
#include "Menu/Options.h"
#include "Menu/Rename.h"
#include "Menu/Results/Results.h"
#include "Menu/Score/ScoreScreen.h"
#include "Menu/Setup/SetupScreen.h"
#include "Menu/Sounds.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/TextBox.h"
#include "OverlayWorkspace.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Text/VariableFont.h"
#include "Render/TextureAnimation.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Settings.h"
#include "State/StateFlags.h"

// decomp: _loadedSubMenu (_loadedSubMenu@BATTLE/BATTLE.PRG)
signed char Menu_Framework_loadedSubMenu; /* data: contents loaded in Step 5 */

// decomp: vs_battle_menuItems (vs_battle_menuItems@BATTLE/BATTLE.PRG)
Menu_Framework_MenuRow* Menu_Framework_menuItems; /* data: contents loaded in Step 5 */

// decomp: D_800EB9C4 (D_800EB9C4@BATTLE/BATTLE.PRG)
Items_LootListNode* Menu_Framework_lootLists; /* data: contents loaded in Step 5 */

// decomp: D_800EB9C8 (D_800EB9C8@BATTLE/BATTLE.PRG)
Items_BattleLoot* Menu_Framework_lootEntry; /* data: contents loaded in Step 5 */

// decomp: D_800EB9CE (D_800EB9CE@BATTLE/BATTLE.PRG)
char Menu_textBoxBufferIndex; /* data: contents loaded in Step 5 */

// decomp: _menuBgUnpackedBuf (_menuBgUnpackedBuf@BATTLE/BATTLE.PRG)
u_int* Menu_Framework_menuBackgroundBuffer; /* data: contents loaded in Step 5 */

// decomp: D_800EBA78 (D_800EBA78@BATTLE/BATTLE.PRG)
char Menu_Framework_kMainMenuHelpText[336]; /* rodata: contents loaded in Step 5 */

// decomp: D_800F4CC0 (D_800F4CC0@BATTLE/BATTLE.PRG)
int Menu_Framework_mainMenuCursorAnimation;

// decomp: D_800F4CD0 (D_800F4CD0@BATTLE/BATTLE.PRG)
u_long Menu_textBoxOrderingTables;

// decomp: vs_battle_shortcutInvoked (vs_battle_shortcutInvoked@BATTLE/BATTLE.PRG)
char Menu_Framework_shortcutInvoked;

// decomp: D_800F4E6B (D_800F4E6B@BATTLE/BATTLE.PRG)
char Menu_saveLocationId;

// decomp: vs_battle_submenuStates (vs_battle_submenuStates@BATTLE/BATTLE.PRG)
char Menu_Framework_submenuStates[16];

// decomp: vs_battle_rowTypeBuf (vs_battle_rowTypeBuf@BATTLE/BATTLE.PRG)
int* Menu_Framework_rowTypeBuf;

// decomp: vs_battle_unlockedBreakArt (vs_battle_unlockedBreakArt@BATTLE/BATTLE.PRG)
char Menu_Framework_unlockedBreakArt;

// decomp: vs_battle_stringBuf (vs_battle_stringBuf@BATTLE/BATTLE.PRG)
char* Menu_Framework_stringBuf;

// decomp: D_800F4E98 (D_800F4E98@BATTLE/BATTLE.PRG)
Menu_Framework_ActionSelection Menu_Framework_actionSelection;

// decomp: D_800F4EA0 (D_800F4EA0@BATTLE/BATTLE.PRG)
int Menu_Framework_actionRestrictions;

// decomp: D_800F4ED4 (D_800F4ED4@BATTLE/BATTLE.PRG)
int Menu_Framework_unusedInterfaceFlag;

// decomp: D_800F4EE8 (D_800F4EE8@BATTLE/BATTLE.PRG)
Menu_Framework_MenuMemory Menu_Framework_menuMemory;

// decomp: vs_battle_unlockedBattleAbility (vs_battle_unlockedBattleAbility@BATTLE/BATTLE.PRG)
char Menu_Framework_unlockedBattleAbility;

// decomp: D_800F4FDB (D_800F4FDB@BATTLE/BATTLE.PRG)
char Menu_Framework_openedWithoutMainMenu;

// decomp: vs_battle_menuState (vs_battle_menuState@BATTLE/BATTLE.PRG)
Menu_Framework_MenuControlState Menu_Framework_menuState;

// decomp: _menuLbas (_menuLbas@BATTLE/BATTLE.PRG)
int Menu_Framework_menuLbas[] = { squashLba(VS_MAINMENU_PRG) + (VS_MENUBG_BIN_SIZE >> 11),
    squashLba(VS_MENU0_PRG), squashLba(VS_MENU1_PRG), squashLba(VS_MENU2_PRG),
    squashLba(VS_MENU3_PRG), squashLba(VS_MENU4_PRG), squashLba(VS_MENU5_PRG),
    squashLba(VS_MENU7_PRG), squashLba(VS_MENU8_PRG), squashLba(VS_MENU9_PRG),
    squashLba(VS_MENUE_PRG), squashLbaVal(VS_MENUA_PRG_LBA, 0), squashLba(VS_MENUB_PRG),
    squashLba(VS_MENUC_PRG), squashLba(VS_MENUD_PRG), squashLba(VS_MENUF_PRG) };

// decomp: _actionsSubMenuId (_actionsSubMenuId@src/MENU/MAINMENU.PRG/58EC.c)
char Menu_Framework_actionsSubMenuId = 0;

// decomp: _backgroundBrightness (_backgroundBrightness@src/MENU/MAINMENU.PRG/58EC.c)
short Menu_Framework_backgroundBrightness = 128;

// decomp: _backgroundFadeState (_backgroundFadeState@MENU/MAINMENU.PRG)
char Menu_Framework_backgroundFadeState = 0;

// decomp: _backgroundOtOffset (_backgroundOtOffset@src/MENU/MAINMENU.PRG/58EC.c)
short Menu_Framework_backgroundOtOffset = -4;

// decomp: _itemActionCount (_itemActionCount@src/MENU/MAINMENU.PRG/58EC.c)
char Menu_Framework_itemActionCount = 0;

// decomp: _itemActionListOffset (_itemActionListOffset@src/MENU/MAINMENU.PRG/58EC.c)
char Menu_Framework_itemActionListOffset = 0;

// decomp: _itemsText (_itemsText@src/MENU/MAINMENU.PRG/58EC.c)
u_short* Menu_Framework_itemsText = NULL;

// decomp: _menuActionCurrent (_menuActionCurrent@src/MENU/MAINMENU.PRG/58EC.c)
char Menu_Framework_menuActionCurrent = 0;

// decomp: _menuActionNext (_menuActionNext@src/MENU/MAINMENU.PRG/58EC.c)
char Menu_Framework_menuActionNext = 0;

// decomp: _menuActionsPageOffset (_menuActionsPageOffset@MENU/MAINMENU.PRG)
char Menu_Framework_menuActionsPageOffset;

// decomp: _menuRowYOffset (_menuRowYOffset@src/MENU/MAINMENU.PRG/58EC.c)
char Menu_Framework_menuRowYOffset = 0;

// decomp: _selectedItemAction (_selectedItemAction@src/MENU/MAINMENU.PRG/58EC.c)
int Menu_Framework_selectedItemAction = 0;

// decomp: _selectedRow (_selectedRow@src/MENU/MAINMENU.PRG/58EC.c)
int Menu_Framework_selectedRow = 0;

// decomp: _sortItemsTextBuf (_sortItemsTextBuf@src/MENU/MAINMENU.PRG/58EC.c)
u_short* Menu_Framework_sortItemsTextBuf = NULL;

// decomp: _subMenuId (_subMenuId@src/MENU/MAINMENU.PRG/58EC.c)
u_char Menu_Framework_subMenuId = 0;

// decomp: _subMenuPage (_subMenuPage@src/MENU/MAINMENU.PRG/58EC.c)
char Menu_Framework_subMenuPage = 0;

// decomp: _subMenuRowCount (_subMenuRowCount@src/MENU/MAINMENU.PRG/58EC.c)
u_short Menu_Framework_subMenuRowCount = 0;

// decomp: _subMenuRowInfo (_subMenuRowInfo@src/MENU/MAINMENU.PRG/58EC.c)
char Menu_Framework_subMenuRowInfo = 0;

// decomp: _textHeaders (_textHeaders@MENU/MAINMENU.PRG)
Menu_Framework_TextHeader Menu_Framework_textHeaders[2];

// decomp: local static of _renderMenuCommand, at file scope so the module reset reaches it
static int Menu_Framework_commandLabelActionYPos = 0;

// decomp: local static of _renderAbilityCost, at file scope so the module reset reaches it
static u_char Menu_Framework_actionCostPanelAnimState = 8;

// decomp: local static of _renderScrollableRow, at file scope so the module reset reaches it
static char Menu_Framework_scrollableRowAnimState = 0;

// decomp: local static of vs_mainMenu_renderTabNavigation, at file scope so the module reset reaches it
static char Menu_Framework_tabNavigationArrowAnimState = 11;

// decomp: bss_3 (bss_3@MENU/MAINMENU.PRG)
char Menu_Framework_unusedBss3[6];

// decomp: bss_4 (bss_4@MENU/MAINMENU.PRG)
char Menu_Framework_unusedBss4[18];

// decomp: bss_7 (bss_7@MENU/MAINMENU.PRG)
char Menu_Framework_unusedBss7[2];

// decomp: local static of vs_mainMenu_renderTabNavigation, at file scope so the module reset reaches it
static int Menu_Framework_tabNavigationControlPositions[] = { vs_getXY(216, 8), vs_getXY(210, 8),
        vs_getXY(288, 8), vs_getXY(282, 8) };

// decomp: local static of vs_mainMenu_renderItemActionMenu, at file scope so the module reset reaches it
static char Menu_Framework_actionMenuCursorAnimStep = 0;

// decomp: local static of vs_mainMenu_renderItemsList, at file scope so the module reset reaches it
static char Menu_Framework_rowSelectionCursorAnimStep = 0;

// decomp: local static of _quitDelay, at file scope so the module reset reaches it
static int Menu_Framework_quitDelay = 0;

// decomp: local static of _renderMenuCommand, at file scope so the module reset reaches it
static u_short Menu_Framework_commandLabelMenuActionUvs[] = { vs_getUV(0, 0), vs_getUV(0, 64), vs_getUV(64, 64),
        vs_getUV(100, 64), vs_getUV(132, 64), vs_getUV(188, 64), vs_getUV(0, 74) };

// decomp: local static of _renderScrollableRow, at file scope so the module reset reaches it
static char Menu_Framework_scrollableRowPreviousFrameBuf = 0;

// decomp: local static of vs_mainMenu_unpackMenubg, at file scope so the module reset reaches it
static RECT Menu_Framework_loadBackgroundTextureRects[] = { { 640, 256, 32, 240 }, { 768, 256, 32, 240 } };

// decomp: vs_mainMenu_actionMenuState (vs_mainMenu_actionMenuState@MENU/MAINMENU.PRG)
char Menu_Framework_actionMenuState = 0;

// decomp: vs_mainMenu_backgroundFadeStep (vs_mainMenu_backgroundFadeStep@MENU/MAINMENU.PRG)
int Menu_Framework_backgroundFadeStep = 0;

// decomp: vs_mainMenu_containerEmptyBackup (vs_mainMenu_containerEmptyBackup@MENU/MAINMENU.PRG)
char Menu_Framework_unusedContainerEmptyBackup;

// decomp: vs_mainMenu_cursorColor (vs_mainMenu_cursorColor@MENU/MAINMENU.PRG)
char Menu_Framework_cursorColor = 0;

// decomp: vs_mainMenu_displayActionCost (vs_mainMenu_displayActionCost@MENU/MAINMENU.PRG)
char Menu_Framework_displayActionCost = 0;

// decomp: vs_mainMenu_hideMenu (vs_mainMenu_hideMenu@MENU/MAINMENU.PRG)
char Menu_Framework_hideMenu = 0;

// decomp: vs_mainMenu_itemsListRow (vs_mainMenu_itemsListRow@MENU/MAINMENU.PRG)
char Menu_Framework_itemsListRow = 0;

// decomp: vs_menu_inventoryStorage (vs_menu_inventoryStorage@MENU/MAINMENU.PRG)
Menu_Container_InventoryStorage* Menu_Framework_unusedInventoryStorage;

// decomp: local static of _renderAshley, at file scope so the module reset reaches it
static int Menu_Framework_ashleyRender = 0;

// decomp: local static of _renderAshley, at file scope so the module reset reaches it
static char Menu_Framework_ashleySkipFrame = 0;

// decomp: BATTLE.PRG vs_battle_loadMenuPrg (src/BATTLE/BATTLE.PRG/5BF94.c:2576)
int Menu_Framework_LoadMenuOverlay(int arg0)
{
    Menu_Framework_MenuControlState* state = &Menu_Framework_menuState;
    int subMenu = arg0 & 0x1F;

    if (subMenu == 31) {
        return 1;
    }

    if (subMenu == 0 && Menu_Framework_loadedSubMenu > 0) {
        return 1;
    }

    if (Menu_Framework_loadedSubMenu != subMenu) {
        void* overlaySlot;
        Content_DiscFile file;
        u_int fileData;

        Menu_Framework_loadedSubMenu = subMenu;
        state->loading = 1;

        if (Menu_Framework_shortcutInvoked != 5) {
            fileData = Menu_Framework_menuLbas[subMenu];
        } else {
            fileData = VS_MAINMENU_PRG_LBA << 8 | VS_MAINMENU_PRG_SIZE >> 11;
        }

        file.lba = fileData >> 8;
        file.size = (fileData & 0xFF) << 11;
        state->cdQueue = Content_AllocateDiscRequest(&file);

        if (subMenu == 0) {
            overlaySlot = Core_overlaySlots[1];
        } else {
            overlaySlot = Core_overlaySlots[2];
        }

        Content_EnqueueDiscRequest(state->cdQueue, overlaySlot);
    }

    if (state->loading == 0) {
        if (Menu_Framework_menuBackgroundBuffer != NULL) {

            Core_Memory_FreeFromHighEnd(Menu_Framework_menuBackgroundBuffer);

            Menu_Framework_menuBackgroundBuffer = NULL;

            Items_Inventory_StoreEquippedStats();
        }

        return 1;
    }

    if (state->cdQueue->state == Content_DiscRequestState_Loaded) {

        Content_ReleaseDiscRequest(state->cdQueue);
        Boot_WaitForOverlayReady();

        state->loading = 0;

        if (Menu_Framework_openedWithoutMainMenu != 0) {
            if (Menu_Framework_shortcutInvoked == 5) {
                Menu_Inventory_UpdateMiscItemShortcut(1);
                return 0;
            }
            return 1;
        }

        if (subMenu == 0) {
            Menu_Framework_menuBackgroundBuffer = (u_int*)Core_Memory_AllocateFromHighEnd(0xB400);
            Menu_Framework_LoadBackgroundTexture(Menu_Framework_menuBackgroundBuffer);
        }
    }

    return 0;
}

// decomp: BATTLE.PRG vs_battle_getMenuItem (src/BATTLE/BATTLE.PRG/5BF94.c:2658)
Menu_Framework_MenuRow* Menu_Framework_MenuItem(int id) { return Menu_Framework_menuItems + id; }

// decomp: BATTLE.PRG vs_battle_setMenuItem (src/BATTLE/BATTLE.PRG/5BF94.c:2660)
Menu_Framework_MenuRow* Menu_Framework_SetMenuItem(
    int row, int x, int y, int w, int backgroundWidth, char* text)
{
    Menu_Framework_MenuRow* menuItem;
    Menu_Framework_MenuRow* var_a0;
    int i;
    u_int c;

    menuItem = &Menu_Framework_menuItems[row];
    menuItem->state = 1;
    menuItem->w = w;
    menuItem->backgroundWidth = backgroundWidth;

    Core_Memory_ZeroWordsBackward(&menuItem->gradientState, 0x3C);

    var_a0 = menuItem;
    menuItem->x = x;
    menuItem->y = y;

    for (i = 0; i < 31;) {

        c = *text++;
        if (c == Core_TextCode_Spacing) {
            var_a0->text[i++] = c;
            c = *text++;
        } else if (c == Core_TextCode_Terminator) {
            var_a0->text[i] = 0xFF;
            break;
        } else if (c >= Core_TextCode_NonPrinting) {
            continue;
        }

        var_a0->text[i++] = c;
    }

    return menuItem;
}

// decomp: BATTLE.PRG vs_battle_uiGradientStop (src/BATTLE/BATTLE.PRG/5BF94.c:2721)
int Menu_Framework_GradientStopColor(u_int gradient, u_int colorIndex, int brightness)
{
    u_int color0;
    u_int color1;

    if (colorIndex < 9) {
        color0 = Render_BlendRgbWithBrightness(0x6B4100, 0x6C8219, 0x80, colorIndex);
        color1 = Render_BlendRgbWithBrightness(0x330500, 0x262801, 0x80, colorIndex);
    } else {
        color0 = Menu_Framework_gradientTopColors[((colorIndex >> 3) - 2)];
        color1 = Menu_Framework_gradientBottomColors[((colorIndex >> 3) - 2)];
    }

    return Render_BlendRgbWithBrightness(color0, color1, brightness, gradient);
}

// decomp: BATTLE.PRG vs_battle_renderMenuItem (src/BATTLE/BATTLE.PRG/5BF94.c:2737)
void Menu_Framework_DrawMenuItem(Menu_Framework_MenuRow* menuItem)
{

    int y = menuItem->gradientState;
    u_long* scratch = *(void**)((uintptr_t)Core_ScratchBase() + 0x008) + 8;
    int w = menuItem->w;
    int bgWidth = menuItem->backgroundWidth;
    char* text = menuItem->text;

    if (menuItem->state != 0) {
        int temp_v1;
        int x;
        int c;
        int var_s2;
        char* pText;
        int color;
        u_long* prim;
        char** v;

        if ((menuItem->selected | menuItem->invertGradient) && !menuItem->unselectable) {
            y = 8;
        }

        color = menuItem->fontColor;

        if (color == 0) {
            color = menuItem->unselectable * 3;
        }

        Render_SetMenuFontStyle(color + 4);

        temp_v1 = menuItem->x;
        var_s2 = temp_v1 + 6;

        if ((menuItem->rowIcon - 1) < 23u) {
            var_s2 = temp_v1 + 22;
        }

        c = text[0];
        pText = text + 1;

        while (c != 0xFF) {
            if (c == Core_TextCode_Spacing) {
                var_s2 += *pText++;
            } else {
                var_s2 = Render_DrawVariableWidthGlyph(
                    c, var_s2, menuItem->y, scratch - 3);
            }
            c = *pText++;
        }

        c = Menu_Framework_GradientStopColor(8 - y, bgWidth, 128);
        var_s2 = Menu_Framework_GradientStopColor(y, bgWidth, 128);

        if (bgWidth & 7) {
            menuItem->backgroundWidth = (bgWidth + 1) & 0xF;
        }

        if (y != 0) {
            menuItem->gradientState = y - 1;
        }

        x = *(int*)&menuItem->x;
        y = x >> 0x10;
        x &= 0xFFFF;

        Render_InsertThreeWordPrimitive(scratch + 1, 0x60000000, vs_getXY_2(x + 2, y + 2), w | 0xC0000);

        prim = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);

        prim[0] = ((*scratch & 0xFFFFFF) | 0x0A000000);
        prim[1] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_On);
        prim[2] = vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4, c);
        prim[3] = (x | (y << 0x10));
        prim[4] = var_s2;
        prim[5] = (((x + w) & 0xFFFF) | (y << 0x10));
        prim[6] = c;
        prim[7] = (x | ((y + 12) << 0x10));
        prim[8] = var_s2;
        prim[9] = (((x + w) & 0xFFFF) | ((y + 12) << 0x10));
        prim[10] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);

        *scratch = (u_int)((uintptr_t)prim << 8) >> 8;
        *(void**)((uintptr_t)Core_ScratchBase() + 0x000) = prim + 11;

        v = &menuItem->subText;
        if (*v != NULL) {
            Render_DrawCompactText(*v, (x - 10) | ((y + 1) << 0x10), scratch);
        }
    }
}

// decomp: BATTLE.PRG func_800C930C (src/BATTLE/BATTLE.PRG/5BF94.c:2829)
int Menu_Framework_UpdateMainMenuSelection(int mode)
{
    u_int selected = Menu_Framework_submenuStates[0];
    Menu_Framework_MenuRow* items = Menu_Framework_menuItems;
    u_int enabled = Input_settings.menuFlags;
    u_int mask;

    if (mode) {
        if (mode == 2 && !Input_settings.cursorMemory) {
            selected = 0;
        }

        while (!((enabled >> selected) & 1)) {
            selected = (selected + 1) % 10;
        }

        Menu_Framework_submenuStates[0] = selected;
        Core_Memory_ZeroWordsBackward(Menu_Framework_menuItems, 0xB24);
        mask = 9;

        for (mode = 0; mode < 10; ++mode) {
            if ((enabled >> mode) & 1) {
                items[mode].animationStep = mask++;
            }
        }

        Menu_Framework_mainMenuCursorAnimation = -mask;
        return 0;
    }

    if (Menu_Framework_mainMenuCursorAnimation < 0) {
        ++Menu_Framework_mainMenuCursorAnimation;

        for (selected = 0, mode = 0; mode < 10; ++mode) {
            u_char step;

            if (!((enabled >> mode) & 1)) {
                continue;
            }

            step = items[mode].animationStep;

            if (step != 0) {
                items[mode].animationStep = --step;
            }

            Menu_Framework_SetMenuItem(mode, Menu_Framework_rowAnimationSteps[step] + 194,
                selected * 16 + 18, 126, mode == 10 ? 24 : 0,
                (char*)&Menu_menuStrings[Menu_menuStrings[mode]]);
            ++selected;
            mask = Menu_Framework_actionRestrictions;
            items[0].unselectable = (mask & 0xB7) != 0;
            items[1].unselectable = (mask & 0x15F) != 0;
            items[3].unselectable = (mask & 7) != 0;
            items[5].unselectable = Field_CurrentSceneId() == 0;
        }

        return 0;
    }

    if (Input_buttonsPressed.allButtons & PADRright) {
        Menu_PlaySelectSound();
        items[selected].backgroundWidth = items[selected].selected = 1;
        Menu_textBoxes[7].state = 0;
        return selected + 1;
    }

    items[selected].selected = 0;

    if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
        Menu_PlayLeaveSound();
        Menu_textBoxes[7].state = 0;
        return -1;
    }

    if (Input_buttonRepeat & PADLup) {
        do {
            selected = (selected + 9) % 10;
        } while (!((enabled >> selected) & 1));
    }

    if (Input_buttonRepeat & PADLdown) {
        selected = (selected + 1) % 10;
    }
    while (!((enabled >> selected) & 1)) {
        selected = (selected + 1) % 10;
    }

    if (selected != Menu_Framework_submenuStates[0]) {
        Menu_PlayChangeSound();
        Menu_Framework_submenuStates[0] = selected;
    }

    items[selected].selected = 1;
    Menu_Framework_mainMenuCursorAnimation = Menu_Framework_DrawMainMenuCursor(Menu_Framework_mainMenuCursorAnimation, (items[selected].y >> 4) - 1);

    if (items[selected].unselectable) {
        u_int conditions;

        if (Input_buttonsState & (PADLup | PADLdown)) {
            return 0;
        }

        Menu_InitInformationTextBox(1);
        Menu_textBoxes[7].state = 11;
        Menu_SetTextBoxText(7, Menu_Framework_kMainMenuHelpText);
        mask = 0;

        if (selected == 0) {
            mask = 0xB7;
        }

        if (selected == 1) {
            mask = 0x15F;
        }

        if (selected == 3) {
            mask = 7;
        }

        conditions = Menu_Framework_actionRestrictions & mask;

        for (mode = 0; mode < 9; ++mode) {
            if (conditions & (1 << mode)) {
                break;
            }
        }

        Battle_stringContext.strings[0] =
            (char*)&Menu_menuStrings[Menu_menuStrings[mode + 12]];
    } else {
        Menu_textBoxes[7].state = 0;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800C97BC (src/BATTLE/BATTLE.PRG/5BF94.c:2966)
void Menu_Framework_UpdateMenuOverlay(void)
{
    Menu_Framework_MenuRow* var_s2;
    int temp_s1;
    int miscIndex;
    int i;
    int var_s0;

    var_s2 = Menu_Framework_menuItems;
    var_s0 = Menu_Framework_menuState.currentMenu;

    if (var_s0 == 0x7F) {
        Menu_Framework_actionSelection.menuStatus = 2;
        Menu_Framework_menuState.currentMenu = 0x3F;
    }

    temp_s1 = var_s0 & 0x3F;

    if (temp_s1 != 0x3F) {
        if (Menu_Framework_LoadMenuOverlay(0) != 0) {

            if (Menu_Framework_shortcutInvoked == 5) {
                Menu_Inventory_UpdateMiscItemShortcut(0);
                return;
            }

            Menu_Framework_UpdateMainMenu(var_s0);
            return;
        }

        if (temp_s1 == 0) {

            miscIndex = Menu_Framework_UpdateMainMenuSelection(0);

            if (miscIndex != 0) {

                var_s0 = 0x1F;

                if (miscIndex > 0) {
                    var_s0 = miscIndex | 0x40;
                }

                Menu_Framework_menuState.currentMenu = var_s0;
            }
        }

        if ((var_s0 >= 0x1F) || (temp_s1 == 0)) {
            for (i = 0; i < 10; ++i) {
                Menu_Framework_DrawMenuItem(var_s2);
                ++var_s2;
            }
        }
    }
}

// decomp: BATTLE.PRG func_800CA9C0 (src/BATTLE/BATTLE.PRG/5BF94.c:3484)
void Menu_Framework_InitGameInterface(void* arg0)
{
    (*(char (*))&Battle_reuseSphereVolumes) = 0;
    Menu_Framework_loadedSubMenu = 0;
    Battle_hudStatusIconsHidden = 0;
    Battle_hudSlideOffset = 0;
    Menu_Framework_unusedInterfaceFlag = 0;
    Effects_screenEffectOverlay = NULL;
    (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere) = NULL;
    (*(Render_GimLayer* (*))&Render_gimLayers) = NULL;
    Menu_Framework_menuItems = 0;
    Menu_textBoxBufferIndex = 0;
    Battle_lowerScreenUiState = 0;
    Battle_hudHidden = 0;
    Battle_hudBattleMode = 0;
    Battle_battleModeIndicatorStep = 0;
    Battle_statusIconCarousel.word = 0xFFFFFF;
    Menu_Framework_menuBackgroundBuffer = NULL;
    Menu_sceneNotificationBuffer = 0;
    Effects_ResetScreenEffectState();
    Core_Memory_ZeroWordsBackward(&Menu_Framework_menuState, sizeof(Menu_Framework_MenuControlState));
    Menu_Framework_menuState.currentMenu = 0x3F;
    Core_Memory_ZeroWordsBackward(&Menu_Framework_actionSelection, sizeof Menu_Framework_actionSelection);
    Core_Memory_ZeroWordsBackward(&Menu_textBoxes[0], sizeof Menu_textBoxes[0]);
    Core_Memory_ZeroWordsBackward(Menu_Framework_submenuStates, sizeof Menu_Framework_submenuStates);
    Menu_Framework_menuMemory = (Menu_Framework_MenuMemory) { { 0 } };
    Menu_activeTextBoxOrderingTable = &Menu_textBoxOrderingTables;
    Render_UploadImageRectangle(0x340, arg0, 0xE00040);
    Render_UploadImageRectangle(0x380, arg0 + 0x7000, 0xE00040);
    Render_UploadImageRectangle(0x4203C0, arg0 + 0xE000, 0x9E0040);
    ClearOTag(Menu_activeTextBoxOrderingTable, 0x22);
}

// decomp: BATTLE.PRG func_800CAB40 (src/BATTLE/BATTLE.PRG/5BF94.c:3518)
void Menu_Framework_ResumeAfterActionSelection(void)
{
    Menu_Framework_actionSelection.selectionKind = 0;
    Menu_Framework_actionSelection.menuStatus = 1;
    if (Menu_Framework_shortcutInvoked == 5) {
        Menu_Framework_menuState.currentMenu = 4;
        Menu_Inventory_UpdateMiscItemShortcut(2);
    } else {
        Menu_Framework_menuState.currentMenu = Menu_Framework_menuState.returnState;
        Menu_Framework_submenuStates[Menu_Framework_menuState.returnState & 0xF] =
            Menu_Framework_menuState.executeAbilityType;
    }
    if (Menu_Framework_openedWithoutMainMenu == 0) {
        Render_ReserveTexturePageRange(0x1A, 3, 6);
    }
}

// decomp: BATTLE.PRG func_800CABE0 (src/BATTLE/BATTLE.PRG/5BF94.c:3535)
int Menu_Framework_ActionRestrictionFlags(int arg0)
{
    int ret;
    Battle_ActorStats* actor = Battle_characterState->stats;
    int temp_v0 = Actors_ActorStateFlags(0);
    int var_a0 = ((temp_v0 & 0x80000) != 0) * 2;

    if ((temp_v0 & 0x40100000) == 0x100000) {
        var_a0 |= 4;
    }

    if (!(Battle_characterState->model->weaponDrawn)) {
        var_a0 |= 8;
    }

    ret = var_a0;

    if (actor->currentMp == 0) {
        ret |= 0x20;
    }

    if (actor->currentHp == 0) {
        ret |= 0x40;
    }

    if (actor->statuses & (Battle_StatusFlag_Regen | Battle_StatusFlag_StrDown)) {
        ret |= 0x80;
    }

    if (actor->statuses & Battle_StatusFlag_Ward) {
        ret |= 0x100;
    }

    if (arg0 == 2) {
        ret |= 0x200;
    }

    return ret;
}

// decomp: BATTLE.PRG func_800CACD0 (src/BATTLE/BATTLE.PRG/5BF94.c:3575)
int Menu_Framework_OpenGameMenu(int menuState, int arg1)
{
    int i;
    u_short var_a1;
    int var_a0;
    void* miscIndex;

    if (Menu_Framework_menuState.currentMenu == 0x3F) {
        Menu_Framework_shortcutInvoked = 0;
        Menu_Framework_openedWithoutMainMenu = 0;
        Menu_Framework_actionRestrictions = Menu_Framework_ActionRestrictionFlags(arg1);
        Menu_Framework_actionSelection.selectionKind = 0;
        Menu_Framework_menuState.currentMenu = menuState;
        Menu_Framework_actionSelection.menuStatus = 1;

        if (arg1 == 0) {
            Menu_PlayOpenSound();
        }

        if (Menu_Framework_menuItems == 0) {
            Menu_Framework_loadedSubMenu = -1;

            Content_AcquireOverlayWorkspace(6);

            miscIndex = Core_Memory_AllocateFromHighEnd(0xB24);
            var_a1 = Input_settings.menuFlags;
            Menu_Framework_stringBuf = miscIndex + 0xA00;
            Menu_Framework_menuItems = miscIndex;
            Menu_Framework_rowTypeBuf = miscIndex + 0xA60;

            for (i = 0; i < 3; ++i) {
                for (var_a0 = Menu_Framework_abilityGroupActionRanges[i * 2]; var_a0 < Menu_Framework_abilityGroupActionRanges[i * 2 + 1];
                    ++var_a0) {
                    if (Battle_actionDefinitions[var_a0].unlocked) {
                        var_a1 |= 1 << i;
                        break;
                    }
                }
            }

            if (Audio_Seq_unusedStateFlags.opaqueB5 != 0) {
                var_a1 |= 1;
            }
            if (Audio_Seq_unusedStateFlags.introState >= 3) {
                var_a1 |= 0x120;
            }
            Input_settings.menuFlags = var_a1;
            Menu_Framework_UpdateMainMenuSelection(2);
        }
        return 1;
    }

    Menu_PlayInvalidSound();
    return 0;
}

// decomp: BATTLE.PRG vs_battle_validateShortcutSelection (src/BATTLE/BATTLE.PRG/5BF94.c:3673)
int Menu_Framework_OpenShortcutMenu(int shortcut)
{
    int noneUnlocked = 0;
    int menuState = 0;

    switch (shortcut) {
    case 1:
    case 2:
    case 3:
    case 4:
        noneUnlocked = Battle_HasUnlockedSpellClass(shortcut - 1) == 0;
        menuState = 1;
        break;
    case 5:
        menuState = 4;
        break;
    case 6:
        noneUnlocked = Battle_HasUnlockedWeaponBreakArt() == 0;
        menuState = 2;
        break;
    case 7:
    case 8:
        noneUnlocked = Battle_HasUnlockedBattleAbility(shortcut - 7) == 0;
        menuState = 3;
        break;
    }

    if (noneUnlocked) {
        Menu_PlayInvalidSound();
        menuState = 0;
        Menu_Framework_actionSelection.selectionKind = 0;
        Menu_Framework_actionSelection.menuStatus = 2;
    } else {
        menuState = Menu_Framework_OpenGameMenu(menuState, 0);
        Menu_Framework_shortcutInvoked = shortcut;
        Menu_Framework_openedWithoutMainMenu = 1;
    }

    return menuState;
}

// decomp: BATTLE.PRG func_800CB114 (src/BATTLE/BATTLE.PRG/5BF94.c:3714)
void Menu_Framework_ReleaseMenuRows(void)
{
    if (Menu_Framework_menuItems != 0) {
        Content_ReleaseOverlayWorkspace(6);
        Core_Memory_FreeFromHighEnd(Menu_Framework_menuItems);
        Menu_Framework_menuItems = 0;
    }
}

// decomp: BATTLE.PRG func_800CB158 (src/BATTLE/BATTLE.PRG/5BF94.c:3723)
void Menu_Framework_OpenLootListMenu(Items_LootListNode* arg0)
{
    Menu_Framework_lootLists = arg0;
    Menu_Framework_lootEntry = NULL;
    Menu_Framework_OpenGameMenu(0xC, 2);
}

// decomp: BATTLE.PRG func_800CB18C (src/BATTLE/BATTLE.PRG/5BF94.c:3730)
void Menu_Framework_OpenLootEntryMenu(Items_BattleLoot* arg0)
{
    Menu_Framework_lootEntry = arg0;
    Menu_Framework_lootLists = NULL;
    Menu_Framework_OpenGameMenu(0xC, 2);
}

// decomp: BATTLE.PRG func_800CB1C0 (src/BATTLE/BATTLE.PRG/5BF94.c:3737)
void Menu_Framework_OpenChestStorageMenu(int arg0)
{
    Menu_Framework_OpenGameMenu(7, 2);
    Menu_saveLocationId = arg0 + 0x40;
    Menu_Framework_actionRestrictions |= 0x400;
}

// decomp: BATTLE.PRG _setArtAndAbilityToUnlock (src/BATTLE/BATTLE.PRG/5BF94.c:3744)
void Menu_Framework_OpenAbilityUnlockMenu(int art, int battleAbility)
{
    Menu_Framework_unlockedBreakArt = art;
    Menu_Framework_unlockedBattleAbility = battleAbility;
    Menu_Framework_OpenGameMenu(3, 2);
}

// decomp: BATTLE.PRG func_800CB23C (src/BATTLE/BATTLE.PRG/5BF94.c:3751)
void Menu_Framework_OpenChallengeMenu(void)
{
    Menu_Framework_OpenGameMenu(0xF, 2);
    Menu_Framework_openedWithoutMainMenu = 1;
}

// decomp: BATTLE.PRG func_800CB66C (src/BATTLE/BATTLE.PRG/5BF94.c:3871)
Menu_Framework_ActionSelection* Menu_Framework_ActionSelectionState(void) { return &Menu_Framework_actionSelection; }

// decomp: BATTLE.PRG vs_battle_drawCursor (src/BATTLE/BATTLE.PRG/5BF94.c:4523)
int Menu_Framework_DrawMainMenuCursor(int animStep, int position)
{
    Render_QueueSpriteWithTexturePage(Menu_Framework_cursorBrightnessAnimation[animStep],
        (((position * 0x10) + 0xA) << 0x10) | 0xB4, 0x100010, Render_frameScratch.overlayOrderingTable)[4] =
        0x37F83020;
    return (animStep + 1) & 0xF;
}

// decomp: MAINMENU.PRG vs_mainMenu_addMenuActions (src/MENU/MAINMENU.PRG/58EC.c:51)
void Menu_Framework_CreateActionRows(
    int rowCount, int subMenuId, char* menuText[], int rowTypes[])
{
    int scrollableRows;

    int i = 0;
    int scrollOffset = 0;
    Menu_Framework_menuActionsPageOffset = rowCount >> 8;
    rowCount &= 0xFF;

    if (!Menu_Framework_menuActionsPageOffset) {
        if (rowCount < 7) {
            Menu_Framework_menuActionsPageOffset = 10 - rowCount;
        } else {
            Menu_Framework_menuActionsPageOffset = 3;
        }
    }

    Menu_Framework_actionsSubMenuId = subMenuId;

    if (subMenuId == Menu_Framework_ActionsMenuAction_Discard) {
        i = 1;
    } else if ((Input_settings.cursorMemory != 0) && (subMenuId != 3)) {
        i = Menu_Framework_menuMemory.cursorMemories[subMenuId * 2];
        scrollOffset = Menu_Framework_menuMemory.cursorMemories[subMenuId * 2 + 1];
    }

    scrollableRows = rowCount - 7;

    if (scrollableRows < 0) {

        i += scrollOffset;
        scrollOffset = 0;

        if (i >= rowCount) {
            i = 0;
        }

    } else if (scrollableRows < scrollOffset) {

        if ((i + scrollOffset) >= rowCount) {
            i = 0;
            scrollOffset = 0;
        } else {
            i += scrollOffset - scrollableRows;
            scrollOffset = scrollableRows;
        }
    }

    if ((scrollOffset > 0) && (i == 0)) {
        i = 1;
        --scrollOffset;
    }

    if ((scrollOffset < scrollableRows) && (i == 6)) {
        i = 5;
        ++scrollOffset;
    }

    Menu_Framework_selectedItemAction = i;
    Menu_Framework_itemActionListOffset = scrollOffset;
    Menu_Framework_itemActionCount = rowCount;
    Menu_Framework_actionMenuState = 0;

    Menu_Framework_sortItemsTextBuf = Core_Memory_AllocateFromHighEnd(rowCount * 128);

    for (i = 0; i < rowCount; ++i) {
        char* text;

        ((Menu_Framework_ItemText*)Menu_Framework_sortItemsTextBuf)[i].menuRowFlags = rowTypes[i];

        if (menuText[i * 2] != NULL) {
            Core_Memory_CopyHalfwords(&Menu_Framework_sortItemsTextBuf[i * 64], menuText[i * 2], 26);

            Menu_Framework_sortItemsTextBuf[(i * 64) + 13] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        } else {
            Menu_Framework_sortItemsTextBuf[i * 64] = Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        }

        text = menuText[i * 2 + 1];

        if (text != NULL) {
            ((Menu_Framework_ItemText*)Menu_Framework_sortItemsTextBuf)[i].description[0] = Core_TextCode_ChunkSize;

            Core_Memory_CopyHalfwords(&Menu_Framework_sortItemsTextBuf[(i * 64) + 17], text, 92);

            ((Menu_Framework_ItemText*)Menu_Framework_sortItemsTextBuf)[i].description[47] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        } else {
            ((Menu_Framework_ItemText*)Menu_Framework_sortItemsTextBuf)[i].description[0] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_getSelectedAction (src/MENU/MAINMENU.PRG/58EC.c:147)
int Menu_Framework_HighlightedActionRow(void)
{
    return Menu_Framework_selectedItemAction + Menu_Framework_itemActionListOffset;
}

// decomp: MAINMENU.PRG vs_mainMenu_actionInputProcessed (src/MENU/MAINMENU.PRG/58EC.c:152)
int Menu_Framework_ActionSelectionResult(void)
{
    return Menu_Framework_sortItemsTextBuf == NULL ? Menu_Framework_selectedItemAction : -1;
}

// decomp: MAINMENU.PRG _renderActionRow (src/MENU/MAINMENU.PRG/58EC.c:162)
Menu_Framework_MenuRow* Menu_Framework_CreateActionRow(int row, int x)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(row + 32, x,
        ((row + Menu_Framework_menuActionsPageOffset) * 16) + 18, 126, 0,
        (char*)((Menu_Framework_ItemText*)Menu_Framework_sortItemsTextBuf)[Menu_Framework_itemActionListOffset + row].name);

    int flags =
        ((Menu_Framework_ItemText*)Menu_Framework_sortItemsTextBuf)[Menu_Framework_itemActionListOffset + row].menuRowFlags;

    menuItem->unselectable = flags & 1;

    if ((row == 0) && (Menu_Framework_itemActionListOffset != 0)) {
        menuItem->isScrollable = Menu_Framework_MenuItemScroll_Up;
    }

    return menuItem;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderItemActionMenu (src/MENU/MAINMENU.PRG/58EC.c:180)
void Menu_Framework_UpdateActionMenu(void)
{

    int selectedAction;
    int i;
    int previousOffset;
    Menu_Framework_MenuRow* menuItem;

    if (Menu_Framework_actionMenuState < 10) {
        menuItem = Menu_Framework_CreateActionRow(Menu_Framework_actionMenuState, -194);
        menuItem->state = 5;
        menuItem->targetPosition0 = 0;

        ++Menu_Framework_actionMenuState;

        if (Menu_Framework_actionMenuState == Menu_Framework_itemActionCount) {
            Menu_Framework_actionMenuState = 16;
        }

        if (Menu_Framework_actionMenuState == 7) {

            if ((Menu_Framework_actionMenuState + Menu_Framework_itemActionListOffset)
                < Menu_Framework_itemActionCount) {
                menuItem->isScrollable = 2;
            }

            Menu_Framework_actionMenuState = 16;
        }

    } else {

        menuItem = Menu_Framework_MenuItem(Menu_Framework_selectedItemAction + 32);

        Menu_Framework_SetInformationMessage(
            (char*)(Menu_Framework_sortItemsTextBuf
                    + (((Menu_Framework_selectedItemAction + Menu_Framework_itemActionListOffset) * 64) + 16)));

        switch (Menu_Framework_actionMenuState) {
        case 16:
            if (Menu_Framework_AreMenuRowsIdle() != 0) {
                Menu_Framework_actionMenuState = 17;
            }
            break;

        case 17:
            selectedAction = Menu_Framework_selectedItemAction + Menu_Framework_itemActionListOffset;
            previousOffset = Menu_Framework_itemActionListOffset;

            if (Input_buttonsPressed.allButtons & PADRup) {

                Core_Memory_FreeFromHighEnd(Menu_Framework_sortItemsTextBuf);

                Menu_Framework_sortItemsTextBuf = NULL;
                Menu_Framework_menuMemory.cursorMemories[Menu_Framework_actionsSubMenuId * 2] = Menu_Framework_selectedItemAction;
                Menu_Framework_menuMemory.cursorMemories[Menu_Framework_actionsSubMenuId * 2 + 1] =
                    Menu_Framework_itemActionListOffset;
                Menu_Framework_selectedItemAction = -3;
                break;
            }

            if (Input_buttonsPressed.allButtons & PADRright) {
                if (menuItem->unselectable == 0) {
                    menuItem->selected = 1;
                    Core_Memory_FreeFromHighEnd(Menu_Framework_sortItemsTextBuf);
                    Menu_Framework_sortItemsTextBuf = NULL;
                    Menu_Framework_menuMemory.cursorMemories[Menu_Framework_actionsSubMenuId * 2] =
                        Menu_Framework_selectedItemAction;
                    Menu_Framework_menuMemory.cursorMemories[Menu_Framework_actionsSubMenuId * 2 + 1] =
                        Menu_Framework_itemActionListOffset;
                    Menu_Framework_selectedItemAction = selectedAction;
                    break;
                }
                Menu_PlayInvalidSound();
            }

            menuItem->selected = 0;

            if (Input_buttonsPressed.allButtons & PADRdown) {
                Core_Memory_FreeFromHighEnd(Menu_Framework_sortItemsTextBuf);
                Menu_Framework_sortItemsTextBuf = NULL;
                Menu_Framework_menuMemory.cursorMemories[Menu_Framework_actionsSubMenuId * 2] = Menu_Framework_selectedItemAction;
                Menu_Framework_menuMemory.cursorMemories[Menu_Framework_actionsSubMenuId * 2 + 1] =
                    Menu_Framework_itemActionListOffset;
                Menu_Framework_selectedItemAction = -2;
                break;
            }

            if (Input_buttonRepeat & PADLup) {
                if (Menu_Framework_itemActionCount < 8) {
                    if (Menu_Framework_selectedItemAction == 0) {
                        Menu_Framework_selectedItemAction = Menu_Framework_itemActionCount - 1;
                    } else {
                        --Menu_Framework_selectedItemAction;
                    }
                } else if (Menu_Framework_itemActionListOffset == 0) {
                    if (Menu_Framework_selectedItemAction == 0) {
                        if (Input_buttonsPressed.allButtons & PADLup) {
                            Menu_Framework_selectedItemAction = 6;
                            Menu_Framework_itemActionListOffset = Menu_Framework_itemActionCount - 7;
                        }
                    } else {
                        --Menu_Framework_selectedItemAction;
                    }
                } else if (Menu_Framework_selectedItemAction == 1) {
                    --Menu_Framework_itemActionListOffset;
                } else {
                    --Menu_Framework_selectedItemAction;
                }
            }

            if (Input_buttonRepeat & PADLdown) {
                if (Menu_Framework_itemActionCount < 8) {
                    if (Menu_Framework_selectedItemAction == (Menu_Framework_itemActionCount - 1)) {
                        Menu_Framework_selectedItemAction = 0;
                    } else {
                        ++Menu_Framework_selectedItemAction;
                    }
                } else if (Menu_Framework_itemActionListOffset == (Menu_Framework_itemActionCount - 7)) {
                    if (Menu_Framework_selectedItemAction == 6) {
                        if (Input_buttonsPressed.allButtons & PADLdown) {
                            Menu_Framework_selectedItemAction = 0;
                            Menu_Framework_itemActionListOffset = 0;
                        }
                    } else {
                        ++Menu_Framework_selectedItemAction;
                    }
                } else if (Menu_Framework_selectedItemAction == 5) {
                    Menu_Framework_itemActionListOffset = Menu_Framework_itemActionListOffset + 1;
                } else {
                    Menu_Framework_selectedItemAction += 1;
                }
            }

            if (selectedAction != (Menu_Framework_selectedItemAction + Menu_Framework_itemActionListOffset)) {
                Menu_PlayChangeSound();

                if (Menu_Framework_itemActionListOffset != previousOffset) {
                    int actionCount;

                    char gradientStates[Menu_Framework_itemActionCount];

                    for (i = 0; i < Menu_Framework_itemActionCount; ++i) {
                        gradientStates[i] = 0;
                    }

                    actionCount = Menu_Framework_itemActionCount;

                    if (actionCount > 7u) {
                        actionCount = 7;
                    }

                    for (i = 0; i < actionCount; ++i) {
                        gradientStates[i + previousOffset] =
                            Menu_Framework_MenuItem(i + 32)->gradientState;
                    }

                    for (i = 0;;) {
                        menuItem = Menu_Framework_CreateActionRow(i, 0);
                        menuItem->gradientState =
                            gradientStates[i + Menu_Framework_itemActionListOffset];
                        ++i;

                        if (i == Menu_Framework_itemActionCount) {
                            break;
                        }

                        if (i == 7) {
                            if ((Menu_Framework_itemActionListOffset + 7) < Menu_Framework_itemActionCount) {
                                menuItem->isScrollable = 2;
                            }
                            break;
                        }
                    }
                }
            }

            menuItem = Menu_Framework_MenuItem(Menu_Framework_selectedItemAction + 32);
            menuItem->selected = 1;
            Menu_Framework_actionMenuCursorAnimStep =
                Menu_Framework_DrawCursor(Menu_Framework_actionMenuCursorAnimStep, vs_getXY(0, menuItem->y - 8));

            break;
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_printInformation (src/MENU/MAINMENU.PRG/58EC.c:367)
void Menu_Framework_UpdateInformationVisibility(int sourceRow, int actionMenuState)
{
    if (actionMenuState < 16) {
        return;
    }

    if (Menu_Framework_rowTypeBuf[sourceRow] & 1) {

        if ((Input_settings.information == 0)
            && (Input_buttonsState & (PADLup | PADLdown))) {
            return;
        }

        Menu_InitInformationTextBox(Menu_InformationBoxHeader_Caution);

        Menu_textBoxes[7].state = 11;

    } else if (Input_settings.information != 0) {

        Menu_InitInformationTextBox(Menu_InformationBoxHeader_Information);

        Menu_textBoxes[7].state = 11;

    } else {
        Menu_textBoxes[7].state = 0;
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_setMenuCommand (src/MENU/MAINMENU.PRG/58EC.c:405)
void Menu_Framework_SetCommandLabel(enum Menu_Framework_MenuCommand action)
{
    Menu_Framework_menuActionNext = action;
}

// decomp: MAINMENU.PRG _renderMenuCommand (src/MENU/MAINMENU.PRG/58EC.c:413)
void Menu_Framework_DrawCommandLabel(void)
{
    static int Menu_Framework_commandLabelMenuActionRenderPostitions[] = { vs_getXY(0, 0), vs_getXY(62, 10),
        vs_getXY(36, 10), vs_getXY(30, 10), vs_getXY(56, 10), vs_getXY(30, 10),
        vs_getXY(32, 10), vs_getXY(0, 0) };

    u_long* prim;
    int action = Menu_Framework_menuActionCurrent;
    int y = Menu_Framework_commandLabelActionYPos;

    if (action != Menu_Framework_menuActionNext) {
        if (y == 0) {
            Menu_Framework_menuActionCurrent = Menu_Framework_menuActionNext;
            return;
        }
        y -= 2;
    } else if (y < 12) {
        y += 2;
    }

    Menu_Framework_commandLabelActionYPos = y;

    if (action != 0) {
        prim = Render_QueueSprite((1 << 8) | 128,
            (312 - (Menu_Framework_commandLabelMenuActionRenderPostitions[action] & 0xFF))
                | ((Menu_Framework_commandLabelActionYPos - 4) << 16),
            Menu_Framework_commandLabelMenuActionRenderPostitions[action], Render_frameScratch.overlayOrderingTable);
        prim[1] = vs_getTpage(768, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off);
        prim[4] = Menu_Framework_commandLabelMenuActionUvs[action] | vs_getUV0Clut(0, 0, 848, 223);
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_toggleBackgroundFade (src/MENU/MAINMENU.PRG/58EC.c:448)
void Menu_Framework_SetBackgroundFade(int fadeState)
{
    if (fadeState != 0) {
        Menu_Framework_backgroundOtOffset = -4;
        Menu_Framework_backgroundBrightness = 128;
    }

    Menu_Framework_backgroundFadeState = fadeState;
}

// decomp: MAINMENU.PRG vs_mainMenu_deactivateMenuItem (src/MENU/MAINMENU.PRG/58EC.c:458)
void Menu_Framework_DeactivateRow(int row)
{
    Menu_Framework_MenuRow* menuItem = &Menu_Framework_menuItems[row];
    menuItem->state = Menu_Framework_MenuItemState_Inactive;
}

// decomp: MAINMENU.PRG vs_mainMenu_dismissInformationBox (src/MENU/MAINMENU.PRG/58EC.c:464)
void Menu_Framework_DismissInformationBox(void) { Menu_DismissTextBox(7); }

// decomp: MAINMENU.PRG vs_mainMenu_initInformationBox (src/MENU/MAINMENU.PRG/58EC.c:466)
void Menu_Framework_InitializeInformationBox(void)
{
    if (Input_settings.information != 0) {
        Menu_InitInformationTextBox(Menu_InformationBoxHeader_Information);
    } else {
        Menu_Framework_DismissInformationBox();
    }
}

// decomp: MAINMENU.PRG vs_mainmenu_setInformationMessage (src/MENU/MAINMENU.PRG/58EC.c:475)
void Menu_Framework_SetInformationMessage(char* message)
{
#pragma vsstring(start)
    static char Menu_Framework_setInformationMessageInfoMessage[] =
        "|!0|000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000\0\0";
#pragma vsstring(end)

    Core_Memory_CopyHalfwordBlocks(&Menu_Framework_setInformationMessageInfoMessage[2], message, 96);
    Menu_textBoxes[7].flags.speakerTexture = 32;
    Menu_SetTextBoxText(7, Menu_Framework_setInformationMessageInfoMessage);
}

// decomp: MAINMENU.PRG vs_mainmenu_renderButton (src/MENU/MAINMENU.PRG/58EC.c:487)
void Menu_Framework_DrawButtonIcon(
    enum Menu_Framework_ButtonId buttonId, int x, int y, u_long* before)
{
    if (before == NULL) {
        before = Render_frameScratch.overlayOrderingTable;
    }

    Render_QueueSpriteWithTexturePage(
        128, vs_getXY_2(x, y), vs_getWH(16, 16), before)[4] =
        vs_getUV0Clut((buttonId & 3) * 16, ((buttonId & 4) * 4) + 128, 944, 223);
}

// decomp: MAINMENU.PRG vs_mainMenu_renderCursor (src/MENU/MAINMENU.PRG/58EC.c:499)
int Menu_Framework_DrawCursor(u_int animStep, int xy)
{
    int clut;
    int cursorAnimStep = animStep >> (Menu_Framework_cursorColor * 8);

    u_long* prim = Render_QueueSpriteWithTexturePage(
        Menu_Framework_cursorBrightnessAnimation[cursorAnimStep], xy, vs_getWH(16, 16),
        Render_frameScratch.overlayOrderingTable);

    if (Menu_Framework_cursorColor == 0) {
        clut = vs_getUV0Clut(32, 48, 896, 223);
    } else {
        clut = vs_getUV0Clut(32, 48, 928, 223);
    }

    prim[4] = clut;
    Menu_Framework_cursorColor = 0;

    return (cursorAnimStep + 1) & 0xF;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderIntColor (src/MENU/MAINMENU.PRG/58EC.c:520)
int Menu_Framework_DrawColoredInteger(int value, int xy, int color, u_long* before)
{
    int buf;
    int bcd = value;

    if (value < 0) {
        bcd = -value;
    }

    do {
        bcd = Core_Math_SplitDecimalDigit(bcd);
        buf = ((bcd & 0xF) << 8) | ('0' << 8) | '#';

        Render_DrawCompactTextColor((char*)&buf, xy, color, before);

        bcd >>= 4;
        xy -= 8;
    } while (bcd != 0);

    if (value < 0) {
        buf = ('-' << 8) | '#';
        Render_DrawCompactTextColor((char*)&buf, xy + 1, color, before);
    }

    return xy;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderIntWithThreshold (src/MENU/MAINMENU.PRG/58EC.c:547)
int Menu_Framework_DrawComparedInteger(int value, int xy, int threshold, u_long* before)
{
    int color = vs_getRGB888(128, 128, 128);

    if (threshold < value) {
        color = vs_getRGB888(32, 64, 128);
    }

    if (value < threshold) {
        color = vs_getRGB888(128, 64, 32);
    }

    return Menu_Framework_DrawColoredInteger(value, xy, color, before);
}

// decomp: MAINMENU.PRG vs_mainMenu_renderIntColorDefault (src/MENU/MAINMENU.PRG/58EC.c:562)
void Menu_Framework_DrawNeutralInteger(int value, int xy, u_long* before)
{
    Menu_Framework_DrawColoredInteger(value, xy, vs_getRGB888(128, 128, 128), before);
}

// decomp: MAINMENU.PRG _renderFeatheredIcon (src/MENU/MAINMENU.PRG/58EC.c:571)
void Menu_Framework_DrawFeatheredRowIcon(int direction, int xy, int uvClut, u_long* before)
{
    int i = 0;

    if (direction < 0) {
        xy += vs_getXY(0, 15);
        uvClut += vs_getUV0Clut(0, 15, 0, 0);
    }

    do {
        ++i;

        Render_QueueSpriteWithTexturePage(
            (i * 8) | (1 << 8), xy, vs_getWH(16, 1), before)[4] = uvClut;

        xy += vs_getXY(0, direction);
        uvClut += vs_getUV0Clut(0, direction, 0, 0);
    } while (i < 16);
}

// decomp: MAINMENU.PRG _renderItemMaterial (src/MENU/MAINMENU.PRG/58EC.c:594)
void Menu_Framework_DrawMaterialIndicator(int materialId, int xy)
{
    u_long* prim = Render_QueueDefaultSprite(vs_getWH(7, 7), xy);
    int uv = (((materialId / 3) * 8) + 216) | ((((materialId % 3) * 8) + 200) << 8);

    if (materialId == 2) {
        prim[4] = uv | (getClut(928, 223) << 16);
    } else {
        prim[4] = uv | (getClut(912, 223) << 16);
    }
}

// decomp: MAINMENU.PRG _renderOutsetIcon (src/MENU/MAINMENU.PRG/58EC.c:609)
void Menu_Framework_DrawEquipmentStateIcon(int icon, int xy)
{
    Render_QueueDefaultSprite(vs_getWH(16, 16), xy)[4] =
        vs_getUV0Clut(96 - (icon * 16), 128, 976, 223);
}

// decomp: MAINMENU.PRG vs_mainMenu_renderMenuRowIcon (src/MENU/MAINMENU.PRG/58EC.c:615)
void Menu_Framework_DrawRowIcon(int icon, int x, int y)
{
    static u_short Menu_Framework_rowIconUvs[] = { vs_getUV(80, 96), vs_getUV(144, 96), vs_getUV(160, 96),
        vs_getUV(112, 96), vs_getUV(128, 96), vs_getUV(176, 96), vs_getUV(208, 96),
        vs_getUV(96, 96), vs_getUV(48, 112), vs_getUV(128, 112), vs_getUV(0, 112),
        vs_getUV(16, 112), vs_getUV(32, 112), vs_getUV(224, 96), vs_getUV(64, 112),
        vs_getUV(80, 112), vs_getUV(96, 112), vs_getUV(112, 112), vs_getUV(128, 112),
        vs_getUV(144, 112), vs_getUV(160, 112), vs_getUV(32, 96), vs_getUV(176, 112),
        vs_getUV(120, 128), vs_getUV(136, 128), vs_getUV(152, 128), vs_getUV(168, 128),
        vs_getUV(184, 128), vs_getUV(200, 128), vs_getUV(216, 128), vs_getUV(232, 128),
        vs_getUV(96, 128), vs_getUV(80, 128) };

    int fade = icon >> 16;
    int brightness = (icon >> 8) & 1;
    icon = (icon - 1) & 0xFF;

    if (fade == 0) {
        u_long* prim = Render_QueueSpriteWithTexturePage(
            64 << brightness, vs_getXY_2(x, y), vs_getWH(16, 16), Render_frameScratch.overlayOrderingTable + 4);

        prim[4] = vs_getUV0Clut(Menu_Framework_rowIconUvs[icon], 0, 992, 223);

        if (icon >= 23) {
            prim[3] -= 12;
            prim[4] = vs_getUV0Clut(Menu_Framework_rowIconUvs[icon], 0, 976, 223);
        }

    } else {
        if (fade == 2) {
            fade = -1;
        }

        Menu_Framework_DrawFeatheredRowIcon(fade, vs_getXY_2(x, y),
            vs_getUV0Clut(Menu_Framework_rowIconUvs[icon], 0, 992, 223), Render_frameScratch.overlayOrderingTable + 4);
    }
}

// decomp: MAINMENU.PRG vs_mainmenu_setActionCost (src/MENU/MAINMENU.PRG/58EC.c:652)
void Menu_Framework_SetActionCostText(int index, char const* text, int xOffset, int disabled)
{
    Menu_Framework_displayActionCost = 1;

    Core_Memory_CopyBytesBackward(&Menu_Framework_textHeaders[index], text, 14);

    Menu_Framework_textHeaders[index].disabled = disabled;
    Menu_Framework_textHeaders[index].x = xOffset;
}

// decomp: MAINMENU.PRG _renderAbilityCost (src/MENU/MAINMENU.PRG/58EC.c:665)
void Menu_Framework_DrawActionCostPanel(void)
{

    u_long* scratch = Render_frameScratch.orderingTable - 4;

    if (Menu_Framework_displayActionCost != 0) {
        if (Menu_Framework_actionCostPanelAnimState != 0) {
            --Menu_Framework_actionCostPanelAnimState;
        }
    } else {
        if (Menu_Framework_actionCostPanelAnimState < 5) {
            Menu_Framework_actionCostPanelAnimState = 5;
        } else if (Menu_Framework_actionCostPanelAnimState < 8) {
            ++Menu_Framework_actionCostPanelAnimState;
        }
    }

    if (Menu_Framework_actionCostPanelAnimState < 8) {
        int i;
        u_long* poly;
        int x0;
        int x1 = -Menu_Framework_rowAnimationSteps[Menu_Framework_actionCostPanelAnimState];

        for (i = 0; i < 2; ++i) {
            Render_DrawCompactTextColor(Menu_Framework_textHeaders[i].text,
                ((x1 + Menu_Framework_textHeaders[i].x) & 0xFFFF) | (160 << 16),
                vs_getRGB888(128, 128, 128) >> Menu_Framework_textHeaders[i].disabled, NULL);
        }

        x0 = x1 & 0xFFFF;
        poly = Render_frameScratch.primitiveCursor;

        poly[0] = (*scratch & 0xFFFFFF) | 0x0A000000;
        poly[1] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_On);
        poly[2] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyG4, 8, 8, 32);
        poly[3] = x0 | (160 << 16);
        poly[4] = vs_getRGB888(64, 48, 102);
        poly[5] = (x1 + 80) | (160 << 16);
        poly[6] = vs_getRGB888(8, 8, 32);
        poly[7] = x0 | (171 << 16);
        poly[8] = vs_getRGB888(64, 48, 102);
        poly[9] = (x1 + 80) | (171 << 16);
        poly[10] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);

        *scratch = ((u_long)poly << 8) >> 8;
        Render_frameScratch.primitiveCursor = poly + 11;

        Render_InsertThreeWordPrimitive(
            scratch, vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0), x0 | (162 << 16), vs_getWH(82, 11));
    }
}

// decomp: MAINMENU.PRG vs_mainmenu_renderButtonBackground (src/MENU/MAINMENU.PRG/58EC.c:718)
void Menu_Framework_DrawButtonPromptBackground(int x, int y, int w, int h)
{
    int i;
    u_long* var_t2 = Render_frameScratch.primitiveCursor;
    u_long* temp_v1 = Render_frameScratch.orderingTable - 0xC;

    for (i = 0; i < h; ++i) {
        var_t2[0] = vs_getTag(u_long[6], temp_v1[0]);
        var_t2[1] = vs_getTpage(0, 0, 0, 0, Render_Dithering_On);
        var_t2[2] = vs_getRGB0(Render_LegacyPrimitiveCode_LineG2, 64, 56, 32);
        var_t2[3] = ((x & 0xFFFF) | ((y + i) << 0x10));
        var_t2[4] = vs_getRGB888(16, 16, 8);
        var_t2[5] = vs_getXY_2((x + w) - i, y + i);
        var_t2[6] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);
        temp_v1[0] = (((uintptr_t)var_t2 << 8) >> 8);
        var_t2 += 7;
    }

    Render_frameScratch.primitiveCursor = var_t2;
}

// decomp: MAINMENU.PRG vs_mainMenu_setBackgroundRenderPriority (src/MENU/MAINMENU.PRG/58EC.c:739)
void Menu_Framework_SetBackgroundPriority(int otOffset, int brightness)
{
    if (otOffset > 0) {
        Battle_lowerScreenUiState |= 2;
    } else {
        Battle_lowerScreenUiState &= 1;
    }

    Menu_Framework_backgroundOtOffset = otOffset;
    Menu_Framework_backgroundBrightness = brightness;
}

// decomp: MAINMENU.PRG vs_mainMenu_unpackMenubg (src/MENU/MAINMENU.PRG/58EC.c:751)
void Menu_Framework_LoadBackgroundTexture(u_int* buf)
{

    int var_a2;
    int i;
    u_int var_v1;
    void* data = Core_overlaySlots[2];
    u_int* temp_t1 = data + 0x200;

    for (i = 0, var_a2 = 0; i < 0x1780;) {

        u_int temp_a0 = temp_t1[i++];

        for (var_v1 = temp_a0 & 0xFFFF; var_v1 != 0; --var_v1) {
            buf[var_a2++] = 0;
        }

        for (var_v1 = temp_a0 >> 16; var_v1 != 0; --var_v1) {
            buf[var_a2++] = temp_t1[i++];
        }
    }

    Render_ReserveTexturePageRange(0x1A, 3, 6);
    ClearImage(&Menu_Framework_loadBackgroundTextureRects[0], 0, 0, 0);
    ClearImage(&Menu_Framework_loadBackgroundTextureRects[1], 0, 0, 0);

    if (((Menu_Framework_menuState.currentMenu & 0x3F) == 7) && (Menu_Framework_actionRestrictions & 0x200)
        && (Audio_Seq_unusedStateFlags.gameOver == 1)) {
        Render_UploadImageRectangle(vs_getXY(640, 511), data, vs_getWH(256, 1));
    } else {
        Render_LoadClutColors((u_short*)data, 3, 0, 256);
    }

    Render_UploadImageRectangle(vs_getXY(672, 256), buf, vs_getWH(96, 240));
    Menu_Framework_SetBackgroundPriority(-4, 128);
}

// decomp: MAINMENU.PRG vs_mainmenu_setMenuRows (src/MENU/MAINMENU.PRG/58EC.c:798)
void Menu_Framework_CreateMenuRows(int rowCount, int rowInfo, char* strings[], int rowTypes[])
{
    int temp_v1_2;
    int var_v0;
    u_int var_a0;

    int i = 0;
    int page = 0;
    int subMenuId = rowInfo & 0xFF;
    Menu_Framework_menuRowYOffset = (rowInfo >> 0xC);
    Menu_Framework_subMenuId = subMenuId;
    rowInfo = (rowInfo >> 8) & 0xF;

    if (Input_settings.cursorMemory != 0) {
        i = Menu_Framework_menuMemory.cursorMemories[subMenuId * 2];
        page = Menu_Framework_menuMemory.cursorMemories[subMenuId * 2 + 1];
    }

    temp_v1_2 = 10;
    temp_v1_2 = rowCount - temp_v1_2 + rowInfo;

    if (temp_v1_2 < 0) {
        i += page;
        page = 0;
        if (i >= rowCount) {
            i = 0;
        }
    } else {
        if (temp_v1_2 < page) {
            if ((i + page) >= rowCount) {
                i = 0;
                page = 0;
            } else {
                i += page - temp_v1_2;
                page = temp_v1_2;
            }
        }

        var_v0 = page < temp_v1_2;

        if ((page > 0) && (i == 0)) {
            i = 1;
            page -= 1;
            var_v0 = page < temp_v1_2;
        }

        if (var_v0 != 0) {
            var_a0 = rowCount << 7;
            if (i == (9 - rowInfo)) {
                i -= 1;
                page += 1;
                ;
            }
        }
    }

    Menu_Framework_selectedRow = i;

    Menu_Framework_subMenuPage = page;
    Menu_Framework_subMenuRowCount = rowCount;
    Menu_Framework_subMenuRowInfo = rowInfo;
    Menu_Framework_itemsListRow = 0;

    Menu_Framework_itemsText = Core_Memory_AllocateFromHighEnd(rowCount * 128);

    for (i = 0; i < rowCount; ++i) {
        char* text;

        ((Menu_Framework_ItemText*)Menu_Framework_itemsText)[i].menuRowFlags = rowTypes[i];

        if (strings[i * 2] != NULL) {
            Core_Memory_CopyHalfwords(&Menu_Framework_itemsText[i * 64], strings[i * 2], 26);

            Menu_Framework_itemsText[(i * 64) + 13] = Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        } else {
            Menu_Framework_itemsText[i * 64] = Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        }

        text = strings[i * 2 + 1];

        if (text != NULL) {
            ((Menu_Framework_ItemText*)Menu_Framework_itemsText)[i].description[0] = Core_TextCode_ChunkSize;

            Core_Memory_CopyHalfwords(&Menu_Framework_itemsText[(i * 64) + 17], text, 92);

            ((Menu_Framework_ItemText*)Menu_Framework_itemsText)[i].description[47] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        } else {
            ((Menu_Framework_ItemText*)Menu_Framework_itemsText)[i].description[0] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_storeCursor (src/MENU/MAINMENU.PRG/58EC.c:892)
int Menu_Framework_CloseRowsAndRememberCursor(void)
{
    Core_Memory_FreeFromHighEnd(Menu_Framework_itemsText);

    Menu_Framework_itemsText = NULL;
    Menu_Framework_menuMemory.cursorMemories[Menu_Framework_subMenuId * 2] = Menu_Framework_selectedRow;
    Menu_Framework_menuMemory.cursorMemories[Menu_Framework_subMenuId * 2 + 1] = Menu_Framework_subMenuPage;

    return (Menu_Framework_selectedRow + Menu_Framework_subMenuPage) | ((Menu_Framework_selectedRow + (Menu_Framework_subMenuRowInfo * 10)) << 8);
}

// decomp: MAINMENU.PRG vs_mainMenu_getConfirmedRow (src/MENU/MAINMENU.PRG/58EC.c:903)
int Menu_Framework_HighlightedRow(void) { return Menu_Framework_selectedRow + Menu_Framework_subMenuPage; }

// decomp: MAINMENU.PRG vs_mainmenu_getSelectedRow (src/MENU/MAINMENU.PRG/58EC.c:905)
int Menu_Framework_GetSelectedRow(void) { return Menu_Framework_itemsText != NULL ? -1 : Menu_Framework_selectedRow; }

// decomp: MAINMENU.PRG _initItemRow (src/MENU/MAINMENU.PRG/58EC.c:910)
Menu_Framework_MenuRow* Menu_Framework_CreateListRow(int row, int x)
{
    int temp_v1;
    Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(row + (Menu_Framework_subMenuRowInfo * 10),
        x, ((row + Menu_Framework_subMenuRowInfo) * 16) + 18, Menu_Framework_menuRowYOffset + 126, 0,
        (char*)((Menu_Framework_ItemText*)Menu_Framework_itemsText)[Menu_Framework_subMenuPage + row].name);

    int flags = ((Menu_Framework_ItemText*)Menu_Framework_itemsText)[Menu_Framework_subMenuPage + row].menuRowFlags;

    menuItem->unselectable = flags & 1;
    menuItem->backgroundWidth += (flags & 2) * 12;

    temp_v1 = (flags >> 3) & 1;

    menuItem->invertGradient = temp_v1;
    menuItem->fontColor = temp_v1 | ((flags >> 2) & 1);
    menuItem->rowIcon = (flags >> 26);

    if ((row == 0) && (Menu_Framework_subMenuPage != 0)) {
        menuItem->isScrollable = Menu_Framework_MenuItemScroll_Up;
    }

    row = (flags >> 9) & 0x7F;

    if (row <= 100) {
        menuItem->count = row;
    } else if (row <= 102) {
        menuItem->outsetIcon = row - 100;
    } else {
        menuItem->subText = Battle_limbNames[row - 103];
    }

    menuItem->material = (flags >> 16) & 7;
    menuItem->max = (flags >> 19) & 0x7F;
    return menuItem;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderItemsList (src/MENU/MAINMENU.PRG/58EC.c:947)
void Menu_Framework_UpdateRowSelection(void)
{

    int absRow;
    int temp_v1;
    int i;
    int var_s1;
    int page;
    Menu_Framework_MenuRow* menuItem;

    if (Menu_Framework_subMenuRowCount == 0) {

        if (Menu_Framework_itemsText != NULL) {
            Core_Memory_FreeFromHighEnd(Menu_Framework_itemsText);
            Menu_Framework_itemsText = NULL;
        }

        Menu_Framework_selectedRow = -2;
        return;
    }

    if (Menu_Framework_itemsListRow < 10) {

        menuItem = Menu_Framework_CreateListRow(Menu_Framework_itemsListRow, 320);

        menuItem->state = Menu_Framework_MenuItemState_SlideX;
        ++Menu_Framework_itemsListRow;
        menuItem->targetPosition0 = 194 - Menu_Framework_menuRowYOffset;

        if (Menu_Framework_itemsListRow == Menu_Framework_subMenuRowCount) {
            Menu_Framework_itemsListRow = 16;
        }

        if (Menu_Framework_itemsListRow == (10 - Menu_Framework_subMenuRowInfo)) {

            if ((Menu_Framework_itemsListRow + Menu_Framework_subMenuPage) < Menu_Framework_subMenuRowCount) {
                menuItem->isScrollable = Menu_Framework_MenuItemScroll_Down;
            }

            Menu_Framework_itemsListRow = 16;
        }
    } else {

        menuItem = Menu_Framework_MenuItem(Menu_Framework_selectedRow + (Menu_Framework_subMenuRowInfo * 10));
        Menu_Framework_SetInformationMessage(
            (char*)(Menu_Framework_itemsText + (((Menu_Framework_selectedRow + Menu_Framework_subMenuPage) * 64) + 16)));

        switch (Menu_Framework_itemsListRow) {
        case 16:
            if (Menu_Framework_AreMenuRowsIdle()) {
                Menu_Framework_itemsListRow = 17;
            }
            return;

        case 17:
            absRow = Menu_Framework_selectedRow + Menu_Framework_subMenuPage;
            page = Menu_Framework_subMenuPage;

            if (Input_buttonsPressed.allButtons & PADRup) {

                Menu_PlayLeaveSound();
                Core_Memory_FreeFromHighEnd(Menu_Framework_itemsText);

                Menu_Framework_itemsText = NULL;
                Menu_Framework_menuMemory.cursorMemories[Menu_Framework_subMenuId * 2] = Menu_Framework_selectedRow;
                Menu_Framework_menuMemory.cursorMemories[Menu_Framework_subMenuId * 2 + 1] = Menu_Framework_subMenuPage;
                Menu_Framework_selectedRow = -3;

                return;
            }

            if (Input_buttonsPressed.allButtons & PADRright) {
                if (menuItem->unselectable == 0) {

                    menuItem->selected = 1;

                    Menu_PlaySelectSound();
                    Core_Memory_FreeFromHighEnd(Menu_Framework_itemsText);

                    Menu_Framework_itemsText = NULL;
                    Menu_Framework_menuMemory.cursorMemories[Menu_Framework_subMenuId * 2] = Menu_Framework_selectedRow;
                    Menu_Framework_menuMemory.cursorMemories[Menu_Framework_subMenuId * 2 + 1] = Menu_Framework_subMenuPage;
                    Menu_Framework_selectedRow = absRow;

                    return;
                }

                Menu_PlayInvalidSound();
            }

            menuItem->selected = 0;

            if (Input_buttonsPressed.allButtons & PADRdown) {

                Menu_PlayLeaveSound();
                Core_Memory_FreeFromHighEnd(Menu_Framework_itemsText);

                Menu_Framework_itemsText = NULL;
                Menu_Framework_menuMemory.cursorMemories[Menu_Framework_subMenuId * 2] = Menu_Framework_selectedRow;
                Menu_Framework_menuMemory.cursorMemories[Menu_Framework_subMenuId * 2 + 1] = Menu_Framework_subMenuPage;
                Menu_Framework_selectedRow = -2;

                return;
            }

            if (Input_buttonRepeat & PADLup) {

                if (Menu_Framework_subMenuRowCount <= (10 - Menu_Framework_subMenuRowInfo)) {

                    if (Menu_Framework_selectedRow == 0) {
                        Menu_Framework_selectedRow = Menu_Framework_subMenuRowCount - 1;
                    } else {
                        --Menu_Framework_selectedRow;
                    }

                } else if (Menu_Framework_subMenuPage == 0) {

                    if (Menu_Framework_selectedRow == 0) {
                        if (Input_buttonsPressed.allButtons & PADLup) {
                            Menu_Framework_selectedRow = 9 - Menu_Framework_subMenuRowInfo;
                            Menu_Framework_subMenuPage = Menu_Framework_subMenuRowInfo + (Menu_Framework_subMenuRowCount + 246);
                        }
                    } else {
                        --Menu_Framework_selectedRow;
                    }

                } else if (Menu_Framework_selectedRow == 1) {
                    --Menu_Framework_subMenuPage;
                } else {
                    --Menu_Framework_selectedRow;
                }
            }

            if (Input_buttonRepeat & PADLdown) {

                if (Menu_Framework_subMenuRowCount <= (10 - Menu_Framework_subMenuRowInfo)) {

                    if (Menu_Framework_selectedRow == (Menu_Framework_subMenuRowCount - 1)) {
                        Menu_Framework_selectedRow = 0;
                    } else {
                        ++Menu_Framework_selectedRow;
                    }

                } else if (Menu_Framework_subMenuPage == (Menu_Framework_subMenuRowCount + (Menu_Framework_subMenuRowInfo - 10))) {

                    if (Menu_Framework_selectedRow == (9 - Menu_Framework_subMenuRowInfo)) {

                        if (Input_buttonsPressed.allButtons & PADLdown) {
                            Menu_Framework_selectedRow = 0;
                            Menu_Framework_subMenuPage = 0;
                        }

                    } else {
                        ++Menu_Framework_selectedRow;
                    }

                } else if (Menu_Framework_selectedRow == (8 - Menu_Framework_subMenuRowInfo)) {
                    ++Menu_Framework_subMenuPage;
                } else {
                    ++Menu_Framework_selectedRow;
                }
            }

            if (absRow != (Menu_Framework_selectedRow + Menu_Framework_subMenuPage)) {

                Menu_PlayChangeSound();

                if (Menu_Framework_subMenuPage != page) {

                    char gradientStates[Menu_Framework_subMenuRowCount];

                    for (i = 0; i < Menu_Framework_subMenuRowCount; ++i) {
                        gradientStates[i] = 0;
                    }

                    var_s1 = Menu_Framework_subMenuRowCount;
                    temp_v1 = 10 - Menu_Framework_subMenuRowInfo;

                    if (temp_v1 < var_s1) {
                        var_s1 = temp_v1;
                    }

                    for (i = 0; i < var_s1; ++i) {
                        gradientStates[i + page] =
                            Menu_Framework_MenuItem(i + (Menu_Framework_subMenuRowInfo * 10))
                                ->gradientState;
                    }

                    for (i = 0;;) {
                        menuItem = Menu_Framework_CreateListRow(i, 194 - Menu_Framework_menuRowYOffset);
                        menuItem->gradientState = gradientStates[i + Menu_Framework_subMenuPage];
                        i += 1;

                        if (i == Menu_Framework_subMenuRowCount) {
                            break;
                        }

                        if (i == (10 - Menu_Framework_subMenuRowInfo)) {
                            if ((i + Menu_Framework_subMenuPage) < Menu_Framework_subMenuRowCount) {
                                menuItem->isScrollable = Menu_Framework_MenuItemScroll_Down;
                            }
                            break;
                        }
                    }
                }
            }

            Menu_Framework_MenuItem(Menu_Framework_selectedRow + (Menu_Framework_subMenuRowInfo * 10))->selected = 1;

            Menu_Framework_rowSelectionCursorAnimStep = Menu_Framework_DrawCursor(Menu_Framework_rowSelectionCursorAnimStep,
                (180 - Menu_Framework_menuRowYOffset)
                    | ((((Menu_Framework_selectedRow + Menu_Framework_subMenuRowInfo) * 16) + 10) << 0x10));

            break;
        }
    }
}

// decomp: MAINMENU.PRG _renderBackground (src/MENU/MAINMENU.PRG/58EC.c:1172)
void Menu_Framework_DrawBackground(int brightness)
{
    int opaque;
    u_long* prim;

    int uCoord = 0;
    u_long* before = Render_frameScratch.orderingTable + Menu_Framework_backgroundOtOffset * 4;

    if (Render_frameBuf == 0) {
        uCoord = 320;
    }

    brightness *= 8;
    opaque = brightness == 128;

    if (opaque) {
        brightness = Menu_Framework_backgroundBrightness;
    } else {
        brightness |= 256;
    }

    prim = Render_QueueSprite(brightness, 256, vs_getWH(64, 240), before);
    prim[1] = vs_getTpage(768, 256, Render_TextureFormat_Palette8Bit, Render_BlendMode_Add, Render_Dithering_Off);
    prim[4] = vs_getUV0Clut(0, 0, 768, 227);

    prim = Render_QueueSprite(brightness, 0, vs_getWH(256, 240), before);
    prim[1] = vs_getTpage(640, 256, Render_TextureFormat_Palette8Bit, Render_BlendMode_Add, Render_Dithering_Off);
    prim[4] = vs_getUV0Clut(0, 0, 768, 227);

    if (!opaque) {
        int new_var = 288;
        brightness = (256 | 128) - brightness;

        Render_QueueSprite(brightness, 0, vs_getWH(256, 240), before)[1] =
            (((u_int)uCoord >> 6) | new_var
                | vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off));
        Render_QueueSprite(brightness, 256, vs_getWH(64, 240), before)[1] =
            (((uCoord + 256) >> 6) | new_var
                | vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off));
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_renderRowLabel (src/MENU/MAINMENU.PRG/58EC.c:1215)
int Menu_Framework_DrawFeatheredRowCharacter(
    u_int character, int x, Menu_Framework_MenuRow* menuItem, u_long* before)
{
    int i;
    int uvClut;
    u_long* prim;
    int new_var;

    int s7 = character >> 0x1F;
    character &= 0xFFFF;
    new_var = Core_TextCode_NonPrinting;

    if ((character < new_var) && (character == ' ')) {
        do {
            do {
                return x + 6;
            } while (0);
        } while (0);
    }

    i = menuItem->fontColor * 16;

    if (menuItem->fontColor == 0) {
        i = menuItem->unselectable * 48;
    }

    uvClut = (((character & 0x1FF) % 21) * 0xC) | (((character & 0x1FF) / 21) * 0xC00)
           | (getClut(i + 896, 222) << 16);

    for (i = 0; i < 12; ++i) {

        prim = Render_QueueSprite(s7 == 0 ? (i * 8) + 264 : 352 - (i * 8),
            vs_getXY_2(x, menuItem->y + i), vs_getWH(12, 1), before);
        prim[4] = uvClut;
        prim[1] = vs_getTpage(832, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, 0);
        uvClut += 256;
    }

    return x + Render_variableFontGlyphWidths[character];
}

// decomp: MAINMENU.PRG vs_mainMenu_renderTabNavigation (src/MENU/MAINMENU.PRG/58EC.c:1259)
void Menu_Framework_DrawTabNavigation(int mode)
{
    static char const* Menu_Framework_kTabNavigationControls[] = { "1", "L", "1", "R" };
    int i;
    int color;
    int clut;
    u_int xy;

    clut = vs_getUV0Clut(0, 0, 912, 223);
    color = vs_getRGB888(64, 64, 64);

    if ((Menu_Inventory_freezeTabArrows != 0) || (Menu_Framework_tabNavigationArrowAnimState != 11)) {

        clut = vs_getUV0Clut(0, 0, 896, 223);
        ++Menu_Framework_tabNavigationArrowAnimState;
        color = vs_getRGB888(128, 128, 128);

        if (Menu_Framework_tabNavigationArrowAnimState >= 12) {
            Menu_Framework_tabNavigationArrowAnimState = 0;
        }
    }

    xy = Menu_Framework_tabNavigationArrowAnimState >> 2;

    if (mode != 0) {
        for (i = 0; i < 4; ++i) {
            Render_DrawCompactTextColor(Menu_Framework_kTabNavigationControls[i], Menu_Framework_tabNavigationControlPositions[i], color, NULL);
        }
        i = vs_getXY(200, 5);
    } else {
        i = vs_getXY(16, 16);
    }

    Render_QueueDefaultSprite(vs_getWH(16, 16), i - xy)[4] = clut | vs_getUV(0, 48);

    i = vs_getXY(162, 17);

    if (mode != 0) {
        i = vs_getXY(292, 6);
    }

    Render_QueueDefaultSprite(vs_getWH(16, 16), i + xy)[4] = clut | vs_getUV(16, 48);
}

// decomp: MAINMENU.PRG _renderScrollableRow (src/MENU/MAINMENU.PRG/58EC.c:1309)
void Menu_Framework_DrawScrollableRow(Menu_Framework_MenuRow* menuItem)
{

    int wh;
    u_int sp14;
    int gradientState;
    int backgroundWidth;
    int scroll;
    int temp_v1_2;
    int i;
    u_long* prim;
    int var_s3;
    u_int xy;
    int temp_a0;
    u_long* before;

    char* ptr = &menuItem->text[0];
    sp14 = 0;

    if (Render_frameBuf == 0) {
        sp14 = 320;
    }

    xy = *(int*)&menuItem->x;
    before = Render_frameScratch.overlayOrderingTable + 8;
    wh = menuItem->w;
    gradientState = menuItem->gradientState;
    scroll = menuItem->isScrollable - 1;
    backgroundWidth = menuItem->backgroundWidth;

    if (Render_frameBuf != Menu_Framework_scrollableRowPreviousFrameBuf) {

        ++Menu_Framework_scrollableRowAnimState;
        Menu_Framework_scrollableRowPreviousFrameBuf = Render_frameBuf;

        if (Menu_Framework_scrollableRowAnimState >= 12) {
            Menu_Framework_scrollableRowAnimState = 0;
        }
    }

    var_s3 = Menu_Framework_scrollableRowAnimState >> 2;
    var_s3 = scroll == 0 ? var_s3 - 5 : 1 - var_s3;

    if (menuItem->x == 0) {

        for (i = 32; i < 40; ++i) {
            if (Menu_Framework_MenuItem(i)->state >= 2) {
                break;
            }
        }

        if (i == 40) {
            prim = before - 2;
            Render_QueueSpriteWithTexturePage(128, vs_getXY_2(126, menuItem->y + var_s3),
                vs_getWH(16, 16), prim)[4] = vs_getUV0Clut(scroll * 16, 48, 912, 223);
        }

    } else {

        for (i = 0; i < 32; ++i) {
            if (Menu_Framework_MenuItem(i)->state >= 2) {
                break;
            }
        }

        if (i == 32) {
            Render_QueueSpriteWithTexturePage(128,
                vs_getXY_2(menuItem->x - 14, menuItem->y + var_s3), vs_getWH(16, 16),
                before - 2)[4] = vs_getUV0Clut(scroll * 16, 48, 912, 223);
        }
    }

    i = menuItem->x + 6;

    if (menuItem->rowIcon != 0) {
        i = menuItem->x + 22;
    }

    while ((var_s3 = *ptr++) != 0xFF) {
        if (var_s3 == Core_TextCode_Spacing) {
            i += *ptr++;
        } else {
            i = Menu_Framework_DrawFeatheredRowCharacter(
                var_s3 | (scroll << 0x1F), i, menuItem, before);
        }
    }

    if (wh != 0) {
        prim = Render_frameScratch.primitiveCursor;

        for (i = 0; i < 12; ++i) {

            if (scroll == 0) {
                var_s3 = i * 8 + 32;
            } else {
                var_s3 = 120 - i * 8;
            }

            prim[0] = vs_getTag(u_long[6], *before);
            prim[1] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_On);
            prim[2] = Menu_Framework_GradientStopColor(8 - gradientState, backgroundWidth, var_s3)
                    | 0x52000000;
            prim[3] = xy;
            prim[4] = Menu_Framework_GradientStopColor(gradientState, backgroundWidth, var_s3);
            prim[5] = ((xy + wh - 1) & 0xFFFF) | ((xy >> 0x10) << 0x10);
            prim[6] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off);

            *before = ((u_long)prim << 8) >> 8;
            prim += 7;
            xy += vs_getXY(0, 1);
        }

        Render_frameScratch.primitiveCursor = prim;

        if (scroll == 0) {
            Render_InsertThreeWordPrimitive(before, vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0),
                vs_getXY_2(xy + 2, xy >> 16), wh | vs_getWH(0, 2));
        }

        if (gradientState != 0) {
            menuItem->gradientState = gradientState - 1;
        }

        xy = xy + 0xFFF40000;

        wh |= vs_getWH(0, 1);
        var_s3 = ((menuItem->x < 0x80) ^ 1) << 7;

        for (i = 0; i < 12; ++i) {
            prim = Render_QueueSprite(
                scroll == 0 ? 120 - i * 8 : i * 8 + 32, xy, wh, before);
            temp_v1_2 = (xy - var_s3);
            temp_a0 = xy >> 0x10;
            xy += vs_getXY(0, 1);
            prim[1] = ((sp14 + var_s3) >> 6)
                    | vs_getTpage(0, 0, Render_BlendMode_Subtract, Render_BlendMode_Add,
                        Render_Dithering_Off);
            prim[4] = temp_v1_2 & 0xFF;
            prim[4] = prim[4] | (temp_a0 << 8);
        }
    }
}

// decomp: MAINMENU.PRG _renderMenuItems (src/MENU/MAINMENU.PRG/58EC.c:1454)
void Menu_Framework_UpdateAndDrawRows(void)
{
    int i;
    Menu_Framework_MenuRow* menuItem = Menu_Framework_menuItems;

    for (i = 0; i < 40; ++i, ++menuItem) {
        int var_v1;
        u_long* before;
        int target;
        int origin;
        int temp_a1;

        int j = menuItem->state;

        if (j == 0) {
            continue;
        }

        switch (j) {
        case 1:
            break;

        case 2:
            origin = menuItem->x;
            target = menuItem->targetPosition0;

            if (origin < target) {

                origin += 32;

                if (target < origin) {
                    origin = target;
                }
            } else {
                if (target < origin) {

                    for (j = 1; j < 16; ++j) {
                        if ((target + Menu_Framework_rowAnimationSteps[j]) >= origin) {
                            break;
                        }
                    }

                    origin = target + Menu_Framework_rowAnimationSteps[j - 1];

                    if ((target >= 155) && (menuItem->w < (320 - origin))) {

                        menuItem->w = 64 - origin;

                        if ((320 - origin) > 165) {
                            menuItem->w = 165;
                        }
                    }
                }
            }

            menuItem->x = origin;

            if (origin == target) {
                menuItem->state = 1;
            }

            break;

        case 3:
            origin = menuItem->y;
            target = menuItem->targetPosition0;

            if (origin < target) {

                for (j = 1; j < 16; ++j) {
                    if (origin >= (target - Menu_Framework_rowAnimationSteps[j])) {
                        break;
                    }
                }

                origin = target - Menu_Framework_rowAnimationSteps[j - 1];

            } else if (target < origin) {

                for (j = 1; j < 16; ++j) {
                    if ((target + Menu_Framework_rowAnimationSteps[j]) >= origin) {
                        break;
                    }
                }

                origin = target + Menu_Framework_rowAnimationSteps[j - 1];
            }

            menuItem->y = origin;

            if (origin == target) {
                menuItem->state = 1;
            }

            break;

        case 4:
            origin = menuItem->x;
            target = menuItem->targetPosition0;

            if (origin != target) {

                for (j = 1; j < 16; ++j) {
                    if ((target + Menu_Framework_rowAnimationSteps[j]) >= origin) {
                        break;
                    }
                }

                menuItem->x = Menu_Framework_rowAnimationSteps[j - 1] + target;

                if (menuItem->w < (320 - menuItem->x)) {
                    menuItem->w = (64 - menuItem->x);
                }

            } else {

                origin = menuItem->y;
                target = menuItem->targetPosition1;

                if (origin != target) {
                    for (j = 1; j < 16; ++j) {
                        if ((target + Menu_Framework_rowAnimationSteps[j]) >= origin) {
                            break;
                        }
                    }
                    menuItem->y = Menu_Framework_rowAnimationSteps[j - 1] + target;
                }

                if (menuItem->y == target) {
                    menuItem->state = 1;
                }
            }
            break;

        case 5:
            origin = menuItem->x;
            target = menuItem->targetPosition0;

            if (target < origin) {

                origin -= 32;

                if (origin < target) {
                    origin = target;
                }

                if (-menuItem->w >= origin) {
                    menuItem->state = 0;
                }

            } else {
                if (origin < target) {

                    for (j = 1; j < 16; ++j) {
                        if (origin >= (target - Menu_Framework_rowAnimationSteps[j])) {
                            break;
                        }
                    }

                    origin = target - Menu_Framework_rowAnimationSteps[j - 1];
                }
            }

            menuItem->x = origin;

            if (origin == target) {
                if (origin < 0) {
                    menuItem->state = 0;
                } else {
                    menuItem->state = 1;
                }
            }
            break;
        }

        if (Menu_Framework_hideMenu) {
            continue;
        }

        j = menuItem->max;
        before = Render_frameScratch.overlayOrderingTable + 8;

        if (j != 0) {
            int xy;

            if (menuItem->w == 126) {
                var_v1 = menuItem->x + 116;
                origin = var_v1 | ((menuItem->y + 1) << 0x10);
            } else {
                var_v1 = (((menuItem->x * 19) >> 4) + 96) & 0xFFFF;
                origin = var_v1 | ((menuItem->y + 1) << 0x10);
            }

            Menu_Framework_DrawNeutralInteger(j, origin, before);

            xy = origin - 30;

            Render_DrawCompactText("/", xy, before);
            Menu_Framework_DrawNeutralInteger(menuItem->count, xy, before);

            if ((menuItem->y == 18) && (menuItem->x < 96)) {
                Menu_Framework_DrawNeutralInteger(
                    menuItem->itemPage, origin + 32, before);
                Render_DrawCompactText("-", origin, before);
            }

        } else {

            j = menuItem->count;

            if (j != 0) {
                Render_DrawIntegerRightAligned(0,
                    ((menuItem->x + menuItem->w) - 16) | ((menuItem->y + 3) << 0x10), j,
                    before);
            }
        }

        if (menuItem->x < 320) {

            if (menuItem->isScrollable != 0) {
                Menu_Framework_DrawScrollableRow(menuItem);
            } else {
                Menu_Framework_DrawMenuItem(menuItem);
            }

            j = menuItem->outsetIcon;

            if (j != 0) {
                Menu_Framework_DrawEquipmentStateIcon(
                    j - 1, (menuItem->x - 24) | ((menuItem->y - 3) << 0x10));
            }
        }

        j = menuItem->buttonId;

        if (j != 0) {
            Menu_Framework_DrawButtonIcon(j - 1, menuItem->x + 5, menuItem->y - 2, NULL);
        }

        j = menuItem->rowIcon;

        if (j != 0) {
            Menu_Framework_DrawRowIcon(
                (menuItem->isScrollable << 0x10) | (menuItem->selected << 8) | j,
                menuItem->x, menuItem->y - 2);
        }

        j = menuItem->material;

        if (j != 0) {
            Menu_Framework_DrawMaterialIndicator(j - 1, (menuItem->x + 13) | ((menuItem->y - 3) << 0x10));
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_renderScreen (src/MENU/MAINMENU.PRG/58EC.c:1709)
void Menu_Framework_UpdateScreen(void)
{
    int fadeStep = Menu_Framework_backgroundFadeStep;

    if (Menu_Framework_itemsText != NULL) {
        Menu_Framework_UpdateRowSelection();
    }

    if (Menu_Framework_backgroundFadeState != 0) {

        fadeStep += 2;

        if (fadeStep >= 17) {
            fadeStep = 16;
        }
    } else {
        fadeStep -= 2;

        if (fadeStep < 0) {
            fadeStep = 0;
        }
    }

    Menu_Framework_backgroundFadeStep = fadeStep;

    if (fadeStep != 0) {
        Menu_Framework_DrawBackground(fadeStep);
    }

    if (Menu_Framework_shortcutInvoked == 0) {
        Menu_Framework_DrawCommandLabel();
    }

    if (Menu_Framework_menuItems != 0) {
        Menu_Framework_UpdateAndDrawRows();
    }

    Menu_Framework_DrawActionCostPanel();
    Menu_Framework_UpdateEquipmentStatPages(0);
    Menu_Framework_UpdatePrimaryStatPanel(0);
    Menu_Framework_UpdateDpPpBars(0);
}

// decomp: MAINMENU.PRG _nop (src/MENU/MAINMENU.PRG/C48.c:73)
void Menu_Framework_NoOperation(void) { }

// decomp: MAINMENU.PRG _getNextEmptyObjectDataSlot (src/MENU/MAINMENU.PRG/C48.c:81)
int Menu_Framework_NextFreeActorAssetRequest(void)
{
    Content_UpdateActorAssetQueue();
    return Content_FindFreeActorAssetRequest();
}

// decomp: MAINMENU.PRG vs_mainMenu_menuItemFlyoutLeft (src/MENU/MAINMENU.PRG/C48.c:226)
void Menu_Framework_FlyOutRowLeft(int row)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(row + 32);

    if (menuItem->state != Menu_Framework_MenuItemState_Inactive) {
        menuItem->state = Menu_Framework_MenuItemState_Dismiss;
        menuItem->targetPosition0 = -menuItem->w;
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_flyoutLeftExcept (src/MENU/MAINMENU.PRG/C48.c:236)
void Menu_Framework_FlyOutRowsLeftExcept(int row)
{
    int i;

    for (i = 0; i < 8; ++i) {
        if (i != row) {
            Menu_Framework_FlyOutRowLeft(i);
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_menuItemFlyoutRight (src/MENU/MAINMENU.PRG/C48.c:247)
void Menu_Framework_FlyOutRowRight(int row)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(row);

    if (menuItem->state != Menu_Framework_MenuItemState_Inactive) {
        menuItem->state = Menu_Framework_MenuItemState_SlideX;
        menuItem->targetPosition0 = 320;
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_clearMenuExcept (src/MENU/MAINMENU.PRG/C48.c:257)
void Menu_Framework_ClearMenuExcept(enum Menu_Framework_MenuItemId id)
{
    int i;

    for (i = 0; i < 32; ++i) {
        if (i != id) {
            Menu_Framework_FlyOutRowRight(i);
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_flyoutMenuRightAndHoistSelection (src/MENU/MAINMENU.PRG/C48.c:268)
void Menu_Framework_FlyOutRowsAndHoistSelection(int row, int offset)
{
    Menu_Framework_MenuRow* menuItem;
    int i;

    for (i = offset * 10; i < 30; ++i) {
        Menu_Framework_FlyOutRowRight(i);
    }

    menuItem = Menu_Framework_MenuItem(row + (offset * 10));
    menuItem->state = 4;
    menuItem->targetPosition0 = 180;
    menuItem->targetPosition1 = (offset * 16) + 18;

    if (menuItem->backgroundWidth == 0) {
        menuItem->backgroundWidth = 1;
    }
}

// decomp: MAINMENU.PRG vs_mainmenu_ready (src/MENU/MAINMENU.PRG/C48.c:287)
int Menu_Framework_AreMenuRowsIdle(void)
{
    int i;
    Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(0);

    for (i = 0; i < 40; ++i) {
        if (menuItem->state > 1) {
            return 0;
        }
        ++menuItem;
    }
    return 1;
}

// decomp: MAINMENU.PRG _quitDelay (src/MENU/MAINMENU.PRG/C48.c:305)
int Menu_Framework_QuitDelay(void)
{

    int ret = ++Menu_Framework_quitDelay;

    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

    return ret == 5;
}

// decomp: MAINMENU.PRG func_800FAA5C (src/MENU/MAINMENU.PRG/C48.c:316)
int Menu_Framework_ReturnFromSubmenu(int arg0)
{
    if (arg0 & Menu_Framework_MenuState_OpenedFromMainMenu) {
        Menu_Framework_menuState.currentMenu = 0;

        Menu_Framework_UpdateMainMenuSelection(1);

        return 0;
    }

    if (Menu_Framework_openedWithoutMainMenu == 0) {
        Render_RetireTexturePageRange(26, 3, 6);
    }

    Menu_Framework_menuState.currentMenu = 0x7F;
    return 1;
}

// decomp: MAINMENU.PRG vs_mainMenu_exec (src/MENU/MAINMENU.PRG/C48.c:334)
void Menu_Framework_UpdateMainMenu(int arg0)
{
    static int (*Menu_Framework_mainMenuSubmenuEntrypoints[])(u_char*) = { Menu_Magic_Update, Menu_BreakArts_Update,
        Menu_BattleAbilities_Update, Menu_Inventory_Update, Menu_Status_Update, Menu_Map_UpdateScreen, Menu_Files_UpdateDataMenu,
        Menu_Options_Update, Menu_Score_UpdateScreen, Menu_Manual_UpdateScreen };

    int selectedMenu;
    int menuLoaded;
    int state;
    char* submenuState;
    Menu_Framework_MenuControlState* menuState = &Menu_Framework_menuState;
    Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem((arg0 - 1) & 0xF);
    int submenuReturn = 0;
    Items_Inventory* inventory = &Audio_Seq_unusedInventory;

    Menu_Framework_weapons = inventory->weapons;
    Menu_Framework_blades = inventory->blades;
    Menu_Framework_grips = inventory->grips;
    Menu_Framework_shields = inventory->shields;
    Menu_Framework_armor = inventory->armor;
    Menu_Framework_gems = inventory->gems;

    submenuState = &Menu_Framework_submenuStates[arg0 & 0xF];
    menuLoaded = Menu_Framework_LoadMenuOverlay(arg0);
    selectedMenu = arg0 & 0x3F;

    if (selectedMenu != 0) {
        if (selectedMenu == 31) {
            submenuReturn = Menu_Framework_QuitDelay();
        } else if (Menu_Framework_actionRestrictions & 0x200) {
            if (menuLoaded) {
                if (selectedMenu == 15) {
                    submenuReturn = Menu_Results_UpdateScreen(submenuState);
                } else if (selectedMenu == 3) {
                    submenuReturn = Menu_BattleAbilities_UpdateUnlockNotifications(submenuState);
                } else if (selectedMenu == 14) {
                    submenuReturn = Menu_Container_UpdateScreen(submenuState);
                } else if (selectedMenu == 8) {
                    submenuReturn = Menu_Rename_Update(submenuState);
                } else if (selectedMenu == 13) {
                    submenuReturn = Menu_Setup_Update(submenuState);
                } else if (selectedMenu == 12) {
                    submenuReturn = Menu_Loot_Update(submenuState);
                } else if (selectedMenu == 7 && (Menu_Framework_actionRestrictions & 0x400)) {
                    submenuReturn = Menu_Files_UpdateContainerSave(submenuState);
                } else if (selectedMenu == 7 && Audio_Seq_unusedStateFlags.gameOver == 1) {
                    submenuReturn = Menu_Files_UpdateGameOver(submenuState);
                } else {
                    Menu_Framework_actionRestrictions = 0;
                }
            }
        } else {

            switch (*submenuState) {
            case 0:
                if (!(arg0 & 0x40)) {

                    state = 0;

                    if (selectedMenu == 1) {
                        state = (Menu_Framework_actionRestrictions & 0xB7) != 0;
                    }

                    if (selectedMenu == 2) {
                        state = (Menu_Framework_actionRestrictions & 0x15F) != 0;
                    }

                    Menu_Framework_SetMenuItem(selectedMenu - 1, 320, 18, 140, 8,
                        (char*)&Menu_menuStrings[Menu_menuStrings[selectedMenu
                                                                            - 1]])
                        ->unselectable = state;
                }

                if (((selectedMenu - 4) < 2U) || (selectedMenu == 7)
                    || (selectedMenu == 9)) {
                    Menu_Framework_SetBackgroundFade(1);
                }

                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
                Menu_Framework_ClearMenuExcept(selectedMenu - 1);

                menuItem->state = Menu_Framework_MenuItemState_SlideX;
                menuItem->targetPosition0 = 180;
                menuItem->selected = 1;

                if (selectedMenu == 5) {

                    Menu_Framework_SetBackgroundPriority(-2, 128);

                    menuItem->targetPosition0 = 320;
                }

                *submenuState = 1;
                break;

            case 1:
                state = menuItem->state;

                if (state != 1) {
                    break;
                }

                menuItem->state = Menu_Framework_MenuItemState_SlideY;
                menuItem->targetPosition0 = 18;

                if (selectedMenu == 5) {
                    menuItem = Menu_Framework_SetMenuItem(
                        4, 320, 18, 140, 8, Battle_characterState->stats->name);
                    menuItem->state = 2;
                    menuItem->targetPosition0 = 0xB4;
                    menuItem->selected = state;
                }

                *submenuState = 3;
                // Fallthrough

            default:
                if (menuLoaded) {
                    submenuReturn = Menu_Framework_mainMenuSubmenuEntrypoints[selectedMenu - 1](submenuState);
                }

                break;
            }
        }
    } else {
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Command);

        state = Menu_Framework_UpdateMainMenuSelection(0);

        if (state != 0) {
            if (state > 0) {
                state |= Menu_Framework_MenuState_OpenedFromMainMenu;
            } else {
                state = Menu_Framework_MenuState_NoneClosing;
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            }

            menuState->currentMenu = state;
        }
    }

    if ((submenuReturn == 0) || (Menu_Framework_ReturnFromSubmenu(menuState->currentMenu) == 0)) {
        Menu_Framework_UpdateScreen();
    }
}

// decomp: MENU5.PRG _setNextAction (src/MENU/MENU5.PRG/4D8.c:90)
void Menu_Framework_SetMenuVisible(int display)
{
    if (display != 0) {
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
        Menu_Framework_hideMenu = 0;
        return;
    }

    Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
    Menu_Framework_hideMenu = 1;
}

// decomp: MENU5.PRG _clearMenu (src/MENU/MENU5.PRG/4D8.c:201)
void Menu_Framework_ClearMenuExceptTitle(void) { Menu_Framework_ClearMenuExcept(5); }

// decomp: MENU5.PRG _buildYawMatrix (src/MENU/MENU5.PRG/4D8.c:1756)
void Menu_Framework_BuildYawMatrix(MATRIX* arg0, short arg1)
{
    int temp_s0 = Core_Gte_Sin(-arg1);
    int temp_v0 = Core_Gte_Cos(-arg1);

    arg0->m[0][0] = temp_v0;
    arg0->m[0][1] = 0;
    arg0->m[0][2] = -temp_s0;
    arg0->m[1][0] = 0;
    arg0->m[1][1] = 0x1000;
    arg0->m[1][2] = 0;
    arg0->m[2][0] = temp_s0;
    arg0->m[2][1] = 0;
    arg0->m[2][2] = temp_v0;
}

// decomp: MENU5.PRG _buildPitchMatrix (src/MENU/MENU5.PRG/4D8.c:1772)
void Menu_Framework_BuildPitchMatrix(MATRIX* arg0, short arg1)
{
    int temp_s0 = Core_Gte_Sin(arg1);
    int temp_v0 = Core_Gte_Cos(arg1);

    arg0->m[0][0] = 0x1000;
    arg0->m[0][1] = 0;
    arg0->m[0][2] = 0;
    arg0->m[1][0] = 0;
    arg0->m[1][1] = temp_v0;
    arg0->m[1][2] = -temp_s0;
    arg0->m[2][0] = 0;
    arg0->m[2][1] = temp_s0;
    arg0->m[2][2] = temp_v0;
}

// decomp: MENU9.PRG _setMenuRow10 (src/MENU/MENU9.PRG/238.c:244)
void Menu_Framework_ShowSecondaryTitle(char* text)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(10, 320, 34, 140, 9, text);
    menuItem->state = Menu_Framework_MenuItemState_SlideX;
    menuItem->targetPosition0 = 180;
    menuItem->selected = 1;
    menuItem->fontColor = 1;
}

// decomp: MENU _renderAshley (src/MENU/_renderAshley.h:27)
void Menu_Framework_RenderAshley(int mode)
{

    static int Menu_Framework_ashleyProjectionDistance;
    static int Menu_Framework_ashleyLayoutPadding[3] __attribute__((unused));
    static MATRIX Menu_Framework_ashleyViewMatrixBackup;
    static Render_Camera Menu_Framework_ashleyCameraBackup;

    int yOffset;
    Render_Camera* camera = &Render_frameScratch.camera;

    switch (mode) {
    case Menu_Framework_AshleyRenderMode_Render:
        yOffset = Menu_Inventory_ashleyYOffset;

        switch (Menu_Inventory_ashleyRenderState) {
        case 0:
            if (yOffset < 10) {
                Menu_Inventory_ashleyYOffset = ++yOffset;
            }

            yOffset = 128 - Menu_Framework_rowAnimationSteps[10 - yOffset];
            break;

        case 1:
            if (yOffset > 0) {
                Menu_Inventory_ashleyYOffset = --yOffset;
            }

            yOffset = Menu_Framework_rowAnimationSteps[yOffset];
            break;

        case 2:
            if (yOffset < 10) {
                Menu_Inventory_ashleyYOffset = ++yOffset;
            } else {
                Menu_Inventory_ashleyRenderState = 0;
            }

            yOffset = Menu_Framework_rowAnimationSteps[10 - yOffset] + 128;
            break;

        case 3:
            if (yOffset > 0) {
                Menu_Inventory_ashleyYOffset = --yOffset;
            }

            yOffset = 608 - yOffset * 48;
            break;
        }

        if (Menu_Framework_ashleySkipFrame != 0) {
            Menu_Framework_ashleySkipFrame = 0;
            return;
        }

        if (Menu_Framework_ashleyRender != 0) {
            int scaledDistance;
            int offsetX;
            int offsetZ;
            int cameraDistance = Actors_actorRecords[1]->menuCameraDistance;

            setVector(&camera->lookAt, ((-Core_Gte_Sin(0xB00) * yOffset) >> 8) * cameraDistance,
                -(Actors_actorRecords[1]->collisionHeight << 0xB),
                ((Core_Gte_Cos(0xB00) * yOffset) >> 8) * cameraDistance);

            scaledDistance = cameraDistance * 4;
            offsetX = Core_Gte_Cos(0xB00) * scaledDistance;

            camera->position.vx = camera->lookAt.vx + offsetX;
            camera->position.vy = camera->lookAt.vy;

            offsetZ = Core_Gte_Sin(0xB00) * scaledDistance;

            setVector(&camera->angles, 0, 0, 0);
            camera->farClip = 0x1000;
            camera->position.vz = camera->lookAt.vz + offsetZ;

            Render_BuildCameraViewMatrix();
            Menu_Framework_DrawActorPreview(&Render_frameScratch.viewMatrix);
        }
        break;

    case Menu_Framework_AshleyRenderMode_Init:
        if (Menu_Framework_ashleyRender == 0) {
            Menu_Inventory_ashleyYOffset = 0;
            Menu_Inventory_ashleyRenderState = 2;
            Menu_Framework_ashleySkipFrame = 1;
            Menu_Framework_ashleyRender = 1;
            Menu_Framework_ashleyProjectionDistance = Render_projectionDistance;

            Render_SetCameraProjectionDistance(512);

            Menu_Framework_ashleyViewMatrixBackup = ((Render_FrameScratch*)((void*)camera - 0x34))->viewMatrix;
            Menu_Framework_ashleyCameraBackup = *camera;
            setVector(&camera->angles, 0, 0, 0);
            camera->farClip = 0x1000;

            Render_BuildCameraViewMatrix();
            Menu_Framework_SetBackgroundPriority(2046, 128);
            Menu_Framework_LoadActorPreview(0);
        }
        break;

    case Menu_Framework_AshleyRenderMode_Restore:
        if (Menu_Framework_ashleyRender != 0) {

            Menu_Framework_ReleaseActorPreview();
            Menu_Framework_SetBackgroundPriority(-4, 128);
            Render_SetCameraProjectionDistance(Menu_Framework_ashleyProjectionDistance);

            ((Render_FrameScratch*)((void*)camera - 0x34))->viewMatrix = Menu_Framework_ashleyViewMatrixBackup;
            *camera = Menu_Framework_ashleyCameraBackup;
            Menu_Framework_ashleyRender = 0;
        }

        break;
    }
}

// decomp: MENU _continueMenuTransition (src/MENU/items0.h:107)
void Menu_Framework_ContinueItemDetailsTransition(int id)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(31);
    menuItem->state = Menu_Framework_MenuItemState_SlideX;
    menuItem->targetPosition0 = 16;
    menuItem->w = 164;

    menuItem = Menu_Framework_MenuItem(id);
    menuItem->state = Menu_Framework_MenuItemState_SlideY;
    menuItem->targetPosition0 = 18;
}

// decomp: MENU _switchItemStatusPage (src/MENU/items1.h:35)
void Menu_Framework_UpdateItemDetailsSelection(int row, char* text[], u_int rowType, int page)
{
    Menu_Framework_MenuRow* menuItem;

    Menu_PlayChangeSound();

    menuItem = Menu_Framework_SetMenuItem(row, 155, 18, 165, 0, text[0]);
    menuItem->selected = 1;
    menuItem->rowIcon = (rowType >> 0x1A);
    menuItem->material = (rowType >> 0x10) & 7;

    Menu_Framework_SetInformationMessage(text[1]);
    Menu_Framework_MenuItem(31)->itemPage = page + 1;
}

// decomp: MENU _getWeaponStatValue (src/MENU/items1.h:695)
int Menu_Framework_WeaponSortValue(int stat, Items_WeaponLoadout* weapon)
{
    switch (stat) {
    case 0:
        return -weapon->blade.category;
    case 1:
        return -weapon->blade.material;
    case 2:
        return weapon->range.range;
    case 3:
        return -weapon->damageType;
    case 4:
        return weapon->currentDp;
    case 5:
        return weapon->maxDp;
    case 6:
        return weapon->currentPp;
    case 7:
        return weapon->maxPp;
    case 8:
        return weapon->currentStr;
    case 9:
        return weapon->currentInt;
    case 10:
        return weapon->currentAgility;
    default:
        if (stat >= 17) {
            return weapon->classAffinityCurrent.affinities[0][stat - 17];
        }
        return weapon->classAffinityCurrent.classes[0][stat - 11];
    }
}

// decomp: MENU _getShieldStatValue (src/MENU/items1.h:792)
int Menu_Framework_ShieldSortValue(int stat, Items_ShieldLoadout* shield)
{
    switch (stat) {
    case 1:
        return -shield->base.material;
    case 4:
        return shield->currentDp;
    case 5:
        return shield->maxDp;
    case 6:
        return shield->currentPp;
    case 7:
        return shield->maxPp;
    case 8:
        return shield->currentStr;
    case 9:
        return shield->currentInt;
    case 10:
        return shield->currentAgility;
    default:
        if (stat < 33) {
            return shield->classAffinityCurrent.classes[0][stat - 27];
        }

        if (stat > 39) {
            return shield->types[stat - 39];
        }

        return shield->classAffinityCurrent.affinities[0][stat - 33];
    }
}

// decomp: MENU _getMiscStatValue (src/MENU/items1.h:888)
int Menu_Framework_MiscSortValue(int stat, Items_InventoryMisc* misc)
{
    if (stat == 0) {
        return -misc->id;
    }
    return misc->count;
}

// decomp: MENU _getEquipmentStatValue (src/MENU/items1.h:949)
int Menu_Framework_EquipmentSortValue(int stat, Items_EquipmentPartStats* equipment)
{
    switch (stat) {
    case 0:
        return -equipment->category;
    case 1:
        return -equipment->material;
    case 2:
        return equipment->range.range;
    case 3:
        return -equipment->damageType;
    case 4:
        return equipment->currentDp;
    case 5:
        return equipment->maxDp;
    case 6:
        return equipment->currentPp;
    case 7:
        return equipment->maxPp;
    case 8:
        return equipment->strength;
    case 9:
        return equipment->intelligence;
    case 10:
        return equipment->agility;
    default:
        if (stat >= 27) {
            stat -= 16;
        }

        if (stat < 17) {
            return equipment->classes[stat - 11];
        }

        if (stat > 23) {
            return equipment->types[stat - 23];
        }

        return equipment->affinities[stat - 17];
    }
}

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Framework_ResetMainMenuStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_actionsSubMenuId, &initial, sizeof Menu_Framework_actionsSubMenuId);
    }
    {
        static const short initial = 128;
        memcpy((void*)&Menu_Framework_backgroundBrightness, &initial, sizeof Menu_Framework_backgroundBrightness);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_backgroundFadeState, &initial, sizeof Menu_Framework_backgroundFadeState);
    }
    {
        static const short initial = -4;
        memcpy((void*)&Menu_Framework_backgroundOtOffset, &initial, sizeof Menu_Framework_backgroundOtOffset);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_itemActionCount, &initial, sizeof Menu_Framework_itemActionCount);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_itemActionListOffset, &initial, sizeof Menu_Framework_itemActionListOffset);
    }
    {
        static const u_short* initial = NULL;
        memcpy((void*)&Menu_Framework_itemsText, &initial, sizeof Menu_Framework_itemsText);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_menuActionCurrent, &initial, sizeof Menu_Framework_menuActionCurrent);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_menuActionNext, &initial, sizeof Menu_Framework_menuActionNext);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_menuRowYOffset, &initial, sizeof Menu_Framework_menuRowYOffset);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Framework_selectedItemAction, &initial, sizeof Menu_Framework_selectedItemAction);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Framework_selectedRow, &initial, sizeof Menu_Framework_selectedRow);
    }
    {
        static const u_short* initial = NULL;
        memcpy((void*)&Menu_Framework_sortItemsTextBuf, &initial, sizeof Menu_Framework_sortItemsTextBuf);
    }
    {
        static const u_char initial = 0;
        memcpy((void*)&Menu_Framework_subMenuId, &initial, sizeof Menu_Framework_subMenuId);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_subMenuPage, &initial, sizeof Menu_Framework_subMenuPage);
    }
    {
        static const u_short initial = 0;
        memcpy((void*)&Menu_Framework_subMenuRowCount, &initial, sizeof Menu_Framework_subMenuRowCount);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_subMenuRowInfo, &initial, sizeof Menu_Framework_subMenuRowInfo);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_actionMenuState, &initial, sizeof Menu_Framework_actionMenuState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Framework_backgroundFadeStep, &initial, sizeof Menu_Framework_backgroundFadeStep);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_cursorColor, &initial, sizeof Menu_Framework_cursorColor);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_displayActionCost, &initial, sizeof Menu_Framework_displayActionCost);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_hideMenu, &initial, sizeof Menu_Framework_hideMenu);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_itemsListRow, &initial, sizeof Menu_Framework_itemsListRow);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_actionMenuCursorAnimStep, &initial, sizeof Menu_Framework_actionMenuCursorAnimStep);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Framework_commandLabelActionYPos, &initial, sizeof Menu_Framework_commandLabelActionYPos);
    }
    {
        static const u_short initial[] = { vs_getUV(0, 0), vs_getUV(0, 64), vs_getUV(64, 64),
        vs_getUV(100, 64), vs_getUV(132, 64), vs_getUV(188, 64), vs_getUV(0, 74) };
        memcpy((void*)&Menu_Framework_commandLabelMenuActionUvs, &initial, sizeof Menu_Framework_commandLabelMenuActionUvs);
    }
    {
        static const u_char initial = 8;
        memcpy((void*)&Menu_Framework_actionCostPanelAnimState, &initial, sizeof Menu_Framework_actionCostPanelAnimState);
    }
    {
        static const RECT initial[] = { { 640, 256, 32, 240 }, { 768, 256, 32, 240 } };
        memcpy((void*)&Menu_Framework_loadBackgroundTextureRects, &initial, sizeof Menu_Framework_loadBackgroundTextureRects);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_rowSelectionCursorAnimStep, &initial, sizeof Menu_Framework_rowSelectionCursorAnimStep);
    }
    {
        static const char initial = 11;
        memcpy((void*)&Menu_Framework_tabNavigationArrowAnimState, &initial, sizeof Menu_Framework_tabNavigationArrowAnimState);
    }
    {
        static const int initial[] = { vs_getXY(216, 8), vs_getXY(210, 8),
        vs_getXY(288, 8), vs_getXY(282, 8) };
        memcpy((void*)&Menu_Framework_tabNavigationControlPositions, &initial, sizeof Menu_Framework_tabNavigationControlPositions);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_scrollableRowAnimState, &initial, sizeof Menu_Framework_scrollableRowAnimState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_scrollableRowPreviousFrameBuf, &initial, sizeof Menu_Framework_scrollableRowPreviousFrameBuf);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Framework_quitDelay, &initial, sizeof Menu_Framework_quitDelay);
    }
}

// MENU3.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Framework_ResetMenu3Statics(void)
{
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Framework_ashleyRender, &initial, sizeof Menu_Framework_ashleyRender);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Framework_ashleySkipFrame, &initial, sizeof Menu_Framework_ashleySkipFrame);
    }
}

