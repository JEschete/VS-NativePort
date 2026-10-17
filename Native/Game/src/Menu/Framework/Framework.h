// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_FRAMEWORK_H_TYPES
#define VS_MENU_FRAMEWORK_FRAMEWORK_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_DiscQueueSlot Content_DiscQueueSlot;
typedef struct Menu_Framework_MenuControlState Menu_Framework_MenuControlState;
typedef struct Menu_Framework_MenuRow Menu_Framework_MenuRow;
typedef struct Menu_Framework_TextHeader Menu_Framework_TextHeader;

// decomp: src/include/lbaMacros.h:4
#define squashLbaVal(lba, size) ((lba) << 8 | ((size) >> 11))

// decomp: vs_mainMenu_actionsMenuActions (src/MENU/MAINMENU.PRG/58EC.h:7)
enum Menu_Framework_ActionsMenuAction { Menu_Framework_ActionsMenuAction_Discard = 4, Menu_Framework_ActionsMenuAction_Sort = 45 };
typedef enum Menu_Framework_ActionsMenuAction Menu_Framework_ActionsMenuAction;

// decomp: vs_mainMenu_buttonIDs (src/MENU/MAINMENU.PRG/58EC.h:89)
enum Menu_Framework_ButtonId {
    Menu_Framework_ButtonId_Triangle,
    Menu_Framework_ButtonId_Square,
    Menu_Framework_ButtonId_Circle,
    Menu_Framework_ButtonId_Cross,
    Menu_Framework_ButtonId_L1,
    Menu_Framework_ButtonId_R1,
    Menu_Framework_ButtonId_L2,
    Menu_Framework_ButtonId_R2
};
typedef enum Menu_Framework_ButtonId Menu_Framework_ButtonId;

// decomp: vs_mainMenu_copyItem_flags (src/MENU/MAINMENU.PRG/2D10.h:200)
enum Menu_Framework_CopyItemFlag { Menu_Framework_CopyItemFlag_Write = 0x10 };
typedef enum Menu_Framework_CopyItemFlag Menu_Framework_CopyItemFlag;

// decomp: vs_mainMenu_icons (src/MENU/MAINMENU.PRG/58EC.h:134)
enum Menu_Framework_MainMenuIcon { Menu_Framework_MainMenuIcon_Gem = 22 };
typedef enum Menu_Framework_MainMenuIcon Menu_Framework_MainMenuIcon;

// decomp: vs_mainMenu_menuCommands (src/MENU/MAINMENU.PRG/58EC.h:45)
enum Menu_Framework_MenuCommand {
    Menu_Framework_MenuCommand_None,
    Menu_Framework_MenuCommand_Command,
    Menu_Framework_MenuCommand_Menu,
    Menu_Framework_MenuCommand_Num,
    Menu_Framework_MenuCommand_Warning,
    Menu_Framework_MenuCommand_Help,
    Menu_Framework_MenuCommand_Info
};
typedef enum Menu_Framework_MenuCommand Menu_Framework_MenuCommand;

// decomp: vs_mainMenu_menuItemIds (src/MENU/MAINMENU.PRG/C48.h:6)
enum Menu_Framework_MenuItemId { Menu_Framework_MenuItemId_None = 40 };
typedef enum Menu_Framework_MenuItemId Menu_Framework_MenuItemId;

// decomp: vs_mainMenu_menuSelection (src/MENU/MAINMENU.PRG/58EC.h:211)
enum Menu_Framework_MenuSelection {
    Menu_Framework_MenuSelection_Confirm,
    Menu_Framework_MenuSelection_Back = 0xFFFF,
    Menu_Framework_MenuSelection_Quit = -2
};
typedef enum Menu_Framework_MenuSelection Menu_Framework_MenuSelection;

// decomp: vs_mainMenu_renderDpPpBarsState (src/MENU/MAINMENU.PRG/C48.h:104)
enum Menu_Framework_DpPpBarsState {
    Menu_Framework_DpPpBarsState_SlideIn = 0 << 2,
    Menu_Framework_DpPpBarsState_SlideOut = 1 << 2,
    Menu_Framework_DpPpBarsState_Static = 2 << 2
};
typedef enum Menu_Framework_DpPpBarsState Menu_Framework_DpPpBarsState;

// decomp: vs_mainMenu_renderDpPpBarsTarget (src/MENU/MAINMENU.PRG/C48.h:97)
enum Menu_Framework_DpPpBarsTarget {
    Menu_Framework_DpPpBarsTarget_None,
    Menu_Framework_DpPpBarsTarget_Dp,
    Menu_Framework_DpPpBarsTarget_Pp,
    Menu_Framework_DpPpBarsTarget_Both
};
typedef enum Menu_Framework_DpPpBarsTarget Menu_Framework_DpPpBarsTarget;

