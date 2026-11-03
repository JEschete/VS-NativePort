#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Render/GimImages.hpp"

// Live binding for the event scripts' GIM images (GimImages.cpp): one set at
// a time, in a high-end heap block of the original's size, read through the
// disc queue, lent texture pages as owner 6 and drawn into the current
// frame's ordering tables. An unknown image, a layer past the third, a
// missing heap block, a missing ordering-table entry or a file the core
// refuses is counted and reported; the set's phases still advance, so the
// scripts waiting on it continue.
namespace VS::Render {
// BATTLE.PRG vs_battle_loadGim (5BF94.c:2347): queues the read; nothing
// happens while a set exists.
void QueueLiveGimImage(int image_id, int first_page);
// BATTLE.PRG func_800C86AC (5BF94.c:2422): frees a ready set.
void ReleaseLiveGimImages();
// BATTLE.PRG func_800C8778 (5BF94.c:2440): advances the load, then draws the
// enabled layers each frame.
void UpdateLiveGimImages();
// BATTLE.PRG func_800CA9C0 (5BF94.c:3484): the heap was reset under the set;
// it is forgotten without being freed.
void ForgetLiveGimImages();

[[nodiscard]] bool LiveGimImagesPresent();
// BATTLE.PRG func_800B6724 (4A0A8.c:609): 1 while the file loads. The
// original read through a missing set; 0 is returned and reported.
[[nodiscard]] int LiveGimImageLoading();
// BATTLE.PRG func_800B7014 (4A0A8.c:857): values truncate to the layer's
// fields as the original's stores did. Without a set nothing happens.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the script's order.
void ConfigureLiveGimLayer(int layer, int routine, int offset_x, int offset_y,
                           int scale_and_order, int brightness);
// The layer's placement, or nothing without a set or past the third layer.
[[nodiscard]] std::optional<GimPlacement> LiveGimLayerPlacement(int layer);
// BATTLE.PRG func_800BB9B0 (4A0A8.c:2971): the tweened placement, truncated
// as the original's stores did. Without a set nothing happens.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the field order.
void PlaceLiveGimLayer(int layer, int offset_x, int offset_y,
                       int scale_and_order, int brightness);

// The set, for tests and diagnostics; null when none exists.
[[nodiscard]] const GimImageSet* LiveGimImages();
// Requests refused since start-up.
[[nodiscard]] std::uint64_t RefusedLiveGimRequestCount();
}  // namespace VS::Render
