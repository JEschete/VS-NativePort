#pragma once

#include <cstddef>

#include "Core/Gte.hpp"
#include "Field/SegmentCollision.hpp"

namespace VS::Field {
struct PlatformContact;

// Collision bounds come from the published actor's expected components.
[[nodiscard]] std::optional<SegmentHit> TraceLiveOwnedActorAndTerrainCollision(
    SegmentEndpoint from, SegmentEndpoint to, int ignored_actor);

// Live binding for the original BATTLE.PRG entry 0x800E6BA0: adapts guest
// platform contacts to the typed predicate; returns 1 when outside.
int IsLivePointOutsideSolidVolumes(const VS::Core::Gte::VECTOR* point,
                                   const PlatformContact* volumes,
                                   std::size_t count);

// Live binding for the original BATTLE.PRG entry 0x800E6C34: traces against
// the live actor, Gimmick, platform and terrain state. On a hit, writes the
// sample and actor id plus one (0 for terrain) to output and returns it;
// otherwise returns nullptr and leaves output untouched. output may alias from.
VS::Core::Gte::SVECTOR* TraceLiveActorAndTerrainCollision(
    const VS::Core::Gte::SVECTOR* from, const VS::Core::Gte::SVECTOR* to,
    VS::Core::Gte::SVECTOR* output, int ignored_actor);

}  // namespace VS::Field
