#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Effects/WeatherPool.hpp"
#include "Field/Background.hpp"

struct Field_WeatherParticle;

// The weather pool's bindings to the copied globals, shared by
// WeatherPoolLive.cpp and WeatherRenderLive.cpp.
namespace VS::Effects::weather_live {
// rand(), the sine table, the low-end heap and the room weather.
class PoolHost final : public WeatherPoolHost {
 public:
  std::int32_t Random() override;
  std::int32_t Sine(std::int32_t angle) override;
  std::int32_t Cosine(std::int32_t angle) override;
  Field::WeatherParticlesId AllocateParticles(std::int32_t bytes) override;
  void FreeParticles(Field::WeatherParticlesId particles) override;
  void SetRoomWeather(std::int32_t kind) override;
};

[[nodiscard]] WeatherParticle ParticleOf(const Field_WeatherParticle& record);
void StoreParticle(Field_WeatherParticle& record,
                   const WeatherParticle& particle);
// Typed copies of `records`, and their store back.
[[nodiscard]] std::vector<WeatherParticle> LoadParticles(
    std::span<const Field_WeatherParticle> records);
void StoreParticles(std::span<Field_WeatherParticle> records,
                    std::span<const WeatherParticle> particles);
// The 0x800-byte buffer LiveAllocateWeatherParticles made, empty when none.
[[nodiscard]] std::span<Field_WeatherParticle> Buffer();
// Whether `particle` is one of the live buffer's records.
[[nodiscard]] bool InBuffer(const Field_WeatherParticle* particle);
// The room record's weather area, absent outside a room.
[[nodiscard]] std::optional<Field::WeatherArea> Area();
[[nodiscard]] WeatherPoolState LoadState();
void StoreState(const WeatherPoolState& state);
}  // namespace VS::Effects::weather_live
