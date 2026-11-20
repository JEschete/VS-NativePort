#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <span>

#include "Legacy/Psx.h"
#include "Render/ModelGeometry.hpp"

struct Actors_ActorRecord;
namespace VS::Actors {
struct WeaponModel;
}
namespace VS::Field {
struct GimmickInstance;
}

// Live binding for the SHP model packets (ModelGeometry.cpp): the GTE, the
// model render context, the model workspace and the current frame's
// primitive buffer. Every model is read through the byte extent its loader
// recorded, and projection is bounded by the matrices its owner holds (#269).
namespace VS::Render {
// Records that `model` holds `bytes` bytes of SHP data, replacing any
// extent that overlaps it.
void RecordLiveModelExtent(const void* model, std::size_t bytes);
// Gives `destination` the extent of the model it was copied from, limited to
// the `capacity` bytes copied.
void RecordLiveModelCopy(const void* destination, const void* source,
                         std::size_t capacity);
void ForgetLiveModelExtent(const void* model);
// The recorded bytes of `model`, or nothing when no loader recorded it.
[[nodiscard]] std::span<const std::byte> LiveModelBytes(const void* model);

// Whether the actor's recorded model data holds every section and polygon
// its counts claim.
[[nodiscard]] bool ValidateLiveActorModel(const Actors_ActorRecord& geometry);
// MoveModelVertex over the actor's model and its recorded size; false when
// the actor has no model or the vertex lies outside its vertex section.
[[nodiscard]] bool MoveLiveActorModelVertex(Actors_ActorRecord& actor,
                                            std::size_t vertex,
                                            const ModelVertex& delta);
// Projects the actor model's vertices through `matrices` into the render
// context; false when nothing was projected or every depth was zero.
[[nodiscard]] bool ProjectLiveActorModel(const Actors_ActorRecord& geometry,
                                         std::span<const MATRIX> matrices);
// The geometry actor's model and counts through func_800B3CF0 (flat) or
// func_800B4594 (Gouraud).
void DrawLiveActorGouraudPolygons(const Actors_ActorRecord& geometry,
                                  const CVECTOR& tint);
void DrawLiveActorFlatPolygons(const Actors_ActorRecord& geometry,
                               const CVECTOR& tint);
// The geometry actor's model and counts through the override drawers,
// func_800B516C (flat) or func_800B5C30 (Gouraud), with the pulsed palettes
// and appearance material of `overrides`, which the original reads from the
// drawing actor.
void DrawLiveActorPolygonsWithOverrides(const Actors_ActorRecord& geometry,
                                        bool colored, const CVECTOR& tint,
                                        const ModelOverrides& overrides);
// Writes the shadow anchor to the actor's sound position.
void DrawLiveActorGroundShadow(Actors_ActorRecord& actor, const MATRIX& camera,
                               const SVECTOR& root_offset);

[[nodiscard]] bool ProjectLiveGimmickModel(
    const VS::Field::GimmickInstance& object);
void DrawLiveGimmickGouraudPolygons(const VS::Field::GimmickInstance& object);
void DrawLiveGimmickGouraudPolygonsWithOverrides(
    const VS::Field::GimmickInstance& object);

[[nodiscard]] bool ProjectLiveWeaponModel(
    const VS::Actors::WeaponModel& weapon);
void DrawLiveWeaponFlatPolygons(const VS::Actors::WeaponModel& weapon,
                                const CVECTOR& tint);
void DrawLiveWeaponFlatPolygonsWithOverrides(
    const VS::Actors::WeaponModel& weapon, const CVECTOR& tint);
}  // namespace VS::Render
