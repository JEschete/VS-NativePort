#pragma once

#include <cstdint>
#include <span>

#include "Menu/MapTerminal.hpp"

namespace VS::Menu::map {
void LiveDrawFrame();
void LiveApplyCamera();
void LiveDarkenBackground();
void LiveDrawPalingDistortion();
int LiveUpdateScreen(unsigned char* step);
int LiveUpdateMapMenu();

// Drain before replacing the game heap or abandoning a map. Failed
// cancellation retains the destination and must block the caller's teardown.
[[nodiscard]] bool DrainMapTerminalLive();
// Completed MENU5 reload: drain owners, restore its physical image state.
// Progression, camera ownership outside MENU5, and immutable text survive.
[[nodiscard]] bool ResetMapTerminalModuleData();
// Cold session: additionally invalidate immutable cached caption bytes.
[[nodiscard]] bool ResetMapTerminalColdState();
[[nodiscard]] const MapScreenState& LiveMapScreenState() noexcept;
[[nodiscard]] std::span<const std::uint8_t> LiveMapPalingText();
[[nodiscard]] std::uint64_t MapTerminalLiveRefusalCount() noexcept;
}  // namespace VS::Menu::map
