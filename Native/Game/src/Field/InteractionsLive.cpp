#include "Field/InteractionsLive.hpp"

#include "Field/ChestInteractionsLive.hpp"

namespace VS::Field {
bool CheckAshleyTileTriggersLive(int actor_id,
                                 VS::Actors::ActorLocation& location) {
  const Actors::ActorPlacementView placement{
      .tile = {.x = location.packed_tile.target.tile_x,
               .layer = location.packed_tile.target.tile_layer,
               .z = location.packed_tile.target.tile_z,
               .spare = location.packed_tile.target.facing},
      .position = {.x = location.position.vx,
                   .y = location.position.vy,
                   .z = location.position.vz,
                   .w = location.position.pad}};
  return CheckLiveAshleyTileTriggers(actor_id, placement);
}
}  // namespace VS::Field
