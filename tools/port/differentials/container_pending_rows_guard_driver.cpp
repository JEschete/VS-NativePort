#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <string_view>

#include "Menu/ContainerPendingRowsLiveFixture.hpp"

namespace {
namespace tests = container_pending_rows_live_tests;
namespace data = container_pending_rows_tests;
namespace rows = VS::Menu::container;
constexpr int kMisc = 6;
constexpr int kPending = 7;

int Probe(std::string_view mode) {
  tests::PendingRowsLiveFixture fixture;
  if (mode == "old-caller-overrun") {
    auto &owner = fixture.containers.storage->container;
    for (std::size_t slot = 0; slot < data::kRowCount; ++slot) {
      owner.items.misc[slot].id = data::kMiscId;
      owner.indices.misc[slot] = static_cast<std::uint16_t>(slot + 1);
    }
    const auto count = rows::BuildLiveItemRows(
        kMisc, fixture.workspace.texts.data(), fixture.workspace.types.data(),
        fixture.workspace.descriptions.data());
    const auto first_outside =
        (tests::kOldAllocationBytes - tests::kOldTextOffset) / sizeof(char *);
    if (count != data::kRowCount ||
        fixture.workspace.texts[first_outside] == nullptr)
      return 2;
    std::cout << "confirmed restored-old-size caller: allocation="
              << tests::kOldAllocationBytes
              << " required=" << sizeof(fixture.workspace)
              << " first-outside-pointer=" << first_outside
              << " first-outside-byte=" << tests::kOldAllocationBytes
              << " current-entry-rows=" << count << '\n';
    return 0;
  }
  if (mode == "lazy-empty-output") {
    if (rows::BuildLiveItemRows(0, nullptr, nullptr, nullptr) != 0)
      return 2;
    std::cout << "lazy-empty-output preserved\n";
    return 0;
  }
  if (mode == "unknown-pending-output") {
    fixture.containers.storage->itemsToTransfer[0] = 0xFF01;
    int type = 123;
    if (rows::BuildLiveItemRows(kPending, nullptr, &type, nullptr) != 1 ||
        type != 123)
      return 2;
    std::cout << "unknown-pending-output preserved\n";
    return 0;
  }
  if (mode.starts_with("missing-pending-")) {
    Menu_Framework_unusedInventoryStorage = nullptr;
    if (mode == "missing-pending-find")
      return rows::FindLivePendingTransfer(0, 0);
    if (mode == "missing-pending-append")
      rows::AppendLivePendingTransfer(0, 0);
    if (mode == "missing-pending-rebuild")
      rows::RebuildLivePendingTransfers();
  } else if (mode == "missing-active-order") {
    Menu_Container_containerData = nullptr;
    return fixture.Build(0);
  } else if (mode == "invalid-category") {
    return fixture.Build(256);
  } else if (mode == "invalid-row-record") {
    fixture.containers.storage->container.indices.weapons[0] = 33;
    return fixture.Build(0);
  } else if (mode == "invalid-pending-record") {
    fixture.containers.storage->itemsToTransfer[0] = 0x141;
    rows::RebuildLivePendingTransfers();
  } else if (mode == "missing-row-text") {
    fixture.AddMisc();
    return rows::BuildLiveItemRows(kMisc, nullptr,
                                   fixture.workspace.types.data(),
                                   fixture.workspace.descriptions.data());
  } else if (mode == "missing-row-types") {
    fixture.AddMisc();
    return rows::BuildLiveItemRows(kMisc, fixture.workspace.texts.data(),
                                   nullptr,
                                   fixture.workspace.descriptions.data());
  }
  std::cerr << "guard unexpectedly returned: " << mode << '\n';
  return 3;
}
} // namespace

int main(int argc, char **argv) {
  try {
    return argc == 2 ? Probe(argv[1]) : 2;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 4;
  }
}
