#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// The statistics caller must pass the actual remaining output span, not an
// unbounded pointer. Returns the bytes written including the terminator.
std::size_t FormatLiveFastestClearTime(std::span<char> destination,
                                       std::int32_t round_index,
                                       std::int32_t completion_minutes);
void CreateLiveTimeTrialRows();
void LeaveLiveTimeTrialMenu();
void RenderLiveTimeTrialRecords(std::int32_t presentation_step);

// Wire into both MENU9 reload and cold-session reset. Only the original
// function-local cursor byte is owned here; the shared gazette rows are not.
void ResetLiveTimeTrialStatics() noexcept;
[[nodiscard]] std::uint64_t LiveTimeTrialRefusals() noexcept;
}  // namespace VS::Menu::Score
