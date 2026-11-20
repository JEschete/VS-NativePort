// Hand-maintained (tools/port/owned.csv).
#include "Effects/WeatherRenderLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Scratch.hpp"
#include "Effects/WeatherPool.hpp"
#include "Effects/WeatherPoolLiveSupport.hpp"
#include "Effects/WeatherRender.hpp"
#include "Field/Background.hpp"
#include "Field/BackgroundLiveState.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/EnvironmentLiveState.hpp"
#include "Field/Mpd.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Field/Weather.hpp"
#include "Legacy/Psx.h"
#include "Render/Frame.hpp"
#include "Render/LegacyPackets.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Timers.hpp"

namespace VS::Effects {
namespace {
// D_800F1D98's velocity words.
constexpr std::size_t kVelocityXWord = 0;
constexpr std::size_t kVelocityYWord = 1;
constexpr std::size_t kVelocityZWord = 2;
constexpr std::uint32_t kLinkMask = 0xFFFFFF;
constexpr std::uint32_t kLengthShift = 24;
constexpr std::uint32_t kHighHalfShift = 16;
constexpr int kCollisionLayer = 0;

std::uint64_t refused_calls = 0;
std::uint64_t dropped_packets = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void Refuse(std::string_view call, std::string_view reason) {
  if (ShouldReport(refused_calls)) {
    VS::Core::LogWarn("effects", "weather draw: {} refused: {} ({} refusals)",
                      call, reason, refused_calls);
  }
}

[[nodiscard]] SVECTOR VectorOf(const WeatherPoint& point) noexcept {
  return {.vx = point.x, .vy = point.y, .vz = point.z, .pad = 0};
}

[[nodiscard]] WeatherScreenPoint ScreenOf(Core_GteWord sxy) noexcept {
  const auto word = static_cast<std::uint32_t>(sxy);
  return {.x = static_cast<std::int16_t>(static_cast<std::uint16_t>(word)),
          .y = static_cast<std::int16_t>(
              static_cast<std::uint16_t>(word >> kHighHalfShift))};
}

class LiveRenderHost final : public WeatherRenderHost {
 public:
  void BeginProjection() override { Render::LoadLiveFrameViewMatrix(); }

  WeatherProjection Project(const WeatherPoint& point) override {
    SVECTOR vertex = VectorOf(point);
    Core_GteWord screen = 0;
    Core_GteWord interpolation = 0;
    Core_GteWord flags = 0;
    const Core_GteWord depth =
        Core_Gte_RotTransPers(&vertex, &screen, &interpolation, &flags);
    return {.screen = ScreenOf(screen),
            .depth = static_cast<std::int32_t>(depth)};
  }

  WeatherQuadProjection ProjectQuad(
      const std::array<WeatherPoint, kWeatherQuadCorners>& corners) override {
    std::array<SVECTOR, kWeatherQuadCorners> vertices{};
    std::ranges::transform(corners, vertices.begin(), VectorOf);
    std::array<Core_GteWord, kWeatherQuadCorners> screens{};
    Core_GteWord interpolation = 0;
    Core_GteWord flags = 0;
    const Core_GteWord depth = Core_Gte_RotTransPers4(
        &vertices[0], &vertices[1], &vertices[2], &vertices.back(), &screens[0],
        &screens[1], &screens[2], &screens.back(), &interpolation, &flags);
    WeatherQuadProjection projection{.depth = static_cast<std::int32_t>(depth)};
    std::ranges::transform(screens, projection.screens.begin(), ScreenOf);
    return projection;
  }

  WeatherTransform Transform(const WeatherPoint& point) override {
    SVECTOR vertex = VectorOf(point);
    VECTOR result{};
    Core_GteWord flags = 0;
    Core_Gte_RotTrans(&vertex, &result, &flags);
    return {.x = static_cast<std::int32_t>(result.vx),
            .y = static_cast<std::int32_t>(result.vy),
            .z = static_cast<std::int32_t>(result.vz)};
  }

  std::optional<std::uint8_t> CollisionSurface(std::int32_t tile_x,
                                               std::int32_t tile_z) override {
    const VS::Field::MpdTileCell* const cell = Field::LiveCollisionCellAt(
        static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_z),
        kCollisionLayer);
    if (cell == nullptr) return std::nullopt;
    return static_cast<std::uint8_t>(cell->surface_kind);
  }

  std::int32_t FloorHeight(std::int16_t x, std::int16_t z) override {
    return Field::LiveFloorHeightAt(x, z);
  }

  // addPrim in front of the scene table's entry. The original wrote past the
  // table or the primitive buffer; such a packet is dropped here.
  void Submit(std::int32_t slot, const WeatherPacket& packet) override {
    u_long* const entry =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, slot);
    const std::size_t words = std::size_t{packet.size} + 1;
    u_long* const tag =
        entry != nullptr ? Render::ReserveLivePrimitiveWords(words) : nullptr;
    if (tag == nullptr || packet.size > kWeatherPacketMaxWords) {
      if (ShouldReport(dropped_packets)) {
        VS::Core::LogWarn(
            "effects",
            "weather draw: packet at slot {} has no ordering entry or "
            "buffer room; dropped ({} drops)",
            slot, dropped_packets);
      }
      return;
    }
    const auto link = static_cast<std::uint32_t>(*entry);
    tag[0] = (std::uint32_t{packet.size} << kLengthShift) | (link & kLinkMask);
    std::copy_n(packet.words.begin(), packet.size, tag + 1);
    *entry = (link & ~kLinkMask) | (LegacyGpu_Handle(tag) & kLinkMask);
    VS::Render::FrameScratchpad().primitive_cursor = tag + words;
  }
};

