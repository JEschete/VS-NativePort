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
#include "Field/InteractionTargetLive.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/PlatformPlacementLive.h"
#include "Field/RoomEffectsLive.hpp"
#include "Legacy/Psx.h"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/UiPacketsLive.hpp"
extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomRenderingLiveState.h"
#include "Render/Display.h"
#include "State/Timers.h"
}

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
using WaypointTable = decltype(Field_MpdMovingPlatform::waypoints);
constexpr std::size_t kWaypointCount = std::extent_v<WaypointTable>;
using SegmentTable = decltype(Field_MpdMovingPlatform::segmentVelocities);
constexpr std::size_t kSegmentCount = std::extent_v<SegmentTable>;
using Waypoints = std::array<VS::Field::MovingPlatformWaypoint, kWaypointCount>;
using Segments = std::array<VS::Field::MovingPlatformWaypoint, kSegmentCount>;

struct LivePlatformMotionTables {
  Waypoints waypoints{};
  Segments velocities{};
};

std::span<Field_MpdMovingPlatform> PlatformRecords(
    std::size_t capacity = kMovingPlatformCapacity) {
  const auto bytes = Field_roomData.header.movingPlatformsLength;
  if (Field_roomData.movingPlatforms == nullptr ||
      bytes % sizeof(Field_MpdMovingPlatform) != 0 ||
      bytes > capacity * sizeof(Field_MpdMovingPlatform) ||
      reinterpret_cast<std::uintptr_t>(Field_roomData.movingPlatforms) %
              alignof(Field_MpdMovingPlatform) !=
          0) {
    return {};
  }
  return {Field_roomData.movingPlatforms,
          bytes / sizeof(Field_MpdMovingPlatform)};
}

Field_MpdMovingPlatform* PlatformRecord(
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
      std::span<Field_MpdMovingPlatform> records)
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
    if (Field_roomPalettes == nullptr) return false;
    return VS::Render::LoadLiveClutRow(
        kClutRow, kFirstColor,
        std::span<const std::uint16_t>(
            Field_roomPalettes + kOriginalPaletteOffset, kFrozenPaletteColors));
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
    state.tint = static_cast<std::uint8_t>(Field_isPlatformFreezeTintActive);
    return true;
  }
  bool StoreTint(std::uint8_t tint) override {
    if (!Valid()) return false;
    Field_isPlatformFreezeTintActive = std::bit_cast<char>(tint);
    return true;
  }

 private:
  bool Valid() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           Field_roomData.movingPlatforms == records_.data() &&
           Field_roomData.header.movingPlatformsLength == records_.size_bytes();
  }
  std::span<Field_MpdMovingPlatform> records_;
  std::uint64_t generation_;
};

class LivePlatformVisibilityHost final
    : public VS::Field::MovingPlatformVisibilityHost {
 public:
  explicit LivePlatformVisibilityHost(Field_MpdMovingPlatform& record)
      : record_(record),
        platforms_(Field_roomData.movingPlatforms),
        platform_bytes_(Field_roomData.header.movingPlatformsLength),
        geometry_(Field_roomData.geometry),
        geometry_bytes_(Field_roomData.header.geometryLength),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}
  bool StoreHidden(bool hidden) override {
    if (!Valid()) return false;
    record_.isHidden = hidden;
    return true;
  }
  bool FindGeometryGroup() override {
    if (!Valid()) return false;
    group_ = record_.sceneObjectId;
    return VS::Field::LiveFindRoomGeometryGroup(group_) != nullptr;
  }
  std::optional<bool> Hidden() const override {
    if (!Valid()) return std::nullopt;
    return record_.isHidden != 0;
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
           platforms_ == Field_roomData.movingPlatforms &&
           platform_bytes_ == Field_roomData.header.movingPlatformsLength &&
           geometry_ == Field_roomData.geometry &&
           geometry_bytes_ == Field_roomData.header.geometryLength;
  }
  Field_MpdMovingPlatform& record_;
  Field_MpdMovingPlatform* platforms_;
  std::uint32_t platform_bytes_;
  decltype(Field_roomData.geometry) geometry_;
  std::uint32_t geometry_bytes_;
  std::uint64_t generation_;
  int group_ = 0;
};

