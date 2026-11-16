#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/HudOrchestration.hpp"

// Battle actor markers (the awareness and talk bubbles above actors), the
// arrow that points at an actor outside the screen, and the ability-timing
// cue's anchor. The functions reach the projection, gimmick and draw services
// through ActorMarkerWorld and keep the original packed arithmetic internally.
namespace VS::Battle {
// A screen position; the original packs it as x | y << 16.
struct MarkerScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
  friend bool operator==(MarkerScreenPoint, MarkerScreenPoint) = default;
};

// The actor's target point projected to the screen with its depth.
struct ProjectedMarkerAnchor {
  MarkerScreenPoint screen;
  std::int16_t depth = 0;
};

// A camera-relative actor point before the view projection.
struct MarkerWorldPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  friend bool operator==(MarkerWorldPoint, MarkerWorldPoint) = default;
};

struct MarkerSize {
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  friend bool operator==(MarkerSize, MarkerSize) = default;
};

// A sprite's texture word: u, v and the CLUT.
struct MarkerTexture {
  std::uint8_t u = 0;
  std::uint8_t v = 0;
  std::uint16_t clut = 0;
  friend bool operator==(MarkerTexture, MarkerTexture) = default;
};

// A brightness-0x80 sprite linked in front of scene ordering entry
// `ordering_slot`; a draw mode replaces the packet's draw-mode word.
struct ActorMarkerSprite {
  int ordering_slot = 0;
  MarkerScreenPoint position;
  MarkerSize size;
  MarkerTexture texture;
  std::optional<std::uint32_t> draw_mode;
};

struct MarkerFrameVertex {
  MarkerScreenPoint position;
  std::uint8_t u = 0;
  std::uint8_t v = 0;
};

inline constexpr std::size_t kMarkerQuadVertices = 4;

// The bubble frame: a textured quad (CLUT 0x373E, texture page 0x17).
struct ActorMarkerFrame {
  int ordering_slot = 0;
  std::array<MarkerFrameVertex, kMarkerQuadVertices> vertices{};
};

// The off-screen arrow quad, vertices in packet order. It is linked four
// entries in front of the scene ordering table.
struct OffscreenActorArrow {
  std::array<MarkerScreenPoint, kMarkerQuadVertices> vertices{};
};
inline constexpr int kOffscreenArrowOrderingSlot = -4;

// Edges an actor's marker was clamped to (the arrow's direction flags).
enum OffscreenEdge : std::uint8_t {
  kOffscreenLeft = 1,
  kOffscreenRight = 2,
  kOffscreenTop = 4,
  kOffscreenBottom = 8,
};

class ActorMarkerWorld {
 public:
  virtual ~ActorMarkerWorld() = default;
  // BATTLE.PRG func_800A1280 with tag 251: the actor's projected target point,
  // or nothing when the actor has none.
  virtual std::optional<ProjectedMarkerAnchor> ProjectActorAnchor(
      int actor) = 0;
  // BATTLE.PRG func_800A190C with tag 255: the actor's camera-relative anchor.
  virtual std::optional<MarkerWorldPoint> ActorAnchorPosition(int actor) = 0;
  // The frame's view matrix loaded for rotation and translation, then
  // RotTransPers: the point's screen position.
  virtual MarkerScreenPoint ProjectThroughView(MarkerWorldPoint point) = 0;
  // BATTLE.PRG func_8009F858: the actor's gimmick subtype, negative for none.
  virtual int GimmickSubtype(int actor) = 0;
  // False when the draw was refused.
  virtual bool DrawSprite(const ActorMarkerSprite& sprite) = 0;
  virtual bool DrawFrame(const ActorMarkerFrame& frame) = 0;
  virtual bool DrawArrow(const OffscreenActorArrow& arrow) = 0;
};

// D_800F2260 (marker kind per actor), D_800F2274 (one bit per actor: the
// marker is drawn on the actor's far side) and D_800F2270 (markers hidden).
struct ActorMarkerState {
  std::array<std::uint8_t, kActorMarkerCount> kinds{};
  std::uint32_t sides = 0;
  std::int16_t hidden = 0;
};

// D_800F2280 (anchor, depth in vz) and D_800F2272 (remaining frames).
struct AbilityTimingCue {
  ProjectedMarkerAnchor anchor;
  std::int16_t frames = 0;
};

inline constexpr std::size_t kMarkerFramePacketWords = 10;
using MarkerFramePacket = std::array<std::uint32_t, kMarkerFramePacketWords>;
using OffscreenArrowPacket = std::span<std::uint32_t, kMarkerFramePacketWords>;

[[nodiscard]] std::uint32_t PackMarkerPoint(MarkerScreenPoint point);
[[nodiscard]] MarkerScreenPoint UnpackMarkerPoint(std::uint32_t packed);
[[nodiscard]] std::uint32_t PackMarkerSize(MarkerSize size);
[[nodiscard]] std::uint32_t PackMarkerTexture(MarkerTexture texture);
[[nodiscard]] MarkerTexture UnpackMarkerTexture(std::uint32_t packed);

// The frame's POLY_FT4 packet linked in front of `next_link`.
[[nodiscard]] MarkerFramePacket BuildActorMarkerFramePacket(
    const ActorMarkerFrame& frame, std::uint32_t next_link);
// Writes the arrow's fields into the packet storage at the primitive cursor
// and links it in front of `next_link`. The colour bytes and the halfwords
// after the third and fourth texture coordinates keep the storage's contents.
void WriteOffscreenArrowPacket(OffscreenArrowPacket storage,
                               const OffscreenActorArrow& arrow,
                               std::uint32_t next_link);

// False when `actor` is outside the sixteen marker slots or a draw was
// refused; the original's early returns are accepted.
[[nodiscard]] bool DrawActorMarker(ActorMarkerWorld& world,
                                   ActorMarkerState& state, int actor,
                                   int kind);
[[nodiscard]] bool DrawActorMarkers(ActorMarkerWorld& world,
                                    ActorMarkerState& state);
// `direction` is the OffscreenEdge flags; other values draw nothing.
[[nodiscard]] bool DrawOffscreenActorArrow(ActorMarkerWorld& world,
                                           MarkerScreenPoint position,
                                           int direction);
void ShowAbilityTimingCue(ActorMarkerWorld& world, bool display_enabled,
                          AbilityTimingCue& cue, int duration_frames);
}  // namespace VS::Battle
