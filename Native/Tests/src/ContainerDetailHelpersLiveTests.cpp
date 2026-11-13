#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/ContainerDetailHelpersSamples.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"

extern "C" {
#include "Menu/Framework/Menus.h"
}

namespace {
namespace detail = VS::Menu::container;
namespace items = VS::Items;
namespace fw = VS::Menu::Framework;
namespace samples = container_detail_helpers_tests::samples;
using equipment_detail_tests::ValueRestore;
constexpr int kSelected = 20;
constexpr std::size_t kHeader = 31;
using MemoryBytes = std::array<std::uint8_t, sizeof(Menu_Framework_MenuMemory)>;

struct LiveFixture : equipment_detail_tests::LiveFixture {
  container_transfers_tests::LiveFixture containers;
  ValueRestore<char> saved_container_title{Menu_Container_categoryTitleVisible};
  ValueRestore<char> saved_container_icons{Menu_Container_categoryIconsVisible};
  ValueRestore<char> saved_container_controls{Menu_Container_controlsPanelMode};
  ValueRestore<char> saved_container_phase{Menu_Container_detailsViewPhase};
  ValueRestore<int> saved_triangle{Menu_Container_detailsClosedWithTriangle};

  LiveFixture() {
    Menu_Container_categoryTitleVisible = samples::kTitle;
    Menu_Container_categoryIconsVisible = samples::kIcons;
    Menu_Container_controlsPanelMode = samples::kControls;
    Menu_Container_detailsViewPhase = samples::kOldPhase;
    Menu_Container_detailsClosedWithTriangle = samples::kOldTriangle;
  }

  void AddContainerWeapon() {
    auto& owner = containers.storage->container.items;
    items::Live::Commit(std::span(owner.blades).back(), samples::kBlade);
    items::Live::Commit(std::span(owner.grips).back(), samples::kGrip);
    items::Live::Commit(std::span(owner.gems).back(), samples::kWeaponGem);
    items::Live::Commit(std::span(owner.weapons).back(), samples::kWeapon);
  }

  void ActivateOtherRow() {
    constexpr std::array<std::uint8_t, 2> kText{1, VS::Render::kTextTerminator};
    fw::SetLiveMenuItemText(
        {.row = samples::kOtherActiveRow, .w = samples::kHeaderWidth}, kText);
  }
};

void DrawClassPage(LiveFixture& fixture) {
  constexpr int kSlideFrames = 10;
  for (int tick = 0; tick < kSlideFrames; ++tick) {
    fixture.ResetFrame();
    fw::UpdateLiveEquipmentStatPages(0);
  }
}

using equipment_detail_tests::DrawnSprite;

std::vector<DrawnSprite> CompletePageSprites(const LiveFixture& fixture) {
  const auto opaque = fixture.Sprites();
  std::vector<DrawnSprite> sprites;
  std::size_t opaque_index = 0;
  const void* packet = &fixture.ordering.back();
  for (std::size_t visits = 0;
       packet != nullptr && visits < equipment_detail_tests::kPacketVisitLimit;
       ++visits) {
    if (!fixture.Owns(packet)) {
      throw std::runtime_error("stat-page packet outside current frame owner");
    }
    const auto* words = static_cast<const u_long*>(packet);
    const auto length = static_cast<const P_TAG*>(packet)->len;
    const auto command = length == VS::Render::kSpritePacketWords - 1
                             ? words[VS::Render::kSpriteColorWord] >>
                                   VS::Render::kPacketLengthShift
                             : 0U;
    if (length == VS::Render::kSpritePacketWords - 1 &&
        command == (VS::Render::kSpriteCommand |
                    VS::Render::kSemiTransparentSpriteFlag)) {
      sprites.push_back({words[VS::Render::kSpriteColorWord],
                         words[VS::Render::kSpritePositionWord],
                         words[VS::Render::kSpriteTextureWord],
                         words[VS::Render::kSpriteSizeWord],
                         words[VS::Render::kSpriteDrawModeWord], false});
    } else if ((length == VS::Render::kSpritePacketWords - 1 &&
                command == VS::Render::kSpriteCommand) ||
               (length == equipment_detail_tests::kCompactWords - 1 &&
                (words[equipment_detail_tests::kCompactColor] >>
                 VS::Render::kPacketLengthShift) ==
                    VS::Render::kSpriteCommand)) {
      sprites.push_back(opaque.at(opaque_index++));
    }
    packet = isendprim(packet) ? nullptr : nextPrim(packet);
  }
  if (packet != nullptr || opaque_index != opaque.size()) {
    throw std::runtime_error("stat-page packet traversal was incomplete");
  }
  return sprites;
}

void CheckTabGlyphs(std::span<const DrawnSprite> sprites) {
  std::vector<DrawnSprite> actual;
  std::ranges::copy_if(sprites, std::back_inserter(actual),
                       [](const DrawnSprite& sprite) {
                         return !sprite.compact &&
                                sprite.draw_mode == samples::kVariableDrawMode;
                       });
  std::vector<DrawnSprite> expected;
  int x = -Menu_Framework_rowAnimationSteps[1] + samples::kTabX;
  for (std::size_t index = 0; index < samples::kTabGlyphs.size(); ++index) {
    const auto glyph = samples::kTabGlyphs[index];
    const auto clut = samples::kClutBase |
                      ((samples::kTabPalettes[index] + samples::kClutBias) &
                       samples::kClutMask);
    const auto texture =
        (glyph % VS::Render::kVariableFontGlyphsPerRow) * samples::kGlyphCell |
        ((glyph / VS::Render::kVariableFontGlyphsPerRow) * samples::kGlyphCell
         << fw::kByteShift) |
        (clut << fw::kHalfwordShift);
    expected.push_back({samples::kVariableColor, fw::PackXy(x, samples::kTabY),
                        texture, samples::kVariableSize,
                        samples::kVariableDrawMode, false});
    x += static_cast<std::uint8_t>(Render_variableFontGlyphWidths[glyph]);
  }
  std::reverse(expected.begin(), expected.end());
  CHECK(actual == expected);
}
}  // namespace

