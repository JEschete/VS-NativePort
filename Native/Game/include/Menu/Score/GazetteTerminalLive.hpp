#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Menu/Score/GazetteEncyclopedia.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// MENU9's terminal entries. The encyclopedia owns native catalog/text
// snapshots and derived UI state; no MON.BIN bytes enter the runtime.
// The camera, rows, preview, times and statistics use
// their existing typed Live APIs.
void ShowLiveEnemyDescription();
void DrawLiveEnemyNumber(int x, int y, int number, int dimmed);
void InitializeLiveEnemyList();
void RenderLiveEnemyList(int step);
void RenderLiveEnemyViewer(int step, int transition);
void DrawLiveEnemyNavigation(int direction);
int UpdateLiveGazette();
int UpdateLiveGazetteScreen(std::uint8_t* phase);

// Releases the native snapshot and any frame-owned text. The separate model
// preview owner must still be cancelled before its compatibility heap resets.
[[nodiscard]] bool ReleaseLiveGazetteEnemyData();

// MENU9's physical original image includes the terminal locals and state,
// all initially zero. Does not reset BATTLE cursor memory, Gazette row/text
// owners, camera backups, progression, or the enemy-preview owner.
// Call after preview teardown, and before replacing the game heap.
[[nodiscard]] bool ResetLiveGazetteTerminalModuleState();
[[nodiscard]] std::uint64_t LiveGazetteTerminalRefusals() noexcept;
[[nodiscard]] bool BeginLiveBestiary();
[[nodiscard]] bool FinishLiveBestiary();
[[nodiscard]] std::uint64_t LiveBestiaryGeneration() noexcept;
[[nodiscard]] std::span<const GazetteEnemy> LiveBestiaryEnemies() noexcept;
[[nodiscard]] std::string_view LiveBestiaryDescription() noexcept;
[[nodiscard]] bool LiveBestiaryInformationVisible();
}  // namespace VS::Menu::Score
