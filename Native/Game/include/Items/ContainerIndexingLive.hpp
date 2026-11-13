#pragma once

#include <cstdint>

struct Items_Container;
struct Items_ContainerData;

namespace VS::Items {
[[nodiscard]] std::uint16_t* LiveContainerCategoryIndices(
    int category, Items_Container* container);
[[nodiscard]] int FindLiveContainerItemRow(int category, int index,
                                           std::uint16_t* indices);
void AppendLiveContainerItemIndex(int category, int index,
                                  std::uint16_t* indices);
void ClearLiveContainerItem(int category, int index,
                            Items_ContainerData* container);
[[nodiscard]] int LiveContainerItemId(int category, int index,
                                      Items_ContainerData* container);
[[nodiscard]] int CountLiveContainerItems(int category,
                                          Items_ContainerData* container);
[[nodiscard]] int FindLiveEmptyContainerSlot(int category,
                                             Items_ContainerData* container);
}  // namespace VS::Items
