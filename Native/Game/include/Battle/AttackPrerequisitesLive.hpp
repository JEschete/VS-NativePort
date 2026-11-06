#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/AttackPrerequisites.hpp"

struct Battle_ActionDefinition;

// Binding from the generated hit resolution caller to the typed attack
// prerequisites (AttackPrerequisites.hpp), in the original argument order
// (tools/port/copy_fixups.py OWNED_CALL_BINDINGS). A null action or target,
// a hit past the action's two hits, a rule past the table and an absent
// target actor are refused with a warning on the "battle.targeting" channel.
namespace VS::Battle {
// decomp: _canPerformAttack (src/BATTLE/BATTLE.PRG/146C.c:10523): 1 when the
// hit's prerequisite lets it apply, else 0.
int LiveCheckActionHitPrerequisite(Battle_ActionDefinition* action,
                                   unsigned char* target_result_bytes,
                                   int hit_number);
}  // namespace VS::Battle
