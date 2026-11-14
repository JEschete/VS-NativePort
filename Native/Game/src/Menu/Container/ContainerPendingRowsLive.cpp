#include "Menu/Container/ContainerPendingRowsLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerPendingRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/FrameworkLiveState.h"
}

namespace VS::Menu::container {
namespace {
constexpr auto kMaximumRows = Items::ContainerContents{}.misc.size();
constexpr std::size_t kTextsPerRow = 2;
constexpr auto kRowBytes = Framework::kRowDescriptionBufferBytes;
constexpr auto kDescriptionsBytes = kMaximumRows * kRowBytes;

[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("menu.container", "Container pending/item rows refused: %.*s",
              static_cast<int>(reason.size()), reason.data());
  std::abort();
}

Items::ItemCategory Category(int category) {
  if (category < 0 ||
      static_cast<std::size_t>(category) >= Items::kItemCategoryCount) {
    Refuse("item row category outside container categories");
  }
  return static_cast<Items::ItemCategory>(category);
}

class LiveWorld final : public ContainerItemRowsWorld {
 public:
  explicit LiveWorld(char** text = nullptr, int* types = nullptr,
                     char* descriptions = nullptr)
      : text_(text), types_(types), descriptions_(descriptions) {}

  std::span<std::uint16_t> PendingTransfers() override {
    auto* const owner = Menu_Framework_unusedInventoryStorage;
    if (owner == nullptr) Refuse("missing pending-transfer owner");
    return owner->itemsToTransfer;
  }
  Items::ContainerWorld& StagingRecords() override {
    auto* const owner = Menu_Framework_unusedInventoryStorage;
    staged_owner_ = owner == nullptr ? nullptr : &owner->stagedTransfer;
    return staged_.emplace(staged_owner_);
  }
  std::uint16_t Capacity(Items::ItemCategory category) override {
    return Menu_Container_containerItemCapacities[static_cast<std::size_t>(
        category)];
  }
  std::span<const std::uint16_t> CaptureOrder(
      Items::ItemCategory category) override {
    return Items::LiveContainerCategoryOrder(category,
                                             Menu_Container_containerData);
  }
  Items::ContainerWorld& ActiveRecords() override {
    auto* const owner = Menu_Container_containerData;
    active_owner_ = owner == nullptr ? nullptr : &owner->items;
    return active_.emplace(active_owner_);
  }
  Framework::ItemTextHost& TextHost() override { return host_; }
  std::span<std::uint8_t> Description(std::size_t row) override {
    if (row >= kMaximumRows) Refuse("item description outside row workspace");
    if (descriptions_ == nullptr) return {};
    return std::span(reinterpret_cast<std::uint8_t*>(descriptions_),
                     kDescriptionsBytes)
        .subspan(row * Framework::kRowDescriptionBufferBytes,
                 Framework::kRowDescriptionBufferBytes);
  }
  void StoreRow(const ContainerRowOutput& output) override {
    if (text_ == nullptr) Refuse("missing item row text workspace");
    if (output.row >= kMaximumRows) Refuse("item text outside row workspace");
    char* name = nullptr;
    if (output.text.name &&
        output.text.name->source == Framework::TextSource::kWeaponName) {
      auto* const owner = output.storage == ItemRowStorage::kStaged
                              ? staged_owner_
                              : active_owner_;
      if (owner == nullptr) Refuse("missing captured weapon-name owner");
      if (output.slot < 0 ||
          static_cast<std::size_t>(output.slot) >= std::size(owner->weapons)) {
        Refuse("weapon name outside captured item records");
      }
      name = owner->weapons[output.slot].name;
    }
    const auto buffer = Description(output.row);
    auto* const bytes = buffer.empty() ? nullptr : buffer.data();
    auto* const target = text_ + output.row * kTextsPerRow;
    if (output.text.name) {
      target[0] =
          Framework::ResolveLiveItemRowText(*output.text.name, bytes, name);
    }
    if (output.text.description) {
      target[1] = Framework::ResolveLiveItemRowText(*output.text.description,
                                                    bytes, name);
    }
    RowType(output.row) = static_cast<int>(output.text.row_type);
  }
  void AddRowFlags(std::size_t row, std::uint32_t flags) override {
    auto& type = RowType(row);
    type = static_cast<int>(static_cast<std::uint32_t>(type) | flags);
  }
  void ReportItemStatus(Items::ItemsStatus status) override {
    Items::Live::WarnRefusal("BuildContainerWeaponRow", status);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    container::Refuse(reason);
  }

 private:
  int& RowType(std::size_t row) {
    if (types_ == nullptr) Refuse("missing item row type workspace");
    if (row >= kMaximumRows) Refuse("item type outside row workspace");
    return types_[row];
  }

  char** const text_;
  int* const types_;
  char* const descriptions_;
  Items_ContainerData* active_owner_ = nullptr;
  Items_ContainerData* staged_owner_ = nullptr;
  std::optional<Items::LiveContainerWorld> active_;
  std::optional<Items::LiveContainerWorld> staged_;
  Framework::LiveItemRowsHost host_;
};
}  // namespace

// decomp: func_801069B0 (src/MENU/MENUD.PRG/234.c:2061), 801069B0.
int FindLivePendingTransfer(int category, int slot) {
  LiveWorld world;
  return FindPendingTransfer({category, slot}, world);
}

// decomp: func_80106A04 (src/MENU/MENUD.PRG/234.c:2073), 80106A04.
void AppendLivePendingTransfer(int category, int slot) {
  LiveWorld world;
  AppendPendingTransfer({category, slot}, world);
}

// decomp: func_80106A50 (src/MENU/MENUD.PRG/234.c:2088), 80106A50.
void RebuildLivePendingTransfers() {
  LiveWorld world;
  RebuildPendingTransfers(world);
}

// decomp: func_80106C64 (src/MENU/MENUD.PRG/234.c:2145), 80106C64.
int BuildLiveItemRows(int category, char** text, int* row_types,
                      char* descriptions) {
  LiveWorld world(text, row_types, descriptions);
  return BuildItemRows(Category(category), world);
}
}  // namespace VS::Menu::container
