#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Effects/WeatherPool.hpp"
#include "Field/Background.hpp"

// The weather draws (BATTLE.PRG 146C.c:17758-18197): the rain's streaks and
// floor splashes, the rising embers' spawn, respawn and gradient flames, the
// snow's flake pairs and floor flakes, and the screen weather's parallax
// tiles. Each draw advances its particles by the frame's ticks, projects them
// and queues GPU packets into the frame's ordering table. The rules work on the
// typed particles of WeatherPool.hpp and reach the random generator and the
// sine table through WeatherPoolHost, and the GTE, the collision map, the floor
// heights and the ordering table through WeatherRenderHost. Projection results
// are inputs: the rules never compute screen coordinates or depths
// themselves. WeatherRenderLive.hpp binds them to the copied globals.
namespace VS::Effects {
// An SVECTOR's three halfwords.
struct WeatherPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;

  friend bool operator==(const WeatherPoint&, const WeatherPoint&) = default;
};

// A screen coordinate pair, one DVECTOR (the projected SXY word).
struct WeatherScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const WeatherScreenPoint&,
                         const WeatherScreenPoint&) = default;
};

// RotTransPers: the screen point and the ordering depth (SZ3 / 4) it returns.
struct WeatherProjection {
  WeatherScreenPoint screen;
  std::int32_t depth = 0;
};

inline constexpr std::size_t kWeatherQuadCorners = 4;

// RotTransPers4: the four screen points and the fourth corner's depth.
struct WeatherQuadProjection {
  std::array<WeatherScreenPoint, kWeatherQuadCorners> screens{};
  std::int32_t depth = 0;
};

// RotTrans: the transformed VECTOR's three words.
struct WeatherTransform {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;

  friend bool operator==(const WeatherTransform&,
                         const WeatherTransform&) = default;
};

// One GPU packet: `size` words after its tag (the tag's length byte). Bytes
// the original left unwritten (pads) are zero.
inline constexpr std::size_t kWeatherPacketMaxWords = 9;
struct WeatherPacket {
  std::array<std::uint32_t, kWeatherPacketMaxWords> words{};
  std::uint8_t size = 0;

  friend bool operator==(const WeatherPacket&, const WeatherPacket&) = default;
};

// The screen weather's ordering table entry.
inline constexpr std::int32_t kScreenWeatherSlot = 4;

// What the draws read from the frame: whether room updates are paused
// (D_800F1BA4), the tick speed (D_8005E24C) and the particle velocity
// (D_800F1D98[0..2]).
struct WeatherFrame {
  bool paused = false;
  std::int32_t tick_speed = 0;
  WeatherVelocity velocity{};
};

class WeatherRenderHost {
 public:
  WeatherRenderHost() = default;
  WeatherRenderHost(const WeatherRenderHost&) = delete;
  WeatherRenderHost& operator=(const WeatherRenderHost&) = delete;
  WeatherRenderHost(WeatherRenderHost&&) = delete;
  WeatherRenderHost& operator=(WeatherRenderHost&&) = delete;
  virtual ~WeatherRenderHost() = default;

  // SetRotMatrix and SetTransMatrix from the frame's view matrix.
  virtual void BeginProjection() = 0;
  // RotTransPers.
  [[nodiscard]] virtual WeatherProjection Project(
      const WeatherPoint& point) = 0;
  // RotTransPers4.
  [[nodiscard]] virtual WeatherQuadProjection ProjectQuad(
      const std::array<WeatherPoint, kWeatherQuadCorners>& corners) = 0;
  // RotTrans.
  [[nodiscard]] virtual WeatherTransform Transform(
      const WeatherPoint& point) = 0;
  // func_8008B764 on layer 0: the cell's surface kind (its first word's low
  // five bits), absent when there is no cell.
  [[nodiscard]] virtual std::optional<std::uint8_t> CollisionSurface(
      std::int32_t tile_x, std::int32_t tile_z) = 0;
  // func_8008DC7C.
  [[nodiscard]] virtual std::int32_t FloorHeight(std::int16_t x,
                                                 std::int16_t z) = 0;
  // addPrim of `packet` in front of ordering table entry `slot`.
  virtual void Submit(std::int32_t slot, const WeatherPacket& packet) = 0;
};

// func_8008F440: rain. A falling drop moves by the velocity, shifted by its
// fall shift, for each two ticks, and is drawn as a streak back along the
// velocity; a drop on an odd index that hits a floor becomes a splash, the
// others respawn (func_80090A1C). A splash plays four frames of its texture
// and respawns. False (refused) when `count` exceeds `particles`; only the
// particles that fit are drawn.
[[nodiscard]] bool DrawRainParticles(
    WeatherPoolHost& pool, WeatherRenderHost& render, const WeatherFrame& frame,
    std::span<WeatherParticle> particles, std::int16_t count,
    const std::optional<Field::WeatherArea>& area);

// func_8008FA14: respawns a rising ember at a random point of the room's
// weather area, 512 to 1023 below the origin, with a speed of 8 to 23. The
// original read the area without checking for a room; the remainder by a zero
// extent leaves the random value, as the R3000's DIV does.
[[nodiscard]] WeatherParticle ResetRisingEmberParticle(
    WeatherPoolHost& pool, WeatherParticle particle,
    const Field::WeatherArea& area);

// func_8008F9A4: sets the kind and both counts and respawns `count` embers in
// order. False (refused) when the embers do not fit `particles`; the state is
// set and only the embers that fit are respawned.
[[nodiscard]] bool InitializeRisingEmberParticles(
    WeatherPoolHost& pool, WeatherPoolState& state,
    std::span<WeatherParticle> particles, const Field::WeatherArea& area,
    std::int32_t count, std::int32_t kind);

// func_8008FAC8: embers rise by their speed times the tick speed, respawn
// above -1024, and are drawn as a gradient flame from the ember to a point
// 192 below it, at the lower point's depth. Refused as the rain draw.
[[nodiscard]] bool DrawRisingEmberParticles(
    WeatherPoolHost& pool, WeatherRenderHost& render, const WeatherFrame& frame,
    std::span<WeatherParticle> particles, std::int16_t count,
    const Field::WeatherArea& area);

// func_8008FDC4: snow. A falling flake drifts with the velocity / 8 and a
// random sway, and is drawn as two tiles; a flake on an odd index that reaches
// the floor settles there and fades out over four frames. Refused as the rain
// draw.
[[nodiscard]] bool DrawSnowParticles(
    WeatherPoolHost& pool, WeatherRenderHost& render, const WeatherFrame& frame,
    std::span<WeatherParticle> particles, std::int16_t count,
    const std::optional<Field::WeatherArea>& area);

// func_80090434: the screen weather. Particles drift in screen space by the
// view's rotation of the velocity, relative to `reference` (the original read
// the first background reference VECTOR as an SVECTOR: its x word's two
// halves and its y word's low half), wrap at the screen's edges and are drawn
// at a perspective of their depth, with a fading trail to their previous
// point. The screen fade (`state.screen_fade`) eases in while room updates
// are paused and out otherwise. Refused as the rain draw for
// `state.particle_count`.
[[nodiscard]] bool DrawScreenWeatherParticles(
    WeatherPoolHost& pool, WeatherRenderHost& render, const WeatherFrame& frame,
    const WeatherPoint& reference, WeatherPoolState& state,
    std::span<WeatherParticle> particles);
}  // namespace VS::Effects
