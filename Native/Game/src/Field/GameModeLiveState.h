#pragma once
// Hand-maintained (tools/port/owned.csv).
// Whether a dialog, object or menu has paused the field, as the copied
// battle rewards and the runtime snapshot read and write it. GameModeLive.cpp
// defines it.

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F1968 (BATTLE/BATTLE.PRG)
extern int Field_isInteractionPaused;

#ifdef __cplusplus
}
#endif
