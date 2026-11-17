#pragma once

#include <cstdint>

namespace VS::Menu {
// Original BATTLE.PRG scene-message entries, with a checked game-heap owner
// and formatting bounded by its 112-byte payload.
void ShowLiveSceneNotification(std::int32_t kind, std::int32_t subject,
                               std::int32_t timed);
std::int32_t IsLiveSceneNotificationPending();
void ReleaseLiveSceneNotification();
// Drain before aliases or heap identity are discarded. Invalid ownership
// is diagnosed and leaves the owner intact so the boundary can refuse.
[[nodiscard]] bool CleanupLiveSceneNotificationOwnedState();
[[nodiscard]] std::uint64_t RefusedLiveSceneNotificationCount();
}  // namespace VS::Menu
