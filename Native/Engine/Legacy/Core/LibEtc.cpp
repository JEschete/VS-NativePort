#include "Legacy/Core/LibEtc.h"

#include <chrono>
#include <cstdint>

#include "Core/Unported.h"
#include "Platform/GameFiber.hpp"

// libetc timing (plan §2.2, strategy R). The PS1 waited for vertical blank;
// natively the game fiber hands control back to the host loop, which presents,
// samples input and resumes it on the next logical tick.

namespace {

using Clock = std::chrono::steady_clock;

int g_ticks = 0;
void (*g_vblankCallback)(void) = nullptr;
Clock::time_point g_rcnt2Start = Clock::now();
unsigned g_rcnt2Target = 0;
std::uint64_t g_rcnt2Clocks = 0;
bool g_rcnt2Running = false;
bool g_rcnt2Enabled = false;
long (*g_rcnt2Callback)(void) = nullptr;

// root counter 2 counts the system clock / 8 (33.8688 MHz / 8)
constexpr double Rcnt2Hz = 33868800.0 / 8.0;

void WaitOneFrame() {
  VS::Platform::GameFiber::YieldToHost();
  ++g_ticks;
  Legacy_AdvanceCounter2(33868800u / 8u / 60u);
  if (g_vblankCallback != nullptr) {
    g_vblankCallback();
  }
}

}  // namespace

extern "C" {

int VSync(int mode) {
  if (mode < 0) {
    return g_ticks;  // vertical blanks since boot
  }
  if (mode == 1) {
    // horizontal lines since the last blank: the frame is always complete
    // natively, so report a full NTSC field
    return 263;
  }
  const int frames = mode == 0 ? 1 : mode;
  for (int i = 0; i < frames; ++i) {
    WaitOneFrame();
  }
  return 0;
}

int VSyncCallback(void (*f)(void)) {
  g_vblankCallback = f;
  return 0;
}

long SetVideoMode(long mode) {
  (void)mode;  // NTSC only
  return MODE_NTSC;
}

long SetRCnt(unsigned long spec, unsigned short target, long mode) {
  (void)mode;
  if (spec == RCntCNT2) {
    g_rcnt2Target = target == 0 ? 65536u : target;
    g_rcnt2Clocks = 0;
  }
  return 1;
}

long GetRCnt(unsigned long spec) {
  if (spec != RCntCNT2) {
    VS_UNPORTED("libetc:GetRCnt (counter other than 2)");
    return 0;
  }
  const double seconds =
      std::chrono::duration<double>(Clock::now() - g_rcnt2Start).count();
  return static_cast<long>(static_cast<unsigned long long>(seconds * Rcnt2Hz) &
                           0xFFFF);
}

long StartRCnt(unsigned long spec) {
  if (spec == RCntCNT2) {
    g_rcnt2Start = Clock::now();
    g_rcnt2Running = true;
  }
  return 1;
}

long StopRCnt(unsigned long spec) {
  if (spec == RCntCNT2) g_rcnt2Running = false;
  return 1;
}

long OpenEvent(unsigned long desc, long spec, long mode,
               long (*callback)(void)) {
  if (desc != RCntCNT2 || spec != EvSpINT || mode != EvMdINTR ||
      callback == nullptr) {
    return 0;
  }
  if (g_rcnt2Callback != nullptr) return -1;
  g_rcnt2Callback = callback;
  g_rcnt2Enabled = false;
  return 1;
}

long CloseEvent(long event) {
  if (event == 1) {
    g_rcnt2Callback = nullptr;
    g_rcnt2Enabled = false;
  }
  return 1;
}

long EnableEvent(long event) {
  if (event == 1 && g_rcnt2Callback != nullptr) g_rcnt2Enabled = true;
  return 1;
}

long DisableEvent(long event) {
  if (event == 1) g_rcnt2Enabled = false;
  return 1;
}

void Legacy_AdvanceCounter2(unsigned elapsedClocks) {
  if (!g_rcnt2Running || g_rcnt2Target == 0) return;
  g_rcnt2Clocks += elapsedClocks;
  while (g_rcnt2Clocks >= g_rcnt2Target) {
    g_rcnt2Clocks -= g_rcnt2Target;
    if (g_rcnt2Enabled && g_rcnt2Callback != nullptr) g_rcnt2Callback();
    if (!g_rcnt2Running || g_rcnt2Target == 0) return;
  }
}

}  // extern "C"
