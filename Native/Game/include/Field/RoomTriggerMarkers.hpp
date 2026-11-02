#pragma once

#include <cstdint>
#include <span>

namespace VS::Field {
inline constexpr int kTriggerMarkerTileUnits = 128;
// Floor trigger kinds at or above this have no marker.
inline constexpr std::int16_t kTriggerMarkerKindLimit = 16;

// The marker-relevant fields of one MPD floor trigger.
struct FloorTriggerMarker {
  std::int16_t tile_x = 0;
  std::int16_t tile_z = 0;
  std::int16_t kind = 0;
  bool activated = false;
  std::uint8_t fade = 0;
};

struct TriggerMarkerFrame {
  int tick_speed = 0;
  // Either state gate the original checks suppresses reveals.
  bool reveal_suppressed = false;
  std::uint16_t room_animation_ticks = 0;
};

// Ticks carried between frames toward the next texture animation step.
struct TriggerMarkerAnimation {
  std::uint16_t pending_ticks = 0;
};

struct TriggerMarkerAnimationFrame {
  std::int16_t texture_x = 0;
  std::int16_t palette_x = 0;

  friend bool operator==(const TriggerMarkerAnimationFrame&,
                         const TriggerMarkerAnimationFrame&) = default;
};

// The far corner of the marker quad and its shade on every vertex.
struct TriggerMarkerPlacement {
  std::int16_t corner_x = 0;
  std::int16_t corner_y = 0;
  std::int16_t corner_z = 0;
  std::uint8_t shade = 0;

  friend bool operator==(const TriggerMarkerPlacement&,
                         const TriggerMarkerPlacement&) = default;
};

class RoomTriggerMarkerRenderer {
 public:
  RoomTriggerMarkerRenderer() = default;
  RoomTriggerMarkerRenderer(const RoomTriggerMarkerRenderer&) = delete;
  RoomTriggerMarkerRenderer& operator=(const RoomTriggerMarkerRenderer&) =
      delete;
  RoomTriggerMarkerRenderer(RoomTriggerMarkerRenderer&&) = delete;
  RoomTriggerMarkerRenderer& operator=(RoomTriggerMarkerRenderer&&) = delete;
  virtual ~RoomTriggerMarkerRenderer() = default;

  // The collision floor query's code: a signed 15-bit height below a flag.
  [[nodiscard]] virtual std::uint16_t FloorHeightCode(int world_x,
                                                      int world_z) = 0;
  virtual void ShowAnimationFrame(const TriggerMarkerAnimationFrame& frame) = 0;
  virtual void DrawMarker(const TriggerMarkerPlacement& placement) = 0;
};

// Advances every trigger's marker fade and draws the visible markers, stepping
// the shared texture animation once before the first one.
void DrawRoomTriggerMarkers(std::span<FloorTriggerMarker> triggers,
                            const TriggerMarkerFrame& frame,
                            TriggerMarkerAnimation& animation,
                            RoomTriggerMarkerRenderer& renderer);

// Places one marker one tile beyond (world_x, world_z) at the floor height.
void DrawRoomTriggerMarker(int world_x, int world_z, std::int16_t height,
                           std::uint32_t brightness,
                           RoomTriggerMarkerRenderer& renderer);
}  // namespace VS::Field
