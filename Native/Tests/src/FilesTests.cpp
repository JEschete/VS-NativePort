#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Items/ItemRecords.hpp"
#include "Menu/Files.hpp"
#include "Menu/FilesLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Render/UiPackets.hpp"

namespace {
namespace files = VS::Menu::Files;
namespace framework = VS::Menu::Framework;
using framework::PackXy;
using VS::Menu::MenuSound;

constexpr std::uint32_t kCircle = 0x20;
constexpr std::uint32_t kCross = 0x40;
constexpr std::uint32_t kDown = 0x4000;
constexpr std::uint32_t Tag(std::uint32_t words) {
  return (words - 1) << VS::Render::kPacketLengthShift;
}

class FakeQueue final : public VS::Render::PacketQueue {
 public:
  std::uint32_t ChainLink() const override { return 0; }
  bool Emit(std::span<const std::uint32_t> /*packet*/) override { return true; }
};

class FakeTarget final : public VS::Render::ImmediatePacketTarget {
 public:
  std::vector<std::vector<std::uint32_t>> packets;
  void Submit(std::span<const std::uint32_t> packet) override {
    packets.emplace_back(packet.begin(), packet.end());
  }
};

// The packet target and the memory-card driver both overlays' hosts share:
// a text table whose accessing and option entries point past the index, and
// scripted readiness events.
template <typename Base>
class FakeCard : public Base {
 public:
  std::vector<std::vector<std::uint32_t>> packets;
  std::vector<std::string> refusals;
  std::vector<std::uint16_t> table = std::vector<std::uint16_t>(200);
  std::vector<std::optional<std::uint16_t>> card_messages;
  std::deque<int> events;
  std::vector<int> started_ports;
  std::vector<int> scanned_ports;
  int found_slot = 0;
  framework::Buttons buttons{};

  FakeCard() {
    table[files::kIndexAccessing0] = 100;
    table[files::kIndexAccessing0 + 1] = 110;
    table[files::kIndexYesOption] = 140;
    table[files::kIndexYesOption + 1] = 160;
  }

  void Submit(std::span<const std::uint32_t> packet) override {
    packets.emplace_back(packet.begin(), packet.end());
  }
  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  std::span<const std::uint16_t> TextTable() override { return table; }
  void SetCardMessage(std::optional<std::uint16_t> offset) override {
    card_messages.push_back(offset);
  }
  int AdvanceCardReadiness(int port) override {
    if (port != 0) {
      started_ports.push_back(port);
      return 0;
    }
    if (events.empty()) {
      return 0;
    }
    const int event = events.front();
    events.pop_front();
    return event;
  }
  int ScanSaveDirectory(int port) override {
    scanned_ports.push_back(port);
    return 0;
  }
  int FindMatchingSaveSlot() override { return found_slot; }
  framework::Buttons Input() override { return buttons; }
};

class FakeHost final : public FakeCard<files::Host> {
 public:
  FakeQueue queue;
  std::array<framework::MenuRow, framework::kNoRow> rows{};
  std::array<std::uint8_t, files::kSettingsBytes> settings{};
  std::vector<std::uint16_t> info_messages;
  std::int32_t cursor = -1;
  std::int32_t progress = -1;
  std::uint16_t transfer_target = 0;

