#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Content/ActorAssets.hpp"

// Binds the encyclopedia preview (EncyclopediaPreview.hpp) to the shared
// model load step (VS::Content::objectDataLoadStep), the ZUD file
// table (zudLbas, zudSizes, paletteFlags),
// the disc queue, the game heap, the battle asset queue and the actor
// records, in the shapes the copied score screen calls. The preview's state
// lives here instead of 592C.c's statics. An id past the file table, a file
// or section the heap block cannot hold, a missing record and a read while
// an earlier preview still holds its blocks are refused and reported on the
// "menu.score" channel.
namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
struct EnemyPreviewState;

// MENU9.PRG (57BC.c, 592C.c). The preview's parts are queued under
// Content::kEnemyPreviewActorAssetOwner (#48); a load that fails returns
// kEnemyPreviewFailed.
int LoadLiveEnemyPreview(int zud_id);
void FreeLiveEnemyPreview();
void InitializeLiveAshleyPreviewPose();
[[nodiscard]] bool QueueLiveEnemyModelRead(int zud_id);
int LoadLiveEnemyModelParts();
void FreeLiveEnemyModelBuffer();
// Native (#48): where the live preview's owned request binds `slot` of
// `table` (EnemyPreviewRecordMemory), the record memory its parts submit with
// (Content::ActorRecordMemory); kNone for another owner or when the preview
// holds no such part.
[[nodiscard]] Content::AssetAddress LiveEnemyPreviewRecordMemory(
    Content::ActorAssetOwner owner, Content::ActorRecordTable table, int slot);

// Abandon a stopped preview at any loading phase: cancels its own queued
// parts and read, then releases the records bound in its buffer. Refuses
// invalid ownership before freeing its backing buffers; foreign
// requests/records survive.
[[nodiscard]] bool CleanupLiveEnemyPreviewOwnedState();

// State reset only (592C.c's bss). At MENU9 reload, checked cleanup must
// succeed before this reset can discard the native resource identities.
void ResetLiveEnemyPreview();
// The bound state, for the reset and tests.
[[nodiscard]] EnemyPreviewState& LiveEnemyPreviewState() noexcept;
// Inputs the binding refused since start-up.
[[nodiscard]] std::uint64_t LiveEnemyPreviewRefusals() noexcept;
}  // namespace VS::Menu::Score
