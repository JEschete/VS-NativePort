#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/ContainerDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"

namespace {
namespace detail = container_detail_controllers_tests::detail;
namespace fw = container_detail_controllers_tests::fw;
using container_detail_controllers_tests::kChangedStrength;
using container_detail_controllers_tests::kContainerFrames;
using container_detail_controllers_tests::kContainerGem;
using container_detail_controllers_tests::kContainerPart;
using container_detail_controllers_tests::kContainerRow;
using container_detail_controllers_tests::kContainerWeapon;
using container_detail_controllers_tests::kWeaponStrength;
using LiveFixture = container_detail_controllers_tests::ContainerLiveFixture;
namespace input = VS::Input;
constexpr int kCategoryCount = 8;
constexpr int kFirstIconU = 120;
constexpr int kIconPitch = 16;
constexpr int kIconX = 32;
constexpr int kHeader = 31;
constexpr int kGemTextX = 280;
constexpr int kGemTextY = 36;
constexpr int kGemIconX = 256;
constexpr int kGemIconY = 32;
constexpr int kGemIcon = 0x116;
constexpr int kIconLayer = 3;
constexpr std::size_t kGemTextBytes = 8;
constexpr std::uint32_t kSelectedPalette = 0x37FD0000;
constexpr std::uint32_t kOtherPalette = 0x37FE0000;
constexpr std::uint32_t kIconTextureV = 128U << fw::kByteShift;
constexpr std::size_t kArmorBoundaryCount = 3;
constexpr std::uint16_t kLastInventoryArmor = 16;
constexpr std::uint16_t kFirstContainerOnlyArmor = 17;
constexpr char kArmorCategory = 1;
constexpr char kAccessoryCategory = 7;
constexpr std::uint8_t kArmorId = 143;
constexpr std::uint8_t kAccessoryId = 223;

void DrawExpectedIcons() {
  for (int category = 0; category < kCategoryCount; ++category) {
    const int x =
        (((category + 1) & (kCategoryCount - 1)) * kIconPitch) + kIconX;
    auto* packet = VS::Render::QueueLiveSpriteWithTexturePage(
        VS::Render::kDefaultSpriteBrightness,
        static_cast<int>(fw::PackXy(x, -kIconPitch)),
        static_cast<int>(fw::PackXy(kIconPitch, kIconPitch)),
        static_cast<u_long*>(Render_frameScratch.orderingTable) - kIconLayer);
    packet[VS::Render::kSpriteTextureWord] =
        static_cast<std::uint32_t>(kFirstIconU + category * kIconPitch) |
        kIconTextureV | (category == 0 ? kSelectedPalette : kOtherPalette);
  }
}
}  // namespace

TEST_CASE("F02953 live weapon uses container records beyond inventory capacity",
          "[container-detail-controllers-live][F02953]") {
  LiveFixture fixture;
  fixture.AddContainerWeapon();
  fixture.Open(detail::UpdateLiveWeaponDetails);
  CHECK(Menu_Framework_currentStatusViewItem == kContainerWeapon + 1);
  CHECK(Menu_Framework_strIntAgi[0] == kWeaponStrength);
  fixture.Advance(detail::UpdateLiveWeaponDetails);
  Input_buttonRepeat = input::R1;
  CHECK(detail::UpdateLiveWeaponDetails(0) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
  CHECK(fixture.rows[kContainerRow].subText == nullptr);
  CHECK(fixture.rows[kHeader].itemPage == 2);
}

TEST_CASE(
    "F02954 native blade formatter and stats use the full container table",
    "[container-detail-controllers-live][F02954]") {
  LiveFixture fixture;
  fixture.AddContainerBlade();
  fixture.Open(detail::UpdateLiveBladeDetails);
  CHECK(Menu_Framework_currentStatusViewItem == kContainerPart + 1);
  CHECK(Menu_Framework_strIntAgi[0] ==
        equipment_detail_tests::kSecondBladeStrength);
  fixture.Advance(detail::UpdateLiveBladeDetails);
  Input_buttonRepeat = input::R1;
  CHECK(detail::UpdateLiveBladeDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] == equipment_detail_tests::kBladeStrength);
  CHECK(fixture.rows[kHeader].itemPage == 2);
}