TEST_CASE("F02925 live stats use actual record conversion and equipment rules",
          "[container-detail-helpers-live][F02925]") {
  LiveFixture fixture;
  fixture.AddContainerWeapon();
  auto& owner = fixture.containers.storage->container.items;
  const auto source = samples::kWeapon;
  Items_WeaponLoadout target{};
  detail::BuildLiveWeaponStats(&target, &std::span(owner.weapons).back(),
                               &owner);
  const auto native = items::Live::Typed(target);
  CHECK(native.blade.id == 41);
  CHECK(native.grip.id == 300);
  CHECK(native.gems[2].id == 511);
  CHECK(native.attributes.current_strength == 19);
  CHECK(native.blade.material == 0x34);
  CHECK(native.index == 0xFE);
  CHECK(native.name == source.name);
}

TEST_CASE("F02925 actual item-row host reloads its current container owner",
          "[container-detail-helpers-live][F02925]") {
  LiveFixture fixture;
  fixture.AddContainerWeapon();
  Menu_Framework_weapons = fixture.containers.storage->container.items.weapons;
  fw::LiveItemRowsHost host;
  auto loadout = host.InventoryWeaponLoadout(samples::kWeapon);
  if (!loadout) {
    FAIL("missing current container weapon loadout");
    return;
  }
  CHECK(loadout->gems[2].id == 511);
  CHECK(loadout->blade.id == 41);
  auto& next = fixture.containers.replacement->container;
  constexpr std::uint8_t kReplacementBlade = 17;
  std::span(next.items.blades).back().id = kReplacementBlade;
  Menu_Container_containerData = &next;
  loadout = host.InventoryWeaponLoadout(samples::kWeapon);
  if (!loadout) {
    FAIL("missing replacement container weapon loadout");
    return;
  }
  CHECK(loadout->blade.id == kReplacementBlade);
}

TEST_CASE("F02925 zero optional parts do not require a container owner",
          "[container-detail-helpers-live][F02925]") {
  LiveFixture fixture;
  Items_InventoryWeapon source{};
  source.name[std::size(source.name) - 1] = 'z';
  Items_WeaponLoadout target{};
  Menu_Container_containerData = nullptr;
  Menu_Framework_menuItems = nullptr;
  detail::BuildLiveWeaponStats(&target, &source, nullptr);
  CHECK(target.name[23] == 'z');
  CHECK(target.blade.id == 0);
}