class LivePlatformShadowHost final
    : public VS::Field::MovingPlatformShadowHost {
 public:
  explicit LivePlatformShadowHost(std::span<Field_MpdMovingPlatform> records)
      : records_(records) {}
  void BeginView() override {
    const auto camera = VS::Render::LiveRoomCamera();
    MATRIX matrix{};
    VS::Core::Math::StoreMatrix(matrix, camera.view.rotation);
    matrix.t[0] = camera.view.translation.x;
    matrix.t[1] = camera.view.translation.y;
    matrix.t[2] = camera.view.translation.z;
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
  }
  VS::Field::MovingPlatformShadowSurface SurfaceFor(
      std::size_t index, const MovingPlatformPose& platform) override {
    constexpr int kFixedTileScale = 4096 * 128;
    constexpr int kFixedPositionScale = 4096;
    if (index >= records_.size() ||
        VS::Field::LiveFindRoomGeometryGroup(
            static_cast<int>(records_[index].sceneObjectId)) == nullptr) {
      return {};
    }
    const auto* cell = VS::Field::LiveCollisionCellOrAbsent(
        platform.x / kFixedTileScale, platform.z / kFixedTileScale, 0);
    return {
        .geometry_present = true,
        .pit = cell->isPit != 0,
        .surface_kind = static_cast<std::uint8_t>(cell->surfaceKind),
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
    Core_GteWord projection = 0;
    Core_GteWord flags = 0;
    return static_cast<std::int32_t>(Core_Gte_RotAverage4(
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
  std::span<Field_MpdMovingPlatform> records_;
  std::array<Core_GteWord, kShadowCornerCount> screen_{};
};

VS::Field::MovingPlatformWaypointState WaypointStateOf(
    const Field_MpdMovingPlatform& record) {
  return {
      .hidden = record.isHidden != 0,
      .waiting = record.isWaiting != 0,
      .moving_forward = record.isMovingForward != 0,
      .control_state = static_cast<std::uint8_t>(record.controlState),
      .start_waypoint = static_cast<std::uint8_t>(record.startWaypoint),
      .current_waypoint = static_cast<std::uint8_t>(record.currentWaypoint),
      .current_segment = static_cast<std::uint8_t>(record.currentSegment),
      .end_waypoint = static_cast<std::uint8_t>(record.endWaypoint),
      .terminal_waypoint = static_cast<std::uint8_t>(record.terminalWaypoint),
      .motion_ticks = static_cast<std::uint16_t>(record.motionTicks),
      .x = record.positionX,
      .y = record.positionY,
      .z = record.positionZ};
}

void StoreWaypointState(Field_MpdMovingPlatform& record,
                        const VS::Field::MovingPlatformWaypointState& state) {
  record.isHidden = state.hidden;
  record.isWaiting = state.waiting;
  record.isMovingForward = state.moving_forward;
  record.controlState = state.control_state;
  record.startWaypoint = state.start_waypoint;
  record.currentWaypoint = state.current_waypoint;
  record.currentSegment = state.current_segment;
  record.motionTicks = state.motion_ticks;
  record.positionX = state.x;
  record.positionY = state.y;
  record.positionZ = state.z;
}

std::optional<VS::Field::MovingPlatformWaypointSelection> WaypointSelectionOf(
    const Field_MpdMovingPlatform& record) {
  Waypoints points{};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const auto& point = record.waypoints[index];
    points[index] = {point.vx, point.vy, point.vz, point.pad};
  }
  return VS::Field::MovingPlatformTargetWaypoint(
      static_cast<std::uint8_t>(record.currentWaypoint), points);
}

VS::Field::MovingPlatformMotionState MotionStateOf(
    const Field_MpdMovingPlatform& record, std::size_t index) {
  return {.waypoint = WaypointStateOf(record),
          .bounce_frame = Field_platformBounceFrames[index],
          .looping = record.isLooping != 0,
          .docked = record.tileCell.isPlatformDocked != 0};
}

class LivePlatformUpdateHost final
    : public VS::Field::MovingPlatformUpdateHost {
 public:
  explicit LivePlatformUpdateHost(std::span<Field_MpdMovingPlatform> records)
      : records_(records),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}

  bool StoreRuntime(
      const VS::Field::MovingPlatformRuntimeState& state) override {
    if (!Valid()) return false;
    Field_areMovingPlatformsPaused = state.paused;
    Field_platformFreezeFrames = state.freeze_frames;
    return true;
  }

  bool RefreshFrame(VS::Field::MovingPlatformUpdateState& state) override {
    if (!Valid()) return false;
    state = {
        .runtime = {.paused = Field_areMovingPlatformsPaused,
                    .freeze_frames = Field_platformFreezeFrames},
        .tick_speed = State_tickSpeed,
        .room_updates_paused = Field_areRoomUpdatesPaused != 0,
        .platform_section_present = Field_roomData.movingPlatforms != nullptr,
        .frame_buffer_nonzero = Render_frameBuf != 0,
        .freeze_tint_active = Field_isPlatformFreezeTintActive != 0};
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
    record.isLooping = state.looping;
    record.tileCell.isPlatformDocked = state.docked;
    Field_platformBounceFrames[index] = state.bounce_frame;
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
        static_cast<int>(records_[placement.platform_index].sceneObjectId);
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
           Field_roomData.movingPlatforms == records_.data() &&
           Field_roomData.header.movingPlatformsLength == records_.size_bytes();
  }
  std::span<Field_MpdMovingPlatform> records_;
  std::uint64_t generation_;
};

std::span<const MovingPlatformPose> LivePlatforms(
    std::span<MovingPlatformPose> poses) {
  const auto records = PlatformRecords(poses.size());
  for (std::size_t index = 0; index < records.size(); ++index) {
    const Field_MpdMovingPlatform& platform = records[index];
    poses[index] = {.x = platform.positionX,
                    .y = platform.positionY,
                    .z = platform.positionZ,
                    .bounce_frame = Field_platformBounceFrames[index],
                    .hidden = platform.isHidden != 0};
  }
  return std::span<const MovingPlatformPose>(poses).first(records.size());
}

template <typename Lookup>
std::optional<Field_PlatformContactPoint> LiveContact(int support_layer,
                                                      Lookup lookup) {
  Poses poses{};
  const VS::Field::MovingPlatformRoom room{
      .platforms = LivePlatforms(poses),
      .bounce_offsets = std::span<const std::int16_t>(
          Field_kPlatformBounceOffsets, kBounceOffsetCount)};
  const auto contact = lookup(room, support_layer);
  if (!contact) return std::nullopt;
  return Field_PlatformContactPoint{contact->x, contact->height, contact->z,
                                    contact->platform_index};
}
}  // namespace

namespace VS::Field {
void DrawLiveMovingPlatformShadows() {
  const auto records = PlatformRecords(std::size(Field_platformBounceFrames));
  std::vector<MovingPlatformPose> poses(records.size());
  LivePlatformShadowHost host(records);
  DrawMovingPlatformShadows(LivePlatforms(poses), host);
}

bool SetLivePlatformFreezeAppearance(std::int32_t requested) {
  MovingPlatformAppearanceState state{
      static_cast<std::uint8_t>(Field_isPlatformFreezeTintActive)};
  if (state.tint == requested) return true;
  const auto records = PlatformRecords(std::numeric_limits<std::size_t>::max() /
                                       sizeof(Field_MpdMovingPlatform));
  if (Field_roomData.movingPlatforms != nullptr &&
      Field_roomData.header.movingPlatformsLength != records.size_bytes()) {
    return false;
  }
  std::vector<MovingPlatformAppearanceTarget> targets;
  for (const auto& record : records) {
    targets.push_back({static_cast<std::uint8_t>(record.sceneObjectId),
                       record.swapsColorsWhenFrozen != 0});
  }
  std::optional<std::int32_t> effect;
  if (Field_roomData.environment != nullptr) {
    constexpr std::size_t kRequiredEnvironmentBytes =
        offsetof(Field_MpdEnvironment, freezeEffectId) + sizeof(std::int32_t);
    if (Field_roomData.header.environmentLength < kRequiredEnvironmentBytes ||
        reinterpret_cast<std::uintptr_t>(Field_roomData.environment) %
                alignof(Field_MpdEnvironment) !=
            0) {
      return false;
    }
    effect = Field_roomData.environment->freezeEffectId;
  }
  LivePlatformAppearanceHost host(records);
  return SetPlatformFreezeAppearance(state, requested, targets, effect, host);
}

void InitializeLiveMovingPlatforms() {
  const auto records = PlatformRecords(std::size(Field_platformBounceFrames));
  std::vector<MovingPlatformPose> poses(records.size());
  const auto initialization = InitializeMovingPlatforms(
      {.platforms = LivePlatforms(poses), .bounce_offsets = {}},
      Field_entranceDoorPlatformFlags);
  Field_areMovingPlatformsPaused = initialization.runtime.paused;
  Field_platformFreezeFrames = initialization.runtime.freeze_frames;
  Field_isPlatformFreezeTintActive =
      static_cast<char>(initialization.freeze_tint_active);
  std::copy(initialization.bounce_frames.begin(),
            initialization.bounce_frames.end(), Field_platformBounceFrames);
  for (const auto& placement : initialization.geometry) {
    const int group =
        static_cast<int>(records[placement.platform_index].sceneObjectId);
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
  const auto records = PlatformRecords(std::size(Field_platformBounceFrames));
  std::vector<MovingPlatformPose> poses(records.size());
  return CollectMovingPlatformPositions(
      {.platforms = LivePlatforms(poses),
       .bounce_offsets = std::span<const std::int16_t>(
           Field_kPlatformBounceOffsets, kBounceOffsetCount)});
}

bool UpdateLiveMovingPlatforms() {
  const auto records = PlatformRecords(std::size(Field_platformBounceFrames));
  if (Field_roomData.movingPlatforms != nullptr &&
      Field_roomData.header.movingPlatformsLength != records.size_bytes()) {
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
      const auto& input = records[index].segmentVelocities[segment];
      tables[index].velocities[segment] = {input.vx, input.vy, input.vz,
                                           input.pad};
    }
    platforms[index] = {.state = MotionStateOf(records[index], index),
                        .waypoints = tables[index].waypoints,
                        .velocities = tables[index].velocities};
  }
  return UpdateMovingPlatforms(
      state, platforms,
      std::span<const std::int16_t>(Field_kPlatformBounceOffsets,
                                    kBounceOffsetCount),
      host);
}

std::optional<MovingPlatformWaypoint> LiveMovingPlatformTargetWaypoint(
    int index) {
  const auto* record =
      PlatformRecord(index, std::size(Field_platformBounceFrames));
  if (record == nullptr) return std::nullopt;
  const auto selection = WaypointSelectionOf(*record);
  return selection ? std::optional<MovingPlatformWaypoint>(selection->point)
                   : std::nullopt;
}

bool SetLiveMovingPlatformActor(int platform_index, std::int32_t control) {
  auto* record = PlatformRecord(platform_index);
  if (record == nullptr) return false;
  const auto index = static_cast<std::size_t>(platform_index);
  MovingPlatformControlState state{
      .control_state = static_cast<std::uint8_t>(record->controlState),
      .bounce_frame = Field_platformBounceFrames[index],
      .waiting = record->isWaiting != 0,
      .motion_ticks = static_cast<std::uint16_t>(record->motionTicks)};
  SetMovingPlatformActor(state, control, Field_platformFreezeFrames);
  record->controlState = state.control_state;
  Field_platformBounceFrames[index] = state.bounce_frame;
  return true;
}

bool ResetLiveMovingPlatformWaypoint(int platform_index,
                                     std::int32_t waypoint) {
  auto* record =
      PlatformRecord(platform_index, std::size(Field_platformBounceFrames));
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
      record->sceneObjectId,
      {state.x / kFixedPositionScale, -(state.y / kFixedPositionScale),
       state.z / kFixedPositionScale}));
  return true;
}

