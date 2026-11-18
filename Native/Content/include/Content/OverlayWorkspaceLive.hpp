#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the overlay workspace rules (OverlayWorkspace.hpp) over the
// legacy owner byte overlayWorkspaceOwner. Generated callers keep the original
// int owner tag and 1/0 result.
namespace VS::Content {

// decomp: D_800F18F8 (BATTLE/BATTLE.PRG 146C.c): the claiming owner tag, 0
// when free. OverlayWorkspaceLive.cpp defines it; the game loop's page
// initialization still clears it directly.
extern char overlayWorkspaceOwner;

int AcquireLiveOverlayWorkspace(int owner_tag);
int ReleaseLiveOverlayWorkspace(int owner_tag);

}  // namespace VS::Content
