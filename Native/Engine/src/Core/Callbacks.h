// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_CALLBACKS_H_TYPES
#define VS_CORE_CALLBACKS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_CALLBACKS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_CALLBACKS_H_DECLS)
#define VS_CORE_CALLBACKS_H_DECLS
#include "Battle/Actor.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

void Core_IgnoreCallback(Battle_ActorStats* actor __attribute__((unused)));

#endif // VS_CORE_CALLBACKS_H_DECLS
