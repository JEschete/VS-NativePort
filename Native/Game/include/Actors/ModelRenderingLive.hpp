#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/ModelRendering.hpp"
#include "Legacy/Psx.h"

namespace VS::Actors {
struct ActorRecord;
}
namespace VS::Actors {
struct WeaponModel;
}
namespace VS::Field {
struct GimmickInstance;
}

// Live binding for the room model pass and the model transforms
// (ModelRendering.cpp): the actor, weapon and gimmick tables, the model
// workspace, the skeleton pose and the ordering table, in the original argument
// order for the generated callers (tools/port/copy_fixups.py
// OWNED_CALL_BINDINGS). Null records, slots past their tables and model data
// that cannot hold the bones read are reported on the "actors" channel and
// refused. The transforms are computed without the GTE; only the model
// projections use it.
namespace VS::Actors {
// func_800AEAE8: the actor's root offset into the model workspace, rotated by
// TransformLiveActorRootOffset, and its look angles.
void PrepareLiveActorRootTransform(VS::Actors::ActorRecord* actor);
// func_800AEF94: UpdateAndDrawRoomModels over the live tables.
void UpdateAndDrawLiveRoomActors(MATRIX* camera);
// func_800B07DC: rotates the workspace root offset by the actor's angles. It
// leaves the rotation in the workspace matrix, with the rotated offset as its
// translation, and the offset moved to the actor's position in rootOffset.
void TransformLiveActorRootOffset(VS::Actors::ActorRecord* actor);
// func_800B1A68: places the gimmick centered at the workspace rootOffset and
// draws it. A held gimmick whose holder slot is empty is refused.
void DrawLiveGimmickModel(VS::Field::GimmickInstance* object, MATRIX* camera);
// func_800B217C: fades the weapon, builds its bone chain from `source` and
// draws it.
void DrawLiveActorWeaponModel(VS::Actors::WeaponModel* weapon, MATRIX* source);
// INITBTL.PRG func_800FA7B0: the texture slots, the model tables and the
// sequence buffers a battle starts from.
void InitializeLiveActorModelResources();
}  // namespace VS::Actors
