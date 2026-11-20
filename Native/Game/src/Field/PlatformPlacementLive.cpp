// Hand-maintained (tools/port/owned.csv).
#include "Field/PlatformPlacementLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/Doors.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/Mpd.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/Room.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Display.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::RotAverage4;
using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::SetTransMatrix;
using VS::Core::Gte::SVECTOR;

namespace {
using VS::Field::kMovingPlatformCapacity;
using VS::Field::MovingPlatformPose;
using Poses = std::array<MovingPlatformPose, kMovingPlatformCapacity>;
using ShadowCorners = decltype(VS::Field::MovingPlatformShadow::vertices);
constexpr std::size_t kShadowCornerCount = std::tuple_size_v<ShadowCorners>;
constexpr std::size_t kFrozenPaletteColors = 16;
using FreezePalette = std::array<std::uint16_t, kFrozenPaletteColors>;
// decomp: D_80068BFC rodata in func_8008E4DC.s; original16halfword CLUT.
constexpr FreezePalette FrozenPaletteValues() {
  constexpr FreezePalette kPalette{
      0x0010, 0x0001, 0x0865, 0x0C66, 0x0869, 0x0CA9, 0x088D, 0x10AD,
      0x0CB1, 0x14D1, 0x0CD5, 0x14F5, 0x0CF9, 0x113A, 0x199B, 0x21FC};
  return kPalette;
}
constexpr auto kFreezePalette = FrozenPaletteValues();

// D_80068BEC holds seven offsets; frame 7 means no bounce.
constexpr std::size_t kBounceOffsetCount = 7;
static_assert(std::tuple_size_v<decltype(VS::Field::kPlatformBounceOffsets)> ==
              kBounceOffsetCount);
using WaypointTable = decltype(VS::Field::MpdMovingPlatform::waypoints);
constexpr std::size_t kWaypointCount = std::tuple_size_v<WaypointTable>;
using SegmentTable = decltype(VS::Field::MpdMovingPlatform::segment_velocities);
constexpr std::size_t kSegmentCount = std::tuple_size_v<SegmentTable>;
using Waypoints = std::array<VS::Field::MovingPlatformWaypoint, kWaypointCount>;
using Segments = std::array<VS::Field::MovingPlatformWaypoint, kSegmentCount>;

struct LivePlatformMotionTables {
  Waypoints waypoints{};
  Segments velocities{};
};

std::span<VS::Field::MpdMovingPlatform> PlatformRecords(
    std::size_t capacity = kMovingPlatformCapacity) {
  const auto bytes = VS::Field::roomData.header.moving_platforms_length;
  if (VS::Field::roomData.moving_platforms == nullptr ||
      bytes % sizeof(VS::Field::MpdMovingPlatform) != 0 ||
      bytes > capacity * sizeof(VS::Field::MpdMovingPlatform) ||
      reinterpret_cast<std::uintptr_t>(VS::Field::roomData.moving_platforms) %
              alignof(VS::Field::MpdMovingPlatform) !=
          0) {
    return {};
  }
  return {VS::Field::roomData.moving_platforms,
          bytes / sizeof(VS::Field::MpdMovingPlatform)};
}

VS::Field::MpdMovingPlatform* PlatformRecord(
    int index, std::size_t capacity = kMovingPlatformCapacity) {
  const auto records = PlatformRecords(capacity);
  return index >= 0 && static_cast<std::size_t>(index) < records.size()
             ? &records[static_cast<std::size_t>(index)]
             : nullptr;
}

class LiveFreezeHost final : public VS::Field::MovingPlatformFreezeHost {
 public:
  bool HasPlatforms() const override { return !PlatformRecords().empty(); }
  void SetFreezeAppearance(bool enabled) override {
    static_cast<void>(
        VS::Field::SetLivePlatformFreezeAppearance(static_cast<int>(enabled)));
  }
};

class LivePlatformAppearanceHost final
    : public VS::Field::MovingPlatformAppearanceHost {
 public:
  explicit LivePlatformAppearanceHost(
      std::span<VS::Field::MpdMovingPlatform> records)
      : records_(records),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}
  bool LoadPalette(bool frozen) override {
    constexpr std::size_t kOriginalPaletteOffset = 0x230;
    constexpr std::uint32_t kClutRow = 2;
    constexpr std::uint32_t kFirstColor = 0x30;
    if (!Valid()) return false;
    if (frozen) {
      return VS::Render::LoadLiveClutRow(kClutRow, kFirstColor, kFreezePalette);
    }
    if (VS::Field::roomPalettes == nullptr) return false;
    return VS::Render::LoadLiveClutRow(
        kClutRow, kFirstColor,
        std::span<const std::uint16_t>(
            VS::Field::roomPalettes + kOriginalPaletteOffset,
            kFrozenPaletteColors));
  }
  bool SwapGroupColors(std::uint8_t group) override {
    if (!Valid()) return false;
    VS::Field::LiveSwapRoomGeometryRedGreen(group);
    return Valid();
  }
  bool ToggleEffect(std::int32_t effect) override {
    if (!Valid()) return false;
    VS::Field::LiveToggleRoomEffectUpdates(effect);
    return Valid();
  }
  bool RefreshTint(VS::Field::MovingPlatformAppearanceState& state) override {
    if (!Valid()) return false;
    state.tint =
        static_cast<std::uint8_t>(VS::Field::isPlatformFreezeTintActive);
    return true;
  }
  bool StoreTint(std::uint8_t tint) override {
    if (!Valid()) return false;
    VS::Field::isPlatformFreezeTintActive = std::bit_cast<char>(tint);
    return true;
  }

