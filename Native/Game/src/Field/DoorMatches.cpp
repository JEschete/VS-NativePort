// Hand-maintained (tools/port/owned.csv).
#include "Field/DoorMatches.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace VS::Field {
// decomp: BATTLE.PRG func_8008BAC8 (src/BATTLE/BATTLE.PRG/146C.c:16185).
void TransitionDoorsAtTile(std::span<const DoorTile> doors, int x, int z,
                           int layer, std::vector<std::size_t>& matches) {
  matches.clear();
  for (std::size_t index = 0; index < doors.size(); ++index) {
    const DoorTile& tile = doors[index];
    if (tile.layer != layer) continue;
    if (layer >= kFirstPlatformLayer || (tile.x == x && tile.z == z)) {
      matches.push_back(index);
    }
  }
}
}  // namespace VS::Field
