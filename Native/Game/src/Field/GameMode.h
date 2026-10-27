// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_GAMEMODE_H_TYPES
#define VS_FIELD_GAMEMODE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_GAMEMODE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_GAMEMODE_H_DECLS)
#define VS_FIELD_GAMEMODE_H_DECLS
#include "Menu/Framework/ActionSelection.h"

extern int Field_isInteractionPaused;
void Field_PauseFieldForInteractionDialog(void);
void Field_PauseFieldForObjectInteraction(void);
void Field_PauseFieldForMenu(void);
void Field_ResumeFieldAfterMenu(void);
void Field_EnterFieldExplorationMode(void);
void Field_BeginShortcutMenu(void);
void Field_EndShortcutMenu(void);
void Field_BeginBattleActionMenu(int arg0 __attribute__((unused)), int arg1 __attribute__((unused)));
void Field_CloseBattleActionMenu(Menu_Framework_ActionSelection* action_selection);
void Field_DispatchBattleActionMenuResult(Menu_Framework_ActionSelection* action_selection);

#endif // VS_FIELD_GAMEMODE_H_DECLS
