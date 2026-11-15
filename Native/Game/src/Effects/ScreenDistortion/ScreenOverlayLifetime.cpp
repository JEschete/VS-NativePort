// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScreenDistortion/ScreenOverlayLifetime.hpp"

#include <cstdint>

namespace VS::Effects::ScreenDistortion {
namespace {
constexpr std::uint8_t kPagesLent = 1;
}  // namespace

// decomp: BATTLE.PRG func_800CA97C (5BF94.c:3477).
void ResetScreenOverlay(ScreenOverlayLifetime& lifetime) noexcept {
  lifetime = {};
  lifetime.controls.radial_warp.scale = kNeutralRadialWarpScale;
}

// decomp: BATTLE.PRG _loadScreff2 (5BF94.c:3873).
void LoadScreenOverlay(ScreenOverlayLifetime& lifetime, std::int32_t begin_load,
                       ScreenOverlayLoader& loader) {
  if (begin_load != 0) {
    lifetime.loading = 1;
    lifetime.read = loader.AllocateOverlayRead();
    if (lifetime.read != nullptr) {
      loader.EnqueueOverlayRead(lifetime.read);
    } else {
      lifetime.read = loader.CompleteRefusedOverlayRead();
    }
    return;
  }
  if (loader.OverlayReadLoaded(lifetime.read)) {
    loader.ReleaseOverlayRead(lifetime.read);
    lifetime.loading = 0;
    loader.WaitForOverlayReady();
  }
}

// decomp: BATTLE.PRG func_800CB708 (5BF94.c:3896).
void DrawScreenOverlay(ScreenOverlayLifetime& lifetime,
                       ScreenOverlayLoader& loader,
                       ScreenOverlayPainter& painter) {
  if (!lifetime.attached) {
    return;
  }
  if (lifetime.loading != 0) {
    LoadScreenOverlay(lifetime, 0, loader);
    return;
  }
  const ScreenOverlayControls& controls = lifetime.controls;
  if (controls.radial_warp.scale != kNeutralRadialWarpScale ||
      controls.radial_warp.growth != 0) {
    painter.WarpPolygons(controls.radial_warp);
  }
  if (controls.feedback_layer != 0) {
    painter.DrawFeedbackTiles(controls.feedback_layer);
  }
  if (controls.vignette_enabled != 0) {
    painter.DrawVignette();
  }
}

// decomp: BATTLE.PRG func_800CB79C (5BF94.c:3916).
void AcquireScreenOverlay(ScreenOverlayLifetime& lifetime,
                          ScreenOverlayLoader& loader) {
  if (lifetime.attached) {
    return;
  }
  lifetime.attached = true;
  loader.AcquireWorkspace(kScreenOverlayWorkspaceOwner);
  LoadScreenOverlay(lifetime, 1, loader);
}

// decomp: BATTLE.PRG func_800CB7DC (5BF94.c:3925).
void ReleaseScreenOverlay(ScreenOverlayLifetime& lifetime,
                          ScreenOverlayLoader& loader,
                          FeedbackTexturePages& pages) {
  if (!lifetime.attached) {
    return;
  }
  loader.ReleaseWorkspace(kScreenOverlayWorkspaceOwner);
  if (lifetime.controls.feedback_pages_reserved == kPagesLent) {
    pages.Retire(kFeedbackTexturePages);
  }
  ResetScreenOverlay(lifetime);
}
}  // namespace VS::Effects::ScreenDistortion
