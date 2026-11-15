#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Legacy/Psx.h"

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
                              int* packet_words, u_long* ordering_head);
// func_800F9FB8: the vignette in front of the frame's overlay table.
void DrawLiveVignette();
}  // namespace VS::Effects::ScreenDistortion
