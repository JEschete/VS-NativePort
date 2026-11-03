// Hand-maintained (tools/port/owned.csv).
#include "Core/Runtime.hpp"

#include "Core/Log.hpp"

namespace VS::Core {
// SLUS_010.40 _gpuSyncVoidCallback (main.c:8227), also 0x80042734,
// 0x8004273C, 0x80042744, 0x80044C6C, 0x800490A8 and 0x80049130.
void IgnoreCall() noexcept {}

// SLUS_010.40 nop5 (main.c:9731), also 0x800455E4 and 0x800455EC.
int ReturnZero() noexcept { return 0; }

// SLUS_010.40 vs_main_nop9 (main.c:11663) and its copy at 0x800490A0.
void IgnoreDiagnostic(int code, int detail) {
  VS_LOG_TRACE("diagnostic", "retail error report {:#x} ({}) ignored", code,
               detail);
}

// SLUS_010.40 _asmNop (main.c:11661).
void IdleInstruction() noexcept {}

// BATTLE.PRG func_8007B0FC (146C.c:8237), also 0x8007B104, 0x8007BF04 and
// 0x8007DF40.
void NoOperation() noexcept {}

// BATTLE.PRG vs_battle_nop0 (146C.c:1354) and its copy at 0x8006C478.
void IgnoreCallback() noexcept {}
}  // namespace VS::Core
