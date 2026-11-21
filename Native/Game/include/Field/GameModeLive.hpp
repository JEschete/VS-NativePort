#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/GameMode.hpp"

namespace VS::Menu::Framework {
struct ActionSelection;
}  // namespace VS::Menu::Framework

// Binding from the field and battle mode globals, Ashley and the battle
// action menu's selection (Menu::Framework::ActionSelection) to the typed
// field mode rules (GameMode.hpp). Each adapter keeps the original call shape
// for the generated callers (copy_fixups.py OWNED_CALL_BINDINGS).
namespace VS::Field {
// The selection as a typed menu result.
[[nodiscard]] ActionMenuResult ActionMenuResultOf(
    const Menu::Framework::ActionSelection& selection);

// decomp: BATTLE.PRG func_8006C2FC (src/BATTLE/BATTLE.PRG/146C.c:1508)
void LivePauseFieldForInteractionDialog();
// decomp: BATTLE.PRG func_8006C350 (src/BATTLE/BATTLE.PRG/146C.c:1522)
void LivePauseFieldForObjectInteraction();
// decomp: BATTLE.PRG func_8006C39C (src/BATTLE/BATTLE.PRG/146C.c:1535)
void LivePauseFieldForMenu();
// decomp: BATTLE.PRG func_8006C40C (src/BATTLE/BATTLE.PRG/146C.c:1549)
void LiveResumeFieldAfterMenu();
// decomp: BATTLE.PRG func_8006F5CC (src/BATTLE/BATTLE.PRG/146C.c:2924)
void LiveEnterFieldExplorationMode();
// decomp: BATTLE.PRG func_8007352C (src/BATTLE/BATTLE.PRG/146C.c:4386)
void LiveBeginShortcutMenu();
// decomp: BATTLE.PRG func_80073554 (src/BATTLE/BATTLE.PRG/146C.c:4392)
void LiveEndShortcutMenu();
// decomp: BATTLE.PRG func_8007357C (src/BATTLE/BATTLE.PRG/146C.c:4398)
// The original's two arguments are never read.
void LiveBeginBattleActionMenu(int /*unused*/, int /*unused*/);
// decomp: BATTLE.PRG func_800735CC (src/BATTLE/BATTLE.PRG/146C.c:4406)
// A null selection still resumes the field; its status is left alone.
void LiveCloseBattleActionMenu(Menu::Framework::ActionSelection* selection);
// decomp: BATTLE.PRG func_800735F8 (src/BATTLE/BATTLE.PRG/146C.c:4413)
// A null selection is refused with a warning and changes nothing.
void LiveDispatchBattleActionMenuResult(
    Menu::Framework::ActionSelection* selection);
}  // namespace VS::Field
