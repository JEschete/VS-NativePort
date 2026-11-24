#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Field/Camera.hpp"

// The room background (BATTLE.PRG 146C.c:17431-17723 and 2842C.c:472-537):
// the sky dome shaded with the room's five gradient colours, the solid
// backdrop rooms with one colour draw instead, and the tiled picture
// backgrounds some rooms load into texture pages and scroll with the camera.
// The rules work on a RoomBackgroundState value and reach the room record,
// the frame camera, the heap, the disc, VRAM and the primitive buffer only
// through BackgroundHost.
namespace VS::Field {
struct RoomWeatherState;  // Field/Environment.hpp

struct OwnedBackgroundLayout;

// A file on the disc, in sectors and bytes (vs_main_CdFile).
struct BackgroundFile {
  std::int32_t lba = 0;
  std::int32_t size = 0;

  friend bool operator==(const BackgroundFile&,
                         const BackgroundFile&) = default;
};

// The room's sky gradient, zenith to horizon, as 0x00BBGGRR words.
inline constexpr std::size_t kGradientColorCount = 5;
using GradientColors = std::array<std::int32_t, kGradientColorCount>;

// D_80068C1C: the sky dome's 34 vertices and the colour word of each.
inline constexpr std::size_t kSkyDomeVertexCount = 34;
using SkyDomeColors = std::array<std::uint32_t, kSkyDomeVertexCount>;

// Q12 1.0: the parallax scale that moves the picture with the camera.
inline constexpr std::int16_t kUnitParallax = 0x1000;

// func_8007DFF0's owner tag for the background's texture pages.
inline constexpr int kBackgroundPageOwner = 5;

// The background globals (146C.c:17431-17723).
struct RoomBackgroundState {
  GradientColors gradient_colors{};     // D_800F1D78
  std::int16_t offset_x = 0;            // D_800F1DB4
  std::int16_t offset_y = 0;            // D_800F1DB6
  std::int16_t parallax_x = 0;          // D_800F1DB8, Q12
  std::int16_t parallax_y = 0;          // D_800F1DBA, Q12
  std::int16_t texture_page_count = 0;  // D_800F1DBE
  std::uint8_t solid = 0;               // D_800F1DC6
  std::uint8_t texture_page = 0;        // D_800F1DC8
  std::uint8_t tiled_enabled = 0;       // D_800F1DC9
  BackgroundFile texture_file{};        // native source for the loaded image
  std::uint8_t texture_uploaded = 0;    // native load step, not a legacy global
  std::shared_ptr<const OwnedBackgroundLayout> layout{};

  friend bool operator==(const RoomBackgroundState&,
                         const RoomBackgroundState&) = default;
};

// _mpdRoomSectionB.weatherArea: as loaded, a corner and the far corner in
// 128-unit cells; InitializeRoomBackgroundColors turns them into a corner
// and a span in world units.
struct WeatherArea {
  std::int16_t origin_x = 0;
  std::int16_t origin_z = 0;
  std::int16_t extent_x = 0;
  std::int16_t extent_z = 0;

  friend bool operator==(const WeatherArea&, const WeatherArea&) = default;
};

// The background and weather fields of the room's environment record
// (_mpdRoomSectionB, 146C.h:136).
struct RoomEnvironment {
  GradientColors background_colors{};
  CameraVector background_rotation;
  std::int16_t wind_direction = 0;
  std::int16_t wind_direction_limit = 0;
  std::int16_t wind_direction_interval = 0;
  std::int16_t weather_intensity = 0;
  std::int16_t weather_intensity_limit = 0;
  std::int16_t weather_intensity_interval = 0;
  std::int16_t wind_strength = 0;
  std::int16_t wind_strength_limit = 0;
  std::int16_t wind_strength_interval = 0;
  WeatherArea weather_area;

  friend bool operator==(const RoomEnvironment&,
                         const RoomEnvironment&) = default;
};

// The collision map's size in tiles.
struct CollisionMapExtent {
  std::int16_t width = 0;
  std::int16_t height = 0;
};

// D_800F1BB0: the room's colour tint, -128..127 per channel.
struct RoomTint {
  std::int8_t red = 0;
  std::int8_t green = 0;
  std::int8_t blue = 0;
  std::int16_t is_active = 0;
};

struct ScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const ScreenPoint&, const ScreenPoint&) = default;
};

