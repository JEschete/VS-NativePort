#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Audio/Sfx.h"
#include "Audio/SoundPlaybackLive.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Hud.h"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Diagnostics.hpp"
#include "DiscQueue.h"
#include "Field/MapProgress.h"
#include "Field/RoomRenderingLiveState.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/Framework/Menus.h"
#include "Menu/GazetteStatisticsLiveFixture.hpp"
#include "Menu/Score/Encyclopedia.h"
#include "Menu/Score/GazetteCameraLive.hpp"
#include "Menu/Score/GazetteRows.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Score/GazetteTerminal.hpp"
#include "Menu/Score/GazetteTerminalLive.hpp"
#include "Menu/Score/ScoreScreen.h"
#include "Render/RoomDrawLive.hpp"
#include "State/Progress.h"
#include "State/StateFlags.h"
#include "State/Timers.hpp"

namespace gazette_terminal_live_tests {
namespace shared = gazette_times_live_tests;
namespace score = VS::Menu::Score;
namespace content = VS::Content;
namespace memory = VS::Core::Memory;
namespace fw = VS::Menu::Framework;
using shared::BytesOf;
inline constexpr std::size_t kEnemyStride = 44;
inline constexpr std::size_t kDescriptionOffset = 150 * kEnemyStride;
inline constexpr unsigned kCircle = 0x20;
inline constexpr unsigned kCross = 0x40;
inline constexpr unsigned kTriangle = 0x10;
inline constexpr unsigned kDown = 0x4000;
inline constexpr std::size_t kFrameBudget = 128;

class SavedRegions {
 public:
  SavedRegions() = default;
  SavedRegions(const SavedRegions&) = delete;
  SavedRegions& operator=(const SavedRegions&) = delete;
  void Keep(std::span<std::byte> region) {
    regions_.push_back({region, {region.begin(), region.end()}});
  }
  ~SavedRegions() { Restore(); }
  void Restore() {
    for (auto& entry : regions_) {
      std::ranges::copy(entry.bytes, entry.region.begin());
    }
    regions_.clear();
  }

