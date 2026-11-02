// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomTriggerMarkers.hpp"

#include <cstdint>
#include <span>

namespace VS::Field {
namespace {
constexpr unsigned kMaximumFade = 128;
constexpr unsigned kRevealRate = 2;
constexpr int kAnimationInterval = 8;
constexpr int kAnimationFrames = 5;
constexpr int kTextureSourceX = 72;
constexpr int kPaletteSourceX = 4;
constexpr int kPaletteStride = 4;
constexpr unsigned kFloorHeightMask = 0x7FFF;
constexpr int kFloorHeightSign = 0x4000;

// Reveals stop once the signed fade byte turns negative (128 and above);
// hiding counts down to zero, one step per tick.
std::uint8_t AdvanceFade(const FloorTriggerMarker& trigger,
                         const TriggerMarkerFrame& frame) {
  unsigned fade = trigger.fade;
  if (frame.tick_speed <= 0) return trigger.fade;
  const auto ticks = static_cast<unsigned>(frame.tick_speed);
  if (!frame.reveal_suppressed && trigger.activated) {
    if (fade < kMaximumFade) {
      const unsigned remaining = kMaximumFade - fade;
      fade += ticks >= (remaining + 1) / kRevealRate ? remaining
                                                     : ticks * kRevealRate;
    }
  } else {
    fade = ticks >= fade ? 0 : fade - ticks;
  }
  return static_cast<std::uint8_t>(fade);
}

void AnimateTexture(const TriggerMarkerFrame& frame,
                    TriggerMarkerAnimation& animation,
                    RoomTriggerMarkerRenderer& renderer) {
  animation.pending_ticks =
      static_cast<std::uint16_t>(animation.pending_ticks + frame.tick_speed);
  if (animation.pending_ticks < kAnimationInterval) return;
  animation.pending_ticks -= kAnimationInterval;
  const int step =
      (frame.room_animation_ticks / kAnimationInterval) % kAnimationFrames;
  renderer.ShowAnimationFrame(TriggerMarkerAnimationFrame{
      static_cast<std::int16_t>(step * kAnimationInterval + kTextureSourceX),
      static_cast<std::int16_t>(step * kPaletteStride + kPaletteSourceX)});
}

std::int16_t DecodeFloorHeight(std::uint16_t code) {
  const unsigned height = code & kFloorHeightMask;
  return static_cast<std::int16_t>(static_cast<int>(height ^ kFloorHeightSign) -
                                   kFloorHeightSign);
}
}  // namespace

// decomp: BATTLE.PRG func_8008DF14 (src/BATTLE/BATTLE.PRG/146C.c:17164).
void DrawRoomTriggerMarkers(std::span<FloorTriggerMarker> triggers,
                            const TriggerMarkerFrame& frame,
                            TriggerMarkerAnimation& animation,
                            RoomTriggerMarkerRenderer& renderer) {
  bool first_marker = true;
  for (auto& trigger : triggers) {
    trigger.fade = AdvanceFade(trigger, frame);
    if (trigger.fade == 0 || trigger.kind >= kTriggerMarkerKindLimit) {
      continue;
    }
    if (first_marker) {
      AnimateTexture(frame, animation, renderer);
      first_marker = false;
    }
    const int world_x = trigger.tile_x * kTriggerMarkerTileUnits;
    const int world_z = trigger.tile_z * kTriggerMarkerTileUnits;
    DrawRoomTriggerMarker(
        world_x, world_z,
        DecodeFloorHeight(renderer.FloorHeightCode(world_x, world_z)),
        trigger.fade, renderer);
  }
}

// decomp: BATTLE.PRG func_8008E19C (src/BATTLE/BATTLE.PRG/146C.c:17216).
void DrawRoomTriggerMarker(int world_x, int world_z, std::int16_t height,
                           std::uint32_t brightness,
                           RoomTriggerMarkerRenderer& renderer) {
  renderer.DrawMarker(TriggerMarkerPlacement{
      static_cast<std::int16_t>(world_x + kTriggerMarkerTileUnits), height,
      static_cast<std::int16_t>(world_z + kTriggerMarkerTileUnits),
      static_cast<std::uint8_t>(brightness)});
}
}  // namespace VS::Field
