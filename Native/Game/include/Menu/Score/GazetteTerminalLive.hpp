#pragma once

#include <cstdint>

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// MENU9's terminal entries. MON.BIN is decoded only inside a checked live
// heap payload; text offsets, enemy links and actor/queue identities are
// checked before use. The camera, rows, preview, times and statistics use
// their existing typed Live APIs.
void ShowLiveEnemyDescription();
void DrawLiveEnemyNumber(int x, int y, int number, int dimmed);
void InitializeLiveEnemyList();
void RenderLiveEnemyList(int step);
void RenderLiveEnemyViewer(int step, int transition);
void DrawLiveEnemyNavigation(int direction);
int UpdateLiveGazette();
int UpdateLiveGazetteScreen(std::uint8_t* phase);

// Cancels pending writes before releasing their request and owned MON.BIN
// allocation. A stale request generation is never released as a new request.
// Failure leaves ownership intact so the caller cannot reset the heap yet.
[[nodiscard]] bool ReleaseLiveGazetteEnemyData();

// MENU9's physical original image includes the terminal locals and state,
// all initially zero. Does not reset BATTLE cursor memory, Gazette row/text
// owners, camera backups, progression, or the enemy-preview owner.
// Call after preview teardown, and before replacing the game heap.
[[nodiscard]] bool ResetLiveGazetteTerminalModuleState();
[[nodiscard]] std::uint64_t LiveGazetteTerminalRefusals() noexcept;
}  // namespace VS::Menu::Score
