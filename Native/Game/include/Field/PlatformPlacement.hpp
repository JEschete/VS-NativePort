#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Field/PlatformLayers.hpp"

// Moving-platform contact lookup. Platforms are passed as their position,
// visibility and bounce frame, so the lookup reads no room global or PS1
// address and returns its contact by value.
namespace VS::Field {
inline constexpr int kMovingPlatformCapacity = 4;

struct MovingPlatformPose {
  // World position in 1/4096 units.
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
  // Index into the bounce offsets; the offsets' size means no bounce.
  int bounce_frame = 0;
  bool hidden = false;
};

// An actor-space platform contact: x/z and height (up is negative) in world
// units, and the platform's index in the room.
struct PlatformContact {
  std::int16_t x = 0;
  std::int16_t height = 0;
  std::int16_t z = 0;
  std::int16_t platform_index = 0;

  friend bool operator==(const PlatformContact&,
                         const PlatformContact&) = default;
};

struct MovingPlatformRoom {
  std::span<const MovingPlatformPose> platforms;
  std::span<const std::int16_t> bounce_offsets;
};

struct MovingPlatformShadowSurface {
  bool geometry_present = false;
  bool pit = false;
  std::uint8_t surface_kind = 0;
  std::uint16_t floor_height = 0;
};

struct MovingPlatformShadowVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  friend bool operator==(const MovingPlatformShadowVertex&,
                         const MovingPlatformShadowVertex&) = default;
};

inline constexpr std::size_t kPlatformShadowCornerCount = 4;

struct MovingPlatformShadow {
  std::array<MovingPlatformShadowVertex, kPlatformShadowCornerCount> vertices{};
  std::uint8_t shade = 0;
};

std::optional<MovingPlatformShadow> BuildMovingPlatformShadow(
    const MovingPlatformPose& platform,
    const MovingPlatformShadowSurface& surface);

class MovingPlatformShadowHost {
 public:
  virtual ~MovingPlatformShadowHost() = default;
  virtual void BeginView() = 0;
  virtual MovingPlatformShadowSurface SurfaceFor(
      std::size_t index, const MovingPlatformPose& platform) = 0;
  virtual std::int32_t Project(const MovingPlatformShadow& shadow) = 0;
  virtual void Draw(const MovingPlatformShadow& shadow, std::int32_t depth) = 0;
};

void DrawMovingPlatformShadows(std::span<const MovingPlatformPose> platforms,
                               MovingPlatformShadowHost& host);

struct MovingPlatformControlState {
  std::uint8_t control_state = 0;
  std::uint8_t bounce_frame = 0;
  bool waiting = false;
  std::uint16_t motion_ticks = 0;
};

struct MovingPlatformRuntimeState {
  std::uint8_t paused = 0;
  std::int16_t freeze_frames = 0;
};

struct MovingPlatformAppearanceState {
  std::uint8_t tint = 0;
};

struct MovingPlatformAppearanceTarget {
  std::uint8_t geometry_group = 0;
  bool swap_colors = false;
};

class MovingPlatformAppearanceHost {
 public:
  virtual ~MovingPlatformAppearanceHost() = default;
  virtual bool LoadPalette(bool frozen) = 0;
  virtual bool SwapGroupColors(std::uint8_t group) = 0;
  virtual bool ToggleEffect(std::int32_t effect) = 0;
  virtual bool RefreshTint(MovingPlatformAppearanceState& state) = 0;
  virtual bool StoreTint(std::uint8_t tint) = 0;
};

bool SetPlatformFreezeAppearance(
    MovingPlatformAppearanceState& state, std::int32_t requested,
    std::span<const MovingPlatformAppearanceTarget> platforms,
    std::optional<std::int32_t> effect, MovingPlatformAppearanceHost& host);

struct MovingPlatformGeometryPlacement {
  std::size_t platform_index = 0;
  bool hide = false;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
};

struct MovingPlatformInitialization {
  MovingPlatformRuntimeState runtime;
  bool freeze_tint_active = false;
  std::array<std::uint8_t, kMovingPlatformCapacity> bounce_frames{};
  std::vector<MovingPlatformGeometryPlacement> geometry;
  std::optional<int> entrance_platform;
};

MovingPlatformInitialization InitializeMovingPlatforms(
    const MovingPlatformRoom& room, std::uint16_t entrance_flags);
std::vector<PlatformContact> CollectMovingPlatformPositions(
    const MovingPlatformRoom& room);

struct MovingPlatformWaypoint {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
  std::int32_t dwell_ticks = 0;
  friend bool operator==(const MovingPlatformWaypoint&,
                         const MovingPlatformWaypoint&) = default;
};

struct MovingPlatformWaypointSelection {
  std::size_t index = 0;
  MovingPlatformWaypoint point{};
};

std::optional<MovingPlatformWaypointSelection> MovingPlatformTargetWaypoint(
    std::uint8_t current_waypoint,
    std::span<const MovingPlatformWaypoint> waypoints);

