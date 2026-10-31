// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Field/CollisionSampling.h"

#include <stdint.h>
#include <string.h>

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
#include "Legacy/Psx.h"
#include "Render/Palette.h"

enum {
  kSamplesPerAxisBits = 2,
  kSamplesPerAxis = 1 << kSamplesPerAxisBits,
  kPatternBytes = kSamplesPerAxis * kSamplesPerAxis,
  kLayerCount = 2,
  kPatternIndexMask = 0x7FF,
  kHeightBiasMask = 0xF800,
  kHeightBiasShift = 9,
  kInvalidHeight = 0x80,
  kHeightValueMask = 0x7F
};

// decomp: BATTLE.PRG func_80099514 (src/BATTLE/BATTLE.PRG/2EA3C.s:2494).
u_char Field_SampleCollisionHeightCode(u_int sample_x, int sample_z,
                                       int layer) {
  return Field_SampleCollisionHeightCodeFromData(
      Field_roomData.collisionMap, Field_roomData.header.collisionMapLength,
      sample_x, sample_z, layer);
}

// Same decoder for checked, isolated room data; no live-room state is changed.
u_char Field_SampleCollisionHeightCodeFromData(const void* data, size_t length,
                                               u_int sample_x, int sample_z,
                                               int layer) {
  Field_MpdCollisionHeader header;
  if (data == nullptr || length < sizeof header) return kInvalidHeight;
  memcpy(&header, data, sizeof header);
  const Field_MpdCollisionHeader* map = &header;
  if (layer < 0 || layer >= kLayerCount || sample_z < 0 || map->width <= 0 ||
      map->height <= 0 || map->cellCount < map->width * map->height ||
      (sample_x >> kSamplesPerAxisBits) >= (unsigned)map->width ||
      ((unsigned)sample_z >> kSamplesPerAxisBits) >= (unsigned)map->height) {
    return kInvalidHeight;
  }
  const size_t patterns = sizeof *map + static_cast<size_t>(map->cellCount) *
                                            kLayerCount * sizeof(uint16_t);
  if (patterns > length) return kInvalidHeight;
  const size_t tile = (static_cast<size_t>(sample_z) >> kSamplesPerAxisBits) *
                          (unsigned)map->width +
                      (sample_x >> kSamplesPerAxisBits);
  const unsigned char* bytes = (const unsigned char*)data;
  uint16_t descriptor;
  memcpy(&descriptor,
         bytes + sizeof *map +
             (static_cast<size_t>(layer) * map->cellCount + tile) *
                 sizeof descriptor,
         sizeof descriptor);
  const size_t pattern =
      patterns +
      static_cast<size_t>(descriptor & kPatternIndexMask) * kPatternBytes;
  if (pattern > length || length - pattern < kPatternBytes) {
    return kInvalidHeight;
  }
  const unsigned char sample =
      bytes[pattern +
            (static_cast<size_t>(sample_z) & (kSamplesPerAxis - 1)) *
                kSamplesPerAxis +
            (sample_x & (kSamplesPerAxis - 1))];
  const int signed_sample =
      sample <= INT8_MAX ? sample : static_cast<int>(sample) - (UINT8_MAX + 1);
  const int height =
      signed_sample + ((descriptor & kHeightBiasMask) >> kHeightBiasShift);
  return (u_char)((height & kHeightValueMask) | (sample & kInvalidHeight));
}
}  // extern "C"
