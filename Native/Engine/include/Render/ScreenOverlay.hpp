#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/DisplayControl.hpp"

// The full-screen overlays drawn over the scene: the framebuffer effect, which
// redraws the current or previous frame in five tinted strips that can be
// rotated, scaled and offset, and the room transition's fades and wipe. The
// policies plan packets as values and hand them to a FramebufferEffectPackets
// or ScreenTransitionTarget, so they reach no GPU packet, heap block,
// frame-buffer global or module address directly. ScreenOverlayLive.hpp binds
// them to the game's packets and transition globals.
namespace VS::Render {
inline constexpr std::size_t kFramebufferEffectStripCount = 5;
// One packet per strip and frame buffer: slot = frame buffer * 5 + strip.
inline constexpr std::size_t kFramebufferEffectSlotCount(
    kFramebufferEffectStripCount* kFrameBufferCount);
inline constexpr std::int32_t kFramebufferEffectStripWidth = 64;
inline constexpr std::int32_t kFramebufferEffectStripHeight = 223;
// Ordering-table entries; a deeper request is ignored.
inline constexpr std::uint32_t kFramebufferEffectDepthLimit = 0x800;
// Q12 one: the scale that leaves the frame its size.
inline constexpr std::int32_t kFramebufferEffectUnitScale = 4096;
inline constexpr std::uint8_t kFramebufferEffectNeutralTint = 0x80;
inline constexpr std::size_t kQuadCornerCount = 4;

template <typename Value>
using PerFramebufferEffectSlot = std::array<Value, kFramebufferEffectSlotCount>;

// The original state word. kStarting shows the effect from the next frame;
// kStopping and kReleasing draw nothing, and the packets are released as
// kReleasing ends.
enum class FramebufferEffectPhase : std::uint8_t {
  kOff,
  kStarting,
  kDrawing,
  kStopping,
  kReleasing,
};

struct FramebufferEffectTint {
  std::uint8_t red = kFramebufferEffectNeutralTint;
  std::uint8_t green = kFramebufferEffectNeutralTint;
  std::uint8_t blue = kFramebufferEffectNeutralTint;

  friend bool operator==(const FramebufferEffectTint&,
                         const FramebufferEffectTint&) = default;
};

// An offset in pixels or a Q12 scale.
struct FramebufferEffectPair {
  std::int32_t x = 0;
  std::int32_t y = 0;

  friend bool operator==(const FramebufferEffectPair&,
                         const FramebufferEffectPair&) = default;
};

struct FramebufferEffectSettings {
  // 4096 units per turn about the screen centre.
  std::int16_t roll = 0;
  FramebufferEffectPair scale{kFramebufferEffectUnitScale,
                              kFramebufferEffectUnitScale};
  FramebufferEffectPair offset{};
  FramebufferEffectTint tint{};
  // The word the script set; the texture pages use its low two bits.
  std::int32_t blend_mode = 0;
  // Kept for the scripts that set it; the effect never reads it.
  std::int32_t parameter = 0;
  std::uint32_t depth = 0;
  // Nonzero draws the frame shown last instead of the one being drawn.
  std::int32_t uses_previous_frame = 0;

  friend bool operator==(const FramebufferEffectSettings&,
                         const FramebufferEffectSettings&) = default;
};

struct FramebufferEffect {
  FramebufferEffectPhase phase = FramebufferEffectPhase::kOff;
  FramebufferEffectSettings settings{};

  friend bool operator==(const FramebufferEffect&,
                         const FramebufferEffect&) = default;
};

struct ScreenVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const ScreenVertex&, const ScreenVertex&) = default;
};

// kTransformed when the effect is rolled or scaled: each strip is a textured
// quad through the GTE transform. kFlat draws each strip as a sprite.
enum class FramebufferEffectLayout : std::uint8_t { kFlat, kTransformed };

