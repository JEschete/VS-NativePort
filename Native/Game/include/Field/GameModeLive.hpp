#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/GameMode.hpp"

struct Menu_Framework_ActionSelection;

// Binding from the field and battle mode globals, Ashley and the battle
// action menu's selection (Menu_Framework_ActionSelection) to the typed
// field mode rules (GameMode.hpp). Each adapter keeps the original call shape
// for the generated callers (copy_fixups.py OWNED_CALL_BINDINGS).
namespace VS::Field {
// The selection as a typed menu result.
[[nodiscard]] ActionMenuResult ActionMenuResultOf(
    const Menu_Framework_ActionSelection& selection);

// decomp: BATTLE.PRG func_8006C2FC (src/BATTLE/BATTLE.PRG/146C.c:1866)
void LivePauseFieldForInteractionDialog();
// decomp: BATTLE.PRG func_8006C350 (src/BATTLE/BATTLE.PRG/146C.c:1880)
void LivePauseFieldForObjectInteraction();
// decomp: BATTLE.PRG func_8006C39C (src/BATTLE/BATTLE.PRG/146C.c:1893)
void LivePauseFieldForMenu();
// decomp: BATTLE.PRG func_8006C40C (src/BATTLE/BATTLE.PRG/146C.c:1907)
void LiveResumeFieldAfterMenu();
// decomp: BATTLE.PRG func_8006F5CC (src/BATTLE/BATTLE.PRG/146C.c:3221)
void LiveEnterFieldExplorationMode();
// decomp: BATTLE.PRG func_8007352C (src/BATTLE/BATTLE.PRG/146C.c:4659)
void LiveBeginShortcutMenu();
// decomp: BATTLE.PRG func_80073554 (src/BATTLE/BATTLE.PRG/146C.c:4665)
void LiveEndShortcutMenu();
// decomp: BATTLE.PRG func_8007357C (src/BATTLE/BATTLE.PRG/146C.c:4671)
// The original's two arguments are never read.
void LiveBeginBattleActionMenu(int /*unused*/, int /*unused*/);
// decomp: BATTLE.PRG func_800735CC (src/BATTLE/BATTLE.PRG/146C.c:4679)
// A null selection still resumes the field; its status is left alone.
void LiveCloseBattleActionMenu(Menu_Framework_ActionSelection* selection);
// decomp: BATTLE.PRG func_800735F8 (src/BATTLE/BATTLE.PRG/146C.c:4686)
// A null selection is refused with a warning and changes nothing.
void LiveDispatchBattleActionMenuResult(
    Menu_Framework_ActionSelection* selection);
}  // namespace VS::Field
