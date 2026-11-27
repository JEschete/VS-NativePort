#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Field/GimmickResources.hpp"

namespace VS::Content {
struct LegacyActorAssetRequest;
}  // namespace VS::Content

namespace VS::Field {
struct GimmickInstance;

std::optional<std::size_t> FindLiveSharedGimmickModel(
    std::int8_t excluded_slot, std::uint8_t requested_kind);
bool SetLiveGimmickPaletteTable(std::span<const std::byte> palette_bytes);
int ReleaseLiveGimmickInstance(std::int32_t slot);
int CreateLiveGimmickInstance(const Content::ActorAssetRequest& request,
                              GimmickInstance* destination);
void DiscardLiveGimmickInstance(std::int32_t slot, GimmickInstance* expected);
GimmickModelResourceRef LiveGimmickModelResource(std::int32_t slot);
void ResetLiveGimmickResources();

// The original BATTLE.PRG entries over the typed operations above.
int FindSharedGimmickModelEntry(Content::LegacyActorAssetRequest* request);
void SetGimmickPaletteTableEntry(const void* palette_data);
int ReleaseGimmickInstanceEntry(int slot);
int CreateGimmickInstanceEntry(Content::LegacyActorAssetRequest* request);
}  // namespace VS::Field
