// Hand-maintained (tools/port/owned.csv).
#include "Effects/WeatherRender.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/WeatherPool.hpp"
#include "Field/Background.hpp"

namespace VS::Effects {
namespace {
namespace Math = Core::Math;

// The screen and the ranges the draws accept a projection in.
constexpr std::int32_t kScreenWidth = 320;
constexpr std::int32_t kScreenHeight = 240;
constexpr std::int32_t kGuardLeft = -639;
constexpr std::int32_t kGuardRight = 960;
constexpr std::int32_t kFarDepth = 2048;
constexpr std::int32_t kNearDepth = 5;
constexpr std::int32_t kEmberNearDepth = 65;

// The packets: their GPU codes (semi-transparent), word byte lanes and
// colours.
constexpr std::uint32_t kLineCode = 0x52;
constexpr std::uint32_t kTexturedQuadCode = 0x2E;
constexpr std::uint32_t kGradientQuadCode = 0x3A;
constexpr std::uint32_t kTileCode = 0x62;
constexpr std::uint32_t kSecondByteShift = 8;
constexpr std::uint32_t kHighHalfShift = 16;
constexpr std::uint32_t kTopByteShift = 24;
constexpr std::uint32_t kTexturePageCommand = 0xE1000200;
constexpr std::uint32_t kTexturePageModeMask = 0x1FF;
constexpr std::uint32_t kAdditiveTexturePage = 0x60;
constexpr std::uint32_t kSnowTexturePage = 0x71;
constexpr std::uint32_t kEmberTexturePage = 0x20;
constexpr std::uint32_t kScreenTexturePage = 0x31;
constexpr std::uint32_t kSplashClut = 0x38BF;
constexpr std::uint32_t kSplashTexturePage = 0x71;
constexpr std::uint8_t kSplashTop = 0xC0;
constexpr std::uint8_t kSplashBottom = 0xD0;
constexpr std::uint8_t kSplashCell = 16;
constexpr std::uint8_t kSplashFrames = 8;
constexpr std::int16_t kSplashHalfSize = 24;
constexpr std::int32_t kSplashDepthBias = 64;
constexpr std::uint8_t kSettledShade = 128;
constexpr std::uint8_t kSettledShadeStep = 16;
constexpr std::uint8_t kFlakeShade = 0xFF;

struct Rgb {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
};
constexpr Rgb kStreakHead{.r = 0x60, .g = 0x60, .b = 0x80};
constexpr Rgb kStreakTail{.r = 0x20, .g = 0x20, .b = 0x64};
constexpr Rgb kSplashShade{.r = 0x80, .g = 0x80, .b = 0x80};
constexpr Rgb kFlameEdge{.r = 0x20, .g = 0x08, .b = 0x20};
constexpr Rgb kFlameCore{.r = 0xC0, .g = 0x40, .b = 0x40};
constexpr Rgb kFlameTip{};

// The fall: the shift a respawned or undrawn particle falls by, the collision
// tile size, the floor's 15-bit height, the surface with no floor, the sway's
// angles and period, and the ember's rise, respawn height and flame length.
constexpr std::uint8_t kUndrawnFallShift = 2;
constexpr std::uint32_t kShiftMask = 31;
constexpr std::int32_t kTileSize = 128;
constexpr unsigned kFloorHeightShift = 17;
constexpr std::uint8_t kNoFloorSurface = 31;
constexpr std::int32_t kDriftShift = 3;
constexpr std::int32_t kSpeedAngleShift = 5;
constexpr std::int32_t kSwayAngleShift = 6;
constexpr std::int32_t kSwayShift = 9;
constexpr std::int32_t kDriftSwayShift = 10;
constexpr std::uint32_t kSpeedJitterMask = 15;
constexpr std::int32_t kSpeedJitterBias = 7;
constexpr std::uint8_t kSpeedMask = 0x7F;
constexpr std::uint32_t kSwayChanceMask = 0x7F;
constexpr std::uint8_t kSwayPeriod = 32;
constexpr std::int32_t kFlakeOffsetShift = 6;
constexpr std::int32_t kEmberCeiling = -1024;
constexpr std::int32_t kEmberHeightBand = 512;
constexpr std::int32_t kEmberDepth = 0x200;
constexpr std::uint32_t kEmberSpeedMask = 15;
constexpr std::uint8_t kEmberMinimumSpeed = 8;
constexpr std::int16_t kFlameLength = 192;
constexpr std::int32_t kFlameWidthDivisor = 32;

// The screen weather: its depth band, perspective, fade and shade.
constexpr std::int16_t kScreenDepthMask = 0xFF;
constexpr std::int32_t kScreenCentreX = 160;
constexpr std::int32_t kScreenCentreY = 120;
constexpr std::int32_t kPerspectiveShift = 12;
constexpr std::int32_t kDepthScaleShift = 2;
constexpr std::int32_t kViewDistance = 3072;
constexpr std::int32_t kFarShade = 255;
constexpr std::int32_t kShadeDivisor = 4;
constexpr std::int32_t kFadeBias = 32;
constexpr std::uint8_t kFadeStep = 8;
constexpr std::uint8_t kFadeLimit = 96;
constexpr std::int32_t kTrailScale = 2;

// The remainder the R3000's DIV leaves in HI: a zero divisor leaves the
// dividend, and INT32_MIN by -1 leaves 0.
[[nodiscard]] constexpr std::int16_t Half(std::int32_t value) noexcept {
  return Math::WrapSigned16(static_cast<std::uint32_t>(value));
}

[[nodiscard]] constexpr std::int16_t AddHalf(std::int16_t value,
                                             std::int32_t delta) noexcept {
  return Half(Math::WrappingAdd(value, delta));
}

// sllv: the shift amount's low five bits.
[[nodiscard]] constexpr std::int32_t ShiftLeft(std::int32_t value,
                                               std::uint8_t shift) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value)
                                   << (shift & kShiftMask));
}

