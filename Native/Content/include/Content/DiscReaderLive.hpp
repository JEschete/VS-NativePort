#pragma once

#include <cstdint>

#include "Content/AssetHandle.hpp"

namespace VS::Content {

// The copied request queue and boot code drive the shared drive record
// through these. The destination stays borrowed until the next command update
// serves the read; its owner keeps it alive until the request is loaded.
[[nodiscard]] bool BeginLiveDiscRead(std::int32_t sector, std::uint32_t bytes,
                                     void* destination);
// The same for a catalog asset (#47): the command update reads `bytes` of it
// through DataStore::ReadAsset, failing the read as a drive error when the
// handle no longer resolves or `bytes` exceeds its whole-sector load.
[[nodiscard]] bool BeginLiveAssetRead(AssetHandle asset, std::uint32_t bytes,
                                      void* destination);
[[nodiscard]] std::uint64_t LiveDiscReadGeneration() noexcept;
[[nodiscard]] bool LiveDiscReadCompleted(std::uint64_t generation) noexcept;
// Low-level command cancellation; queue owners use CancelLiveDiscWrites.
void CancelLiveDiscRead(const void* destination);
void UpdateLiveDiscCommands();
void ResetLiveDiscDrive();

}  // namespace VS::Content
