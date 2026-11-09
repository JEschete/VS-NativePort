#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Field/GimmickResources.hpp"

namespace VS::Field {
std::optional<std::size_t> FindLiveSharedGimmickModel(
    std::int8_t excluded_slot, std::uint8_t requested_kind);
bool SetLiveGimmickPaletteTable(std::span<const std::byte> palette_bytes);
int ReleaseLiveGimmickInstance(std::int32_t slot);
int CreateLiveGimmickInstance(Content::ActorAssetRequest& request);
}  // namespace VS::Field
