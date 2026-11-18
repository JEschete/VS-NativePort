#include "Menu/Score/GazetteStatisticsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Audio/Seq/Channel.h"
#include "Battle/StatusEffects.h"
#include "Content/StringTables.h"
#include "Core/Log.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Score/GazetteStatistics.hpp"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreRankLive.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Progress.h"

namespace VS::Menu::Score {
namespace {
GazetteRecordAnimation title_animation;
GazetteRecordAnimation statistic_animation;
std::uint64_t refusals = 0;

void Refuse(std::string_view reason) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("menu.score", "gazette statistics: {} ({} refusals)",
                      reason, refusals);
  }
}

template <std::size_t Size>
GazetteText Bytes(const u_short (&table)[Size]) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return {reinterpret_cast<const std::uint8_t*>(table), sizeof table};
}

template <std::size_t Size>
GazetteText Label(const u_short (&table)[Size], std::size_t index) {
  if (index >= Size || table[index] >= Size) return {};
  return Bytes(table).subspan(table[index] * sizeof(u_short));
}

GazetteText BorrowedText(const char* pointer) {
  const std::array tables{
      Bytes(Menu_Score_titleText), Bytes(Menu_Score_titleDescriptions),
      Bytes(Menu_Score_statHeaders), Bytes(Menu_Score_statDescriptions),
      Bytes(Menu_Score_miscInfo)};
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

std::array<GazetteRecordRow, kGazetteTitleCount> Rows() {
  std::array<GazetteRecordRow, kGazetteTitleCount> rows{};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& original = Menu_Score_gazetteRows[index];
    rows[index] = {
        .selected = static_cast<std::uint8_t>(original.selected),
        .unlocked = static_cast<std::uint8_t>(original.opaque1),
        .highlight_step = static_cast<std::uint8_t>(original.animationState),
        .disabled = static_cast<std::uint8_t>(original.disabled),
        .x = original.x,
        .number = original.rowIndex,
        .title = BorrowedText(original.title),
        .description = BorrowedText(original.description)};
  }
  return rows;
}

char* OriginalPointer(GazetteText text) {
  // These spans borrow writable module arrays; the policy never changes text.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<char*>(const_cast<std::uint8_t*>(text.data()));
}

void StoreRows(std::span<const GazetteRecordRow> rows, bool labels) {
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& row = rows[index];
    auto& original = Menu_Score_gazetteRows[index];
    original.selected = static_cast<char>(row.selected);
    original.animationState = static_cast<char>(row.highlight_step);
    if (labels) {
      original.opaque1 = static_cast<char>(row.unlocked);
      original.disabled = static_cast<char>(row.disabled);
      original.x = row.x;
      original.rowIndex = row.number;
      original.title = OriginalPointer(row.title);
      original.description = OriginalPointer(row.description);
    }
  }
}

GazetteTitleLabels TitleLabels() {
  GazetteTitleLabels labels;
  for (std::size_t index = 0; index < kGazetteTitleCount; ++index) {
    labels.titles[index] = Label(Menu_Score_titleText, index);
    labels.descriptions[index] = Label(Menu_Score_titleDescriptions, index);
  }
  labels.locked_title =
      Label(Menu_Score_miscInfo, Text_Menu9_miscInfo_Index_noneClear);
  labels.locked_description =
      Label(Menu_Score_miscInfo, Text_Menu9_miscInfo_Index_conditionsNotMet);
  return labels;
}

GazetteStatisticLabels StatisticLabels() {
  GazetteStatisticLabels labels;
  for (std::size_t index = 0; index < kGazetteStatisticCount; ++index) {
    labels.titles[index] = Label(Menu_Score_statHeaders, index);
    labels.descriptions[index] = Label(Menu_Score_statDescriptions, index);
  }
  return labels;
}

class LiveNumberTarget final : public TitleNumberTarget {
 public:
  bool DrawQuad(const Render::UiTexturedQuad& quad) override {
    auto* entry =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0);
    if (entry == nullptr) return false;
    auto* storage =
        Render::ReserveLivePrimitiveWords(Render::kUiTexturedQuadWords);
    if (storage == nullptr) return false;
    Render::UiTexturedQuadPacket previous{};
    std::ranges::copy(std::span(storage, Render::kUiTexturedQuadWords),
                      previous.begin());
    Render::LivePacketQueue queue(entry);
    return queue.Emit(
        Render::BuildUiTexturedQuadPacket(quad, previous, queue.ChainLink()));
  }
};

