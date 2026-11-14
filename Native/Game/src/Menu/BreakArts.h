// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_BREAKARTS_H_TYPES
#define VS_MENU_BREAKARTS_H_TYPES
#include "Legacy/Psx.h"


// decomp: drawArtsListOpt (src/MENU/MENU1.PRG/30.c:94)
enum Menu_BreakArts_ArtListOption { Menu_BreakArts_ArtListOption_OptForceCursorMemory = 0x10 };
typedef enum Menu_BreakArts_ArtListOption Menu_BreakArts_ArtListOption;

#endif // VS_MENU_BREAKARTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_BREAKARTS_H_DECLS)
#define VS_MENU_BREAKARTS_H_DECLS

extern char Menu_BreakArts_layoutPadding1;;
extern u_short Menu_BreakArts_strings[2151]; /* contents: disc string table (ModuleData.cpp) */;

#endif // VS_MENU_BREAKARTS_H_DECLS
