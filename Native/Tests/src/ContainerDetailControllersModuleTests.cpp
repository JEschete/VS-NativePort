#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Menu/ContainerDetailControllersLiveFixture.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace detail = container_detail_controllers_tests::detail;
namespace fw = container_detail_controllers_tests::fw;
using container_detail_controllers_tests::kChangedStrength;
using container_detail_controllers_tests::kContainerGem;
using container_detail_controllers_tests::kContainerPart;
using container_detail_controllers_tests::kContainerRow;
using container_detail_controllers_tests::kWeaponStrength;
using LiveFixture = container_detail_controllers_tests::ContainerLiveFixture;
using Entry = int (*)(int);
constexpr std::array<Entry, 6> kEntries{
    detail::UpdateLiveWeaponDetails, detail::UpdateLiveBladeDetails,
    detail::UpdateLiveGripDetails,   detail::UpdateLiveShieldDetails,
    detail::UpdateLiveArmorDetails,  detail::UpdateLiveGemDetails,
};
constexpr std::array<unsigned, 6> kAddresses{
    0x801042D0, 0x80104534, 0x80104728, 0x8010496C, 0x80104BDC, 0x80104E14,
};

template <typename Function>
bool Bound(unsigned address, Function function) {
  return VS::Content::ResolveModuleAddress(
             {Content_moduleSymbols, Content_moduleSymbolCount},
             "MENU/MENUD.PRG",
             address) == reinterpret_cast<const void*>(function);
}
}  // namespace

TEST_CASE("F02944 registry uses the actual two-word MENUD icon signature",
          "[container-detail-controllers-module][F02944]") {
  static_assert(std::is_same_v<decltype(&detail::DrawLiveCategoryIcons),
                               void (*)(int, int)>);
  CHECK(Bound(0x80103E24, &detail::DrawLiveCategoryIcons));
}

TEST_CASE("Container controller registry covers six actual MENUD word entries",
          "[container-detail-controllers-module][F02953][F02954][F02955]"
          "[F02956][F02957][F02958]") {
  for (std::size_t index = 0; index < kEntries.size(); ++index) {
    CHECK(Bound(kAddresses[index], kEntries[index]));
  }
}

TEST_CASE("F02959 registry targets the actual native MENUD detail dispatcher",
          "[container-detail-controllers-module][F02959]") {
  static_assert(
      std::is_same_v<decltype(&detail::UpdateLiveItemDetails), Entry>);
  CHECK(Bound(0x80105008, &detail::UpdateLiveItemDetails));
}

TEST_CASE("Ordinary MENUD reload retains weapon controller BSS animation",
          "[container-detail-controllers-module][F02953]") {
  LiveFixture fixture;
  fixture.AddContainerWeapon();
  fixture.Open(detail::UpdateLiveWeaponDetails);
  fixture.containers.storage->container.items.blades[kContainerPart].strength =
      kChangedStrength;
  Boot_ResetMenudStatics();
  fixture.IdleRows();
  CHECK(detail::UpdateLiveWeaponDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] == kWeaponStrength);
  CHECK(Menu_Container_containerData == &fixture.containers.storage->container);
  detail::ResetLiveContainerDetailControllers();
  fixture.IdleRows();
  CHECK(detail::UpdateLiveWeaponDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] ==
        kChangedStrength + equipment_detail_tests::kGripStrength);
}

TEST_CASE(
    "Ordinary MENUD reload retains gem countdown rather than cold resetting",
    "[container-detail-controllers-module][F02958]") {
  LiveFixture fixture;
  fixture.AddContainerGems();
  fixture.Open(detail::UpdateLiveGemDetails);
  fixture.containers.storage->container.items.gems[kContainerGem].strength =
      kChangedStrength;
  Boot_ResetMenudStatics();
  fixture.IdleRows();
  CHECK(detail::UpdateLiveGemDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] == 9);
  detail::ResetLiveContainerDetailControllers();
  fixture.IdleRows();
  CHECK(detail::UpdateLiveGemDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] == kChangedStrength);
}

TEST_CASE("F02957 ordinary reload preserves full-domain armor countdown",
          "[container-detail-controllers-module][F02957][issue366]") {
  LiveFixture fixture;
  fixture.AddContainerArmor();
  auto& owner = fixture.containers.storage->container;
  owner.indices.armor[0] = kContainerPart + 1;
  fixture.Open(detail::UpdateLiveArmorDetails);
  const auto strength = Menu_Framework_strIntAgi[0];
  owner.items.armor[kContainerPart].strength = kChangedStrength;
  Boot_ResetMenudStatics();
  fixture.IdleRows();
  CHECK(detail::UpdateLiveArmorDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] == strength);
  CHECK(Menu_Container_containerData == &owner);
  detail::ResetLiveContainerDetailControllers();
  fixture.IdleRows();
  CHECK(detail::UpdateLiveArmorDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] == kChangedStrength);
}

TEST_CASE("F02959 ordinary reload retains selected category and panel BSS",
          "[container-detail-controllers-module][F02959]") {
  LiveFixture fixture;
  fixture.AddContainerGems();
  constexpr int kGemEntry = (((kContainerRow << fw::kByteShift) | 1) << 4) | 5;
  CHECK(detail::UpdateLiveItemDetails(kGemEntry) == 0);
  Boot_ResetMenudStatics();
  fixture.IdleRows();
  CHECK(detail::UpdateLiveItemDetails(0) == 0);
  CHECK(Menu_Framework_equipmentSubtype == fw::equipment_subtype::kGem);
  CHECK(Menu_Container_detailsViewPhase == 1);
  detail::ResetLiveContainerDetailControllers();
  fixture.IdleRows();
  CHECK(detail::UpdateLiveItemDetails(0) == 0);
  CHECK(Menu_Framework_equipmentSubtype == fw::equipment_subtype::kWeapon);
}
