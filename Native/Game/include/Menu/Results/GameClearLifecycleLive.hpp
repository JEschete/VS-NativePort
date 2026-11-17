#pragma once

#include <cstdint>

namespace VS::Menu::Results {
int LiveInitializeGameClear();
void LiveQueueResultsMusic();
int LiveUpdateGameClear();
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): original ABI order.
void LiveDrawBonusReel(int x, int y, int timer, int screen_state);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): original ABI order.
void LiveDrawBonusReelTexture(int x, int y, int texture_index);
void LiveApplyHealingScoreBonus();

// MENUF.PRG physically includes its request/buffer slots, state timer and
// presentation counters (0x8010984C..0x801099F3). Ordinary reload clears
// those views, not BATTLE's phase or any resident resource owner.
void ResetLiveGameClearModuleData();
// Call on interrupted screen exit and before cold heap/disc resets. Cancels
// only this screen's writes, releases its matching request and heap payload,
// and retires only its page loan. Failure retains ownership for retry.
// This does not reset module controller data or any other screen's state.
[[nodiscard]] bool CleanupLiveGameClearOwnedState();

struct LiveGameClearStatus {
  std::int32_t state_timer = 0;
  bool request_bound = false;
  bool buffer_bound = false;
  bool owns_request = false;
  bool owns_buffer = false;
  bool owns_pages = false;
};
[[nodiscard]] LiveGameClearStatus InspectLiveGameClear();
}  // namespace VS::Menu::Results