struct FramebufferEffectStrip {
  std::size_t slot = 0;
  std::uint16_t texture_page = 0;
  // kTransformed: top-left, top-right, bottom-left, bottom-right.
  std::array<ScreenVertex, kQuadCornerCount> corners{};
  // kFlat: the sprite's top-left corner.
  ScreenVertex origin{};

  friend bool operator==(const FramebufferEffectStrip&,
                         const FramebufferEffectStrip&) = default;
};

struct FramebufferEffectFrame {
  FramebufferEffectLayout layout = FramebufferEffectLayout::kFlat;
  FramebufferEffectTint tint{};
  // The ordering-table entry the strips are added to.
  std::uint32_t depth = 0;
  std::array<FramebufferEffectStrip, kFramebufferEffectStripCount> strips{};

  friend bool operator==(const FramebufferEffectFrame&,
                         const FramebufferEffectFrame&) = default;
};

// What each packet slot holds before its first frame: the quad's left edge
// (it spans 64 pixels by 223 lines from there), the texel column of its right
// edge and its texture page. Drawing replaces the corners and page.
struct FramebufferEffectSlotTemplate {
  std::int16_t left = 0;
  std::uint8_t right_texel = 0;
  std::uint16_t texture_page = 0;

  friend bool operator==(const FramebufferEffectSlotTemplate&,
                         const FramebufferEffectSlotTemplate&) = default;
};

// The GTE rotation and translation the strips and the wipe are placed with.
class ScreenTransformer {
 public:
  ScreenTransformer() = default;
  ScreenTransformer(const ScreenTransformer&) = delete;
  ScreenTransformer& operator=(const ScreenTransformer&) = delete;
  ScreenTransformer(ScreenTransformer&&) = delete;
  ScreenTransformer& operator=(ScreenTransformer&&) = delete;
  virtual ~ScreenTransformer() = default;

  virtual void LoadTransform(const Core::Math::Transform3& transform) = 0;
  // (x, y, 0) through the loaded transform.
  [[nodiscard]] virtual Core::Math::Vec3 Transform(std::int16_t x,
                                                   std::int16_t y) = 0;
};

// The effect's packet block: kFramebufferEffectSlotCount quads, sprites and
// draw modes (0x2D0 bytes from the high end of the game heap).
class FramebufferEffectPackets {
 public:
  FramebufferEffectPackets() = default;
  FramebufferEffectPackets(const FramebufferEffectPackets&) = delete;
  FramebufferEffectPackets& operator=(const FramebufferEffectPackets&) = delete;
  FramebufferEffectPackets(FramebufferEffectPackets&&) = delete;
  FramebufferEffectPackets& operator=(FramebufferEffectPackets&&) = delete;
  virtual ~FramebufferEffectPackets() = default;

  [[nodiscard]] virtual bool Ready() const = 0;
  // Allocates the block and fills it from BuildFramebufferEffectTemplates;
  // false when there is no memory for it.
  virtual bool Acquire(const FramebufferEffectTint& tint) = 0;
  virtual void Release() = 0;
  // Drops the block without freeing it (start-up, over a fresh heap).
  virtual void Forget() = 0;
  virtual void Submit(const FramebufferEffectFrame& frame) = 0;
};

enum class FramebufferEffectDraw : std::uint8_t {
  kHidden,
  kDrawn,
  // kDrawing with no packet block: nothing is drawn.
  kNoPackets,
};

struct FramebufferEffectAdvance {
  FramebufferEffectPhase phase = FramebufferEffectPhase::kOff;
  bool release_packets = false;

  friend bool operator==(const FramebufferEffectAdvance&,
                         const FramebufferEffectAdvance&) = default;
};

void ResetFramebufferEffect(FramebufferEffect& effect) noexcept;
void InitializeFramebufferEffect(FramebufferEffect& effect,
                                 FramebufferEffectPackets& packets);
[[nodiscard]] PerFramebufferEffectSlot<FramebufferEffectSlotTemplate>
BuildFramebufferEffectTemplates() noexcept;

// The libgpu texture page of a 15-bit image at VRAM column x.
[[nodiscard]] std::uint16_t FramebufferEffectTexturePage(
    std::int32_t blend_mode, std::int32_t x) noexcept;
