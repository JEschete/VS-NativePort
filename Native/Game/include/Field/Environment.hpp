#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Field/Background.hpp"

// The room weather (BATTLE.PRG 2842C.c:266-547): starting the room's rain,
// snow, screen weather or embers, drifting the wind and intensity inside the
// room's limits, the ambient sound that follows the intensity, and the
// particle buffer's release; with them in 2842C.c, the tiled background's
// scroll offset and cell draw (Field/Background.hpp). The rules work on a
// RoomWeatherState value and reach the room record, the room clock, the
// random generator, the weather particles and the sound effects only through
// WeatherHost.
namespace VS::Field {
// The weather particle buffer's low-end heap handle; compared and never
// dereferenced.
enum class WeatherParticlesId : std::uintptr_t { kNone = 0 };

// Field_roomWeatherKind values.
inline constexpr std::uint8_t kNoWeather = 0;
inline constexpr std::uint8_t kRainWeather = 1;
inline constexpr std::uint8_t kSnowWeather = 2;
inline constexpr std::uint8_t kScreenWeather = 3;
inline constexpr std::uint8_t kEmberWeather = 4;

// Field_weatherAmbientSoundId values: -2 when the room has no ambient sound
// and -1 while the ambient sound is held off by the caller.
inline constexpr std::int16_t kNoAmbientSound = -2;
inline constexpr std::int16_t kAmbientSoundHeld = -1;
// The rain's looping sound effect.
inline constexpr std::int16_t kRainAmbientSound = 21;

// D_80068EB4: the ambient sound's volume for each weather intensity.
inline constexpr std::size_t kWeatherIntensityLevels = 16;
using WeatherVolumeTable = std::array<std::uint8_t, kWeatherIntensityLevels>;
inline constexpr WeatherVolumeTable kWeatherVolumeByIntensity = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40,
    0x5E, 0x67, 0x6E, 0x74, 0x79, 0x7C, 0x7E, 0x7F,
};

// The weather globals (2842C.c:266-547). The wind words are
// D_800F1D98[4..9]; [0..2] hold the particle velocity the weather effects
// derive from them.
struct RoomWeatherState {
  std::int16_t wind_strength = 0;      // D_800F1D98[4]
  std::int16_t wind_direction = 0;     // D_800F1D98[5]
  std::int16_t direction_step = 0;     // D_800F1D98[7]
  std::int16_t intensity_step = 0;     // D_800F1D98[8]
  std::int16_t strength_step = 0;      // D_800F1D98[9]
  std::int16_t intensity_scale = 0;    // D_800F1DA4, D_800F1D98[6]
  std::int16_t ambient_sound_id = 0;   // D_800F1DBC
  std::int16_t particle_count = 0;     // D_800F1DC0
  std::int16_t particle_capacity = 0;  // D_800F1DC2
  std::int16_t intensity = 0;          // D_800F1DC4
  std::uint8_t kind = kNoWeather;      // D_800F1DCA
  std::uint8_t variation_enabled = 0;  // D_800F1DCB
  WeatherParticlesId particles{};      // D_800F1DD4

  friend bool operator==(const RoomWeatherState&,
                         const RoomWeatherState&) = default;
};

class WeatherHost {
 public:
  WeatherHost() = default;
  WeatherHost(const WeatherHost&) = delete;
  WeatherHost& operator=(const WeatherHost&) = delete;
  WeatherHost(WeatherHost&&) = delete;
  WeatherHost& operator=(WeatherHost&&) = delete;
  virtual ~WeatherHost() = default;

  // vs_battle_roomData.environment, absent outside a room.
  [[nodiscard]] virtual std::optional<RoomEnvironment> Environment() = 0;
  // D_800F1BA4 and D_800F1CDC: the room's pause flag and frame clock.
  [[nodiscard]] virtual bool AreRoomUpdatesPaused() = 0;
  [[nodiscard]] virtual std::uint16_t RoomAnimationTicks() = 0;
  [[nodiscard]] virtual std::int32_t Random() = 0;
  // The weather effects (func_80090B28 and BATTLE.PRG 146C.c:17723 on): the
  // particle velocity from the wind words and the intensity scale, the
  // particle spawns, which set the kind, count and capacity, and the
  // per-frame draws.
  virtual void RecalculateWeatherVelocity() = 0;
  virtual void InitializeFallingParticles(std::int32_t count,
                                          std::int32_t kind) = 0;
  virtual void InitializeScreenParticles(std::int32_t count,
                                         std::int32_t kind) = 0;
  virtual void InitializeRisingEmberParticles(std::int32_t count,
                                              std::int32_t kind) = 0;
  virtual void DrawRainParticles() = 0;
  virtual void DrawSnowParticles() = 0;
  virtual void DrawScreenWeatherParticles() = 0;
  virtual void DrawRisingEmberParticles() = 0;
  // The ambient sound's effect in the field sound group.
  virtual void StopSound(std::int32_t sound) = 0;
  virtual void PlaySoundCentered(std::int32_t sound) = 0;
  virtual void FadeSoundVolume(std::int32_t sound, std::int32_t volume,
                               std::int32_t fade_ticks) = 0;
  // vs_main_freeHeap over the particle buffer.
  virtual void FreeWeatherParticles(WeatherParticlesId particles) = 0;
};

// func_80090C2C: starts the room's weather of `kind` at the room's wind and
// intensity, spawns its particles over the weather area and switches the
// ambient sound to the rain's or to none.
void SetRoomWeather(WeatherHost& host, RoomWeatherState& state,
                    std::int32_t kind);
// func_80090EEC: drifts the wind direction, wind strength and intensity on
// the room's intervals, then draws the particles.
void UpdateRoomWeather(WeatherHost& host, RoomWeatherState& state);
// func_8009121C: the particle count, ambient volume and intensity scale for
// the current intensity.
void ApplyWeatherIntensity(WeatherHost& host, RoomWeatherState& state);
// func_80091314.
void SetWeatherVariationEnabled(RoomWeatherState& state, std::int32_t enabled);
// func_80091320.
void SetWeatherIntensity(WeatherHost& host, RoomWeatherState& state,
                         std::int32_t intensity);
// func_8009134C.
void SetWeatherWindAngles(WeatherHost& host, RoomWeatherState& state,
                          std::int32_t azimuth, std::int32_t elevation);
// func_80091378.
void ReleaseWeatherParticles(WeatherHost& host, RoomWeatherState& state);
// func_800913BC: switches the ambient sound and returns the previous one.
[[nodiscard]] std::int32_t SetWeatherAmbientSound(WeatherHost& host,
                                                  RoomWeatherState& state,
                                                  std::int32_t sound);
// func_800918E8: lowers the ambient sound to a quarter, or restores it.
void DuckWeatherAmbientSound(WeatherHost& host, const RoomWeatherState& state,
                             std::int32_t ducked);

// func_80091468.
void SetRoomBackgroundOffset(RoomBackgroundState& state, std::int32_t offset_x,
                             std::int32_t offset_y);
// func_8009147C: the picture's cells turned by the camera's roll and wrapped
// every 1024 pixels across, then a black fill behind them. A layout whose
// cells do not cover its rows is refused with a diagnostic and only the fill
// is drawn. The original takes the texture page as an int but keeps only the
// halfword it ORs into each POLY_GT4's tpage.
void DrawRoomBackgroundTiles(BackgroundHost& host,
                             const BackgroundLayout& layout,
                             std::int32_t horizontal_offset,
                             std::int32_t vertical_offset,
                             std::uint16_t base_texture_page);
}  // namespace VS::Field
