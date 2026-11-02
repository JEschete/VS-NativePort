#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <span>

#include "Legacy/Psx.h"
#include "Render/ModelGeometry.hpp"

struct Actors_ActorRecord;
struct Actors_WeaponModel;
struct Field_GimmickInstance;

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
// Projects the actor model's vertices through `matrices` into the render
// context; false when nothing was projected or every depth was zero.
[[nodiscard]] bool ProjectLiveActorModel(const Actors_ActorRecord& geometry,
                                         std::span<const MATRIX> matrices);
// 800B3680..800B38E0: textured actor polygons, with the pulsed palettes and
// appearance material while either is active.
void DrawLiveActorPolygons(const Actors_ActorRecord& geometry,
                           const CVECTOR& tint);
void DrawLiveActorGouraudPolygons(const Actors_ActorRecord& geometry,
                                  const CVECTOR& tint);
void DrawLiveActorFlatPolygons(const Actors_ActorRecord& geometry,
                               const CVECTOR& tint);
// Writes the shadow anchor to the actor's sound position.
void DrawLiveActorGroundShadow(Actors_ActorRecord& actor, const MATRIX& camera,
                               const SVECTOR& root_offset);

[[nodiscard]] bool ProjectLiveGimmickModel(const Field_GimmickInstance& object);
void DrawLiveGimmickGouraudPolygons(const Field_GimmickInstance& object);
void DrawLiveGimmickGouraudPolygonsWithOverrides(
    const Field_GimmickInstance& object);

[[nodiscard]] bool ProjectLiveWeaponModel(const Actors_WeaponModel& weapon);
void DrawLiveWeaponFlatPolygons(const Actors_WeaponModel& weapon,
                                const CVECTOR& tint);
void DrawLiveWeaponFlatPolygonsWithOverrides(const Actors_WeaponModel& weapon,
                                             const CVECTOR& tint);
}  // namespace VS::Render
