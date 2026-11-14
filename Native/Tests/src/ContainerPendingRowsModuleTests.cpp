#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Menu/Container/ContainerPendingRowsLive.hpp"

namespace {
namespace rows = VS::Menu::container;

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  const auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "MENU/MENUD.PRG",
      address);
  return resolved == reinterpret_cast<const void*>(entry);
}
}  // namespace

TEST_CASE("F02979 binds its actual MENUD two-word int-return ABI",
          "[container-pending-rows-module][F02979]") {
  static_assert(std::is_same_v<decltype(&rows::FindLivePendingTransfer),
                               int (*)(int, int)>);
  CHECK(IsBound(0x801069B0, &rows::FindLivePendingTransfer));
}

TEST_CASE("F02980 binds its actual MENUD two-word void-return ABI",
          "[container-pending-rows-module][F02980]") {
  static_assert(std::is_same_v<decltype(&rows::AppendLivePendingTransfer),
                               void (*)(int, int)>);
  CHECK(IsBound(0x80106A04, &rows::AppendLivePendingTransfer));
}

TEST_CASE("F02981 binds its actual MENUD zero-argument void ABI",
          "[container-pending-rows-module][F02981]") {
  static_assert(
      std::is_same_v<decltype(&rows::RebuildLivePendingTransfers), void (*)()>);
  CHECK(IsBound(0x80106A50, &rows::RebuildLivePendingTransfers));
}

TEST_CASE("F02982 binds its actual MENUD category/text/type/buffer ABI",
          "[container-pending-rows-module][F02982]") {
  static_assert(std::is_same_v<decltype(&rows::BuildLiveItemRows),
                               int (*)(int, char**, int*, char*)>);
  CHECK(IsBound(0x80106C64, &rows::BuildLiveItemRows));
}