 private:
  bool Valid() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           VS::Field::roomData.moving_platforms == records_.data() &&
           VS::Field::roomData.header.moving_platforms_length ==
               records_.size_bytes();
  }
  std::span<VS::Field::MpdMovingPlatform> records_;
  std::uint64_t generation_;
};

class LivePlatformVisibilityHost final
    : public VS::Field::MovingPlatformVisibilityHost {
 public:
  explicit LivePlatformVisibilityHost(VS::Field::MpdMovingPlatform& record)
      : record_(record),
        platforms_(VS::Field::roomData.moving_platforms),
        platform_bytes_(VS::Field::roomData.header.moving_platforms_length),
        geometry_(VS::Field::roomData.geometry),
        geometry_bytes_(VS::Field::roomData.header.geometry_length),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}
  bool StoreHidden(bool hidden) override {
    if (!Valid()) return false;
    record_.is_hidden = hidden;
    return true;
  }
  bool FindGeometryGroup() override {
    if (!Valid()) return false;
    group_ = record_.scene_object_id;
    return VS::Field::LiveFindRoomGeometryGroup(group_) != nullptr;
  }
  std::optional<bool> Hidden() const override {
    if (!Valid()) return std::nullopt;
    return record_.is_hidden != 0;
  }
  bool StoreGeometryHidden(bool hidden) override {
    if (!Valid()) return false;
    const auto stored = hidden ? VS::Field::LiveHideRoomGeometryGroup(group_)
                               : VS::Field::LiveShowRoomGeometryGroup(group_);
    return stored != 0 && Valid();
  }

 private:
  bool Valid() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           platforms_ == VS::Field::roomData.moving_platforms &&
           platform_bytes_ ==
               VS::Field::roomData.header.moving_platforms_length &&
           geometry_ == VS::Field::roomData.geometry &&
           geometry_bytes_ == VS::Field::roomData.header.geometry_length;
  }
  VS::Field::MpdMovingPlatform& record_;
  VS::Field::MpdMovingPlatform* platforms_;
  std::uint32_t platform_bytes_;
  decltype(VS::Field::roomData.geometry) geometry_;
  std::uint32_t geometry_bytes_;
  std::uint64_t generation_;
  int group_ = 0;
};

