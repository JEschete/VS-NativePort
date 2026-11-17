#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Menu/Framework/Framework.hpp"
#include "Menu/Score/ScoreRank.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kGazetteTitleCount = 32;
inline constexpr std::size_t kGazetteStatisticCount = 23;
inline constexpr std::size_t kGazetteVisibleRecords = 8;

using GazetteText = std::span<const std::uint8_t>;

struct GazetteRecordRow {
  std::uint8_t selected = 0;
  std::uint8_t unlocked = 0;
  std::uint8_t highlight_step = 0;
  std::uint8_t disabled = 0;
  std::int16_t x = 0;
  std::int16_t number = 0;
  GazetteText title;
  GazetteText description;
};

struct GazetteTitleLabels {
  std::array<GazetteText, kGazetteTitleCount> titles;
  std::array<GazetteText, kGazetteTitleCount> descriptions;
  GazetteText locked_title;
  GazetteText locked_description;
};

struct GazetteStatisticLabels {
  std::array<GazetteText, kGazetteStatisticCount> titles;
  std::array<GazetteText, kGazetteStatisticCount> descriptions;
};

struct GazetteRecordCursor {
  std::uint8_t first_row = 0;
  std::uint8_t selected_row = 0;
};

struct GazetteRecordAnimation {
  std::uint8_t cursor_step = 0;
};

struct GazetteStatisticsValues {
  // Current counts are halfwords; saved percentage products wrap as words.
  ScoreScreenProgress current;
  ScoreRecord best;
  std::uint32_t best_clear_round = 0;
  GazetteText no_clear;
};

struct TitleNumber {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t value = 0;
  bool dimmed = false;
};

class TitleNumberTarget {
 public:
  virtual ~TitleNumberTarget() = default;
  virtual bool DrawQuad(const Render::UiTexturedQuad& quad) = 0;
};

class GazetteRecordHost {
 public:
  GazetteRecordHost() = default;
  GazetteRecordHost(const GazetteRecordHost&) = delete;
  GazetteRecordHost& operator=(const GazetteRecordHost&) = delete;
  GazetteRecordHost(GazetteRecordHost&&) = delete;
  GazetteRecordHost& operator=(GazetteRecordHost&&) = delete;
  virtual ~GazetteRecordHost() = default;

  // Consume/copy text synchronously; value rows borrow a per-call buffer.
  virtual Framework::MenuRow& CreateRow(
      const Framework::RowPlacement& placement, GazetteText text) = 0;
  virtual Framework::MenuRow& Row() = 0;
  virtual void SetInformation(GazetteText description) = 0;
  virtual std::int32_t DrawCursor(std::uint8_t step, int x, int y) = 0;
  virtual void DrawNumber(const TitleNumber& number) = 0;
  // Both lists reuse framework row zero. The Live binding delegates scrolling
  // to GazetteRows' existing renderer and its separate arrow animation.
  virtual void DrawRow(bool scrolling) = 0;
  virtual std::int32_t MeasureText(GazetteText text) = 0;
  virtual void Refuse(std::string_view reason) = 0;
};

// F02814/F02824: validate the chosen labels before writing the first 32/23
// rows. Extra rows are untouched. All required labels must contain an end.
[[nodiscard]] bool CreateTitleRows(std::span<GazetteRecordRow> rows,
                                   std::uint32_t titles,
                                   const GazetteTitleLabels& labels);
[[nodiscard]] bool CreateStatisticRows(std::span<GazetteRecordRow> rows,
                                       const GazetteStatisticLabels& labels);

// F02815/F02825: first_row must leave eight rows in the list. selected_row
// retains the original byte comparison even when it selects no visible row.
// Visible titles and a newly selected description must be terminated.
// Invalid inputs are refused before selection, cursor state or drawing.
[[nodiscard]] bool RenderTitles(std::span<GazetteRecordRow> rows,
                                GazetteRecordCursor cursor,
                                std::int32_t presentation_step,
                                GazetteRecordAnimation& animation,
                                GazetteRecordHost& host);
[[nodiscard]] bool RenderStatistics(std::span<GazetteRecordRow> rows,
                                    const GazetteStatisticsValues& values,
                                    GazetteRecordCursor cursor,
                                    std::int32_t presentation_step,
                                    GazetteRecordAnimation& animation,
                                    GazetteRecordHost& host);

// F02816: two digits, units first. Coordinates retain their low 16 bits.
// Values outside 0..99 are a native refusal; a failed emission stops drawing.
[[nodiscard]] bool DrawTitleNumber(const TitleNumber& number,
                                   TitleNumberTarget& target);
}  // namespace VS::Menu::Score
