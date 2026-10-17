// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_TEXTBOXES_H_TYPES
#define VS_MENU_FRAMEWORK_TEXTBOXES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Framework_TextBoxSelector Menu_Framework_TextBoxSelector;
typedef struct Menu_StringContext Menu_StringContext;
typedef struct Menu_TextAnchorWord Menu_TextAnchorWord;
typedef struct Menu_TextBoxState Menu_TextBoxState;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_MENU_FRAMEWORK_TEXTBOXES_H_SET_TYPES_ONLY
#endif
#include "Battle/TextBoxes.h"
#ifdef VS_MENU_FRAMEWORK_TEXTBOXES_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_MENU_FRAMEWORK_TEXTBOXES_H_SET_TYPES_ONLY
#endif

// decomp: vs_battle_informationTextBoxHeader (src/BATTLE/BATTLE.PRG/5BF94.h:538)
enum Menu_InformationBoxHeader {
    Menu_InformationBoxHeader_None,
    Menu_InformationBoxHeader_Caution,
    Menu_InformationBoxHeader_Information,
    Menu_InformationBoxHeader_Check
};
typedef enum Menu_InformationBoxHeader Menu_InformationBoxHeader;

// decomp: _textBoxSelector_t (src/BATTLE/BATTLE.PRG/5BF94.c:186)
typedef struct Menu_Framework_TextBoxSelector {
    u_char order[8];
    u_char reserved8;
    u_char reserved9;
    u_char selectedIndex;
    u_char count;
    u_char reservedC;
} Menu_Framework_TextBoxSelector;

// decomp: func_800CCE10_t (src/BATTLE/BATTLE.PRG/5BF94.c:307)
typedef struct Menu_TextAnchorWord {
    int x : 12;
    int y : 12;
    u_int alignment : 2;
    u_int opaque3Bit2 : 4;
} Menu_TextAnchorWord;

// decomp: vs_battle_stringContext_t (src/BATTLE/BATTLE.PRG/5BF94.h:165)
typedef struct Menu_StringContext {
    int integers[10];
    char* strings[10];
} Menu_StringContext;

// decomp: vs_battle_textBox (src/BATTLE/BATTLE.PRG/5BF94.h:121)
typedef struct Menu_TextBoxState {
    Battle_TextBoxFlags flags;
    short opaque4;
    short opaque6;
    char* string;
    u_short speed;
    u_short prevChunksPrinted;
    short xIndent;
    short yIndent;
    short x;
    short y;
    short centerX;
    short centerY;
    short state;
    u_char brightness;
    u_char reserved1F;
    char charsPerLine;
    char lineCount;
    char openDuration;
    char reserved23;
    short frameX;
    short frameY;
    short frameWidth;
    short frameHeight;
    short opaque2C;
    short opaque2E;
} Menu_TextBoxState;

#endif // VS_MENU_FRAMEWORK_TEXTBOXES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_FRAMEWORK_TEXTBOXES_H_DECLS)
#define VS_MENU_FRAMEWORK_TEXTBOXES_H_DECLS

extern Menu_TextAnchorWord Menu_textBoxAnchors[2749][2];
extern int Menu_informationHeaderPositions[5480];

#endif // VS_MENU_FRAMEWORK_TEXTBOXES_H_DECLS