void SetLiveMovingPlatformsPaused(std::int32_t paused) {
  MovingPlatformRuntimeState state{.paused = Field_areMovingPlatformsPaused,
                                   .freeze_frames = Field_platformFreezeFrames};
  SetMovingPlatformsPaused(state, paused);
  Field_areMovingPlatformsPaused = state.paused;
}

bool FreezeLiveMovingPlatforms(std::int32_t freeze_frames) {
  MovingPlatformRuntimeState state{.paused = Field_areMovingPlatformsPaused,
                                   .freeze_frames = Field_platformFreezeFrames};
  LiveFreezeHost host;
  if (!FreezeMovingPlatforms(state, freeze_frames, host)) return false;
  Field_platformFreezeFrames = state.freeze_frames;
  return true;
}

bool FreezeLiveRoomGimmicksTemporarily() {
  MovingPlatformRuntimeState state{.paused = Field_areMovingPlatformsPaused,
                                   .freeze_frames = Field_platformFreezeFrames};
  LiveFreezeHost host;
  if (!FreezeRoomGimmicksTemporarily(state, host)) return false;
  Field_platformFreezeFrames = state.freeze_frames;
  return true;
}

bool SetLiveMovingPlatformVisibility(int platform_index,
                                     std::int32_t visibility) {
  const auto limit = std::numeric_limits<std::uint32_t>::max() /
                     sizeof(Field_MpdMovingPlatform);
  auto* record = PlatformRecord(platform_index, limit);
  if (record == nullptr) return false;
  LivePlatformVisibilityHost host(*record);
  return SetMovingPlatformVisibility(visibility, host);
}

