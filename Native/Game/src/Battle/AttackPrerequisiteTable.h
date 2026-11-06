// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ATTACKPREREQUISITETABLE_H_TYPES
#define VS_BATTLE_ATTACKPREREQUISITETABLE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_ATTACKPREREQUISITETABLE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ATTACKPREREQUISITETABLE_H_DECLS)
#define VS_BATTLE_ATTACKPREREQUISITETABLE_H_DECLS
#include "Battle/Action.h"

extern int (*Battle_attackPrerequisiteFunctions[40])(Battle_ActionDefinition*, u_char*);

#endif // VS_BATTLE_ATTACKPREREQUISITETABLE_H_DECLS
