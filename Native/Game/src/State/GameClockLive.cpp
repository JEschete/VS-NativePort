// Hand-maintained (tools/port/owned.csv).
// Live binding for the play time clock (State/GameClock.hpp): the play time
// record, the tick speed, the vertical sync and the disc queue.
#include "State/GameClockLive.hpp"

#include "Content/CdDriveLive.hpp"
#include "Core/Runtime.hpp"
#include "State/GameClock.hpp"

extern "C" {
#include "Legacy/Psx.h"
#include "State/GameTime.h"
#include "State/PlayTime.h"
#include "State/Timers.h"

// decomp: vs_main_gametime (vs_main_gametime@SLUS_010.40)
State_PlayTime State_playTime;
}

namespace VS::State {
PlayTime LivePlayTime() noexcept {
  const auto& record = State_playTime.t;
  return PlayTime{.frames = record.frames,
                  .seconds = record.seconds,
                  .minutes = record.minutes,
                  .hours = record.hours};
}

void StoreLivePlayTime(const PlayTime& time) noexcept {
  auto& record = State_playTime.t;
  record.frames = time.frames;
  record.seconds = time.seconds;
  record.minutes = time.minutes;
  record.hours = time.hours;
}

int UpdateLivePlayTimeAndLoading(int vsync) {
  const int result = VSync(vsync);
  if (vsync != 1) VS::Core::IdleInstruction();
  VS::Content::UpdateLiveDiscQueue();
  PlayTime time = LivePlayTime();
  AdvancePlayTime(time, State_tickSpeed);
  StoreLivePlayTime(time);
  return result;
}
}  // namespace VS::State
