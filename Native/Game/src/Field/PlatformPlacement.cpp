#include "Field/PlatformPlacement.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "Core/Diagnostics.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

namespace {
constexpr int kPlatformLayerBias = 2;
constexpr int kPlatformCapacity = 4;
constexpr int kNoBounceFrame = 7;

const Field_MpdMovingPlatform* PlatformAtLayer(int support_layer) {
  const auto bytes = Field_roomData.header.movingPlatformsLength;
  if (support_layer < kPlatformLayerBias ||
      support_layer >= kPlatformLayerBias + kPlatformCapacity ||
      bytes % sizeof(Field_MpdMovingPlatform) != 0 ||
      bytes > kPlatformCapacity * sizeof(Field_MpdMovingPlatform)) {
    Core_Errorf("field.platform", "Invalid moving-platform layer %d or span %u",
                support_layer, bytes);
    return nullptr;
  }
  const int index = support_layer - kPlatformLayerBias;
  if (Field_roomData.movingPlatforms == nullptr ||
      static_cast<std::size_t>(index) >=
          bytes / sizeof(Field_MpdMovingPlatform)) {
    return nullptr;
  }
  return &Field_roomData.movingPlatforms[index];
}
}  // namespace

// F00901/3A1A0.c:274 and func_8008D2C0/146C.c:16763 share this transform.
// Placement may precede the scene's visibility command; collision stays hidden.
int Field_CopyMovingPlatformPlacement(int support_layer,
                                      Field_PlatformContactPoint* contact) {
  const auto* platform = PlatformAtLayer(support_layer);
  if (contact == nullptr || platform == nullptr) {
    Core_Errorf("field.platform",
                "Placement requires authored platform layer %d and an output",
                support_layer);
    return 0;
  }
  const int index = support_layer - kPlatformLayerBias;
  const int bounce_frame = Field_platformBounceFrames[index];
  if (bounce_frame < 0 || bounce_frame > kNoBounceFrame) {
    Core_Errorf("field.platform", "Invalid bounce frame %d for platform %d",
                bounce_frame, index);
    return 0;
  }
  // The original seven-entry table is followed by a zero sentinel in BATTLE.
  const int bounce = bounce_frame == kNoBounceFrame
                         ? 0
                         : Field_kPlatformBounceOffsets[bounce_frame];
  const int x = platform->positionX / ONE;
  const int height = bounce - platform->positionY / ONE;
  const int z = platform->positionZ / ONE;
  if (!std::in_range<std::int16_t>(x) || !std::in_range<std::int16_t>(height) ||
      !std::in_range<std::int16_t>(z)) {
    Core_Errorf("field.platform",
                "Platform %d placement exceeds actor coordinates (%d,%d,%d)",
                index, x, height, z);
    return 0;
  }
  *contact = {static_cast<std::int16_t>(x), static_cast<std::int16_t>(height),
              static_cast<std::int16_t>(z), static_cast<std::int16_t>(index)};
  return 1;
}

// F00920/F00938: func_800A4A24, original BATTLE/3A1A0.c:1089,40564.c:120.
Field_PlatformContactPoint* Field_FindMovingPlatformById(int support_layer) {
  const auto* platform = PlatformAtLayer(support_layer);
  if (platform == nullptr || platform->isHidden) return nullptr;
  thread_local Field_PlatformContactPoint contact;
  if (!Field_CopyMovingPlatformPlacement(support_layer, &contact)) {
    return nullptr;
  }
  return &contact;
}
