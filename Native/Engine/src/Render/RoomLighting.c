// Hand-maintained (tools/port/owned.csv).
#include "Render/RoomLighting.h"
#include "Legacy/Psx.h"
#include "Field/Room.h"
#include "Render/Render.h"
#include "Field/Gimmicks.h"

#include <stdint.h>
#include <string.h>


typedef struct LightingTriangle {
    short minimumX, minimumZ, maximumX, maximumZ;
    short points[3][2];
    unsigned colors[3];
} LightingTriangle;
_Static_assert(sizeof(LightingTriangle) == 32, "room lighting triangle disk layout");

static int64_t TriangleArea(const short first[2], const short second[2], const short third[2])
{
    return (int64_t)(second[0] - first[0]) * (third[1] - first[1])
        - (int64_t)(second[1] - first[1]) * (third[0] - first[0]);
}

// decomp: BATTLE.PRG func_80098B38 / func_80098C3C (src/BATTLE/BATTLE.PRG/2EA3C.s:1782,1857).
void Render_SampleRoomLighting(int packedPosition, int layer, Render_ColorCode* color)
{
    if (color == NULL) return;
    unsigned fallback = 0x808080;
    const unsigned char* data = Field_roomData.lighting;
    const size_t size = Field_roomData.header.lightingLength;
    if (!Field_roomData.isLoaded || data == NULL || size < 12) {
        memcpy(color, &fallback, sizeof fallback);
        return;
    }
    unsigned header[3];
    memcpy(header, data, sizeof header);
    fallback = header[0];
    const unsigned count = header[layer != 0 ? 2 : 1];
    const size_t offset = 12 + (layer != 0 ? (size_t)header[1] * sizeof(LightingTriangle) : 0);
    if (offset > size || count > (size - offset) / sizeof(LightingTriangle)) {
        memcpy(color, &fallback, sizeof fallback);
        return;
    }
    const short point[2] = {(short)packedPosition, (short)((unsigned)packedPosition >> 16)};
    for (unsigned index = 0; index < count; ++index) {
        LightingTriangle triangle;
        memcpy(&triangle, data + offset + index * sizeof triangle, sizeof triangle);
        if (point[0] < triangle.minimumX || point[0] > triangle.maximumX
            || point[1] < triangle.minimumZ || point[1] > triangle.maximumZ) continue;
        const int64_t area = TriangleArea(triangle.points[0], triangle.points[2], triangle.points[1]);
        if (area <= 0) continue;
        const int64_t first = TriangleArea(point, triangle.points[2], triangle.points[1]);
        const int64_t second = TriangleArea(point, triangle.points[1], triangle.points[0]);
        const int64_t third = area - first - second;
        if (first < 0 || second < 0 || third < 0) continue;
        unsigned char* channels = (unsigned char*)color;
        for (unsigned channel = 0; channel < 3; ++channel) {
            const unsigned shift = channel * 8;
            const int64_t total = ((triangle.colors[0] >> shift) & 255) * first
                + ((triangle.colors[2] >> shift) & 255) * second
                + ((triangle.colors[1] >> shift) & 255) * third;
            channels[channel] = (unsigned char)(total / area);
        }
        return;
    }
    memcpy(color, &fallback, sizeof fallback);
}
