#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/Targeting.hpp"
#include "Legacy/Psx.h"

struct Battle_ActorStats;
struct Battle_Ai_ActionRangeShape;
struct Battle_Ai_ActionRangeView;

// Bindings from the generated action flow, AI and field callers
// (tools/port/copy_fixups.py OWNED_CALL_BINDINGS) and from the hand-written
// gameplay loop, field mode, actor command, target page and diagnostics code
// to the typed action targeting (Targeting.hpp), in the original call shapes.
// The hosts read the live action state, actors, room and targeting sphere; a
// call without an action state, a sphere or a range view is refused with an
// error on the "battle.targeting" channel.
namespace VS::Battle {
// decomp: BATTLE.PRG func_800704D8 (src/BATTLE/BATTLE.PRG/146C.c:3578)
void LiveReturnToBattleActionMenu();
// decomp: BATTLE.PRG func_8007053C (src/BATTLE/BATTLE.PRG/146C.c:3588)
void LiveFilterAreaActionTargets();
// decomp: BATTLE.PRG func_800708EC (src/BATTLE/BATTLE.PRG/146C.c:3660)
void LiveSetSingleActionTarget();
// decomp: BATTLE.PRG func_80070B04 (src/BATTLE/BATTLE.PRG/146C.c:3695)
void LiveSetAllActionTargets();
// decomp: BATTLE.PRG func_80070CAC (src/BATTLE/BATTLE.PRG/146C.c:3728)
void LiveCollectEnvironmentActionTargets();
// decomp: BATTLE.PRG func_80072BA8 (src/BATTLE/BATTLE.PRG/146C.c:4416)
void LiveInitializeActionTargeting(int action_id);
// decomp: BATTLE.PRG func_80072EC4 (src/BATTLE/BATTLE.PRG/146C.c:4489)
void LiveBeginActorActionSelection(int source_actor_slot,
                                   std::uint16_t action_id);
// decomp: BATTLE.PRG func_800C4794 (src/BATTLE/BATTLE.PRG/5BF94.c:640): 1
// when the player confirmed or cancelled (a cancel sets the pad to 1).
int LiveUpdateSphereTargetPosition(SVECTOR* target_position);
// decomp: BATTLE.PRG func_800C58F8 (src/BATTLE/BATTLE.PRG/5BF94.c:1058): 1
// when the list closed; a null target resets the list.
int LiveUpdateTargetSelectionMenu(std::uint8_t* target);
// decomp: BATTLE.PRG func_800E5600 (src/BATTLE/BATTLE.PRG/7CD68.c:459); the
// first argument is unused, as in the original.
int LiveCalculateActionAreaReach(std::intptr_t unused, int action_id,
                                 std::uint32_t maximum_reach);
// decomp: BATTLE.PRG func_800E5698 (src/BATTLE/BATTLE.PRG/7CD68.c:476)
int LiveCalculateActionRangeSquared(Battle_Ai_ActionRangeView* actor_range_view,
                                    int action_id);
// func_800E5698 for typed callers, from the weapon range in the actor's stats.
int LiveCalculateActorActionRangeSquared(const Battle_ActorStats* stats,
                                         int action_id);
// decomp: BATTLE.PRG func_800C51B4 (src/BATTLE/BATTLE.PRG/5BF94.c:890)
void LiveUpdateTargetSelectionHighlight(int mode);
// decomp: BATTLE.PRG func_800E5568 (src/BATTLE/BATTLE.PRG/7CD68.c:437); a null
// range shape is refused and reads as 0.
int LiveCalculateRangeShapeExtent(
    const Battle_Ai_ActionRangeShape* range_shape);
}  // namespace VS::Battle
