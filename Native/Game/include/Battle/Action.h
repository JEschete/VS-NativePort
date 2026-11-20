// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Battle_ActionAiFlags;
struct Battle_ActionDefinition;
union Battle_ActionHitParamsWord;
struct Battle_ActionRequest;
struct Battle_ActionState;

#include "Battle/Hit.hpp"
#include "Battle/TargetingState.hpp"

// decomp: actionTypes (src/SLUS_010.40/main.h:51)
enum Battle_ActionType {
    Battle_ActionType_Spell = 1,
    Battle_ActionType_Ability,
    Battle_ActionType_BreakArt,
    Battle_ActionType_Item = 5,
    Battle_ActionType_BaseAttack,
    Battle_ActionType_Trap
};

// decomp: actions_e (src/SLUS_010.40/main.h:44)
enum Battle_ActionId { Battle_ActionId_DaggerArtOne = 184 };

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

// decomp: D_800F19CC_t2 (src/BATTLE/BATTLE.PRG/146C.c:89)
struct Battle_ActionRequest {
    u_short actionId;
    char effectVariant;
    char damageType;
    VS::Battle::HitResult primaryTarget;
    u_short animationParameter;
    u_short targetCount;
    VS::Battle::HitResult targets[255];
    SVECTOR focusPosition;
};

// decomp: D_800F19CC_t (src/BATTLE/BATTLE.PRG/146C.c:138)
struct Battle_ActionState {
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
    VS::Battle::TargetPreview previews[255];
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
};

// decomp: vs_actionFlags_t (src/SLUS_010.40/main.h:72)
struct Battle_ActionAiFlags {
    u_short reserved0Bit0 : 4;
    u_short opaque0Bit4 : 1;
    u_short rarelyChosen : 1;
    u_short reserved0Bit6 : 1;
    u_short shapedArea : 1;
    u_short reserved1Bit0 : 4;
    u_short opaque1Bit4 : 1;
    u_short radialArea : 1;
    u_short reserved1Bit6 : 2;
};
static_assert(sizeof(Battle_ActionAiFlags) == 0x2, "Battle_ActionAiFlags must keep its PS1 layout");

// decomp: vs_action_t (src/SLUS_010.40/main.h:107)
struct Battle_ActionDefinition {
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
};
static_assert(sizeof(Battle_ActionDefinition) == 0x34, "Battle_ActionDefinition must keep its PS1 layout");

// decomp: vs_hitParams_t (src/SLUS_010.40/main.h:49)
union Battle_ActionHitParamsWord {
    u_short halves[2];
    u_int word;
};
static_assert(sizeof(Battle_ActionHitParamsWord) == 0x4, "Battle_ActionHitParamsWord must keep its PS1 layout");