[[nodiscard]] bool FramebufferEffectIsTransformed(
    const FramebufferEffectSettings& settings) noexcept;
// Roll about the screen centre, offset and scale (z scale 0).
[[nodiscard]] Core::Math::Transform3 FramebufferEffectTransform(
    const FramebufferEffectSettings& settings) noexcept;
// The strips for frame buffer `frame_buffer & 1`. The transformer must hold
// FramebufferEffectTransform(settings).
[[nodiscard]] FramebufferEffectFrame PlanFramebufferEffectFrame(
    const FramebufferEffectSettings& settings, std::size_t frame_buffer,
    ScreenTransformer& transformer);
[[nodiscard]] FramebufferEffectAdvance AdvanceFramebufferEffectPhase(
    FramebufferEffectPhase phase) noexcept;
// In kDrawing loads the transform and submits the frame's strips; then
// advances the phase, releasing the packets as kReleasing ends.
FramebufferEffectDraw DrawFramebufferEffect(FramebufferEffect& effect,
                                            FramebufferEffectPackets& packets,
                                            ScreenTransformer& transformer,
                                            std::size_t frame_buffer);
// Enabling acquires the packets when there are none and starts the effect;
// when they could not be had it returns false and leaves the phase alone.
// Disabling starts the release.
bool SetFramebufferEffectEnabled(FramebufferEffect& effect,
                                 FramebufferEffectPackets& packets,
                                 bool enabled);

// The roll keeps the low 16 bits of `angle`.
void SetFramebufferEffectRoll(FramebufferEffectSettings& settings,
                              std::int32_t angle) noexcept;
void SetFramebufferEffectScale(FramebufferEffectSettings& settings,
                               FramebufferEffectPair scale) noexcept;
void SetFramebufferEffectTint(FramebufferEffectSettings& settings,
                              FramebufferEffectTint tint) noexcept;
void SetFramebufferEffectOffset(FramebufferEffectSettings& settings,
                                FramebufferEffectPair offset) noexcept;
void SetFramebufferEffectBlendMode(FramebufferEffectSettings& settings,
                                   std::int32_t blend_mode) noexcept;
void SetFramebufferEffectParameter(FramebufferEffectSettings& settings,
                                   std::int32_t parameter) noexcept;
// False, leaving the depth unchanged, at kFramebufferEffectDepthLimit and up.
bool SetFramebufferEffectDepth(FramebufferEffectSettings& settings,
                               std::uint32_t depth) noexcept;
void SetFramebufferEffectPreviousFrame(
    FramebufferEffectSettings& settings,
    std::int32_t uses_previous_frame) noexcept;

// The original state word (0 to 4).
[[nodiscard]] std::int32_t FramebufferEffectStateCode(
    const FramebufferEffect& effect) noexcept;
[[nodiscard]] std::int16_t ReadFramebufferEffectRoll(
    const FramebufferEffectSettings& settings) noexcept;
// The original reports the x scale for both axes.
[[nodiscard]] FramebufferEffectPair ReadFramebufferEffectScale(
    const FramebufferEffectSettings& settings) noexcept;
[[nodiscard]] FramebufferEffectTint ReadFramebufferEffectTint(
    const FramebufferEffectSettings& settings) noexcept;
[[nodiscard]] FramebufferEffectPair ReadFramebufferEffectOffset(
    const FramebufferEffectSettings& settings) noexcept;
[[nodiscard]] std::int32_t ReadFramebufferEffectBlendMode(
    const FramebufferEffectSettings& settings) noexcept;
[[nodiscard]] std::int32_t ReadFramebufferEffectParameter(
    const FramebufferEffectSettings& settings) noexcept;
[[nodiscard]] std::uint32_t ReadFramebufferEffectDepth(
    const FramebufferEffectSettings& settings) noexcept;
[[nodiscard]] std::int32_t ReadFramebufferEffectPreviousFrame(
    const FramebufferEffectSettings& settings) noexcept;

