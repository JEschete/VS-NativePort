#pragma once
// Hand-maintained (tools/port/owned.csv).
// Whether a dialog, object or menu has paused the field, as the copied
// battle rewards and the runtime snapshot read and write it. GameModeLive.cpp
// defines it.

namespace VS::Field {

// decomp: D_800F1968 (BATTLE/BATTLE.PRG)
extern int isInteractionPaused;

}  // namespace VS::Field
