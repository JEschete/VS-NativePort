// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomLightingLive.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

#include "Render/RoomLighting.hpp"
extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Room.h"
#include "Render/Palette.h"
}

namespace VS::Field {

std::span<const std::byte> LiveRoomLightingSection() {
  if (!Field_roomData.isLoaded || Field_roomData.lighting == nullptr) {
    return {};
  }
  return {static_cast<const std::byte*>(Field_roomData.lighting),
          static_cast<std::size_t>(Field_roomData.header.lightingLength)};
}

void SampleLiveRoomLighting(std::uint32_t packed_position,
                            Render::RoomLightLayer layer,
                            Render_ColorCode* color) {
  if (color == nullptr) return;
  const Render::RoomLightColor sample = Render::SampleRoomLighting(
      LiveRoomLightingSection(), Render::UnpackRoomLightPoint(packed_position),
      layer, {color->red, color->green, color->blue, color->code});
  color->red = sample.red;
  color->green = sample.green;
  color->blue = sample.blue;
  color->code = sample.code;
}

void SampleLiveRoomLighting(int packed_position, int layer,
                            Render_ColorCode* color) {
  SampleLiveRoomLighting(static_cast<std::uint32_t>(packed_position),
                         layer != 0 ? Render::RoomLightLayer::kCeiling
                                    : Render::RoomLightLayer::kFloor,
                         color);
}

}  // namespace VS::Field
