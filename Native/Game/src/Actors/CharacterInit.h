// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_CHARACTERINIT_H_TYPES
#define VS_ACTORS_CHARACTERINIT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_CharacterInit Actors_CharacterInit;
typedef struct Actors_LimbActionInit Actors_LimbActionInit;
typedef struct Actors_LimbInit Actors_LimbInit;
typedef struct Actors_SpawnSettings Actors_SpawnSettings;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ACTORS_CHARACTERINIT_H_SET_TYPES_ONLY
#endif
#include "Items/EquipmentInit.h"
#include "Items/Loadout.h"
#ifdef VS_ACTORS_CHARACTERINIT_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ACTORS_CHARACTERINIT_H_SET_TYPES_ONLY
#endif

// decomp: _actions (src/BATTLE/BATTLE.PRG/146C.h:845)
typedef struct Actors_LimbActionInit {
    u_short actionId;
    char opaque2;
    char index;
} Actors_LimbActionInit;

// decomp: _limbInit (src/BATTLE/BATTLE.PRG/146C.h:857)
typedef struct Actors_LimbInit {
    u_short hp;
    char agilityDefenseBonus;
    char chainEvasion;
    signed char types[4];
    signed char affinities[8];
    Actors_LimbActionInit actions[4];
    Items_ArmorInit armor;
    char dpSplash[6];
    char reserved5A[2];
} Actors_LimbInit;

// decomp: actorSpawnSettings (src/BATTLE/BATTLE.PRG/146C.c:260)
typedef struct Actors_SpawnSettings {
    char bytes[8];
} Actors_SpawnSettings;

// decomp: vs_battle_charInitData (src/BATTLE/BATTLE.PRG/146C.h:886)
typedef struct Actors_CharacterInit {
    u_short subType;
    char characterKind;
    u_int reach : 5;
    u_int enemyClass : 3;
    char name[24];
    short hp;
    short mp;
    char strength;
    char intelligence;
    char agility;
    char opaque23;
    char material;
    char walkSpeed;
    char opaque26;
    char runSpeed;
    int opaque28;
    int opaque2C;
    char opaque30;
    char opaque31;
    char opaque32;
    char reserved33;
    Items_WeaponSpawnLoadout weapon;
    Items_ShieldSpawnLoadout shield;
    Items_AccessoryInit accessory;
    Actors_LimbInit limbs[6];
    int zudIndex;
} Actors_CharacterInit;

#endif // VS_ACTORS_CHARACTERINIT_H_TYPES
