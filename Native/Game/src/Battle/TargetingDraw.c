// Hand-maintained (tools/port/owned.csv).
#include "Battle/TargetingDraw.h"

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
  Battle_TargetSphere* sphere = (void*)Battle_targetSphere;
  if (sphere == NULL || shape == NULL || sphere->vertexCount < 2) return;
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
  const int pulse = Core_Gte_Cos(sphere->wireShape * 64);
  VECTOR scale = {pulse, pulse, pulse, 0};
  Core_Gte_ScaleMatrix(&rotation, &scale);
  Core_Math_MultiplyMatrixIntoRight(&volume, &rotation);
  Render_LoadFrameViewMatrix();
  SVECTOR previous = {0};
  for (int index = 0; index < sphere->vertexCount; ++index) {
    const Battle_ShapeVertex* vertex = &sphere->wireVertices[index];
    int16_t coordinates[3];
    for (unsigned axis = 0; axis < 3; ++axis) {
      long long sum = (long long)shape->position[axis] * 4096;
      for (unsigned component = 0; component < 3; ++component) {
        sum += (long long)rotation.m[axis][component] * vertex->xyz[component];
      }
      const long long value = sum >> 12;
      coordinates[axis] = value < -32768  ? -32768
                          : value > 32767 ? 32767
                                          : (int16_t)value;
    }
    SVECTOR world = {coordinates[0], coordinates[1], coordinates[2], 0};
    Core_GteWord screen, depth;
    Core_Gte_LoadV0(&world);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2(&screen);
    Core_Gte_StoreSz3Otz(&depth);
    SVECTOR projected = {(int16_t)screen, (int16_t)((u_int)screen >> 16),
                         (int16_t)depth, 0};
    if (index != 0 && vertex->opaque6 != 0) {
      const int intensity =
          32 - (projected.vz + previous.vz - sphere->depth * 2) / extent;
      const u_int command_color =
          (0x44000000u | (u_int)color) - ((u_int)vertex->opaque6 << 25);
      Battle_DrawSphereWireLine(&projected, &previous, command_color,
                                intensity);
    }
    previous = projected;
  }
}