struct MovingPlatformWaypointState {
  bool hidden = false;
  bool waiting = false;
  bool moving_forward = false;
  std::uint8_t control_state = 0;
  std::uint8_t start_waypoint = 0;
  std::uint8_t current_waypoint = 0;
  std::uint8_t current_segment = 0;
  std::uint8_t end_waypoint = 0;
  std::uint8_t terminal_waypoint = 0;
  std::uint16_t motion_ticks = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
  friend bool operator==(const MovingPlatformWaypointState&,
                         const MovingPlatformWaypointState&) = default;
};

bool ResetMovingPlatformWaypoint(
    MovingPlatformWaypointState& state,
    std::span<const MovingPlatformWaypoint> waypoints, std::int32_t waypoint);

struct MovingPlatformMotionState {
  MovingPlatformWaypointState waypoint{};
  std::uint8_t bounce_frame = 0;
  bool looping = false;
  bool docked = false;
  friend bool operator==(const MovingPlatformMotionState&,
                         const MovingPlatformMotionState&) = default;
};

class MovingPlatformMotionHost {
 public:
  virtual ~MovingPlatformMotionHost() = default;
  virtual std::optional<bool> IsObstructed(
      const PlatformContact& candidate, MovingPlatformMotionState& state) = 0;
};

bool AdvanceMovingPlatformMotion(
    MovingPlatformMotionState& state,
    std::span<const MovingPlatformWaypoint> waypoints,
    std::span<const MovingPlatformWaypoint> velocities,
    std::int32_t tick_increment, MovingPlatformMotionHost& host,
    std::size_t platform_index = 0);

struct MovingPlatformUpdateState {
  MovingPlatformRuntimeState runtime{};
  std::int32_t tick_speed = 0;
  bool room_updates_paused = false;
  bool platform_section_present = false;
  bool frame_buffer_nonzero = false;
  bool freeze_tint_active = false;
};

struct MovingPlatformMotionRecord {
  MovingPlatformMotionState state{};
  std::span<const MovingPlatformWaypoint> waypoints;
  std::span<const MovingPlatformWaypoint> velocities;
};

class MovingPlatformUpdateHost : public MovingPlatformMotionHost {
 public:
  virtual bool StoreRuntime(const MovingPlatformRuntimeState& state) = 0;
  virtual bool RefreshFrame(MovingPlatformUpdateState& state) = 0;
  virtual bool SetFreezeAppearance(bool enabled,
                                   MovingPlatformUpdateState& state) = 0;
  virtual bool StorePlatform(std::size_t index,
                             const MovingPlatformMotionState& state) = 0;
  virtual bool WriteGeometry(
      const MovingPlatformGeometryPlacement& placement) = 0;
};

bool UpdateMovingPlatforms(MovingPlatformUpdateState& state,
                           std::span<MovingPlatformMotionRecord> platforms,
                           std::span<const std::int16_t> bounce_offsets,
                           MovingPlatformUpdateHost& host);

class MovingPlatformFreezeHost {
 public:
  virtual ~MovingPlatformFreezeHost() = default;
  virtual bool HasPlatforms() const = 0;
  virtual void SetFreezeAppearance(bool enabled) = 0;
};

void SetMovingPlatformActor(MovingPlatformControlState& state,
                            std::int32_t control, std::int16_t freeze_frames);
void SetMovingPlatformsPaused(MovingPlatformRuntimeState& state,
                              std::int32_t paused);
bool FreezeMovingPlatforms(MovingPlatformRuntimeState& state,
                           std::int32_t freeze_frames,
                           MovingPlatformFreezeHost& host);
bool FreezeRoomGimmicksTemporarily(MovingPlatformRuntimeState& state,
                                   MovingPlatformFreezeHost& host);
class MovingPlatformVisibilityHost {
 public:
  virtual ~MovingPlatformVisibilityHost() = default;
  virtual bool StoreHidden(bool hidden) = 0;
  virtual bool FindGeometryGroup() = 0;
  virtual std::optional<bool> Hidden() const = 0;
  virtual bool StoreGeometryHidden(bool hidden) = 0;
};

bool SetMovingPlatformVisibility(std::int32_t visibility,
                                 MovingPlatformVisibilityHost& host);
std::optional<std::uint16_t> MovingPlatformDwellTicks(
    const std::optional<MovingPlatformControlState>& platform);

// The authored contact of the platform on `support_layer`, hidden or not, for
// scripted placement. Empty, and logged, for a layer with no room platform, a
// bounce frame beyond the table or a position beyond actor coordinates.
std::optional<PlatformContact> MovingPlatformPlacement(
    const MovingPlatformRoom& room, int support_layer);

// The contact of the visible platform on `support_layer` (func_800A4A24).
// Empty when it is hidden, absent or invalid.
std::optional<PlatformContact> FindMovingPlatformById(
    const MovingPlatformRoom& room, int support_layer);
}  // namespace VS::Field