TEST_CASE(
    "F02955 real glyph queue reads writable owned container grip text",
    "[container-detail-controllers-live][container-detail-controllers-font]"
    "[F02955]") {
  LiveFixture fixture;
  fixture.AddContainerGrip();
  fixture.Open(detail::UpdateLiveGripDetails);
  fixture.Advance(detail::UpdateLiveGripDetails);
  fixture.ResetFrame();
  CHECK(detail::UpdateLiveGripDetails(0) == 0);
  const auto actual = fixture.Sprites();
  CHECK(fixture.CountLabel(actual) == "X     3");
  fixture.ResetFrame();
  std::array<char, kGemTextBytes> expected_text{'X', ' ', ' ', ' ',
                                                ' ', ' ', '3', '\0'};
  fw::DrawLiveRowIcon(kGemIcon, kGemIconX, kGemIconY);
  VS::Render::DrawLiveCompactText(
      expected_text.data(), static_cast<int>(fw::PackXy(kGemTextX, kGemTextY)),
      nullptr);
  CHECK(actual == fixture.Sprites());
  fixture.ResetFrame();
  Input_buttonRepeat = input::R1;
  CHECK(detail::UpdateLiveGripDetails(0) == 0);
  CHECK(fixture.CountLabel(fixture.Sprites()) == "X     2");
}

TEST_CASE("F02956 actual shield helpers support unoccupied container gem slots",
          "[container-detail-controllers-live][F02956]") {
  LiveFixture fixture;
  fixture.AddContainerShield();
  fixture.Open(detail::UpdateLiveShieldDetails);
  CHECK(Menu_Framework_currentStatusViewItem == kContainerWeapon + 1);
  fixture.Advance(detail::UpdateLiveShieldDetails);
  Input_buttonRepeat = input::R1;
  CHECK(detail::UpdateLiveShieldDetails(0) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
  CHECK(fixture.rows[kHeader].itemPage == 2);
  CHECK(Menu_Framework_equipmentSubtype == fw::equipment_subtype::kShield);
}

TEST_CASE("F02957 actual armor paging reaches container record sixty four",
          "[container-detail-controllers-live][F02957]") {
  LiveFixture fixture;
  fixture.AddContainerArmor();
  fixture.Open(detail::UpdateLiveArmorDetails);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
  fixture.Advance(detail::UpdateLiveArmorDetails);
  Input_buttonRepeat = input::R1;
  CHECK(detail::UpdateLiveArmorDetails(0) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == kContainerPart + 1);
  CHECK(fixture.rows[kHeader].itemPage == 2);
}

TEST_CASE(
    "F02957 actual DP glyphs follow selected container armor not inventory",
    "[container-detail-controllers-live][container-detail-controllers-font]"
    "[F02957][issue366]") {
  constexpr std::array<std::uint16_t, kArmorBoundaryCount> kReferences{
      kLastInventoryArmor,
      kFirstContainerOnlyArmor,
      kContainerPart + 1,
  };
  for (const auto reference : kReferences) {
    for (const bool accessory : {false, true}) {
      LiveFixture fixture;
      fixture.AddContainerArmor();
      auto& owner = fixture.containers.storage->container;
      auto& selected = owner.items.armor[reference - 1];
      selected = owner.items.armor[0];
      selected.category = accessory ? kAccessoryCategory : kArmorCategory;
      selected.id = accessory ? kAccessoryId : kArmorId;
      owner.indices.armor[0] = reference;
      for (auto& armor : Audio_Seq_unusedInventory.armor) {
        armor.category = accessory ? kArmorCategory : kAccessoryCategory;
      }
      fixture.Open(detail::UpdateLiveArmorDetails);
      for (int tick = 0; tick < kContainerFrames; ++tick) {
        fixture.ResetFrame();
        fw::UpdateLiveDpPpBars(0);
      }
      CHECK(static_cast<std::uint8_t>(Menu_Framework_currentStatusViewItem) ==
            reference);
      CHECK(fixture.Sprites().empty() == accessory);
      CHECK(static_cast<std::uint8_t>(Menu_Framework_equipmentSubtype) ==
            (accessory ? fw::equipment_subtype::kAccessory
                       : fw::equipment_subtype::kArmor));
    }
  }
}

TEST_CASE("F02957 real reentry resolves replacement container-only armor",
          "[container-detail-controllers-live][F02957][issue366]") {
  LiveFixture fixture;
  fixture.AddContainerArmor();
  auto& previous = fixture.containers.storage->container;
  previous.items.armor[kLastInventoryArmor - 1] = previous.items.armor[0];
  previous.indices.armor[0] = kLastInventoryArmor;
  fixture.Open(detail::UpdateLiveArmorDetails);
  CHECK(Menu_Framework_currentStatusViewItem == kLastInventoryArmor);
  auto& next = fixture.containers.replacement->container;
  auto& selected = next.items.armor[kFirstContainerOnlyArmor - 1];
  selected = previous.items.armor[0];
  selected.category = kAccessoryCategory;
  selected.id = kAccessoryId;
  selected.strength = kChangedStrength;
  next.indices.armor[0] = kFirstContainerOnlyArmor;
  LiveFixture::Bind(next);
  CHECK(detail::UpdateLiveArmorDetails((kContainerRow << fw::kByteShift) | 1) ==
        0);
  fixture.IdleRows();
  CHECK(detail::UpdateLiveArmorDetails(0) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == kFirstContainerOnlyArmor);
  CHECK(Menu_Framework_strIntAgi[0] == kChangedStrength);
}

TEST_CASE("F02958 gem statistics preserve container reference one ninety two",
          "[container-detail-controllers-live][F02958]") {
  LiveFixture fixture;
  fixture.AddContainerGems();
  fixture.Open(detail::UpdateLiveGemDetails);
  CHECK(static_cast<std::uint8_t>(Menu_Framework_currentStatusViewItem) ==
        kContainerGem + 1);
  CHECK(Menu_Framework_strIntAgi[0] == 9);
  fixture.Advance(detail::UpdateLiveGemDetails);
  Input_buttonRepeat = input::R1;
  CHECK(detail::UpdateLiveGemDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] == 5);
  CHECK(fixture.rows[kHeader].itemPage == 2);
}

