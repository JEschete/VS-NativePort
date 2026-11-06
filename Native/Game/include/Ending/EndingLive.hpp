// Hand-maintained (tools/port/owned.csv).
#pragma once

// Live binding for ENDING.PRG's staff roll (Credits/StaffRollCore.hpp) and
// clear results (ClearResultsCore.hpp): the game's score, playthrough and room
// records, the module image's script, art and results tables, the disc queue,
// the game heap and the current frame's GPU buffers (EndingLive.cpp).
namespace VS::Ending {
// vs_ending_exec: the staff roll, then the clear-progress commit and the
// results screen.
void LiveRunStaffRoll();
// _do_updateClearGameStats over the game's records.
void LiveCommitClearProgress();
// func_8006C3CC: the rank screen's panels at `fade_frame`, queued into the
// current frame's ordering table.
void LiveDrawRankBackground(int fade_frame);
}  // namespace VS::Ending
