// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"

extern int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