TEST_CASE(
    "F02926 live shield uses flagged references and retains reserved data",
    "[container-detail-helpers-live][F02926]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  items::Live::Commit(owner.gems[samples::kFlaggedGemIndex],
                      samples::kShieldGem);
  items::Live::Commit(std::span(owner.shields).back(), samples::kShield);
  Items_ShieldLoadout target{};
  std::span(target.reserved0).back() = samples::kReservedHeaderTail;
  detail::BuildLiveShieldStats(&target, &std::span(owner.shields).back(),
                               &owner);
  CHECK(target.index == 255);
  CHECK(target.base.id == 31);
  CHECK(target.gems[0].id == 500);
  CHECK(target.gems[1].id == 0);
  CHECK(target.gems[2].id == 500);
  CHECK(target.currentStr == -6);
  CHECK(target.reserved0[5] == 71);
  detail::BuildLiveShieldStats(&target, nullptr, nullptr);
  CHECK(target.base.id == 0);
  CHECK(target.reserved0[5] == 71);
}

TEST_CASE("F02926 actual item-row host uses flagged container gem records",
          "[container-detail-helpers-live][F02926]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  items::Live::Commit(owner.gems[samples::kFlaggedGemIndex],
                      samples::kShieldGem);
  Menu_Framework_shields = owner.shields;
  fw::LiveItemRowsHost host;
  const auto loadout = host.InventoryShieldLoadout(samples::kShield);
  if (!loadout) {
    FAIL("missing current container shield loadout");
    return;
  }
  CHECK(loadout->gems[0].id == 500);
  CHECK(loadout->gems[2].id == 500);
  CHECK(loadout->attributes.current_strength == -6);
}

TEST_CASE(
    "F02942 live byte visibility does not touch unrelated inventory state",
    "[container-detail-helpers-live][F02942]") {
  LiveFixture fixture;
  Menu_Framework_menuItems = nullptr;
  Menu_Container_containerData = nullptr;
  detail::SetLivePanelVisibility(static_cast<char>(samples::kByteMax));
  CHECK(static_cast<std::uint8_t>(Menu_Container_categoryTitleVisible) == 255);
  CHECK(static_cast<std::uint8_t>(Menu_Container_categoryIconsVisible) == 255);
  CHECK(static_cast<std::uint8_t>(Menu_Container_controlsPanelMode) == 255);
  CHECK(Menu_Inventory_secondaryTitleEnabled == fixture.saved_title.previous);
  CHECK(Menu_Container_detailsViewPhase == 99);
}

TEST_CASE(
    "F02943 live paging counts the current container not Ashley inventory",
    "[container-detail-helpers-live][F02943]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container;
  owner.items.weapons[0].blade = 1;
  std::span(owner.items.weapons).back().blade = samples::kWeapon.blade;
  Input_buttonRepeat = VS::Input::L1;
  CHECK(detail::PollLiveDetailsPaging(0, 0) == 1);
  Menu_Container_containerData = &fixture.containers.replacement->container;
  fixture.containers.replacement->container.items.weapons[0].blade = 1;
  CHECK(detail::PollLiveDetailsPaging(0, 0) == 0);
  Menu_Container_containerData = nullptr;
  Input_buttonsState = VS::Input::L1 | VS::Input::R1;
  CHECK(detail::PollLiveDetailsPaging(-1, -17) == -17);
}

TEST_CASE(
    "F02945 live transition uses real clear rows and clears selectability",
    "[container-detail-helpers-live][F02945]") {
  LiveFixture fixture;
  fixture.IdleRows();
  fixture.ActivateOtherRow();
  fixture.rows[kSelected].unselectable = samples::kControls;
  char borrowed_text[] = "borrowed";
  fixture.rows[kSelected].subText = borrowed_text;
  detail::BeginLiveDetailsTransition(kSelected);
  CHECK(fixture.rows[3].targetPosition0 == 320);
  CHECK(fixture.rows[kHeader].state == Menu_Framework_MenuItemState_SlideY);
  CHECK(fixture.rows[kHeader].targetPosition0 == 18);
  CHECK(fixture.rows[kSelected].targetPosition0 == 155);
  CHECK(fixture.rows[kSelected].selected == 1);
  CHECK(fixture.rows[kSelected].unselectable == 0);
  CHECK(fixture.rows[kSelected].subText == nullptr);
  detail::BeginLiveDetailsTransition(static_cast<int>(kHeader));
  CHECK(fixture.rows[kHeader].state == Menu_Framework_MenuItemState_SlideX);
  CHECK(fixture.rows[kHeader].targetPosition0 == 155);
}

