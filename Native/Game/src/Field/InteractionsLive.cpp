#include "Field/InteractionsLive.hpp"

#include "Field/ChestInteractionsLive.h"

namespace VS::Field {
bool CheckAshleyTileTriggersLive(int actor_id, Actors_ActorLocation& location) {
  const Actors::ActorPlacementView placement{
      .tile = {.x = location.packedTile.target.tileX,
               .layer = location.packedTile.target.tileLayer,
               .z = location.packedTile.target.tileZ,
               .spare = location.packedTile.target.facing},
      .position = {.x = location.position.vx,
                   .y = location.position.vy,
                   .z = location.position.vz,
                   .w = location.position.pad}};
  return CheckLiveAshleyTileTriggers(actor_id, placement);
}
}  // namespace VS::Field
