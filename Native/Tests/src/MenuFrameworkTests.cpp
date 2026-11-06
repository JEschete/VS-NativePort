#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/GameFont.hpp"
#include "Render/UiPackets.hpp"

extern "C" {
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
}

namespace {
namespace fw = VS::Menu::Framework;

// The text codes SetMenuItem reads (BATTLE.PRG 5BF94.c:2660).
constexpr std::uint8_t kGlyphA = 0x10;
constexpr std::uint8_t kGlyphB = 0x11;
constexpr std::uint8_t kGlyphC = 0x12;
constexpr std::uint8_t kNonPrinting = 0xE5;
constexpr std::uint8_t kTerminator = 0xE7;
constexpr std::uint8_t kChunkSize = 0xF8;
constexpr std::uint8_t kSpacing = 0xFA;
constexpr std::uint8_t kRowEnd = 0xFF;
constexpr std::uint8_t kSpacingWidth = 5;

constexpr int kRowX = 20;
constexpr int kRowY = 30;
constexpr int kRowWidth = 64;
constexpr int kRowBackground = 9;
constexpr int kSampleRow = 3;
constexpr int kScreenWidth = 320;

class FakeQueue final : public VS::Render::PacketQueue {
 public:
  [[nodiscard]] std::uint32_t ChainLink() const override { return 0; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    words += packet.size();
    return true;
  }
  std::size_t words = 0;
};

// Every request lands in the fake's records; nothing touches a live global.
class FakeHost final : public fw::Host {
 public:
  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  std::span<fw::MenuRow> Rows() override {
    if (!workspace) return {};
    return rows;
  }
  std::span<std::uint8_t> Strings() override { return strings; }
  std::span<std::int32_t> RowTypes() override { return row_types; }
  std::span<std::uint8_t> MenuMemory() override { return memory; }
  bool AllocateWorkspace() override {
    workspace = true;
    return true;
  }
  void ReleaseWorkspace() override { workspace = false; }
  void ForgetWorkspace() override { workspace = false; }
  std::span<fw::ItemText> Items(fw::ItemList /*list*/) override { return {}; }
  bool HasItems(fw::ItemList /*list*/) override { return false; }
  std::span<fw::ItemText> AllocateItems(fw::ItemList /*list*/,
                                        int /*count*/) override {
    return {};
  }
  void FreeItems(fw::ItemList /*list*/) override {}
  fw::Buttons Input() override { return {}; }
  fw::Settings MenuSettings() override { return {}; }
  void SetMenuFlags(std::uint32_t /*flags*/) override {}
  void PlaySound(fw::MenuSound sound) override { sounds.push_back(sound); }
  void InitInformationBox(fw::InformationHeader /*header*/) override {}
  void SetInformationBoxState(std::uint8_t /*state*/) override {}
  void SetInformationText(std::span<const std::uint8_t> /*text*/) override {}
  void ShowInformationMessage(std::span<const std::uint8_t> text) override {
    shown.assign(text.begin(), text.end());
  }
  void DismissInformationBox() override {}
  void SetStringArgument(std::span<const std::uint8_t> /*text*/) override {}
  int CurrentSceneId() override { return 0; }
  std::optional<fw::RestrictionInputs> Restrictions() override {
    return restrictions;
  }
  bool IsActionUnlocked(int /*action*/) override { return false; }
  fw::StoryFlags Story() override { return {}; }
  bool HasUnlockedSpellClass(int /*spell_class*/) override { return false; }
  bool HasUnlockedWeaponBreakArt() override { return false; }
  bool HasUnlockedBattleAbility(int /*ability*/) override { return false; }
  std::span<const std::uint8_t> AshleyName() override { return {}; }
  const char* LimbName(int /*limb*/) override { return ""; }
  void BeginOverlayLoad(int /*sub_menu*/, bool /*main_menu_program*/) override {
  }
  bool IsOverlayLoaded() override { return true; }
  void FinishOverlayLoad() override {}
  void ForgetOverlayLoad() override {}
  void ResetMainMenuOverlay() override {}
  bool ReleaseBackgroundBuffer() override { return false; }
  void StoreEquippedStats() override {}
  std::span<std::uint32_t> AllocateBackgroundBuffer() override { return {}; }
  void ForgetBackgroundBuffer() override {}
  std::span<const std::uint8_t> BackgroundSource() override { return {}; }
  void UpdateMiscItemShortcut(int /*mode*/) override {}
  int RunSubmenu(fw::Submenu /*submenu*/, int /*state_index*/) override {
    return 0;
  }
  void BindInventoryViews() override {}
  void UpdateStatPanels() override {}
  void ReserveMenuTexturePages() override {}
  void RetireMenuTexturePages() override {}
  void UploadImage(std::uint32_t /*position*/,
                   std::span<const std::byte> /*pixels*/,
                   std::uint32_t /*size*/) override {}
  void ClearImage(const fw::ImageRect& /*rect*/) override {}
  void LoadBackgroundClut(std::span<const std::uint8_t> /*colors*/) override {}
  void ResetBattleInterface() override {}
  void SetBackgroundRaised(bool /*raised*/) override {}
  VS::Render::PacketQueue& Queue(fw::Layer /*layer*/) override { return queue; }
  VS::Render::FontStyle& FontStyle() override { return font; }
  void DrawCompactText(fw::Layer /*layer*/, std::string_view /*text*/,
                       std::uint32_t /*position*/,
                       std::uint32_t /*color*/) override {}
  int FrameBufferIndex() override { return 0; }
  bool TabArrowsFrozen() override { return false; }
  fw::AshleyPreviewState AshleyPreview() override { return {}; }
  void SetAshleyPreview(const fw::AshleyPreviewState& /*preview*/) override {}
  std::optional<fw::AshleyMetrics> Ashley() override { return std::nullopt; }
  void BeginAshleyPreview() override {}
  void LoadActorPreview() override {}
  void ReleaseActorPreview() override {}
  void EndAshleyPreview() override {}
  void DrawAshleyPreview(const VS::Core::Math::Vec3& /*look_at*/,
                         const VS::Core::Math::Vec3& /*position*/) override {}
  void UpdateActorAssetQueue() override {}
  int FindFreeActorAssetRequest() override { return 0; }