class LiveRecordHost final : public GazetteRecordHost {
 public:
  Framework::MenuRow& CreateRow(const Framework::RowPlacement& placement,
                                GazetteText text) override {
    return Framework::SetLiveMenuItemText(placement, text);
  }
  Framework::MenuRow& Row() override { return Framework::LiveMenuRow(0); }
  void SetInformation(GazetteText description) override {
    Framework::SetLiveInformationMessageText(description);
  }
  std::int32_t DrawCursor(std::uint8_t step, int x, int y) override {
    return Framework::DrawLiveCursor(
        step, std::bit_cast<std::int32_t>(Framework::PackXy(x, y)));
  }
  void DrawNumber(const TitleNumber& number) override {
    DrawLiveTitleNumber(number.x, number.y, number.value,
                        number.dimmed ? 1 : 0);
  }
  void DrawRow(bool scrolling) override {
    auto* row = Framework::LiveMenuItem(0);
    if (scrolling) {
      RenderLiveScrollableRow(row);
    } else {
      Framework::DrawLiveMenuItem(row);
    }
  }
  std::int32_t MeasureText(GazetteText text) override {
    return Render::MeasureEncodedTextLine(
        text, Render::LiveVariableFontGlyphWidths());
  }
  void Refuse(std::string_view reason) override { Score::Refuse(reason); }
};
}  // namespace

// decomp: _setTitleRows (src/MENU/MENU9.PRG/238.c:1722), 801056B8.
void CreateLiveTitleRows() {
  auto rows = Rows();
  if (!CreateTitleRows(
          rows, static_cast<std::uint32_t>(Audio_Seq_unusedScoredata.titles),
          TitleLabels())) {
    Refuse("title labels outside their tables or without an end");
    return;
  }
  StoreRows(rows, true);
}

// decomp: _renderTitleMenu (src/MENU/MENU9.PRG/238.c:1756), 8010579C.
void RenderLiveTitles(std::int32_t presentation_step) {
  auto rows = Rows();
  LiveRecordHost host;
  if (RenderTitles(rows,
                   {Battle_unusedMenu9CursorMemory.titlePage,
                    Battle_unusedMenu9CursorMemory.titleRow},
                   presentation_step, title_animation, host)) {
    StoreRows(rows, false);
  }
}

// decomp: _renderTitleNo (src/MENU/MENU9.PRG/238.c:1835), 80105A0C.
void DrawLiveTitleNumber(std::int32_t x, std::int32_t y, std::int32_t number,
                         std::int32_t dimmed) {
  LiveNumberTarget target;
  if (!DrawTitleNumber({x, y, number, dimmed != 0}, target)) {
    Refuse("title number outside 0..99 or no primitive capacity/ordering");
  }
}

// decomp: _setStatText (src/MENU/MENU9.PRG/238.c:2201), 80106780.
void CreateLiveStatisticRows() {
  auto rows = Rows();
  if (!CreateStatisticRows(rows, StatisticLabels())) {
    Refuse("statistic labels outside their tables or without an end");
    return;
  }
  StoreRows(std::span(rows).first(kGazetteStatisticCount), true);
}

// decomp: _renderBossRushDetails (src/MENU/MENU9.PRG/238.c:2226), 80106808.
void RenderLiveStatistics(std::int32_t presentation_step) {
  auto rows = Rows();
  const GazetteStatisticsValues values{
      .current = {.map_completion = Menu_Score_mapCompletion,
                  .opened_chest_count = Menu_Score_openedChestCount,
                  .clear_count = Menu_Score_clearCount,
                  .rank = {}},
      .best = ReadLiveScoreRecord(),
      .best_clear_round = Audio_Seq_unusedScoredata.clearCount,
      .no_clear =
          Label(Menu_Score_miscInfo, Text_Menu9_miscInfo_Index_noneClear)};
  LiveRecordHost host;
  if (RenderStatistics(rows, values,
                       {Battle_unusedMenu9CursorMemory.recordTimePage,
                        Battle_unusedMenu9CursorMemory.recordTimeRow},
                       presentation_step, statistic_animation, host)) {
    StoreRows(std::span(rows).first(kGazetteStatisticCount), false);
  }
}

void ResetLiveGazetteStatisticsStatics() noexcept {
  title_animation = {};
  statistic_animation = {};
}

std::uint64_t LiveGazetteStatisticsRefusals() noexcept { return refusals; }
}  // namespace VS::Menu::Score
