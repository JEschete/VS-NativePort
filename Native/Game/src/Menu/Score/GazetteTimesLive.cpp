#include "Menu/Score/GazetteTimesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

#include "Core/Log.hpp"
#include "Core/String/GameText.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Score/GazetteCameraLive.hpp"
#include "Menu/Score/GazetteTimes.hpp"
#include "Render/GameFontLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/StatusEffects.h"
#include "Content/StringTables.h"
#include "Menu/Score/Records.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"
}

namespace VS::Menu::Score {
namespace {
constexpr std::uint32_t kMissingPackedTime = 0x800000;
constexpr unsigned kMinutesShift = 16;
constexpr unsigned kSecondsShift = 8;
constexpr std::size_t kTextOrderingSlot = 7;

TimeTrialAnimation live_animation;
std::uint64_t refusals = 0;

void Refuse(std::string_view reason) {
  if (std::has_single_bit(++refusals)) {
    VS_LOG_WARN("menu.score", "time trial: {} ({} refusals)", reason, refusals);
  }
}

template <std::size_t Size>
TimeTrialText Bytes(const u_short (&table)[Size]) {
  // The text tables contain halfword offsets followed by encoded bytes.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return {reinterpret_cast<const std::uint8_t*>(table), sizeof table};
}

template <std::size_t Size>
TimeTrialText Label(const u_short (&table)[Size], std::size_t index) {
  if (index >= Size || table[index] >= Size) return {};
  return Bytes(table).subspan(table[index] * sizeof(u_short));
}

TimeTrialText BorrowedText(const char* pointer) {
  const std::array tables{Bytes(Menu_Score_timeAttacks),
                          Bytes(Menu_Score_timeAttackDescriptions),
                          Bytes(Menu_Score_miscInfo)};
  // Integer comparisons avoid comparing pointers to unrelated arrays.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  for (const auto table : tables) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto first = reinterpret_cast<std::uintptr_t>(table.data());
    if (address >= first && address - first < table.size()) {
      return table.subspan(address - first);
    }
  }
  return {};
}

TimeTrialLabels Labels() {
  TimeTrialLabels labels;
  for (std::size_t index = 0; index < kTimeTrialCount; ++index) {
    labels.titles[index] = Label(Menu_Score_timeAttacks, index);
    labels.descriptions[index] =
        Label(Menu_Score_timeAttackDescriptions, index);
  }
  labels.missing_title =
      Label(Menu_Score_miscInfo, Text_Menu9_miscInfo_Index_noneBossRush);
  labels.missing_description =
      Label(Menu_Score_miscInfo, Text_Menu9_miscInfo_Index_noneBossRushDesc);
  return labels;
}

TimeTrialScores Scores() {
  TimeTrialScores scores;
  for (std::size_t trial = 0; trial < kTimeTrialCount; ++trial) {
    for (std::size_t place = 0; place < kTimeTrialPlaces; ++place) {
      const auto& original =
          Audio_Seq_unusedScoredata.bossTimeTrialScores[trial][place];
      TimeTrialRecord& record = scores[trial][place];
      if (original.time != kMissingPackedTime) {
        record.time = TimeTrialTime{
            static_cast<std::uint8_t>(original.time >> kMinutesShift),
            static_cast<std::uint8_t>(original.time >> kSecondsShift),
            static_cast<std::uint8_t>(original.time)};
      }
      record.round_index = static_cast<std::uint8_t>(original.round);
    }
  }
  return scores;
}

std::array<TimeTrialRow, kTimeTrialCount> Rows() {
  std::array<TimeTrialRow, kTimeTrialCount> rows{};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& original = Menu_Score_gazetteRows[index];
    rows[index] = {
        .selected = static_cast<std::uint8_t>(original.selected),
        .enabled_style = static_cast<std::uint8_t>(original.opaque1),
        .entrance_delay = static_cast<std::uint8_t>(original.animationState),
        .disabled = static_cast<std::uint8_t>(original.disabled),
        .x = original.x,
        .row_number = original.rowIndex,
        .title = BorrowedText(original.title),
        .description = BorrowedText(original.description)};
  }
  return rows;
}

char* OriginalPointer(TimeTrialText text) {
  // These spans borrow writable module arrays; the core never mutates text.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<char*>(const_cast<std::uint8_t*>(text.data()));
}