  VS::Render::PacketQueue& OverlayQueue() override { return queue; }
  void UploadImage(std::int32_t, std::span<const std::uint16_t>,
                   std::int32_t) override {}
  void ReadImage(std::int32_t, std::span<std::uint16_t>,
                 std::int32_t) override {}
  void StoreImage(std::int32_t, std::span<std::uint16_t>,
                  std::int32_t) override {}
  void DrawSync() override {}
  void DrawFileOverlay() override {}
  void DrawCompactText(std::string_view, std::uint32_t) override {}
  std::uint32_t InterfaceRandomBits() override { return 0; }
  std::span<const std::uint16_t> LocationPalette(int) override { return {}; }
  void SetDisplayMask(bool) override {}
  void LoadClutColors(std::span<const std::uint16_t>, int, int) override {}
  void RetireTexturePages(std::uint32_t, std::uint32_t, int) override {}
  int FrameBuffer() override { return 0; }
  void ClearScreen() override {}
  std::span<const std::uint16_t> ContainerStrings() override { return {}; }
  bool CardMessageIs(std::uint16_t) override { return false; }
  std::span<const std::uint8_t> CardMessage() override { return {}; }
  std::int32_t CursorPosition() override { return cursor; }
  void SetCursorPosition(std::int32_t position) override { cursor = position; }
  std::uint8_t DataNotSaved() override { return 0; }
  void SetDataNotSaved(std::uint8_t) override {}
  std::int32_t ProgressCounter() override { return progress; }
  void SetProgressCounter(std::int32_t counter) override { progress = counter; }
  files::TransferProgress Transfer() override { return {}; }
  void SetTransferTarget(std::uint16_t target) override {
    transfer_target = target;
  }
  files::SlotSummary Summary(int) override { return {}; }
  void ZeroSummary(int) override {}
  void SetSummaryStatus(int, std::uint32_t) override {}
  void LoadWrittenSummary(int) override {}
  void ClearCardDirectory() override {}
  int NewestSaveSlot() override { return 0; }
  int CreateSaveFile(int, int) override { return 0; }
  int FormatCard(int) override { return 0; }
  int WriteSaveStep(int) override { return 0; }
  int ReadSaveStep(int) override { return 0; }
  int ValidateAndApplyLoadedSave(int) override { return 0; }
  void PackageGameSave(int) override {}
  void StashSaveImage() override {}
  void RestoreSaveImage() override {}
  std::span<std::uint8_t> Settings() override { return settings; }
  void ClearDoorEntered() override {}
  std::uint32_t PlayTime() override { return 0; }
  void SetPlayTime(std::uint32_t) override {}
  void PlaySound(MenuSound) override {}
  void PlaySoundEffect(int) override {}
  framework::MenuRow& SetMenuItem(const framework::RowPlacement& placement,
                                  std::span<const std::uint8_t>) override {
    return rows.at(static_cast<std::size_t>(placement.row));
  }
  framework::MenuRow& MenuItem(int row) override {
    return rows.at(static_cast<std::size_t>(row));
  }
  bool RowsIdle() override { return true; }
  void ClearMenuExcept(int) override {}
  int DrawMainMenuCursor(int step, int) override { return step; }
  void CreateMenuRows(std::span<const std::span<const std::uint8_t>>,
                      std::span<int>, int) override {}
  int SelectedRow() override { return 0; }
  int HighlightedRow() override { return 0; }
  void ClearCommandLabel() override {}
  void DismissInformationBox() override {}
  void SetBackgroundFade(int) override {}
  int BackgroundFadeStep() override { return 0; }
  void InitInformationBox(framework::InformationHeader) override {}
  void SetInformationBoxState(int) override {}
  void SetInformationMessage(std::uint16_t offset) override {
    info_messages.push_back(offset);
  }
  int CurrentSavePointId() override { return 0; }
  int SaveLocationId() override { return 0; }
  void SetSaveLocationId(int) override {}
  files::GameFlags Flags() override { return {}; }
  void SetGameOver(bool) override {}
  void SetDifficulty(std::uint8_t) override {}
  std::uint8_t CursorMemory() override { return 0; }
  void SetCursorMemory(std::uint8_t) override {}
  std::uint8_t MonoSound() override { return 0; }
  int UpdateDataSaveMenu(int) override { return 0; }
  int UpdateDataLoadMenu(int) override { return 0; }
  int StepMenuResources(int) override { return 0; }
  void ReleaseMenuResources() override {}
  void EnterBattle() override {}
  void ResetToTitle() override {}
  void SetFieldRendering(bool) override {}
  void FreeMusicSlot(int) override {}
  void FadeMusicSlotVolume(int, int, int) override {}
  void SetMonoOutput(int) override {}
  int LoadIntroSnapshotStep(int) override { return 0; }
  void SetCurrentMenu(int) override {}
  void BeginGameOverLoad() override {}
  std::optional<bool> GameOverLoaded() override { return std::nullopt; }
  std::span<const std::uint16_t> FinishGameOverLoad() override { return {}; }
  void FreeGameOverImage() override {}
};

class FakeTitleHost final : public FakeCard<files::TitleHost> {
 public:
  struct ElementInit {
    int id = 0;
    std::uint32_t position = 0;
    std::uint32_t size = 0;
    std::uint16_t text_offset = 0;
  };