TEST_CASE("F02958 live re-entry resolves the new owner before loading stats",
          "[container-detail-controllers-live][F02958]") {
  LiveFixture fixture;
  fixture.AddContainerGems();
  fixture.Open(detail::UpdateLiveGemDetails);
  auto& next = fixture.containers.replacement->container;
  next.items.gems[0].strength = kChangedStrength;
  next.indices.gems[0] = 1;
  LiveFixture::Bind(next);
  CHECK(detail::UpdateLiveGemDetails((kContainerRow << fw::kByteShift) | 1) ==
        0);
  fixture.IdleRows();
  CHECK(detail::UpdateLiveGemDetails(0) == 0);
  CHECK(Menu_Framework_strIntAgi[0] == kChangedStrength);
}

TEST_CASE("F02944 real queue traversal preserves container layer and palettes",
          "[container-detail-controllers-live][F02944]") {
  LiveFixture fixture;
  Menu_Framework_menuMemory.opaqueA0[0] = 1;
  Menu_Framework_menuMemory.opaque3A.currentCategory = 2;
  fixture.ResetFrame();
  detail::DrawLiveCategoryIcons(0, 0);
  const auto actual = fixture.Sprites();
  fixture.ResetFrame();
  DrawExpectedIcons();
  const auto expected = fixture.Sprites();
  REQUIRE(expected.size() == kCategoryCount);
  CHECK(actual == expected);
}

TEST_CASE("F02944 merged header does not require unused container text owners",
          "[container-detail-controllers-live][F02944]") {
  LiveFixture fixture;
  constexpr int kMergeX = 60;
  constexpr int kFirstCategoryIcon = 24;
  Menu_Framework_menuMemory.opaqueA0[0] = 1;
  fixture.rows[kHeader].x = kMergeX;
  Menu_Container_containerData = nullptr;
  Menu_Framework_itemNames = nullptr;
  Menu_Framework_itemHelp = nullptr;
  fixture.ResetFrame();
  detail::DrawLiveCategoryIcons(0, 1);
  CHECK(fixture.rows[kHeader].rowIcon == kFirstCategoryIcon);
  CHECK(fixture.Sprites().size() == kCategoryCount - 1);
}

TEST_CASE(
    "F02959 real dispatcher leaves gem details then restores container UI",
    "[container-detail-controllers-live][F02959]") {
  LiveFixture fixture;
  fixture.AddContainerGems();
  constexpr int kGemCategory = 5;
  constexpr int kCategoryShift = 4;
  const int entry =
      (((kContainerRow << fw::kByteShift) | 1) << kCategoryShift) |
      kGemCategory;
  CHECK(detail::UpdateLiveItemDetails(entry) == 0);
  fixture.IdleRows();
  CHECK(detail::UpdateLiveItemDetails(0) == 0);
  fixture.Advance(detail::UpdateLiveItemDetails);
  Input_buttonsPressed.allButtons = input::Cross;
  CHECK(detail::UpdateLiveItemDetails(0) == 0);
  fixture.IdleRows();
  CHECK(detail::UpdateLiveItemDetails(0) == 1);
  CHECK(Menu_Container_detailsViewPhase == 0);
  CHECK(Menu_Container_categoryIconsVisible == 1);
  CHECK(Menu_Container_controlsPanelMode == 1);
}
