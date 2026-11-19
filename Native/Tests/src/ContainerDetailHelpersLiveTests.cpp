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
#include "Menu/Framework/Menus.hpp"
#include "Menu/Inventory/Inventory.hpp"

namespace {
namespace detail = VS::Menu::container;
namespace items = VS::Items;
namespace fw = VS::Menu::Framework;
namespace samples = container_detail_helpers_tests::samples;
using equipment_detail_tests::ValueRestore;
constexpr int kSelected = 20;
constexpr std::size_t kHeader = 31;
using MemoryBytes = std::array<std::uint8_t, sizeof(fw::MenuMemory)>;

struct LiveFixture : equipment_detail_tests::LiveFixture {
  container_transfers_tests::LiveFixture containers;
  ValueRestore<std::uint8_t> saved_container_title{
      VS::Menu::container::categoryTitleVisible};
  ValueRestore<std::uint8_t> saved_container_icons{
      VS::Menu::container::categoryIconsVisible};
  ValueRestore<std::uint8_t> saved_container_controls{
      VS::Menu::container::controlsPanelMode};
  ValueRestore<std::uint8_t> saved_container_phase{
      VS::Menu::container::detailsViewPhase};
  ValueRestore<int> saved_triangle{
      VS::Menu::container::detailsClosedWithTriangle};

  LiveFixture() {
    VS::Menu::container::categoryTitleVisible = samples::kTitle;
    VS::Menu::container::categoryIconsVisible = samples::kIcons;
    VS::Menu::container::controlsPanelMode = samples::kControls;
    VS::Menu::container::detailsViewPhase = samples::kOldPhase;
    VS::Menu::container::detailsClosedWithTriangle = samples::kOldTriangle;
  }