  std::uint8_t frame_buffer = 7;
  std::vector<std::pair<int, int>> displays;
  int clears = 0;
  std::optional<bool> display_mask;
  std::vector<ElementInit> inits;
  std::vector<std::pair<int, std::int16_t>> animations;
  std::vector<std::pair<int, bool>> selections;
  bool animations_complete = true;
  std::int32_t cursor = -1;
  std::vector<MenuSound> sounds;

  std::uint8_t FrameBuffer() override { return frame_buffer; }
  void SetFrameBuffer(std::uint8_t buffer) override { frame_buffer = buffer; }
  void PutDisplay(int display_x, int draw_x) override {
    displays.emplace_back(display_x, draw_x);
  }
  void ClearScreen() override { ++clears; }
  void SetDisplayMask(bool shown) override { display_mask = shown; }
  void InitializeElement(int id, std::uint32_t position, std::uint32_t size,
                         std::uint16_t text_offset) override {
    inits.push_back({id, position, size, text_offset});
  }
  void AnimateElement(int id, files::ElementState state,
                      std::int16_t target) override {
    CHECK(state == files::ElementState::kAnimateX);
    animations.emplace_back(id, target);
  }
  void SetElementSelected(int id, bool selected) override {
    selections.emplace_back(id, selected);
  }
  bool AnimationsComplete() override { return animations_complete; }
  void SetCursorPosition(std::int32_t position) override { cursor = position; }
  void PlaySound(MenuSound sound) override { sounds.push_back(sound); }
  void PlaySoundEffect(int /*effect*/) override {}
};

struct FilesFixture {
  files::State state{};
  FakeHost host;
  files::Context ctx{.state = state, .host = host};
};

struct TitleFixture {
  files::State state{};
  FakeTitleHost host;
  files::TitleContext ctx{.state = state, .host = host};

  // Steps the save search until it settles.
  int Search() {
    files::FindCurrentSaveStep(ctx, true);
    int result = 0;
    for (int frame = 0; frame < 16 && result == 0; ++frame) {
      result = files::FindCurrentSaveStep(ctx, false);
    }
    return result;
  }

  // Slides both reminder options in and waits for them.
  void OpenReminder() {
    files::UpdateClearSaveReminder(ctx, true);
    for (int frame = 0; frame < 3; ++frame) {
      files::UpdateClearSaveReminder(ctx, false);
    }
  }

  int Press(std::uint32_t pressed, std::uint32_t repeat = 0) {
    host.buttons = {.pressed = pressed, .repeat = repeat};
    return files::UpdateClearSaveReminder(ctx, false);
  }
};
}  // namespace

TEST_CASE("Files digit counts follow the original", "[menu-files][menu]") {
  CHECK(files::DecimalDigitCount(0) == 1);
  CHECK(files::DecimalDigitCount(9) == 1);
  CHECK(files::DecimalDigitCount(10) == 2);
  CHECK(files::DecimalDigitCount(999) == 3);
  CHECK(files::DecimalDigitCount(1000) == 4);
  CHECK(files::DecimalDigitCount(-15) == 2);
}

