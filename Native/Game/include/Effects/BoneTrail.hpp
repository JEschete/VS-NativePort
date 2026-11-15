#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Actors/ModelDrawing.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/Templates/RingCore.hpp"

struct Effects_EffectInstance;
struct Effects_RendererSlot;

// Bone trails (BATTLE.PRG 5BF94.c:4898-5000 and 5666-5760; effect 54): a
// ribbon behind one bone of an actor, usually its weapon. Every tick samples
// the bone's base and tip into an eight-entry history; from the fourth sample
// on, Lagrange-smoothed quads join consecutive half samples, brightest at the
// newest. A trail adds or subtracts a palette colour through a small texture,
// or distorts the frame on display behind it. Every function works on value
// records; the bones, the GTE, the palettes, the heap, the effect chains and
// the frame's primitive buffer are reached through hosts (BoneTrailLive.hpp
// binds them).
namespace VS::Effects {
inline constexpr std::size_t kTrailHistory = 8;
inline constexpr std::size_t kTrailCorners = 4;
// The life of a trail that only a fade or a stop ends, and of a fading one.
inline constexpr std::uint8_t kEndlessTrailLife = 0xFF;
inline constexpr std::uint8_t kFadingTrailLife = 8;
// The texture number that distorts instead of drawing a texture, and the
// colour number that subtracts instead of adding.
inline constexpr std::uint8_t kDistortionTrailTexture = 6;
inline constexpr std::uint8_t kSubtractiveTrailColor = 7;
inline constexpr std::uint16_t kBoneTrailEffectId = 54;
// The GPU codes of a textured trail quad (semi-transparent) and of a
// distortion quad (opaque); both are Gouraud-shaded, textured and blended.
inline constexpr std::uint8_t kTrailQuadCode = Templates::kRingQuadCode;
inline constexpr std::uint8_t kDistortionQuadCode = 0x3C;
inline constexpr std::size_t kAdditiveTrailColors = 8;
inline constexpr std::size_t kDistortionTrailColors = 7;

// The renderer parameter of a trail (Battle_EffectTrailConfig): actor and
// bone bytes, colour and texture nibbles, and the remaining-life byte.
struct BoneTrailConfig {
  std::uint8_t actor{};
  std::uint8_t bone{};
  std::uint8_t color{};
  std::uint8_t texture{};
  std::uint8_t life{};

  [[nodiscard]] static constexpr BoneTrailConfig Unpack(
      std::uint32_t packed) noexcept {
    constexpr std::uint32_t kByte = 0xFF;
    constexpr std::uint32_t kNibble = 0xF;
    return {
        .actor = static_cast<std::uint8_t>(packed & kByte),
        .bone = static_cast<std::uint8_t>((packed >> kBoneShift) & kByte),
        .color = static_cast<std::uint8_t>((packed >> kColorShift) & kNibble),
        .texture =
            static_cast<std::uint8_t>((packed >> kTextureShift) & kNibble),
        .life = static_cast<std::uint8_t>(packed >> kLifeShift)};
  }
  [[nodiscard]] constexpr std::uint32_t Pack() const noexcept {
    constexpr std::uint32_t kNibble = 0xF;
    return actor | (static_cast<std::uint32_t>(bone) << kBoneShift) |
           ((color & kNibble) << kColorShift) |
           ((texture & kNibble) << kTextureShift) |
           (static_cast<std::uint32_t>(life) << kLifeShift);
  }

  friend bool operator==(const BoneTrailConfig&,
                         const BoneTrailConfig&) = default;

 private:
  static constexpr int kBoneShift = 8;
  static constexpr int kColorShift = 16;
  static constexpr int kTextureShift = 20;
  static constexpr int kLifeShift = 24;
};

// A camera-relative point (an SVECTOR without its pad).
struct TrailPoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};

  friend bool operator==(const TrailPoint&, const TrailPoint&) = default;
};

// The bone's base and tip at one moment.
struct TrailEdge {
  TrailPoint base{};
  TrailPoint tip{};

  friend bool operator==(const TrailEdge&, const TrailEdge&) = default;
};

// The renderer state (effectTrailState): the configuration, the history ring
// and the segment being drawn.
struct BoneTrail {
  BoneTrailConfig config{};
  // The newest history entry.
  std::uint32_t newest{};
  // Samples taken; the original's int, wrapping.
  std::int32_t age{};
  std::array<TrailEdge, kTrailHistory> history{};
  // The older and newer edge of the segment DrawTrailSegment draws.
  TrailEdge previous{};
  TrailEdge current{};