[[nodiscard]] constexpr std::uint32_t PointWord(std::int32_t x,
                                                std::int32_t y) noexcept {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y))
          << kHighHalfShift);
}

[[nodiscard]] constexpr std::uint32_t PointWord(
    WeatherScreenPoint point) noexcept {
  return PointWord(point.x, point.y);
}

[[nodiscard]] constexpr std::uint32_t ColorWord(Rgb color,
                                                std::uint32_t code) noexcept {
  return color.r | (static_cast<std::uint32_t>(color.g) << kSecondByteShift) |
         (static_cast<std::uint32_t>(color.b) << kHighHalfShift) |
         (code << kTopByteShift);
}

[[nodiscard]] constexpr Rgb Gray(std::int32_t shade) noexcept {
  const auto value = static_cast<std::uint8_t>(shade);
  return {.r = value, .g = value, .b = value};
}

[[nodiscard]] constexpr std::uint32_t TexelWord(std::uint8_t u, std::uint8_t v,
                                                std::uint32_t high) noexcept {
  return u | (static_cast<std::uint32_t>(v) << kSecondByteShift) |
         (high << kHighHalfShift);
}

// A packet of the given words; the packets here never exceed the maximum.
[[nodiscard]] WeatherPacket PacketOf(
    std::initializer_list<std::uint32_t> words) {
  WeatherPacket packet{};
  const std::size_t count = std::min(words.size(), packet.words.size());
  std::copy_n(words.begin(), count, packet.words.begin());
  packet.size = static_cast<std::uint8_t>(count);
  return packet;
}

// _insertTPage: a draw-mode command and a NOP.
[[nodiscard]] WeatherPacket TexturePagePacket(std::uint32_t mode) {
  return PacketOf({kTexturePageCommand | (mode & kTexturePageModeMask), 0});
}

// A TILE of `size` pixels on each side.
[[nodiscard]] WeatherPacket TilePacket(std::int32_t x, std::int32_t y,
                                       std::int16_t size, Rgb color) {
  return PacketOf(
      {ColorWord(color, kTileCode), PointWord(x, y), PointWord(size, size)});
}

[[nodiscard]] constexpr bool OnScreen(WeatherScreenPoint point) noexcept {
  return point.y > 0 && point.y < kScreenHeight && point.x > 0 &&
         point.x < kScreenWidth;
}

// The projection a falling particle stays alive in.
[[nodiscard]] constexpr bool InFallRange(
    const WeatherProjection& projection) noexcept {
  return projection.screen.x >= kGuardLeft &&
         projection.screen.x < kGuardRight && projection.depth < kFarDepth;
}