TEST_CASE("Files container sections follow the original",
          "[menu-files][menu]") {
  CHECK(files::ContainerIndexSection(-1) == 0);
  CHECK(files::ContainerIndexSection(0) == 0);
  CHECK(files::ContainerIndexSection(1) == 32);
  CHECK(files::ContainerIndexSection(3) == 160);
  CHECK(files::ContainerIndexSection(6) == 448);
  CHECK(files::ContainerIndexSection(7) == 704);
  CHECK(files::ContainerIndexSection(8) == 706);
  CHECK(files::ContainerIndexSection(100) == 706);
}

TEST_CASE("Files element colours blend the box gradients",
          "[menu-files][menu]") {
  CHECK(files::ElementBackgroundColor(0, 0) == 0x6B4100U);
  CHECK(files::ElementBackgroundColor(8, 0) == 0x330500U);
  CHECK(files::ElementBackgroundColor(4, 0) == 0x4F2300U);
  CHECK(files::ElementBackgroundColor(0, 8) == 0x6C8219U);
  CHECK(files::ElementBackgroundColor(8, 16) == 0x200808U);
  CHECK(files::ElementBackgroundColor(0, 24) == 0x203840U);
  CHECK_FALSE(files::ElementBackgroundColor(0, 9).has_value());
  CHECK_FALSE(files::ElementBackgroundColor(0, 32).has_value());
}

TEST_CASE("Files game-over text clut blends between two palettes",
          "[menu-files][menu]") {
  std::array<std::uint16_t, files::kClutColors> from{};
  std::array<std::uint16_t, files::kClutColors> to{};
  from.fill(0x0000);
  to.fill(0x7FFF);
  from[0] = 0x1234;
  std::array<std::uint16_t, files::kClutColors> clut{};
  files::BlendGameOverTextClut(clut, 0, from, to);
  CHECK(clut[0] == 0);
  CHECK(clut[1] == 0x8000);
  files::BlendGameOverTextClut(clut, 16, from, to);
  CHECK(clut[0] == 0);
  CHECK(clut[15] == 0xFFFF);
  files::BlendGameOverTextClut(clut, 8, from, to);
  CHECK(clut[1] == (0x8000 | 0xF | (0xF << 5) | (0xF << 10)));
  from[2] = 0x001F;
  to[2] = 0x0000;
  files::BlendGameOverTextClut(clut, 8, from, to);
  // 31 * 16 - 31 * 8 = 248 keeps only 0xF0 after the channel mask.
  CHECK((clut[2] & 0x1F) == 0xF);
}

TEST_CASE("Files glyphs advance by their widths and skip spacing pages",
          "[menu-files][menu]") {
  FakeTarget target;
  CHECK(files::DrawGlyph(target, 0x0E05, 10, 20, 0) == 15);
  CHECK(files::DrawGlyph(target, 377, 10, 20, 0) == 10);
  CHECK(files::DrawGlyph(target, 0x8F, 10, 20, 0) == 16);
  CHECK(target.packets.empty());
  CHECK(files::DrawGlyph(target, 1, 10, 20, 0) == 16);
  CHECK(files::DrawGlyph(target, 62, 10, 20, 1) == 22);
  REQUIRE(target.packets.size() == 2);
  CHECK(target.packets[1] == std::vector<std::uint32_t>{
                                 Tag(6), 0xE100002D, 0x66808080, PackXy(10, 20),
                                 framework::PackUvClut(240, 24, 912, 222),
                                 PackXy(12, 12)});
}

