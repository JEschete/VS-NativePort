#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the primary effect state and the effect token pool
// (PrimaryEffect.hpp) over Runtime.cpp's globals. The entries keep the
// original signatures for the generated runtime, the battle and event callers
// and the module's function pointers. A track outside the pending mask, a
// phase the runtime never sets and a token outside the pool are refused with
// a diagnostic, leaving the state.
namespace VS::Effects {
// func_800CEEBC (5BF94.c:5373): 1 when a track can start.
int LiveCanStartEffectTrack();
// func_800CEF0C (5BF94.c:5382).
int LivePrimaryEffectTrackCount();
// func_800CEF38 (5BF94.c:5391).
void LiveRequeueEffectTrack(int track);
// func_800CEF64 (5BF94.c:5397): 1 when the primary effect's package is
// loaded.
int LiveIsPrimaryEffectLoaded();
// func_800CF478 (5BF94.c:5546).
void LiveSetPrimaryEffectPhase(int phase);
// func_800CF48C (5BF94.c:5550): the stored phase number.
int LivePrimaryEffectPhase();
// func_800CF49C (5BF94.c:5552): 0, with a diagnostic, when the pool is full.
int LiveAllocateEffectToken();
// func_800CF514 (5BF94.c:5582).
void LiveReleaseEffectToken(int token);
}  // namespace VS::Effects
