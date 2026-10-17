// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_HIT_H_TYPES
#define VS_BATTLE_HIT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_DamageCalculation Battle_DamageCalculation;
typedef struct Battle_HitResult Battle_HitResult;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_HIT_H_SET_TYPES_ONLY
#endif
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#ifdef VS_BATTLE_HIT_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_HIT_H_SET_TYPES_ONLY
#endif

// decomp: _hitEntity_t (src/BATTLE/BATTLE.PRG/146C.c:78)
typedef struct Battle_HitResult {
    Battle_HitTargetWord target;
    short riskChange;
    short strengthChange;
    short intelligenceChange;
    short agilityChange;
    short maxHpChange;
    short maxMpChange;
    int removeStatuses;
    int inflictStatuses;
    u_int hpEffect : 2;
    u_int mpEffect : 2;
    u_int riskEffect : 2;
    u_int strengthEffect : 2;
    u_int intelligenceEffect : 2;
    u_int agilityEffect : 2;
    u_int maxHpEffect : 2;
    u_int maxMpEffect : 2;
    u_int statStatusEffect : 2;
    u_int weaponDpEffect : 2;
    u_int weaponPpEffect : 2;
    u_int armorDpEffect : 2;
    u_int shieldPpEffect : 2;
    u_int sourceResult : 2;
    u_int reservedBits28 : 4;
    union {
        Battle_LimbEffects effects;
        u_int packed;
    } limbs[6];
    short weaponDpChange;
    short weaponPpChange;
    short armorDpChange;
    short shieldPpChange;
    int targetsPosition;
} Battle_HitResult;

// decomp: _hitIntermediate (src/BATTLE/BATTLE.PRG/146C.c:11269)
typedef struct Battle_DamageCalculation {
    /* 0x00 */ u_char type;
    /* 0x01 */ u_char sourceAffinity;
    /* 0x02 */ u_char sourceClass;
    /* 0x03 */ u_char reserved3;
    /* 0x04 */ short typeValue;
    /* 0x06 */ short weaponAffinity;
    /* 0x08 */ short weaponClass;
    /* 0x0A */ short attackStat;
    /* 0x0C */ short weaponDamage;
    /* 0x0E */ short reservedE;
    /* 0x10 */ u_char reserved10;
    /* 0x11 */ u_char targetAffinity;
    /* 0x12 */ u_char targetClass;
    /* 0x13 */ u_char reserved13;
    /* 0x14 */ short reserved14;
    /* 0x16 */ short tempTargetAffinity;
    /* 0x18 */ short reserved18;
    /* 0x1A */ short mediatingStat;
    /* 0x1C */ short shieldStat;
    /* 0x1E */ short armorStat;
    /* 0x20 */ short statFactor;
    /* 0x22 */ short shieldStatFactor;
    /* 0x24 */ short armorStatFactor;
} Battle_DamageCalculation;
_Static_assert(sizeof(Battle_DamageCalculation) == 0x26, "Battle_DamageCalculation must keep its PS1 layout");

#endif // VS_BATTLE_HIT_H_TYPES