  bool workspace = true;
  std::array<fw::MenuRow, fw::kRowCount> rows{};
  std::array<std::uint8_t, fw::kStringBufferBytes> strings{};
  std::array<std::int32_t, fw::kRowTypeCount> row_types{};
  std::array<std::uint8_t, fw::kMenuMemoryBytes> memory{};
  std::optional<fw::RestrictionInputs> restrictions;
  std::vector<std::string> refusals;
  std::vector<fw::MenuSound> sounds;
  std::vector<std::uint8_t> shown;
  FakeQueue queue;
  VS::Render::FontStyle font;
};

struct Fixture {
  FakeHost host;
  fw::State state;
  fw::Tables tables;
  fw::Context ctx{.state = state, .host = host, .tables = tables};
};

std::size_t Index(int row) { return static_cast<std::size_t>(row); }

// A row text with a spacing code and a non-printing code.
constexpr std::array<std::uint8_t, 7> kSampleText{
    kGlyphA,      kGlyphB, kSpacing,    kSpacingWidth,
    kNonPrinting, kGlyphC, kTerminator,
};

// SetMenuItem over a row a previous menu left selected.
fw::MenuRow& SetSampleRow(Fixture& f) {
  f.host.rows[Index(kSampleRow)].selected = 1;
  f.host.rows[Index(kSampleRow)].target0 = kRowX;
  return fw::SetMenuItem(f.ctx,
                         {.row = kSampleRow,
                          .x = kRowX,
                          .y = kRowY,
                          .w = kRowWidth,
                          .background_width = kRowBackground},
                         kSampleText);
}

// Every row idle and kRowWidth wide.
void IdleRows(Fixture& f) {
  for (auto& row : f.host.rows) {
    row.state = fw::RowState::kIdle;
    row.w = kRowWidth;
  }
}

// func_800CABE0's state bits and restriction flags (BATTLE.PRG
// 5BF94.c:3535).
constexpr std::uint32_t kLocked = 0x80000;
constexpr std::uint32_t kLimited = 0x100000;
constexpr std::uint32_t kLimitBlocker = 0x40000000;
constexpr std::uint32_t kRegen = 1U << 12U;
constexpr std::uint32_t kWard = 1U << 13U;
constexpr std::int32_t kLockedFlag = 2;
constexpr std::int32_t kLimitedFlag = 4;
constexpr std::int32_t kSheathed = 8;
constexpr std::int32_t kNoMp = 0x20;
constexpr std::int32_t kNoHp = 0x40;
constexpr std::int32_t kRegenOrStrDown = 0x80;
constexpr std::int32_t kWarded = 0x100;
constexpr std::int32_t kEvent = 0x200;
constexpr int kEventMode = 2;
constexpr std::int32_t kHealthy = 10;

// The sort stat numbers of items1.h:695-949.
constexpr int kStatCategory = 0;
constexpr int kStatMaterial = 1;
constexpr int kStatRange = 2;
constexpr int kStatStrength = 8;
constexpr int kStatFirstClass = 11;
constexpr int kStatFirstAffinity = 17;
constexpr int kStatShieldFirstClass = 27;
constexpr int kStatShieldFirstAffinity = 33;
constexpr int kStatShieldFirstType = 40;
constexpr int kStatOutsideClasses = 40;
constexpr std::int32_t kSortId = 7;
constexpr std::int32_t kSortCount = 3;
constexpr std::int32_t kSortValue = 11;

fw::SortStats SampleSortStats() {
  fw::SortStats stats{.category = kSortId,
                      .material = kSortCount,
                      .range = kSortValue,
                      .strength = kSortValue};
  stats.classes[0] = kSortValue;
  stats.affinities[0] = kSortId;
  stats.types[1] = kSortCount;
  return stats;
}

// One in 4.12 fixed point, and a quarter turn.
constexpr std::int16_t kFixedOne = 0x1000;
constexpr std::int16_t kQuarterTurn = 0x400;

// Points the generated row table at local rows for one test.
class LiveRows {
 public:
  LiveRows() : saved_(Menu_Framework_menuItems) {
    Menu_Framework_menuItems = rows.data();
  }
  ~LiveRows() { Menu_Framework_menuItems = saved_; }
  LiveRows(const LiveRows&) = delete;
  LiveRows& operator=(const LiveRows&) = delete;
  LiveRows(LiveRows&&) = delete;
  LiveRows& operator=(LiveRows&&) = delete;

