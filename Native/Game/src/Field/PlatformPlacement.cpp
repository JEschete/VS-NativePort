// Hand-maintained (tools/port/owned.csv).
#include "Field/PlatformPlacement.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "Core/Log.hpp"

namespace VS::Field {
namespace {
constexpr int kFixedOne = 4096;
constexpr std::uint8_t kMotionWaypointMask = 3;
constexpr std::uint8_t kMotionSegmentMask = 7;
constexpr std::uint8_t kSettledBounceFrame = 7;
constexpr std::uint8_t kBounceFrameLimit = 8;

std::int32_t AddMotionWord(std::int32_t first, std::int32_t second) {
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(first) +
                                     static_cast<std::uint32_t>(second));
}

std::int32_t SubtractMotionWord(std::int32_t first, std::int32_t second) {
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(first) -
                                     static_cast<std::uint32_t>(second));
}

std::int16_t MotionHalfword(std::int32_t value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

void DepartMovingPlatformWaypoint(MovingPlatformMotionState& state) {
  auto& path = state.waypoint;
  path.motion_ticks = 0;
  path.waiting = false;
  path.start_waypoint = path.current_waypoint;
  path.current_segment = static_cast<std::uint8_t>((path.current_segment + 1) &
                                                   kMotionSegmentMask);
  state.docked = false;
  if (path.start_waypoint == 0) {
    path.moving_forward = true;
    path.current_segment = 0;
  } else if (path.start_waypoint == path.end_waypoint) {
    path.moving_forward = false;
  }
  const int next = path.current_waypoint + (path.moving_forward ? 1 : -1);
  path.current_waypoint = static_cast<std::uint8_t>(next & kMotionWaypointMask);
}

std::optional<std::size_t> PlatformIndex(const MovingPlatformRoom& room,
                                         int support_layer) {
  if (support_layer < kFirstPlatformLayer ||
      support_layer >= kFirstPlatformLayer + kMovingPlatformCapacity ||
      room.platforms.size() > kMovingPlatformCapacity) {
    VS_LOG_ERROR("field.platform",
                 "Invalid moving-platform layer {} or {} platforms",
                 support_layer, room.platforms.size());
    return std::nullopt;
  }
  const auto index =
      static_cast<std::size_t>(support_layer - kFirstPlatformLayer);
  if (index >= room.platforms.size()) return std::nullopt;
  return index;
}

std::optional<PlatformContact> ContactOf(const MovingPlatformRoom& room,
                                         std::size_t index) {
  const MovingPlatformPose& platform = room.platforms[index];
  const int frame = platform.bounce_frame;
  if (frame < 0 || std::cmp_greater(frame, room.bounce_offsets.size())) {
    VS_LOG_ERROR("field.platform", "Invalid bounce frame {} for platform {}",
                 frame, index);
    return std::nullopt;
  }
  // The original seven-entry table is followed by a zero sentinel in BATTLE.
  const int bounce = std::cmp_equal(frame, room.bounce_offsets.size())
                         ? 0
                         : room.bounce_offsets[static_cast<std::size_t>(frame)];
  const int x = platform.x / kFixedOne;
  const int height = bounce - platform.y / kFixedOne;
  const int z = platform.z / kFixedOne;
  if (!std::in_range<std::int16_t>(x) || !std::in_range<std::int16_t>(height) ||
      !std::in_range<std::int16_t>(z)) {
    VS_LOG_ERROR("field.platform",
                 "Platform {} placement exceeds actor coordinates ({},{},{})",
                 index, x, height, z);
    return std::nullopt;
  }
  return PlatformContact{.x = static_cast<std::int16_t>(x),
                         .height = static_cast<std::int16_t>(height),
                         .z = static_cast<std::int16_t>(z),
                         .platform_index = static_cast<std::int16_t>(index)};
}
}  // namespace

