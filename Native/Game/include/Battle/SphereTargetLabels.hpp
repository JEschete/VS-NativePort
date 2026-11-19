#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Battle/Targeting.hpp"
#include "Battle/TargetingDraw.hpp"

// The sphere's target labels: the two columns of limb names that slide in
// beside the target sphere, the selected label's animated link to its target
// and the palette pulse on the actors inside the secondary volume. Labels are
// passed as values and every primitive, projection and actor effect goes
// through a canvas or host, so the labels read no target sphere global, GTE
// register, ordering table or PS1 address (SphereTargetLabelsLive.cpp binds
// them to the live sphere).
namespace VS::Battle {
inline constexpr std::size_t kSphereTargetLabelCount = 24;
inline constexpr std::size_t kTargetLabelSlideStepCount = 16;

// Label states. A sliding label moves every update until it rests in its
// column or leaves the screen; leaving the visible range hides it.
inline constexpr std::uint8_t kTargetLabelHidden = 0;
inline constexpr std::uint8_t kTargetLabelSettled = 1;
// Enters the right column from the right, or leaves the left column.
inline constexpr std::uint8_t kTargetLabelSlidingLeft = 2;
// Enters the left column from the left, or leaves the right column.
inline constexpr std::uint8_t kTargetLabelSlidingRight = 3;

// Labels left of this screen x are in the left column.
inline constexpr std::int16_t kTargetLabelColumnSplitX = 160;
inline constexpr std::int16_t kTargetLabelWidth = 0x48;
inline constexpr std::int16_t kTargetLabelHeight = 8;

// The semi-transparent flat line command of each link segment and the draw
// mode inserted after them (getTPage(0, 1, 0, 0): additive blending).
inline constexpr std::uint32_t kTargetLabelLinkCommand = 0x42000000;
inline constexpr std::uint32_t kTargetLabelLinkDrawMode = 0xE1000020;

// A screen position as the original packs it: x in the low halfword and y in
// the high halfword of a GPU word.
struct LabelScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const LabelScreenPoint&,
                         const LabelScreenPoint&) = default;
};

// One label of Battle_SphereTargetLabel without its text pointer.
struct SphereTargetLabel {
  std::uint8_t state = kTargetLabelHidden;
  // Nonzero for the selected label, which is fully lit and linked to its
  // target. The label goes in front of overlay ordering entry 2 - depth.
  std::uint8_t depth = 0;
  // Highlight fade that counts down once per draw; 8 is fully lit.
  std::uint8_t gradient = 0;
  // Phase of the selected label's link pulse.
  std::uint8_t ring_size = 0;
  std::int16_t screen_x = 0;
  // Row offset below the column top.
  std::uint8_t offset_y = 0;
  // Gradient colour row of the label box.
  std::uint8_t fade = 0;
  TargetWorldPoint world_position{};

  friend bool operator==(const SphereTargetLabel&,
                         const SphereTargetLabel&) = default;
};

// The first entries of VS::Menu::Framework::rowAnimationSteps: how far a label
// is from its rest position on each of its final slide steps.
using LabelSlideSteps = std::array<std::uint8_t, kTargetLabelSlideStepCount>;

// One link segment: the line command and colour word and its two ends.
struct TargetLabelLinkLine {
  std::uint32_t command_color = 0;
  LabelScreenPoint start{};
  LabelScreenPoint end{};

  friend bool operator==(const TargetLabelLinkLine&,
                         const TargetLabelLinkLine&) = default;
};

class TargetLabelLinkCanvas {
 public:
  TargetLabelLinkCanvas() = default;
  TargetLabelLinkCanvas(const TargetLabelLinkCanvas&) = delete;
  TargetLabelLinkCanvas& operator=(const TargetLabelLinkCanvas&) = delete;
  TargetLabelLinkCanvas(TargetLabelLinkCanvas&&) = delete;
  TargetLabelLinkCanvas& operator=(TargetLabelLinkCanvas&&) = delete;
  virtual ~TargetLabelLinkCanvas() = default;

  // Inserts a line in front of the overlay ordering table's first entry
  // (vs_battle_addTile).
  virtual void DrawLinkLine(const TargetLabelLinkLine& line) = 0;
  // Inserts a draw-mode command in front of the same entry
  // (vs_battle_insertTpage).
  virtual void SetLinkDrawMode(std::uint32_t draw_mode) = 0;
};

// The label's text request: its packed position, colour and the overlay
// ordering entry it goes in front of.
struct TargetLabelTextDraw {
  LabelScreenPoint position{};
  std::uint32_t color = 0;
  int overlay_slot = 0;

  friend bool operator==(const TargetLabelTextDraw&,
                         const TargetLabelTextDraw&) = default;
};

// The label box: a black shadow tile under a horizontal gradient quad. The
// colour words are the GradientStop results, ORed into the quad's command
// word unmasked as the original does.
struct TargetLabelBox {
  LabelScreenPoint shadow_position{};
  std::int16_t shadow_width = 0;
  LabelScreenPoint position{};
  std::uint32_t left_color = 0;
  std::uint32_t right_color = 0;
  int overlay_slot = 0;