// The tick loops' count: tick speed / 2 while room updates run, else none.
[[nodiscard]] constexpr std::int32_t TicksOf(
    const WeatherFrame& frame) noexcept {
  return frame.paused ? 0 : frame.tick_speed >> 1;
}

[[nodiscard]] std::span<WeatherParticle> DrawnOf(
    std::span<WeatherParticle> particles, std::int32_t count) {
  if (count <= 0) return {};
  const auto wanted = static_cast<std::size_t>(count);
  return particles.first(wanted < particles.size() ? wanted : particles.size());
}

[[nodiscard]] bool Fits(std::span<WeatherParticle> particles,
                        std::int32_t count) {
  return count <= 0 || static_cast<std::size_t>(count) <= particles.size();
}

[[nodiscard]] constexpr bool IsOdd(std::size_t index) noexcept {
  return (index & 1U) != 0;
}

[[nodiscard]] WeatherPoint PositionOf(const WeatherParticle& particle) {
  return {.x = particle.x, .y = particle.y, .z = particle.z};
}

// func_8008DC7C's height, kept to its low 15 bits, sign-extended.
[[nodiscard]] std::int16_t FloorOf(WeatherRenderHost& render,
                                   const WeatherParticle& particle) {
  const std::int32_t height = render.FloorHeight(particle.x, particle.z);
  return Half(static_cast<std::int32_t>(static_cast<std::uint32_t>(height)
                                        << kFloorHeightShift) >>
              kFloorHeightShift);
}

void Respawn(WeatherPoolHost& pool, WeatherParticle& particle,
             const std::optional<Field::WeatherArea>& area) {
  particle = ResetFallingWeatherParticle(pool, particle, area);
}

// The snow and screen weather's drift for one tick: a random change of
// speed, the drift and a sway along the speed's angle, larger while the
// particle sways down and back (`sway_phase` 1 to 32), which a 1 in 128 chance
// starts.
void Drift(WeatherPoolHost& pool, WeatherParticle& particle,
           std::int32_t drift_x, std::int32_t drift_z) {
  const auto jitter = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(pool.Random()) & kSpeedJitterMask);
  particle.speed = static_cast<std::uint8_t>(
      (particle.speed + jitter - kSpeedJitterBias) & kSpeedMask);
  const std::int32_t angle = particle.speed << kSpeedAngleShift;
  const bool swaying = particle.sway_phase != 0;
  const std::int32_t sway_shift = swaying ? kSwayShift : kDriftSwayShift;
  particle.x = AddHalf(
      particle.x, Math::WrappingAdd(drift_x, pool.Sine(angle) >> sway_shift));
  particle.z = AddHalf(
      particle.z,
      Math::WrappingSubtract(drift_z, pool.Cosine(angle) >> sway_shift));
  if (swaying) {
    particle.y = AddHalf(
        particle.y,
        -(pool.Sine(particle.sway_phase << kSwayAngleShift) >> kSwayShift));
    ++particle.sway_phase;
    if (particle.sway_phase > kSwayPeriod) particle.sway_phase = 0;
  } else if ((static_cast<std::uint32_t>(pool.Random()) & kSwayChanceMask) ==
             0) {
    particle.sway_phase = 1;
  }
}

// A settled rain splash or snow flake's frame advance: false once its four
// frames have played.
[[nodiscard]] bool AdvanceSettled(const WeatherFrame& frame,
                                  WeatherParticle& particle) {
  if (!frame.paused) {
    particle.sway_phase = static_cast<std::uint8_t>(particle.sway_phase +
                                                    (frame.tick_speed >> 1));
  }
  return particle.sway_phase < kSplashFrames;
}

[[nodiscard]] WeatherPacket StreakPacket(WeatherScreenPoint head,
                                         WeatherScreenPoint tail) {
  return PacketOf({ColorWord(kStreakHead, kLineCode), PointWord(head),
                   ColorWord(kStreakTail, 0), PointWord(tail)});
}

