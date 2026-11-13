#pragma once

#include <cstdint>

#include "Items/ContainerIndexing.hpp"

struct Items_Container;
struct Items_ContainerData;

namespace VS::Items {
// A non-owning checked record view; lifetime belongs to the calling system.
class LiveContainerWorld final : public ContainerWorld {
 public:
  explicit LiveContainerWorld(Items_ContainerData* owner = nullptr);
  std::uint16_t Capacity(ItemCategory category) override;
  ContainerRecord Read(ItemCategory category, std::size_t index) override;
  void Write(ItemCategory category, std::size_t index,
             const ContainerRecord& item) override;
  [[noreturn]] void Refuse(std::string_view reason) override;

 private:
  Items_ContainerData* const owner_;
};

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
