// Hand-maintained (tools/port/owned.csv).
// Live binding for the play time clock (State/GameClock.hpp): the play time,
// the tick speed, the vertical sync and the disc queue.
#include "State/GameClockLive.hpp"

#include "Content/CdDriveLive.hpp"
#include "Core/Runtime.hpp"
#include "Legacy/Psx.h"
#include "State/GameClock.hpp"
#include "State/PlayTime.hpp"
#include "State/Timers.hpp"

// decomp: vs_main_gametime (vs_main_gametime@SLUS_010.40)
VS::State::PlayTime VS::State::playTime;

namespace VS::State {
int UpdateLivePlayTimeAndLoading(int vsync) {
  const int result = VS::Legacy::VSync(vsync);
  if (vsync != 1) VS::Core::IdleInstruction();
  VS::Content::UpdateLiveDiscQueue();
  AdvancePlayTime(playTime, tickSpeed);
  return result;
}
}  // namespace VS::State
