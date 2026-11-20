#include "Battle/SphereTargetLabelsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <span>
#include <vector>

#include "Actors/AppearanceLive.hpp"
#include "Battle/BattleTypes.h"
#include "Battle/Sphere.hpp"
#include "Battle/SphereContainmentLive.hpp"
#include "Battle/SphereTargetLabels.hpp"
#include "Battle/TargetVolume.hpp"
#include "Battle/Targeting.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;

constexpr unsigned kCoordinateBits = 16;
constexpr std::uint32_t kCoordinateMask = 0xFFFF;
// func_800C4650 tests the sphere's secondary volume.
constexpr int kSecondaryVolume = 1;
constexpr int kNoPulseFlags = 0;
constexpr int kLinkOverlaySlot = 0;
constexpr std::size_t kPointAxes = 3;

static_assert(std::size(VS::Battle::TargetSphereRecord{}.labels) ==
              kSphereTargetLabelCount);
static_assert(std::size(VS::Menu::Framework::rowAnimationSteps) >=
              kTargetLabelSlideStepCount);
static_assert(sizeof(Battle_TargetLabelLine) ==
              sizeof(VS::Battle::TargetPreview));

std::uint64_t unordered_labels = 0;

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS::Core::LogError("battle.sphere", "{} refused: {}", operation, reason);
  std::abort();
}

VS::Battle::TargetSphereRecord& RequiredSphere(const char* operation) {
  auto* sphere = reinterpret_cast<VS::Battle::TargetSphereRecord*>(
      VS::Battle::targetSphere);
  if (sphere == nullptr) Refuse(operation, "missing sphere workspace");
  return *sphere;
}

LabelScreenPoint UnpackPoint(int packed) noexcept {
  const auto word = static_cast<std::uint32_t>(packed);
  return {.x = math::WrapSigned16(word),
          .y = math::WrapSigned16(word >> kCoordinateBits)};
}

int PackPoint(LabelScreenPoint point) noexcept {
  return static_cast<int>(
      (static_cast<std::uint32_t>(point.x) & kCoordinateMask) |
      (static_cast<std::uint32_t>(point.y) << kCoordinateBits));
}

SphereTargetLabel ReadLabel(
    const VS::Battle::SphereTargetLabelRecord& label) noexcept {
  return {.state = static_cast<std::uint8_t>(label.state),
          .depth = static_cast<std::uint8_t>(label.depth),
          .gradient = static_cast<std::uint8_t>(label.gradient),
          .ring_size = static_cast<std::uint8_t>(label.ring_size),
          .screen_x = label.screen_x,
          .offset_y = label.offset_y,
          .fade = static_cast<std::uint8_t>(label.fade),
          .world_position = {label.world_position.vx, label.world_position.vy,
                             label.world_position.vz}};
}

// The labels change only their state, highlight fade and screen x.
void WriteLabel(const SphereTargetLabel& value,
                VS::Battle::SphereTargetLabelRecord& label) noexcept {
  label.state = static_cast<char>(value.state);
  label.gradient = static_cast<char>(value.gradient);
  label.screen_x = value.screen_x;
}

// Entry `slot` of the frame's overlay ordering table, or null (reported, at
// powers of two) when it lies outside the table.
u_long* LabelOverlayEntry(int slot) {
  u_long* entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, slot);
  if (entry == nullptr && std::has_single_bit(++unordered_labels)) {
    VS::Core::LogError("battle.sphere",
                       "target label skipped: overlay entry {} is outside the "
                       "frame's ordering table ({} skipped)",
                       slot, unordered_labels);
  }
  return entry;
}

// The link and draw-mode helpers drop and count a primitive with no chain.
u_long* LinkOverlayEntry() {
  return Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay,
                                        kLinkOverlaySlot);
}

class LiveTargetLabelCanvas final : public SphereTargetLabelCanvas {
 public:
  explicit LiveTargetLabelCanvas(const char* text = nullptr) : text_(text) {}