struct TexelPoint {
  std::uint8_t u = 0;
  std::uint8_t v = 0;

  friend bool operator==(const TexelPoint&, const TexelPoint&) = default;
};

struct PrimitiveColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const PrimitiveColor&,
                         const PrimitiveColor&) = default;
};

// The corners of a POLY_FT4, a POLY_GT4 and a picture cell.
inline constexpr std::size_t kQuadCorners = 4;

// The POLY_FT4 and POLY_GT4 fields the background writes, corners in
// top-left, top-right, bottom-left, bottom-right order. A flat quad uses
// colors[0].
struct TexturedQuad {
  std::array<ScreenPoint, kQuadCorners> corners{};
  std::array<TexelPoint, kQuadCorners> texels{};
  std::array<PrimitiveColor, kQuadCorners> colors{};
  std::uint16_t texture_page = 0;
  std::uint16_t clut = 0;

  friend bool operator==(const TexturedQuad&, const TexturedQuad&) = default;
};

// A TILE: an untextured rectangle.
struct FillRect {
  PrimitiveColor color;
  ScreenPoint position;
  std::int16_t width = 0;
  std::int16_t height = 0;

  friend bool operator==(const FillRect&, const FillRect&) = default;
};

// A RECT in VRAM or texture-page coordinates.
struct TextureRect {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;

  friend bool operator==(const TextureRect&, const TextureRect&) = default;
};

// A CVECTOR: a corner colour and, in the second corner, the tile's
// texture-page depth code (not semi-transparency).
struct TileCorner {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t code = 0;
};

// vs_battle_backgroundTile (2842C.h:9): one 32x32 cell of the picture.
struct BackgroundTile {
  TextureRect texture;
  std::array<TileCorner, kQuadCorners> corners{};
};

// vs_battle_backgroundLayout (2842C.h:16): the picture's size in cells and
// the cells the host could read, row by row.
struct BackgroundLayout {
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::span<const BackgroundTile> tiles;
};

struct OwnedBackgroundLayout {
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::vector<BackgroundTile> tiles;
};

// The room's tiled background section: the texture pages its picture takes
// and its image source and layout files.
struct TiledBackgroundSection {
  std::uint16_t texture_page_count = 0;
  BackgroundFile texture;
  BackgroundFile layout;
};

// Image data in PS1 VRAM upload order, retaining the source TIM's rectangle.
struct BackgroundTextureUpload {
  TextureRect source_rect;
  std::vector<std::uint16_t> pixel_words;
  std::vector<std::uint16_t> palette_words;
};

class BackgroundHost {
 public:
  BackgroundHost() = default;
  BackgroundHost(const BackgroundHost&) = delete;
  BackgroundHost& operator=(const BackgroundHost&) = delete;
  BackgroundHost(BackgroundHost&&) = delete;
  BackgroundHost& operator=(BackgroundHost&&) = delete;
  virtual ~BackgroundHost() = default;