class LivePlatformShadowHost final
    : public VS::Field::MovingPlatformShadowHost {
 public:
  explicit LivePlatformShadowHost(
      std::span<VS::Field::MpdMovingPlatform> records)
      : records_(records) {}
  void BeginView() override {
    const auto camera = VS::Render::LiveRoomCamera();
    MATRIX matrix{};
    VS::Core::Math::StoreMatrix(matrix, camera.view.rotation);
    matrix.t[0] = camera.view.translation.x;
    matrix.t[1] = camera.view.translation.y;
    matrix.t[2] = camera.view.translation.z;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
  }
  VS::Field::MovingPlatformShadowSurface SurfaceFor(
      std::size_t index, const MovingPlatformPose& platform) override {
    constexpr int kFixedTileScale = 4096 * 128;
    constexpr int kFixedPositionScale = 4096;
    if (index >= records_.size() ||
        VS::Field::LiveFindRoomGeometryGroup(
            static_cast<int>(records_[index].scene_object_id)) == nullptr) {
      return {};
    }
    const auto* cell = VS::Field::LiveCollisionCellOrAbsent(
        platform.x / kFixedTileScale, platform.z / kFixedTileScale, 0);
    return {
        .geometry_present = true,
        .pit = cell->is_pit != 0,
        .surface_kind = static_cast<std::uint8_t>(cell->surface_kind),
        .floor_height = static_cast<std::uint16_t>(
            VS::Field::LiveFloorHeightAt(platform.x / kFixedPositionScale,
                                         platform.z / kFixedPositionScale))};
  }
  std::int32_t Project(const VS::Field::MovingPlatformShadow& shadow) override {
    std::array<SVECTOR, kShadowCornerCount> vertices{};
    for (std::size_t index = 0; index < vertices.size(); ++index) {
      const auto& vertex = shadow.vertices[index];
      vertices[index] = {vertex.x, vertex.y, vertex.z, 0};
    }
    VS::Core::Gte::Word projection = 0;
    VS::Core::Gte::Word flags = 0;
    return static_cast<std::int32_t>(RotAverage4(
        &vertices[0], &vertices[1], &vertices[2], &vertices.back(), &screen_[0],
        &screen_[1], &screen_[2], &screen_.back(), &projection, &flags));
  }
  void Draw(const VS::Field::MovingPlatformShadow& shadow,
            std::int32_t depth) override {
    constexpr std::uint32_t kShadowColorCommand = 0x2A000000;
    constexpr std::uint32_t kShadowPacketLength = 5;
    constexpr int kSubtractiveTexturePage = 0x40;
    constexpr std::size_t kShadowPacketWords = 6;
    auto* entry = VS::Render::LiveFrameOrderingEntry(
        VS::Render::LiveOrderingBase::kScene, depth);
    VS::Render::LivePacketQueue queue(entry);
    if (!queue.CanEmit(kShadowPacketWords + VS::Render::kDrawModePacketWords)) {
      return;
    }
    const auto shade = static_cast<std::uint32_t>(shadow.shade);
    const std::array<std::uint32_t, kShadowPacketWords> packet{
        (queue.ChainLink() & VS::Render::kPacketLinkMask) |
            (kShadowPacketLength << VS::Render::kPacketLengthShift),
        kShadowColorCommand | shade | (shade << 8) | (shade << 16),
        static_cast<std::uint32_t>(screen_[0]),
        static_cast<std::uint32_t>(screen_[1]),
        static_cast<std::uint32_t>(screen_[2]),
        static_cast<std::uint32_t>(screen_.back())};
    if (queue.Emit(packet)) {
      VS::Render::AppendLiveTexturePage(kSubtractiveTexturePage, depth);
    }
  }

 private:
  std::span<VS::Field::MpdMovingPlatform> records_;
  std::array<VS::Core::Gte::Word, kShadowCornerCount> screen_{};
};