// decomp: vs_mainMenu_rowTypeFlags (src/MENU/MAINMENU.PRG/58EC.h:178)
enum Menu_Framework_RowTypeFlag {
    Menu_Framework_RowTypeFlag_Disabled = 1,
};
typedef enum Menu_Framework_RowTypeFlag Menu_Framework_RowTypeFlag;

// decomp: vs_mainMenu_statPage (src/MENU/MAINMENU.PRG/58EC.h:337)
enum Menu_Framework_StatPage {
    Menu_Framework_StatPage_Class,
    Menu_Framework_StatPage_Affinity,
    Menu_Framework_StatPage_Type,
    Menu_Framework_StatPage_None,
};
typedef enum Menu_Framework_StatPage Menu_Framework_StatPage;

// decomp: vs_mainMenu_statPageEnabled (src/MENU/MAINMENU.PRG/C48.h:77)
enum Menu_Framework_StatPageEnabled {
    Menu_Framework_StatPageEnabled_Class = 1 << 0,
    Menu_Framework_StatPageEnabled_Affinity = 1 << 1,
    Menu_Framework_StatPageEnabled_Type = 1 << 2,
    Menu_Framework_StatPageEnabled_All =
        Menu_Framework_StatPageEnabled_Class | Menu_Framework_StatPageEnabled_Affinity | Menu_Framework_StatPageEnabled_Type
};
typedef enum Menu_Framework_StatPageEnabled Menu_Framework_StatPageEnabled;

// decomp: vs_mainMenu_statusUiIds (src/MENU/MAINMENU.PRG/58EC.h:319)
enum Menu_Framework_StatusUiId {
    Menu_Framework_StatusUiId_UiPp,
    Menu_Framework_StatusUiId_UiDp,
    Menu_Framework_StatusUiId_UistatPageFirstElement,
    Menu_Framework_StatusUiId_UistatPageLastElement = 8,
    Menu_Framework_StatusUiId_UiPrincipleItem,
    Menu_Framework_StatusUiId_UiRange = 16,
    Menu_Framework_StatusUiId_UiRisk,
    Menu_Framework_StatusUiId_UiStr,
    Menu_Framework_StatusUiId_UiInt,
    Menu_Framework_StatusUiId_UiAgi,
};
typedef enum Menu_Framework_StatusUiId Menu_Framework_StatusUiId;

// decomp: textHeader_t (src/MENU/MAINMENU.PRG/58EC.c:21)
typedef struct Menu_Framework_TextHeader {
    char text[14];
    char disabled;
    char x;
} Menu_Framework_TextHeader;

// decomp: vs_battle_menuItem_t (src/BATTLE/BATTLE.PRG/5BF94.h:66)
typedef struct Menu_Framework_MenuRow {
    u_char state;
    u_char w;
    u_char backgroundWidth;
    u_char animationStep;
    u_char gradientState;
    u_char isScrollable;
    u_char selected;
    u_char unselectable;
    u_char buttonId;
    u_char rowIcon;
    u_char fontColor;
    u_char invertGradient;
    u_char material;
    u_char outsetIcon;
    u_short itemPage;
    u_short count;
    u_short max;
    short x;
    short y;
    short targetPosition0;
    short targetPosition1;
    char text[32];
    char* subText;
} Menu_Framework_MenuRow;

// decomp: vs_battle_menuState_t (src/BATTLE/BATTLE.PRG/5BF94.h:81)
typedef struct Menu_Framework_MenuControlState {
    char currentMenu;
    char returnState;
    char executeAbilityType;
    char loading;
    Content_DiscQueueSlot* cdQueue;
} Menu_Framework_MenuControlState;

#endif // VS_MENU_FRAMEWORK_FRAMEWORK_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_FRAMEWORK_FRAMEWORK_H_DECLS)
#define VS_MENU_FRAMEWORK_FRAMEWORK_H_DECLS
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/Weapons.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/MenuMemory.h"

