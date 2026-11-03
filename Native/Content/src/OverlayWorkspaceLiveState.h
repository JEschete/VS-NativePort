#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy overlay workspace owner byte. OverlayWorkspaceLive.cpp defines it
// and binds it to the typed rules (OverlayWorkspace.hpp); the game loop's
// page initialization still clears it directly.

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F18F8 (BATTLE/BATTLE.PRG 146C.c): the claiming owner tag, 0
// when free.
extern char Content_overlayWorkspaceOwner;

#ifdef __cplusplus
}
#endif