VS::Field::MovingPlatformWaypointState WaypointStateOf(
    const VS::Field::MpdMovingPlatform& record) {
  return {
      .hidden = record.is_hidden != 0,
      .waiting = record.is_waiting != 0,
      .moving_forward = record.is_moving_forward != 0,
      .rider_actor = static_cast<std::uint8_t>(record.rider_actor),
      .start_waypoint = static_cast<std::uint8_t>(record.start_waypoint),
      .current_waypoint = static_cast<std::uint8_t>(record.current_waypoint),
      .current_segment = static_cast<std::uint8_t>(record.current_segment),
      .end_waypoint = static_cast<std::uint8_t>(record.end_waypoint),
      .terminal_waypoint = static_cast<std::uint8_t>(record.terminal_waypoint),
      .motion_ticks = static_cast<std::uint16_t>(record.motion_ticks),
      .x = record.position_x,
      .y = record.position_y,
      .z = record.position_z};
}

void StoreWaypointState(VS::Field::MpdMovingPlatform& record,
                        const VS::Field::MovingPlatformWaypointState& state) {
  record.is_hidden = state.hidden;
  record.is_waiting = state.waiting;
  record.is_moving_forward = state.moving_forward;
  record.rider_actor = state.rider_actor;
  record.start_waypoint = state.start_waypoint;
  record.current_waypoint = state.current_waypoint;
  record.current_segment = state.current_segment;
  record.motion_ticks = state.motion_ticks;
  record.position_x = state.x;
  record.position_y = state.y;
  record.position_z = state.z;
}

std::optional<VS::Field::MovingPlatformWaypointSelection> WaypointSelectionOf(
    const VS::Field::MpdMovingPlatform& record) {
  Waypoints points{};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const auto& point = record.waypoints[index];
    points[index] = {point.vx, point.vy, point.vz, point.pad};
  }
  return VS::Field::MovingPlatformTargetWaypoint(
      static_cast<std::uint8_t>(record.current_waypoint), points);
}

VS::Field::MovingPlatformMotionState MotionStateOf(
    const VS::Field::MpdMovingPlatform& record, std::size_t index) {
  return {.waypoint = WaypointStateOf(record),
          .bounce_frame = VS::Field::platformBounceFrames[index],
          .needs_rider = record.needs_rider != 0,
          .docked = record.tile_cell.is_platform_docked != 0};
}

class LivePlatformUpdateHost final
    : public VS::Field::MovingPlatformUpdateHost {
 public:
  explicit LivePlatformUpdateHost(
      std::span<VS::Field::MpdMovingPlatform> records)
      : records_(records),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}

  bool StoreRuntime(
      const VS::Field::MovingPlatformRuntimeState& state) override {
    if (!Valid()) return false;
    VS::Field::areMovingPlatformsPaused = state.paused;
    VS::Field::platformFreezeFrames = state.freeze_frames;
    return true;
  }

  bool RefreshFrame(VS::Field::MovingPlatformUpdateState& state) override {
    if (!Valid()) return false;
    state = {.runtime = {.paused = VS::Field::areMovingPlatformsPaused,
                         .freeze_frames = VS::Field::platformFreezeFrames},
             .tick_speed = VS::State::tickSpeed,
             .room_updates_paused = VS::Field::areRoomUpdatesPaused != 0,
             .platform_section_present =
                 VS::Field::roomData.moving_platforms != nullptr,
             .frame_buffer_nonzero = VS::Render::frameBuf != 0,
             .freeze_tint_active = VS::Field::isPlatformFreezeTintActive != 0};
    return true;
  }

  bool SetFreezeAppearance(
      bool enabled, VS::Field::MovingPlatformUpdateState& state) override {
    if (!Valid()) return false;
    return VS::Field::SetLivePlatformFreezeAppearance(
               static_cast<int>(enabled)) &&
           RefreshFrame(state);
  }

  bool StorePlatform(
      std::size_t index,
      const VS::Field::MovingPlatformMotionState& state) override {
    if (!Valid() || index >= records_.size()) return false;
    auto& record = records_[index];
    StoreWaypointState(record, state.waypoint);
    record.needs_rider = state.needs_rider;
    record.tile_cell.is_platform_docked = state.docked;
    VS::Field::platformBounceFrames[index] = state.bounce_frame;
    return true;
  }

  std::optional<bool> IsObstructed(
      const VS::Field::PlatformContact& candidate,
      VS::Field::MovingPlatformMotionState& state) override {
    if (candidate.platform_index < 0) return std::nullopt;
    const auto index = static_cast<std::size_t>(candidate.platform_index);
    if (!StorePlatform(index, state)) return std::nullopt;
    SVECTOR point{candidate.x, candidate.height, candidate.z, 0};
    const bool blocked =
        VS::Field::LiveCheckPlatformActorObstruction(
            candidate.platform_index + VS::Field::kFirstPlatformLayer,
            &point) != 0;
    if (!Valid()) return std::nullopt;
    state = MotionStateOf(records_[index], index);
    return blocked;
  }

  bool WriteGeometry(
      const VS::Field::MovingPlatformGeometryPlacement& placement) override {
    if (!Valid() || placement.platform_index >= records_.size()) return false;
    const int group =
        static_cast<int>(records_[placement.platform_index].scene_object_id);
    if (placement.hide) {
      VS::Field::LiveHideRoomGeometryGroup(group);
    } else if (VS::Field::LiveSetRoomGeometryGroupPosition(
                   group, {placement.x, placement.y, placement.z})) {
      VS::Field::LiveShowRoomGeometryGroup(group);
    }
    return true;
  }

 private:
  bool Valid() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           VS::Field::roomData.moving_platforms == records_.data() &&
           VS::Field::roomData.header.moving_platforms_length ==
               records_.size_bytes();
  }
  std::span<VS::Field::MpdMovingPlatform> records_;
  std::uint64_t generation_;
};