TEST_CASE("F02947 live layout integrates native transition and stat helpers",
          "[container-detail-helpers-live][F02947]") {
  LiveFixture fixture;
  detail::InitializeLiveDetailsLayout(kSelected, samples::kClassAffinityPages);
  CHECK(Menu_Container_categoryTitleVisible == 7);
  CHECK(Menu_Container_categoryIconsVisible == 0);
  CHECK(Menu_Container_controlsPanelMode == 0);
  CHECK(Menu_Inventory_ashleyRenderState == 0);
  CHECK(fixture.rows[kHeader].w == 164);
  CHECK(fixture.rows[kHeader].targetPosition0 == 16);
  CHECK(fixture.rows[kSelected].targetPosition0 == 18);
  CHECK(Menu_Framework_enabledStatPages == 3);
}

TEST_CASE("F02947 live selected-header alias keeps continuation write order",
          "[container-detail-helpers-live][F02947]") {
  LiveFixture fixture;
  detail::InitializeLiveDetailsLayout(static_cast<int>(kHeader),
                                      samples::kTypePage);
  CHECK(fixture.rows[kHeader].state == Menu_Framework_MenuItemState_SlideY);
  CHECK(fixture.rows[kHeader].targetPosition0 == 18);
  CHECK(Menu_Framework_enabledStatPages == 4);
}

TEST_CASE(
    "F02947 real stat-page glyphs and ordering match original helper calls",
    "[container-detail-helpers-live][container-detail-helpers-font][F02947]") {
  LiveFixture fixture;
  ValueRestore<char> saved_slide_step{Menu_Framework_rowAnimationSteps[1]};
  Menu_Framework_rowAnimationSteps[1] = samples::kFinalSlideStep;
  const auto font_before = VS::Render::LiveFontStyle();
  detail::InitializeLiveDetailsLayout(kSelected, samples::kClassAffinityPages);
  DrawClassPage(fixture);
  const auto actual = CompletePageSprites(fixture);
  CheckTabGlyphs(actual);
  fw::ResetLiveEquipmentDetailsStatics();
  VS::Render::LiveFontStyle() = font_before;
  fw::ContinueLiveItemDetailsTransition(kSelected);
  fw::UpdateLiveEquipmentStatPages(samples::kClassAffinityPages);
  fw::UpdateLivePrimaryStatPanel(1);
  DrawClassPage(fixture);
  const auto expected = CompletePageSprites(fixture);
  CheckTabGlyphs(expected);
  REQUIRE(expected.size() >= 48);
  REQUIRE(expected.size() == samples::kCompleteClassPageSprites);
  CHECK(actual == expected);
}

TEST_CASE("F02949 live selection preserves exact container cursor bytes",
          "[container-detail-helpers-live][F02949]") {
  LiveFixture fixture;
  MemoryBytes expected{};
  expected.fill(samples::kReservedByte);
  Menu_Framework_menuMemory =
      std::bit_cast<Menu_Framework_MenuMemory>(expected);
  std::span(fixture.containers.storage->container.indices.misc).back() =
      samples::kHalfwordToken;
  expected[samples::kCursorBase + samples::kMiscCategory * 2] = 0;
  expected[samples::kCursorBase + samples::kMiscCategory * 2 + 1] =
      samples::kByteMax;
  Menu_Framework_menuItems = nullptr;
  CHECK(detail::SelectLiveDetailsItem(6, 255) == 65535);
  CHECK(std::bit_cast<MemoryBytes>(Menu_Framework_menuMemory) == expected);
  Menu_Container_containerData = &fixture.containers.replacement->container;
  std::span(fixture.containers.replacement->container.indices.misc).back() =
      samples::kByteOverflowToken;
  CHECK(detail::SelectLiveDetailsItem(6, 255) == 256);
}

TEST_CASE("F02950 real exit retains container phase and full triangle mask",
          "[container-detail-helpers-live][F02950]") {
  LiveFixture fixture;
  detail::InitializeLiveDetailsLayout(kSelected, samples::kClassAffinityPages);
  fixture.IdleRows();
  fixture.rows[kHeader].w = samples::kHeaderWidth;
  Input_buttonsPressed.allButtons = VS::Input::Triangle;
  detail::ExitLiveDetailsView(1);
  CHECK(fixture.rows[kHeader].targetPosition0 == -164);
  CHECK(Menu_Container_detailsViewPhase == 2);
  CHECK(Menu_Container_detailsClosedWithTriangle ==
        static_cast<int>(VS::Input::Triangle));
  Input_buttonsPressed.allButtons = VS::Input::Circle;
  detail::ExitLiveDetailsView(0);
  CHECK(Menu_Container_detailsClosedWithTriangle == 0);
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 1);
}