// decomp: BATTLE.PRG func_8008CEB0 (146C.c:16688),260MIPS shadow geometry.
std::optional<MovingPlatformShadow> BuildMovingPlatformShadow(
    const MovingPlatformPose& platform,
    const MovingPlatformShadowSurface& surface) {
  constexpr std::uint8_t kFirstPitSurface = 15;
  constexpr std::uint8_t kPitSurfaceEnd = 19;
  constexpr int kFloorMask = 0x7FFF;
  constexpr int kFloorSign = 0x4000;
  constexpr int kFloorModulus = 0x8000;
  constexpr int kMaximumFade = 223;
  constexpr int kMaximumShade = 255;
  constexpr int kBaseRadius = 32;
  constexpr int kRadiusShadeDivisor = 8;
  constexpr int kShadeBias = 16;
  if (platform.hidden || !surface.geometry_present ||
      (surface.pit && (surface.surface_kind < kFirstPitSurface ||
                       surface.surface_kind >= kPitSurfaceEnd))) {
    return std::nullopt;
  }
  int height = surface.floor_height & kFloorMask;
  if ((height & kFloorSign) != 0) height -= kFloorModulus;
  const int y = -(platform.y / kFixedOne);
  if (height <= y) return std::nullopt;
  const int fade = std::min((height - y) / 2, kMaximumFade);
  const int shade = kMaximumShade - fade;
  const int radius = kBaseRadius + shade / kRadiusShadeDivisor;
  const int x = platform.x / kFixedOne;
  const int z = platform.z / kFixedOne;
  const auto vertex = [height](int corner_x, int corner_z) {
    return MovingPlatformShadowVertex{MotionHalfword(corner_x),
                                      MotionHalfword(height),
                                      MotionHalfword(corner_z)};
  };
  return MovingPlatformShadow{
      .vertices = {vertex(x - radius, z - radius),
                   vertex(x + radius, z - radius),
                   vertex(x - radius, z + radius),
                   vertex(x + radius, z + radius)},
      .shade = static_cast<std::uint8_t>(shade / 2 + kShadeBias)};
}

void DrawMovingPlatformShadows(std::span<const MovingPlatformPose> platforms,
                               MovingPlatformShadowHost& host) {
  constexpr std::int32_t kMinimumShadowDepth = 4;
  constexpr std::int32_t kShadowDepthLimit = 2048;
  host.BeginView();
  for (std::size_t index = 0; index < platforms.size(); ++index) {
    const auto& platform = platforms[index];
    if (platform.hidden) continue;
    const auto shadow =
        BuildMovingPlatformShadow(platform, host.SurfaceFor(index, platform));
    if (!shadow) continue;
    const auto depth = host.Project(*shadow);
    if (depth > kMinimumShadowDepth && depth < kShadowDepthLimit) {
      host.Draw(*shadow, depth);
    }
  }
}

// decomp: BATTLE.PRG func_8008C538 (146C.c:16489),95MIPS reset/geometry/entry.
MovingPlatformInitialization InitializeMovingPlatforms(
    const MovingPlatformRoom& room, std::uint16_t entrance_flags) {
  constexpr std::uint8_t kInitialBounceFrame = 7;
  constexpr int kEntranceLayerShift = 13;
  MovingPlatformInitialization initialization;
  initialization.bounce_frames.fill(kInitialBounceFrame);
  for (std::size_t index = 0; index < room.platforms.size(); ++index) {
    const auto& platform = room.platforms[index];
    initialization.geometry.push_back({.platform_index = index,
                                       .hide = platform.hidden,
                                       .x = platform.x / kFixedOne,
                                       .y = -(platform.y / kFixedOne),
                                       .z = platform.z / kFixedOne});
  }
  const int layer = entrance_flags >> kEntranceLayerShift;
  if (layer >= kFirstPlatformLayer) {
    initialization.entrance_platform = layer - kFirstPlatformLayer;
  }
  return initialization;
}

