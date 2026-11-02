#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/ModelDrawing.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Legacy/Psx.h"
}

// Binds the model drawing policies to the live actor records, model workspace,
// GTE and ordering table.
namespace VS::Actors {
// Builds the actor's bone matrices from its blended pose and `camera`; false
// when the model, bone count or bone table cannot be trusted (func_800B28A8
// bone loop).
bool BuildLiveActorModelPose(Actors_ActorRecord& actor, MATRIX& camera);
// The parent bone matrix an attached actor draws from, or null while the parent
// slot, model or bone is unavailable (44F14.c:1010).
MATRIX* ResolveLiveAttachmentParentMatrix(const Actors_ActorRecord* actor);
ModelDrawResult DrawLiveActorModel(Actors_ActorRecord& actor, MATRIX& camera,
                                   int companion);
// Writes the packed screen position and depth to output[0..1]; the bone must
// lie within the actor's bone matrices and the model's bone table (#263).
int ProjectLiveActorBoneEndpoint(int slot, int bone, int* output, int endpoint);
}  // namespace VS::Actors
