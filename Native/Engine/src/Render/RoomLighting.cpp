// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Render/RoomLighting.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Core/Math/ScalarMath.h"
#include "Field/Gimmicks.h"
#include "Field/Room.h"
#include "Legacy/Psx.h"
#include "Render/Render.h"

enum { kLightingHeaderWords = 3, kNeutralLight = 0x808080, kColorChannels = 3 };

static int32_t SignedWord(uint32_t value) {
  int32_t result;
  memcpy(&result, &value, sizeof(result));
  return result;
}

static int32_t TriangleArea(const int16_t first[2], const int16_t second[2],
                            const int16_t third[2]) {
  const int64_t area =
      static_cast<int64_t>(second[0] - first[0]) * (third[1] - first[1]) -
      static_cast<int64_t>(second[1] - first[1]) * (third[0] - first[0]);
  return SignedWord(static_cast<uint32_t>(area));
}

// decomp: BATTLE.PRG func_80098C3C (src/BATTLE/BATTLE.PRG/2EA3C.s:1857).
int Render_InterpolateRoomLightTriangle(
    uint32_t packed_position, Render_ColorCode* color,
    const Render_RoomLightTriangle* triangle) {
  if (color == nullptr || triangle == nullptr) {
    Core_Debugf("render",
                "Room light interpolation requires triangle and color");
    abort();
  }
  const int16_t point[2] = {
      Core_Math_WrapSigned16(packed_position),
      Core_Math_WrapSigned16(packed_position >> (sizeof(int16_t) * CHAR_BIT))};
  const int32_t area = TriangleArea(triangle->points[0], triangle->points[2],
                                    triangle->points[1]);
  if (area <= 0) return 0;
  const int32_t first =
      TriangleArea(point, triangle->points[2], triangle->points[1]);
  const int32_t second =
      TriangleArea(point, triangle->points[1], triangle->points[0]);
  const int32_t third =
      SignedWord(static_cast<uint32_t>(area) - static_cast<uint32_t>(first) -
                 static_cast<uint32_t>(second));
  if (first < 0 || second < 0 || third < 0) return 0;
  unsigned char* channels = (unsigned char*)color;
  for (unsigned channel = 0; channel < kColorChannels; ++channel) {
    const unsigned shift = channel * CHAR_BIT;
    const uint32_t total = ((triangle->colors[0] >> shift) & UINT8_MAX) *
                               static_cast<uint32_t>(first) +
                           ((triangle->colors[2] >> shift) & UINT8_MAX) *
                               static_cast<uint32_t>(second) +
                           ((triangle->colors[1] >> shift) & UINT8_MAX) *
                               static_cast<uint32_t>(third);
    uint32_t value = total / static_cast<uint32_t>(area);
    if (value > INT32_MAX) value = 0;
    if (value > UINT8_MAX) value = UINT8_MAX;
    channels[channel] = (unsigned char)value;
  }
  return 1;
}

// decomp: BATTLE.PRG func_80098B38 (src/BATTLE/BATTLE.PRG/2EA3C.s:1782).
void Render_SampleRoomLighting(int packed_position, int layer,
                               Render_ColorCode* color) {
  if (color == nullptr) return;
  unsigned fallback = kNeutralLight;
  unsigned header[kLightingHeaderWords];
  const unsigned char* data = (const unsigned char*)(Field_roomData.lighting);
  const size_t size = Field_roomData.header.lightingLength;
  if (!Field_roomData.isLoaded || data == nullptr || size < sizeof header) {
    memcpy(color, &fallback, sizeof fallback);
    return;
  }
  memcpy(header, data, sizeof header);
  fallback = header[0];
  const unsigned count = header[layer != 0 ? 2 : 1];
  const size_t offset =
      sizeof header + (layer != 0 ? static_cast<size_t>(header[1]) *
                                        sizeof(Render_RoomLightTriangle)
                                  : 0);
  if (offset > size ||
      count > (size - offset) / sizeof(Render_RoomLightTriangle)) {
    memcpy(color, &fallback, sizeof fallback);
    return;
  }
  const int16_t point[2] = {
      Core_Math_WrapSigned16(static_cast<uint32_t>(packed_position)),
      Core_Math_WrapSigned16(static_cast<uint32_t>(packed_position) >>
                             (sizeof(int16_t) * CHAR_BIT))};
  for (unsigned index = 0; index < count; ++index) {
    Render_RoomLightTriangle triangle;
    memcpy(&triangle, data + offset + index * sizeof triangle, sizeof triangle);
    if (point[0] < triangle.minimum_x || point[0] > triangle.maximum_x ||
        point[1] < triangle.minimum_z || point[1] > triangle.maximum_z) {
      continue;
    }
    if (Render_InterpolateRoomLightTriangle(
            static_cast<uint32_t>(packed_position), color, &triangle)) {
      return;
    }
  }
  memcpy(color, &fallback, sizeof fallback);
}
}  // extern "C"