// decomp: BATTLE.PRG func_8008D2C0 (146C.c:16763),68MIPS visible order/SH.
std::vector<PlatformContact> CollectMovingPlatformPositions(
    const MovingPlatformRoom& room) {
  std::vector<PlatformContact> contacts;
  const auto word = [](std::int32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
  };
  for (std::size_t index = 0; index < room.platforms.size(); ++index) {
    const auto& platform = room.platforms[index];
    if (platform.hidden) continue;
    const int frame = platform.bounce_frame;
    if (frame < 0 || std::cmp_greater(frame, room.bounce_offsets.size())) {
      VS_LOG_ERROR("field.platform", "Invalid collector bounce frame {}",
                   frame);
      return {};
    }
    const int bounce =
        std::cmp_equal(frame, room.bounce_offsets.size())
            ? 0
            : room.bounce_offsets[static_cast<std::size_t>(frame)];
    contacts.push_back(
        {word(platform.x / kFixedOne), word(bounce - platform.y / kFixedOne),
         word(platform.z / kFixedOne), word(static_cast<std::int32_t>(index))});
  }
  return contacts;
}

// decomp: BATTLE.PRG func_8008E4DC (146C.c:17333),69MIPS
// palette/group/effect/SB.
bool SetPlatformFreezeAppearance(
    MovingPlatformAppearanceState& state, std::int32_t requested,
    std::span<const MovingPlatformAppearanceTarget> platforms,
    std::optional<std::int32_t> effect, MovingPlatformAppearanceHost& host) {
  if (state.tint == requested) return true;
  if (!host.LoadPalette(requested != 0)) return false;
  for (const auto& platform : platforms) {
    if (platform.swap_colors &&
        !host.SwapGroupColors(platform.geometry_group)) {
      return false;
    }
  }
  if (effect && *effect >= 0 && !host.ToggleEffect(*effect)) return false;
  if (!host.RefreshTint(state)) return false;
  state.tint = static_cast<std::uint8_t>(state.tint == 0);
  return host.StoreTint(state.tint);
}

// decomp: BATTLE.PRG func_8008C49C (146C.c:16475),31MIPS control7bits then
// bounce.
void SetMovingPlatformActor(MovingPlatformControlState& state,
                            std::int32_t control, std::int16_t freeze_frames) {
  constexpr std::uint8_t kControlMask = 0x7F;
  state.control_state = static_cast<std::uint8_t>(control & kControlMask);
  if (control >= 0 && freeze_frames == 0) state.bounce_frame = 0;
}

// decomp: BATTLE.PRG func_8008D594 (146C.c:16855), original SB input low byte.
void SetMovingPlatformsPaused(MovingPlatformRuntimeState& state,
                              std::int32_t paused) {
  state.paused = static_cast<std::uint8_t>(paused);
}

// decomp: BATTLE.PRG func_8008D5A0 (146C.c:16857), appearance before SH
// duration.
bool FreezeMovingPlatforms(MovingPlatformRuntimeState& state,
                           std::int32_t freeze_frames,
                           MovingPlatformFreezeHost& host) {
  if (!host.HasPlatforms()) return false;
  host.SetFreezeAppearance(freeze_frames != 0);
  state.freeze_frames =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(freeze_frames));
  return true;
}

// decomp: BATTLE.PRG func_8006CE50 (146C.c:2161), original8MIPS.
bool FreezeRoomGimmicksTemporarily(MovingPlatformRuntimeState& state,
                                   MovingPlatformFreezeHost& host) {
  constexpr std::int32_t kTemporaryFreezeFrames = 180;
  return FreezeMovingPlatforms(state, kTemporaryFreezeFrames, host);
}

// decomp: BATTLE.PRG func_8008E224 (146C.c:17243), original44MIPS.
bool SetMovingPlatformVisibility(std::int32_t visibility,
                                 MovingPlatformVisibilityHost& host) {
  constexpr std::uint32_t kHiddenBit = 1;
  const bool hidden =
      ((static_cast<std::uint32_t>(visibility) - kHiddenBit) & kHiddenBit) != 0;
  if (!host.StoreHidden(hidden)) return false;
  const bool geometry = host.FindGeometryGroup();
  const auto current_hidden = host.Hidden();
  if (!current_hidden) return false;
  return !geometry || host.StoreGeometryHidden(*current_hidden);
}

// decomp: BATTLE.PRG func_8008E320 (146C.c:17273), LHU ticks only for waiting1.
std::optional<std::uint16_t> MovingPlatformDwellTicks(
    const std::optional<MovingPlatformControlState>& platform) {
  if (!platform || !platform->waiting) return std::nullopt;
  return platform->motion_ticks;
}

