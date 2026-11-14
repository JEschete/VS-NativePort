#pragma once

#include <array>

#include "Items/ContainerIndexing.hpp"
#include "Menu/Framework/ItemRows.hpp"

namespace VS::Menu::container {
inline constexpr auto kItemRowCount = Items::ContainerContents{}.misc.size();

// The compatibility caller's three buffers have native pointer-width storage.
// The typed row policy borrows each reached description and emits into them.
struct ItemRowsWorkspace {
  std::array<char, kItemRowCount * Framework::kRowDescriptionBufferBytes>
      descriptions{};
  std::array<int, kItemRowCount> types{};
  std::array<char*, kItemRowCount * 2> texts{};
};

[[nodiscard]] int FindLivePendingTransfer(int category, int slot);
void AppendLivePendingTransfer(int category, int slot);
void RebuildLivePendingTransfers();
[[nodiscard]] int BuildLiveItemRows(int category, char** text, int* row_types,
                                    char* descriptions);
}  // namespace VS::Menu::container