TEST_CASE("Files glyph widths cover the whole original sheet",
          "[menu-files][menu]") {
  FakeTarget target;
  int x = 0;
  for (std::uint32_t glyph = 0; glyph < files::kGlyphCount; ++glyph) {
    x = files::DrawGlyph(target, glyph, x, 0, 0);
  }
  // _printCharacter's 377 widths sum to 2830; only the space draws nothing.
  CHECK(x == 2830);
  CHECK(target.packets.size() == files::kGlyphCount - 1);
  CHECK(files::DrawGlyph(target, 173, 0, 0, 0) == 6);
  CHECK(files::DrawGlyph(target, 174, 0, 0, 0) == 12);
  CHECK(files::DrawGlyph(target, 189, 0, 0, 0) == 8);
  CHECK(files::DrawGlyph(target, 376, 0, 0, 0) == 12);
}

TEST_CASE("Files text handles spacing, new lines, the arrow and its end",
          "[menu-files][menu]") {
  FakeTarget target;
  std::uint8_t arrow = 0;
  constexpr auto kText = std::to_array<std::uint8_t>(
      {0x01, 0xFA, 0x04, 0x02, 0xE8, 0x03, 0xFA, 0xF8, 0xE6, 0xE7, 0x05});
  files::DrawText(target, arrow, kText, 10, 20, 0);
  REQUIRE(target.packets.size() == 4);
  CHECK(target.packets[0][3] == PackXy(10, 20));
  CHECK(target.packets[1][3] == PackXy(20, 20));
  CHECK(target.packets[2][3] == PackXy(10, 33));
  CHECK(target.packets[3][3] == PackXy(8, 33));
  CHECK(arrow == 1);
  arrow = 11;
  files::DrawText(target, arrow, std::span(kText).subspan(8), 10, 20, 0);
  CHECK(arrow == 0);
  CHECK(target.packets.size() == 5);
}

TEST_CASE("Files integers draw one digit sprite a divisor",
          "[menu-files][menu]") {
  FakeTarget target;
  files::DrawInteger(target, static_cast<std::int32_t>(PackXy(100, 50)), 305,
                     100);
  CHECK(target.packets.size() == 3);
  files::DrawInteger(target, 0, 7, 1);
  CHECK(target.packets.size() == 4);
  files::DrawInteger(target, 0, 7, 0);
  CHECK(target.packets.size() == 4);
  files::DrawBackground(target);
  CHECK(target.packets.size() == 6);
}

namespace {
// Copies one assembled weapon, its blade, grip and gem, an unused shield
// slot and an out-of-range miscellaneous index onto a target with a stale
// weapon record.
std::unique_ptr<files::ContainerData> CopySampleContainerItems() {
  const auto source = std::make_unique<files::ContainerData>();
  auto target = std::make_unique<files::ContainerData>();
  const auto to_copy = std::make_unique<files::ContainerIndices>();
  source->weapons[2] = {.index = 3, .blade = 3, .grip = 1, .gems = {2, 0, 0}};
  source->blades[2].id = 9;
  source->blades[2].assembled_weapon_index = 3;
  source->grips[0].assembled_weapon_index = 3;
  source->gems[1].id = 77;
  source->gems[1].set_item_index = 0x81;
  target->weapons[1].blade = 5;
  to_copy->weapons[0] = 3;
  to_copy->blades[0] = 3;
  to_copy->grips[0] = 1;
  to_copy->shields[0] = 1;
  to_copy->gems[0] = 2;
  to_copy->misc[0] = 300;
  files::CopySelectedContainerItems(*target, *source, *to_copy);
  return target;
}
}  // namespace

TEST_CASE("Files container copy renumbers the selected weapons",
          "[menu-files][menu]") {
  const auto target = CopySampleContainerItems();
  CHECK(target->weapons[0].index == 1);
  CHECK(target->weapons[0].blade == 1);
  CHECK(target->weapons[0].grip == 1);
  CHECK(target->weapons[0].gems[0] == 1);
  CHECK(target->weapons[1] == VS::Items::InventoryWeapon{});
}