void StoreAnimation(const std::array<TimeTrialRow, kTimeTrialCount>& rows) {
  for (std::size_t index = 0; index < rows.size(); ++index) {
    Menu_Score_gazetteRows[index].selected =
        static_cast<char>(rows[index].selected);
    Menu_Score_gazetteRows[index].animationState =
        static_cast<char>(rows[index].entrance_delay);
  }
}

class LiveTimeTrialHost final : public TimeTrialHost {
 public:
  Framework::MenuRow& Row(TimeTrialId trial) override {
    return Framework::LiveMenuRow(static_cast<int>(trial));
  }
  Framework::MenuRow& CreateRow(const Framework::RowPlacement& placement,
                                TimeTrialText title) override {
    return Framework::SetLiveMenuItemText(placement, title);
  }
  void SetInformation(TimeTrialText description) override {
    Framework::SetLiveInformationMessageText(description);
  }
  std::int32_t DrawCursor(std::uint8_t step, int x, int y) override {
    return Framework::DrawLiveCursor(
        step, std::bit_cast<std::int32_t>(Framework::PackXy(x, y)));
  }
  void DrawText(std::string_view text, int x, int y,
                std::uint32_t color) override {
    auto* ordering = static_cast<u_long*>(Render_frameScratch.orderingTable);
    if (ordering == nullptr) {
      Score::Refuse("time trial text without a scene ordering table");
      return;
    }
    const std::string terminated(text);
    Render::DrawLiveCompactTextColor(
        terminated.c_str(),
        std::bit_cast<std::int32_t>(Framework::PackXy(x, y)),
        static_cast<int>(color), ordering + kTextOrderingSlot);
  }
  void DrawPanel(const PanelRect& rect, int gradient) override {
    DrawLiveGradientPanel(rect.x, rect.y, rect.width, rect.height, gradient);
  }
  void Refuse(std::string_view reason) override { Score::Refuse(reason); }
};
}  // namespace

// decomp: _printFastestClearTime (src/MENU/MENU9.PRG/238.c:2400),
// MENU9.PRG 80106DE0.
std::size_t FormatLiveFastestClearTime(std::span<char> destination,
                                       std::int32_t round_index,
                                       std::int32_t completion_minutes) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::span bytes(reinterpret_cast<std::uint8_t*>(destination.data()),
                        destination.size());
  const std::size_t written = FormatFastestClearTime(
      bytes, {Menu_Score_clearCount, round_index, completion_minutes},
      Label(Menu_Score_miscInfo, Text_Menu9_miscInfo_Index_noneClear));
  if (written == 0) Refuse("fastest clear text does not fit or has no end");
  return written;
}

// decomp: _printBossRushMenuRow (src/MENU/MENU9.PRG/238.c:2461),
// MENU9.PRG 80106F9C.
void CreateLiveTimeTrialRows() {
  auto rows = Rows();
  if (!CreateTimeTrialRows(rows, Scores(), Labels())) {
    Refuse("time trial labels outside their tables or without an end");
    return;
  }
  for (std::size_t index = 0; index < rows.size(); ++index) {
    auto& original = Menu_Score_gazetteRows[index];
    const TimeTrialRow& row = rows[index];
    original.x = row.x;
    original.disabled = static_cast<char>(row.disabled);
    original.opaque1 = static_cast<char>(row.enabled_style);
    original.title = OriginalPointer(row.title);
    original.description = OriginalPointer(row.description);
  }
  StoreAnimation(rows);
}

// decomp: _leaveBossRushMenu (src/MENU/MENU9.PRG/238.c:2488),
// MENU9.PRG 80107090.
void LeaveLiveTimeTrialMenu() {
  auto rows = Rows();
  LiveTimeTrialHost host;
  if (LeaveTimeTrialMenu(rows, host)) StoreAnimation(rows);
}

// decomp: _renderBossRushRecords (src/MENU/MENU9.PRG/238.c:2507),
// MENU9.PRG 80107120; cursorAnimStep at 801098A0 starts at zero on reload.
void RenderLiveTimeTrialRecords(std::int32_t presentation_step) {
  auto rows = Rows();
  LiveTimeTrialHost host;
  if (RenderTimeTrialRecords(
          rows, Scores(),
          static_cast<TimeTrialId>(Battle_unusedMenu9CursorMemory.gazettePage),
          presentation_step, live_animation, host)) {
    StoreAnimation(rows);
  }
}

void ResetLiveTimeTrialStatics() noexcept { live_animation = {}; }

std::uint64_t LiveTimeTrialRefusals() noexcept { return refusals; }
}  // namespace VS::Menu::Score
