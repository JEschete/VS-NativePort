// Hand-maintained (tools/port/owned.csv).
#include "Field/CollisionSampling.h"
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Render/Palette.h"

#include <string.h>


// decomp: BATTLE.PRG func_80099514 (src/BATTLE/BATTLE.PRG/2EA3C.s:2494).
u_char Field_SampleCollisionHeightCode(u_int sampleX, int sampleZ, int layer)
{
    const Field_MpdCollisionHeader* map = Field_roomData.collisionMap;
    const size_t length = Field_roomData.header.collisionMapLength;
    if (map == NULL || length < sizeof *map || layer < 0 || layer > 1 || sampleZ < 0
        || map->width <= 0 || map->height <= 0 || map->cellCount < map->width * map->height
        || (sampleX >> 2) >= (unsigned)map->width || ((unsigned)sampleZ >> 2) >= (unsigned)map->height) {
        return 0x80;
    }
    const size_t patterns = sizeof *map + (size_t)map->cellCount * 4;
    if (patterns > length) return 0x80;
    const size_t tile = ((unsigned)sampleZ >> 2) * (unsigned)map->width + (sampleX >> 2);
    const unsigned char* bytes = (const unsigned char*)map;
    unsigned short descriptor;
    memcpy(&descriptor, bytes + sizeof *map + ((size_t)layer * map->cellCount + tile) * 2, sizeof descriptor);
    const size_t pattern = patterns + (size_t)(descriptor & 0x7FF) * 16;
    if (pattern > length || length - pattern < 16) return 0x80;
    const unsigned char sample = bytes[pattern + ((unsigned)sampleZ & 3) * 4 + (sampleX & 3)];
    const int height = (signed char)sample + ((descriptor & 0xF800) >> 9);
    return (u_char)((height & 0x7F) | (sample & 0x80));
}
