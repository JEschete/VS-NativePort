// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Field/RoomTriggerMarkers.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Audio/Seq/Channel.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/TriggerMarkers.h"
#include "Render/RoomDraw.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

enum {
  kMarkerRecordBytes = 116,
  kMarkerViewOffset = 36,
  kMarkerPolygonCountsOffset = 68,
  kMarkerQuadOffset = 76,
  kMarkerTileUnits = 128,
  kMarkerMaximumFade = 128,
  kMarkerRevealRate = 2,
  kMarkerAnimationInterval = 8,
  kMarkerAnimationFrames = 5,
  kMarkerTextureSourceX = 72,
  kMarkerPaletteSourceX = 4,
  kMarkerPaletteStride = 4,
  kMarkerTextureTargetX = 64,
  kMarkerTextureTargetY = 384,
  kMarkerPaletteTargetY = 320,
  kMarkerKindLimit = 16,
  kFloorHeightMask = 0x7FFF,
  kFloorHeightSign = 0x4000
};

// Complete D_800E8548 definition from BATTLE.PRG:0x7FD48, loaded by Content.
extern unsigned int
    Content_battleTriggerMarkerBytes[kMarkerRecordBytes / sizeof(unsigned int)];

typedef struct RoomTriggerMarkerGeometry {
  unsigned group_count;
  Render_RoomGeometryGroup group;
  unsigned triangle_count;
  unsigned quad_count;
  Field_MpdQuad quad;
} RoomTriggerMarkerGeometry;

_Static_assert(sizeof(RoomTriggerMarkerGeometry) == kMarkerRecordBytes,
               "Complete original trigger marker definition");
_Static_assert(offsetof(RoomTriggerMarkerGeometry, group.view) ==
                   kMarkerViewOffset,
               "Trigger marker view matrix");
_Static_assert(offsetof(RoomTriggerMarkerGeometry, triangle_count) ==
                   kMarkerPolygonCountsOffset,
               "Trigger marker polygon counts");
_Static_assert(offsetof(RoomTriggerMarkerGeometry, quad) == kMarkerQuadOffset,
               "Trigger marker quad");

static void AdvanceMarkerFade(Field_MpdFloorTrigger* trigger) {
  if (State_tickSpeed <= 0) return;
  unsigned fade = (unsigned char)trigger->markerFade;
  const unsigned ticks = (unsigned)State_tickSpeed;
  if (Audio_Seq_unusedStateFlags.opaque20 == 0 &&
      Audio_Seq_unusedStateFlags.opaque40 == 0 && trigger->activation != 0) {
    if (fade < kMarkerMaximumFade) {
      const unsigned remaining = kMarkerMaximumFade - fade;
      fade += ticks >= (remaining + 1) / kMarkerRevealRate
                  ? remaining
                  : ticks * kMarkerRevealRate;
    }
  } else {
    fade = ticks >= fade ? 0 : fade - ticks;
  }
  trigger->markerFade = static_cast<char>(fade);
}

static void AnimateMarkerTexture(void) {
  Field_triggerMarkerFrameTicks += State_tickSpeed;
  if (Field_triggerMarkerFrameTicks < kMarkerAnimationInterval) return;
  Field_triggerMarkerFrameTicks -= kMarkerAnimationInterval;
  const int frame = (Field_roomAnimationTicks / kMarkerAnimationInterval) %
                    kMarkerAnimationFrames;
  Field_triggerMarkerTextureSource.x = static_cast<int16_t>(
      frame * kMarkerAnimationInterval + kMarkerTextureSourceX);
  Field_triggerMarkerPaletteSource.x = static_cast<int16_t>(
      frame * kMarkerPaletteStride + kMarkerPaletteSourceX);
  MoveImage(&Field_triggerMarkerTextureSource, kMarkerTextureTargetX,
            kMarkerTextureTargetY);
  MoveImage(&Field_triggerMarkerPaletteSource, 0, kMarkerPaletteTargetY);
}

// decomp: BATTLE.PRG func_8008DF14 (src/BATTLE/BATTLE.PRG/146C.c:17164).
void Field_DrawRoomTriggerMarkers(void) {
  if (Field_roomData.floorTriggers == nullptr) return;
  const size_t count = Field_roomData.header.floorTriggersLength /
                       sizeof *Field_roomData.floorTriggers;
  int first_marker = 1;
  for (size_t index = 0; index < count; ++index) {
    Field_MpdFloorTrigger* trigger = &Field_roomData.floorTriggers[index];
    AdvanceMarkerFade(trigger);
    const unsigned fade = (unsigned char)trigger->markerFade;
    if (fade == 0 || trigger->kind >= kMarkerKindLimit) continue;
    if (first_marker) {
      AnimateMarkerTexture();
      first_marker = 0;
    }
    const int world_x = trigger->tileX * kMarkerTileUnits;
    const int world_z = trigger->tileZ * kMarkerTileUnits;
    // The floor query packs a flag above its signed 15-bit height.
    const unsigned packed_height =
        static_cast<uint16_t>(Field_FloorHeightAt(world_x, world_z)) &
        kFloorHeightMask;
    const int16_t height = static_cast<int16_t>(
        static_cast<int>(packed_height ^ kFloorHeightSign) - kFloorHeightSign);
    Field_DrawRoomTriggerMarker(world_x, world_z, height, fade);
  }
}

// decomp: BATTLE.PRG func_8008E19C (src/BATTLE/BATTLE.PRG/146C.c:17216).
void Field_DrawRoomTriggerMarker(int world_x, int world_z, int16_t height,
                                 u_int brightness) {
  RoomTriggerMarkerGeometry marker;
  memcpy(&marker, Content_battleTriggerMarkerBytes, sizeof marker);
  if (marker.group_count != 1 ||
      marker.group.polygonOffset !=
          offsetof(RoomTriggerMarkerGeometry, triangle_count) ||
      marker.triangle_count != 0 || marker.quad_count != 1) {
    return;
  }
  // G00039/G00040/G00041 were aliases inside this record, not native storage.
  marker.group.view = Render_frameScratch.viewMatrix;
  marker.quad.vertex1X = static_cast<int16_t>(world_x + kMarkerTileUnits);
  marker.quad.vertex1Y = height;
  marker.quad.vertex1Z = static_cast<int16_t>(world_z + kMarkerTileUnits);
  const char shade = static_cast<char>(brightness);
  marker.quad.red1 = marker.quad.green1 = marker.quad.blue1 = shade;
  marker.quad.red2 = marker.quad.green2 = marker.quad.blue2 = shade;
  marker.quad.red3 = marker.quad.green3 = marker.quad.blue3 = shade;
  marker.quad.red4 = marker.quad.green4 = marker.quad.blue4 = shade;
  Render_DrawRoomGeometry(&marker, Field_roomGeometryDrawMode);
}
}  // extern "C"
