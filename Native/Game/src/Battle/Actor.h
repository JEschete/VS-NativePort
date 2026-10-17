// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTOR_H_TYPES
#define VS_BATTLE_ACTOR_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ActorModelSlot Actors_ActorModelSlot;
typedef struct Actors_ModelInstance Actors_ModelInstance;
typedef struct Battle_ActionSlot Battle_ActionSlot;
typedef struct Battle_Actor Battle_Actor;
typedef struct Battle_ActorStats Battle_ActorStats;
typedef struct Battle_ActorStatsFlags Battle_ActorStatsFlags;
typedef struct Battle_LimbStats Battle_LimbStats;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_ACTOR_H_SET_TYPES_ONLY
#endif
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#ifdef VS_BATTLE_ACTOR_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_ACTOR_H_SET_TYPES_ONLY
#endif

// decomp: vs_battle_limbStatus (src/BATTLE/BATTLE.PRG/5BF94.h:502)
enum Battle_LimbConditionLevel {
    Battle_LimbConditionLevel_Critical,
    Battle_LimbConditionLevel_Damaged,
    Battle_LimbConditionLevel_Wounded,
    Battle_LimbConditionLevel_Good,
    Battle_LimbConditionLevel_Excellent
};
typedef enum Battle_LimbConditionLevel Battle_LimbConditionLevel;

// decomp: vs_battle_actor (src/BATTLE/BATTLE.PRG/146C.h:603)
typedef struct Battle_Actor {
    /* 0x00 */ struct Battle_Actor* next;
    /* 0x04 */ int id;
    /* 0x08 */ u_char commandState;
    /* 0x09 */ u_char commandOpcode;
    /* 0x0A */ u_char commandTarget;
    /* 0x0B */ u_char commandFlags;
    /* 0x0C */ int commandArg0;
    union {
        u_char bytes[4];
        u_short halves[2];
        int word;
    /* 0x10 */ } commandArg1;
    int commandArg2;
    u_short modelId;
    u_short subType;
    u_short kind;
    u_char facing;
    u_char spawnByte26;
    int weaponDrawn;
    u_char interactionFlags;
    u_char equippedWeaponCategory;
    u_char retainAfterDefeat;
    u_char roomSpawnIndex;
    u_char defeated;
    u_char killCounted;
    u_char bladeId;
    u_char shieldId;
    union {
        u_char bytes[4];
        int word;
    } lastTriggerTile;
    u_int aiGroup : 4;
    u_int reserved30Bit4 : 5;
    u_int aiLinkedGroup : 5;
    u_int reserved31Bit6 : 18;
    int reserved34;
    int templateWord2C;
    Battle_ActorStats* stats;
    int allocationKind;
    Actors_ModelInstance* model;
    Actors_ActorModelSlot* equipmentModels[2];
} Battle_Actor;

// decomp: vs_battle_actor_unk8C0 (src/BATTLE/BATTLE.PRG/146C.h:447)
typedef struct Battle_ActionSlot {
    u_short actionId;
    char index : 4;
    char effectModelId : 4;
    char opaque3;
} Battle_ActionSlot;
_Static_assert(sizeof(Battle_ActionSlot) == 0x4, "Battle_ActionSlot must keep its PS1 layout");

// decomp: vs_battle_uiEquipment_limb (src/BATTLE/BATTLE.PRG/146C.h:411)
typedef struct Battle_LimbStats {
    /* 0x00 */ short hp;
    /* 0x02 */ short maxHp;
    /* 0x04 */ signed char agilityDefenseBonus;
    /* 0x05 */ char anchorBone;
    /* 0x06 */ u_char nameIndex;
    /* 0x07 */ char chainEvasion;
    /* 0x08 */ short types[4];
    /* 0x10 */ short affinities[8];
    /* 0x20 */ Items_ArmorLoadout armor;
    /* 0xDC */
} Battle_LimbStats;
_Static_assert(sizeof(Battle_LimbStats) == 0xDC, "Battle_LimbStats must keep its PS1 layout");

// decomp: vs_battle_actor2 (src/BATTLE/BATTLE.PRG/146C.h:559)
typedef struct Battle_ActorStats {
    /* 0x000 */ char name[24];
    /* 0x018 */ short currentHp;
    /* 0x01A */ short maxHp;
    /* 0x01C */ short currentMp;
    /* 0x01E */ short maxMp;
    /* 0x020 */ u_short risk;
    /* 0x022 */ u_short strength;
    /* 0x024 */ u_short totalStrength;
    /* 0x026 */ u_short intelligence;
    /* 0x028 */ u_short totalIntelligence;
    /* 0x02A */ u_short agility;
    /* 0x02C */ u_short totalAgility;
    /* 0x02E */ char baseWalkSpeed;
    /* 0x02F */ char baseRunSpeed;
    /* 0x030 */ char movementClass;
    /* 0x031 */ char walkSpeed;
    /* 0x032 */ char gaitClass;
    /* 0x033 */ char runSpeed;
    /* 0x034 */ u_int mobilityLimb0 : 8;
    /* 0x035 */ u_int mobilityLimb1 : 8;
    /* 0x036 */ u_int defaultTargetLimb : 8;
    /* 0x037 */ u_int enemyClass : 3;
    /* 0x037 */ u_int reach : 5;
    /* 0x038 */ u_int currentRange : 8;
    /* 0x039 */ u_int currentRangeY : 8;
    /* 0x03A */ u_int currentRangeZ : 8;
    /* 0x03B */ u_int currentAttackShape : 3;
    /* 0x03B */ u_int currentAttackShapeAngle : 5;
    /* 0x03C */ Items_WeaponLoadout weapon;
    /* 0x1D4 */ Items_ShieldLoadout shield;
    /* 0x338 */ Items_AccessorySpawnLoadout accessory;
    /* 0x398 */ Battle_LimbStats limbs[6];
    /* 0x8C0 */ Battle_ActionSlot limbActions[6][4];
    /* 0x920 */ char dpSplash[6][6];
    /* 0x944 */ int statusImmunities;
    /* 0x948 */ int statuses;
    /* 0x94C */ u_char statusTimers[8];
    /* 0x954 */ u_short actionRecoveryTimer;
    /* 0x956 */ u_char cancelsHitResults : 1;
    /* 0x956 */ u_char isAnalyzed : 1;
    /* 0x956 */ u_char recoveryDelayBonus : 6;
    /* 0x957 */ u_char actorId;
    /* 0x958 */ Items_MiscItemStack dropItem;
    /* 0x95C */ Items_MiscItemStack secondDropItem;
    /* 0x960 */ Items_MiscItemDrop randomDropItem;
    /* 0x964 */
} Battle_ActorStats;
_Static_assert(sizeof(Battle_ActorStats) == 0x964, "Battle_ActorStats must keep its PS1 layout");

// decomp: vs_battle_actor2_flags (src/BATTLE/BATTLE.PRG/146C.h:455)
typedef struct Battle_ActorStatsFlags {
    u_short reserved0;
    char reserved2Bit0 : 1;
    char reserved2Bit1 : 1;
    char reserved2Bit2 : 6;
    u_char reserved3;
} Battle_ActorStatsFlags;
_Static_assert(sizeof(Battle_ActorStatsFlags) == 0x4, "Battle_ActorStatsFlags must keep its PS1 layout");

#endif // VS_BATTLE_ACTOR_H_TYPES
