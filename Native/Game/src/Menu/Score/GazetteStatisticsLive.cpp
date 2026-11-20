#include "Menu/Score/GazetteStatisticsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Battle/StatusEffects.hpp"
#include "Content/StringTables.hpp"
#include "Core/Log.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Score/GazetteStatistics.hpp"
#include "Menu/Score/Records.hpp"
#include "Menu/Score/ScoreRankLive.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Progress.hpp"

namespace menu9_misc_info = VS::Content::text::menu9_prg::misc_info;

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

GazetteText BorrowedText(const std::uint8_t* pointer) {
  const std::array tables{Bytes(titleText), Bytes(titleDescriptions),
                          Bytes(statHeaders), Bytes(statDescriptions),
                          Bytes(miscInfo)};
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
    const auto& original = gazetteRows[index];
    rows[index] = {.selected = original.selected,
                   .unlocked = original.opaque1,
                   .highlight_step = original.animation_state,
                   .disabled = original.disabled,
                   .x = original.x,
                   .number = original.row_index,
                   .title = BorrowedText(original.title),
                   .description = BorrowedText(original.description)};
  }
  return rows;
}

std::uint8_t* OriginalPointer(GazetteText text) {
  // These spans borrow writable module arrays; the policy never changes text.
  return const_cast<std::uint8_t*>(text.data());
}

void StoreRows(std::span<const GazetteRecordRow> rows, bool labels) {
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& row = rows[index];
    auto& original = gazetteRows[index];
    original.selected = row.selected;
    original.animation_state = row.highlight_step;
    if (labels) {
      original.opaque1 = row.unlocked;
      original.disabled = row.disabled;
      original.x = row.x;
      original.row_index = row.number;
      original.title = OriginalPointer(row.title);
      original.description = OriginalPointer(row.description);
    }
  }
}

GazetteTitleLabels TitleLabels() {
  GazetteTitleLabels labels;
  for (std::size_t index = 0; index < kGazetteTitleCount; ++index) {
    labels.titles[index] = Label(titleText, index);
    labels.descriptions[index] = Label(titleDescriptions, index);
  }
  labels.locked_title = Label(miscInfo, menu9_misc_info::index::kNoneClear);
  labels.locked_description =
      Label(miscInfo, menu9_misc_info::index::kConditionsNotMet);
  return labels;
}

GazetteStatisticLabels StatisticLabels() {
  GazetteStatisticLabels labels;
  for (std::size_t index = 0; index < kGazetteStatisticCount; ++index) {
    labels.titles[index] = Label(statHeaders, index);
    labels.descriptions[index] = Label(statDescriptions, index);
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
  if (!CreateTitleRows(rows,
                       static_cast<std::uint32_t>(VS::State::scoreData.titles),
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
                   {VS::Battle::menu9CursorMemory.title_page,
                    VS::Battle::menu9CursorMemory.title_row},
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
      .current = {.map_completion = mapCompletion,
                  .opened_chest_count = openedChestCount,
                  .clear_count = clearCount,
                  .rank = {}},
      .best = ReadLiveScoreRecord(),
      .best_clear_round = VS::State::scoreData.clear_count,
      .no_clear = Label(miscInfo, menu9_misc_info::index::kNoneClear)};
  LiveRecordHost host;
  if (RenderStatistics(rows, values,
                       {VS::Battle::menu9CursorMemory.record_time_page,
                        VS::Battle::menu9CursorMemory.record_time_row},
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
