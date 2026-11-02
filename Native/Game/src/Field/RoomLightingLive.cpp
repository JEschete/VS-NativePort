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

void SampleLiveRoomLighting(int packed_position, int layer,
                            Render_ColorCode* color) {
  if (color == nullptr) return;
  std::span<const std::byte> lighting;
  if (Field_roomData.isLoaded && Field_roomData.lighting != nullptr) {
    lighting = {static_cast<const std::byte*>(Field_roomData.lighting),
                static_cast<std::size_t>(Field_roomData.header.lightingLength)};
  }
  const Render::RoomLightColor sample = Render::SampleRoomLighting(
      lighting,
      Render::UnpackRoomLightPoint(static_cast<std::uint32_t>(packed_position)),
      layer != 0 ? Render::RoomLightLayer::kCeiling
                 : Render::RoomLightLayer::kFloor,
      {color->red, color->green, color->blue, color->code});
  color->red = sample.red;
  color->green = sample.green;
  color->blue = sample.blue;
  color->code = sample.code;
}

}  // namespace VS::Field