  void AddContainerWeapon() {
    auto& owner = containers.storage->container.items;
    owner.blades.back() = samples::kBlade;
    owner.grips.back() = samples::kGrip;
    owner.gems.back() = samples::kWeaponGem;
    owner.weapons.back() = samples::kWeapon;
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
  int x = -VS::Menu::Framework::rowAnimationSteps[1] + samples::kTabX;
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
    x += static_cast<std::uint8_t>(VS::Render::variableFontGlyphWidths[glyph]);
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
  VS::Items::WeaponLoadout target{};
  detail::BuildLiveWeaponStats(&target, &owner.weapons.back(), &owner);
  CHECK(target.blade.id == 41);
  CHECK(target.grip.id == 300);
  CHECK(target.gems[2].id == 511);
  CHECK(target.attributes.current_strength == 19);
  CHECK(target.blade.material == 0x34);
  CHECK(target.index == 0xFE);
  CHECK(target.name == source.name);
}

TEST_CASE("F02925 actual item-row host reloads its current container owner",
          "[container-detail-helpers-live][F02925]") {
  LiveFixture fixture;
  fixture.AddContainerWeapon();
  VS::Menu::Framework::weapons =
      fixture.containers.storage->container.items.weapons.data();
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
  VS::Menu::container::containerData = &next;
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
  VS::Items::InventoryWeapon source{};
  source.name[std::size(source.name) - 1] = 'z';
  VS::Items::WeaponLoadout target{};
  VS::Menu::container::containerData = nullptr;
  VS::Menu::Framework::menuItems = nullptr;
  detail::BuildLiveWeaponStats(&target, &source, nullptr);
  CHECK(target.name[23] == 'z');
  CHECK(target.blade.id == 0);
}

TEST_CASE(
    "F02926 live shield uses flagged references and retains reserved data",
    "[container-detail-helpers-live][F02926]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  owner.gems[samples::kFlaggedGemIndex] = samples::kShieldGem;
  owner.shields.back() = samples::kShield;
  VS::Items::ShieldLoadout target{};
  std::span(target.reserved0).back() = samples::kReservedHeaderTail;
  detail::BuildLiveShieldStats(&target, &owner.shields.back(), &owner);
  CHECK(target.index == 255);
  CHECK(target.base.id == 31);
  CHECK(target.gems[0].id == 500);
  CHECK(target.gems[1].id == 0);
  CHECK(target.gems[2].id == 500);
  CHECK(target.attributes.current_strength == -6);
  CHECK(target.reserved0[5] == 71);
  detail::BuildLiveShieldStats(&target, nullptr, nullptr);
  CHECK(target.base.id == 0);
  CHECK(target.reserved0[5] == 71);
}

TEST_CASE("F02926 actual item-row host uses flagged container gem records",
          "[container-detail-helpers-live][F02926]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  owner.gems[samples::kFlaggedGemIndex] = samples::kShieldGem;
  VS::Menu::Framework::shields = owner.shields.data();
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
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::container::containerData = nullptr;
  detail::SetLivePanelVisibility(static_cast<char>(samples::kByteMax));
  CHECK(static_cast<std::uint8_t>(VS::Menu::container::categoryTitleVisible) ==
        255);
  CHECK(static_cast<std::uint8_t>(VS::Menu::container::categoryIconsVisible) ==
        255);
  CHECK(static_cast<std::uint8_t>(VS::Menu::container::controlsPanelMode) ==
        255);
  CHECK(VS::Menu::Inventory::secondaryTitleEnabled ==
        fixture.saved_title.previous);
  CHECK(VS::Menu::container::detailsViewPhase == 99);
}

TEST_CASE(
    "F02943 live paging counts the current container not Ashley inventory",
    "[container-detail-helpers-live][F02943]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container;
  owner.items.weapons[0].blade = 1;
  std::span(owner.items.weapons).back().blade = samples::kWeapon.blade;
  VS::Input::buttonRepeat = VS::Input::L1;
  CHECK(detail::PollLiveDetailsPaging(0, 0) == 1);
  VS::Menu::container::containerData =
      &fixture.containers.replacement->container;
  fixture.containers.replacement->container.items.weapons[0].blade = 1;
  CHECK(detail::PollLiveDetailsPaging(0, 0) == 0);
  VS::Menu::container::containerData = nullptr;
  VS::Input::buttonsState = VS::Input::L1 | VS::Input::R1;
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
  fixture.rows[kSelected].sub_text = borrowed_text;
  detail::BeginLiveDetailsTransition(kSelected);
  CHECK(fixture.rows[3].target0 == 320);
  CHECK(fixture.rows[kHeader].state == fw::RowState::kSlideY);
  CHECK(fixture.rows[kHeader].target0 == 18);
  CHECK(fixture.rows[kSelected].target0 == 155);
  CHECK(fixture.rows[kSelected].selected == 1);
  CHECK(fixture.rows[kSelected].unselectable == 0);
  CHECK(fixture.rows[kSelected].sub_text == nullptr);
  detail::BeginLiveDetailsTransition(static_cast<int>(kHeader));
  CHECK(fixture.rows[kHeader].state == fw::RowState::kSlideX);
  CHECK(fixture.rows[kHeader].target0 == 155);
}

TEST_CASE("F02947 live layout integrates native transition and stat helpers",
          "[container-detail-helpers-live][F02947]") {
  LiveFixture fixture;
  detail::InitializeLiveDetailsLayout(kSelected, samples::kClassAffinityPages);
  CHECK(VS::Menu::container::categoryTitleVisible == 7);
  CHECK(VS::Menu::container::categoryIconsVisible == 0);
  CHECK(VS::Menu::container::controlsPanelMode == 0);
  CHECK(VS::Menu::Inventory::ashleyRenderState == 0);
  CHECK(fixture.rows[kHeader].w == 164);
  CHECK(fixture.rows[kHeader].target0 == 16);
  CHECK(fixture.rows[kSelected].target0 == 18);
  CHECK(VS::Menu::Framework::enabledStatPages == 3);
}

TEST_CASE("F02947 live selected-header alias keeps continuation write order",
          "[container-detail-helpers-live][F02947]") {
  LiveFixture fixture;
  detail::InitializeLiveDetailsLayout(static_cast<int>(kHeader),
                                      samples::kTypePage);
  CHECK(fixture.rows[kHeader].state == fw::RowState::kSlideY);
  CHECK(fixture.rows[kHeader].target0 == 18);
  CHECK(VS::Menu::Framework::enabledStatPages == 4);
}

TEST_CASE(
    "F02947 real stat-page glyphs and ordering match original helper calls",
    "[container-detail-helpers-live][container-detail-helpers-font][F02947]") {
  LiveFixture fixture;
  ValueRestore<std::uint8_t> saved_slide_step{
      VS::Menu::Framework::rowAnimationSteps[1]};
  VS::Menu::Framework::rowAnimationSteps[1] = samples::kFinalSlideStep;
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
  VS::Menu::Framework::menuMemory =
      std::bit_cast<VS::Menu::Framework::MenuMemory>(expected);
  std::span(fixture.containers.storage->container.indices.misc).back() =
      samples::kHalfwordToken;
  expected[samples::kCursorBase + samples::kMiscCategory * 2] = 0;
  expected[samples::kCursorBase + samples::kMiscCategory * 2 + 1] =
      samples::kByteMax;
  VS::Menu::Framework::menuItems = nullptr;
  CHECK(detail::SelectLiveDetailsItem(6, 255) == 65535);
  CHECK(std::bit_cast<MemoryBytes>(VS::Menu::Framework::menuMemory) ==
        expected);
  VS::Menu::container::containerData =
      &fixture.containers.replacement->container;
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
  VS::Input::buttonsPressed = VS::Input::Triangle;
  detail::ExitLiveDetailsView(1);
  CHECK(fixture.rows[kHeader].target0 == -164);
  CHECK(VS::Menu::container::detailsViewPhase == 2);
  CHECK(VS::Menu::container::detailsClosedWithTriangle ==
        static_cast<int>(VS::Input::Triangle));
  VS::Input::buttonsPressed = VS::Input::Circle;
  detail::ExitLiveDetailsView(0);
  CHECK(VS::Menu::container::detailsClosedWithTriangle == 0);
  CHECK(VS::Menu::Inventory::selectedItemCategoryIconOnTop == 1);
}
