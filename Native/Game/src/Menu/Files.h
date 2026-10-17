// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FILES_H_TYPES
#define VS_MENU_FILES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Files_FileMenuElement Menu_Files_FileMenuElement;
typedef struct Menu_Files_PrimitiveBuffer Menu_Files_PrimitiveBuffer;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_MENU_FILES_H_SET_TYPES_ONLY
#endif
#include "Render/LegacyPackets.h"
#ifdef VS_MENU_FILES_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_MENU_FILES_H_SET_TYPES_ONLY
#endif

// decomp: src/SLUS_010.40/sfx.h:3
#define VS_SFX_MENUCHANGE 4

// decomp: src/SLUS_010.40/sfx.h:5
#define VS_SFX_MENULEAVE 6

// decomp: src/SLUS_010.40/sfx.h:6
#define VS_SFX_INVALID 7

// decomp: src/SLUS_010.40/sfx.h:7
#define VS_SFX_FILEOPCOMPLETE 8

// decomp: src/include/gpu.h:106
#define vs_getRGB5551(r, g, b, a)                                                        \
    (((r) & 0x1F) | (((g) & 0x1F) << 5) | (((b) & 0x1F) << 10) | (((a) & 0x1) << 15))

// decomp: fileMenuElementState_e (src/MENU/MENU7.PRG/260.c:110)
enum Menu_Files_ElementState {
    Menu_Files_ElementState_Inactive = 0,
    Menu_Files_ElementState_Static = 1,
    Menu_Files_ElementState_AnimateX = 2,
    Menu_Files_ElementState_AnimateY = 3,
    Menu_Files_ElementState_AnimateNegX = 4
};
typedef enum Menu_Files_ElementState Menu_Files_ElementState;

// decomp: statType_e (src/MENU/MENU7.PRG/260.c:1265)
enum Menu_Files_StatType { Menu_Files_StatType_HP = 0, Menu_Files_StatType_MP = 1 };
typedef enum Menu_Files_StatType Menu_Files_StatType;

// decomp: vs_fileMenuUiIds_e (src/MENU/MENU7.PRG/260.c:1206)
enum Menu_Files_FileMenuUiId {
    Menu_Files_FileMenuUiId_Colon = 0,
    Menu_Files_FileMenuUiId_Number = 1,
    Menu_Files_FileMenuUiId_Map = 2,
    Menu_Files_FileMenuUiId_Save = 3,
    Menu_Files_FileMenuUiId_Clear = 4,
    Menu_Files_FileMenuUiId_Time = 5,
    Menu_Files_FileMenuUiId_Percent = 6,
    Menu_Files_FileMenuUiId_Hp = 7,
    Menu_Files_FileMenuUiId_Mp = 8,
    Menu_Files_FileMenuUiId_Dot = 9,
};
typedef enum Menu_Files_FileMenuUiId Menu_Files_FileMenuUiId;

// decomp: fileMenuElements_t (src/MENU/MENU7.PRG/260.c:134)
typedef struct Menu_Files_FileMenuElement {
    char state;
    char slotId;
    char outertextBlendFactor;
    char innertextBlendFactor;
    char selected;
    char slotUnavailable;
    char saveLocation;
    char reserved7;
    short targetPosition;
    short reservedA;
    short x;
    short y;
    short w;
    short h;
    char text[32];
} Menu_Files_FileMenuElement;
_Static_assert(sizeof(Menu_Files_FileMenuElement) == 0x34, "Menu_Files_FileMenuElement must keep its PS1 layout");

// decomp: primBuf_t (src/MENU/MENU7.PRG/260.c:150)
typedef struct Menu_Files_PrimitiveBuffer {
    u_long tag;
    union {
        Render_SpriteTexturePagePacket sprt;
        Render_TileTexturePagePacket tileTpage;
        Render_TilePacket tile;
        Render_PagedGouraudQuadPacket polyG4Tpage;
        Render_GouraudQuadPacket polyG4;
        struct {
            Render_TileTexturePagePacket tileTpage;
            Render_GouraudQuadPacket polyG4;
        } tilePoly;
        u_long reserved0[15];
    } prim;
} Menu_Files_PrimitiveBuffer;

#endif // VS_MENU_FILES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_FILES_H_DECLS)
#define VS_MENU_FILES_H_DECLS
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Save/MemoryCard/GameFiles.h"