  friend bool operator==(const BoneTrail&, const BoneTrail&) = default;
};

// The colour weights of a segment's base corners and tip corners.
struct TrailSegmentWeights {
  std::int32_t base{};
  std::int32_t tip{};

  friend bool operator==(const TrailSegmentWeights&,
                         const TrailSegmentWeights&) = default;
};

// RotAverage4 of a segment: the corners' packed screen positions and their
// average ordering depth.
struct TrailProjection {
  std::array<std::uint32_t, kTrailCorners> screens{};
  std::int32_t depth{};
};

// The module's two colour tables (D_800EC2E8 and D_800EC308).
struct TrailPalettes {
  std::array<Templates::RingColor, kAdditiveTrailColors> additive{};
  std::array<Templates::RingColor, kDistortionTrailColors> distortion{};
};

enum class TrailPalette : std::uint8_t { kAdditive, kDistortion };

// Colour `index` of `palette`. The original placed the distortion table right
// after the additive one, so additive numbers 8-14 read distortion colours
// 0-6; additive 15 and distortion numbers past 6 read beyond both tables and
// have no colour.
[[nodiscard]] std::optional<Templates::RingColor> TrailPaletteColor(
    const TrailPalettes& palettes, TrailPalette palette,
    std::uint8_t index) noexcept;

// func_800CF92C (5BF94.c:5714): the texel under the packed screen position
// moved by the offsets, each axis kept within 1..254.
[[nodiscard]] Templates::RingUv ClampTrailTexturePoint(
    std::int32_t u_offset, std::int32_t v_offset,
    std::uint32_t screen) noexcept;

// func_800CF988 (5BF94.c:5738): points the quad at the frame on display
// behind it, offset by `u_offset` and `v_offset`. The texture page is the
// 15-bit page holding the quad's leftmost corner (at most x 320) in the
// buffer the frame is not drawing into, with `flags`; `draw_buffer` is the
// buffer being drawn (Render_frameBuf's low bit).
void MapTrailFramebufferTexture(Templates::RingQuad& frame_quad,
                                std::int32_t u_offset, std::int32_t v_offset,
                                std::uint16_t flags, std::uint32_t draw_buffer);

// What DrawTrailSegment reaches outside the trail.
class TrailSegmentHost {
 public:
  virtual ~TrailSegmentHost() = default;
  // The corners through the loaded view (RotAverage4).
  [[nodiscard]] virtual TrailProjection Project(
      const std::array<TrailPoint, kTrailCorners>& corners) = 0;
  [[nodiscard]] virtual const TrailPalettes& Palettes() = 0;
  // The buffer being drawn: 0 or 1.
  [[nodiscard]] virtual std::uint32_t DrawBuffer() = 0;
  // Queues `quad` with GPU command `code` at ordering depth `depth`.
  virtual void Submit(const Templates::RingQuad& quad, std::uint8_t code,
                      std::int32_t depth) = 0;

 protected:
  TrailSegmentHost() = default;
  TrailSegmentHost(const TrailSegmentHost&) = default;
  TrailSegmentHost& operator=(const TrailSegmentHost&) = default;
  TrailSegmentHost(TrailSegmentHost&&) = default;
  TrailSegmentHost& operator=(TrailSegmentHost&&) = default;
};

enum class TrailSegmentDraw : std::uint8_t {
  kDrawn,
  // The average depth is outside 17..2047.
  kOutsideDepth,
  // The trail's colour number has no palette entry (TrailPaletteColor).
  kUnknownColor,
};

// func_800CDCBC (5BF94.c:4898): draws the quad from the previous to the
// current edge. A distortion trail shades its corners by the distortion
// colour times the weight less 128 and maps the frame on display, shifted by
// the base weight; any other trail shades by the additive colour times the
// weight over 8 and draws its texture, 40 texels apart by column and 10 by
// row, three per column. Colour 7 subtracts instead of adding.
TrailSegmentDraw DrawTrailSegment(const BoneTrail& trail,
                                  TrailSegmentWeights weights,
                                  TrailSegmentHost& host);

enum class BoneTrailRefusal : std::uint8_t {
  // A tick or fade of a slot without a trail; the original wrote through
  // the null state.
  kNoTrail,
  // The heap had no room for a new trail; the original used the null block.
  kNoRoom,
};