[[nodiscard]] WeatherPacket SplashPacket(const WeatherQuadProjection& quad,
                                         std::uint8_t sway_phase) {
  const auto left = static_cast<std::uint8_t>((sway_phase >> 1U) * kSplashCell);
  const auto right = static_cast<std::uint8_t>(left + kSplashCell);
  return PacketOf(
      {ColorWord(kSplashShade, kTexturedQuadCode), PointWord(quad.screens[0]),
       TexelWord(left, kSplashTop, kSplashClut), PointWord(quad.screens[1]),
       TexelWord(right, kSplashTop, kSplashTexturePage),
       PointWord(quad.screens[2]), TexelWord(left, kSplashBottom, 0),
       PointWord(quad.screens.back()), TexelWord(right, kSplashBottom, 0)});
}

void FallRain(WeatherPoolHost& pool, WeatherRenderHost& render,
              const WeatherFrame& frame, WeatherParticle& particle,
              std::size_t index,
              const std::optional<Field::WeatherArea>& area) {
  const std::uint8_t fall_shift = particle.fall_shift;
  for (std::int32_t tick = 0; tick < TicksOf(frame); ++tick) {
    particle.y = AddHalf(particle.y, ShiftLeft(frame.velocity.y, fall_shift));
    if (fall_shift == 0) {
      particle.x = AddHalf(particle.x, frame.velocity.x);
      particle.z = AddHalf(particle.z, frame.velocity.z);
    }
  }
  particle.fall_shift = kUndrawnFallShift;
  const WeatherProjection head = render.Project(PositionOf(particle));
  if (!InFallRange(head)) {
    Respawn(pool, particle, area);
  } else if (OnScreen(head.screen) && head.depth >= kNearDepth) {
    const WeatherProjection tail =
        render.Project({.x = AddHalf(particle.x, -frame.velocity.x),
                        .y = AddHalf(particle.y, -frame.velocity.y),
                        .z = AddHalf(particle.z, -frame.velocity.z)});
    render.Submit(head.depth, StreakPacket(head.screen, tail.screen));
    render.Submit(head.depth, TexturePagePacket(kAdditiveTexturePage));
    particle.fall_shift = 0;
  }
  if (particle.y <= 0) return;
  if (particle.fall_shift == 0 && IsOdd(index)) {
    const std::optional<std::uint8_t> surface =
        render.CollisionSurface(particle.x / kTileSize, particle.z / kTileSize);
    if (surface && *surface != kNoFloorSurface) {
      particle.opaque_c = 1;
      particle.y = FloorOf(render, particle);
      return;
    }
  }
  Respawn(pool, particle, area);
}

void SplashRain(WeatherPoolHost& pool, WeatherRenderHost& render,
                const WeatherFrame& frame, WeatherParticle& particle,
                const std::optional<Field::WeatherArea>& area) {
  const auto corner = [&particle](std::int16_t dx, std::int16_t dz) {
    return WeatherPoint{.x = AddHalf(particle.x, dx),
                        .y = particle.y,
                        .z = AddHalf(particle.z, dz)};
  };
  const WeatherQuadProjection quad =
      render.ProjectQuad({corner(-kSplashHalfSize, -kSplashHalfSize),
                          corner(kSplashHalfSize, -kSplashHalfSize),
                          corner(-kSplashHalfSize, kSplashHalfSize),
                          corner(kSplashHalfSize, kSplashHalfSize)});
  const std::int32_t depth =
      Math::WrappingSubtract(quad.depth, kSplashDepthBias);
  if (depth > 0 && depth < kFarDepth) {
    render.Submit(depth, SplashPacket(quad, particle.sway_phase));
    if (AdvanceSettled(frame, particle)) return;
  }
  Respawn(pool, particle, area);
}

[[nodiscard]] WeatherPacket FlamePacket(
    const std::array<WeatherScreenPoint, kWeatherQuadCorners>& corners) {
  return PacketOf({ColorWord(kFlameEdge, kGradientQuadCode),
                   PointWord(corners[0]), ColorWord(kFlameCore, 0),
                   PointWord(corners[1]), ColorWord(kFlameTip, 0),
                   PointWord(corners[2]), ColorWord(kFlameEdge, 0),
                   PointWord(corners.back())});
}

