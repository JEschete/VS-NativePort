#include <process.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Boot/OverlayReset.hpp"
#include "Core/Log.hpp"
#include "Items/ContainerGemCountsLive.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Container/ContainerCapacityLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/ContainerCapacityLiveFixture.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Platform/App.hpp"
#include "Render/GameFont.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPacketsLive.hpp"

namespace {
namespace items = VS::Items;
namespace capacity = VS::Menu::container;
namespace framework = VS::Menu::Framework;
namespace live = container_capacity_live_tests;
namespace equipment = equipment_detail_tests;
using live::LiveFixture;

struct LogScope {
  VS::Core::LogObserver previous;
  explicit LogScope(VS::Core::LogObserver observer)
      : previous(VS::Core::SetLogObserver(std::move(observer))) {}
  ~LogScope() { VS::Core::SetLogObserver(std::move(previous)); }
  LogScope(const LogScope&) = delete;
  LogScope& operator=(const LogScope&) = delete;
  LogScope(LogScope&&) = delete;
  LogScope& operator=(LogScope&&) = delete;
};

std::uint32_t GlyphWidth(char letter) {
  const auto glyph = static_cast<std::size_t>(letter - ' ');
  return (VS::Render::compactGlyphWidths[glyph / 2] >>
          ((glyph % 2) * equipment::kNibbleBits)) &
         equipment::kWidthMask;
}
}  // namespace

TEST_CASE("F02971 live emits actual compact packets, colors and palette",
          "[container-capacity-live][F02971]") {
  LiveFixture fixture;
  constexpr auto kPosition = framework::PackXy(112, 100);
  constexpr int kLimit = 8;
  constexpr int kUsed = 3;
  constexpr std::size_t kGlyphCount = 3;
  constexpr std::uint32_t kBelowHalf = 0x804020;
  capacity::DrawLiveCapacityCount(kLimit, static_cast<int>(kPosition), kUsed);
  const auto sprites = fixture.Sprites();
  REQUIRE(sprites.size() == kGlyphCount);
  CHECK(fixture.Line(100) == "3/8");
  CHECK(sprites.front().compact);
  CHECK((sprites.front().color & equipment::kRgbMask) == kBelowHalf);
  CHECK((sprites.front().texture >> framework::kHalfwordShift) ==
        equipment::kCompactClut);
  CHECK(sprites.front().position == kPosition - 32U - GlyphWidth('3'));
  constexpr std::size_t kDrawModeWords = 2;
  const auto* cursor = static_cast<const u_long*>(
      VS::Render::FrameScratchpad().primitive_cursor);
  CHECK(cursor - (fixture.primitives.data() + 1) ==
        kGlyphCount * (equipment::kCompactWords + kDrawModeWords));
}

TEST_CASE("F02971 live half equality is gray on the real overlay chain",
          "[container-capacity-live][F02971]") {
  LiveFixture fixture;
  constexpr auto kPosition = framework::PackXy(112, 100);
  constexpr int kLimit = 8;
  constexpr int kHalf = 4;
  capacity::DrawLiveCapacityCount(kLimit, static_cast<int>(kPosition), kHalf);
  const auto sprites = fixture.Sprites();
  REQUIRE_FALSE(sprites.empty());
  CHECK((sprites.front().color & equipment::kRgbMask) ==
        VS::Render::kCompactTextGray);
  CHECK(LegacyGpu_Pointer(LegacyGpu_Handle(fixture.primitives.data() + 1)) ==
        fixture.primitives.data() + 1);
}

TEST_CASE("F02972 live measures all seven owners with genuine space glyphs",
          "[container-capacity-live][F02972]") {
  LiveFixture fixture;
  for (std::size_t value = 0; value < live::kRealCategories; ++value) {
    const auto category = static_cast<items::ItemCategory>(value);
    LiveFixture::Fill(fixture.containers.storage->gadget_bag.items, category,
                      value + 1);
    LiveFixture::Fill(fixture.containers.storage->container.items, category,
                      value + 2);
  }
  capacity::DrawLiveStorageCapacities();
  constexpr std::size_t kGlyphCountIncludingSpace = 108;
  CHECK(fixture.Sprites().size() == kGlyphCountIncludingSpace);
  CHECK(fixture.Line(88) == "GADGET BAGCONTAINER");
  CHECK(fixture.Line(100) == "1/8WEAPON2/32");
  CHECK(fixture.Line(172) == "7/64MISC8/256");
  CHECK(std::ranges::all_of(
      fixture.Sprites(), [](const equipment::DrawnSprite& sprite) {
        return sprite.compact &&
               sprite.draw_mode == VS::Render::kDefaultSpriteDrawMode &&
               sprite.texture >> framework::kHalfwordShift ==
                   equipment::kCompactClut;
      }));
}

