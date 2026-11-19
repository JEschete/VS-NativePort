#pragma once
// Hand-maintained (tools/port/owned.csv).
// The original MENUF.PRG cube-puzzle start, results and quit screen
// signatures bound to vs_battle_submenuState, the Results screen state,
// clutBuf, the par time and rank cap, the puzzle clock, the
// score, Circle, the disc queue, the heap, LoadImage, the texture pages, the
// sound effects and the live cube-puzzle draws. A TIM, palette or record table
// the asset cannot hold, a missing minimap room, a missing request or buffer
// and a rank outside the points table are refused with a throttled warning.

namespace VS::Menu::Results {
void LivePulseCubePuzzleClut(int blend_weight);
int LiveInitializeCubePuzzleStart();
int LiveInitializeCubePuzzleResults();
int LiveInitializeCubePuzzleQuit();
int LiveUpdateCubePuzzleStart();
int LiveUpdateCubePuzzleResults();
int LiveUpdateCubePuzzleQuit();
// The IQ.DIS and ESC.DIS requests and buffers (the initializers' statics),
// cleared when MENUF.PRG reloads (VS::Boot::ResetMenufStatics).
void ResetLiveCubePuzzleLifecycleStatics();
}  // namespace VS::Menu::Results