  std::array<Menu_Framework_MenuRow, fw::kRowCount> rows{};

 private:
  Menu_Framework_MenuRow* saved_;
};

Menu_Framework_MenuRow* SetLiveSampleRow() {
  std::array<char, 3> text{static_cast<char>(kGlyphA),
                           static_cast<char>(kGlyphB),
                           static_cast<char>(kTerminator)};
  return fw::SetLiveMenuItem(kSampleRow, kRowX, kRowY, kRowWidth,
                             kRowBackground, text.data());
}
}  // namespace

TEST_CASE("Menu framework refuses rows outside the menu table",
          "[menu-framework][menu]") {
  Fixture f;
  const int past_end = static_cast<int>(fw::kRowCount);
  CHECK(&fw::MenuItem(f.ctx, past_end) == &f.state.discard_row);
  CHECK(&fw::MenuItem(f.ctx, -1) == &f.state.discard_row);
  CHECK(f.host.refusals.size() == 2);
  CHECK(&fw::MenuItem(f.ctx, kSampleRow) == &f.host.rows[Index(kSampleRow)]);
  CHECK(f.host.refusals.size() == 2);

  // Without the workspace every row request is refused; the row loops refuse
  // the missing table once.
  constexpr std::size_t kWorkspaceRefusals = 4;
  f.host.workspace = false;
  CHECK(&fw::MenuItem(f.ctx, 0) == &f.state.discard_row);
  fw::FlyOutRowRight(f.ctx, kSampleRow);
  fw::ClearMenuExcept(f.ctx, kSampleRow);
  CHECK(fw::AreMenuRowsIdle(f.ctx));
  CHECK(f.host.refusals.size() == 2 + kWorkspaceRefusals);
  CHECK(f.state.discard_row.state == fw::RowState::kInactive);
}

TEST_CASE("Menu framework SetMenuItem places the row",
          "[menu-framework][menu]") {
  Fixture f;
  const fw::MenuRow& row = SetSampleRow(f);
  CHECK(&row == &f.host.rows[Index(kSampleRow)]);
  CHECK(row.state == fw::RowState::kIdle);
  CHECK(row.x == kRowX);
  CHECK(row.y == kRowY);
  CHECK(row.w == kRowWidth);
  CHECK(row.background_width == kRowBackground);
  CHECK(row.selected == 0);
  CHECK(row.target0 == 0);
}

