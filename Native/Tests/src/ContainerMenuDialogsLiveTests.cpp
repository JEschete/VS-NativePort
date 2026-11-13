#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Menu/ContainerMenuDialogsLiveFixture.hpp"

namespace {
using LiveFixture = container_dialog_live_tests::DialogLiveFixture;
namespace dialogs = VS::Menu::container;
namespace framework = VS::Menu::Framework;
namespace input = VS::Input;
constexpr std::uint8_t kMaximumQuantity = 255;
constexpr std::uint8_t kSmallStack = 10;
constexpr char kReplacementStack = 20;
constexpr std::uint16_t kReplacementItem = 322;
constexpr std::size_t kTextPointers = 4;
constexpr int kContainerListInfo = 81 | 0x19200;
constexpr auto kQuantityPositions = std::to_array<std::uint32_t>({
    54,
    60,
    66,
    72,
    78,
    84,
    90,
});
constexpr auto kForegroundPositions = std::to_array<std::uint32_t>({
    54,
    60,
    66,
});
constexpr std::uint32_t kQuantityGlyphSize = 0x000C000C;
constexpr std::uint32_t kQuantityGlyphMode = 0xE100002D;

struct QuantityPackets {
  std::vector<std::uint32_t> positions;
  std::vector<std::uint32_t> foreground_textures;
  bool shapes_valid = true;
};

QuantityPackets ReadQuantityPackets(const LiveFixture& fixture) {
  QuantityPackets result;
  for (const auto& packet : fixture.Packets()) {
    if (packet.size() != VS::Render::kSpritePacketWords ||
        static_cast<std::int16_t>(packet[VS::Render::kSpritePositionWord] >>
                                  framework::kHalfwordShift) !=
            container_dialog_live_tests::kQuantityY) {
      continue;
    }
    const auto x =
        packet[VS::Render::kSpritePositionWord] & framework::kHalfwordMask;
    result.positions.push_back(x);
    result.shapes_valid &= packet[0] >> VS::Render::kPacketLengthShift ==
                               VS::Render::kSpritePacketWords - 1 &&
                           packet[VS::Render::kSpriteTextureWord] != 0;
    if (std::ranges::find(kForegroundPositions, x) !=
        kForegroundPositions.end()) {
      result.foreground_textures.push_back(
          packet[VS::Render::kSpriteTextureWord]);
      result.shapes_valid &=
          packet[VS::Render::kSpriteSizeWord] == kQuantityGlyphSize &&
          packet[VS::Render::kSpriteDrawModeWord] == kQuantityGlyphMode;
    }
  }
  std::ranges::sort(result.positions);
  std::ranges::sort(result.foreground_textures);
  return result;
}
}  // namespace

TEST_CASE("F02969 live uses actual controller edges and halfword misc slot 256",
          "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container;
  fixture.OpenMiscSort();
  REQUIRE(fixture.Frame(dialogs::UpdateLiveSortMenu, input::Down) == 0);
  REQUIRE((Input_buttonsPressed.allButtons & input::Down) != 0);
  fixture.Frame(dialogs::UpdateLiveSortMenu);
  REQUIRE(fixture.Frame(dialogs::UpdateLiveSortMenu, input::Circle) == 2);
  REQUIRE(owner.indices.misc[0] == 256);
  REQUIRE(owner.indices.misc[1] == 1);
}

TEST_CASE(
    "F02970 live advances real information text and preserves encoded spacing",
    "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  fixture.OpenQuantity(kMaximumQuantity);
  const auto& row = fixture.rows[container_dialog_live_tests::kQuantityRow];
  REQUIRE(Menu_textBoxes[7].flags.done != 0);
  REQUIRE(static_cast<std::uint8_t>(row.text[0]) == VS::Render::kTextSpacing);
  REQUIRE(static_cast<std::uint8_t>(row.text[1]) == 48);
  REQUIRE(static_cast<std::uint8_t>(row.text[2]) ==
          VS::Render::kTextSpaceGlyph);
  REQUIRE(static_cast<std::uint8_t>(row.text[5]) == 0xA1);
  REQUIRE(static_cast<std::uint8_t>(row.text[6]) == 2);
  REQUIRE(static_cast<std::uint8_t>(row.text[7]) == 5);
  REQUIRE(static_cast<std::uint8_t>(row.text[8]) == 5);
}

TEST_CASE(
    "F02970 live emits bounded six-word glyphs on the real ordering chain",
    "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  fixture.OpenQuantity(kMaximumQuantity);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  const auto glyphs = ReadQuantityPackets(fixture);
  REQUIRE(glyphs.shapes_valid);
  REQUIRE(glyphs.positions ==
          std::vector<std::uint32_t>(kQuantityPositions.begin(),
                                     kQuantityPositions.end()));
  REQUIRE(glyphs.foreground_textures ==
          std::vector<std::uint32_t>{0x37B90000, 0x37B90000, 0x37B9000C});
}

TEST_CASE(
    "F02970 live confirms a real selected quantity and cancels without "
    "mutation",
    "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  fixture.OpenQuantity(kMaximumQuantity);
  auto& stack =
      std::span(fixture.containers.storage->container.items.misc).back();
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  REQUIRE(fixture.Frame(dialogs::UpdateLiveDiscardMenu,
                        input::Triangle | input::Circle | input::Cross) == -2);
  REQUIRE(static_cast<std::uint8_t>(stack.count) == 255);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  REQUIRE(fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Circle) == 0);
  fixture.RunFrames(dialogs::UpdateLiveDiscardMenu);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  REQUIRE(fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Circle) == 1);
  REQUIRE(static_cast<std::uint8_t>(stack.count) == 254);
  REQUIRE(stack.id == 321);
}

