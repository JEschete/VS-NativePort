#pragma once

// libetc/libapi timing and kernel surface (plan §2.2). VSync and the root
// counters are timing (strategy R, backed by logical frames in LibEtc.cpp).
// Counter 2 dispatches its registered sound callback; the other kernel and IRQ
// operations have no native meaning and their calls were deleted.

#include <cstdint>

namespace VS::Legacy {

// SetVideoMode's NTSC mode (MODE_NTSC).
inline constexpr std::int32_t kModeNtsc = 0;

// Root counter 2's kernel descriptor (RCntCNT2: DescRC | 2) and its
// interrupt mode (RCntMdINTR).
inline constexpr std::uint32_t kRootCounter2 = 0xF2000000u | 0x02u;
inline constexpr std::int32_t kRootCounterInterruptMode = 0x1000;
// OpenEvent's interrupt event spec and handler mode (EvSpINT, EvMdINTR).
inline constexpr std::int32_t kEventSpecInterrupt = 0x0002;
inline constexpr std::int32_t kEventModeInterrupt = 0x1000;

// The kernel event handler OpenEvent registers, returning the SDK's long
// (Audio::Seq::CounterEventResult); the result is ignored.
using EventHandler = long (*)();

std::int32_t VSync(std::int32_t mode);
std::int32_t VSyncCallback(void (*f)());
std::int32_t SetVideoMode(std::int32_t mode);
std::int32_t SetRCnt(std::uint32_t spec, std::uint16_t target,
                     std::int32_t mode);
std::int32_t GetRCnt(std::uint32_t spec);
std::int32_t StartRCnt(std::uint32_t spec);
std::int32_t StopRCnt(std::uint32_t spec);
std::int32_t OpenEvent(std::uint32_t desc, std::int32_t spec, std::int32_t mode,
                       EventHandler callback);
std::int32_t CloseEvent(std::int32_t event);
std::int32_t EnableEvent(std::int32_t event);
std::int32_t DisableEvent(std::int32_t event);
// Runs root counter 2 forward by `elapsed_clocks` of its clock (system clock /
// 8), firing the enabled event handler at each target.
void AdvanceRootCounter2(std::uint32_t elapsed_clocks);

}  // namespace VS::Legacy
