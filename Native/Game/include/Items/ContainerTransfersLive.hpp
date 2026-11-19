#pragma once

#include <span>

#include "Items/ItemRecords.hpp"

namespace VS::Items {
// The original offset resolver, bounded to its actual array subobject.
[[nodiscard]] std::span<std::uint16_t> LiveContainerCategoryOrder(
    ItemCategory category, VS::Items::Container* container);
void PreviewLiveTransferredMisc();
void RefreshLiveMiscTransferCount();
[[nodiscard]] int CopyLiveContainerItem(int category_flags,
                                        VS::Items::ContainerData* target,
                                        int source_slot,
                                        VS::Items::ContainerData* source);
void RebuildLiveContainerCategoryIndices(int category,
                                         VS::Items::Container* container);
void SnapshotLiveGadgetBag();
void CommitLiveGadgetBag();
[[nodiscard]] int LiveContainerParentWeaponIndex(
    int category, int index, VS::Items::ContainerData* container);
[[nodiscard]] int LiveContainerParentShieldIndex(
    int category, int index, VS::Items::ContainerData* container);
}  // namespace VS::Items
