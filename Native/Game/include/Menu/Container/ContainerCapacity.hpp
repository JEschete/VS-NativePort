#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Items/ContainerIndexing.hpp"
#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::container {
inline constexpr std::size_t kPendingTransferEntries = 64;

enum class StorageArea : std::uint8_t { kGadgetBag, kContainer };

struct CapacityReadout {
  std::int32_t limit{};
  std::uint32_t position{};
  std::int32_t occupied{};
};

class CapacityRenderer {
 public:
  CapacityRenderer() = default;
  CapacityRenderer(const CapacityRenderer&) = delete;
  CapacityRenderer& operator=(const CapacityRenderer&) = delete;
  CapacityRenderer(CapacityRenderer&&) = delete;
  CapacityRenderer& operator=(CapacityRenderer&&) = delete;
  virtual ~CapacityRenderer() = default;
  virtual void DrawNumber(const Framework::NumberText& number) = 0;
  virtual void DrawLabel(std::string_view text, std::uint32_t position,
                         std::uint32_t color) = 0;
};

class StorageCapacityWorld : public CapacityRenderer {
 public:
  [[nodiscard]] virtual std::int32_t Occupied(StorageArea area,
                                              Items::ItemCategory category) = 0;
  [[nodiscard]] virtual std::uint16_t Limit(StorageArea area,
                                            Items::ItemCategory category) = 0;
};

struct TransferItem {
  Items::ItemCategory category{};
  std::int32_t slot{};
};

enum class TransferError : std::uint8_t {
  kFits,
  kPendingListFull,
  kDestinationFull,
};

class CapacityTransferWorld {
 public:
  CapacityTransferWorld() = default;
  CapacityTransferWorld(const CapacityTransferWorld&) = delete;
  CapacityTransferWorld& operator=(const CapacityTransferWorld&) = delete;
  CapacityTransferWorld(CapacityTransferWorld&&) = delete;
  CapacityTransferWorld& operator=(CapacityTransferWorld&&) = delete;
  virtual ~CapacityTransferWorld() = default;
  [[nodiscard]] virtual Items::ContainerWorld& Active() = 0;
  [[nodiscard]] virtual Items::ContainerWorld& Receiving() = 0;
  [[nodiscard]] virtual Items::ContainerWorld& Staging() = 0;
  [[nodiscard]] virtual std::span<const std::uint16_t> PendingTransfers() = 0;
  [[nodiscard]] virtual std::uint16_t ReceivingLimit(
      Items::ItemCategory category) = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

void DrawCapacityCount(CapacityRenderer& renderer,
                       const CapacityReadout& readout);
void DrawStorageCapacities(StorageCapacityWorld& world);
[[nodiscard]] bool WouldExceedCapacity(Items::ItemCategory category,
                                       std::int32_t additional,
                                       CapacityTransferWorld& world);
[[nodiscard]] TransferError TransferCapacityError(TransferItem item,
                                                  CapacityTransferWorld& world);
void UndoPendingTransfer(std::int32_t row, CapacityTransferWorld& world);
void StageItemTransfer(TransferItem item, CapacityTransferWorld& world);
}  // namespace VS::Menu::container
