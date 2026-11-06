// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomRendering.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/RoomEffects.hpp"
#include "Field/RoomEffectsCore.hpp"

namespace VS::Field {
namespace {
constexpr const char* kChannel = "field";
using room_effects::Fits;
using room_effects::Load;
using room_effects::Store;

// The camera's yaw in 4096ths of a turn; an eighth of a turn is 512.
constexpr std::uint32_t kAngleMask = 0xFFF;
constexpr unsigned kEighthShift = 9;
// 146C.c:16412 rounds the yaw to the view triggers' eighths.
constexpr std::uint32_t kViewDirectionBias = 0x900;
// 146C.c:15791: the facing eighth and its two neighbours.
constexpr std::array<std::uint32_t, 3> kFacingBiases{0x9C0, 0x840, 0x900};

// Field_MpdViewTrigger: a packed position word, the effect count, then one
// signed geometry group id per effect padded to four bytes.
constexpr std::size_t kViewTriggerHeaderBytes = 4;
constexpr std::size_t kViewTriggerCountOffset = 2;
constexpr std::size_t kViewTriggerIdAlignment = 4;
constexpr std::uint16_t kTriggerTileMask = 0x1F;
constexpr unsigned kTriggerTileZShift = 5;
constexpr unsigned kTriggerLayerShift = 10;
constexpr unsigned kTriggerDirectionShift = 11;
constexpr std::uint16_t kTriggerDirectionMask = 0x7;
constexpr unsigned kTriggerModeShift = 14;

// The minimap's clip area on each display buffer, its projection centre
// and its slide step (146C.c:15791).
constexpr int kDisplayWidth = 320;
constexpr std::int16_t kMinimapNearX = 216;
constexpr std::int16_t kMinimapFarX = 536;
constexpr std::int16_t kMinimapY = 156;
constexpr std::int16_t kMinimapWidth = 96;
constexpr std::int16_t kMinimapHeight = 64;
constexpr int kMinimapOffsetX = 392;
constexpr int kMinimapOffsetY = 196;
constexpr int kMinimapSlideStep = 16;
constexpr int kMinimapYawBase = 0x800;

// The frame pulse: a Q12 sine of the phase in 256ths, scaled to +-64 around
// a mid red.
constexpr unsigned kPulsePhaseShift = 8;
constexpr int kPulseDivisor = 64;
constexpr int kPulseCentre = 128;
constexpr int kBorderMidLift = 32;
constexpr int kBorderFarLift = 63;
constexpr MinimapFrameColors kCalmFrameColors{
    .border = {FrameColor{0, 128, 0}, FrameColor{0, 192, 0},
               FrameColor{0, 255, 0}, FrameColor{0, 192, 0}},
    .edge = {FrameColor{0, 192, 0}, FrameColor{0, 128, 0}},
};

struct ViewTrigger {
  int tile_x = 0;
  int tile_z = 0;
  int floor_layer = 0;
  int view_direction = 0;
  int mode = 0;
  std::size_t effect_count = 0;
};

ViewTrigger DecodeViewTrigger(std::span<const std::byte> triggers,
                              std::size_t offset) {
  const auto bits = Load<std::uint16_t>(triggers, offset);
  return ViewTrigger{.tile_x = bits & kTriggerTileMask,
                     .tile_z = (bits >> kTriggerTileZShift) & kTriggerTileMask,
                     .floor_layer = (bits >> kTriggerLayerShift) & 1,
                     .view_direction = (bits >> kTriggerDirectionShift) &
                                       kTriggerDirectionMask,
                     .mode = (bits >> kTriggerModeShift) & 1,
                     .effect_count = Load<std::uint16_t>(
                         triggers, offset + kViewTriggerCountOffset)};
}

bool Matches(const ViewTrigger& trigger, const OcclusionQuery& query,
             int view_direction) {
  return trigger.tile_x == query.tile_x && trigger.tile_z == query.tile_z &&
         trigger.floor_layer == query.floor_layer &&
         trigger.mode == query.distance_mode &&
         trigger.view_direction == view_direction;
}

// A record's bytes: its header and its ids rounded up to four bytes.
std::size_t ViewTriggerStride(std::size_t effect_count) {
  const std::size_t padded = (effect_count + kViewTriggerIdAlignment - 1) /
                             kViewTriggerIdAlignment * kViewTriggerIdAlignment;
  return kViewTriggerHeaderBytes + padded;
}

// The group records the geometry holds whole, up to its group count.
std::size_t GeometryGroupCount(std::span<const std::byte> geometry,
                               const char* walk) {
  if (!Fits(geometry.size(), 0, room_effects::kGeometryCountBytes)) {
    if (!geometry.empty()) {
      VS_LOG_WARN(kChannel, "{} refused: {} bytes of geometry hold no count",
                  walk, geometry.size());
    }
    return 0;
  }
  const auto count = Load<std::int32_t>(geometry, 0);
  if (count <= 0) {
    return 0;
  }
  const std::size_t whole =
      (geometry.size() - room_effects::kGeometryCountBytes) /
      room_effects::kGroupRecordBytes;
  const auto groups = static_cast<std::size_t>(count);
  if (groups > whole) {
    VS_LOG_WARN(kChannel,
                "{} stopped after {} of {} groups: the rest leave the {} byte "
                "geometry",
                walk, whole, groups, geometry.size());
    return whole;
  }
  return groups;
}

std::size_t GroupOffset(std::size_t group) {
  return room_effects::kGeometryCountBytes +
         (group * room_effects::kGroupRecordBytes);
}

void OffsetTextureIds(std::span<std::byte> polygons, std::size_t count,
                      std::size_t stride, int offset) {
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t at = (index * stride) + kRoomPolygonTextureIdOffset;
    const auto id = Load<std::uint16_t>(polygons, at);
    Store(polygons, at,
          static_cast<std::uint16_t>(static_cast<std::uint32_t>(id) +
                                     static_cast<std::uint32_t>(offset)));
  }
}

// 146C.c:15966 addresses the block as geometry words, so its byte offset
// drops the low two bits.
bool OffsetGroupTextureIds(std::span<std::byte> geometry, std::size_t group,
                           int offset) {
  const std::size_t word_offset =
      GroupOffset(group) +
      (room_effects::kGroupPolygonBlockWord * room_effects::kGroupWordBytes);
  const auto block_word = Load<std::int32_t>(geometry, word_offset);
  const std::int32_t block_words = block_word >> 2;
  const auto block =
      static_cast<std::size_t>(block_words) * sizeof(std::int32_t);
  if (block_words < 0 ||
      !Fits(geometry.size(), block, room_effects::kPolygonBlockCountsBytes)) {
    VS_LOG_WARN(kChannel,
                "Texture ids of group {} not offset: its polygon block at {} "
                "leaves the {} byte geometry",
                group, block_word, geometry.size());
    return false;
  }
  const auto triangles = static_cast<std::uint64_t>(
      std::max(Load<std::int32_t>(geometry, block), 0));
  const auto quads = static_cast<std::uint64_t>(
      std::max(Load<std::int32_t>(geometry, block + sizeof(std::int32_t)), 0));
  const std::size_t first = block + room_effects::kPolygonBlockCountsBytes;
  const std::uint64_t triangle_bytes =
      triangles * Render::kRoomTriangleRecordBytes;
  if (triangle_bytes + (quads * Render::kRoomQuadRecordBytes) >
      geometry.size() - first) {
    VS_LOG_WARN(kChannel,
                "Texture ids of group {} not offset: {} triangles and {} "
                "quads at {} leave the {} byte geometry",
                group, triangles, quads, first, geometry.size());
    return false;
  }
  const auto polygons = geometry.subspan(first);
  OffsetTextureIds(polygons, static_cast<std::size_t>(triangles),
                   Render::kRoomTriangleRecordBytes, offset);
  OffsetTextureIds(polygons.subspan(static_cast<std::size_t>(triangle_bytes)),
                   static_cast<std::size_t>(quads),
                   Render::kRoomQuadRecordBytes, offset);
  return true;
}

bool DisableTriggerGroups(std::span<std::byte> geometry,
                          std::span<const std::byte> triggers,
                          std::size_t offset, std::size_t effect_count) {
  const std::size_t ids = offset + kViewTriggerHeaderBytes;
  if (!Fits(triggers.size(), ids, effect_count)) {
    VS_LOG_WARN(kChannel,
                "View trigger at {} refused: its {} group ids leave the {} "
                "byte triggers",
                offset, effect_count, triggers.size());
    return false;
  }
  for (std::size_t index = 0; index < effect_count; ++index) {
    const auto group = Load<std::int8_t>(triggers, ids + index);
    DisableRoomGeometryGroup(geometry, group);
  }
  return true;
}

std::int32_t WrappingAdd(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

FrameColor Red(int shade) { return {.red = static_cast<std::uint8_t>(shade)}; }

void DrawScenePasses(RoomSceneHost& host) {
  host.LoadViewTransform();
  host.DrawTriggerMarkers();
  host.DrawActorMarkers();
  host.UpdateGimmickAnimations();
  if (!host.RoomUpdatesPaused()) {
    const int steps = host.TickSpeed() >> 1;
    for (int step = 0; step < steps; ++step) {
      host.UpdateRoomEffects();
    }
  }
  host.DrawPlatformShadows();
  host.UpdateWeather();
}

void DrawSceneMinimap(RoomSceneHost& host) {
  if (!host.RoomShown()) {
    return;
  }
  const MinimapSettings settings = host.Minimap();
  if (!settings.shown || !settings.has_scene) {
    return;
  }
  const std::int16_t slide =
      AdvanceMinimapSlide(host.MinimapSlideFrame(), host.MinimapVisible());
  host.SetMinimapSlideFrame(slide);
  if (slide <= 0) {
    return;
  }
  host.DrawMinimap(
      MinimapLayoutFor(slide, host.DisplayClipX(), host.CameraYaw()));
  if (!settings.framed) {
    return;
  }
  int phase = host.BorderPulsePhase();
  const MinimapFrameColors colors =
      AdvanceMinimapFrameColors(host.WeaponDrawn(), phase, host.TickSpeed());
  host.SetBorderPulsePhase(phase);
  host.DrawMinimapFrame(colors);
}
}  // namespace

void SetFieldRenderingEnabled(RoomRenderState& state, bool enabled) {
  state.field_rendering_enabled = enabled;
}

void SetRoomAnimationsPaused(RoomRenderState& state, bool paused) {
  state.updates_paused = paused;
}

void SetRoomTint(RoomTintState& tint, const TintColor& color, int strength) {
  if (color.red == 0 && color.green == 0 && color.blue == 0 && strength == 0) {
    tint.active = false;
    return;
  }
  tint.active = true;
  tint.parameters.strength = static_cast<std::int16_t>(strength);
  tint.parameters.red = static_cast<std::int8_t>(color.red);
  tint.parameters.green = static_cast<std::int8_t>(color.green);
  tint.parameters.blue = static_cast<std::int8_t>(color.blue);
}

void ClearRoomTint(RoomTintState& tint) { tint.active = false; }

std::uint16_t AdvanceRoomAnimationClock(bool paused, std::uint16_t ticks,
                                        int tick_speed) {
  if (paused) {
    return ticks;
  }
  return static_cast<std::uint16_t>(static_cast<std::uint32_t>(ticks) +
                                    static_cast<std::uint32_t>(tick_speed));
}

bool SetRoomRenderingEnabled(RoomDisplay& display, bool enabled) {
  if (!display.loaded) {
    return false;
  }
  display.displayed = enabled;
  return true;
}

bool CopyZonePaletteRow(std::span<std::uint16_t> palettes,
                        std::span<const std::uint16_t> colors, int row,
                        int first) {
  const std::size_t count = colors.size() - (colors.size() % 2);
  const std::int64_t start =
      (std::int64_t{row} * static_cast<std::int64_t>(kRoomPaletteRowColors)) +
      first;
  if (start < 0 || static_cast<std::uint64_t>(start) > palettes.size() ||
      count > palettes.size() - static_cast<std::size_t>(start)) {
    VS_LOG_WARN(kChannel,
                "Zone palette row {} from colour {} refused: {} colours leave "
                "the {} colour palettes",
                row, first, colors.size(), palettes.size());
    return false;
  }
  std::ranges::copy(colors.first(count),
                    palettes.subspan(static_cast<std::size_t>(start)).begin());
  return true;
}

bool UploadRoomPalettes(std::span<std::uint16_t> palettes,
                        RoomPaletteHost& host) {
  if (palettes.size() < kRoomPaletteColors) {
    VS_LOG_WARN(kChannel,
                "Room palettes not uploaded: {} colours are fewer than the "
                "room's {}",
                palettes.size(), kRoomPaletteColors);
    return false;
  }
  for (int row = 0; row < kUploadedRoomPaletteRows; ++row) {
    host.UploadClutRow(
        palettes.subspan(static_cast<std::size_t>(row) * kRoomPaletteRowColors,
                         kRoomPaletteRowColors),
        row);
  }
  host.SetGimmickPalettes(
      palettes.subspan(kGimmickPaletteFirstColor, kRoomPaletteRowColors));
  return true;
}

int OffsetRoomTextureIds(std::span<std::byte> geometry, int offset) {
  const std::size_t groups = GeometryGroupCount(geometry, "Texture id offset");
  int rewritten = 0;
  for (std::size_t group = 0; group < groups; ++group) {
    if (OffsetGroupTextureIds(geometry, group, offset)) {
      ++rewritten;
    }
  }
  return rewritten;
}

void ClearGeometryOcclusion(std::span<std::byte> geometry) {
  const std::size_t groups = GeometryGroupCount(geometry, "Occlusion clear");
  for (std::size_t group = 0; group < groups; ++group) {
    const std::size_t at = GroupOffset(group);
    const auto flags = Load<std::uint32_t>(geometry, at);
    Store(geometry, at, flags & ~room_effects::kGroupDisabledFlag);
  }
}

int CameraViewDirection(int camera_yaw) {
  return static_cast<int>(
      ((static_cast<std::uint32_t>(camera_yaw) + kViewDirectionBias) &
       kAngleMask) >>
      kEighthShift);
}

bool ApplyCameraGeometryOcclusion(std::span<std::byte> geometry,
                                  std::span<const std::byte> view_triggers,
                                  const OcclusionQuery& query) {
  if (view_triggers.empty()) {
    return false;
  }
  ClearGeometryOcclusion(geometry);
  const int direction = CameraViewDirection(query.camera_yaw);
  std::size_t offset = 0;
  while (offset < view_triggers.size()) {
    if (!Fits(view_triggers.size(), offset, kViewTriggerHeaderBytes)) {
      VS_LOG_WARN(kChannel,
                  "View triggers stopped at {}: a record header leaves the {} "
                  "byte section",
                  offset, view_triggers.size());
      return false;
    }
    const ViewTrigger trigger = DecodeViewTrigger(view_triggers, offset);
    if (Matches(trigger, query, direction)) {
      return DisableTriggerGroups(geometry, view_triggers, offset,
                                  trigger.effect_count);
    }
    offset += ViewTriggerStride(trigger.effect_count);
  }
  return false;
}

bool DrawRoomGeometry(const RoomTintState& tint, RoomGeometryHost& host) {
  if (!host.GeometryValid()) {
    VS_LOG_DEBUG("render", "Invalid room geometry extent");
    return false;
  }
  host.LoadViewTransform();
  host.UpdateGroupTransforms();
  host.DrawGroups();
  if (tint.active) {
    host.TintDrawnPrimitives(tint.parameters);
  }
  return true;
}

std::uint8_t RoomViewFacingMask(int camera_yaw) {
  std::uint32_t mask = 0;
  for (const std::uint32_t bias : kFacingBiases) {
    const std::uint32_t eighth =
        ((bias - static_cast<std::uint32_t>(camera_yaw)) & kAngleMask) >>
        kEighthShift;
    mask |= std::uint32_t{1} << eighth;
  }
  return static_cast<std::uint8_t>(mask);
}

std::int16_t AdvanceMinimapSlide(std::int16_t frame, bool visible) {
  if (visible) {
    return std::min(static_cast<std::int16_t>(frame + 1), kMinimapSlideFrames);
  }
  return std::max(static_cast<std::int16_t>(frame - 2), std::int16_t{0});
}

MinimapLayout MinimapLayoutFor(std::int16_t slide_frame, int display_clip_x,
                               int camera_yaw) {
  return MinimapLayout{
      .area = {.x = display_clip_x < kDisplayWidth ? kMinimapNearX
                                                   : kMinimapFarX,
               .y = kMinimapY,
               .width = kMinimapWidth,
               .height = kMinimapHeight},
      .offset_x = kMinimapOffsetX - (slide_frame * kMinimapSlideStep),
      .offset_y = kMinimapOffsetY,
      .map_yaw = static_cast<int>(static_cast<std::uint32_t>(kMinimapYawBase) -
                                  static_cast<std::uint32_t>(camera_yaw))};
}

MinimapFrameColors AdvanceMinimapFrameColors(bool weapon_drawn,
                                             int& pulse_phase, int tick_speed) {
  if (!weapon_drawn) {
    pulse_phase = 0;
    return kCalmFrameColors;
  }
  const auto angle = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(pulse_phase) << kPulsePhaseShift);
  const int pulse = Core::Math::Sine(angle) / kPulseDivisor;
  pulse_phase = WrappingAdd(pulse_phase, tick_speed / 2);
  const int shade = kPulseCentre - pulse;
  return MinimapFrameColors{
      .border = {Red(shade), Red(shade + kBorderMidLift),
                 Red(shade + kBorderFarLift), Red(shade + kBorderMidLift)},
      .edge = {Red(shade + kBorderMidLift), Red(shade)},
  };
}

void DrawRoomScene(RoomSceneHost& host) {
  DrawScenePasses(host);
  if (!host.RoomShown()) {
    return;
  }
  if (host.HasGeometry()) {
    host.SetViewFacing(RoomViewFacingMask(host.CameraYaw()));
    host.DrawGeometry();
  }
  DrawSceneMinimap(host);
}
}  // namespace VS::Field
