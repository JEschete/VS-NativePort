// Hand-maintained (tools/port/owned.csv).
#include "Field/Environment.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
namespace {
namespace Math = Core::Math;

constexpr std::string_view kChannel = "field.environment";

// func_80090C2C: one particle per four (embers: two) 128-unit cells of the
// weather area, 4..128 of them.
constexpr std::int32_t kWeatherCellUnits = 128;
constexpr std::int32_t kCellsPerParticle = 4;
constexpr std::int32_t kCellsPerEmber = 2;
constexpr std::int32_t kMinParticles = 4;
constexpr std::int32_t kMaxParticles = 128;

// func_80090EEC: the drift intervals' frame masks, the clock offsets that
// keep the three drifts on different frames, and the 3-in-8 chance that a
// drift turns back.
constexpr std::array<std::int32_t, 4> kIntervalMasks = {0xF, 0x1F, 0x3F, 0x7F};
constexpr std::int32_t kStrengthClockOffset = 30;
constexpr std::int32_t kIntensityClockOffset = 60;
constexpr std::int32_t kRandomEighthMask = 7;
constexpr std::int32_t kTurnBackEighths = 3;
constexpr std::int32_t kAngleMask = 0xFFF;

// func_8009121C: eight particles per intensity level, at least four; the
// scale is 1.75 x (6 per level, 12..90).
constexpr std::int32_t kParticlesPerLevel = 8;
constexpr std::int32_t kMinActiveParticles = 4;
constexpr std::int32_t kScalePerLevel = 6;
constexpr std::int32_t kMinScale = 12;
constexpr std::int32_t kMaxScale = 90;
constexpr std::int32_t kSoundFadeTicks = 8;
// func_800918E8: the ducked volume is a quarter.
constexpr std::int32_t kDuckShift = 2;

// func_8009147C: the picture is 1024 pixels around in 32-pixel cells and is
// drawn while a cell's corner is within 64 pixels of the 320x240 screen.
constexpr std::int32_t kFixedOne = 4096;
constexpr std::int32_t kCellPixels = 32;
constexpr std::int32_t kPictureWidth = 1024;
constexpr std::int32_t kPictureWrapMask = kPictureWidth - 1;
constexpr std::int32_t kPictureHalfWidth = kPictureWidth / 2;
constexpr std::int32_t kScreenMargin = 64;
constexpr std::int16_t kScreenWidth = 320;
constexpr std::int16_t kScreenHeight = 240;
constexpr std::int32_t kMarginRight = kScreenWidth + kScreenMargin;
constexpr std::int32_t kMarginBottom = kScreenHeight + kScreenMargin;
constexpr std::int32_t kVisibleLeft = -kScreenMargin * kFixedOne;
constexpr std::int32_t kVisibleTop = -kScreenMargin * kFixedOne;
constexpr std::int32_t kVisibleRight = kMarginRight * kFixedOne;
constexpr std::int32_t kVisibleBottom = kMarginBottom * kFixedOne;
constexpr std::uint16_t kTileClut = 0x3BB0;
constexpr std::int32_t kSemiTransparencyShift = 7;

[[nodiscard]] std::int16_t Half(std::int32_t value) {
  return Math::WrapSigned16(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::int16_t ParticleCount(const WeatherArea& area,
                                         std::int32_t cells_per_particle) {
  const std::int32_t cells =
      (area.extent_x / kWeatherCellUnits) * (area.extent_z / kWeatherCellUnits);
  return Math::ClampSignedShort(Half(cells / cells_per_particle), kMinParticles,
                                kMaxParticles);
}

// The volume for an intensity; the original reads past its 16-entry table
// for any other value, which is refused.
[[nodiscard]] std::optional<std::int32_t> VolumeAt(std::int16_t intensity,
                                                   std::string_view entry) {
  if (intensity < 0 ||
      static_cast<std::size_t>(intensity) >= kWeatherVolumeByIntensity.size()) {
    VS_LOG_WARN(kChannel,
                "{}: weather intensity {} is outside the volume table; the "
                "ambient sound is left unchanged",
                entry, intensity);
    return std::nullopt;
  }
  return kWeatherVolumeByIntensity.at(static_cast<std::size_t>(intensity));
}

// The frame mask for a drift interval; the original reads its stack past the
// 4-entry table for any other value, which is refused.
[[nodiscard]] std::optional<std::int32_t> IntervalMask(std::int16_t interval,
                                                       std::string_view drift) {
  if (interval < 0 ||
      static_cast<std::size_t>(interval) >= kIntervalMasks.size()) {
    VS_LOG_WARN(kChannel,
                "UpdateRoomWeather: {} interval {} is outside the mask table; "
                "the {} does not drift this frame",
                drift, interval, drift);
    return std::nullopt;
  }
  return kIntervalMasks.at(static_cast<std::size_t>(interval));
}

// Whether the drift on `clock`'s frame is due, and if so, whether the step
// turns back first.
[[nodiscard]] bool DriftDue(std::int32_t clock, std::int16_t interval,
                            std::string_view drift) {
  const std::optional<std::int32_t> mask = IntervalMask(interval, drift);
  return mask && (clock & *mask) == 0;
}

void MaybeTurnBack(WeatherHost& host, std::int16_t& step) {
  if ((host.Random() & kRandomEighthMask) < kTurnBackEighths) {
    step = Half(Math::WrappingNegate(step));
  }
}

[[nodiscard]] std::int16_t ScreenAxis(std::int32_t value) {
  return Half(value / kFixedOne);
}

[[nodiscard]] PrimitiveColor ColorOf(const TileCorner& corner) {
  return {.red = corner.red, .green = corner.green, .blue = corner.blue};
}

[[nodiscard]] std::array<PrimitiveColor, kQuadCorners> ColorsOf(
    const std::array<TileCorner, kQuadCorners>& corners) {
  std::array<PrimitiveColor, kQuadCorners> colors{};
  std::ranges::transform(corners, colors.begin(), ColorOf);
  return colors;
}

[[nodiscard]] std::uint8_t Texel(std::int32_t value) {
  return static_cast<std::uint8_t>(value);
}

// The layout's cells must cover every row drawn; the original indexes past
// them otherwise, and divides by a zero width.
[[nodiscard]] bool CellsCoverRows(const BackgroundLayout& layout,
                                  std::int32_t row_end) {
  if (row_end <= 0) return true;
  if (layout.width <= 0) return false;
  const std::uint64_t rows =
      (static_cast<std::uint64_t>(row_end) + kCellPixels - 1) / kCellPixels;
  return rows * static_cast<std::uint64_t>(layout.width) <= layout.tiles.size();
}

[[nodiscard]] TexturedQuad TileQuad(const BackgroundTile& tile,
                                    std::int32_t screen_x,
                                    std::int32_t screen_y,
                                    std::int32_t cosine_step,
                                    std::int32_t sine_step,
                                    std::uint16_t base_texture_page) {
  const std::int32_t right_x = Math::WrappingAdd(screen_x, cosine_step);
  const std::int32_t right_y = Math::WrappingAdd(screen_y, sine_step);
  const TextureRect& texture = tile.texture;
  const std::int32_t far_u = texture.x + texture.width;
  const std::int32_t far_v = texture.y + texture.height;
  const auto& corners = tile.corners;
  return {
      .corners = {{{.x = ScreenAxis(screen_x), .y = ScreenAxis(screen_y)},
                   {.x = ScreenAxis(right_x), .y = ScreenAxis(right_y)},
                   {.x =
                        ScreenAxis(Math::WrappingSubtract(screen_x, sine_step)),
                    .y = ScreenAxis(Math::WrappingAdd(screen_y, cosine_step))},
                   {.x = ScreenAxis(Math::WrappingSubtract(right_x, sine_step)),
                    .y = ScreenAxis(Math::WrappingAdd(right_y, cosine_step))}}},
      .texels = {{{.u = Texel(texture.x), .v = Texel(texture.y)},
                  {.u = Texel(far_u), .v = Texel(texture.y)},
                  {.u = Texel(texture.x), .v = Texel(far_v)},
                  {.u = Texel(far_u), .v = Texel(far_v)}}},
      .colors = ColorsOf(corners),
      .texture_page = static_cast<std::uint16_t>(
          base_texture_page | (corners[1].code << kSemiTransparencyShift)),
      .clut = kTileClut};
}
}  // namespace

// decomp: BATTLE.PRG func_80090C2C (src/BATTLE/BATTLE.PRG/2842C.c:266)
void SetRoomWeather(WeatherHost& host, RoomWeatherState& state,
                    std::int32_t kind) {
  const std::optional<RoomEnvironment> environment = host.Environment();
  if (!environment) {
    if (state.ambient_sound_id >= 0) {
      host.StopSound(state.ambient_sound_id);
      state.ambient_sound_id = kNoAmbientSound;
    }
    return;
  }

  std::int32_t sound = kNoAmbientSound;
  state.direction_step = 1;
  state.strength_step = 1;
  state.intensity_step = 1;
  state.wind_strength = environment->wind_strength;
  state.wind_direction = environment->wind_direction;
  state.kind = static_cast<std::uint8_t>(kind);
  state.intensity = environment->weather_intensity;
  ApplyWeatherIntensity(host, state);
  host.RecalculateWeatherVelocity();

  const WeatherArea& area = environment->weather_area;
  switch (kind) {
    case kRainWeather:
      host.InitializeFallingParticles(ParticleCount(area, kCellsPerParticle),
                                      kind);
      sound = kRainAmbientSound;
      break;
    case kSnowWeather:
      host.InitializeFallingParticles(ParticleCount(area, kCellsPerParticle),
                                      kind);
      break;
    case kScreenWeather:
      host.InitializeScreenParticles(ParticleCount(area, kCellsPerParticle),
                                     kind);
      break;
    case kEmberWeather:
      host.InitializeRisingEmberParticles(ParticleCount(area, kCellsPerEmber),
                                          kind);
      break;
    default:
      break;
  }

  if (state.ambient_sound_id != kAmbientSoundHeld) {
    if (state.ambient_sound_id >= 0 && state.ambient_sound_id != sound) {
      host.StopSound(state.ambient_sound_id);
    }
    if (sound >= 0) host.PlaySoundCentered(sound);
    state.ambient_sound_id = Half(sound);
  }
}

// decomp: BATTLE.PRG func_80090EEC (src/BATTLE/BATTLE.PRG/2842C.c:329)
void UpdateRoomWeather(WeatherHost& host, RoomWeatherState& state) {
  const std::optional<RoomEnvironment> environment = host.Environment();
  if (!environment || state.kind == kNoWeather) return;

  if (!host.AreRoomUpdatesPaused() && state.variation_enabled != 0) {
    const std::int32_t ticks = host.RoomAnimationTicks();
    bool changed = false;
    if (DriftDue(ticks, environment->wind_direction_interval,
                 "wind direction")) {
      MaybeTurnBack(host, state.direction_step);
      const std::int32_t direction =
          (state.wind_direction + state.direction_step) & kAngleMask;
      state.wind_direction =
          Math::ClampSignedShort(Half(direction), environment->wind_direction,
                                 environment->wind_direction_limit);
      changed = true;
    }
    if (DriftDue(ticks + kStrengthClockOffset,
                 environment->wind_strength_interval, "wind strength")) {
      MaybeTurnBack(host, state.strength_step);
      state.wind_strength = Math::ClampSignedShort(
          Half(state.wind_strength + state.strength_step),
          environment->wind_strength, environment->wind_strength_limit);
      changed = true;
    }
    if (DriftDue(ticks + kIntensityClockOffset,
                 environment->weather_intensity_interval, "intensity")) {
      MaybeTurnBack(host, state.intensity_step);
      state.intensity = Math::ClampSignedShort(
          Half(state.intensity + state.intensity_step),
          environment->weather_intensity, environment->weather_intensity_limit);
      ApplyWeatherIntensity(host, state);
      changed = true;
    }
    if (changed) host.RecalculateWeatherVelocity();
  }

  switch (state.kind) {
    case kRainWeather:
      host.DrawRainParticles();
      break;
    case kSnowWeather:
      host.DrawSnowParticles();
      break;
    case kScreenWeather:
      host.DrawScreenWeatherParticles();
      break;
    case kEmberWeather:
      host.DrawRisingEmberParticles();
      break;
    default:
      break;
  }
}

// decomp: BATTLE.PRG func_8009121C (src/BATTLE/BATTLE.PRG/2842C.c:411)
void ApplyWeatherIntensity(WeatherHost& host, RoomWeatherState& state) {
  state.particle_count =
      Math::ClampSignedShort(Half(state.intensity * kParticlesPerLevel),
                             kMinActiveParticles, state.particle_capacity);
  if (state.ambient_sound_id >= 0) {
    if (const std::optional<std::int32_t> volume =
            VolumeAt(state.intensity, "ApplyWeatherIntensity")) {
      if (*volume != 0) {
        host.PlaySoundCentered(state.ambient_sound_id);
        host.FadeSoundVolume(state.ambient_sound_id, *volume, kSoundFadeTicks);
      } else {
        host.StopSound(state.ambient_sound_id);
      }
    }
  }
  const std::int16_t scale = Math::ClampSignedShort(
      Half(state.intensity * kScalePerLevel), kMinScale, kMaxScale);
  state.intensity_scale = Half(scale + (scale >> 1) + (scale >> 2));
}

// decomp: BATTLE.PRG func_80091314 (src/BATTLE/BATTLE.PRG/2842C.c:428)
void SetWeatherVariationEnabled(RoomWeatherState& state, std::int32_t enabled) {
  state.variation_enabled = static_cast<std::uint8_t>(enabled);
}

// decomp: BATTLE.PRG func_80091320 (src/BATTLE/BATTLE.PRG/2842C.c:430)
void SetWeatherIntensity(WeatherHost& host, RoomWeatherState& state,
                         std::int32_t intensity) {
  state.intensity = Half(intensity);
  ApplyWeatherIntensity(host, state);
  host.RecalculateWeatherVelocity();
}

// decomp: BATTLE.PRG func_8009134C (src/BATTLE/BATTLE.PRG/2842C.c:437)
void SetWeatherWindAngles(WeatherHost& host, RoomWeatherState& state,
                          std::int32_t azimuth, std::int32_t elevation) {
  state.wind_direction = Half(azimuth);
  state.wind_strength = Half(elevation);
  host.RecalculateWeatherVelocity();
}

// decomp: BATTLE.PRG func_80091378 (src/BATTLE/BATTLE.PRG/2842C.c:444)
void ReleaseWeatherParticles(WeatherHost& host, RoomWeatherState& state) {
  state.particle_count = 0;
  state.particle_capacity = 0;
  if (state.particles != WeatherParticlesId::kNone) {
    host.FreeWeatherParticles(state.particles);
  }
  state.particles = WeatherParticlesId::kNone;
}

// decomp: BATTLE.PRG func_800913BC (src/BATTLE/BATTLE.PRG/2842C.c:454)
std::int32_t SetWeatherAmbientSound(WeatherHost& host, RoomWeatherState& state,
                                    std::int32_t sound) {
  const std::int32_t previous = state.ambient_sound_id;
  state.ambient_sound_id = Half(sound);
  if (sound >= 0) {
    if (const std::optional<std::int32_t> volume =
            VolumeAt(state.intensity, "SetWeatherAmbientSound")) {
      if (*volume != 0) {
        host.PlaySoundCentered(Half(sound));
        host.FadeSoundVolume(state.ambient_sound_id, *volume, kSoundFadeTicks);
      } else {
        host.StopSound(Half(sound));
      }
    }
  }
  return previous;
}

// decomp: BATTLE.PRG func_80091468 (src/BATTLE/BATTLE.PRG/2842C.c:472)
void SetRoomBackgroundOffset(RoomBackgroundState& state, std::int32_t offset_x,
                             std::int32_t offset_y) {
  state.offset_x = Half(offset_x);
  state.offset_y = Half(offset_y);
}

// decomp: BATTLE.PRG func_8009147C (src/BATTLE/BATTLE.PRG/2842C.c:478)
void DrawRoomBackgroundTiles(BackgroundHost& host,
                             const BackgroundLayout& layout,
                             std::int32_t horizontal_offset,
                             std::int32_t vertical_offset,
                             std::uint16_t base_texture_page) {
  const ViewCamera camera = host.FrameCamera();
  const std::int32_t cosine = Math::Cosine(camera.angles.z);
  const std::int32_t sine = Math::Sine(camera.angles.z);
  const std::int32_t cosine_step = cosine * kCellPixels;
  const std::int32_t sine_step = sine * kCellPixels;
  const std::int32_t row_end =
      Math::WrappingMultiply(layout.height, kCellPixels);

  if (!CellsCoverRows(layout, row_end)) {
    VS_LOG_WARN(kChannel,
                "DrawRoomBackgroundTiles: a {}x{} layout has {} cells; only "
                "the backdrop fill is drawn",
                layout.width, layout.height, layout.tiles.size());
  } else {
    for (std::int32_t row = 0; row < row_end; row += kCellPixels) {
      const std::int32_t y = Math::WrappingSubtract(row, vertical_offset);
      const auto row_start = static_cast<std::size_t>(row / kCellPixels) *
                             static_cast<std::size_t>(layout.width);
      for (std::int32_t column = 0; column < kPictureWidth;
           column += kCellPixels) {
        const std::int32_t x =
            (Math::WrappingAdd(column, horizontal_offset) & kPictureWrapMask) -
            kPictureHalfWidth;
        const std::int32_t screen_x = Math::WrappingSubtract(
            Math::WrappingMultiply(cosine, x), Math::WrappingMultiply(sine, y));
        const std::int32_t screen_y = Math::WrappingAdd(
            Math::WrappingMultiply(sine, x), Math::WrappingMultiply(cosine, y));
        if (screen_x < kVisibleLeft || screen_x > kVisibleRight ||
            screen_y < kVisibleTop || screen_y > kVisibleBottom) {
          continue;
        }
        const std::size_t cell =
            row_start +
            static_cast<std::size_t>((column / kCellPixels) % layout.width);
        const BackgroundTile& tile = layout.tiles[cell];
        if (tile.texture.width == 0 && tile.texture.height == 0) continue;
        host.AddShadedTexturedQuad(TileQuad(tile, screen_x, screen_y,
                                            cosine_step, sine_step,
                                            base_texture_page));
      }
    }
  }
  host.AddFill({.color = {},
                .position = {.x = 0, .y = 0},
                .width = kScreenWidth,
                .height = kScreenHeight});
}

// decomp: BATTLE.PRG func_800918E8 (src/BATTLE/BATTLE.PRG/2842C.c:539)
void DuckWeatherAmbientSound(WeatherHost& host, const RoomWeatherState& state,
                             std::int32_t ducked) {
  if (state.ambient_sound_id < 0) return;
  const std::optional<std::int32_t> volume =
      VolumeAt(state.intensity, "DuckWeatherAmbientSound");
  if (!volume) return;
  host.FadeSoundVolume(state.ambient_sound_id,
                       ducked != 0 ? (*volume >> kDuckShift) : *volume,
                       kSoundFadeTicks);
}
}  // namespace VS::Field
