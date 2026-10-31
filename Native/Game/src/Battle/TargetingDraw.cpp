// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Battle/TargetingDraw.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "Battle/BattleTypes.h"
#include "Battle/Sphere.h"
#include "Battle/TargetVolume.h"
#include "Core/Gte.h"
#include "Core/Math/ScalarMath.h"
#include "Legacy/Psx.h"
#include "Render/Sprites.h"

// decomp: BATTLE.PRG func_800C0B50 (src/BATTLE/BATTLE.PRG/58350.s:8).
void Battle_DrawSphereVolumeWireframe(Battle_TargetVolume* shape, int color) {
  enum {
    kSpatialDimensions = 3,
    kFixedFractionBits = 12,
    kFixedOne = 1 << kFixedFractionBits,
    kPulseAngleStep = 64,
    kIntensityMaximum = 32,
    kWireCommand = 0x44000000,
    kSegmentFlagShift = 25,
    kCoordinateBits = sizeof(int16_t) * CHAR_BIT
  };
  Battle_TargetSphere* sphere =
      reinterpret_cast<Battle_TargetSphere*>(Battle_targetSphere);
  if (sphere == nullptr || shape == nullptr || sphere->vertexCount < 2) return;
  if ((unsigned)sphere->vertexCount >
      sizeof sphere->wireVertices / sizeof sphere->wireVertices[0]) {
    Core_Debugf("targeting", "Invalid wireframe vertex count %d",
                sphere->vertexCount);
    abort();
  }
  const int extent = shape->scale.extents[0] > shape->scale.extents[2]
                         ? shape->scale.extents[0]
                         : shape->scale.extents[2];
  if (extent == 0) return;
  MATRIX volume =
      *Battle_BuildSphereVolumeTransform(shape->scale.extents, shape->angle);
  MATRIX rotation = {0};
  SVECTOR angles = {sphere->wirePhaseA, sphere->wirePhaseB, sphere->wirePhaseC,
                    0};
  Core_Gte_RotMatrixYXZ(&angles, &rotation);
  const int pulse = Core_Gte_Cos(sphere->wireShape * kPulseAngleStep);
  VECTOR scale = {pulse, pulse, pulse, 0};
  Core_Gte_ScaleMatrix(&rotation, &scale);
  Core_Math_MultiplyMatrixIntoRight(&volume, &rotation);
  Render_LoadFrameViewMatrix();
  SVECTOR previous = {0};
  for (int index = 0; index < sphere->vertexCount; ++index) {
    const Battle_ShapeVertex* vertex = &sphere->wireVertices[index];
    int16_t coordinates[kSpatialDimensions];
    for (unsigned axis = 0; axis < kSpatialDimensions; ++axis) {
      int64_t sum = static_cast<int64_t>(shape->position[axis]) * kFixedOne;
      for (unsigned component = 0; component < kSpatialDimensions;
           ++component) {
        sum += static_cast<int64_t>(rotation.m[axis][component]) *
               vertex->xyz[component];
      }
      const int64_t value = sum >> kFixedFractionBits;
      coordinates[axis] = static_cast<int16_t>(value < INT16_MIN   ? INT16_MIN
                                               : value > INT16_MAX ? INT16_MAX
                                                                   : value);
    }
    SVECTOR world = {coordinates[0], coordinates[1], coordinates[2], 0};
    Core_GteWord screen, depth;
    Core_Gte_LoadV0(&world);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2(&screen);
    Core_Gte_StoreSz3Otz(&depth);
    SVECTOR projected = {static_cast<int16_t>(screen),
                         static_cast<int16_t>((u_int)screen >> kCoordinateBits),
                         static_cast<int16_t>(depth), 0};
    if (index != 0 && vertex->opaque6 != 0) {
      const int intensity =
          kIntensityMaximum -
          (projected.vz + previous.vz - sphere->depth * 2) / extent;
      const u_int command_color = ((u_int)kWireCommand | (u_int)color) -
                                  ((u_int)vertex->opaque6 << kSegmentFlagShift);
      Battle_DrawSphereWireLine(&projected, &previous, command_color,
                                intensity);
    }
    previous = projected;
  }
}
}  // extern "C"
