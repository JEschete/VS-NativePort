// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTION_H_TYPES
#define VS_BATTLE_ACTION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_ActionAiFlags Battle_ActionAiFlags;
typedef struct Battle_ActionDefinition Battle_ActionDefinition;
typedef union Battle_ActionHitParamsWord Battle_ActionHitParamsWord;
typedef struct Battle_ActionRequest Battle_ActionRequest;
typedef struct Battle_ActionState Battle_ActionState;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_ACTION_H_SET_TYPES_ONLY
#endif
#include "Battle/Hit.h"
#include "Battle/Targeting.h"
#ifdef VS_BATTLE_ACTION_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_ACTION_H_SET_TYPES_ONLY
#endif

// decomp: actionTypes (src/SLUS_010.40/main.h:51)
enum Battle_ActionType {
    Battle_ActionType_Spell = 1,
    Battle_ActionType_Ability,
    Battle_ActionType_BreakArt,
    Battle_ActionType_Item = 5,
    Battle_ActionType_BaseAttack,
    Battle_ActionType_Trap
};
typedef enum Battle_ActionType Battle_ActionType;

// decomp: actions_e (src/SLUS_010.40/main.h:44)
enum Battle_ActionId { Battle_ActionId_DaggerArtOne = 184 };
typedef enum Battle_ActionId Battle_ActionId;

// decomp: vs_battle_warlockSpellIds_e (src/BATTLE/BATTLE.PRG/5BF94.h:6)
enum Battle_WarlockSpell {
    Battle_WarlockSpell_SolidShock = 0x61,
    Battle_WarlockSpell_LightningBolt,
    Battle_WarlockSpell_Fireball,
    Battle_WarlockSpell_VulcanLance,
    Battle_WarlockSpell_AquaBlast,
    Battle_WarlockSpell_SpiritSurge,
    Battle_WarlockSpell_DarkChant,
    Battle_WarlockSpell_Exorcism,
    Battle_WarlockSpell_Banish,
    Battle_WarlockSpell_Explosion,
    Battle_WarlockSpell_Thunderburst = 0x6E,
    Battle_WarlockSpell_FlameSphere = 0x72,
    Battle_WarlockSpell_GaeaStrike = 0x76,
    Battle_WarlockSpell_Avalanche = 0x7A,
    Battle_WarlockSpell_RadialSurge = 0x7E,
    Battle_WarlockSpell_Meteor = 0x82,
    Battle_WarlockSpell_DrainHeart = 0x5E,
    Battle_WarlockSpell_DrainMind = 0x5F
};
typedef enum Battle_WarlockSpell Battle_WarlockSpell;

// decomp: D_800F19CC_t2 (src/BATTLE/BATTLE.PRG/146C.c:89)
typedef struct Battle_ActionRequest {
    u_short actionId;
    char effectVariant;
    char damageType;
    Battle_HitResult primaryTarget;
    u_short animationParameter;
    u_short targetCount;
    Battle_HitResult targets[30];
    SVECTOR focusPosition;
} Battle_ActionRequest;

// decomp: D_800F19CC_t (src/BATTLE/BATTLE.PRG/146C.c:138)
typedef struct Battle_ActionState {
    u_int queueIndex;
    u_int chainStep;
    Battle_ActionRequest selection;
    Battle_ActionRequest queue[4];
    int isPlayerAction;
    int sourceActorId;
    u_char selectedPreview;
    u_char cameraPreview;
    char selectionFrames;
    char reserved298F;
    SVECTOR savedAreaCenter;
    SVECTOR areaCenter;
    char rangeShape;
    char rangeKind;
    short rangeFacing;
    char rangeX;
    char rangeY;
    char rangeZ;
    char rangeAngle;
    SVECTOR rangeOrigin;
    char areaShape;
    char areaKind;
    short areaFacing;
    char areaX;
    char areaY;
    char areaZ;
    char areaAngle;
    SVECTOR areaOrigin;
    Battle_TargetPreview previews[23];
    int reserved2BE8;
    int reserved2BEC;
    int reserved2BF0;
    int reserved2BF4;
    int reserved2BF8;
    int reserved2BFC;
    u_short previewCount;
    char previousChainButtons;
    char chainAbilitySlot;
    char previousChainInput;
    char abilityInputSucceeded;
    char abilityInputAttempted;
    char chainContinued;
    u_short abilityTimer;
    u_short abilityWindow;
    short preferredCameraDistance;
    short cameraZoomState;
} Battle_ActionState;

// decomp: vs_actionFlags_t (src/SLUS_010.40/main.h:72)
typedef struct Battle_ActionAiFlags {
    u_short reserved0Bit0 : 4;
    u_short opaque0Bit4 : 1;
    u_short rarelyChosen : 1;
    u_short reserved0Bit6 : 1;
    u_short shapedArea : 1;
    u_short reserved1Bit0 : 4;
    u_short opaque1Bit4 : 1;
    u_short radialArea : 1;
    u_short reserved1Bit6 : 2;
} Battle_ActionAiFlags;
_Static_assert(sizeof(Battle_ActionAiFlags) == 0x2, "Battle_ActionAiFlags must keep its PS1 layout");

// decomp: vs_action_t (src/SLUS_010.40/main.h:107)
typedef struct Battle_ActionDefinition {
    u_char id;
    u_char effectId;
    u_char returnsToMenuOnCancel : 1;
    u_char type : 3;
    u_char category : 4;
    u_char cost;
    u_char rangeX;
    u_char rangeY;
    u_char rangeZ;
    u_char shape : 3;
    u_char angle : 5;
    u_char areaRangeX;
    u_char areaRangeY;
    u_char areaRangeZ;
    u_char areaShape;
    u_char recoveryDelay;
    u_char reservedDBit0 : 4;
    u_char opaqueDBit4 : 2;
    u_char resetsHitResults : 1;
    u_char unlocked : 1;
    Battle_ActionAiFlags aiFlags;
    int reserved10;
    struct {
        u_int effectId : 7;
        u_int prerequisiteFunctionId : 6;
        u_int hitFunctionId : 3;
        u_int statCalculatorId : 6;
        u_int statFactor : 5;
        u_int damageType : 2;
        u_int affinity : 3;
    } hits[2];
    char name[24];
} Battle_ActionDefinition;

// decomp: vs_hitParams_t (src/SLUS_010.40/main.h:49)
typedef union Battle_ActionHitParamsWord {
    u_short halves[2];
    u_int word;
} Battle_ActionHitParamsWord;
_Static_assert(sizeof(Battle_ActionHitParamsWord) == 0x4, "Battle_ActionHitParamsWord must keep its PS1 layout");

#endif // VS_BATTLE_ACTION_H_TYPES
