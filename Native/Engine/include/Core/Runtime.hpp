#pragma once
// Hand-maintained (tools/port/owned.csv).

// The original's empty handlers. Hooks and function tables that need a
// callable which does nothing point at these; none of them reads its
// arguments or changes game state. The identical copies in other overlays
// share one definition.
namespace VS::Core {
// SLUS_010.40 _gpuSyncVoidCallback (main.c:8227) and the identical SLUS
// stubs.
void IgnoreCall() noexcept;

// SLUS_010.40 nop5 (main.c:9731) and its copies.
int ReturnZero() noexcept;

// SLUS_010.40 vs_main_nop9 (main.c:11663): the retail build's stubbed error
// report. The code and detail are traced on the "diagnostic" channel and
// otherwise ignored, as on the console.
void IgnoreDiagnostic(int code, int detail);

// SLUS_010.40 _asmNop (main.c:11661): the game-time synchronization hook's
// single no-op instruction. There is no host sleep or yield.
void IdleInstruction() noexcept;

// BATTLE.PRG func_8007B0FC (146C.c:8237) and its copies.
void NoOperation() noexcept;

// BATTLE.PRG vs_battle_nop0 (146C.c:1354): the actor-stats callback that
// does nothing. RuntimeLive.hpp keeps the original actor-pointer signature.
void IgnoreCallback() noexcept;
}  // namespace VS::Core
