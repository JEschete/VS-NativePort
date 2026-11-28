#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <vector>

#include "Actors/ActorInstances.hpp"
#include "Core/Gte.hpp"
#include "Field/CollisionSampling.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/SegmentCollision.hpp"

namespace VS::Field {
struct PlatformContact;

struct SegmentColliderInput {
  Actors::SpawnedActor identity = Actors::SpawnedActor::kNone;
  std::expected<std::optional<ActorTraceBounds>, std::string> bounds;
};

struct SegmentCollisionInputs {
  std::array<SegmentColliderInput, kSegmentTraceActorSlots> actors{};
  std::expected<MovingPlatformQueryInputs, std::string> platforms;
  std::expected<CollisionHeightMap, std::string> terrain;
};

// All inputs survive room/actor teardown. Invalid unused inputs are retained
// as errors until the original query ordering actually needs them.
class SegmentCollisionContext final : public SegmentTraceWorld {
 public:
  explicit SegmentCollisionContext(SegmentCollisionInputs inputs);
  [[nodiscard]] Actors::SpawnedActor ActorIdentity(int slot) const;
  [[nodiscard]] std::optional<ActorTraceBounds> ActorBounds(
      int slot) const override;
  [[nodiscard]] std::vector<SolidVolume> CollectSolidVolumes() override;
  [[nodiscard]] std::uint16_t FloorHeightWord(std::int32_t x,
                                              std::int32_t z) const override;

 private:
  SegmentCollisionInputs inputs_;
};

[[nodiscard]] SegmentCollisionContext CaptureLiveSegmentCollisionContext();

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