  void DrawLinkLine(const TargetLabelLinkLine& line) override {
    Render::InsertLiveThreeWordPrimitive(
        LinkOverlayEntry(), static_cast<int>(line.command_color),
        PackPoint(line.start), PackPoint(line.end));
  }

  void SetLinkDrawMode(std::uint32_t draw_mode) override {
    Render::InsertLiveDrawModeCommand(static_cast<int>(draw_mode),
                                      LinkOverlayEntry());
  }

  std::uint32_t GradientStopColor(int gradient, int color_index,
                                  int brightness) override {
    return static_cast<std::uint32_t>(Menu::Framework::LiveGradientStopColor(
        static_cast<u_int>(gradient), static_cast<u_int>(color_index),
        brightness));
  }

  void DrawText(const TargetLabelTextDraw& text) override {
    u_long* chain = LabelOverlayEntry(text.overlay_slot);
    if (chain == nullptr) return;
    Render::DrawLiveCompactTextColor(text_, PackPoint(text.position),
                                     static_cast<int>(text.color), chain);
  }

  void DrawBox(const TargetLabelBox& box) override {
    u_long* chain = LabelOverlayEntry(box.overlay_slot);
    if (chain == nullptr) return;
    Render::LivePacketQueue queue(chain);
    queue.Emit(BuildTargetLabelBoxPacket(box, queue.ChainLink()));
  }

  void BeginProjection() override { Render::LoadLiveFrameViewMatrix(); }

  LabelScreenPoint Project(const TargetWorldPoint& world) override {
    SVECTOR point = {world[0], world[1], world[2], 0};
    Core_GteWord screen = 0;
    Core_Gte_LoadV0(&point);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2(&screen);
    return UnpackPoint(static_cast<int>(screen));
  }

 private:
  const char* text_;
};

void DrawLiveLabel(VS::Battle::SphereTargetLabelRecord& live) {
  SphereTargetLabel label = ReadLabel(live);
  LiveTargetLabelCanvas canvas(live.text);
  DrawSphereTargetLabel(label, VS::Battle::targetColumnTopOffset, canvas);
  WriteLabel(label, live);
}

// Draws each label with its live text; UpdateSphereTargetLabelsEntry stores
// the labels afterwards, as nothing the drawing calls reads them.
class LiveTargetLabelDrawer final : public SphereTargetLabelDrawer {
 public:
  explicit LiveTargetLabelDrawer(
      std::span<const VS::Battle::SphereTargetLabelRecord,
                kSphereTargetLabelCount>
          labels)
      : labels_(labels) {}

  void DrawLabel(std::size_t index, SphereTargetLabel& label) override {
    LiveTargetLabelCanvas canvas(labels_[index].text);
    DrawSphereTargetLabel(label, VS::Battle::targetColumnTopOffset, canvas);
  }

 private:
  std::span<const VS::Battle::SphereTargetLabelRecord, kSphereTargetLabelCount>
      labels_;
};

class LiveHighlightHost final : public SphereTargetHighlightHost {
 public:
  bool IsInSecondaryVolume(const TargetPoint& point) override {
    const std::array<std::uint16_t, kPointAxes> components = {
        static_cast<std::uint16_t>(point.x),
        static_cast<std::uint16_t>(point.y),
        static_cast<std::uint16_t>(point.z)};
    return IsLivePointInCurrentSphereVolume(kSecondaryVolume, components) != 0;
  }

  void EnablePalettePulse(int actor_id, int pulse_slot) override {
    Actors::EnableLiveModelPalettePulse(actor_id, kNoPulseFlags, pulse_slot);
  }

  void DisablePalettePulse(int actor_id, int pulse_slot) override {
    Actors::DisableLiveModelPalettePulse(actor_id, pulse_slot);
  }
};

// func_800C4650 reads its records as target previews
// (VS::Battle::TargetPreview).
ActionTargetPreview ReadTarget(const Battle_TargetLabelLine& line) noexcept {
  const auto target = std::bit_cast<VS::Battle::TargetPreview>(line);
  return {.position = {.x = target.position.vx,
                       .y = target.position.vy,
                       .z = target.position.vz,
                       .pad = target.position.pad},
          .kind = target.kind,
          .actor_id = target.actor_id,
          .limb = target.limb};
}
}  // namespace

