#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>

#include <crtdbg.h>

#include "Core/Log.hpp"
#include "Items/ContainerGemCountsLive.hpp"
#include "Items/ContainerIndexing.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerCapacityLive.hpp"

extern "C" {
#include "Items/InventoryLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/FrameworkLiveState.h"
}

namespace {
namespace items = VS::Items;
namespace capacity = VS::Menu::container;
constexpr int kWeapon = static_cast<int>(items::ItemCategory::kWeapon);
constexpr int kGem = static_cast<int>(items::ItemCategory::kGem);
constexpr int kManage = static_cast<int>(items::ItemCategory::kManage);
constexpr int kLastWeapon = 31;
constexpr int kLastPart = 63;
constexpr int kLastGem = 191;
constexpr unsigned kInvalidGrip = 65;
constexpr unsigned kBeyondGem = 193;
constexpr unsigned kBeyondSlots = 5;
constexpr unsigned kOpaque = 0xFFFF;
constexpr int kBeyondPending = 64;

void Snapshot(const Menu_Container_InventoryStorage &storage, int result) {
  const auto &active = storage.container.items;
  const auto &staged = storage.stagedTransfer;
  std::cout << "STATE " << result << ' '
            << static_cast<unsigned>(staged.weapons[0].index) << ' '
            << static_cast<unsigned>(staged.weapons[0].blade) << ' '
            << static_cast<unsigned>(staged.weapons[0].grip) << ' '
            << static_cast<unsigned>(staged.blades[0].id) << ' '
            << static_cast<unsigned>(staged.grips[0].id) << ' '
            << static_cast<unsigned>(active.weapons[kLastWeapon].blade) << ' '
            << static_cast<unsigned>(active.grips[kLastPart].id) << ' '
            << static_cast<unsigned>(active.gems[kLastGem].id) << ' '
            << storage.itemsToTransfer[0] << '\n'
            << std::flush;
}

void Weapon(Items_ContainerData &records) {
  items::Live::Commit(std::span(records.weapons).back(),
                      items::InventoryWeapon{.blade = kLastPart + 1,
                                             .grip = kLastPart + 1,
                                             .gems = {kLastGem + 1, 0, 0, 0}});
  items::Live::Commit(std::span(records.blades).back(),
                      items::InventoryBlade{.id = 41});
  items::Live::Commit(
      std::span(records.grips).back(),
      items::InventoryGrip{.id = 300, .gem_slots = items::kWeaponGemCount});
  items::Live::Commit(std::span(records.gems).back(),
                      items::InventoryGem{.id = 511});
}

int Run(std::string_view mode, Menu_Container_InventoryStorage &storage) {
  if (mode == "weapon-owner") {
    Menu_Container_containerData = nullptr;
    return items::CountLiveContainerWeaponGems(kLastWeapon);
  }
  if (mode == "weapon-grip-zero" || mode == "weapon-grip-high") {
    storage.container.items.weapons[kLastWeapon].grip =
        mode == "weapon-grip-zero" ? 0 : kInvalidGrip;
    return items::CountLiveContainerWeaponGems(kLastWeapon);
  }
  if (mode == "shield-owner") {
    Menu_Container_containerData = nullptr;
    return items::CountLiveContainerShieldGems(kLastWeapon);
  }
  if (mode == "shield-slot-extent") {
    storage.container.items.shields[kLastWeapon].base.gemSlots = kBeyondSlots;
    return items::CountLiveContainerShieldGems(kLastWeapon);
  }
  if (mode == "receiving-owner") {
    Menu_Container_otherStorage = nullptr;
    return capacity::WouldExceedLiveCapacity(kGem, 1);
  }
  if (mode == "staging-owner") {
    Menu_Framework_unusedInventoryStorage = nullptr;
    return capacity::WouldExceedLiveCapacity(kGem, 1);
  }
  if (mode == "pending-owner") {
    Menu_Framework_unusedInventoryStorage = nullptr;
    return capacity::LiveTransferCapacityError(kGem, 0);
  }
  if (mode == "full-queue-mandatory-grip") {
    std::ranges::fill(storage.itemsToTransfer, kOpaque);
    return capacity::LiveTransferCapacityError(kWeapon, kLastWeapon);
  }
  if (mode == "undo-row") {
    capacity::UndoLivePendingTransfer(kBeyondPending);
    return 0;
  }
  if (mode == "undo-empty-token") {
    capacity::UndoLivePendingTransfer(0);
    return 0;
  }
  if (mode == "stage-grip-zero" || mode == "stage-gem-high") {
    Weapon(storage.container.items);
    if (mode == "stage-grip-zero") {
      storage.container.items.weapons[kLastWeapon].grip = 0;
    } else {
      storage.container.items.weapons[kLastWeapon].gems[0] = kBeyondGem;
    }
    capacity::StageLiveItemTransfer(kWeapon, kLastWeapon);
    return 0;
  }
  if (mode == "empty-shield") {
    return items::CountLiveContainerShieldGems(kLastWeapon);
  }
  if (mode == "lazy-source") {
    Menu_Container_containerData = nullptr;
    return capacity::LiveTransferCapacityError(kGem, -1);
  }
  if (mode == "full-queue-lazy") {
    std::ranges::fill(storage.itemsToTransfer, kOpaque);
    Menu_Container_containerData = nullptr;
    Menu_Container_otherStorage = nullptr;
    return capacity::LiveTransferCapacityError(kGem, -1);
  }
  if (mode == "zero-capacity-lazy") {
    std::fill_n(Menu_Container_containerItemCapacities,
                items::kItemCategoryCount, 0);
    Menu_Container_otherStorage = nullptr;
    Menu_Framework_unusedInventoryStorage = nullptr;
    Menu_Container_browsingGadgetBag = 1;
    return capacity::WouldExceedLiveCapacity(kGem, 0);
  }
  if (mode == "stage-manage-lazy") {
    Menu_Container_containerData = nullptr;
    Menu_Framework_unusedInventoryStorage = nullptr;
    capacity::StageLiveItemTransfer(kManage, -1);
    return 0;
  }
  if (mode == "undo-manage-lazy") {
    storage.itemsToTransfer[0] = 0x700;
    Menu_Container_containerData = nullptr;
    capacity::UndoLivePendingTransfer(0);
    return 0;
  }
  std::cerr << "unknown guard mode\n";
  std::exit(2);
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _set_error_mode(_OUT_TO_STDERR);
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  auto storage = std::make_unique<Menu_Container_InventoryStorage>();
  std::copy(items::kContainerCapacities.begin(),
            items::kContainerCapacities.end(),
            Menu_Container_containerItemCapacities);
  Menu_Framework_unusedInventoryStorage = storage.get();
  Menu_Container_containerData = &storage->container;
  Menu_Container_otherStorage = &storage->gadgetBag;
  Menu_Container_browsingGadgetBag = 0;
  if (!VS::Core::SetLogFile(argv[2]))
    return 2;
  VS::Core::SetLogObserver([&](const VS::Core::LogRecord &record) {
    if (record.level == VS::Core::LogLevel::Error)
      Snapshot(*storage, -1);
  });
  const auto result = Run(argv[1], *storage);
  Snapshot(*storage, result);
  VS::Core::SetLogObserver({});
  VS::Core::SetLogFile("");
}
