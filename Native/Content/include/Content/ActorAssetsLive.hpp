#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Content_ActorAssetRequest;
struct Content_SequenceBuffer;

// Live binding for the battle asset queue and loaders (ActorAssets.hpp) over
// the legacy request queue (Content_legacyActorAssetRequests), the loaders'
// globals (ModelLoading.h, ActorAssetsLiveState.h), the actor and weapon
// records (Actors_actorRecords, Actors_weaponModels), the shared sequence
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

}  // namespace VS::Content
