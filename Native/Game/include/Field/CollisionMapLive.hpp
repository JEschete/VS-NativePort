#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <cstdint>
#include <optional>

#include "Field/CollisionMap.hpp"
#include "Field/PlatformPlacement.hpp"

struct Field_MpdTileCell;
struct Field_ZoneBounds;
struct SVECTOR;

// Binds the typed room collision map rules (CollisionMap.hpp) to the loaded
// room, the gimmick and actor tables and the platform support state, in the
// shapes the copied callers use. Cells come back as pointers into the room's
// tables so callers can change them; a missing cell is null.
namespace VS::Field {
// The room's cell table; null when the room has none.
Field_MpdTileCell* LiveRoomCollisionCells();
int LiveCollisionMapDimensions(int unused);
// The room's camera bounds record; null when the room has none.
const Field_ZoneBounds* LiveRoomCameraBounds(int unused);
Field_MpdTileCell* LiveCollisionCellAt(unsigned int tile_x, unsigned int tile_z,
                                       int layer);
// For the readers the original let dereference a missed lookup: never null,
// a missing cell reads as a shared kAbsentCellWord cell (#288).
const Field_MpdTileCell* LiveCollisionCellOrAbsent(unsigned int tile_x,
                                                   unsigned int tile_z,
                                                   int layer);
// Returns 1 when a cell was changed; a cell outside the tables is refused.
int SetLiveCollisionCellBlocked(int tile_x, int tile_z, int layer, int blocked);
Field_MpdTileCell* FindLiveCollisionCellAtPosition(const SVECTOR* position,
                                                   int layer);

std::int16_t LiveInterpolatedFloorHeight(int world_x, int world_z);
std::int16_t LiveFloorHeightAt(int world_x, int world_z);
std::int16_t LiveCeilingHeightAt(int world_x, int world_z);
// The floor in the low halfword and the ceiling in the high one.
int LiveFloorAndCeilingHeightsAt(int world_x, int world_z);
int LiveRoomCeilingHeight(int world_x, int world_z);

int LiveActorSupportHeight(int world_x, int world_z);
void SnapLiveRoomActorsToSupport();
// Also records the supporting gimmick slot (Actors_supportingGimmickIndex).
int LiveGimmickSupportHeight(int tile_x, int tile_z);
int LiveGimmickSupportHeightBelow(int tile_x, int tile_z, int reference_height);

int LiveIsPointOnMovingPlatform(int platform_layer, const SVECTOR* position);
// The room's authored platforms; 0 when the section is absent or malformed.
int LiveMovingPlatformCount();
// Also records the supporting platform layer (Actors_supportingPlatformLayer).
int LiveMovingPlatformContactHeight(int world_x, int world_y, int world_z,
                                    int search_mode);

// Actor slots outside the table are refused with a diagnostic.
void SetLiveActorCollisionBypass(int actor_index, int flags);
int LiveActorCollisionBypass(int actor_index);

// Typed views of the same room state for the actor collision rules.
std::optional<CollisionCellRef> LiveCollisionCellRefAtPosition(
    std::int16_t world_x, std::int16_t world_z, int layer);
// A cell's word; nullopt for a reference outside the current room's tables.
std::optional<std::uint32_t> LiveCollisionCellWord(CollisionCellRef cell);
// Also records the supporting gimmick slot (Actors_supportingGimmickIndex).
GimmickSupport LiveGimmickSupport(int tile_x, int tile_z);
// Also records the supporting platform layer (Actors_supportingPlatformLayer).
PlatformContactHeight LiveMovingPlatformContact(PlatformProbe probe,
                                                int search_mode);
// The visible platform on `support_layer`.
std::optional<PlatformContact> LiveVisiblePlatform(int support_layer);
}  // namespace VS::Field
