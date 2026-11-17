#pragma once

#include "Menu/Results/TimeAttackTerminal.hpp"

namespace VS::Menu::Results {
int LiveInitializeTimeAttackResults();
int LiveInitializeTimeAttackStart();
int LiveUpdateTimeAttackResults();
int LiveUpdateTimeAttackStart();
int LiveInitializeResultsScreen();
int LiveUpdateResultsPresentation();
int LiveUpdateResultsScreen(unsigned char* caller_state);

// Only the additional MENUF file-backed views at 0x80109854..0x80109863
// and record/palette fields at 0x801098A4..0x801099EB. Call alongside
// ResetLiveGameClearModuleData for the shared presentation counters.
// Neither reset releases resources, resets BATTLE/Framework callers, nor
// changes resident scores, music, tick speed or framebuffer effects.
void ResetLiveTimeAttackModuleData();

// Cancels this packet's writes and releases matching request, buffer and
// texture-page owners. Retains unreleased owners on refusal. Also stops
// countdown controls it enabled; Render still owns deferred effect-packet
// retirement. Boot::ResetLiveSessionHeap abandons the stopped frame producer's
// bindings and frees those packets before replacing the heap. Call before any
// heap/queue/page/effect identity reuse, not as ordinary module-data reset.
[[nodiscard]] bool CleanupLiveTimeAttackOwnedState();

struct LiveTimeAttackLoadStatus {
  bool request_bound = false;
  bool buffer_bound = false;
  bool owns_request = false;
  bool owns_buffer = false;
  bool owns_pages = false;
};
[[nodiscard]] LiveTimeAttackLoadStatus InspectLiveTimeAttackLoad(
    TimeAttackAsset asset);
[[nodiscard]] bool LiveTimeAttackOwnsCountdownControls();
}  // namespace VS::Menu::Results
