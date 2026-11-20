// Hand-maintained (tools/port/owned.csv).
// Live binding for the play time clock (State/GameClock.hpp): the play time,
// the tick speed, the logical tick and the disc queue.
#include "State/GameClockLive.hpp"

#include "Content/CdDriveLive.hpp"
#include "Core/Runtime.hpp"
#include "Platform/LogicalTicks.hpp"
#include "State/GameClock.hpp"
#include "State/PlayTime.hpp"
#include "State/Timers.hpp"

// decomp: vs_main_gametime (vs_main_gametime@SLUS_010.40)
VS::State::PlayTime VS::State::playTime;

namespace VS::State {
void UpdateLivePlayTimeAndLoading(int ticks) {
  VS::Platform::WaitTicks(ticks);
  VS::Core::IdleInstruction();
  VS::Content::UpdateLiveDiscQueue();
  AdvancePlayTime(playTime, tickSpeed);
}
}  // namespace VS::State