TEST_CASE(
    "F02983 live builds production list rows before restoring their cursor",
    "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  const auto help =
      std::span(reinterpret_cast<std::uint8_t*>(Menu_Framework_itemHelp),
                framework::kItemHelpBytes);
  const auto name = framework::ItemHelpEntry(help, 552);
  const auto description = framework::ItemHelpEntry(help, 554);
  REQUIRE(name.has_value());
  REQUIRE(description.has_value());
  if (!name.has_value() || !description.has_value()) return;
  auto* const name_text = reinterpret_cast<char*>(help.data() + name->offset);
  auto* const description_text =
      reinterpret_cast<char*>(help.data() + description->offset);
  std::array<char*, kTextPointers> texts{name_text, description_text, name_text,
                                         description_text};
  std::array<int, 2> types{};
  framework::CreateLiveMenuRows(2, kContainerListInfo, texts.data(),
                                types.data());
  dialogs::RestoreLiveItemRows(0);
  REQUIRE(static_cast<std::uint8_t>(Menu_Framework_itemsListRow) >= 16);
  REQUIRE(fixture.rows[20].state == Menu_Framework_MenuItemState_Idle);
  REQUIRE(fixture.rows[20].x == fixture.rows[20].targetPosition0);
  REQUIRE(fixture.rows[20].selected == 1);
  REQUIRE(fixture.rows[21].selected == 0);
}

TEST_CASE(
    "F02984 live builds real prompts and waits for actual flyout completion",
    "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  REQUIRE(dialogs::UpdateLiveTransferConfirmation(1) == 0);
  REQUIRE(Menu_Container_controlsPanelMode == 5);
  REQUIRE(Menu_Inventory_freezeTabArrows == 0);
  fixture.RunFrames(dialogs::UpdateLiveTransferConfirmation);
  REQUIRE(fixture.Frame(dialogs::UpdateLiveTransferConfirmation,
                        input::Circle) == 0);
  REQUIRE(fixture.RunFrames(dialogs::UpdateLiveTransferConfirmation) == 1);
  REQUIRE(Menu_Container_categoryTitleVisible == 0);
  REQUIRE(Menu_Container_categoryIconsVisible == 0);
}

TEST_CASE(
    "Ordinary MENUD load retains active sort BSS but resets presentation data",
    "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container;
  fixture.OpenMiscSort();
  fixture.Frame(dialogs::UpdateLiveSortMenu, input::Down);
  fixture.Frame(dialogs::UpdateLiveSortMenu);
  Boot_ResetMenudStatics();
  REQUIRE(fixture.Frame(dialogs::UpdateLiveSortMenu, input::Circle) == 2);
  REQUIRE(owner.indices.misc[0] == 256);
}

TEST_CASE("F02970 production-equipped gadget-bag weapons cannot select discard",
          "[container-menu-dialogs-live]") {
  container_dialog_live_tests::EquippedFixture fixture;
  fixture.EquipThroughProductionMenu();
  REQUIRE(Audio_Seq_unusedInventory.weapons[0].isEquipped != 0);
  fixture.OpenEquippedContainerActions();
  const auto before = VS::Items::Live::Typed(
      fixture.containers.storage->gadgetBag.items.weapons[0]);
  REQUIRE(before.is_equipped != 0);
  REQUIRE(fixture.rows[33].unselectable != 0);
  fixture.Frame(Menu_Container_UpdateItemList, input::Down);
  fixture.Frame(Menu_Container_UpdateItemList);
  REQUIRE(fixture.Frame(Menu_Container_UpdateItemList, input::Circle) == 0);
  REQUIRE(framework::LiveActionSelectionResult() == -1);
  REQUIRE(VS::Items::Live::Typed(
              fixture.containers.storage->gadgetBag.items.weapons[0]) ==
          before);
}

TEST_CASE(
    "F02970 live resolves the current owner only when confirming the discard",
    "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  fixture.OpenQuantity(kSmallStack);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Circle);
  fixture.RunFrames(dialogs::UpdateLiveDiscardMenu);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  auto& replacement = fixture.containers.replacement->container;
  std::span(replacement.items.misc).back() = {
      .id = kReplacementItem, .count = kReplacementStack, .index = 0};
  Menu_Container_containerData = &replacement;
  REQUIRE(fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Circle) == 1);
  REQUIRE(std::span(replacement.items.misc).back().count == 19);
  REQUIRE(std::span(fixture.containers.storage->container.items.misc)
              .back()
              .count == 10);
}

TEST_CASE(
    "Ordinary MENUD reload retains quantity selection and confirmation BSS",
    "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  fixture.OpenQuantity(kSmallStack);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  Boot_ResetMenudStatics();
  REQUIRE(fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Circle) == 0);
  REQUIRE(fixture.rows[34].text[4] == 1);
  REQUIRE(fixture.rows[34].text[6] == 0);
  fixture.RunFrames(dialogs::UpdateLiveDiscardMenu);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Up);
  fixture.Frame(dialogs::UpdateLiveDiscardMenu);
  REQUIRE(fixture.Frame(dialogs::UpdateLiveDiscardMenu, input::Circle) == 1);
  REQUIRE(std::span(fixture.containers.storage->container.items.misc)
              .back()
              .count == 9);
}

TEST_CASE("F02984 live cancellation retains the full triangle result",
          "[container-menu-dialogs-live]") {
  LiveFixture fixture;
  REQUIRE(dialogs::UpdateLiveTransferConfirmation(1) == 0);
  fixture.RunFrames(dialogs::UpdateLiveTransferConfirmation);
  REQUIRE(fixture.Frame(dialogs::UpdateLiveTransferConfirmation,
                        input::Triangle) == 0);
  REQUIRE(fixture.RunFrames(dialogs::UpdateLiveTransferConfirmation) == -2);
}