TEST_CASE(
    "F02972 live reloads the actual workspace after a render-log callback",
    "[container-capacity-live][F02972]") {
  // Render diagnostics are process-wide and report only power-of-two counts.
  const auto arguments = std::to_array<std::string>(
      {(VS::Platform::ExecutableDir() / "VS.Tests.exe").string(),
       "[.container-capacity-owner-rebind]", "--reporter", "compact"});
  const auto output =
      VS::Platform::ExecutableDir() /
      ("container-capacity-owner-rebind-" + std::to_string(_getpid()) + ".log");
  CAPTURE(output);
  const auto result = VS::Platform::RunProcess(arguments, output);
  REQUIRE(result.has_value());
  CHECK(*result == 0);
}

TEST_CASE("F02972 isolated callback retains the original workspace and packets",
          "[.container-capacity-owner-rebind]") {
  LiveFixture fixture;
  auto& next = *fixture.containers.replacement;
  std::span(next.gadget_bag.items.gems).back().id = 1;
  std::span(next.container.items.blades).back().id = 1;
  const auto before = VS::Render::DroppedLiveUiPacketCount();
  bool replaced = false;
  LogScope observer([&](const VS::Core::LogRecord& record) {
    if (record.channel == "render" && !replaced) {
      replaced = true;
      VS::Menu::Framework::inventoryStorage = &next;
      VS::Render::legacyPrimitiveBuffers[0] = fixture.primitives.data() + 1;
    }
  });
  VS::Render::legacyPrimitiveBuffers[0] = nullptr;
  capacity::DrawLiveStorageCapacities();
  CHECK(replaced);
  CHECK(VS::Render::DroppedLiveUiPacketCount() > before);
  CHECK(fixture.Line(112) == "0/16BLADE1/64");
  CHECK(fixture.Line(160) == "1/48GEM0/192");
}

TEST_CASE(
    "F02973 live uses current last-slot records and production stat helpers",
    "[container-capacity-live][F02973]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  LiveFixture::AddWeapon(owner);
  CHECK(items::CountLiveContainerWeaponGems(live::kLastWeapon) == 2);
  VS::Items::WeaponLoadout loadout{};
  capacity::BuildLiveWeaponStats(&loadout, &std::span(owner.weapons).back(),
                                 &owner);
  constexpr int kExpectedStrength = 19;
  CHECK(loadout.attributes.current_strength == kExpectedStrength);
  auto& next = fixture.containers.replacement->container;
  next.items.weapons[live::kLastWeapon].grip = live::kLastPart + 1;
  VS::Menu::container::containerData = &next;
  CHECK(items::CountLiveContainerWeaponGems(live::kLastWeapon) == 0);
}

TEST_CASE("F02974 live respects unsigned fourth references and changed owners",
          "[container-capacity-live][F02974]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  constexpr std::uint8_t kFourSlots = 4;
  constexpr std::uint8_t kFlagOnly = 128;
  constexpr std::uint8_t kHighReference = 255;
  std::span(owner.shields).back() =
      items::InventoryShield{.base = {.gem_slots = kFourSlots},
                             .gems = {0, 1, kFlagOnly, kHighReference}};
  CHECK(items::CountLiveContainerShieldGems(live::kLastWeapon) ==
        kFourSlots - 1);
  auto& next = fixture.containers.replacement->gadget_bag;
  next.items.shields[live::kLastWeapon].base.gem_slots = 1;
  next.items.shields[live::kLastWeapon].gems[0] = 1;
  VS::Menu::container::containerData = &next;
  CHECK(items::CountLiveContainerShieldGems(live::kLastWeapon) == 1);
}

TEST_CASE("F02975 live measures stock receiving equality in every category",
          "[container-capacity-live][F02975]") {
  LiveFixture fixture;
  auto& target = fixture.containers.storage->gadget_bag.items;
  for (std::size_t value = 0; value < live::kRealCategories; ++value) {
    const auto category = static_cast<items::ItemCategory>(value);
    const auto limit = items::kCategoryCapacities.at(value);
    LiveFixture::Fill(target, category, limit - 1);
    CHECK(capacity::WouldExceedLiveCapacity(static_cast<int>(value), 1) == 0);
    CHECK(capacity::WouldExceedLiveCapacity(static_cast<int>(value), 2) == 1);
  }
}

