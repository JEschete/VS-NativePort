#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <cstdint>

#include "Field/ActorCollision.hpp"
#include "Legacy/Psx.h"

struct Actors_ActorRecord;

// Live bindings of the actor terrain collision rules (ActorCollision.cpp).
// Generated callers keep the original record and vector call shapes: each
// binding loads the actors it names into typed values, runs the rule against
// the current room and stores the changes back before its vector out-params.
// The probe position and sector are mirrored into the generated globals the
// movement rules still read.
namespace VS::Field {
// The first actor standing on a tile; nullptr when none (func_800A3C34).
Actors_ActorRecord* LiveFindActorOccupyingTile(int tile_x, int tile_z,
                                               int minimum_height, int mode);
int LiveCheckPlatformActorObstruction(int layer, SVECTOR* point);
int LiveCanActorHangFromLedge(Actors_ActorRecord* actor, SVECTOR* offset,
                              SVECTOR* motion);
int LiveHasActorLedgeSideSupport(Actors_ActorRecord* actor);
// Modes outside 0 to 4 sample as mode 1, as in the original.
int LiveSampleActorSupportHeight(const SVECTOR* origin, int offset_x,
                                 int offset_z, int mode);
// The same sample from a position held as three halfwords (a gimmick's).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveSampleSupportHeightAt(int x, int y, int z, int offset_x, int offset_z,
                              int mode);
void LiveSampleActorCollisionPerimeter(Actors_ActorRecord* actor, int diagonal);
void LiveApplyActorWallRepulsion(Actors_ActorRecord* actor, SVECTOR* motion);
// Writes `result` only when the motion reached a probe.
int LiveResolveActorTerrainMotion(Actors_ActorRecord* actor, SVECTOR* motion,
                                  int* result, int probe_only);
int LiveCalcActorLedgeOffset(Actors_ActorRecord* actor, SVECTOR* offset);

// The word of the cell the last support sample found; kAbsentCellWord when
// none was found or the room's tables no longer hold it.
std::uint32_t LiveCollisionProbeCellWord();
// The state the bindings keep between calls.
CollisionProbe& LiveCollisionProbe();
CollisionSamples& LiveCollisionSamples();
}  // namespace VS::Field
