#pragma once

#include <cstdint>

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
void CreateLiveTitleRows();
void RenderLiveTitles(std::int32_t presentation_step);
void DrawLiveTitleNumber(std::int32_t x, std::int32_t y, std::int32_t number,
                         std::int32_t dimmed);
void CreateLiveStatisticRows();
void RenderLiveStatistics(std::int32_t presentation_step);

// MENU9 reload and cold reset restore only the two cursor bytes at
// 80109898/8010989A. Shared rows, score data and scrolling state are separate.
void ResetLiveGazetteStatisticsStatics() noexcept;
[[nodiscard]] std::uint64_t LiveGazetteStatisticsRefusals() noexcept;
}  // namespace VS::Menu::Score
