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
