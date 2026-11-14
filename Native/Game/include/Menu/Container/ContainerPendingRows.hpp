#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Items/ContainerIndexing.hpp"
#include "Menu/Container/ContainerCapacity.hpp"
#include "Menu/Framework/ItemRows.hpp"

namespace VS::Menu::container {
struct PendingTransferKey {
  std::int32_t category{};
  std::int32_t slot{};
};

class PendingTransferWorld {
 public:
  virtual ~PendingTransferWorld() = default;
  [[nodiscard]] virtual std::span<std::uint16_t> PendingTransfers() = 0;
  [[nodiscard]] virtual Items::ContainerWorld& StagingRecords() = 0;
  [[nodiscard]] virtual std::uint16_t Capacity(
      Items::ItemCategory category) = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

enum class ItemRowStorage : std::uint8_t { kActive, kStaged };

struct ContainerRowOutput {
  std::size_t row{};
  std::int32_t slot{};
  ItemRowStorage storage{};
  Framework::ItemRowText text{};
};

// Capturing an order does not capture its record owner. Active record reads
// reload the current owner; staged row formatting retains its entry owner.
class ContainerItemRowsWorld : public PendingTransferWorld {
 public:
  [[nodiscard]] virtual std::span<const std::uint16_t> CaptureOrder(
      Items::ItemCategory category) = 0;
  [[nodiscard]] virtual Items::ContainerWorld& ActiveRecords() = 0;
  [[nodiscard]] virtual Framework::ItemTextHost& TextHost() = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> Description(
      std::size_t row) = 0;
  virtual void StoreRow(const ContainerRowOutput& output) = 0;
  virtual void AddRowFlags(std::size_t row, std::uint32_t flags) = 0;
  virtual void ReportItemStatus(Items::ItemsStatus status) = 0;
};

// Comparisons retain the complete wrapped word; only an appended token is
// truncated to its stored halfword. Neither operation stops at a zero token.
[[nodiscard]] std::int32_t FindPendingTransfer(PendingTransferKey key,
                                               PendingTransferWorld& world);
void AppendPendingTransfer(PendingTransferKey key, PendingTransferWorld& world);
void RebuildPendingTransfers(PendingTransferWorld& world);
[[nodiscard]] std::int32_t BuildItemRows(Items::ItemCategory category,
                                         ContainerItemRowsWorld& world);
}  // namespace VS::Menu::container