 private:
  struct Entry {
    std::span<std::byte> region;
    std::vector<std::byte> bytes;
  };
  std::vector<Entry> regions_;
};

inline auto ControlOwners() {
  return std::array{BytesOf(VS::Input::buttonsPressed),
                    BytesOf(VS::Input::buttonRepeat),
                    BytesOf(VS::Input::previousButtons),
                    BytesOf(VS::Input::releasedButtons),
                    BytesOf(VS::Input::stickPosBuf),
                    BytesOf(VS::Input::settings),
                    BytesOf(VS::State::stateFlags),
                    BytesOf(Audio_nextSfxChannelGroup7E),
                    BytesOf(Menu_Score_menuState),
                    BytesOf(Menu_Score_nextState),
                    BytesOf(Menu_Score_cameraAngles),
                    BytesOf(Menu_Score_cameraDistance),
                    BytesOf(Menu_Score_cameraHeightOffset),
                    BytesOf(Menu_Score_initCameraDistance),
                    BytesOf(Menu_Score_selectedEnemy),
                    BytesOf(Menu_Score_enemySkipDelay),
                    BytesOf(Menu_Score_enemyNavigationState),
                    BytesOf(Menu_Score_buttonHeld),
                    BytesOf(Menu_Score_hideUi),
                    BytesOf(Menu_Score_rotationColor),
                    BytesOf(Menu_Score_zoomColor),
                    BytesOf(Menu_Score_monBinData),
                    BytesOf(Menu_Score_monBinCdQueueSlot),
                    BytesOf(Menu_Score_enemyDescriptions),
                    BytesOf(Menu_Score_characterRank),
                    BytesOf(Menu_Score_score),
                    BytesOf(Menu_Score_cameraPositionBackup),
                    BytesOf(Menu_Score_cameraAnglesBackup),
                    BytesOf(VS::Menu::Framework::menuState),
                    BytesOf(VS::Menu::Framework::actionSelection),
                    BytesOf(VS::Menu::Framework::backgroundFadeStep),
                    BytesOf(VS::Menu::Framework::actionRestrictions),
                    BytesOf(VS::Menu::Framework::itemsText),
                    BytesOf(VS::Menu::Framework::rowTypeBuf),
                    BytesOf(VS::Menu::Framework::selectedRow),
                    BytesOf(VS::Menu::Framework::subMenuId),
                    BytesOf(VS::Menu::Framework::subMenuPage),
                    BytesOf(VS::Menu::Framework::subMenuRowCount),
                    BytesOf(VS::Menu::Framework::subMenuRowInfo),
                    BytesOf(VS::Menu::Framework::itemsListRow),
                    BytesOf(VS::Menu::Framework::menuRowYOffset),
                    BytesOf(VS::Menu::Framework::textHeaders),
                    BytesOf(VS::Menu::Framework::actionMenuState),
                    BytesOf(VS::Menu::Framework::displayActionCost),
                    BytesOf(Menu_textBoxSelector),
                    BytesOf(Battle_hudHidden),
                    BytesOf(Battle_hudSlideOffset),
                    BytesOf(VS::Field::isFieldRenderingEnabled),
                    BytesOf(VS::State::tickSpeed),
                    BytesOf(State_roomVisitProgress),
                    BytesOf(Battle_actionDefinitions),
                    BytesOf(Content_discRequestSlots),
                    BytesOf(Content_discRequestCounts),
                    BytesOf(Content_discDriveState),
                    BytesOf(score::LiveTopMenuState())};
}

inline std::uint16_t Halfword(std::span<const std::byte> bytes,
                              std::size_t offset) {
  if (offset + 2 > bytes.size()) {
    throw std::runtime_error("terminal test halfword outside MON owner");
  }
  return static_cast<std::uint16_t>(
      std::to_integer<unsigned>(bytes[offset]) |
      (std::to_integer<unsigned>(bytes[offset + 1]) << 8));
}

// Extends the immutable shared frame/text/row/font world; it does not replace
// any production service. The nested heap outlives every terminal borrow.
class TerminalFixture
    : public gazette_statistics_live_tests::StatisticsFixture {
 private:
  SavedRegions saved_;
  std::vector<std::byte> arena_ = std::vector<std::byte>(0x800000);
  memory::ScopedLiveGameHeap heap_{arena_};
  content::DataStore::ScopedOverride store_;
  std::array<int, fw::kRowTypeCount> row_types_{};
  std::array<std::uint32_t, 0x180> sound_offsets_{};
  std::array<std::uint16_t, 0x180> sound_banks_{};
  VS::Audio::Playback::ScopedLiveEffectTables sounds_{
      {.program_offsets = sound_offsets_,
       .bank_selectors = sound_banks_,
       .program_data = {}}};
  std::byte* borrowed_mon_ = nullptr;
  VS::Render::ScreenOffset projection_ =
      VS::Render::GetLiveProjectionScreenOffset();

 public:
  TerminalFixture() {
    if (Menu_Score_monBinData != nullptr ||
        Menu_Score_monBinCdQueueSlot != nullptr ||
        content::LiveRefusedDiscRecordsInUse() != 0 ||
        !std::ranges::all_of(Content_discRequestSlots, [](const auto& slot) {
          return slot.state == Content_DiscRequestState_Free;
        })) {
      throw std::runtime_error("terminal fixture requires idle disc owners");
    }
    for (const auto owner : ControlOwners()) saved_.Keep(owner);
    ImportTable(Menu_Score_menuText);
    ImportTable(Menu_Score_rankText);
    ImportTable(Menu_menuStrings);
    if (!content::DataStore::Open(R"(Native\data)")) {
      throw std::runtime_error("terminal fixture needs extracted data");
    }
    content::ResetLiveDiscDrive();
    content::InitializeLiveDiscQueue();
    if (!score::ResetLiveGazetteTerminalModuleState()) {
      throw std::runtime_error("terminal fixture could not reset its owner");
    }
    fw::ResetLiveMainMenuStatics();
    score::ResetGazetteRowsLive();
    VS::Menu::Framework::itemsText = nullptr;
    VS::Menu::Framework::rowTypeBuf = row_types_.data();
    VS::Menu::Framework::menuState = {};
    VS::Menu::Framework::actionSelection = {};
    VS::Menu::Framework::actionRestrictions = 0;
    VS::Menu::Framework::backgroundFadeStep = 0;
    Menu_Score_characterRank = 0;
    VS::Input::settings.cursorMemory = 1;
    VS::State::stateFlags.introState = 3;
    sound_offsets_.fill(std::numeric_limits<std::uint32_t>::max());
    VS::State::tickSpeed = 4;
    Input();
  }
  ~TerminalFixture() {
    if (VS::Menu::Framework::itemsText != nullptr) {
      static_cast<void>(fw::CloseLiveRowsAndRememberCursor());
    }
    ReleaseBorrowedMon();
    if (!score::ResetLiveGazetteTerminalModuleState()) {
      VS::Core::Errorf("tests",
                       "terminal fixture retained an unsafe MON owner");
      std::abort();
    }
    content::ResetLiveDiscDrive();
    VS::Render::SetLiveProjectionScreenOffset(projection_);
    saved_.Restore();
  }
  TerminalFixture(const TerminalFixture&) = delete;
  TerminalFixture& operator=(const TerminalFixture&) = delete;

  template <typename T>
  void ImportTable(T& table) {
    ImportRegion(BytesOf(table));
  }

  void ImportRegion(std::span<std::byte> region) {
    const auto entries = VS::Content::ModuleDataEntries();
    const auto found =
        std::ranges::find(entries, static_cast<void*>(region.data()),
                          &VS::Content::ModuleDataEntry::target);
    if (found == entries.end() || found->module == nullptr ||
        found->capacity != region.size() ||
        (std::string_view(found->module) != shared::kGazetteModule &&
         std::string_view(found->module) != shared::kBattleModule &&
         std::string_view(found->module) != "MENU/MAINMENU.PRG")) {
      throw std::runtime_error("terminal original table owner mismatch");
    }
    saved_.Keep(region);
    std::string path = R"(Native\data\disc\)";
    path += found->module;
    std::ranges::replace(path, '/', '\\');
    const auto image = shared::ReadImage(path.c_str());
    if (!content::CopyModuleDataEntry(*found, image)) {
      throw std::runtime_error("terminal original table import failed");
    }
  }

  static void Input(unsigned pressed = 0, unsigned repeated = 0,
                    unsigned held = 0, unsigned released = 0) {
    VS::Input::buttonsPressed = pressed;
    VS::Input::buttonRepeat = std::bit_cast<std::int32_t>(repeated);
    VS::Input::previousButtons = std::bit_cast<std::int32_t>(held);
    VS::Input::releasedButtons = std::bit_cast<std::int32_t>(released);
    VS::Input::stickPosBuf.l_stick_x = 128;
    VS::Input::stickPosBuf.l_stick_y = 128;
    VS::Input::stickPosBuf.r_stick_x = 128;
    VS::Input::stickPosBuf.r_stick_y = 128;
  }

  void BorrowMon() {
    if (borrowed_mon_ != nullptr || Menu_Score_monBinData != nullptr) {
      throw std::runtime_error("terminal test already has a MON owner");
    }
    borrowed_mon_ = static_cast<std::byte*>(memory::AllocateLiveFromHighEnd(
        static_cast<std::size_t>(Menu_Score_kMonBinFile.size)));
    if (borrowed_mon_ == nullptr ||
        content::ReadLiveDiscFileBlocking(Menu_Score_kMonBinFile.lba,
                                          Menu_Score_kMonBinFile.size,
                                          borrowed_mon_) != 1) {
      throw std::runtime_error("real MON read did not complete");
    }
    Menu_Score_monBinData =
        reinterpret_cast<Menu_Score_EnemyEntry*>(borrowed_mon_);
    Menu_Score_enemyDescriptions =
        reinterpret_cast<u_short*>(borrowed_mon_ + kDescriptionOffset);
  }

  void ReleaseBorrowedMon() {
    if (borrowed_mon_ == nullptr) return;
    const auto capacity = memory::LivePayloadCapacity(borrowed_mon_);
    const auto bytes = static_cast<std::size_t>(Menu_Score_kMonBinFile.size);
    if (!capacity || *capacity < bytes ||
        !content::CancelLiveDiscWrites({borrowed_mon_, bytes})) {
      VS::Core::Errorf("tests", "terminal borrowed MON cleanup refused");
      std::abort();
    }
    Menu_Score_monBinData = nullptr;
    Menu_Score_enemyDescriptions = nullptr;
    memory::FreeLiveFromHighEnd(borrowed_mon_);
    borrowed_mon_ = nullptr;
  }

  static std::span<std::byte> MonBytes() {
    const auto capacity = memory::LivePayloadCapacity(Menu_Score_monBinData);
    const auto bytes = static_cast<std::size_t>(Menu_Score_kMonBinFile.size);
    if (!capacity || *capacity < bytes) {
      throw std::runtime_error("terminal MON borrow left its real heap owner");
    }
    return {reinterpret_cast<std::byte*>(Menu_Score_monBinData), bytes};
  }

  static shared::EncodedText Description(std::size_t index) {
    const auto descriptions =
        std::as_bytes(MonBytes()).subspan(kDescriptionOffset);
    const auto offset =
        static_cast<std::size_t>(Halfword(descriptions, index * 2)) * 2;
    const auto bytes = descriptions.subspan(offset);
    return shared::TerminatedText(
        {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
  }

  static shared::EncodedText EnemyName(std::size_t index,
                                       bool skip_prefix = false) {
    const auto bytes = MonBytes().subspan(index * kEnemyStride + 16, 28);
    const auto text = std::span(
        reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    return shared::TerminatedText(text.subspan(skip_prefix ? 2 : 0));
  }

  static shared::EncodedText RankText() {
    const auto address =
        reinterpret_cast<std::uintptr_t>(Menu_textBoxes[0].string);
    const std::array<std::span<const std::byte>, 2> owners{
        std::as_bytes(std::span(Menu_Score_rankText)),
        Menu_Score_monBinData == nullptr ? std::span<const std::byte>{}
                                         : MonBytes()};
    for (const auto owner : owners) {
      const auto first = reinterpret_cast<std::uintptr_t>(owner.data());
      if (address < first || address - first >= owner.size()) continue;
      const auto bytes = owner.subspan(address - first);
      return shared::TerminatedText(
          {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    }
    throw std::runtime_error("rank text box left the original rank/MON owner");
  }

  static void UnlockAll() {
    std::ranges::fill(VS::State::scoreData.enemyKillFlags, -1);
    score::InitializeLiveEnemyList();
  }
};
using Fixture = TerminalFixture;
}  // namespace gazette_terminal_live_tests