TEST_CASE("Files container copy renumbers the selected parts",
          "[menu-files][menu]") {
  const auto target = CopySampleContainerItems();
  CHECK(target->blades[0].id == 9);
  CHECK(target->blades[0].assembled_weapon_index == 1);
  CHECK(target->grips[0].assembled_weapon_index == 1);
  CHECK(target->gems[0].id == 77);
  CHECK(target->gems[0].set_item_index == 0x81);
  CHECK(target->gems[0].index == 1);
  CHECK(target->misc[0].id == 0);
}

TEST_CASE("Files animations settle and the statics reset",
          "[menu-files][menu]") {
  files::State state{};
  CHECK(files::AreAnimationsComplete(state));
  state.elements[4].state =
      static_cast<std::uint8_t>(files::ElementState::kAnimateX);
  CHECK_FALSE(files::AreAnimationsComplete(state));
  state.elements[4].state =
      static_cast<std::uint8_t>(files::ElementState::kStatic);
  CHECK(files::AreAnimationsComplete(state));
  state.text_arrow = 5;
  state.container_cursor = 3;
  state.play_time[0] = '9';
  state.cursor_color = 4;
  files::ResetStatics(state);
  CHECK(state.text_arrow == 0);
  CHECK(state.container_cursor == 0);
  CHECK(std::string_view(state.play_time.data()) == "00:00:00:00");
  CHECK(state.cursor_color == 4);
}

TEST_CASE("Files elements copy their text within the record",
          "[menu-files][menu]") {
  FilesFixture fixture;
  constexpr auto kText =
      std::to_array<std::uint8_t>({0x01, 0xFA, 0x05, 0xE6, 0x02, 0xE7, 0x09});
  files::Element* element = files::InitializeElement(
      fixture.ctx, 3, PackXy(10, 20), PackXy(30, 40), kText);
  REQUIRE(element == &fixture.state.elements[3]);
  CHECK(element->state ==
        static_cast<std::uint8_t>(files::ElementState::kStatic));
  CHECK(element->slot_id == 0xFF);
  CHECK(element->x == 10);
  CHECK(element->h == 40);
  CHECK(std::vector(element->text.begin(), element->text.begin() + 5) ==
        std::vector<std::uint8_t>{0x01, 0xFA, 0x05, 0x02, 0xFF});
}

TEST_CASE("Files elements bound their text and refuse unknown records",
          "[menu-files][menu]") {
  FilesFixture fixture;
  std::vector<std::uint8_t> long_text(40, 0x11);
  const files::Element* element =
      files::InitializeElement(fixture.ctx, 4, 0, 0, long_text);
  REQUIRE(element != nullptr);
  CHECK(element->text[31] == 0x11);
  CHECK(files::InitializeElement(fixture.ctx, 10, 0, 0, {}) == nullptr);
  CHECK(fixture.host.refusals.size() == 1);
}

TEST_CASE("Files element clear resets one record", "[menu-files][menu]") {
  FilesFixture fixture;
  REQUIRE(files::InitializeElement(fixture.ctx, 2, 0, 0, {}) != nullptr);
  CHECK(fixture.state.elements[2].text[0] == 0xFF);
  files::ClearElement(fixture.ctx, 2);
  CHECK(fixture.state.elements[2] == files::Element{});
  files::ClearElement(fixture.ctx, -1);
  CHECK(fixture.host.refusals.size() == 1);
}

TEST_CASE("Files element reset clears the records and the frame counters",
          "[menu-files][menu]") {
  FilesFixture fixture;
  REQUIRE(files::InitializeElement(fixture.ctx, 5, 0, 0, {}) != nullptr);
  fixture.state.cursor_color = 3;
  fixture.state.screen_fade = 2;
  files::ResetElements(fixture.ctx);
  CHECK(fixture.state.elements[5] == files::Element{});
  CHECK(fixture.state.cursor_color == 0);
  CHECK(fixture.state.screen_fade == 0);
  CHECK(fixture.host.cursor == 0);
  CHECK(fixture.host.progress == 0);
  CHECK(fixture.host.transfer_target == 0x180);
  CHECK(fixture.host.card_messages ==
        std::vector<std::optional<std::uint16_t>>{std::nullopt});
}