std::span<const MovingPlatformPose> LivePlatforms(
    std::span<MovingPlatformPose> poses) {
  const auto records = PlatformRecords(poses.size());
  for (std::size_t index = 0; index < records.size(); ++index) {
    const VS::Field::MpdMovingPlatform& platform = records[index];
    poses[index] = {.x = platform.position_x,
                    .y = platform.position_y,
                    .z = platform.position_z,
                    .bounce_frame = VS::Field::platformBounceFrames[index],
                    .hidden = platform.is_hidden != 0};
  }
  return std::span<const MovingPlatformPose>(poses).first(records.size());
}

template <typename Lookup>
std::optional<VS::Field::PlatformContact> LiveContact(int support_layer,
                                                      Lookup lookup) {
  Poses poses{};
  const VS::Field::MovingPlatformRoom room{
      .platforms = LivePlatforms(poses),
      .bounce_offsets = std::span<const std::int16_t>(
          VS::Field::kPlatformBounceOffsets.data(), kBounceOffsetCount)};
  return lookup(room, support_layer);
}
}  // namespace

namespace VS::Field {
void DrawLiveMovingPlatformShadows() {
  const auto records =
      PlatformRecords(std::size(VS::Field::platformBounceFrames));
  std::vector<MovingPlatformPose> poses(records.size());
  LivePlatformShadowHost host(records);
  DrawMovingPlatformShadows(LivePlatforms(poses), host);
}

bool SetLivePlatformFreezeAppearance(std::int32_t requested) {
  MovingPlatformAppearanceState state{
      static_cast<std::uint8_t>(VS::Field::isPlatformFreezeTintActive)};
  if (state.tint == requested) return true;
  const auto records = PlatformRecords(std::numeric_limits<std::size_t>::max() /
                                       sizeof(VS::Field::MpdMovingPlatform));
  if (VS::Field::roomData.moving_platforms != nullptr &&
      VS::Field::roomData.header.moving_platforms_length !=
          records.size_bytes()) {
    return false;
  }
  std::vector<MovingPlatformAppearanceTarget> targets;
  for (const auto& record : records) {
    targets.push_back({static_cast<std::uint8_t>(record.scene_object_id),
                       record.swaps_colors_when_frozen != 0});
  }
  std::optional<std::int32_t> effect;
  if (VS::Field::roomData.environment != nullptr) {
    constexpr std::size_t kRequiredEnvironmentBytes =
        offsetof(VS::Field::MpdEnvironment, freeze_effect_id) +
        sizeof(std::int32_t);
    if (VS::Field::roomData.header.environment_length <
            kRequiredEnvironmentBytes ||
        reinterpret_cast<std::uintptr_t>(VS::Field::roomData.environment) %
                alignof(VS::Field::MpdEnvironment) !=
            0) {
      return false;
    }
    effect = VS::Field::roomData.environment->freeze_effect_id;
  }
  LivePlatformAppearanceHost host(records);
  return SetPlatformFreezeAppearance(state, requested, targets, effect, host);
}

void InitializeLiveMovingPlatforms() {
  const auto records =
      PlatformRecords(std::size(VS::Field::platformBounceFrames));
  std::vector<MovingPlatformPose> poses(records.size());
  const auto initialization = InitializeMovingPlatforms(
      {.platforms = LivePlatforms(poses), .bounce_offsets = {}},
      std::bit_cast<std::uint16_t>(
          VS::Field::entranceDoorRecord.tile_location));
  VS::Field::areMovingPlatformsPaused = initialization.runtime.paused;
  VS::Field::platformFreezeFrames = initialization.runtime.freeze_frames;
  VS::Field::isPlatformFreezeTintActive =
      static_cast<char>(initialization.freeze_tint_active);
  std::copy(initialization.bounce_frames.begin(),
            initialization.bounce_frames.end(),
            VS::Field::platformBounceFrames.begin());
  for (const auto& placement : initialization.geometry) {
    const int group =
        static_cast<int>(records[placement.platform_index].scene_object_id);
    if (placement.hide) LiveHideRoomGeometryGroup(group);
    static_cast<void>(LiveSetRoomGeometryGroupPosition(
        group, {placement.x, placement.y, placement.z}));
  }
  if (initialization.entrance_platform) {
    static_cast<void>(
        ResetLiveMovingPlatformWaypoint(*initialization.entrance_platform, -1));
  }
}

std::vector<PlatformContact> CollectLiveMovingPlatformPositions() {
  const auto records =
      PlatformRecords(std::size(VS::Field::platformBounceFrames));
  std::vector<MovingPlatformPose> poses(records.size());
  return CollectMovingPlatformPositions(
      {.platforms = LivePlatforms(poses),
       .bounce_offsets = std::span<const std::int16_t>(
           VS::Field::kPlatformBounceOffsets.data(), kBounceOffsetCount)});
}

bool UpdateLiveMovingPlatforms() {
  const auto records =
      PlatformRecords(std::size(VS::Field::platformBounceFrames));
  if (VS::Field::roomData.moving_platforms != nullptr &&
      VS::Field::roomData.header.moving_platforms_length !=
          records.size_bytes()) {
    return false;
  }
  LivePlatformUpdateHost host(records);
  MovingPlatformUpdateState state;
  if (!host.RefreshFrame(state)) return false;
  std::vector<LivePlatformMotionTables> tables(records.size());
  std::vector<MovingPlatformMotionRecord> platforms(records.size());
  for (std::size_t index = 0; index < records.size(); ++index) {
    for (std::size_t point = 0; point < kWaypointCount; ++point) {
      const auto& input = records[index].waypoints[point];
      tables[index].waypoints[point] = {input.vx, input.vy, input.vz,
                                        input.pad};
    }
    for (std::size_t segment = 0; segment < kSegmentCount; ++segment) {
      const auto& input = records[index].segment_velocities[segment];
      tables[index].velocities[segment] = {input.vx, input.vy, input.vz,
                                           input.pad};
    }
    platforms[index] = {.state = MotionStateOf(records[index], index),
                        .waypoints = tables[index].waypoints,
                        .velocities = tables[index].velocities};
  }
  return UpdateMovingPlatforms(
      state, platforms,
      std::span<const std::int16_t>(VS::Field::kPlatformBounceOffsets.data(),
                                    kBounceOffsetCount),
      host);
}

std::optional<MovingPlatformWaypoint> LiveMovingPlatformTargetWaypoint(
    int index) {
  const auto* record =
      PlatformRecord(index, std::size(VS::Field::platformBounceFrames));
  if (record == nullptr) return std::nullopt;
  const auto selection = WaypointSelectionOf(*record);
  return selection ? std::optional<MovingPlatformWaypoint>(selection->point)
                   : std::nullopt;
}

bool SetLiveMovingPlatformActor(int platform_index, std::int32_t rider) {
  auto* record = PlatformRecord(platform_index);
  if (record == nullptr) return false;
  const auto index = static_cast<std::size_t>(platform_index);
  MovingPlatformControlState state{
      .rider_actor = static_cast<std::uint8_t>(record->rider_actor),
      .bounce_frame = VS::Field::platformBounceFrames[index],
      .waiting = record->is_waiting != 0,
      .motion_ticks = static_cast<std::uint16_t>(record->motion_ticks)};
  SetMovingPlatformActor(state, rider, VS::Field::platformFreezeFrames);
  record->rider_actor = state.rider_actor;
  VS::Field::platformBounceFrames[index] = state.bounce_frame;
  return true;
}

bool ResetLiveMovingPlatformWaypoint(int platform_index,
                                     std::int32_t waypoint) {
  auto* record = PlatformRecord(platform_index,
                                std::size(VS::Field::platformBounceFrames));
  if (record == nullptr) return false;
  std::array<MovingPlatformWaypoint, kWaypointCount> points{};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const auto& point = record->waypoints[index];
    points[index] = {point.vx, point.vy, point.vz, point.pad};
  }
  auto state = WaypointStateOf(*record);
  if (!ResetMovingPlatformWaypoint(state, points, waypoint)) return false;
  StoreWaypointState(*record, state);
  constexpr std::int32_t kFixedPositionScale = 4096;
  static_cast<void>(LiveSetRoomGeometryGroupPosition(
      record->scene_object_id,
      {state.x / kFixedPositionScale, -(state.y / kFixedPositionScale),
       state.z / kFixedPositionScale}));
  return true;
}

