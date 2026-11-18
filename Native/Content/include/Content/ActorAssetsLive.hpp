#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

struct Content_ActorAssetRequest;
struct Content_SequenceBuffer;

// Live binding for the battle asset queue and loaders (ActorAssets.hpp) over
// the legacy request queue (Content_legacyActorAssetRequests), the loaders'
// globals (ModelLoading.h and the ones below), the actor and weapon
// records (Actors_actorRecords, VS::Actors::weaponModels), the shared sequence
// banks and the texture slot tables. Generated callers keep the original
// call shapes.
//
// A record, buffer or file the typed loaders cannot bound, or a request they
// refuse, is counted and warned about on the "content" channel. A loader
// read the disc queue refuses (#282) is read before the loader returns.
namespace VS::Content {
struct ActorAssetRequest;

int FindLiveFreeActorAssetRequest();
// Copies `request` into the first free queue slot; -1 when the queue is
// full or `request` is null.
int QueueLiveActorAssetRequest(const Content_ActorAssetRequest* request);
// The typed request's entry for promoted callers; the same result.
int QueueLiveActorAsset(const ActorAssetRequest& request);
int UpdateLiveActorAssetQueue();
// Between queue updates, abandon model/weapon/battle-sequence parsing and
// actor-model reads borrowing either allocation. Drain a matching head first;
// other request kinds refuse, and unrelated requests/loader state survive.
[[nodiscard]] bool CancelLiveActorAssetBorrows(
    std::span<std::byte> source, std::span<std::byte> destination);
// Adds the texture offsets to the UVs of `count` polygons in a game heap
// block; polygons running past the block are refused.
void OffsetLiveModelTextureCoordinates(void* polygons, int count, int colored,
                                       int u_offset, int v_offset);
void* CopyLiveModelGeometry(int* destination, void* source, int bytes);
int ReleaseLiveWeaponModel(int slot);
void CopyLiveWeaponSequence(Content_SequenceBuffer* destination,
                            Content_SequenceBuffer* source);
int ReleaseLiveActorSequence(int target, int kind);
int ReleaseLiveActorModel(int slot);
int ReleaseLiveSequenceEffectTexture(int bank);

// Inputs the loaders refused since start-up.
[[nodiscard]] std::uint64_t LiveActorAssetRefusals() noexcept;

// The battle asset queue's legacy globals. ActorAssetsLive.cpp defines them
// and binds them to the typed loaders (ActorAssets.hpp); the battle start-up
// (ModelRendering.cpp) still clears the parse flag directly.

// decomp: D_800E8F29 (BATTLE/BATTLE.PRG 30DB0.c): the parse phase of the
// model being parsed; 1 while its primitives wait for the second frame.
extern unsigned char modelParseResetFlag;
// decomp: D_800E8FC4 (BATTLE/BATTLE.PRG 30DB0.c): the slowest request (in
// VSync(1) ticks) while a model file was parsing.
extern int assetQueuePeakUsage;
// decomp: D_800F457C (BATTLE/BATTLE.PRG 30DB0.c): the model load phase of
// that request.
extern int lastLoadPhaseSnapshot;
// decomp: D_800F4580 (BATTLE/BATTLE.PRG 30DB0.c): the ticks the last request
// took.
extern int assetQueueFrameTicks;
// decomp: D_800F45D8 (BATTLE/BATTLE.PRG 30DB0.c): the kind of the slowest
// request.
extern int lastAssetRequestType;

}  // namespace VS::Content
