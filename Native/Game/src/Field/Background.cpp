// Hand-maintained (tools/port/owned.csv).
#include "Field/Background.hpp"  // NOLINT(build/include): typed API

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/Environment.hpp"

namespace VS::Field {
namespace {
namespace Math = Core::Math;

constexpr std::string_view kChannel = "field.background";

// ONE: a world unit and a full parallax scale in Q12.
constexpr std::int32_t kFixedOne = 4096;

// func_8008EB30: the dome's colour words are gouraud colours; the zenith, the
// upper ring of 8, the middle ring of 16 and the lower ring of 8 take the
// first four gradient colours and the horizon vertex the fifth.
constexpr std::uint32_t kColorMask = 0xFFFFFF;
constexpr std::uint32_t kSkyDomeColorCode = 0x30000000;
constexpr std::size_t kUpperRingEnd = 9;
constexpr std::size_t kMiddleRingEnd = 25;
constexpr std::size_t kLowerRingEnd = 33;
constexpr std::size_t kUpperBand = 1;
constexpr std::size_t kMiddleBand = 2;
constexpr std::size_t kLowerBand = 3;
constexpr std::size_t kHorizonBand = 4;

// func_8008E938: the weather area is in 128-unit cells, four to a collision
// tile; a room without a collision map gets 32 cells.
constexpr std::int32_t kWeatherCellUnits = 128;
constexpr std::int32_t kWeatherCellsPerTile = 4;
constexpr std::int16_t kDefaultWeatherCells = 0x20;

// func_8008EC48: tiled layouts are 1..31 cells on a side.
constexpr std::uint32_t kLayoutSideLimit = 31;
// The solid backdrop: a full-screen quad over texels 248..249 of texture page
// 0x11 with CLUT 0x38B1, grey or tinted by the room tint.
constexpr std::int16_t kScreenWidth = 320;
constexpr std::int16_t kScreenHeight = 240;
constexpr std::uint8_t kBackdropTexel = 248;
constexpr std::int32_t kNeutralTint = 128;
constexpr std::int32_t kTintScale = 4;
constexpr std::int32_t kMaxChannel = 255;
constexpr std::uint16_t kBackdropTexturePage = 0x11;
constexpr std::uint16_t kBackdropClut = 0x38B1;

// func_8008F0FC: where the image pixels go: the low 64 columns of the
// reserved page, in the page's row of the texture area, and its CLUT in row
// 14 of the CLUT strip.
constexpr std::int32_t kPageColumnMask = 0x3F;
constexpr std::int32_t kPageXMask = 0xF;
constexpr std::int32_t kPageWidthShift = 6;
constexpr std::int32_t kPageRowBit = 0x10;
constexpr std::int32_t kPageRowShift = 4;
constexpr int kBackgroundClutRow = 0xE;
constexpr int kBackgroundClutColors = 0x100;

[[nodiscard]] std::uint32_t SkyDomeColor(std::int32_t color) {
  return (static_cast<std::uint32_t>(color) & kColorMask) | kSkyDomeColorCode;
}

[[nodiscard]] bool IsSingleColor(const GradientColors& colors) {
  for (std::size_t i = 1; i < colors.size(); ++i) {
    if (colors.at(i) != colors.front()) return false;
  }
  return true;
}

// A halfword times `factor`, stored back as a halfword.
[[nodiscard]] std::int16_t ScaledHalf(std::int32_t value, std::int32_t factor) {
  return Math::WrapSigned16(
      static_cast<std::uint32_t>(Math::WrappingMultiply(value, factor)));
}

[[nodiscard]] std::uint8_t TintChannel(std::int8_t tint) {
  const std::int32_t value = kNeutralTint + (tint * kTintScale);
  return static_cast<std::uint8_t>(
      Math::ClampSignedShort(static_cast<std::int16_t>(value), 0, kMaxChannel));
}

// The dome's position: the camera's whole units scaled by the zoomed far
// clip.
[[nodiscard]] std::int32_t DomeAxis(std::int32_t position,
                                    std::int32_t far_clip) {
  return Math::WrappingMultiply(position / kFixedOne, far_clip) / kFixedOne;
}

[[nodiscard]] bool IsValidBackgroundTexture(
    const BackgroundTextureUpload& image) {
  const TextureRect& rect = image.source_rect;
  return rect.width > 0 && rect.height > 0 &&
         image.pixel_words.size() == (static_cast<std::size_t>(rect.width) *
                                      static_cast<std::size_t>(rect.height)) &&
         image.palette_words.size() == kBackgroundClutColors;
}

void DrawSolidBackdrop(BackgroundHost& host) {
  PrimitiveColor color{.red = static_cast<std::uint8_t>(kNeutralTint),
                       .green = static_cast<std::uint8_t>(kNeutralTint),
                       .blue = static_cast<std::uint8_t>(kNeutralTint)};
  const RoomTint tint = host.ActiveRoomTint();
  if (tint.is_active != 0) {
    color = {.red = TintChannel(tint.red),
             .green = TintChannel(tint.green),
             .blue = TintChannel(tint.blue)};
  }
  constexpr std::uint8_t kFarTexel = kBackdropTexel + 1;
  TexturedQuad quad{.corners = {{{.x = 0, .y = 0},
                                 {.x = kScreenWidth, .y = 0},
                                 {.x = 0, .y = kScreenHeight},
                                 {.x = kScreenWidth, .y = kScreenHeight}}},
                    .texels = {{{.u = kBackdropTexel, .v = kBackdropTexel},
                                {.u = kFarTexel, .v = kBackdropTexel},
                                {.u = kBackdropTexel, .v = kFarTexel},
                                {.u = kFarTexel, .v = kFarTexel}}},
                    .colors = {},
                    .texture_page = kBackdropTexturePage,
                    .clut = kBackdropClut};
  quad.colors.front() = color;
  host.AddFlatTexturedQuad(quad);
}

void DrawSkyDome(BackgroundHost& host,
                 const std::optional<CameraVector>& rotation_override) {
  CameraVector angles;
  if (rotation_override) {
    angles = *rotation_override;
  } else if (const std::optional<RoomEnvironment> environment =
                 host.Environment()) {
    angles = environment->background_rotation;
  } else {
    angles = host.ReferenceRotation();
  }
  const ViewCamera camera = host.FrameCamera();
  const CameraVector position{
      .x = DomeAxis(camera.position.x, camera.far_clip),
      .y = DomeAxis(camera.position.y, camera.far_clip),
      .z = DomeAxis(camera.position.z, camera.far_clip)};
  host.DrawSkyDome(position, angles, camera.far_clip != kFixedOne);
}
}  // namespace

// decomp: BATTLE.PRG func_8008E88C (src/BATTLE/BATTLE.PRG/146C.c:17310)
void InitializeRoomBackground(BackgroundHost& host, RoomBackgroundState& state,
                              RoomWeatherState& weather) {
  ReleaseTiledBackground(host, state);
  state.texture_uploaded = 0;
  // The particle buffer is dropped without being freed, as in the original.
  weather.particles = WeatherParticlesId::kNone;
  state.tiled_enabled = 0;
  weather.ambient_sound_id = kNoAmbientSound;
  weather.kind = kNoWeather;
  weather.variation_enabled = 1;
  InitializeRoomBackgroundColors(host, state);
}

// decomp: BATTLE.PRG func_8008E938 (src/BATTLE/BATTLE.PRG/146C.c:17332)
void InitializeRoomBackgroundColors(BackgroundHost& host,
                                    RoomBackgroundState& state) {
  state.offset_x = 0;
  state.offset_y = 0;
  state.parallax_x = kUnitParallax;
  state.parallax_y = kUnitParallax;

  const std::optional<RoomEnvironment> environment = host.Environment();
  if (!environment) return;

  const GradientColors& colors = environment->background_colors;
  if (IsSingleColor(colors) &&
      (static_cast<std::uint32_t>(colors.front()) & kColorMask) != 0) {
    state.solid = 1;
  } else {
    state.solid = 0;
    SetRoomBackgroundColors(host, state, colors);
  }

  WeatherArea area = environment->weather_area;
  if (area == WeatherArea{}) {
    if (const std::optional<CollisionMapExtent> map = host.CollisionMapSize()) {
      area.extent_x = ScaledHalf(map->width, kWeatherCellsPerTile);
      area.extent_z = ScaledHalf(map->height, kWeatherCellsPerTile);
    } else {
      area.extent_x = kDefaultWeatherCells;
      area.extent_z = kDefaultWeatherCells;
    }
  }
  area.origin_x = ScaledHalf(area.origin_x, kWeatherCellUnits);
  area.origin_z = ScaledHalf(area.origin_z, kWeatherCellUnits);
  area.extent_x = ScaledHalf(area.extent_x, kWeatherCellUnits);
  area.extent_z = ScaledHalf(area.extent_z, kWeatherCellUnits);
  area.extent_x = Math::WrapSigned16(static_cast<std::uint32_t>(
      Math::WrappingSubtract(area.extent_x, area.origin_x)));
  area.extent_z = Math::WrapSigned16(static_cast<std::uint32_t>(
      Math::WrappingSubtract(area.extent_z, area.origin_z)));
  host.StoreWeatherArea(area);
}

// decomp: BATTLE.PRG func_8008EAE4 (src/BATTLE/BATTLE.PRG/146C.c:17394)
void SetBackgroundParallaxScale(RoomBackgroundState& state,
                                std::int32_t parallax_x,
                                std::int32_t parallax_y) {
  state.parallax_x = Math::WrapSigned16(static_cast<std::uint32_t>(parallax_x));
  state.parallax_y = Math::WrapSigned16(static_cast<std::uint32_t>(parallax_y));
}

// decomp: BATTLE.PRG func_8008EAF8 (src/BATTLE/BATTLE.PRG/146C.c:17400)
void SetTiledBackgroundEnabled(RoomBackgroundState& state,
                               std::int32_t enabled) {
  state.tiled_enabled = static_cast<std::uint8_t>(enabled);
}

// decomp: BATTLE.PRG func_8008EB24 (src/BATTLE/BATTLE.PRG/146C.c:17408)
const GradientColors& RoomBackgroundColors(const RoomBackgroundState& state) {
  return state.gradient_colors;
}

// decomp: BATTLE.PRG func_8008EB30 (src/BATTLE/BATTLE.PRG/146C.c:17410)
void SetRoomBackgroundColors(BackgroundHost& host, RoomBackgroundState& state,
                             const GradientColors& colors) {
  const GradientColors source = colors;
  state.gradient_colors = source;
  SkyDomeColors dome{};
  for (std::size_t vertex = 0; vertex < dome.size(); ++vertex) {
    std::size_t band = 0;
    if (vertex >= kLowerRingEnd) {
      band = kHorizonBand;
    } else if (vertex >= kMiddleRingEnd) {
      band = kLowerBand;
    } else if (vertex >= kUpperRingEnd) {
      band = kMiddleBand;
    } else if (vertex >= 1) {
      band = kUpperBand;
    }
    dome.at(vertex) = SkyDomeColor(source.at(band));
  }
  host.StoreSkyDomeColors(dome);
}

// decomp: BATTLE.PRG func_8008EC48 (src/BATTLE/BATTLE.PRG/146C.c:17448)
void DrawRoomBackground(BackgroundHost& host, const RoomBackgroundState& state,
                        const std::optional<CameraVector>& rotation_override) {
  if (state.tiled_enabled != 0 && state.layout) {
    const auto& layout = state.layout;
    // Out-of-range sizes draw nothing at all, as in the original.
    if (static_cast<std::uint32_t>(Math::WrappingSubtract(layout->width, 1)) <
            kLayoutSideLimit &&
        static_cast<std::uint32_t>(Math::WrappingSubtract(layout->height, 1)) <
            kLayoutSideLimit) {
      const ViewCamera camera = host.FrameCamera();
      const std::int32_t horizontal = Math::WrappingSubtract(
          Math::WrappingMultiply(camera.angles.y, state.parallax_x) / kFixedOne,
          state.offset_x);
      const std::int32_t vertical = Math::WrappingSubtract(
          state.offset_y,
          Math::WrappingMultiply(camera.angles.x, state.parallax_y) /
              kFixedOne);
      DrawRoomBackgroundTiles(host,
                              {.width = layout->width,
                               .height = layout->height,
                               .tiles = layout->tiles},
                              horizontal, vertical, state.texture_page);
    }
    return;
  }
  if (state.solid != 0) {
    DrawSolidBackdrop(host);
    return;
  }
  DrawSkyDome(host, rotation_override);
}

// decomp: BATTLE.PRG func_8008EFCC (src/BATTLE/BATTLE.PRG/146C.c:17515)
std::int32_t QueueTiledBackgroundLoad(BackgroundHost& host,
                                      RoomBackgroundState& state,
                                      std::int32_t texture_page) {
  const std::uint8_t was_enabled = state.tiled_enabled;
  ReleaseTiledBackground(host, state);
  state.texture_page = static_cast<std::uint8_t>(texture_page);
  state.texture_uploaded = 0;
  if (const std::optional<TiledBackgroundSection> section =
          host.TiledBackground()) {
    auto layout = host.LoadBackgroundLayout(*section);
    if (!layout || !*layout) {
      VS::Core::LogError(kChannel, "QueueTiledBackgroundLoad: {}",
                         layout ? "empty native background" : layout.error());
      return 0;
    }
    if (!IsValidBackgroundTexture((*layout)->image)) {
      VS::Core::LogError(
          kChannel, "QueueTiledBackgroundLoad: invalid native image upload");
      return 0;
    }
    const auto& image_rect = (*layout)->image.source_rect;
    const auto word_width = image_rect.width + (image_rect.x & kPageColumnMask);
    state.texture_page_count = static_cast<std::int16_t>(
        (word_width + kPageColumnMask) / (kPageColumnMask + 1));
    if (host.ReserveTexturePages(state.texture_page, state.texture_page_count,
                                 kBackgroundPageOwner)) {
      state.layout = std::move(*layout);
      state.tiled_enabled = was_enabled;
      return 1;
    }
  }
  return 0;
}

// decomp: BATTLE.PRG func_8008F0FC (src/BATTLE/BATTLE.PRG/146C.c:17557)
std::int32_t FinishTiledBackgroundLoad(BackgroundHost& host,
                                       RoomBackgroundState& state) {
  if (!state.layout) return 0;
  if (state.texture_uploaded == 0) {
    state.texture_uploaded = 1;
    const auto& image = state.layout->image;
    if (!IsValidBackgroundTexture(image)) {
      VS::Core::LogError(
          kChannel,
          "FinishTiledBackgroundLoad: converted background image has "
          "invalid pixel dimensions or palette size");
      ReleaseTiledBackground(host, state);
      return 1;
    } else {
      const std::int32_t page = state.texture_page;
      TextureRect rect = image.source_rect;
      rect.x =
          static_cast<std::int16_t>((rect.x & kPageColumnMask) +
                                    ((page & kPageXMask) << kPageWidthShift));
      rect.y =
          static_cast<std::int16_t>(static_cast<std::uint8_t>(rect.y) +
                                    ((page & kPageRowBit) << kPageRowShift));
      host.UploadBackgroundPixels(image.pixel_words, rect);
      host.UploadBackgroundPalette(image.palette_words, kBackgroundClutRow);
    }
    return 0;
  }
  if (state.texture_uploaded == 1) {
    state.texture_uploaded = 2;
    return 1;
  }
  return 0;
}

// decomp: BATTLE.PRG func_8008F234 (src/BATTLE/BATTLE.PRG/146C.c:17587)
std::int32_t ReleaseTiledBackground(BackgroundHost& host,
                                    RoomBackgroundState& state) {
  state.tiled_enabled = 0;
  if (!state.layout) return 0;
  host.RetireTexturePages(state.texture_page, state.texture_page_count,
                          kBackgroundPageOwner);
  state.layout.reset();
  state.texture_uploaded = 0;
  return 1;
}
}  // namespace VS::Field