// What UpdateBoneTrail reaches outside the trail.
class BoneTrailHost {
 public:
  virtual ~BoneTrailHost() = default;
  // The slot's trail; null before a start or after a stop.
  [[nodiscard]] virtual BoneTrail* Trail() = 0;
  // A new default trail held by the slot; null when the heap has no room.
  [[nodiscard]] virtual BoneTrail* Allocate() = 0;
  // Frees the slot's trail, when it has one, and clears the slot.
  virtual void Release() = 0;
  // Stores `endpoint` of `bone` of `actor` in `point`; a bone the actor
  // lacks leaves `point` as it was.
  virtual void SampleBone(std::uint8_t actor, std::uint8_t bone,
                          Actors::BoneEndpoint endpoint, TrailPoint& point) = 0;
  // Loads the frame's view matrix into the GTE.
  virtual void LoadView() = 0;
  virtual void DrawSegment(const BoneTrail& trail,
                           TrailSegmentWeights weights) = 0;
  virtual void Refuse(RendererCommand command, BoneTrailRefusal refusal) = 0;

 protected:
  BoneTrailHost() = default;
  BoneTrailHost(const BoneTrailHost&) = default;
  BoneTrailHost& operator=(const BoneTrailHost&) = default;
  BoneTrailHost(BoneTrailHost&&) = default;
  BoneTrailHost& operator=(BoneTrailHost&&) = default;
};

// func_800CE174 (5BF94.c:4958): the trail renderer. A start allocates the
// trail from `parameter`; an update samples the bone and, from the fourth
// sample, draws one segment at phase -1/2 then two per history span, up to
// five spans, with weights falling by one per segment; a fade sets the life
// to 8; a stop frees the trail. An update spends one life unless it is
// endless, wrapping 0 to 255 as the original did. False ends the renderer:
// a stop, a life run out, or a refusal; other commands keep it.
bool UpdateBoneTrail(RendererCommand command, std::uint32_t parameter,
                     BoneTrailHost& host);

// The secondary effect request func_800CF7A8 begins: effect 54 with one
// target, from the origin to the origin, its source and target being
// positions, with the packed trail as the effect's parameter.
struct BoneTrailRequest {
  std::uint16_t effect_id{};
  std::uint8_t target_count{};
  std::uint8_t other_count{};
  std::uint16_t source_type{};
  std::uint16_t target_type{};
  std::uint32_t parameter{};

  friend bool operator==(const BoneTrailRequest&,
                         const BoneTrailRequest&) = default;
};

// func_800CF7A8 (5BF94.c:5666): the request for an endless trail; the actor
// and bone keep their low byte, the colour and texture their low nibble.
[[nodiscard]] BoneTrailRequest MakeBoneTrailRequest(
    std::int32_t actor, std::int32_t bone, std::int32_t color,
    std::int32_t texture) noexcept;
// func_8006C480 (146C.c:1923): the request for a weapon trail, colour 0 and
// texture 0.
[[nodiscard]] BoneTrailRequest MakeWeaponTrailRequest(
    std::int32_t actor, std::int32_t bone) noexcept;

// The secondary effect instances and their renderers, in chain order.
class BoneTrailChains {
 public:
  virtual ~BoneTrailChains() = default;
  [[nodiscard]] virtual Effects_EffectInstance* FirstInstance() = 0;
  [[nodiscard]] virtual Effects_EffectInstance* NextInstance(
      Effects_EffectInstance& instance) = 0;
  [[nodiscard]] virtual Effects_RendererSlot* FirstRenderer(
      Effects_EffectInstance& instance) = 0;
  [[nodiscard]] virtual Effects_RendererSlot* NextRenderer(
      Effects_RendererSlot& slot) = 0;
  // Whether the slot runs the bone trail renderer.
  [[nodiscard]] virtual bool RunsBoneTrail(Effects_RendererSlot& slot) = 0;
  // Sends the fade command to the slot's renderer.
  virtual void Fade(Effects_RendererSlot& slot) = 0;

 protected:
  BoneTrailChains() = default;
  BoneTrailChains(const BoneTrailChains&) = default;
  BoneTrailChains& operator=(const BoneTrailChains&) = default;
  BoneTrailChains(BoneTrailChains&&) = default;
  BoneTrailChains& operator=(BoneTrailChains&&) = default;
};

// func_800CF830 (5BF94.c:5681): fades every bone trail of every secondary
// instance. Each next slot and instance is read after the fade.
void FadeBoneTrails(BoneTrailChains& chains);
// func_8006C4A4 (146C.c:1925): FadeBoneTrails for the weapon trail commands,
// whose actor and bone arguments the original ignored.
void FadeWeaponTrails(BoneTrailChains& chains);
}  // namespace VS::Effects
