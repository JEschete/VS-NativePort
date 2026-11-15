#pragma once
// Hand-maintained (tools/port/owned.csv).

// Bindings from the generated event VM frame update to the typed splash and
// weather tweens and the scanline overlay (ScriptEffectCommands.hpp)
// (tools/port/copy_fixups.py OWNED_CALL_BINDINGS).
#include "Event/ScriptEffectCommands.hpp"

namespace VS::Event {
// decomp: func_800BB874 (src/BATTLE/BATTLE.PRG/4A0A8.c:2937): draws the
// overlay into the frame's primitive buffer. A frame whose buffer cannot
// hold it is skipped.
void DrawLiveScanlineOverlay();
// decomp: func_800BB9B0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2971)
void UpdateLiveImageTweens();
// decomp: func_800BBAFC (src/BATTLE/BATTLE.PRG/4A0A8.c:2995)
void UpdateLiveWeatherTween();
// The splash tween record (D_800F4B30) of `layer`; idle past the records.
[[nodiscard]] SplashTween LiveSplashTween(int layer);
// Stores `layer`'s splash tween record, keeping its reserved byte; nothing
// happens past the records.
void SetLiveSplashTween(int layer, const SplashTween& tween);

// The event's weather tween (D_800F4B88) as a typed value, and back; the
// store leaves the record's two reserved bytes alone.
[[nodiscard]] WeatherTween LiveWeatherTween();
void StoreLiveWeatherTween(const WeatherTween& tween);
}  // namespace VS::Event
