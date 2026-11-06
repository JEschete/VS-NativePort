// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_TARGETINGSTATE_H_TYPES
#define VS_BATTLE_TARGETINGSTATE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_TargetPreview Battle_TargetPreview;

// decomp: D_800F19CC_t5 (src/BATTLE/BATTLE.PRG/146C.h:901)
typedef struct Battle_TargetPreview {
    SVECTOR position;
    u_char kind;
    u_char actorId;
    signed char limb;
    char effect;
    short previewAmount;
    short hitChance;
    int removedStatuses;
    char actionKind;
    char opaque15;
    char opaque16;
    char enemyClass;
} Battle_TargetPreview;
_Static_assert(sizeof(Battle_TargetPreview) == 0x18, "Battle_TargetPreview must keep its PS1 layout");

#endif // VS_BATTLE_TARGETINGSTATE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_TARGETINGSTATE_H_DECLS)
#define VS_BATTLE_TARGETINGSTATE_H_DECLS
#include "Menu/Framework/TextBoxes.h"

extern char Battle_categoryNeedsTargetSelection[96];
extern u_char Battle_categoryIsAreaTarget[80];
extern u_char Battle_categoryIncludesActingActor[48];
extern u_char Battle_categoryTargetsBodyParts[32];
extern u_char Battle_categoryTargetActorMasks[16];
extern u_short Battle_statusStrings[1712];
extern char Battle_targetInfoTemplate[3140];
extern char Battle_targetStatusLineTemplate[3090];
extern char Battle_emptyStatusString[2932];
extern u_char Battle_targetSelectionStep;
extern u_char Battle_targetColumnEntries[2][16];
extern u_char Battle_secondColumnGroups[8];
extern u_char Battle_firstColumnGroups[8];
extern u_char Battle_secondColumnGroupFirstTargets[8];
extern u_char Battle_firstColumnGroupFirstTargets[8];
extern u_char Battle_secondColumnEntryCount;
extern u_char Battle_firstColumnEntryCount;
extern u_char Battle_secondColumnGroupCount;
extern u_char Battle_firstColumnGroupCount;
extern u_char Battle_targetColumnTopOffset;
extern Menu_StringContext Battle_stringContext;

#endif // VS_BATTLE_TARGETINGSTATE_H_DECLS
