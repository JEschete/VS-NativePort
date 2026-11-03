#pragma once
// Hand-maintained (tools/port/owned.csv).
// The battle asset queue's legacy globals. ActorAssetsLive.cpp defines them
// and binds them to the typed loaders (ActorAssets.hpp); the battle start-up
// (ModelRendering.cpp) still clears the parse flag directly.

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800E8F29 (BATTLE/BATTLE.PRG 30DB0.c): the parse phase of the
// model being parsed; 1 while its primitives wait for the second frame.
extern unsigned char Content_modelParseResetFlag;
// decomp: D_800E8FC4 (BATTLE/BATTLE.PRG 30DB0.c): the slowest request (in
// VSync(1) ticks) while a model file was parsing.
extern int Content_assetQueuePeakUsage;
// decomp: D_800F457C (BATTLE/BATTLE.PRG 30DB0.c): the model load phase of
// that request.
extern int Content_lastLoadPhaseSnapshot;
// decomp: D_800F4580 (BATTLE/BATTLE.PRG 30DB0.c): the ticks the last request
// took.
extern int Content_assetQueueFrameTicks;
// decomp: D_800F45D8 (BATTLE/BATTLE.PRG 30DB0.c): the kind of the slowest
// request.
extern int Content_lastAssetRequestType;

#ifdef __cplusplus
}
#endif