  // vs_battle_roomData.environment, absent outside a room.
  [[nodiscard]] virtual std::optional<RoomEnvironment> Environment() = 0;
  // Writes the weather area back into the room's environment record.
  virtual void StoreWeatherArea(const WeatherArea& area) = 0;
  // vs_battle_roomData.collisionMap's size, absent without one.
  [[nodiscard]] virtual std::optional<CollisionMapExtent>
  CollisionMapSize() = 0;
  // Writes the 34 vertex colour words into the sky dome.
  virtual void StoreSkyDomeColors(const SkyDomeColors& colors) = 0;
  // The frame camera (scratch 0x1F800034).
  [[nodiscard]] virtual ViewCamera FrameCamera() = 0;
  [[nodiscard]] virtual RoomTint ActiveRoomTint() = 0;
  // A frame primitive linked at the farthest ordering-table entry.
  virtual void AddFlatTexturedQuad(const TexturedQuad& quad) = 0;
  virtual void AddShadedTexturedQuad(const TexturedQuad& quad) = 0;
  virtual void AddFill(const FillRect& fill) = 0;
  // Loads the frame's view into the GTE, builds the dome's view at
  // `position` turned by `angles` (corrected for a zoomed far clip when
  // `scale_compensated`) and draws the dome.
  virtual void DrawSkyDome(const CameraVector& position,
                           const CameraVector& angles,
                           bool scale_compensated) = 0;
  // D_800F1CE0[0]: the dome's turn when neither the caller nor the room
  // gives one.
  [[nodiscard]] virtual CameraVector ReferenceRotation() = 0;
  [[nodiscard]] virtual std::expected<
      std::shared_ptr<const OwnedBackgroundLayout>, std::string>
  LoadBackgroundLayout(const TiledBackgroundSection& section) = 0;
  // vs_battle_roomData.tiledBackground, absent when the room has none.
  [[nodiscard]] virtual std::optional<TiledBackgroundSection>
  TiledBackground() = 0;
  // func_8007DFF0 and func_8007E0A8: lends or retires texture pages; the
  // reservation is false when it was refused.
  [[nodiscard]] virtual bool ReserveTexturePages(int first_page, int page_count,
                                                 int owner) = 0;
  virtual void RetireTexturePages(int first_page, int page_count,
                                  int owner) = 0;
  // Loads an already-converted native image; it does not read or decode TIM.
  [[nodiscard]] virtual std::expected<BackgroundTextureUpload, std::string>
  LoadBackgroundTexture(const BackgroundFile& source) = 0;
  virtual void UploadBackgroundPixels(std::span<const std::uint16_t> words,
                                      const TextureRect& rect) = 0;
  virtual void UploadBackgroundPalette(std::span<const std::uint16_t> words,
                                       int row) = 0;
};

// func_8008E88C: clears the background and weather loads, then reads the
// room's colours (InitializeRoomBackgroundColors).
void InitializeRoomBackground(BackgroundHost& host, RoomBackgroundState& state,
                              RoomWeatherState& weather);
// func_8008E938: resets the scroll and parallax, picks the solid backdrop or
// the dome's colours, and turns the room's weather area into world units.
void InitializeRoomBackgroundColors(BackgroundHost& host,
                                    RoomBackgroundState& state);
// func_8008EAE4.
void SetBackgroundParallaxScale(RoomBackgroundState& state,
                                std::int32_t parallax_x,
                                std::int32_t parallax_y);
// func_8008EAF8.
void SetTiledBackgroundEnabled(RoomBackgroundState& state,
                               std::int32_t enabled);
// func_8008EB24.
[[nodiscard]] const GradientColors& RoomBackgroundColors(
    const RoomBackgroundState& state);
// func_8008EB30: keeps the five colours and spreads them over the dome's
// vertex rings.
void SetRoomBackgroundColors(BackgroundHost& host, RoomBackgroundState& state,
                             const GradientColors& colors);
// func_8008EC48: the tiled picture, the solid backdrop or the sky dome.
void DrawRoomBackground(BackgroundHost& host, const RoomBackgroundState& state,
                        const std::optional<CameraVector>& rotation_override);
// func_8008EFCC: validates the native layout/image binding, then lends pages.
[[nodiscard]] std::int32_t QueueTiledBackgroundLoad(BackgroundHost& host,
                                                    RoomBackgroundState& state,
                                                    std::int32_t texture_page);
// func_8008F0FC: uploads the converted image once, then signals completion.
[[nodiscard]] std::int32_t FinishTiledBackgroundLoad(
    BackgroundHost& host, RoomBackgroundState& state);
// func_8008F234: disables the picture and returns its pages and layout; 1
// when a layout was loaded.
std::int32_t ReleaseTiledBackground(BackgroundHost& host,
                                    RoomBackgroundState& state);
// SetRoomBackgroundOffset and DrawRoomBackgroundTiles (2842C.c:472-537) are
// declared with the weather in Field/Environment.hpp.
}  // namespace VS::Field