TEST_CASE("Files card checks report each readiness event",
          "[menu-files][menu]") {
  FilesFixture fixture;
  CHECK(files::CheckCardStep(fixture.ctx, 0x31) == 0);
  CHECK(fixture.host.started_ports == std::vector{1});
  CHECK(fixture.state.card_mask == 3);
  fixture.host.events = {0, 1, 2, 3, 4};
  CHECK(files::CheckCardStep(fixture.ctx, 0) == 0);
  CHECK(files::CheckCardStep(fixture.ctx, 0) == 1);
  CHECK(files::CheckCardStep(fixture.ctx, 0) == -1);
  CHECK(files::CheckCardStep(fixture.ctx, 0) == -1);
  CHECK(files::CheckCardStep(fixture.ctx, 0) == -1);
  CHECK(fixture.host.card_messages ==
        std::vector<std::optional<std::uint16_t>>{
            files::kTextInsertError, files::kTextNoData, files::kTextRemoved});
}

TEST_CASE("Files data save search shows each port it accesses",
          "[menu-files][menu]") {
  FilesFixture fixture;
  fixture.host.events = {3, 1};
  fixture.host.found_slot = 4;
  CHECK(files::FindCurrentDataSaveStep(fixture.ctx, true) == 0);
  int result = 0;
  for (int frame = 0; frame < 8 && result == 0; ++frame) {
    result = files::FindCurrentDataSaveStep(fixture.ctx, false);
  }
  CHECK(result == 20);
  CHECK(fixture.host.info_messages == std::vector<std::uint16_t>{100, 110});
  CHECK(fixture.host.card_messages.size() == 2);
  CHECK(fixture.host.started_ports == std::vector{1, 2});
  CHECK(fixture.host.scanned_ports == std::vector{2});
}

TEST_CASE("Files title save search reports the slot, a timeout or nothing",
          "[menu-files][menu]") {
  TitleFixture fixture;
  fixture.host.events = {1};
  fixture.host.found_slot = 3;
  CHECK(fixture.Search() == 3);
  CHECK(fixture.host.card_messages.back() == std::optional<std::uint16_t>{100});
  fixture.host.found_slot = 0;
  fixture.host.events = {0, 2, 2};
  CHECK(fixture.Search() == -2);
  CHECK(fixture.host.events.empty());
  fixture.host.events = {3, 3};
  CHECK(fixture.Search() == -1);
  CHECK(fixture.state.title_save_search.step == 4);
  fixture.host.events = {1, 1};
  CHECK(fixture.Search() == -1);
  CHECK(fixture.host.scanned_ports.back() == 2);
  CHECK(fixture.host.refusals.empty());
}

TEST_CASE("Files title display set-up swaps buffers and fades out",
          "[menu-files][menu]") {
  FakeTitleHost host;
  files::InitializeDisplay(host);
  CHECK(host.frame_buffer == 0);
  CHECK(host.clears == 1);
  CHECK(host.display_mask == true);
  files::SwapBuffers(host);
  files::SwapBuffers(host);
  CHECK(host.displays ==
        std::vector<std::pair<int, int>>{{320, 0}, {0, 320}, {320, 0}});
  host.frame_buffer = 1;
  files::DrawExitFade(host);
  REQUIRE(host.packets.size() == 3);
  CHECK(host.packets[2] ==
        std::vector<std::uint32_t>{Tag(4), 0x62080808, 0, PackXy(320, 224)});
  CHECK(host.refusals.empty());
}