TEST_CASE("Menu framework SetMenuItem copies the row text to its end",
          "[menu-framework][menu]") {
  Fixture f;
  const fw::MenuRow& row = SetSampleRow(f);
  // The spacing code keeps its width; the non-printing code is dropped.
  const std::array<std::uint8_t, 6> expected{kGlyphA,       kGlyphB, kSpacing,
                                             kSpacingWidth, kGlyphC, kRowEnd};
  CHECK(std::equal(expected.begin(), expected.end(), row.text.begin()));
  CHECK(f.host.refusals.empty());
}

TEST_CASE("Menu framework SetMenuItem stops at the end of an open source",
          "[menu-framework][menu]") {
  Fixture f;
  // A source without its terminator stops at the end of the span.
  const std::array<std::uint8_t, 2> open{kGlyphA, kGlyphB};
  const fw::MenuRow& refused = fw::SetMenuItem(f.ctx, {.row = 0}, open);
  CHECK(refused.text[0] == kGlyphA);
  CHECK(refused.text[2] == kRowEnd);
  CHECK(f.host.refusals.size() == 1);
}

TEST_CASE("Menu framework fly-out left moves the action row",
          "[menu-framework][menu]") {
  Fixture f;
  IdleRows(f);
  CHECK(fw::AreMenuRowsIdle(f.ctx));

  // vs_mainMenu_menuItemFlyoutLeft moves the action row (row + 32).
  fw::FlyOutRowLeft(f.ctx, 0);
  const fw::MenuRow& action = f.host.rows[Index(fw::kActionRowBase)];
  CHECK(action.state == fw::RowState::kDismiss);
  CHECK(action.target0 == -kRowWidth);
  CHECK_FALSE(fw::AreMenuRowsIdle(f.ctx));
}

TEST_CASE("Menu framework ClearMenuExcept sends the other rows right",
          "[menu-framework][menu]") {
  Fixture f;
  IdleRows(f);
  // vs_mainMenu_clearMenuExcept sends every other list row right.
  fw::ClearMenuExcept(f.ctx, kSampleRow);
  CHECK(f.host.rows[Index(kSampleRow)].state == fw::RowState::kIdle);
  CHECK(f.host.rows[0].state == fw::RowState::kSlideX);
  CHECK(f.host.rows[0].target0 == kScreenWidth);
  CHECK(f.host.rows[Index(fw::kActionRowBase - 1)].state ==
        fw::RowState::kSlideX);
}

TEST_CASE("Menu framework fly-outs leave inactive rows in place",
          "[menu-framework][menu]") {
  Fixture f;
  IdleRows(f);
  f.host.rows[1].state = fw::RowState::kInactive;
  f.host.rows[1].target0 = 0;
  fw::FlyOutRowRight(f.ctx, 1);
  CHECK(f.host.rows[1].state == fw::RowState::kInactive);
  CHECK(f.host.rows[1].target0 == 0);
  CHECK(f.host.refusals.empty());
}

TEST_CASE("Menu framework quit delay closes on the fifth frame",
          "[menu-framework][menu]") {
  constexpr int kQuitFrames = 5;
  Fixture f;
  for (int frame = 1; frame <= kQuitFrames; ++frame) {
    CHECK(fw::QuitDelay(f.ctx) == (frame == kQuitFrames ? 1 : 0));
  }
  CHECK(fw::QuitDelay(f.ctx) == 0);
  f.state.main_menu = fw::MainMenuStatics{};
  CHECK(f.state.main_menu.quit_delay == 0);
  CHECK(f.state.main_menu.background_brightness ==
        fw::kInitialBackgroundBrightness);
  CHECK(f.state.main_menu.action_cost_panel_step == fw::kActionCostPanelHidden);
}

TEST_CASE("Menu framework restriction flags follow func_800CABE0",
          "[menu-framework][menu]") {
  fw::RestrictionInputs inputs;
  CHECK(fw::RestrictionFlagsOf(inputs, 0) == (kSheathed | kNoMp | kNoHp));
  inputs = {.weapon_drawn = true, .mp = kHealthy, .hp = kHealthy};
  CHECK(fw::RestrictionFlagsOf(inputs, 0) == 0);
  inputs.state_flags = kLocked | kLimited;
  CHECK(fw::RestrictionFlagsOf(inputs, 0) == (kLockedFlag | kLimitedFlag));
  inputs.state_flags = kLimited | kLimitBlocker;
  CHECK(fw::RestrictionFlagsOf(inputs, 0) == 0);
  inputs.state_flags = 0;
  inputs.statuses = kRegen;
  CHECK(fw::RestrictionFlagsOf(inputs, kEventMode) ==
        (kRegenOrStrDown | kEvent));
  inputs.statuses = 1U | kWard;
  CHECK(fw::RestrictionFlagsOf(inputs, 1) == (kRegenOrStrDown | kWarded));
}

