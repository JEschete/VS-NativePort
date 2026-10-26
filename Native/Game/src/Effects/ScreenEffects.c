// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Effects/ScreenEffectState.h"
#include "Effects/ScreenEffects.h"
#include "OverlayWorkspace.h"
#include "Render/Textures.h"

// decomp: D_800EB9B4 (D_800EB9B4@BATTLE/BATTLE.PRG)
Effects_ScreenEffectOverlay* Effects_screenEffectOverlay; /* data: contents loaded in Step 5 */

// decomp: BATTLE.PRG func_800CA97C (src/BATTLE/BATTLE.PRG/5BF94.c:3477)
void Effects_ResetScreenEffectState(void)
{
    Effects_screenEffectOverlay = NULL;
    memset(&Effects_screenEffectDefaults, 0, sizeof Effects_screenEffectDefaults);
    Effects_screenEffectDefaults.radialWarpArg0 = 0x1000;
}

// decomp: BATTLE.PRG _loadScreff2 (src/BATTLE/BATTLE.PRG/5BF94.c:3873)
void Effects_LoadScreenEffectOverlay(int arg0)
{
    Content_DiscFile file;
    Effects_ScreenEffectOverlay* p = Effects_screenEffectOverlay;
    void* const* new_var;

    if (arg0 != 0) {
        p->loading = 1;
        file.lba = VS_SCREFF2_PRG_LBA;
        file.size = VS_SCREFF2_PRG_SIZE;
        p->slot = Content_AllocateDiscRequest(&file);
        new_var = &Core_overlaySlots[1];
        Content_EnqueueDiscRequest(p->slot, *new_var);
        return;
    }

    if (p->slot->state == 4) {
        Content_ReleaseDiscRequest(p->slot);
        p->loading = 0;
        Boot_WaitForOverlayReady();
    }
}

// decomp: BATTLE.PRG func_800CB708 (src/BATTLE/BATTLE.PRG/5BF94.c:3896)
void Effects_DrawScreenEffectOverlay(void)
{
    Effects_ScreenEffectOverlay* temp_s0 = Effects_screenEffectOverlay;
    if (temp_s0 != NULL) {
        if (temp_s0->loading != 0) {
            Effects_LoadScreenEffectOverlay(0);
            return;
        }
        if (temp_s0->radialWarpArg0 != 0x1000 || temp_s0->radialWarpArg1 != 0) {
            Effects_ScreenDistortion_WarpPolygonVertices();
        }
        if (temp_s0->feedbackLayer != 0) {
            Effects_ScreenDistortion_DrawFeedbackTiles();
        }
        if (temp_s0->vignetteEnabled != 0) {
            Effects_ScreenDistortion_DrawVignette();
        }
    }
}

// decomp: BATTLE.PRG func_800CB79C (src/BATTLE/BATTLE.PRG/5BF94.c:3916)
void Effects_AcquireScreenEffectOverlay(void)
{
    if (Effects_screenEffectOverlay == NULL) {
        Effects_screenEffectOverlay = &Effects_screenEffectDefaults;
        Content_AcquireOverlayWorkspace(6);
        Effects_LoadScreenEffectOverlay(1);
    }
}

// decomp: BATTLE.PRG func_800CB7DC (src/BATTLE/BATTLE.PRG/5BF94.c:3925)
void Effects_ReleaseScreenEffectOverlay(void)
{
    if (Effects_screenEffectOverlay != NULL) {
        Content_ReleaseOverlayWorkspace(6);
        if (Effects_screenEffectOverlay->opaque1 == 1) {
            Render_RetireTexturePageRange(0x1B, 5, 6);
        }
        Effects_ResetScreenEffectState();
    }
}