void FallSnow(WeatherPoolHost& pool, WeatherRenderHost& render,
              const WeatherFrame& frame, WeatherParticle& particle,
              std::size_t index,
              const std::optional<Field::WeatherArea>& area) {
  for (std::int32_t tick = 0; tick < TicksOf(frame); ++tick) {
    if (particle.fall_shift != 0) {
      particle.y =
          AddHalf(particle.y, ShiftLeft(frame.velocity.y, particle.fall_shift));
      continue;
    }
    Drift(pool, particle, frame.velocity.x >> kDriftShift,
          frame.velocity.z >> kDriftShift);
    particle.y = AddHalf(particle.y, frame.velocity.y >> kDriftShift);
  }
  particle.fall_shift = kUndrawnFallShift;
  const WeatherProjection flake = render.Project(PositionOf(particle));
  if (!InFallRange(flake)) {
    Respawn(pool, particle, area);
  } else if (flake.depth >= kNearDepth) {
    const bool odd = IsOdd(index);
    if (OnScreen(flake.screen)) {
      render.Submit(flake.depth, TilePacket(flake.screen.x, flake.screen.y,
                                            odd ? 2 : 1, Gray(kFlakeShade)));
      particle.fall_shift = 0;
    }
    const auto angle = static_cast<std::int32_t>(index) << kSpeedAngleShift;
    const std::int32_t x =
        flake.screen.x + (pool.Sine(angle) >> kFlakeOffsetShift);
    const std::int32_t y =
        flake.screen.y - (pool.Cosine(angle) >> kFlakeOffsetShift);
    if (y > 0 && y < kScreenHeight && x > 0 && x < kScreenWidth) {
      render.Submit(flake.depth,
                    TilePacket(x, y, odd ? 1 : 2, Gray(kFlakeShade)));
      particle.fall_shift = 0;
    }
    if (particle.fall_shift == 0) {
      render.Submit(flake.depth, TexturePagePacket(kSnowTexturePage));
    }
  }
  if (particle.y <= 0) return;
  if (particle.fall_shift == 0 && IsOdd(index)) {
    particle.opaque_c = 1;
    particle.sway_phase = 0;
    particle.y = FloorOf(render, particle);
    return;
  }
  Respawn(pool, particle, area);
}

void SettledSnow(WeatherPoolHost& pool, WeatherRenderHost& render,
                 const WeatherFrame& frame, WeatherParticle& particle,
                 const std::optional<Field::WeatherArea>& area) {
  const WeatherProjection flake = render.Project(PositionOf(particle));
  if (OnScreen(flake.screen) && flake.depth >= kNearDepth) {
    const std::int32_t shade =
        kSettledShade - (particle.sway_phase * kSettledShadeStep);
    render.Submit(flake.depth,
                  TilePacket(flake.screen.x, flake.screen.y, 2, Gray(shade)));
    render.Submit(flake.depth, TexturePagePacket(kAdditiveTexturePage));
    if (AdvanceSettled(frame, particle)) return;
  }
  Respawn(pool, particle, area);
}

[[nodiscard]] WeatherTransform DriftOf(const WeatherTransform& origin,
                                       const WeatherTransform& velocity) {
  return {.x = Math::WrappingSubtract(velocity.x, origin.x) >> kDriftShift,
          .y = Math::WrappingSubtract(velocity.y, origin.y) >> kDriftShift,
          .z = Math::WrappingSubtract(velocity.z, origin.z) >> kDriftShift};
}

[[nodiscard]] std::int16_t WrapScreen(std::int16_t value, std::int32_t extent) {
  if (value < 0) value = AddHalf(value, extent);
  if (value > extent) value = AddHalf(value, -extent);
  return value;
}

void DriftScreen(WeatherPoolHost& pool, const WeatherTransform& drift,
                 WeatherParticle& particle) {
  Drift(pool, particle, drift.x, drift.z);
  particle.y = AddHalf(particle.y, drift.y);
  particle.z = static_cast<std::int16_t>(particle.z & kScreenDepthMask);
  particle.x = WrapScreen(particle.x, kScreenWidth);
  particle.y = WrapScreen(particle.y, kScreenHeight);
}

// The screen point of a coordinate at the particle's depth.
[[nodiscard]] std::int32_t Perspective(std::int16_t value, std::int16_t depth,
                                       std::int32_t centre) {
  const std::int32_t offset = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(value - centre) << kPerspectiveShift);
  const std::int32_t distance = Math::WrappingAdd(
      static_cast<std::int32_t>(static_cast<std::uint32_t>(depth)
                                << kDepthScaleShift),
      kViewDistance);
  return Math::WrappingAdd(Math::R3000Quotient(offset, distance), centre);
}