std::optional<std::uint16_t> LiveMovingPlatformDwellTicks(int platform_index) {
  const auto* record = PlatformRecord(platform_index);
  if (record == nullptr) return std::nullopt;
  return MovingPlatformDwellTicks(MovingPlatformControlState{
      .waiting = record->isWaiting != 0,
      .motion_ticks = static_cast<std::uint16_t>(record->motionTicks)});
}

std::optional<Field_PlatformContactPoint> LiveMovingPlatformPlacement(
    int support_layer) {
  return LiveContact(support_layer, MovingPlatformPlacement);
}

// decomp: BATTLE.PRG func_800A4A24 (src/BATTLE/BATTLE.PRG/3A1A0.c:1089).
std::optional<Field_PlatformContactPoint> FindLiveMovingPlatformById(
    int support_layer) {
  return LiveContact(support_layer, FindMovingPlatformById);
}
}  // namespace VS::Field

void Field_InitializeMovingPlatformsEntry(void) {
  VS::Field::InitializeLiveMovingPlatforms();
}
void Field_DrawMovingPlatformShadowsEntry(void) {
  VS::Field::DrawLiveMovingPlatformShadows();
}
void Field_SetPlatformFreezeAppearanceEntry(int requested) {
  static_cast<void>(VS::Field::SetLivePlatformFreezeAppearance(requested));
}
void* Field_MovingPlatformTargetWaypointEntry(int index) {
  auto* record = PlatformRecord(index, std::size(Field_platformBounceFrames));
  if (record == nullptr) return nullptr;
  const auto selection = WaypointSelectionOf(*record);
  return selection ? &record->waypoints[selection->index] : nullptr;
}
int Field_UpdateMovingPlatformsEntry(void) {
  static_cast<void>(VS::Field::UpdateLiveMovingPlatforms());
  return 0;
}
int Field_CollectMovingPlatformPositionsEntry(
    Field_PlatformContactPoint output_contacts[]) {
  if (output_contacts == nullptr) return 0;
  const auto contacts = VS::Field::CollectLiveMovingPlatformPositions();
  if (contacts.size() > VS::Field::kMovingPlatformCapacity) return 0;
  for (std::size_t index = 0; index < contacts.size(); ++index) {
    const auto& contact = contacts[index];
    output_contacts[index] = {contact.x, contact.height, contact.z,
                              contact.platform_index};
  }
  return static_cast<int>(contacts.size());
}
int Field_SetMovingPlatformActorEntry(int platform_index, int control) {
  return VS::Field::SetLiveMovingPlatformActor(platform_index, control);
}
void Field_ResetMovingPlatformWaypointEntry(int platform_index, int waypoint) {
  static_cast<void>(
      VS::Field::ResetLiveMovingPlatformWaypoint(platform_index, waypoint));
}
void Field_SetMovingPlatformsPausedEntry(int paused) {
  VS::Field::SetLiveMovingPlatformsPaused(paused);
}
void Field_FreezeMovingPlatformsEntry(int freeze_frames) {
  static_cast<void>(VS::Field::FreezeLiveMovingPlatforms(freeze_frames));
}
void Field_FreezeRoomGimmicksTemporarilyEntry(void) {
  static_cast<void>(VS::Field::FreezeLiveRoomGimmicksTemporarily());
}
void Field_SetMovingPlatformVisibilityEntry(int platform_index,
                                            int visibility) {
  static_cast<void>(
      VS::Field::SetLiveMovingPlatformVisibility(platform_index, visibility));
}
unsigned int Field_MovingPlatformDwellTicksEntry(int platform_index) {
  const auto ticks = VS::Field::LiveMovingPlatformDwellTicks(platform_index);
  return ticks ? static_cast<unsigned int>(*ticks)
               : std::numeric_limits<unsigned int>::max();
}