extern char Menu_Files_layoutPadding1[4];;
extern char Menu_Files_layoutPadding2[1];;
extern char Menu_Files_layoutPadding3[8];;
extern char Menu_Files_backupMainSetting;;
extern char Menu_Files_containerDataEmpty;;
extern u_short Menu_Files_containerItemCapacities[];
extern u_short Menu_Files_containerStrings[];
extern signed char Menu_Files_cursorFileOpSaturation[];
extern char Menu_Files_dataNotSaved;;
extern Menu_Files_FileMenuElement Menu_Files_fileMenuElements[10];;
extern char Menu_Files_fileMenuScreenFade;;
extern int Menu_Files_fileProgressCounter;;
extern char Menu_Files_isSaving;;
extern char* Menu_Files_memoryCardMessage;;
extern char Menu_Files_menuElementStops[];
extern u_short Menu_Files_menuItemTextClut[][16];
extern Menu_Files_PrimitiveBuffer Menu_Files_primBuf;;
extern u_char Menu_Files_selectCursorColor;;
extern int Menu_Files_selectCursorXy;;
extern u_short* Menu_Files_textTable;;
extern char Menu_Files_frameBuf;;
void Menu_Files_DrawSummaryLabel(int xy, enum Menu_Files_FileMenuUiId id);
void Menu_Files_DrawInteger(int xy, u_int value, u_int placeDivisor);
int Menu_Files_DecimalDigitCount(int val);
void Menu_Files_DrawHpMpSummary(int xy, enum Menu_Files_StatType stat, u_int currentValue, u_int maxValue);
void Menu_Files_DrawTransferProgress(int x, int y);
void Menu_Files_DrawTransferComplete(int colour, int y);
void Menu_Files_ResetElements(void);
Menu_Files_FileMenuElement* Menu_Files_InitializeElement(int id, int xy, int wh, char* text);
void Menu_Files_ClearElement(int id);
int Menu_Files_AreAnimationsComplete(void);
int Menu_Files_DrawGlyph(u_int c, int x, int y, int clut);
int Menu_Files_FindCurrentDataSaveStep(int init);
enum Save_MaskedHandlerResult Menu_Files_CheckCardStep(int portMask);
void Menu_Files_DrawText(char* text, int x, int y, int clut);
u_int Menu_Files_ElementBackgroundColor(u_int outerFactor, u_int innerFactor);
void Menu_Files_DrawElement(Menu_Files_FileMenuElement* element);
void Menu_Files_DrawFrame(int framebuf);
void Menu_Files_DrawBackground(void);
int Menu_Files_UpdateConfirmation(int arg0);
int Menu_Files_UpdateOverwritePrompt(int arg0);
int Menu_Files_UpdateFormatPrompt(int initPort);
int Menu_Files_UpdateSaveFiles(int initPort);
int Menu_Files_UpdateSaveCard(int initPort);
int Menu_Files_UpdateDataSaveMenu(int initState);
int Menu_Files_UpdateLoadFiles(int initPort);
long Menu_Files_UpdateLoadCard(int initPort);
int Menu_Files_UpdateDataLoadMenu(int initFadeout);
int Menu_Files_LoadGameOverAssetsStep(int arg0);
void Menu_Files_BlendGameOverTextClut(
    short* clut, int textBlendFactor, u_short* clut0, u_short* clut1);
int Menu_Files_UpdateGameOverChoices(int init);
int Menu_Files_UpdateGameOver(u_char* state);
int Menu_Files_UpdateContainerConfirmation(int initParams);
void* Menu_Files_ContainerIndexSection(int section, Items_ContainerIndices* indices);
void Menu_Files_CopySelectedContainerItems(Items_ContainerData* target, Items_ContainerData* source,
    Items_ContainerIndices* toCopy);
int Menu_Files_UpdateContainerSave(u_char* state);
void Menu_Files_DrawPlayTime(void);
int Menu_Files_UpdateDataMenu(u_char* state);
int Menu_Files_FindCurrentSaveStep(int init);
int Menu_Files_UpdateLoadMenu(int initFadeout);
int Menu_Files_UpdateSaveMenu(int initState);
void Menu_Files_InitializeDisplay(void);
void Menu_Files_SwapBuffers(void);
int Menu_Files_UpdateClearSaveReminder(int init);
void Menu_Files_DrawExitFade(void);
void Menu_Files_ResetMenu7Statics(void);

#endif // VS_MENU_FILES_H_DECLS