extern signed char Menu_Framework_loadedSubMenu;
extern Menu_Framework_MenuRow* Menu_Framework_menuItems;
extern Items_LootListNode* Menu_Framework_lootLists;
extern Items_BattleLoot* Menu_Framework_lootEntry;
extern char Menu_textBoxBufferIndex;
extern u_int* Menu_Framework_menuBackgroundBuffer;
extern char Menu_Framework_kMainMenuHelpText[336];
extern int Menu_Framework_mainMenuCursorAnimation;
extern u_long Menu_textBoxOrderingTables;
extern char Menu_Framework_shortcutInvoked;
extern char Menu_saveLocationId;
extern char Menu_Framework_submenuStates[16];
extern int* Menu_Framework_rowTypeBuf;
extern char Menu_Framework_unlockedBreakArt;
extern char* Menu_Framework_stringBuf;
extern Menu_Framework_ActionSelection Menu_Framework_actionSelection;
extern int Menu_Framework_actionRestrictions;
extern int Menu_Framework_unusedInterfaceFlag;
extern Menu_Framework_MenuMemory Menu_Framework_menuMemory;
extern char Menu_Framework_unlockedBattleAbility;
extern char Menu_Framework_openedWithoutMainMenu;
extern Menu_Framework_MenuControlState Menu_Framework_menuState;
extern int Menu_Framework_menuLbas[];
extern char Menu_Framework_actionsSubMenuId;
extern short Menu_Framework_backgroundBrightness;
extern char Menu_Framework_backgroundFadeState;
extern short Menu_Framework_backgroundOtOffset;
extern char Menu_Framework_itemActionCount;
extern char Menu_Framework_itemActionListOffset;
extern u_short* Menu_Framework_itemsText;
extern char Menu_Framework_menuActionCurrent;
extern char Menu_Framework_menuActionNext;
extern char Menu_Framework_menuActionsPageOffset;;
extern char Menu_Framework_menuRowYOffset;
extern int Menu_Framework_selectedItemAction;
extern int Menu_Framework_selectedRow;
extern u_short* Menu_Framework_sortItemsTextBuf;
extern u_char Menu_Framework_subMenuId;
extern char Menu_Framework_subMenuPage;
extern u_short Menu_Framework_subMenuRowCount;
extern char Menu_Framework_subMenuRowInfo;
extern Menu_Framework_TextHeader Menu_Framework_textHeaders[2];;
extern char Menu_Framework_unusedBss3[6];;
extern char Menu_Framework_unusedBss4[18];;
extern char Menu_Framework_unusedBss7[2];;
extern char Menu_Framework_actionMenuState;
extern int Menu_Framework_backgroundFadeStep;
extern char Menu_Framework_unusedContainerEmptyBackup;;
extern char Menu_Framework_cursorColor;
extern char Menu_Framework_displayActionCost;
extern char Menu_Framework_hideMenu;
extern char Menu_Framework_itemsListRow;
extern Menu_Container_InventoryStorage* Menu_Framework_unusedInventoryStorage;;
int Menu_Framework_LoadMenuOverlay(int arg0);
Menu_Framework_MenuRow* Menu_Framework_MenuItem(int id);
Menu_Framework_MenuRow* Menu_Framework_SetMenuItem(
    int row, int x, int y, int w, int backgroundWidth, char* text);
int Menu_Framework_GradientStopColor(u_int gradient, u_int colorIndex, int brightness);
void Menu_Framework_DrawMenuItem(Menu_Framework_MenuRow* menuItem);
int Menu_Framework_UpdateMainMenuSelection(int mode);
void Menu_Framework_UpdateMenuOverlay(void);
void Menu_Framework_InitGameInterface(void* arg0);
void Menu_Framework_ResumeAfterActionSelection(void);
int Menu_Framework_ActionRestrictionFlags(int arg0);
int Menu_Framework_OpenGameMenu(int menuState, int arg1);
int Menu_Framework_OpenShortcutMenu(int shortcut);
void Menu_Framework_ReleaseMenuRows(void);
void Menu_Framework_OpenLootListMenu(Items_LootListNode* arg0);
void Menu_Framework_OpenLootEntryMenu(Items_BattleLoot* arg0);
void Menu_Framework_OpenChestStorageMenu(int arg0);
void Menu_Framework_OpenAbilityUnlockMenu(int art, int battleAbility);
void Menu_Framework_OpenChallengeMenu(void);
Menu_Framework_ActionSelection* Menu_Framework_ActionSelectionState(void);
int Menu_Framework_DrawMainMenuCursor(int animStep, int position);
void Menu_Framework_CreateActionRows(
    int rowCount, int subMenuId, char* menuText[], int rowTypes[]);
