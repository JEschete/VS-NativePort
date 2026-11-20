#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/ActorRecord.hpp"
#include "Actors/ModelDrawing.hpp"
#include "Legacy/Psx.hpp"

// Binds the model drawing policies to the live actor records, model workspace,
// GTE and ordering table.
namespace VS::Actors {
// Builds the actor's bone matrices from its blended pose and `camera`; false
// when the model, bone count or bone table cannot be trusted (func_800B28A8
// bone loop).
bool BuildLiveActorModelPose(VS::Actors::ActorRecord& actor,
                             VS::Core::Gte::MATRIX& camera);
// The parent bone matrix an attached actor draws from, or null while the parent
// slot, model or bone is unavailable (44F14.c:1010).
VS::Core::Gte::MATRIX* ResolveLiveAttachmentParentMatrix(
    const VS::Actors::ActorRecord* actor);
ModelDrawResult DrawLiveActorModel(VS::Actors::ActorRecord& actor,
                                   VS::Core::Gte::MATRIX& camera,
                                   int companion);
// Writes the packed screen position and depth to output[0..1]; the bone must
// lie within the actor's bone matrices and the model's bone table (#263).
int ProjectLiveActorBoneEndpoint(int slot, int bone, int* output, int endpoint);
}  // namespace VS::Actors
