#include "Event/InterpolationCurveLive.hpp"

#include "Event/ScriptVmLive.hpp"

// decomp: BATTLE.PRG func_800BFEBC (src/BATTLE/BATTLE.PRG/573B8.c:168).
int VS::Event::EvaluateInterpolationCurveEntry(int16_t mode, int16_t frame,
                                               int16_t duration) {
  return VS::Event::EvaluateInterpolationCurve(mode, frame, duration,
                                               VS::Event::LiveScriptVm());
}