void SetLiveMovingPlatformsPaused(std::int32_t paused) {
  MovingPlatformRuntimeState state{
      .paused = VS::Field::areMovingPlatformsPaused,
      .freeze_frames = VS::Field::platformFreezeFrames};
  SetMovingPlatformsPaused(state, paused);
  VS::Field::areMovingPlatformsPaused = state.paused;
}

bool FreezeLiveMovingPlatforms(std::int32_t freeze_frames) {
  MovingPlatformRuntimeState state{
      .paused = VS::Field::areMovingPlatformsPaused,
      .freeze_frames = VS::Field::platformFreezeFrames};
  LiveFreezeHost host;
  if (!FreezeMovingPlatforms(state, freeze_frames, host)) return false;
  VS::Field::platformFreezeFrames = state.freeze_frames;
  return true;
}

bool FreezeLiveRoomGimmicksTemporarily() {
  MovingPlatformRuntimeState state{
      .paused = VS::Field::areMovingPlatformsPaused,
      .freeze_frames = VS::Field::platformFreezeFrames};
  LiveFreezeHost host;
  if (!FreezeRoomGimmicksTemporarily(state, host)) return false;
  VS::Field::platformFreezeFrames = state.freeze_frames;
  return true;
}

bool SetLiveMovingPlatformVisibility(int platform_index,
                                     std::int32_t visibility) {
  const auto limit = std::numeric_limits<std::uint32_t>::max() /
                     sizeof(VS::Field::MpdMovingPlatform);
  auto* record = PlatformRecord(platform_index, limit);
  if (record == nullptr) return false;
  LivePlatformVisibilityHost host(*record);
  return SetMovingPlatformVisibility(visibility, host);
}