// The trail's far point: the previous point mirrored through the new one.
[[nodiscard]] std::int32_t TrailOf(std::int32_t point, std::int16_t previous) {
  return Math::WrappingSubtract(
      point, Math::WrappingMultiply(Math::WrappingSubtract(point, previous),
                                    kTrailScale));
}

void UpdateScreenFade(const WeatherFrame& frame, WeatherPoolState& state) {
  if (!frame.paused) {
    if (state.screen_fade != 0) {
      state.screen_fade =
          static_cast<std::uint8_t>(state.screen_fade - kFadeStep);
    }
  } else if (state.screen_fade < kFadeLimit) {
    state.screen_fade =
        static_cast<std::uint8_t>(state.screen_fade + kFadeStep);
  }
}
}  // namespace

// decomp: BATTLE.PRG func_8008F440 (146C.c:17758).
bool DrawRainParticles(WeatherPoolHost& pool, WeatherRenderHost& render,
                       const WeatherFrame& frame,
                       std::span<WeatherParticle> particles, std::int16_t count,
                       const std::optional<Field::WeatherArea>& area) {
  render.BeginProjection();
  const std::span<WeatherParticle> drawn = DrawnOf(particles, count);
  for (std::size_t index = 0; index < drawn.size(); ++index) {
    WeatherParticle& particle = drawn[index];
    if (particle.opaque_c == 0) {
      FallRain(pool, render, frame, particle, index, area);
    } else {
      SplashRain(pool, render, frame, particle, area);
    }
  }
  return Fits(particles, count);
}

// decomp: BATTLE.PRG func_8008FA14 (146C.c:17878).
WeatherParticle ResetRisingEmberParticle(WeatherPoolHost& pool,
                                         WeatherParticle particle,
                                         const Field::WeatherArea& area) {
  particle.x = Half(Math::WrappingAdd(
      area.origin_x, Math::R3000Remainder(pool.Random(), area.extent_x)));
  particle.z = Half(Math::WrappingAdd(
      area.origin_z, Math::R3000Remainder(pool.Random(), area.extent_z)));
  particle.y = Half((pool.Random() % kEmberHeightBand) + kEmberDepth);
  particle.opaque_c = 0;
  particle.speed = static_cast<std::uint8_t>(
      (static_cast<std::uint32_t>(pool.Random()) & kEmberSpeedMask) +
      kEmberMinimumSpeed);
  return particle;
}

// decomp: BATTLE.PRG func_8008F9A4 (146C.c:17864).
bool InitializeRisingEmberParticles(WeatherPoolHost& pool,
                                    WeatherPoolState& state,
                                    std::span<WeatherParticle> particles,
                                    const Field::WeatherArea& area,
                                    std::int32_t count, std::int32_t kind) {
  state.kind = static_cast<std::uint8_t>(static_cast<std::uint32_t>(kind));
  state.particle_capacity = Half(count);
  state.particle_count = Half(count);
  for (WeatherParticle& particle : DrawnOf(particles, count)) {
    particle = ResetRisingEmberParticle(pool, particle, area);
  }
  return Fits(particles, count);
}

// decomp: BATTLE.PRG func_8008FAC8 (146C.c:17889).
bool DrawRisingEmberParticles(WeatherPoolHost& pool, WeatherRenderHost& render,
                              const WeatherFrame& frame,
                              std::span<WeatherParticle> particles,
                              std::int16_t count,
                              const Field::WeatherArea& area) {
  render.BeginProjection();
  for (WeatherParticle& particle : DrawnOf(particles, count)) {
    if (particle.opaque_c != 0) continue;
    if (!frame.paused) {
      particle.y = Half(Math::WrappingSubtract(
          particle.y,
          Math::WrappingMultiply(particle.speed, frame.tick_speed)));
      if (particle.y < kEmberCeiling) {
        particle = ResetRisingEmberParticle(pool, particle, area);
      }
    }
    const WeatherProjection ember = render.Project(PositionOf(particle));
    if (!OnScreen(ember.screen) || ember.depth < kEmberNearDepth ||
        ember.depth >= kFarDepth) {
      continue;
    }
    // The original moved the ember down for the flame's tip and back.
    particle.y = AddHalf(particle.y, kFlameLength);
    const WeatherProjection tip = render.Project(PositionOf(particle));
    particle.y = AddHalf(particle.y, -kFlameLength);
    const std::int32_t dx =
        (tip.screen.x - ember.screen.x) / kFlameWidthDivisor;
    const std::int32_t dy =
        (tip.screen.y - ember.screen.y) / kFlameWidthDivisor;
    const WeatherScreenPoint base = ember.screen;
    render.Submit(
        tip.depth,
        FlamePacket({WeatherScreenPoint{.x = Half(base.x + dx + dy),
                                        .y = Half(base.y + dy - dx)},
                     base, tip.screen,
                     WeatherScreenPoint{.x = Half(base.x + dx - dy),
                                        .y = Half(dx + base.y + dy)}}));
    render.Submit(tip.depth, TexturePagePacket(kEmberTexturePage));
  }
  return Fits(particles, count);
}

