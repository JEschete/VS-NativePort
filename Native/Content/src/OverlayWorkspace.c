// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Runtime.h"
#include "OverlayWorkspace.h"

// decomp: D_800F18F8 (D_800F18F8@BATTLE/BATTLE.PRG)
char Content_overlayWorkspaceOwner;

// decomp: BATTLE.PRG func_8007E180 (src/BATTLE/BATTLE.PRG/146C.c:9562)
int Content_AcquireOverlayWorkspace(int arg0)
{
    if (Content_overlayWorkspaceOwner == 0) {
        Content_overlayWorkspaceOwner = arg0;
        return 1;
    }
    Core_IgnoreDiagnostic(0x66, 0);
    return 0;
}

// decomp: BATTLE.PRG func_8007E1C0 (src/BATTLE/BATTLE.PRG/146C.c:9572)
int Content_ReleaseOverlayWorkspace(int arg0)
{
    if (Content_overlayWorkspaceOwner == arg0) {
        Content_overlayWorkspaceOwner = 0;
        return 1;
    }
    Core_IgnoreDiagnostic(0x67, 0);
    return 0;
}