// --- Room transition --------------------------------------------------------

inline constexpr std::int32_t kScreenTransitionOpaque = 255;
// Columns of the wipe: the covered edge, the solid front, the edge quad's far
// side and the gradient's far side.
inline constexpr std::size_t kScreenTransitionWipeColumns = 4;

enum class ScreenTransitionStyle : std::uint8_t {
  kNone,
  kFadeToBlack,
  kFadeToWhite,
  kWipe,
};

// The Field transition the draw reads and steps.
struct ScreenTransition {
  // 0 clear to 255 covered.
  std::int32_t alpha = 0;
  // Steps 1, 2 and 5 cover the screen (5 becomes 6 once covered); 4 and 7
  // uncover it and end at step 0.
  std::uint32_t step = 0;
  std::int32_t effect = 0;
  std::int32_t wipe_angle = 0;
  std::int32_t speed = 0;

  friend bool operator==(const ScreenTransition&,
                         const ScreenTransition&) = default;
};

// A full-screen quad of grey `level` with the draw-mode page that picks its
// blend: subtractive for black, additive for white.
struct ScreenTransitionFade {
  std::uint8_t level = 0;
  bool semi_transparent = true;
  std::uint16_t texture_page = 0;

  friend bool operator==(const ScreenTransitionFade&,
                         const ScreenTransitionFade&) = default;
};

// The wipe's columns, each as its top (y -200) and bottom (y 200) points:
// the covered quad spans columns 0 and 1, the edge quad 1 and 2 and the
// gradient 2 and 3.
struct ScreenTransitionWipe {
  struct Column {
    ScreenVertex top{};
    ScreenVertex bottom{};

    friend bool operator==(const Column&, const Column&) = default;
  };
  std::array<Column, kScreenTransitionWipeColumns> columns{};
  // The draw-mode page: subtractive blending.
  std::uint16_t texture_page = 0;

  friend bool operator==(const ScreenTransitionWipe&,
                         const ScreenTransitionWipe&) = default;
};

class ScreenTransitionTarget {
 public:
  ScreenTransitionTarget() = default;
  ScreenTransitionTarget(const ScreenTransitionTarget&) = delete;
  ScreenTransitionTarget& operator=(const ScreenTransitionTarget&) = delete;
  ScreenTransitionTarget(ScreenTransitionTarget&&) = delete;
  ScreenTransitionTarget& operator=(ScreenTransitionTarget&&) = delete;
  virtual ~ScreenTransitionTarget() = default;

  virtual void DrawFade(const ScreenTransitionFade& fade) = 0;
  virtual void DrawWipe(const ScreenTransitionWipe& wipe) = 0;
};

[[nodiscard]] ScreenTransitionStyle ScreenTransitionStyleOf(
    std::int32_t effect) noexcept;
[[nodiscard]] ScreenTransitionFade PlanScreenTransitionFade(
    ScreenTransitionStyle style, std::int32_t alpha) noexcept;
// The x of columns 1 to 3 for `alpha`.
[[nodiscard]] std::array<std::int16_t, kScreenTransitionWipeColumns - 1>
ScreenTransitionWipeEdges(std::int32_t alpha) noexcept;
// Rotation by the wipe angle (half a turn more while uncovering) about the
// screen centre.
[[nodiscard]] Core::Math::Transform3 ScreenTransitionWipeTransform(
    const ScreenTransition& transition) noexcept;
// The transformer must hold ScreenTransitionWipeTransform(transition).
[[nodiscard]] ScreenTransitionWipe PlanScreenTransitionWipe(
    const ScreenTransition& transition, ScreenTransformer& transformer);
void AdvanceScreenTransition(ScreenTransition& transition) noexcept;
// Draws the fade or wipe while alpha is nonzero, then steps the alpha.
void DrawScreenTransition(ScreenTransition& transition,
                          ScreenTransitionTarget& target,
                          ScreenTransformer& transformer);
}  // namespace VS::Render