TEST_CASE("F02975 live uses nonzero direction, full gems and staging",
          "[container-capacity-live][F02975]") {
  LiveFixture fixture;
  auto& target = fixture.containers.storage->gadget_bag.items;
  constexpr int kGemCategory = static_cast<int>(items::ItemCategory::kGem);
  constexpr u_char kNonzeroDirection = 255;
  VS::Menu::container::browsingGadgetBag = kNonzeroDirection;
  LiveFixture::Fill(target, items::ItemCategory::kGem, live::kLastGem);
  CHECK(capacity::WouldExceedLiveCapacity(kGemCategory, 1) == 0);
  fixture.containers.storage->staged_transfer.gems[0].id = 1;
  CHECK(capacity::WouldExceedLiveCapacity(kGemCategory, 1) == 1);
  VS::Menu::container::otherStorage =
      &fixture.containers.replacement->gadget_bag;
  CHECK(capacity::WouldExceedLiveCapacity(kGemCategory, 1) == 0);
}

TEST_CASE(
    "F02975 live empty capacities never dereference missing record owners",
    "[container-capacity-live][F02975]") {
  LiveFixture fixture;
  VS::Menu::container::containerItemCapacities.fill(0);
  VS::Menu::container::otherStorage = nullptr;
  VS::Menu::Framework::inventoryStorage = nullptr;
  VS::Menu::container::browsingGadgetBag = 1;
  constexpr int kMisc = static_cast<int>(items::ItemCategory::kMisc);
  CHECK(capacity::WouldExceedLiveCapacity(kMisc, 0) == 0);
  CHECK(capacity::WouldExceedLiveCapacity(kMisc, 1) == 1);
}

TEST_CASE("F02976 live checks every mandatory weapon component capacity",
          "[container-capacity-live][F02976]") {
  constexpr std::array kCategories{
      items::ItemCategory::kWeapon, items::ItemCategory::kBlade,
      items::ItemCategory::kGrip, items::ItemCategory::kGem};
  for (const auto blocked : kCategories) {
    LiveFixture fixture;
    LiveFixture::AddWeapon(fixture.containers.storage->container.items);
    const auto value = static_cast<std::size_t>(blocked);
    LiveFixture::Fill(fixture.containers.storage->gadget_bag.items, blocked,
                      items::kCategoryCapacities.at(value));
    CHECK(capacity::LiveTransferCapacityError(
              static_cast<int>(items::ItemCategory::kWeapon),
              live::kLastWeapon) == 2);
  }
}

TEST_CASE("F02976 live reaches exactly 64 pending halfwords and no source read",
          "[container-capacity-live][F02976]") {
  LiveFixture fixture;
  auto pending = std::span(fixture.containers.storage->items_to_transfer);
  constexpr std::uint16_t kOpaqueToken = 0xFFFF;
  std::ranges::fill(pending, kOpaqueToken);
  VS::Menu::container::containerData = nullptr;
  constexpr int kGem = static_cast<int>(items::ItemCategory::kGem);
  CHECK(capacity::LiveTransferCapacityError(kGem, -1) == 1);
  pending.back() = 0;
  CHECK(capacity::LiveTransferCapacityError(kGem, -1) == 0);
  CHECK(pending.front() == kOpaqueToken);
}

TEST_CASE("F02976 live shield attached gems enforce real receiving equality",
          "[container-capacity-live][F02976]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  auto& shield = owner.shields[live::kLastWeapon];
  shield.base.gem_slots = items::kWeaponGemCount;
  shield.gems[0] = 1;
  shield.gems[2] = 1;
  constexpr int kShield = static_cast<int>(items::ItemCategory::kShield);
  constexpr auto kGem = static_cast<std::size_t>(items::ItemCategory::kGem);
  LiveFixture::Fill(fixture.containers.storage->gadget_bag.items,
                    items::ItemCategory::kGem,
                    items::kCategoryCapacities.at(kGem) - 2);
  CHECK(capacity::LiveTransferCapacityError(kShield, live::kLastWeapon) == 0);
  fixture.containers.storage->staged_transfer.gems[0].id = 1;
  CHECK(capacity::LiveTransferCapacityError(kShield, live::kLastWeapon) == 2);
}

TEST_CASE("F02977 live restores an equipment tree and production loadout stats",
          "[container-capacity-live][F02977]") {
  LiveFixture fixture;
  auto& storage = *fixture.containers.storage;
  LiveFixture::AddWeapon(storage.staged_transfer);
  constexpr std::uint16_t kLastWeaponToken = live::kLastWeapon + 1;
  std::span(storage.items_to_transfer).back() = kLastWeaponToken;
  constexpr int kLastRow = 63;
  capacity::UndoLivePendingTransfer(kLastRow);
  VS::Items::WeaponLoadout loadout{};
  capacity::BuildLiveWeaponStats(&loadout, &storage.container.items.weapons[0],
                                 &storage.container.items);
  constexpr int kExpectedStrength = 19;
  CHECK(loadout.attributes.current_strength == kExpectedStrength);
  CHECK(storage.staged_transfer.weapons[live::kLastWeapon].blade == 0);
  CHECK(storage.staged_transfer.grips[live::kLastPart].id == 0);
  CHECK(storage.staged_transfer.gems[live::kLastGem].id == 0);
  CHECK(std::span(storage.items_to_transfer).back() == kLastWeaponToken);
}