// decomp: BATTLE.PRG func_8008FDC4 (146C.c:17938).
bool DrawSnowParticles(WeatherPoolHost& pool, WeatherRenderHost& render,
                       const WeatherFrame& frame,
                       std::span<WeatherParticle> particles, std::int16_t count,
                       const std::optional<Field::WeatherArea>& area) {
  render.BeginProjection();
  const std::span<WeatherParticle> drawn = DrawnOf(particles, count);
  for (std::size_t index = 0; index < drawn.size(); ++index) {
    WeatherParticle& particle = drawn[index];
    if (particle.opaque_c == 0) {
      FallSnow(pool, render, frame, particle, index, area);
    } else {
      SettledSnow(pool, render, frame, particle, area);
    }
  }
  return Fits(particles, count);
}

// decomp: BATTLE.PRG func_80090434 (146C.c:18072).
bool DrawScreenWeatherParticles(WeatherPoolHost& pool,
                                WeatherRenderHost& render,
                                const WeatherFrame& frame,
                                const WeatherPoint& reference,
                                WeatherPoolState& state,
                                std::span<WeatherParticle> particles) {
  render.BeginProjection();
  const WeatherTransform origin = render.Transform(reference);
  const WeatherTransform velocity = render.Transform(
      {.x = frame.velocity.x, .y = frame.velocity.y, .z = frame.velocity.z});
  const WeatherTransform drift = DriftOf(origin, velocity);
  UpdateScreenFade(frame, state);
  const std::span<WeatherParticle> drawn =
      DrawnOf(particles, state.particle_count);
  for (std::size_t index = 0; index < drawn.size(); ++index) {
    WeatherParticle& particle = drawn[index];
    for (std::int32_t tick = 0; tick < TicksOf(frame); ++tick) {
      DriftScreen(pool, drift, particle);
    }
    const std::int32_t x = Perspective(particle.x, particle.z, kScreenCentreX);
    const std::int32_t y = Perspective(particle.y, particle.z, kScreenCentreY);
    std::int32_t shade = ((kFarShade - particle.z) / kShadeDivisor) -
                         (state.screen_fade - kFadeBias);
    if (shade < 0) shade = 0;
    const std::int16_t size = IsOdd(index) ? 2 : 1;
    if (static_cast<std::uint32_t>(x) < kScreenWidth &&
        static_cast<std::uint32_t>(y) < kScreenHeight) {
      render.Submit(kScreenWeatherSlot, TilePacket(x, y, size, Gray(shade)));
    }
    if (static_cast<std::uint16_t>(particle.screen_x) < kScreenWidth &&
        static_cast<std::uint16_t>(particle.screen_y) < kScreenHeight) {
      shade >>= 1;
      render.Submit(
          kScreenWeatherSlot,
          TilePacket(particle.screen_x, particle.screen_y, size, Gray(shade)));
      shade >>= 1;
      render.Submit(
          kScreenWeatherSlot,
          TilePacket(TrailOf(x, particle.screen_x),
                     TrailOf(y, particle.screen_y), size, Gray(shade)));
    }
    if (!frame.paused) {
      particle.screen_x = Half(x);
      particle.screen_y = Half(y);
    }
  }
  render.Submit(kScreenWeatherSlot, TexturePagePacket(kScreenTexturePage));
  return Fits(particles, state.particle_count);
}
}  // namespace VS::Effects
