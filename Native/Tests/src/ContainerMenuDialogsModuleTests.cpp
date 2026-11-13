#include <cstdint>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Menu/Container/ContainerMenuDialogsLive.hpp"

namespace {
namespace dialogs = VS::Menu::container;
constexpr std::uint32_t kSortAddress = 0x80105ACC;
constexpr std::uint32_t kDiscardAddress = 0x80105D24;
constexpr std::uint32_t kRestoreAddress = 0x801071D8;
constexpr std::uint32_t kTransferAddress = 0x801072B0;

template <typename Entry>
bool HasBinding(std::uint32_t address, Entry entry) {
  const auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "MENU/MENUD.PRG",
      address);
  return resolved == reinterpret_cast<const void*>(entry);
}
}  // namespace

TEST_CASE("F02969 registry binds its original integer dialog ABI",
          "[container-menu-dialogs-module]") {
  static_assert(
      std::is_same_v<decltype(&dialogs::UpdateLiveSortMenu), int (*)(int)>);
  CHECK(HasBinding(kSortAddress, &dialogs::UpdateLiveSortMenu));
}

TEST_CASE("F02970 registry binds its full-word packed discard ABI",
          "[container-menu-dialogs-module]") {
  static_assert(
      std::is_same_v<decltype(&dialogs::UpdateLiveDiscardMenu), int (*)(int)>);
  CHECK(HasBinding(kDiscardAddress, &dialogs::UpdateLiveDiscardMenu));
}

TEST_CASE("F02983 registry retains the original void return ABI",
          "[container-menu-dialogs-module]") {
  static_assert(
      std::is_same_v<decltype(&dialogs::RestoreLiveItemRows), void (*)(int)>);
  CHECK(HasBinding(kRestoreAddress, &dialogs::RestoreLiveItemRows));
}

TEST_CASE("F02984 registry binds the original integer transfer dialog ABI",
          "[container-menu-dialogs-module]") {
  static_assert(
      std::is_same_v<decltype(&dialogs::UpdateLiveTransferConfirmation),
                     int (*)(int)>);
  CHECK(HasBinding(kTransferAddress, &dialogs::UpdateLiveTransferConfirmation));
}