// decomp: BATTLE.PRG func_800C16FC (src/BATTLE/BATTLE.PRG/58578.c:477),
// F01298.
void DrawRightTargetLabelLinkEntry(int anchor_xy, int ring_xy, int pulse_head) {
  LiveTargetLabelCanvas canvas;
  DrawRightTargetLabelLink(UnpackPoint(anchor_xy), UnpackPoint(ring_xy),
                           pulse_head, canvas);
}

// decomp: BATTLE.PRG func_800C1A40 (src/BATTLE/BATTLE.PRG/58578.c:561),
// F01299.
void DrawLeftTargetLabelLinkEntry(int anchor_xy, int ring_xy, int pulse_head) {
  LiveTargetLabelCanvas canvas;
  DrawLeftTargetLabelLink(UnpackPoint(anchor_xy), UnpackPoint(ring_xy),
                          pulse_head, canvas);
}

// decomp: BATTLE.PRG func_800C1D84 (src/BATTLE/BATTLE.PRG/58578.c:645),
// F01300.
int AreTargetLabelsSettledEntry() {
  const VS::Battle::TargetSphereRecord& sphere =
      RequiredSphere("AreTargetLabelsSettledEntry");
  std::array<SphereTargetLabel, kSphereTargetLabelCount> labels{};
  std::ranges::transform(sphere.labels, labels.begin(), ReadLabel);
  return AreTargetLabelsSettled(labels) ? 1 : 0;
}

// decomp: BATTLE.PRG func_800C1DC4 (src/BATTLE/BATTLE.PRG/58578.c:656),
// F01301.
void DrawSphereTargetLabelEntry(VS::Battle::SphereTargetLabelRecord* label) {
  if (label == nullptr) Refuse("DrawSphereTargetLabelEntry", "missing label");
  DrawLiveLabel(*label);
}

// decomp: BATTLE.PRG func_800C20B4 (src/BATTLE/BATTLE.PRG/58578.c:750),
// F01302.
void UpdateSphereTargetLabelsEntry() {
  VS::Battle::TargetSphereRecord& sphere =
      RequiredSphere("UpdateSphereTargetLabelsEntry");
  const std::span<VS::Battle::SphereTargetLabelRecord, kSphereTargetLabelCount>
      live(sphere.labels);
  std::array<SphereTargetLabel, kSphereTargetLabelCount> labels{};
  std::ranges::transform(live, labels.begin(), ReadLabel);
  // The original reads the steps unsigned (lbu).
  LabelSlideSteps slide_steps{};
  std::transform(std::begin(VS::Menu::Framework::rowAnimationSteps),
                 std::next(std::begin(VS::Menu::Framework::rowAnimationSteps),
                           kTargetLabelSlideStepCount),
                 slide_steps.begin(),
                 [](char step) { return static_cast<std::uint8_t>(step); });
  LiveTargetLabelDrawer drawer(live);
  UpdateSphereTargetLabels(labels, slide_steps, drawer);
  for (std::size_t index = 0; index < labels.size(); ++index) {
    WriteLabel(labels[index], live[index]);
  }
}

// decomp: BATTLE.PRG func_800C4650 (src/BATTLE/BATTLE.PRG/5BE50.c:65),
// F01310.
void UpdateSphereTargetHighlightsEntry(Battle_TargetLabelLine* targets,
                                       int count) {
  if (count <= 0) return;
  if (targets == nullptr) {
    VS::Core::LogError("battle.sphere", "{} highlight targets without a list",
                       count);
    return;
  }
  const std::span<const Battle_TargetLabelLine> lines(
      targets, static_cast<std::size_t>(count));
  std::vector<ActionTargetPreview> previews;
  previews.reserve(lines.size());
  std::ranges::transform(lines, std::back_inserter(previews), ReadTarget);
  LiveHighlightHost host;
  UpdateSphereTargetHighlights(previews, host);
}
}  // namespace VS::Battle
