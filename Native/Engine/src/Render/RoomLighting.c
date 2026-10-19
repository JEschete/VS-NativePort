// Hand-maintained (tools/port/owned.csv).
#include "Render/RoomLighting.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "Field/Gimmicks.h"
#include "Field/Room.h"
#include "Legacy/Psx.h"
#include "Render/Render.h"

enum {
  kTriangleCorners = 3,
  kLightingHeaderWords = 3,
  kNeutralLight = 0x808080
};

typedef struct LightingTriangle {
  int16_t minimum_x, minimum_z, maximum_x, maximum_z;
  int16_t points[kTriangleCorners][2];
  unsigned colors[kTriangleCorners];
} LightingTriangle;
_Static_assert(sizeof(LightingTriangle) == 32,
               "room lighting triangle disk layout");

static int64_t TriangleArea(const int16_t first[2], const int16_t second[2],
                            const int16_t third[2]) {
  return (int64_t)(second[0] - first[0]) * (third[1] - first[1]) -
         (int64_t)(second[1] - first[1]) * (third[0] - first[0]);
}

// decomp: BATTLE.PRG func_80098B38 / func_80098C3C
// (src/BATTLE/BATTLE.PRG/2EA3C.s:1782,1857).
void Render_SampleRoomLighting(int packed_position, int layer,
                               Render_ColorCode* color) {
  if (color == NULL) return;
  unsigned fallback = kNeutralLight;
  unsigned header[kLightingHeaderWords];
  const unsigned char* data = Field_roomData.lighting;
  const size_t size = Field_roomData.header.lightingLength;
  if (!Field_roomData.isLoaded || data == NULL || size < sizeof header) {
    memcpy(color, &fallback, sizeof fallback);
    return;
  }
  memcpy(header, data, sizeof header);
  fallback = header[0];
  const unsigned count = header[layer != 0 ? 2 : 1];
  const size_t offset =
      sizeof header +
      (layer != 0 ? (size_t)header[1] * sizeof(LightingTriangle) : 0);
  if (offset > size || count > (size - offset) / sizeof(LightingTriangle)) {
    memcpy(color, &fallback, sizeof fallback);
    return;
  }
  const int16_t point[2] = {
      (int16_t)packed_position,
      (int16_t)((unsigned)packed_position >> (sizeof(int16_t) * CHAR_BIT))};
  for (unsigned index = 0; index < count; ++index) {
    LightingTriangle triangle;
    memcpy(&triangle, data + offset + index * sizeof triangle, sizeof triangle);
    if (point[0] < triangle.minimum_x || point[0] > triangle.maximum_x ||
        point[1] < triangle.minimum_z || point[1] > triangle.maximum_z) {
      continue;
    }
    const int64_t area = TriangleArea(triangle.points[0], triangle.points[2],
                                      triangle.points[1]);
    if (area <= 0) continue;
    const int64_t first =
        TriangleArea(point, triangle.points[2], triangle.points[1]);
    const int64_t second =
        TriangleArea(point, triangle.points[1], triangle.points[0]);
    const int64_t third = area - first - second;
    if (first < 0 || second < 0 || third < 0) continue;
    unsigned char* channels = (unsigned char*)color;
    for (unsigned channel = 0;
         channel < sizeof color->red + sizeof color->green + sizeof color->blue;
         ++channel) {
      const unsigned shift = channel * CHAR_BIT;
      const int64_t total =
          ((triangle.colors[0] >> shift) & UINT8_MAX) * first +
          ((triangle.colors[2] >> shift) & UINT8_MAX) * second +
          ((triangle.colors[1] >> shift) & UINT8_MAX) * third;
      channels[channel] = (unsigned char)(total / area);
    }
    return;
  }
  memcpy(color, &fallback, sizeof fallback);
}
