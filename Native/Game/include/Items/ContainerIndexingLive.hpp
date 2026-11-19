#pragma once

#include <cstdint>

#include "Items/ContainerIndexing.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Items {
// A non-owning checked record view; lifetime belongs to the calling system.
class LiveContainerWorld final : public ContainerWorld {
 public:
  explicit LiveContainerWorld(VS::Items::ContainerData* owner = nullptr);
  std::uint16_t Capacity(ItemCategory category) override;
  ContainerRecord Read(ItemCategory category, std::size_t index) override;
  void Write(ItemCategory category, std::size_t index,
             const ContainerRecord& item) override;
  [[noreturn]] void Refuse(std::string_view reason) override;

 private:
  VS::Items::ContainerData* const owner_;
};

[[nodiscard]] std::uint16_t* LiveContainerCategoryIndices(
    int category, VS::Items::Container* container);
[[nodiscard]] int FindLiveContainerItemRow(int category, int index,
                                           std::uint16_t* indices);
void AppendLiveContainerItemIndex(int category, int index,
                                  std::uint16_t* indices);
void ClearLiveContainerItem(int category, int index,
                            VS::Items::ContainerData* container);
[[nodiscard]] int LiveContainerItemId(int category, int index,
                                      VS::Items::ContainerData* container);
[[nodiscard]] int CountLiveContainerItems(int category,
                                          VS::Items::ContainerData* container);
[[nodiscard]] int FindLiveEmptyContainerSlot(
    int category, VS::Items::ContainerData* container);
}  // namespace VS::Items
