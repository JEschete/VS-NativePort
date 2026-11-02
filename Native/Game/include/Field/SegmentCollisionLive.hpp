#pragma once

#include <cstddef>

#include "Legacy/Psx.h"

struct Field_PlatformContactPoint;

namespace VS::Field {

// Live binding for the original BATTLE.PRG entry 0x800E6BA0: adapts guest
// platform contacts to the typed predicate; returns 1 when outside.
int IsLivePointOutsideSolidVolumes(const VECTOR* point,
                                   const Field_PlatformContactPoint* volumes,
                                   std::size_t count);

// Live binding for the original BATTLE.PRG entry 0x800E6C34: traces against
// the live actor, Gimmick, platform and terrain state. On a hit, writes the
// sample and actor id plus one (0 for terrain) to output and returns it;
// otherwise returns nullptr and leaves output untouched. output may alias from.
SVECTOR* TraceLiveActorAndTerrainCollision(const SVECTOR* from,
                                           const SVECTOR* to, SVECTOR* output,
                                           int ignored_actor);

}  // namespace VS::Field
