#pragma once

#include <span>

#include "Items/ItemRecords.hpp"

struct Items_Container;
struct Items_ContainerData;

namespace VS::Items {
// The original offset resolver, bounded to its actual array subobject.
[[nodiscard]] std::span<std::uint16_t> LiveContainerCategoryOrder(
    ItemCategory category, Items_Container* container);
void PreviewLiveTransferredMisc();
void RefreshLiveMiscTransferCount();
[[nodiscard]] int CopyLiveContainerItem(int category_flags,
                                        Items_ContainerData* target,
                                        int source_slot,
                                        Items_ContainerData* source);
void RebuildLiveContainerCategoryIndices(int category,
                                         Items_Container* container);
void SnapshotLiveGadgetBag();
void CommitLiveGadgetBag();
[[nodiscard]] int LiveContainerParentWeaponIndex(
    int category, int index, Items_ContainerData* container);
[[nodiscard]] int LiveContainerParentShieldIndex(
    int category, int index, Items_ContainerData* container);
}  // namespace VS::Items