// decomp: BATTLE.PRG func_8008E2D4 (146C.c:17262),19MIPS selected two-bit
// point.
std::optional<MovingPlatformWaypointSelection> MovingPlatformTargetWaypoint(
    std::uint8_t current_waypoint,
    std::span<const MovingPlatformWaypoint> waypoints) {
  const auto index =
      static_cast<std::size_t>(current_waypoint & kMotionWaypointMask);
  if (index >= waypoints.size()) return std::nullopt;
  return MovingPlatformWaypointSelection{index, waypoints[index]};
}

// decomp: BATTLE.PRG func_8008C6B4 (146C.c:16528),125MIPS reset/field wrap
// order.
bool ResetMovingPlatformWaypoint(
    MovingPlatformWaypointState& state,
    std::span<const MovingPlatformWaypoint> waypoints, std::int32_t waypoint) {
  constexpr std::uint8_t kWaypointMask = 3;
  constexpr std::uint8_t kSegmentMask = 7;
  const auto selected = waypoint == -1 ? state.terminal_waypoint : waypoint;
  if (selected < 0) return false;
  const auto index = static_cast<std::size_t>(selected & kWaypointMask);
  if (index >= waypoints.size()) return false;
  const auto& point = waypoints[index];
  state.hidden = false;
  state.waiting = false;
  state.control_state = 0;
  state.start_waypoint = static_cast<std::uint8_t>(index);
  state.current_segment = static_cast<std::uint8_t>(selected & kSegmentMask);
  state.motion_ticks = 0;
  state.x = point.x;
  state.y = point.y;
  state.z = point.z;
  state.moving_forward =
      state.start_waypoint != (state.end_waypoint & kWaypointMask);
  const auto next =
      static_cast<std::int32_t>(index) + (state.moving_forward ? 1 : -1);
  state.current_waypoint = static_cast<std::uint8_t>(next & kWaypointMask);
  return true;
}

// decomp: BATTLE.PRG func_8008C8A8 (146C.c:16570), substep8008C9FC-CDAC.
bool AdvanceMovingPlatformMotion(
    MovingPlatformMotionState& state,
    std::span<const MovingPlatformWaypoint> waypoints,
    std::span<const MovingPlatformWaypoint> velocities,
    std::int32_t tick_increment, MovingPlatformMotionHost& host,
    std::size_t platform_index) {
  auto& path = state.waypoint;
  if (tick_increment <= 0 || path.current_waypoint >= waypoints.size() ||
      (!path.waiting && path.current_segment >= velocities.size())) {
    return false;
  }
  if (state.bounce_frame < kSettledBounceFrame) {
    state.bounce_frame = static_cast<std::uint8_t>(
        static_cast<std::uint32_t>(state.bounce_frame) +
        static_cast<std::uint32_t>(tick_increment));
    if (state.bounce_frame >= kBounceFrameLimit) {
      state.bounce_frame = kSettledBounceFrame;
    }
  }
  if (path.waiting) {
    ++path.motion_ticks;
    if (path.motion_ticks >= waypoints[path.current_waypoint].dwell_ticks) {
      DepartMovingPlatformWaypoint(state);
    }
    return true;
  }
  const auto velocity = velocities[path.current_segment];
  path.x = AddMotionWord(path.x, velocity.x);
  path.y = AddMotionWord(path.y, velocity.y);
  path.z = AddMotionWord(path.z, velocity.z);
  const PlatformContact candidate{
      MotionHalfword(path.x / kFixedOne), MotionHalfword(-(path.y / kFixedOne)),
      MotionHalfword(path.z / kFixedOne),
      MotionHalfword(static_cast<std::int32_t>(platform_index))};
  const auto blocked = host.IsObstructed(candidate, state);
  if (!blocked || path.current_segment >= velocities.size() ||
      path.current_waypoint >= waypoints.size()) {
    return false;
  }
  const auto current_velocity = velocities[path.current_segment];
  if (*blocked) {
    path.x = SubtractMotionWord(path.x, current_velocity.x);
    path.y = SubtractMotionWord(path.y, current_velocity.y);
    path.z = SubtractMotionWord(path.z, current_velocity.z);
    return true;
  }
  ++path.motion_ticks;
  if (path.motion_ticks >= current_velocity.dwell_ticks) {
    path.motion_ticks = 0;
    path.waiting = true;
    const auto& point = waypoints[path.current_waypoint];
    path.x = point.x;
    path.y = point.y;
    path.z = point.z;
    if (path.current_waypoint == path.terminal_waypoint) state.docked = true;
  }
  return true;
}

