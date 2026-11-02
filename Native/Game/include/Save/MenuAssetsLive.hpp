#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Save/MenuAssets.hpp"

// Binds the memory-card asset load to the disc queue, low-end heap, texture
// pages and the title and Data-menu text tables. Both screens share the one
// SPMCIMG.BIN buffer, so they share one load state.
namespace VS::Save::MemoryCard {
// `init` nonzero begins the load; afterwards returns 1 once the assets are
// loaded.
int StepLiveMenuResources(int init);
void ReleaseLiveMenuResources();
int StepLiveTitleResources(int init);
void ReleaseLiveTitleResources();
[[nodiscard]] MenuAssetPhase LiveMenuAssetPhase();
}  // namespace VS::Save::MemoryCard