TEST_CASE(
    "F02977 live decodes a last-gem token and clears after full-target copy",
    "[container-capacity-live][F02977]") {
  LiveFixture fixture;
  auto& storage = *fixture.containers.storage;
  constexpr std::uint16_t kLastGemToken = 0x5C0;
  storage.items_to_transfer[0] = kLastGemToken;
  storage.staged_transfer.gems[live::kLastGem].id = 2;
  LiveFixture::Fill(storage.container.items, items::ItemCategory::kGem,
                    items::kContainerCapacities.at(
                        static_cast<std::size_t>(items::ItemCategory::kGem)));
  capacity::UndoLivePendingTransfer(0);
  CHECK(storage.container.items.gems[0].id == 1);
  CHECK(storage.staged_transfer.gems[live::kLastGem].id == 0);
  CHECK(storage.staged_transfer.gems[live::kLastGem].index ==
        live::kLastGem + 1);
  CHECK(storage.items_to_transfer[0] == kLastGemToken);
}

TEST_CASE("F02978 live stages native-width parts and clears the source tree",
          "[container-capacity-live][F02978]") {
  LiveFixture fixture;
  auto& storage = *fixture.containers.storage;
  LiveFixture::AddWeapon(storage.container.items);
  constexpr int kWeapon = static_cast<int>(items::ItemCategory::kWeapon);
  capacity::StageLiveItemTransfer(kWeapon, live::kLastWeapon);
  CHECK(storage.staged_transfer.weapons[0].blade == 1);
  CHECK(storage.staged_transfer.weapons[0].grip == 1);
  CHECK(storage.staged_transfer.weapons[0].gems[2] == 2);
  CHECK(storage.staged_transfer.gems[1].set_item_index == 1);
  CHECK(storage.container.items.weapons[live::kLastWeapon].blade == 0);
  CHECK(storage.container.items.grips[live::kLastPart].id == 0);
  CHECK(storage.items_to_transfer[0] == 0);
}

TEST_CASE("F02978 live honors current active owners and full staging mutations",
          "[container-capacity-live][F02978]") {
  LiveFixture fixture;
  auto& storage = *fixture.containers.storage;
  constexpr int kGem = static_cast<int>(items::ItemCategory::kGem);
  storage.container.items.gems[live::kLastGem].id = 2;
  LiveFixture::Fill(storage.staged_transfer, items::ItemCategory::kGem,
                    items::kContainerCapacities.at(
                        static_cast<std::size_t>(items::ItemCategory::kGem)));
  VS::Menu::container::otherStorage = &storage.container;
  capacity::StageLiveItemTransfer(kGem, live::kLastGem);
  CHECK(storage.container.items.gems[live::kLastGem].id == 0);
  auto& next = fixture.containers.replacement->container;
  next.items.gems[live::kLastGem].id = 2;
  VS::Menu::container::containerData = &next;
  capacity::StageLiveItemTransfer(kGem, live::kLastGem);
  CHECK(next.items.gems[live::kLastGem].id == 0);
  CHECK(storage.staged_transfer.gems[0].id == 1);
}

TEST_CASE(
    "Ordinary MENUD reload retains capacity/staging BSS without a new hook",
    "[container-capacity-live][container-capacity-reload]"
    "[F02971][F02972][F02973][F02974][F02975][F02976][F02977][F02978]") {
  LiveFixture fixture;
  auto& storage = *fixture.containers.storage;
  constexpr std::uint16_t kToken = 0x5C0;
  storage.items_to_transfer[0] = kToken;
  storage.staged_transfer.gems[live::kLastGem].id = 2;
  VS::Menu::container::browsingGadgetBag = 2;
  VS::Menu::container::containerItemCapacities[0] = 0;
  VS::Boot::ResetMenudStatics();
  CHECK(VS::Menu::container::containerData == &storage.container);
  CHECK(VS::Menu::container::otherStorage == &storage.gadget_bag);
  CHECK(VS::Menu::Framework::inventoryStorage == &storage);
  CHECK(VS::Menu::container::browsingGadgetBag == 2);
  CHECK(storage.items_to_transfer[0] == kToken);
  CHECK(storage.staged_transfer.gems[live::kLastGem].id == 2);
  CHECK(VS::Menu::container::containerItemCapacities[0] ==
        items::kContainerCapacities.front());
}