int Menu_Framework_HighlightedActionRow(void);
int Menu_Framework_ActionSelectionResult(void);
Menu_Framework_MenuRow* Menu_Framework_CreateActionRow(int row, int x);
void Menu_Framework_UpdateActionMenu(void);
void Menu_Framework_UpdateInformationVisibility(int sourceRow, int actionMenuState);
void Menu_Framework_SetCommandLabel(enum Menu_Framework_MenuCommand action);
void Menu_Framework_DrawCommandLabel(void);
void Menu_Framework_SetBackgroundFade(int fadeState);
void Menu_Framework_DeactivateRow(int row);
void Menu_Framework_DismissInformationBox(void);
void Menu_Framework_InitializeInformationBox(void);
void Menu_Framework_SetInformationMessage(char* message);
void Menu_Framework_DrawButtonIcon(
    enum Menu_Framework_ButtonId buttonId, int x, int y, u_long* before);
int Menu_Framework_DrawCursor(u_int animStep, int xy);
int Menu_Framework_DrawColoredInteger(int value, int xy, int color, u_long* before);
int Menu_Framework_DrawComparedInteger(int value, int xy, int threshold, u_long* before);
void Menu_Framework_DrawNeutralInteger(int value, int xy, u_long* before);
void Menu_Framework_DrawFeatheredRowIcon(int direction, int xy, int uvClut, u_long* before);
void Menu_Framework_DrawMaterialIndicator(int materialId, int xy);
void Menu_Framework_DrawEquipmentStateIcon(int icon, int xy);
void Menu_Framework_DrawRowIcon(int icon, int x, int y);
void Menu_Framework_SetActionCostText(int index, char const* text, int xOffset, int disabled);
void Menu_Framework_DrawActionCostPanel(void);
void Menu_Framework_DrawButtonPromptBackground(int x, int y, int w, int h);
void Menu_Framework_SetBackgroundPriority(int otOffset, int brightness);
void Menu_Framework_LoadBackgroundTexture(u_int* buf);
void Menu_Framework_CreateMenuRows(int rowCount, int rowInfo, char* strings[], int rowTypes[]);
int Menu_Framework_CloseRowsAndRememberCursor(void);
int Menu_Framework_HighlightedRow(void);
int Menu_Framework_GetSelectedRow(void);
Menu_Framework_MenuRow* Menu_Framework_CreateListRow(int row, int x);
void Menu_Framework_UpdateRowSelection(void);
void Menu_Framework_DrawBackground(int brightness);
int Menu_Framework_DrawFeatheredRowCharacter(
    u_int character, int x, Menu_Framework_MenuRow* menuItem, u_long* before);
void Menu_Framework_DrawTabNavigation(int mode);
void Menu_Framework_DrawScrollableRow(Menu_Framework_MenuRow* menuItem);
void Menu_Framework_UpdateAndDrawRows(void);
void Menu_Framework_UpdateScreen(void);
void Menu_Framework_NoOperation(void);
int Menu_Framework_NextFreeActorAssetRequest(void);
void Menu_Framework_FlyOutRowLeft(int row);
void Menu_Framework_FlyOutRowsLeftExcept(int row);
void Menu_Framework_FlyOutRowRight(int row);
void Menu_Framework_ClearMenuExcept(enum Menu_Framework_MenuItemId id);
void Menu_Framework_FlyOutRowsAndHoistSelection(int row, int offset);
int Menu_Framework_AreMenuRowsIdle(void);
int Menu_Framework_QuitDelay(void);
int Menu_Framework_ReturnFromSubmenu(int arg0);
void Menu_Framework_UpdateMainMenu(int arg0);
void Menu_Framework_SetMenuVisible(int display);
void Menu_Framework_ClearMenuExceptTitle(void);
void Menu_Framework_BuildYawMatrix(MATRIX* arg0, short arg1);
void Menu_Framework_BuildPitchMatrix(MATRIX* arg0, short arg1);
void Menu_Framework_ShowSecondaryTitle(char* text);
void Menu_Framework_RenderAshley(int mode);
void Menu_Framework_ContinueItemDetailsTransition(int id);
void Menu_Framework_UpdateItemDetailsSelection(int row, char* text[], u_int rowType, int page);
int Menu_Framework_WeaponSortValue(int stat, Items_WeaponLoadout* weapon);
int Menu_Framework_ShieldSortValue(int stat, Items_ShieldLoadout* shield);
int Menu_Framework_MiscSortValue(int stat, Items_InventoryMisc* misc);
int Menu_Framework_EquipmentSortValue(int stat, Items_EquipmentPartStats* equipment);
void Menu_Framework_ResetMainMenuStatics(void);
void Menu_Framework_ResetMenu3Statics(void);

#endif // VS_MENU_FRAMEWORK_FRAMEWORK_H_DECLS