TEST_CASE("Menu framework action restrictions need Ashley",
          "[menu-framework][menu]") {
  // Without Ashley the host refuses and only the event bit remains.
  Fixture f;
  CHECK(fw::ActionRestrictionFlags(f.ctx, kEventMode) == kEvent);
  CHECK(fw::ActionRestrictionFlags(f.ctx, 0) == 0);
  CHECK(f.host.refusals.size() == 2);
  f.host.restrictions = fw::RestrictionInputs{.weapon_drawn = true,
                                              .mp = kHealthy,
                                              .hp = kHealthy,
                                              .statuses = 1U | kWard};
  CHECK(fw::ActionRestrictionFlags(f.ctx, 0) == (kRegenOrStrDown | kWarded));
}

TEST_CASE("Menu framework information message keeps 96 bytes",
          "[menu-framework][menu]") {
  constexpr std::size_t kLongMessage = 120;
  constexpr std::uint8_t kLetter = 0x41;
  Fixture f;
  const std::vector<std::uint8_t> message(kLongMessage, kLetter);
  fw::SetInformationMessage(f.ctx, message);
  const auto& buffer = f.state.information_message;
  CHECK(buffer[0] == kChunkSize);
  CHECK(buffer[1] == 0);
  const std::span<const std::uint8_t> copied =
      std::span(buffer).subspan(2, fw::kInformationMessageCopyBytes);
  CHECK(std::all_of(copied.begin(), copied.end(),
                    [](std::uint8_t c) { return c == kLetter; }));
  CHECK(buffer[fw::kInformationMessageCopyBytes + 2] == kTerminator);
  CHECK(buffer[fw::kInformationMessageCopyBytes + 3] == kTerminator);
  CHECK(buffer[fw::kInformationMessageCopyBytes + 4] == 0);
  CHECK(f.host.shown.size() == fw::kInformationMessageBytes);
  CHECK(std::equal(f.host.shown.begin(), f.host.shown.end(), buffer.begin()));
}

TEST_CASE("Menu framework misc sort key reads the id or the count",
          "[menu-framework][menu]") {
  constexpr fw::MiscSortStats kMisc{.id = kSortId, .count = kSortCount};
  CHECK(fw::MiscSortValue(kStatCategory, kMisc) == -kSortId);
  CHECK(fw::MiscSortValue(kStatStrength, kMisc) == kSortCount);
}

TEST_CASE("Menu framework weapon sort key reads the loadout stats",
          "[menu-framework][menu]") {
  Fixture f;
  const fw::SortStats stats = SampleSortStats();
  CHECK(fw::WeaponSortValue(f.ctx, kStatCategory, stats) == -kSortId);
  CHECK(fw::WeaponSortValue(f.ctx, kStatRange, stats) == kSortValue);
  CHECK(fw::WeaponSortValue(f.ctx, kStatStrength, stats) == kSortValue);
  CHECK(fw::WeaponSortValue(f.ctx, kStatFirstClass, stats) == kSortValue);
  CHECK(fw::WeaponSortValue(f.ctx, kStatFirstAffinity, stats) == kSortId);
  CHECK(f.host.refusals.empty());
}

TEST_CASE("Menu framework shield and equipment sort keys read the stats",
          "[menu-framework][menu]") {
  Fixture f;
  const fw::SortStats stats = SampleSortStats();
  CHECK(fw::ShieldSortValue(f.ctx, kStatMaterial, stats) == -kSortCount);
  CHECK(fw::ShieldSortValue(f.ctx, kStatShieldFirstClass, stats) == kSortValue);
  CHECK(fw::ShieldSortValue(f.ctx, kStatShieldFirstAffinity, stats) == kSortId);
  CHECK(fw::ShieldSortValue(f.ctx, kStatShieldFirstType, stats) == kSortCount);
  // Equipment stats past the shield's classes fold onto the weapon's.
  CHECK(fw::EquipmentSortValue(f.ctx, kStatShieldFirstClass, stats) ==
        kSortValue);
  CHECK(f.host.refusals.empty());
}

