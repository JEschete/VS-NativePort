// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_CharacterInit;
struct Actors_LimbActionInit;
struct Actors_LimbInit;
struct Actors_SpawnSettings;

#include "Items/EquipmentInit.h"
#include "Items/Loadout.h"

// decomp: _actions (src/BATTLE/BATTLE.PRG/146C.h:845)
struct Actors_LimbActionInit {
    u_short actionId;
    char opaque2;
    char index;
};
static_assert(sizeof(Actors_LimbActionInit) == 0x4, "Actors_LimbActionInit must keep its PS1 layout");

// decomp: _limbInit (src/BATTLE/BATTLE.PRG/146C.h:857)
struct Actors_LimbInit {
    u_short hp;
    char agilityDefenseBonus;
    char chainEvasion;
    signed char types[4];
    signed char affinities[8];
    Actors_LimbActionInit actions[4];
    Items_ArmorInit armor;
    char dpSplash[6];
    char reserved5A[2];
};
static_assert(sizeof(Actors_LimbInit) == 0x5C, "Actors_LimbInit must keep its PS1 layout");

// decomp: actorSpawnSettings (src/BATTLE/BATTLE.PRG/146C.c:260)
struct Actors_SpawnSettings {
    char bytes[8];
};
static_assert(sizeof(Actors_SpawnSettings) == 0x8, "Actors_SpawnSettings must keep its PS1 layout");

// decomp: vs_battle_charInitData (src/BATTLE/BATTLE.PRG/146C.h:886)
struct Actors_CharacterInit {
    u_short subType;
    char characterKind;
    u_char reach : 5;
    u_char enemyClass : 3;
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
};
static_assert(sizeof(Actors_CharacterInit) == 0x464, "Actors_CharacterInit must keep its PS1 layout");
