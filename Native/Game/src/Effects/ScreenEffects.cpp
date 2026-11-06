// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Effects/ScreenDistortion/RadialWarpLive.hpp"
#include "Render/TexturePagesLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Content/DiscLayout.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Effects/ScreenEffectState.h"
#include "Effects/ScreenEffects.h"

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
void Effects_LoadScreenEffectOverlay(int begin_load)
{
    Content_DiscFile file;
    Effects_ScreenEffectOverlay* p = Effects_screenEffectOverlay;
    void* const* overlay_destination_slot;

    if (begin_load != 0) {
        p->loading = 1;
        file.lba = VS_SCREFF2_PRG_LBA;
        file.size = VS_SCREFF2_PRG_SIZE;
        p->slot = VS::Content::AllocateLiveDiscRequest(&file);
        overlay_destination_slot = &Core_overlaySlots[1];
        if (p->slot != NULL) {
            VS::Content::EnqueueLiveDiscRequest(p->slot, *overlay_destination_slot);
        } else {
            p->slot = VS::Content::CompleteLiveRefusedDiscRequest(&file, *overlay_destination_slot);
        }
        return;
    }

    if (p->slot->state == 4) {
        VS::Content::ReleaseLiveDiscRequest(p->slot);
        p->loading = 0;
        VS::Boot::WaitForOverlayReadyLive();
    }
}

// decomp: BATTLE.PRG func_800CB708 (src/BATTLE/BATTLE.PRG/5BF94.c:3896)
void Effects_DrawScreenEffectOverlay(void)
{
    Effects_ScreenEffectOverlay* screen_effect_state = Effects_screenEffectOverlay;
    if (screen_effect_state != NULL) {
        if (screen_effect_state->loading != 0) {
            Effects_LoadScreenEffectOverlay(0);
            return;
        }
        if (screen_effect_state->radialWarpArg0 != 0x1000 || screen_effect_state->radialWarpArg1 != 0) {
            VS::Effects::ScreenDistortion::WarpLivePolygonVertices();
        }
        if (screen_effect_state->feedbackLayer != 0) {
            Effects_ScreenDistortion_DrawFeedbackTiles();
        }
        if (screen_effect_state->vignetteEnabled != 0) {
            Effects_ScreenDistortion_DrawVignette();
        }
    }
}

// decomp: BATTLE.PRG func_800CB79C (src/BATTLE/BATTLE.PRG/5BF94.c:3916)
void Effects_AcquireScreenEffectOverlay(void)
{
    if (Effects_screenEffectOverlay == NULL) {
        Effects_screenEffectOverlay = &Effects_screenEffectDefaults;
        VS::Content::AcquireLiveOverlayWorkspace(6);
        Effects_LoadScreenEffectOverlay(1);
    }
}

// decomp: BATTLE.PRG func_800CB7DC (src/BATTLE/BATTLE.PRG/5BF94.c:3925)
void Effects_ReleaseScreenEffectOverlay(void)
{
    if (Effects_screenEffectOverlay != NULL) {
        VS::Content::ReleaseLiveOverlayWorkspace(6);
        if (Effects_screenEffectOverlay->opaque1 == 1) {
            VS::Render::RetireLiveTexturePageRange(0x1B, 5, 6);
        }
        Effects_ResetScreenEffectState();
    }
}

}  // extern "C"
