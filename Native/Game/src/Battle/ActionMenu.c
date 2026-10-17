// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Battle/ActionMenu.h"

// decomp: _spellClassCounts (_spellClassCounts@BATTLE/BATTLE.PRG)
u_char Battle_spellClassCounts[4]; /* contents: module image (ModuleData.c) */

// decomp: _spellIds (_spellIds@BATTLE/BATTLE.PRG)
u_char* Battle_spellClassSpellIds[4]; /* contents: module image, relocated (ModulePointers.c) */

// decomp: vs_battle_chainAbilityOffsets (vs_battle_chainAbilityOffsets@BATTLE/BATTLE.PRG)
char Battle_chainAbilityActionIds[16]; /* contents: module image (ModuleData.c) */

// decomp: vs_battle_defenseAbilityOffsets (vs_battle_defenseAbilityOffsets@BATTLE/BATTLE.PRG)
char Battle_defenseAbilityActionIds[16]; /* contents: module image (ModuleData.c) */

