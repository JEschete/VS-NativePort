#include "Legacy/Core/LibEtc.h"
#include "Core/Unported.h"
#include "Platform/GameFiber.hpp"

#include <chrono>

// libetc timing (plan §2.2, strategy R). The PS1 waited for vertical blank; natively the game fiber hands control back
// to the host loop, which presents, samples input and resumes it on the next logical tick.

namespace {

using Clock = std::chrono::steady_clock;

int g_ticks = 0;
void (*g_vblankCallback)(void) = nullptr;
Clock::time_point g_rcnt2Start = Clock::now();

// root counter 2 counts the system clock / 8 (33.8688 MHz / 8)
constexpr double Rcnt2Hz = 33868800.0 / 8.0;

void WaitOneFrame()
{
    VS::Platform::GameFiber::YieldToHost();
    ++g_ticks;
    if (g_vblankCallback != nullptr) {
        g_vblankCallback();
    }
}

} // namespace

extern "C" {

int VSync(int mode)
{
    if (mode < 0) {
        return g_ticks;  // vertical blanks since boot
    }
    if (mode == 1) {
        // horizontal lines since the last blank: the frame is always complete natively, so report a full NTSC field
        return 263;
    }
    const int frames = mode == 0 ? 1 : mode;
    for (int i = 0; i < frames; ++i) {
        WaitOneFrame();
    }
    return 0;
}

int VSyncCallback(void (*f)(void))
{
    g_vblankCallback = f;
    return 0;
}

long SetVideoMode(long mode)
{
    (void)mode;  // NTSC only
    return MODE_NTSC;
}

long SetRCnt(unsigned long spec, unsigned short target, long mode)
{
    (void)spec;
    (void)target;
    (void)mode;
    return 1;
}

long GetRCnt(unsigned long spec)
{
    if (spec != RCntCNT2) {
        VS_UNPORTED("libetc:GetRCnt (counter other than 2)");
        return 0;
    }
    const double seconds = std::chrono::duration<double>(Clock::now() - g_rcnt2Start).count();
    return static_cast<long>(static_cast<unsigned long long>(seconds * Rcnt2Hz) & 0xFFFF);
}

long StartRCnt(unsigned long spec)
{
    if (spec == RCntCNT2) {
        g_rcnt2Start = Clock::now();
    }
    return 1;
}

long StopRCnt(unsigned long spec)
{
    (void)spec;
    return 1;
}

} // extern "C"