TEST_CASE("Menu framework sort keys refuse elements past the record",
          "[menu-framework][menu]") {
  Fixture f;
  // The original read past the affinity array; the native key refuses.
  CHECK(fw::WeaponSortValue(f.ctx, kStatFirstAffinity + kStatOutsideClasses,
                            SampleSortStats()) == 0);
  CHECK(f.host.refusals.size() == 1);
}

TEST_CASE("Menu framework yaw and pitch matrices start at the identity",
          "[menu-framework][menu]") {
  const VS::Core::Math::Mat3 identity{
      .m = {{{kFixedOne, 0, 0}, {0, kFixedOne, 0}, {0, 0, kFixedOne}}}};
  CHECK(fw::BuildYawMatrix(0) == identity);
  CHECK(fw::BuildPitchMatrix(0) == identity);
}

TEST_CASE("Menu framework yaw matrix turns by the negated angle",
          "[menu-framework][menu]") {
  // _buildYawMatrix rotates by the negated angle about Y.
  const VS::Core::Math::Mat3 yaw = fw::BuildYawMatrix(kQuarterTurn);
  CHECK(yaw.m[0][0] == 0);
  CHECK(yaw.m[0][2] == kFixedOne);
  CHECK(yaw.m[2][0] == -kFixedOne);
  CHECK(yaw.m[1][1] == kFixedOne);
}

TEST_CASE("Menu framework pitch matrix turns about X",
          "[menu-framework][menu]") {
  const VS::Core::Math::Mat3 pitch = fw::BuildPitchMatrix(kQuarterTurn);
  CHECK(pitch.m[1][1] == 0);
  CHECK(pitch.m[1][2] == -kFixedOne);
  CHECK(pitch.m[2][1] == kFixedOne);
  CHECK(pitch.m[0][0] == kFixedOne);
}

TEST_CASE("Menu framework live SetMenuItem fills the generated row",
          "[menu-framework][menu]") {
  LiveRows live;
  const Menu_Framework_MenuRow* row = SetLiveSampleRow();
  CHECK(row == &live.rows[Index(kSampleRow)]);
  CHECK(row->state == 1);
  CHECK(row->x == kRowX);
  CHECK(row->y == kRowY);
  CHECK(row->w == kRowWidth);
  CHECK(row->backgroundWidth == kRowBackground);
  CHECK(static_cast<std::uint8_t>(row->text[0]) == kGlyphA);
  CHECK(static_cast<std::uint8_t>(row->text[2]) == kRowEnd);
}

TEST_CASE("Menu framework live rows fly out and deactivate",
          "[menu-framework][menu]") {
  const LiveRows live;
  const Menu_Framework_MenuRow* row = SetLiveSampleRow();
  CHECK(fw::LiveMenuItem(kSampleRow) == row);
  CHECK(fw::AreLiveMenuRowsIdle() == 1);
  fw::FlyOutLiveRowRight(kSampleRow);
  CHECK(row->state == 2);
  CHECK(row->targetPosition0 == kScreenWidth);
  CHECK(fw::AreLiveMenuRowsIdle() == 0);
  fw::DeactivateLiveRow(kSampleRow);
  CHECK(row->state == 0);
}

TEST_CASE("Menu framework live rows outside the table land in the discard row",
          "[menu-framework][menu]") {
  LiveRows live;
  // A row past the table, or no table at all, lands in the discard row.
  Menu_Framework_MenuRow* const past_end =
      fw::LiveMenuItem(static_cast<int>(fw::kRowCount));
  REQUIRE(past_end != nullptr);
  CHECK(std::none_of(live.rows.begin(), live.rows.end(),
                     [past_end](const Menu_Framework_MenuRow& entry) {
                       return &entry == past_end;
                     }));
  Menu_Framework_menuItems = nullptr;
  CHECK(fw::LiveMenuItem(0) != nullptr);
  CHECK(fw::AreLiveMenuRowsIdle() == 1);
}

TEST_CASE("Menu framework live quit delay restarts after a reload",
          "[menu-framework][menu]") {
  constexpr int kQuitFrames = 5;
  const LiveRows live;
  fw::ResetLiveMainMenuStatics();

  for (int frame = 1; frame < kQuitFrames; ++frame) {
    CHECK(fw::LiveQuitDelay() == 0);
  }
  fw::ResetLiveMainMenuStatics();
  for (int frame = 1; frame <= kQuitFrames; ++frame) {
    CHECK(fw::LiveQuitDelay() == (frame == kQuitFrames ? 1 : 0));
  }
  fw::ResetLiveMainMenuStatics();
}
