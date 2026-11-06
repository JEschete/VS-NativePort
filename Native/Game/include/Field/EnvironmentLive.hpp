#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Field/Environment.hpp"

// Live binding for the room weather (Environment.hpp) over the copied weather
// globals (EnvironmentLiveState.h), the room record, the room clock, the
// random generator, the weather effects (Effects/Weather.h), the field sound
// group and the low-end heap. Each entry loads the globals into a
// RoomWeatherState, runs the typed core and stores them back; the weather
// effects read and write the globals themselves, so the host stores the state
// before each and reloads it after. The entries keep the original call shapes
// (BATTLE.PRG 2842C.c:266-547) for the generated room, script and effect
// callers and the ambience binding.
namespace VS::Field {
// The weather globals as a typed value, and back. D_800F1DA4 is the
// original's D_800F1D98[6]: the store writes the intensity scale to both.
[[nodiscard]] RoomWeatherState LoadLiveRoomWeatherState();
void StoreLiveRoomWeatherState(const RoomWeatherState& state);

class LiveWeatherHost final : public WeatherHost {
 public:
  explicit LiveWeatherHost(RoomWeatherState& state) : state_(state) {}

  [[nodiscard]] std::optional<RoomEnvironment> Environment() override;
  [[nodiscard]] bool AreRoomUpdatesPaused() override;
  [[nodiscard]] std::uint16_t RoomAnimationTicks() override;
  [[nodiscard]] std::int32_t Random() override;
  void RecalculateWeatherVelocity() override;
  void InitializeFallingParticles(std::int32_t count,
                                  std::int32_t kind) override;
  void InitializeScreenParticles(std::int32_t count,
                                 std::int32_t kind) override;
  void InitializeRisingEmberParticles(std::int32_t count,
                                      std::int32_t kind) override;
  void DrawRainParticles() override;
  void DrawSnowParticles() override;
  void DrawScreenWeatherParticles() override;
  void DrawRisingEmberParticles() override;
  void StopSound(std::int32_t sound) override;
  void PlaySoundCentered(std::int32_t sound) override;
  void FadeSoundVolume(std::int32_t sound, std::int32_t volume,
                       std::int32_t fade_ticks) override;
  void FreeWeatherParticles(WeatherParticlesId particles) override;

 private:
  // Stores the state for a weather effect that reads or writes the globals,
  // runs it and reloads the state.
  template <typename Call>
  void Escape(Call call);

  RoomWeatherState& state_;
};

// func_80090C2C (2842C.c:266).
void LiveSetRoomWeather(int kind);
// func_80090EEC (2842C.c:329).
void LiveUpdateRoomWeather();
// func_80091314 (2842C.c:428).
void LiveSetWeatherVariationEnabled(int enabled);
// func_80091320 (2842C.c:430).
void LiveSetWeatherIntensity(int intensity);
// func_8009134C (2842C.c:437).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveSetWeatherWindAngles(int azimuth, int elevation);
// func_80091378 (2842C.c:444).
void LiveReleaseWeatherParticles();
// func_800913BC (2842C.c:454): the previous ambient sound.
int LiveSetWeatherAmbientSound(int sound);
// func_80091468 (2842C.c:472): the tiled background's scroll offset.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveSetRoomBackgroundOffset(int offset_x, int offset_y);
// func_800918E8 (2842C.c:539).
void LiveDuckWeatherAmbientSound(int ducked);
}  // namespace VS::Field