  friend bool operator==(const TargetLabelBox&,
                         const TargetLabelBox&) = default;
};

// Tag, shadow tile (3), draw mode, gouraud quad (8) and closing draw mode.
inline constexpr std::size_t kTargetLabelBoxPacketWords = 14;
using LabelBoxPacket = std::array<std::uint32_t, kTargetLabelBoxPacketWords>;

// The box's packet linked in front of `next_link` (labelBoxPrim_t).
[[nodiscard]] LabelBoxPacket BuildTargetLabelBoxPacket(
    const TargetLabelBox& box, std::uint32_t next_link) noexcept;

class SphereTargetLabelCanvas : public TargetLabelLinkCanvas {
 public:
  // vs_battle_getGradientStop (func_800C8FAC).
  [[nodiscard]] virtual std::uint32_t GradientStopColor(int gradient,
                                                        int color_index,
                                                        int brightness) = 0;
  // Draws the label's own text (vs_battle_renderTextRawColor).
  virtual void DrawText(const TargetLabelTextDraw& text) = 0;
  virtual void DrawBox(const TargetLabelBox& box) = 0;
  // Loads the frame view for the projection that follows (func_800C02A8).
  virtual void BeginProjection() = 0;
  // The GTE's RTPS screen position of a world point.
  [[nodiscard]] virtual LabelScreenPoint Project(
      const TargetWorldPoint& world) = 0;
};

// Draws one label that stays on screen after its slide step.
class SphereTargetLabelDrawer {
 public:
  SphereTargetLabelDrawer() = default;
  SphereTargetLabelDrawer(const SphereTargetLabelDrawer&) = delete;
  SphereTargetLabelDrawer& operator=(const SphereTargetLabelDrawer&) = delete;
  SphereTargetLabelDrawer(SphereTargetLabelDrawer&&) = delete;
  SphereTargetLabelDrawer& operator=(SphereTargetLabelDrawer&&) = delete;
  virtual ~SphereTargetLabelDrawer() = default;

  virtual void DrawLabel(std::size_t index, SphereTargetLabel& label) = 0;
};

class SphereTargetHighlightHost {
 public:
  SphereTargetHighlightHost() = default;
  SphereTargetHighlightHost(const SphereTargetHighlightHost&) = delete;
  SphereTargetHighlightHost& operator=(const SphereTargetHighlightHost&) =
      delete;
  SphereTargetHighlightHost(SphereTargetHighlightHost&&) = delete;
  SphereTargetHighlightHost& operator=(SphereTargetHighlightHost&&) = delete;
  virtual ~SphereTargetHighlightHost() = default;

  // Whether the point is inside the sphere's secondary volume
  // (func_800C1564 on volumes[1]).
  [[nodiscard]] virtual bool IsInSecondaryVolume(const TargetPoint& point) = 0;
  // func_8009FD5C with no pulse flags.
  virtual void EnablePalettePulse(int actor_id, int pulse_slot) = 0;
  // func_8009FE74.
  virtual void DisablePalettePulse(int actor_id, int pulse_slot) = 0;
};

// The selected label's link from its projected target `anchor` to the label's
// `ring` point, for a label in the right column (func_800C16FC) or the left
// column (func_800C1A40). The link runs diagonally from the anchor and then
// horizontally to the ring; segments up to `pulse_head` light up, brightest at
// the head, and later segments are not drawn.
void DrawRightTargetLabelLink(LabelScreenPoint anchor, LabelScreenPoint ring,
                              int pulse_head, TargetLabelLinkCanvas& canvas);
void DrawLeftTargetLabelLink(LabelScreenPoint anchor, LabelScreenPoint ring,
                             int pulse_head, TargetLabelLinkCanvas& canvas);

// True when no label is still sliding (func_800C1D84).
[[nodiscard]] bool AreTargetLabelsSettled(
    std::span<const SphereTargetLabel, kSphereTargetLabelCount>
        labels) noexcept;

// Draws the label's text, box and, when selected, its link, and steps its
// highlight fade (func_800C1DC4).
void DrawSphereTargetLabel(SphereTargetLabel& label,
                           std::uint8_t column_top_offset,
                           SphereTargetLabelCanvas& canvas);

// Slides each shown label one step, hides those that left the screen and
// draws the rest in order (func_800C20B4).
void UpdateSphereTargetLabels(
    std::span<SphereTargetLabel, kSphereTargetLabelCount> labels,
    const LabelSlideSteps& slide_steps, SphereTargetLabelDrawer& drawer);

// Pulses the palette of each actor target inside the secondary volume and
// stops the pulse of the others; position targets are skipped
// (func_800C4650).
void UpdateSphereTargetHighlights(std::span<const ActionTargetPreview> targets,
                                  SphereTargetHighlightHost& host);
}  // namespace VS::Battle