TEST_CASE("Files clear-save reminder slides in two options",
          "[menu-files][menu]") {
  TitleFixture fixture;
  fixture.host.animations_complete = false;
  fixture.OpenReminder();
  REQUIRE(fixture.host.inits.size() == 2);
  CHECK(fixture.host.inits[1].id == 2);
  CHECK(fixture.host.inits[1].position == PackXy(320, 162));
  CHECK(fixture.host.inits[1].size == PackXy(126, 12));
  CHECK(fixture.host.inits[1].text_offset == 160);
  CHECK(fixture.state.reminder_step == 2);
}

TEST_CASE("Files clear-save reminder answers its options",
          "[menu-files][menu]") {
  TitleFixture fixture;
  fixture.host.animations_complete = false;
  fixture.OpenReminder();
  REQUIRE(fixture.state.reminder_step == 2);
  fixture.host.animations_complete = true;
  CHECK(fixture.Press(0) == 0);
  CHECK(fixture.Press(kCross, kDown) == 0);
  CHECK(fixture.state.reminder_confirm == 2);
  CHECK(fixture.host.cursor == static_cast<std::int32_t>(PackXy(180, 154)));
  CHECK(fixture.Press(kCircle) == 0);
  CHECK(fixture.host.sounds == std::vector{MenuSound::kInvalid,
                                           MenuSound::kChange,
                                           MenuSound::kLeave});
  CHECK(fixture.host.animations.back() == std::pair<int, std::int16_t>{2, 320});
  CHECK(fixture.Press(0) == 2);
}

TEST_CASE("Files live statics reset restores the play time",
          "[menu-files][menu]") {
  files::State& live = files::LiveFilesState();
  const files::State saved = live;
  live.text_arrow = 7;
  live.container_cursor = 2;
  live.play_time[0] = '5';
  live.reminder_step = 3;
  files::ResetLiveFilesStatics();
  CHECK(live.text_arrow == 0);
  CHECK(live.container_cursor == 0);
  CHECK(live.play_time[0] == '0');
  CHECK(live.reminder_step == 3);
  CHECK(&files::LiveFilesState() == &live);
  CHECK(&files::LiveFilesHost() == &files::LiveFilesHost());
  CHECK(files::LiveElementBackgroundColor(0, 0) == 0x6B4100U);
  CHECK(files::LiveElementBackgroundColor(0, 32) == 0);
  live = saved;
}

TEST_CASE("Files retires the MENU7.PRG and TITLE.PRG file module symbols",
          "[menu-files][menu]") {
  // MENU7.PRG 260.c (_drawSaveInfoUI .. vs_menu7_dataMenu) and TITLE.PRG
  // 22C.c (_drawInteger .. _saveScreenExitScreen). No module image stores
  // them, so their entries are retired with the generated code.
  constexpr auto kMenu7 = std::to_array<unsigned>(
      {0x80104764, 0x801047A8, 0x80104840, 0x80104870, 0x80104B00, 0x80104C20,
       0x80104D58, 0x80104DB8, 0x80104EC0, 0x80104F04, 0x80104F40, 0x80105080,
       0x801051F4, 0x801052D0, 0x80105458, 0x8010550C, 0x80105BE4, 0x80106080,
       0x801060C8, 0x801062E0, 0x8010639C, 0x80106554, 0x80106E70, 0x8010787C,
       0x80107E98, 0x801086DC, 0x801087F0, 0x801088B4, 0x80108CE8, 0x8010903C,
       0x8010928C, 0x801092C4, 0x80109D64, 0x80109EB8});
  constexpr auto kTitle = std::to_array<unsigned>(
      {0x8006A860, 0x8006A8F8, 0x8006B138, 0x8006B364, 0x8006B4EC, 0x8006C114,
       0x8006E5D0, 0x8006E68C, 0x8006EA70, 0x8006ECF4});
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const unsigned address : kMenu7) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENU7.PRG",
                                            address) == nullptr);
  }
  for (const unsigned address : kTitle) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "TITLE/TITLE.PRG",
                                            address) == nullptr);
  }
}
