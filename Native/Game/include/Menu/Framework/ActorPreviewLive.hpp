#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>

#include "Content/ActorAssets.hpp"
#include "Legacy/Psx.hpp"

// Binds the actor preview (ActorPreview.hpp) to the actor and weapon records
// (VS::Actors::actorRecords, VS::Actors::weaponModels), the game heap, the
// battle asset queue, the actor model, animation and appearance systems and the
// scene ordering table, in the shapes the copied menus call. A slot outside its
// table, a missing record, a bone past the record's bone matrices and a null
// camera matrix are refused and reported.
namespace VS::Menu::Framework {
// MAINMENU.PRG (224.c).
void LoadLiveActorPreview(int source_actor_slot);
void CloneLiveActorPreview(int source_actor_slot);
void ResetLiveActorPreviewPose();
void ReleaseLiveActorPreview();
[[nodiscard]] bool CleanupLiveActorPreviewOwnedState();
int DrawLiveActorPreview(VS::Core::Gte::MATRIX* preview_camera_matrix);
void ResetLiveAshleyEquipmentPose();
void QueueLiveAshleyWeaponAnimations(int sequence_model_id);
[[nodiscard]] std::optional<Content::ActorAssetTicket>
SubmitLiveAshleyWeaponAnimations(int sequence_model_id);
}  // namespace VS::Menu::Framework