std::optional<std::uint16_t> LiveMovingPlatformDwellTicks(int platform_index) {
  const auto* record = PlatformRecord(platform_index);
  if (record == nullptr) return std::nullopt;
  return MovingPlatformDwellTicks(MovingPlatformControlState{
      .waiting = record->is_waiting != 0,
      .motion_ticks = static_cast<std::uint16_t>(record->motion_ticks)});
}

std::optional<PlatformContact> LiveMovingPlatformPlacement(int support_layer) {
  return LiveContact(support_layer, MovingPlatformPlacement);
}

// decomp: BATTLE.PRG func_800A4A24 (src/BATTLE/BATTLE.PRG/3A1A0.c:1089).
std::optional<PlatformContact> FindLiveMovingPlatformById(int support_layer) {
  return LiveContact(support_layer, FindMovingPlatformById);
}
}  // namespace VS::Field

void VS::Field::InitializeMovingPlatformsEntry(void) {
  VS::Field::InitializeLiveMovingPlatforms();
}
void VS::Field::DrawMovingPlatformShadowsEntry(void) {
  VS::Field::DrawLiveMovingPlatformShadows();
}
void VS::Field::SetPlatformFreezeAppearanceEntry(int requested) {
  static_cast<void>(VS::Field::SetLivePlatformFreezeAppearance(requested));
}
void* VS::Field::MovingPlatformTargetWaypointEntry(int index) {
  auto* record =
      PlatformRecord(index, std::size(VS::Field::platformBounceFrames));
  if (record == nullptr) return nullptr;
  const auto selection = WaypointSelectionOf(*record);
  return selection ? &record->waypoints[selection->index] : nullptr;
}
int VS::Field::UpdateMovingPlatformsEntry(void) {
  static_cast<void>(VS::Field::UpdateLiveMovingPlatforms());
  return 0;
}
// decomp: BATTLE.PRG func_8008D2C0 (src/BATTLE/BATTLE.PRG/146C.c:16637)
// Its callers pass SVECTOR arrays: x, height, z and the platform index.
static_assert(sizeof(VS::Field::PlatformContact) == sizeof(SVECTOR));
int VS::Field::CollectMovingPlatformPositionsEntry(
    PlatformContact output_contacts[]) {
  if (output_contacts == nullptr) return 0;
  const auto contacts = VS::Field::CollectLiveMovingPlatformPositions();
  if (contacts.size() > VS::Field::kMovingPlatformCapacity) return 0;
  std::ranges::copy(contacts, output_contacts);
  return static_cast<int>(contacts.size());
}
int VS::Field::SetMovingPlatformActorEntry(int platform_index, int rider) {
  return VS::Field::SetLiveMovingPlatformActor(platform_index, rider);
}
void VS::Field::ResetMovingPlatformWaypointEntry(int platform_index,
                                                 int waypoint) {
  static_cast<void>(
      VS::Field::ResetLiveMovingPlatformWaypoint(platform_index, waypoint));
}
void VS::Field::SetMovingPlatformsPausedEntry(int paused) {
  VS::Field::SetLiveMovingPlatformsPaused(paused);
}
void VS::Field::FreezeMovingPlatformsEntry(int freeze_frames) {
  static_cast<void>(VS::Field::FreezeLiveMovingPlatforms(freeze_frames));
}
void VS::Field::FreezeRoomGimmicksTemporarilyEntry(void) {
  static_cast<void>(VS::Field::FreezeLiveRoomGimmicksTemporarily());
}
void VS::Field::SetMovingPlatformVisibilityEntry(int platform_index,
                                                 int visibility) {
  static_cast<void>(
      VS::Field::SetLiveMovingPlatformVisibility(platform_index, visibility));
}
unsigned int VS::Field::MovingPlatformDwellTicksEntry(int platform_index) {
  const auto ticks = VS::Field::LiveMovingPlatformDwellTicks(platform_index);
  return ticks ? static_cast<unsigned int>(*ticks)
               : std::numeric_limits<unsigned int>::max();
}
