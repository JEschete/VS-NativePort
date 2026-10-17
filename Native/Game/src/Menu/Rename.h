// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_RENAME_H_TYPES
#define VS_MENU_RENAME_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_RENAME_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_RENAME_H_DECLS)
#define VS_MENU_RENAME_H_DECLS

extern char Menu_Rename_navIconPulseRequest;
extern char Menu_Rename_layoutPadding2;;
extern u_char Menu_Rename_animStep;;
extern char* Menu_Rename_charTable;;
extern char Menu_Rename_nameNavIconAnim;
extern u_short Menu_Rename_renderRenameMenuStrings[];
extern u_char Menu_Rename_selectedControllerAction;
extern char Menu_Rename_showRenameInterface;;
extern char Menu_Rename_stringBuf[20];;
extern u_char Menu_Rename_weaponId;;
int Menu_Rename_EditNameAtCursor(int bufPos);
int Menu_Rename_CopyTrimmedName(char* stringBuf);
void Menu_Rename_DrawFrame(int xOffset);
void Menu_Rename_DrawSelectionBox(int x, int y, int w, int h);
int Menu_Rename_HighlightGridCharacter(int cursorState, int column, int row);
int Menu_Rename_UpdateConfirmation(int init);
void Menu_Rename_SetCommandHighlight(int row);
int Menu_Rename_UpdateNameEditor(int initialize);
int Menu_Rename_Update(char* state);
void Menu_Rename_ResetMenu8Statics(void);

#endif // VS_MENU_RENAME_H_DECLS
