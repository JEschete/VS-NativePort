#include "Event/InterpolationCurveLive.h"

#include "Event/ScriptVmLive.hpp"

// decomp: BATTLE.PRG func_800BFEBC (src/BATTLE/BATTLE.PRG/573B8.c:168).
int Event_EvaluateInterpolationCurve(int16_t mode, int16_t frame,
                                     int16_t duration) {
  return VS::Event::EvaluateInterpolationCurve(mode, frame, duration,
                                               VS::Event::LiveScriptVm());
}
