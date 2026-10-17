// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_SPAWNING_H_TYPES
#define VS_ACTORS_SPAWNING_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_SPAWNING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_SPAWNING_H_DECLS)
#define VS_ACTORS_SPAWNING_H_DECLS
#include "Actors/CharacterInit.h"
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/Loadout.h"

extern Actors_CharacterInit Actors_ashleyInitialLoadout;
extern char Actors_placeholderActorName[20];
extern u_char Actors_usesSecondaryBodyModel[256];

#endif // VS_ACTORS_SPAWNING_H_DECLS
