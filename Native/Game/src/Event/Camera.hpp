#pragma once
// The event camera workspace (decomp: D_800F4BA4_t), which BATTLE.PRG
// allocated from the top of the heap and the event script camera, lens, shake
// and screen opcodes update. The typed rules are EventCameraTrack.hpp,
// EventCameraLens.hpp, EventCameraFrame.hpp and EventScreenTweens.hpp; the
// Event*Live.cpp bindings load and store these records. The original
// toolchain's plain char was unsigned (-funsigned-char), so its bytes are
// std::uint8_t.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Gte.hpp"
#include "Core/Math/Distance.hpp"
#include "Event/Screen.hpp"

namespace VS::Event {

// decomp: D_800F4BA4_t2 (src/BATTLE/BATTLE.PRG/4DEA8.c:37)
// One end of the camera, the look-at point or the eye, moving toward its
// destination.
struct CameraTrack {
  std::uint8_t motion_mode{};
  std::uint8_t easing_primary{};
  std::uint8_t easing_secondary{};
  std::uint8_t easing_tertiary{};
  std::uint8_t elapsed_frames{};
  std::uint8_t duration_frames{};
  std::uint8_t follow_request{};
  std::uint8_t is_following{};
  std::int32_t motion_scalar{};
  std::int32_t motion_scalar_delta{};
  std::int16_t orbit_angle{};
  std::int16_t orbit_angle_delta{};
  VS::Core::Gte::VECTOR start_position{};
  VS::Core::Gte::VECTOR destination{};
  VS::Core::Gte::VECTOR displacement{};
  VS::Core::Gte::VECTOR height_offset{};
  VS::Core::Gte::VECTOR follow_delta{};
  VS::Core::Gte::VECTOR follow_position{};
  VS::Core::Gte::VECTOR previous_camera_position{};
  VS::Core::Gte::VECTOR computed_position{};
  VS::Core::Gte::VECTOR current_position{};
  VS::Core::Gte::VECTOR partner_position{};
};

// decomp: func_800BD57C_t (src/BATTLE/BATTLE.PRG/4DEA8.c:62)
// The camera roll or the captured screen's roll.
struct AngleTween {
  std::uint8_t has_changed{};
  std::uint8_t easing_mode{};
  std::int16_t angle{};
  std::int16_t start_angle{};
  std::int16_t delta_angle{};
  std::int16_t elapsed_frames{};
  std::int16_t duration_frames{};
};

// decomp: func_800BDF6C_t (src/BATTLE/BATTLE.PRG/4DEA8.c:72), as the code
// addressed it: the decomp declares the records at 0x1A4 and 0x1B0, but
// vs_battle_script_EA_EB_EC_EE_EF_camera and func_800BE180 walk them from
// &unk1A4.unk2, so each record starts two bytes later and the active count
// (unk1A4.unk0.s16) stands before them (CameraState::shake_active_count).
struct ShakeOscillator {
  std::uint8_t reserved00{};
  std::uint8_t envelope_shape{};
  std::int16_t displacement{};
  std::int16_t cycles{};
  std::int16_t amplitude{};
  std::uint8_t elapsed_frames{};
  std::uint8_t duration_frames{};
  // Bit 0 shakes the look-at point and bit 1 the eye; bits 2-7 hold each
  // axis's direction, minus 1.
  std::uint8_t axis_mask{};
  std::uint8_t reserved0b{};
};

// decomp: func_800BDF6C_unk180_t (src/BATTLE/BATTLE.PRG/4DEA8.c:86)
// The projection distance, near clip or far clip.
struct LensTween {
  std::uint8_t has_changed{};
  std::uint8_t easing_mode{};
  std::int16_t value{};
  std::int16_t start_value{};
  std::int16_t delta_value{};
  std::uint8_t elapsed_frames{};
  std::uint8_t duration_frames{};
  std::uint8_t reserved0a{};
  std::uint8_t reserved0b{};
};

inline constexpr std::size_t kShakeOscillatorCount = 2;
// decomp: D_800F4BA4_unk1D0..unk1E7 (vs_ActorPosition and two words).
inline constexpr std::size_t kActorQueryScratchWords = 7;

// decomp: D_800F4BA4_t (src/BATTLE/BATTLE.PRG/4DEA8.c:118)
struct CameraState {
  CameraTrack look_at{};
  CameraTrack eye{};
  AngleTween camera_roll{};
  AngleTween captured_screen_roll{};
  LensTween projection_distance{};
  LensTween near_clip{};
  LensTween far_clip{};
  // How many oscillators moved on the last shake update.
  std::int16_t shake_active_count{};
  std::array<ShakeOscillator, kShakeOscillatorCount> shake_oscillators{};
  // The summed shake, rotated by the camera's yaw and added to each end.
  Core::Math::Position3 shake_look_at_offset{};
  std::int16_t reserved1c4{};
  Core::Math::Position3 shake_eye_offset{};
  // Camera-motion operation 10 queried an actor's position into these words
  // (decomp unk1CC..unk1E0); the native track rule keeps that query local.
  std::array<std::int32_t, kActorQueryScratchWords> actor_query_scratch{};
  // 0x2000 when the camera has no origin actor.
  std::uint16_t origin_actor_id{};
  std::uint16_t reserved1ea{};
  VS::Core::Gte::VECTOR origin_offset{};
  TripleTween screen_scale{};
  TripleTween screen_tint{};
  TripleTween screen_translation{};
  TripleTween screen_effect_tween{};
  VS::Core::Gte::VECTOR last_terrain_position{};
};

// The byte sizes and field offsets the original stored these records with.
namespace layout {
inline constexpr std::size_t kCameraTrackBytes = 0xB4;
static_assert(sizeof(CameraTrack) == kCameraTrackBytes);
inline constexpr std::size_t kAngleTweenBytes = 0xC;
static_assert(sizeof(AngleTween) == kAngleTweenBytes);
inline constexpr std::size_t kShakeOscillatorBytes = 0xC;
static_assert(sizeof(ShakeOscillator) == kShakeOscillatorBytes);
inline constexpr std::size_t kLensTweenBytes = 0xC;
static_assert(sizeof(LensTween) == kLensTweenBytes);
inline constexpr std::size_t kCameraRollOffset = 0x168;
static_assert(offsetof(CameraState, camera_roll) == kCameraRollOffset);
inline constexpr std::size_t kProjectionDistanceOffset = 0x180;
static_assert(offsetof(CameraState, projection_distance) ==
              kProjectionDistanceOffset);
inline constexpr std::size_t kShakeActiveCountOffset = 0x1A4;
static_assert(offsetof(CameraState, shake_active_count) ==
              kShakeActiveCountOffset);
inline constexpr std::size_t kShakeOscillatorsOffset = 0x1A6;
static_assert(offsetof(CameraState, shake_oscillators) ==
              kShakeOscillatorsOffset);
inline constexpr std::size_t kShakeLookAtOffsetOffset = 0x1BE;
static_assert(offsetof(CameraState, shake_look_at_offset) ==
              kShakeLookAtOffsetOffset);
inline constexpr std::size_t kShakeEyeOffsetOffset = 0x1C6;
static_assert(offsetof(CameraState, shake_eye_offset) == kShakeEyeOffsetOffset);
inline constexpr std::size_t kOriginActorIdOffset = 0x1E8;
static_assert(offsetof(CameraState, origin_actor_id) == kOriginActorIdOffset);
inline constexpr std::size_t kScreenScaleOffset = 0x1FC;
static_assert(offsetof(CameraState, screen_scale) == kScreenScaleOffset);
inline constexpr std::size_t kLastTerrainPositionOffset = 0x27C;
static_assert(offsetof(CameraState, last_terrain_position) ==
              kLastTerrainPositionOffset);
inline constexpr std::size_t kCameraStateBytes = 0x28C;
static_assert(sizeof(CameraState) == kCameraStateBytes);
}  // namespace layout

}  // namespace VS::Event