// decomp: BATTLE.PRG func_8008C8A8 (146C.c:16570),386MIPS frame gates/order.
bool UpdateMovingPlatforms(MovingPlatformUpdateState& state,
                           std::span<MovingPlatformMotionRecord> platforms,
                           std::span<const std::int16_t> bounce_offsets,
                           MovingPlatformUpdateHost& host) {
  constexpr std::int16_t kFreezeFlashThreshold = 30;
  constexpr std::uint8_t kDetachedLoopControl = 127;
  if (state.room_updates_paused || !state.platform_section_present ||
      state.runtime.paused != 0) {
    return true;
  }
  if (state.runtime.freeze_frames > 0) {
    state.runtime.freeze_frames = MotionHalfword(
        SubtractMotionWord(state.runtime.freeze_frames, state.tick_speed >> 1));
    if (!host.StoreRuntime(state.runtime)) return false;
    if (state.runtime.freeze_frames < kFreezeFlashThreshold &&
        state.frame_buffer_nonzero &&
        !host.SetFreezeAppearance(!state.freeze_tint_active, state)) {
      return false;
    }
    if (state.runtime.freeze_frames <= 0) {
      if (!host.SetFreezeAppearance(false, state)) return false;
      state.runtime.freeze_frames = 0;
      if (!host.StoreRuntime(state.runtime)) return false;
    }
    return true;
  }
  for (std::size_t index = 0; index < platforms.size(); ++index) {
    auto& platform = platforms[index];
    auto& motion = platform.state;
    const bool hidden = motion.waypoint.hidden;
    if (!hidden && !(motion.looping &&
                     motion.waypoint.control_state == kDetachedLoopControl)) {
      for (std::int32_t tick = 0; tick < (state.tick_speed >> 1); ++tick) {
        if (!AdvanceMovingPlatformMotion(motion, platform.waypoints,
                                         platform.velocities,
                                         state.tick_speed >> 1, host, index) ||
            !host.StorePlatform(index, motion) || !host.RefreshFrame(state)) {
          return false;
        }
      }
    }
    MovingPlatformGeometryPlacement placement{.platform_index = index,
                                              .hide = hidden};
    if (!hidden) {
      if (motion.bounce_frame > bounce_offsets.size()) return false;
      const int bounce = motion.bounce_frame == bounce_offsets.size()
                             ? 0
                             : bounce_offsets[motion.bounce_frame];
      placement.x = motion.waypoint.x / kFixedOne;
      placement.y = bounce - motion.waypoint.y / kFixedOne;
      placement.z = motion.waypoint.z / kFixedOne;
    }
    if (!host.WriteGeometry(placement)) return false;
  }
  return true;
}

// F00901/3A1A0.c:274 and func_8008D2C0/146C.c:16763 share this transform.
// Placement may precede the scene's visibility command; collision stays hidden.
std::optional<PlatformContact> MovingPlatformPlacement(
    const MovingPlatformRoom& room, int support_layer) {
  const auto index = PlatformIndex(room, support_layer);
  if (!index) {
    VS_LOG_ERROR("field.platform",
                 "Placement requires authored platform layer {}",
                 support_layer);
    return std::nullopt;
  }
  return ContactOf(room, *index);
}

// decomp: BATTLE.PRG func_800A4A24 (F00920 src/BATTLE/BATTLE.PRG/3A1A0.c:1089,
// F00938 40564.c:120). The original returned its local contact array.
std::optional<PlatformContact> FindMovingPlatformById(
    const MovingPlatformRoom& room, int support_layer) {
  const auto index = PlatformIndex(room, support_layer);
  if (!index || room.platforms[*index].hidden) return std::nullopt;
  return ContactOf(room, *index);
}
}  // namespace VS::Field
