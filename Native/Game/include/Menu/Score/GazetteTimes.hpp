#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Menu/Framework/Framework.hpp"
#include "Menu/Score/GazetteCamera.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kTimeTrialCount = 8;
inline constexpr std::size_t kTimeTrialPlaces = 3;
inline constexpr std::size_t kFastestClearTimeBytes = 32;

using TimeTrialText = std::span<const std::uint8_t>;
enum class TimeTrialId : std::uint8_t {};

struct FastestClearTime {
  std::uint16_t clear_count = 0;
  std::int32_t round_index = 0;
  // The caller supplies completionTimeMinutes, not seconds.
  std::int32_t completion_minutes = 0;
};

// F02828: R<round+1>, spacing 12, hours:minutes, capped at 99:59.
// Signed division and the wrapping round increment match the instructions.
// Returns bytes including the game terminator. A short destination or an
// unterminated no-clear label is refused without writing anything.
std::size_t FormatFastestClearTime(std::span<std::uint8_t> destination,
                                   const FastestClearTime& value,
                                   TimeTrialText no_clear) noexcept;

struct TimeTrialTime {
  std::uint8_t minutes = 0;
  std::uint8_t seconds = 0;
  std::uint8_t centiseconds = 0;
};

struct TimeTrialRecord {
  // The import binding maps the exact 24-bit 0x800000 sentinel to no time.
  std::optional<TimeTrialTime> time;
  std::uint8_t round_index = 0;
};
using TimeTrialResults = std::array<TimeTrialRecord, kTimeTrialPlaces>;
using TimeTrialScores = std::array<TimeTrialResults, kTimeTrialCount>;

struct TimeTrialRow {
  std::uint8_t selected = 0;
  std::uint8_t enabled_style = 0;
  std::uint8_t entrance_delay = 0;
  std::uint8_t disabled = 0;
  std::int16_t x = 0;
  std::int16_t row_number = 0;
  TimeTrialText title;
  TimeTrialText description;
};

struct TimeTrialLabels {
  std::array<TimeTrialText, kTimeTrialCount> titles;
  std::array<TimeTrialText, kTimeTrialCount> descriptions;
  TimeTrialText missing_title;
  TimeTrialText missing_description;
};

struct TimeTrialAnimation {
  std::uint8_t cursor_step = 0;
};

class TimeTrialHost {
 public:
  TimeTrialHost() = default;
  TimeTrialHost(const TimeTrialHost&) = delete;
  TimeTrialHost& operator=(const TimeTrialHost&) = delete;
  TimeTrialHost(TimeTrialHost&&) = delete;
  TimeTrialHost& operator=(TimeTrialHost&&) = delete;
  virtual ~TimeTrialHost() = default;

  virtual Framework::MenuRow& Row(TimeTrialId trial) = 0;
  virtual Framework::MenuRow& CreateRow(
      const Framework::RowPlacement& placement, TimeTrialText title) = 0;
  virtual void SetInformation(TimeTrialText description) = 0;
  virtual std::int32_t DrawCursor(std::uint8_t step, int x, int y) = 0;
  virtual void DrawText(std::string_view text, int x, int y,
                        std::uint32_t color) = 0;
  virtual void DrawPanel(const PanelRect& rect, int gradient) = 0;
  virtual void Refuse(std::string_view reason) = 0;
};

// F02829: configures only the first eight rows. row_number is preserved.
// Missing/unterminated labels and short row spans are native refusals.
[[nodiscard]] bool CreateTimeTrialRows(std::span<TimeTrialRow> rows,
                                       const TimeTrialScores& scores,
                                       const TimeTrialLabels& labels);

// F02830: active rows slide to x 320. Pending rows become delay 255,
// which the original decrements as an unsigned byte on later render calls.
[[nodiscard]] bool LeaveTimeTrialMenu(std::span<TimeTrialRow> rows,
                                      TimeTrialHost& host);

// F02831: animates the eight choices and draws all three records even while
// entering/leaving. Selection, information and reference time appear only
// when all rows were already idle at entry and presentation_step is eight.
// Short rows, an invalid trial, an out-of-range seven-bit round, an
// unterminated entering-row title or an unselected trial's unterminated
// description are refused before changing state or drawing.
[[nodiscard]] bool RenderTimeTrialRecords(std::span<TimeTrialRow> rows,
                                          const TimeTrialScores& scores,
                                          TimeTrialId selected_trial,
                                          std::int32_t presentation_step,
                                          TimeTrialAnimation& animation,
                                          TimeTrialHost& host);
}  // namespace VS::Menu::Score