[[nodiscard]] WeatherFrame LiveFrame() {
  return {.paused = VS::Field::areRoomUpdatesPaused != 0,
          .tick_speed = VS::State::tickSpeed,
          .velocity = {.x = VS::Field::weatherParameters[kVelocityXWord],
                       .y = VS::Field::weatherParameters[kVelocityYWord],
                       .z = VS::Field::weatherParameters[kVelocityZWord]}};
}

// The first background reference VECTOR read as an SVECTOR, as the original
// did: its x word's halves and its y word's low half.
[[nodiscard]] WeatherPoint LiveReference() {
  const VECTOR& vector = VS::Field::backgroundReferenceVectors[0];
  const auto x = static_cast<std::uint32_t>(vector.vx);
  const auto y = static_cast<std::uint32_t>(vector.vy);
  return {.x = static_cast<std::int16_t>(static_cast<std::uint16_t>(x)),
          .y = static_cast<std::int16_t>(
              static_cast<std::uint16_t>(x >> kHighHalfShift)),
          .z = static_cast<std::int16_t>(static_cast<std::uint16_t>(y))};
}

// Runs a draw over typed copies of the live buffer's particles and stores
// them back. `draw` returns whether the particles fitted.
template <typename Draw>
void RunDraw(std::string_view call, Draw draw) {
  const std::span<VS::Field::WeatherParticleRecord> buffer =
      weather_live::Buffer();
  std::vector<WeatherParticle> particles = weather_live::LoadParticles(buffer);
  weather_live::PoolHost pool;
  LiveRenderHost render;
  const bool fitted = draw(pool, render, std::span(particles));
  weather_live::StoreParticles(buffer, particles);
  if (!fitted) {
    Refuse(call, buffer.empty() ? "no particle buffer"
                                : "more particles than the buffer holds");
  }
}
}  // namespace

// decomp: BATTLE.PRG func_8008F440 (146C.c:17758).
void LiveDrawRainParticles() {
  const WeatherFrame frame = LiveFrame();
  const std::optional<Field::WeatherArea> area = weather_live::Area();
  RunDraw("DrawRainParticles",
          [&](WeatherPoolHost& pool, WeatherRenderHost& render,
              std::span<WeatherParticle> particles) {
            return DrawRainParticles(pool, render, frame, particles,
                                     VS::Field::weatherParticleCount, area);
          });
}

// decomp: BATTLE.PRG func_8008F9A4 (146C.c:17864).
void LiveInitializeRisingEmberParticles(int particle_count, int weather_kind) {
  const std::optional<Field::WeatherArea> area = weather_live::Area();
  if (!area) {
    Refuse("InitializeRisingEmberParticles", "no room record");
    return;
  }
  WeatherPoolState state = weather_live::LoadState();
  RunDraw("InitializeRisingEmberParticles",
          [&](WeatherPoolHost& pool, WeatherRenderHost& /*render*/,
              std::span<WeatherParticle> particles) {
            return InitializeRisingEmberParticles(pool, state, particles, *area,
                                                  particle_count, weather_kind);
          });
  weather_live::StoreState(state);
}

// decomp: BATTLE.PRG func_8008FA14 (146C.c:17878).
void LiveResetRisingEmberParticle(
    VS::Field::WeatherParticleRecord* weather_particle) {
  const std::optional<Field::WeatherArea> area = weather_live::Area();
  if (!area) {
    Refuse("ResetRisingEmberParticle", "no room record");
    return;
  }
  if (!weather_live::InBuffer(weather_particle)) {
    Refuse("ResetRisingEmberParticle", "particle outside the particle buffer");
    return;
  }
  weather_live::PoolHost pool;
  weather_live::StoreParticle(
      *weather_particle,
      ResetRisingEmberParticle(
          pool, weather_live::ParticleOf(*weather_particle), *area));
}

// decomp: BATTLE.PRG func_8008FAC8 (146C.c:17889).
void LiveDrawRisingEmberParticles() {
  const std::optional<Field::WeatherArea> area = weather_live::Area();
  if (!area) {
    Refuse("DrawRisingEmberParticles", "no room record");
    return;
  }
  const WeatherFrame frame = LiveFrame();
  RunDraw("DrawRisingEmberParticles",
          [&](WeatherPoolHost& pool, WeatherRenderHost& render,
              std::span<WeatherParticle> particles) {
            return DrawRisingEmberParticles(pool, render, frame, particles,
                                            VS::Field::weatherParticleCount,
                                            *area);
          });
}

// decomp: BATTLE.PRG func_8008FDC4 (146C.c:17938).
void LiveDrawSnowParticles() {
  const WeatherFrame frame = LiveFrame();
  const std::optional<Field::WeatherArea> area = weather_live::Area();
  RunDraw("DrawSnowParticles",
          [&](WeatherPoolHost& pool, WeatherRenderHost& render,
              std::span<WeatherParticle> particles) {
            return DrawSnowParticles(pool, render, frame, particles,
                                     VS::Field::weatherParticleCount, area);
          });
}

// decomp: BATTLE.PRG func_80090434 (146C.c:18072).
void LiveDrawScreenWeatherParticles() {
  const WeatherFrame frame = LiveFrame();
  const WeatherPoint reference = LiveReference();
  WeatherPoolState state = weather_live::LoadState();
  RunDraw("DrawScreenWeatherParticles",
          [&](WeatherPoolHost& pool, WeatherRenderHost& render,
              std::span<WeatherParticle> particles) {
            return DrawScreenWeatherParticles(pool, render, frame, reference,
                                              state, particles);
          });
  weather_live::StoreState(state);
}
}  // namespace VS::Effects
