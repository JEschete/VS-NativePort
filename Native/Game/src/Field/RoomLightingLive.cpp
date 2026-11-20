// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomLightingLive.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

#include "Field/Gimmicks.hpp"
#include "Field/Room.hpp"
#include "Render/Palette.hpp"
#include "Render/RoomLighting.hpp"

namespace VS::Field {

std::span<const std::byte> LiveRoomLightingSection() {
  if (!VS::Field::roomData.is_loaded ||
      VS::Field::roomData.lighting == nullptr) {
    return {};
  }
  return {static_cast<const std::byte*>(VS::Field::roomData.lighting),
          static_cast<std::size_t>(VS::Field::roomData.header.lighting_length)};
}

void SampleLiveRoomLighting(std::uint32_t packed_position,
                            Render::RoomLightLayer layer,
                            VS::Render::ColorCode* color) {
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
                            VS::Render::ColorCode* color) {
  SampleLiveRoomLighting(static_cast<std::uint32_t>(packed_position),
                         layer != 0 ? Render::RoomLightLayer::kCeiling
                                    : Render::RoomLightLayer::kFloor,
                         color);
}

}  // namespace VS::Field
