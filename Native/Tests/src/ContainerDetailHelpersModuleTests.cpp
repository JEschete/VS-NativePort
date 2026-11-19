#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Boot/OverlayReset.hpp"
#include "Content/ModuleData.hpp"
#include "Input/Pad.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/ContainerDetailHelpersSamples.hpp"

namespace {
namespace detail = VS::Menu::container;
namespace samples = container_detail_helpers_tests::samples;
using container_transfers_tests::LiveFixture;

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  return VS::Content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                           "MENU/MENUD.PRG", address) ==
         reinterpret_cast<const void*>(entry);
}
}  // namespace

TEST_CASE("F02925 binds actual three-pointer MENUD weapon entry",
          "[container-detail-helpers-module][F02925]") {
  static_assert(std::is_same_v<decltype(&detail::BuildLiveWeaponStats),
                               void (*)(VS::Items::WeaponLoadout*,
                                        const VS::Items::InventoryWeapon*,
                                        VS::Items::ContainerData*)>);
  CHECK(IsBound(0x80102A34, &detail::BuildLiveWeaponStats));
}

TEST_CASE("F02926 binds actual three-pointer MENUD shield entry",
          "[container-detail-helpers-module][F02926]") {
  static_assert(std::is_same_v<decltype(&detail::BuildLiveShieldStats),
                               void (*)(VS::Items::ShieldLoadout*,
                                        const VS::Items::InventoryShield*,
                                        VS::Items::ContainerData*)>);
  CHECK(IsBound(0x80102BB0, &detail::BuildLiveShieldStats));
}

TEST_CASE("F02942 binds actual byte argument and terminal-delay entry",
          "[container-detail-helpers-module][F02942]") {
  static_assert(std::is_same_v<decltype(&detail::SetLivePanelVisibility),
                               void (*)(char)>);
  CHECK(IsBound(0x80103D50, &detail::SetLivePanelVisibility));
}

TEST_CASE("F02943 binds actual signed paging result and arguments",
          "[container-detail-helpers-module][F02943]") {
  static_assert(std::is_same_v<decltype(&detail::PollLiveDetailsPaging),
                               int (*)(int, int)>);
  CHECK(IsBound(0x80103D6C, &detail::PollLiveDetailsPaging));
}

TEST_CASE("F02945 binds actual void transition entry",
          "[container-detail-helpers-module][F02945]") {
  static_assert(std::is_same_v<decltype(&detail::BeginLiveDetailsTransition),
                               void (*)(int)>);
  CHECK(IsBound(0x80103F64, &detail::BeginLiveDetailsTransition));
}

TEST_CASE("F02947 binds actual two-command layout entry",
          "[container-detail-helpers-module][F02947]") {
  static_assert(std::is_same_v<decltype(&detail::InitializeLiveDetailsLayout),
                               void (*)(int, int)>);
  CHECK(IsBound(0x80104034, &detail::InitializeLiveDetailsLayout));
}

TEST_CASE("F02949 binds unsigned source halfword through actual int signature",
          "[container-detail-helpers-module][F02949]") {
  static_assert(std::is_same_v<decltype(&detail::SelectLiveDetailsItem),
                               int (*)(int, int)>);
  CHECK(IsBound(0x80104114, &detail::SelectLiveDetailsItem));
}

TEST_CASE("F02950 binds actual optional-bars exit argument",
          "[container-detail-helpers-module][F02950]") {
  static_assert(
      std::is_same_v<decltype(&detail::ExitLiveDetailsView), void (*)(int)>);
  CHECK(IsBound(0x80104170, &detail::ExitLiveDetailsView));
}

TEST_CASE("Container detail helpers retain normal MENUD BSS across reload",
          "[container-detail-helpers-module][F02925][F02926][F02942][F02943]"
          "[F02945][F02947][F02949][F02950]") {
  LiveFixture fixture;
  const std::uint8_t title = VS::Menu::container::categoryTitleVisible;
  const std::uint8_t icons = VS::Menu::container::categoryIconsVisible;
  const std::uint8_t controls = VS::Menu::container::controlsPanelMode;
  const std::uint8_t phase = VS::Menu::container::detailsViewPhase;
  const int triangle = VS::Menu::container::detailsClosedWithTriangle;
  detail::SetLivePanelVisibility(2);
  VS::Menu::container::detailsViewPhase = samples::kOldPhase;
  VS::Menu::container::detailsClosedWithTriangle =
      static_cast<int>(VS::Input::Triangle);
  VS::Boot::ResetMenudStatics();
  CHECK(VS::Menu::container::categoryTitleVisible == 2);
  CHECK(VS::Menu::container::categoryIconsVisible == 2);
  CHECK(VS::Menu::container::controlsPanelMode == 2);
  CHECK(VS::Menu::container::detailsViewPhase == 99);
  CHECK(VS::Menu::container::detailsClosedWithTriangle ==
        static_cast<int>(VS::Input::Triangle));
  VS::Menu::container::categoryTitleVisible = title;
  VS::Menu::container::categoryIconsVisible = icons;
  VS::Menu::container::controlsPanelMode = controls;
  VS::Menu::container::detailsViewPhase = phase;
  VS::Menu::container::detailsClosedWithTriangle = triangle;
}

TEST_CASE("Ordinary MENUD reload retains detail owner and halfword orders",
          "[container-detail-helpers-module][F02943][F02949]") {
  LiveFixture fixture;
  const auto memory = VS::Menu::Framework::menuMemory;
  std::span(fixture.storage->container.indices.misc).back() =
      samples::kHalfwordToken;
  VS::Menu::container::containerItemCapacities[samples::kMiscCategory] = 0;
  VS::Boot::ResetMenudStatics();
  CHECK(VS::Menu::container::containerData == &fixture.storage->container);
  CHECK(VS::Menu::container::containerItemCapacities[6] == 256);
  CHECK(detail::SelectLiveDetailsItem(6, 255) == 65535);
  VS::Menu::Framework::menuMemory = memory;
}
