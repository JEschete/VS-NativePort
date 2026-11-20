#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Legacy/Psx.hpp"

// Binding from the active screen effect overlay controller, the frame's
// ordering tables and primitive buffer, and the lendable texture pages to the
// typed screen overlays (ScreenOverlays.cpp). The setters refuse and report a
// missing controller; packets that do not fit or have no chain are dropped
// and counted by the render packet queue.
namespace VS::Effects::ScreenDistortion {
// func_800F9800.
void SetLiveFeedbackLayer(int ordering_layer);
// func_800F986C: the feedback tiles of the controller's feedback layer.
void DrawLiveFeedbackTiles();
// func_800F9BC0: the typed radial warp setter for event camera commands.
void SetLiveRadialWarp(RadialWarpSettings settings);
// func_800F9BC0 behind word arguments; keeps the low halfword of each, as the
// original's stores did for its word-passing callers.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveRadialWarpWords(int scale_word, int growth_word);
// func_800F9DE8.
void SetLiveVignetteEnabled(int enabled);
// func_800F9EBC behind the original call shape. `packet_words` must be the
// frame's primitive cursor; anything else is refused and returned unchanged.
// Returns the cursor after the queued triangles.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int* AppendLiveVignetteCorner(int packed_corner_xy, int first_angular_step,
                              int* packet_words, std::uint32_t* ordering_head);
// func_800F9FB8: the vignette in front of the frame's overlay table.
void DrawLiveVignette();

// The controller lifetime (ScreenOverlayLifetime.cpp) over D_800EB9B4 and its
// default controller (VS::Effects::screenEffectDefaults), the disc queue,
// overlay workspace and texture pages. Attaching points D_800EB9B4 at the
// defaults; detaching resets the defaults. func_800CA97C.
void ResetLiveScreenOverlay();
// _loadScreff2. Refuses and reports a missing controller; a missing read
// request is reported and stays pending.
void LoadLiveScreenOverlay(int begin_load);
// func_800CB708.
void DrawLiveScreenOverlay();
// func_800CB79C.
void AcquireLiveScreenOverlay();
// func_800CB7DC.
void ReleaseLiveScreenOverlay();
}  // namespace VS::Effects::ScreenDistortion
