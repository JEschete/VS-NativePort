// Hand-maintained (tools/port/owned.csv).
#include "Menu/Status/ActorSwitching.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
using Core::Math::R3000Quotient;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;

constexpr std::uint8_t kVitalsSlideSteps = 4;
constexpr std::uint8_t kVitalsDelayFrames = 4;

// Battle actor kinds below this are people the screen may inspect.
constexpr std::uint16_t kInspectableKindLimit = 5;
// func_800A0BE0's flags: the actor is present (bit 0) and not hidden (bit 24).
constexpr std::uint32_t kInspectableFlagsMask = 0x01000001;
constexpr std::uint32_t kInspectableFlags = 1;

// The model slide: 32 pixels a frame over 160.
constexpr std::int32_t kSlideStep = 32;
constexpr std::int32_t kSlideDistance = 160;
constexpr std::uint32_t kReversePhaseOffset = 3;
constexpr std::int32_t kReverseRequestBit = 0x10;
constexpr std::int32_t kRequestSlotMask = 0xF;
constexpr ActorNameRow kActorNameRow{
    .row = 4,
    .x = 180,
    .y = 18,
    .width = 140,
    .background_width = 8,
    .selected = true,
};

// The packed y (high halfword) steps and x positions of the panel.
constexpr std::int32_t kRowStep = 0x10000;
constexpr std::int32_t kPanelSlideRow = 8;
constexpr std::int32_t kPanelTopRow = -22;
constexpr std::int32_t kHpX = 66;
constexpr std::int32_t kMpX = 136;
constexpr std::int32_t kRiskX = 64;
constexpr std::int32_t kRiskRow = 15;
constexpr std::int32_t kBarRow = -8;
constexpr std::int32_t kRiskBarRow = 4;
constexpr std::int32_t kHpBarX = 10;
constexpr std::int32_t kMpBarX = 80;
constexpr std::int32_t kRiskBarX = 10;
// vs_getXY(-7, -2): the current value's position from the maximum's.
constexpr std::int32_t kCurrentFromMax = -0x10007;

constexpr std::int32_t kMaxValueFont = 0;
constexpr std::int32_t kCurrentValueFont = 1;
constexpr std::int32_t kSeparatorFont = 2;

constexpr std::int32_t kSpriteBrightness = 128;
constexpr std::int32_t kLabelBrightness = 384;
constexpr std::int32_t kLabelSlideShift = 19;
constexpr std::int32_t kLabelHeight = 0x90000;
constexpr std::uint32_t kLabelClut = 0x37F60000;
// vs_getWH(6, 9) and vs_getUV0Clut(228, 0, 832, 223): the small "?".
constexpr std::int32_t kSmallGlyphSize = 0x90006;
constexpr std::uint32_t kSmallGlyphTexture = 0x37F400E4;
constexpr std::int32_t kSmallGlyphAdvance = 5;
constexpr std::int32_t kSeparatorAdvance = 7;
// vs_getWH(7, 10) and vs_getUV0Clut(234, 0, 832, 223): the large "?".
constexpr std::int32_t kLargeGlyphSize = 0xA0007;
constexpr std::uint32_t kLargeGlyphTexture = 0x37F400EA;
constexpr std::int32_t kLargeGlyphAdvance = 6;
constexpr int kUnknownDigits = 3;

constexpr std::int32_t kAnalyzedBit = 0x100;
constexpr std::int32_t kAnalyzedShift = 8;
constexpr std::int32_t kColorMask = 0xFF;
constexpr std::int32_t kMpColor = 1;
constexpr std::int32_t kRiskColor = 2;
constexpr std::int32_t kGaugeScale = 64;
constexpr std::int32_t kMaxRisk = 100;

[[nodiscard]] std::int32_t Row(std::int32_t rows) {
  return WrappingMultiply(rows, kRowStep);
}

// The selected actor's slot, (_selectedActor - 1).
[[nodiscard]] std::int32_t SelectedSlot(const ActorSwitchState& state) {
  return WrappingSubtract(state.selected_actor, 1);
}

// menuLoading / menuLoadingReverse: fades, loads the new actor's model and
// starts its slide in from `from`.
void LoadSelectedModel(ActorSwitchState& state, ActorSwitchHost& host,
                       std::int32_t fade, std::int32_t from,
                       std::uint32_t next_phase) {
  state.fade_screen = fade;
  host.LoadActorPreview(SelectedSlot(state));
  state.delay_screen_update = 1;
  state.animation_step = from;
  state.phase = next_phase;
}

void StartSwitch(ActorSwitchState& state, std::int32_t request,
                 ActorSwitchHost& host) {
  if (state.selected_actor == 0) {
    state.phase = actor_switch_phase::kMenuLoading;
  } else {
    host.PlaySwitchSound();
    state.phase = actor_switch_phase::kMenuLoaded;
  }
  if ((request & kReverseRequestBit) != 0) {
    state.phase += kReversePhaseOffset;
  }
  state.selected_actor = request & kRequestSlotMask;
  host.ShowActorName(SelectedSlot(state), kActorNameRow);
}

// Advances the panel's slide; true when it draws the previous actor.
bool StepVitalsSlide(VitalsPanelState& state, std::int32_t hud_slide_offset,
                     VitalsHost& host) {
  switch (state.phase) {
    case vitals_phase::kHiding:
      if (state.slide != 0) {
        --state.slide;
        return true;
      }
      if (state.delay == 0) {
        host.SetHudHidden(0);
        state.phase = vitals_phase::kIdle;
      } else {
        --state.delay;
      }
      return false;
    case vitals_phase::kSwapping:
      if (state.slide != 0) {
        --state.slide;
        return true;
      }
      if (state.delay == 0) {
        state.phase = vitals_phase::kShowing;
      } else {
        --state.delay;
      }
      return false;
    case vitals_phase::kShowing:
      if (state.slide < kVitalsSlideSteps) {
        ++state.slide;
      } else {
        state.phase = vitals_phase::kIdle;
      }
      return false;
    case vitals_phase::kAwaitingHud:
      if (hud_slide_offset == kHudFullyHiddenOffset) {
        if (state.delay == 0) {
          state.phase = vitals_phase::kShowing;
        } else {
          --state.delay;
        }
      }
      return false;
    default:
      return false;
  }
}

void DrawKnownValues(VitalsHost& host, std::int32_t& y,
                     const ActorVitals& actor) {
  std::int32_t left = host.DrawValue(kMaxValueFont, y | kHpX, actor.max_hp);
  host.DrawValue(kSeparatorFont, left, 0);
  host.DrawValue(kCurrentValueFont, WrappingAdd(left, kCurrentFromMax),
                 actor.current_hp);
  left = host.DrawValue(kMaxValueFont, y | kMpX, actor.max_mp);
  host.DrawValue(kSeparatorFont, left, 0);
  host.DrawValue(kCurrentValueFont, WrappingAdd(left, kCurrentFromMax),
                 actor.current_mp);
  y = WrappingAdd(y, Row(kRiskRow));
  host.DrawValue(kCurrentValueFont, y | kRiskX, actor.risk);
  y = WrappingAdd(y, Row(1));
}

void DrawUnknownValues(VitalsHost& host, std::int32_t& y) {
  DrawUnanalyzedValue(host, y | kHpX, true);
  DrawUnanalyzedValue(host, y | kMpX, true);
  y = WrappingAdd(y, Row(kRiskRow + 1));
  DrawUnanalyzedValue(host, y | kRiskX, false);
}
}  // namespace

// decomp: MENU4.PRG _initEquipmentScreen (120.c:528).
void BeginVitalsTransition(VitalsPanelState& state, std::int32_t actor,
                           VitalsHost& host) {
  if (actor == 0) {
    state.phase = vitals_phase::kHiding;
  } else if (state.shown_actor == 0) {
    host.SetHudHidden(1);
    state.phase = vitals_phase::kAwaitingHud;
  } else {
    state.phase = vitals_phase::kSwapping;
  }
  state.delay = kVitalsDelayFrames;
  state.shown_actor = static_cast<std::uint8_t>(actor);
}

// decomp: MENU4.PRG _getNextValidActor (120.c:548).
std::optional<std::int32_t> FindNextInspectableActor(
    InspectableActorRoster& roster, std::int32_t current, std::int32_t step) {
  // After the first probe the candidate is one of 15 slots, so 15 probes
  // either find an actor or show that the original would never stop.
  for (int probe = 0; probe < kInspectableActorSlots; ++probe) {
    current = WrappingAdd(current, step) % kInspectableActorSlots;
    if (current < 0) return std::nullopt;
    const std::uint32_t flags = roster.StateFlags(current);
    const std::optional<std::uint16_t> kind = roster.Kind(current);
    if (kind.has_value() && *kind < kInspectableKindLimit &&
        (flags & kInspectableFlagsMask) == kInspectableFlags) {
      return current;
    }
  }
  return std::nullopt;
}

// decomp: MENU4.PRG _switchCurrentActor (120.c:579).
bool AnimateActorSwitch(ActorSwitchState& state, std::int32_t request,
                        ActorSwitchHost& host) {
  namespace phase = actor_switch_phase;
  if (request != 0) {
    StartSwitch(state, request, host);
    return false;
  }
  switch (state.phase) {
    case phase::kMenuLoaded:
      if (state.animation_step < kSlideDistance) {
        state.draw_background_first = 1;
        state.animation_step = WrappingAdd(state.animation_step, kSlideStep);
        return false;
      }
      state.draw_background_first = 0;
      host.ReleaseActorPreview();
      [[fallthrough]];
    case phase::kMenuLoading:
      LoadSelectedModel(state, host, 1, -kSlideDistance,
                        phase::kAnimateForward);
      return false;
    case phase::kAnimateForward:
      if (state.animation_step < 0) {
        state.animation_step = WrappingAdd(state.animation_step, kSlideStep);
        return false;
      }
      state.fade_screen = 0;
      state.phase = phase::kComplete;
      return true;
    case phase::kMenuLoadedReverse:
      if (state.animation_step > -kSlideDistance) {
        state.fade_screen = 1;
        state.animation_step =
            WrappingSubtract(state.animation_step, kSlideStep);
        return false;
      }
      host.ReleaseActorPreview();
      [[fallthrough]];
    case phase::kMenuLoadingReverse:
      LoadSelectedModel(state, host, 0, kSlideDistance, phase::kAnimateReverse);
      return false;
    case phase::kAnimateReverse:
      if (state.animation_step > 0) {
        state.draw_background_first = 1;
        state.animation_step =
            WrappingSubtract(state.animation_step, kSlideStep);
        return false;
      }
      state.draw_background_first = 0;
      state.phase = phase::kComplete;
      return true;
    case phase::kComplete:
      return true;
    default:
      return false;
  }
}

// decomp: MENU4.PRG _renderUnknownValue (120.c:692).
void DrawUnanalyzedValue(VitalsHost& host, std::int32_t xy, bool with_max) {
  if (with_max) {
    for (int digit = 0; digit < kUnknownDigits; ++digit) {
      host.QueueSprite({.brightness = kSpriteBrightness,
                        .xy = xy,
                        .wh = kSmallGlyphSize,
                        .uv_clut = kSmallGlyphTexture});
      xy = WrappingSubtract(xy, kSmallGlyphAdvance);
    }
    host.DrawValue(kSeparatorFont, xy, 0);
    xy = WrappingSubtract(xy, kSeparatorAdvance);
  }
  xy = WrappingSubtract(xy, kRowStep);
  for (int digit = 0; digit < kUnknownDigits; ++digit) {
    host.QueueSprite({.brightness = kSpriteBrightness,
                      .xy = xy,
                      .wh = kLargeGlyphSize,
                      .uv_clut = kLargeGlyphTexture});
    xy = WrappingSubtract(xy, kLargeGlyphAdvance);
  }
}

// decomp: MENU4.PRG _renderStatBar (120.c:721).
void DrawVitalBar(VitalsHost& host, std::int32_t color_index,
                  std::int32_t current, std::int32_t max, std::int32_t xy) {
  if (max == 0) {
    current = 0;
    max = 1;
  }
  if ((color_index >> kAnalyzedShift) == 0) {
    current = max;
  }
  current = WrappingMultiply(current, kGaugeScale);
  const std::int32_t fill =
      R3000Quotient(WrappingSubtract(WrappingAdd(current, max), 1), max);
  host.DrawGauge(color_index & kColorMask, fill, xy);
}

// decomp: MENU4.PRG _renderBasicStats (120.c:741).
bool DrawVitals(VitalsPanelState& state, const VitalsFrame& frame,
                VitalsHost& host) {
  if (!frame.selected.has_value()) return false;
  const bool previous = StepVitalsSlide(state, frame.hud_slide_offset, host);
  if (state.slide == 0) return true;
  if (previous && !frame.previous.has_value()) return false;
  const ActorVitals& actor = previous ? *frame.previous : *frame.selected;

  std::int32_t y = Row(
      WrappingAdd(WrappingMultiply(state.slide, kPanelSlideRow), kPanelTopRow));
  if (actor.analyzed) {
    DrawKnownValues(host, y, actor);
  } else {
    DrawUnknownValues(host, y);
  }
  const std::int32_t label_slide = WrappingMultiply(
      WrappingSubtract(state.slide, kVitalsSlideSteps), 1 << kLabelSlideShift);
  for (std::size_t label = 0; label < kVitalsLabels; ++label) {
    host.QueueLabelSprite(
        {.brightness = kLabelBrightness,
         .xy = WrappingAdd(frame.labels.positions[label], label_slide),
         .wh = frame.labels.widths[label] | kLabelHeight,
         .uv_clut = frame.labels.texture_u[label] | kLabelClut});
  }
  y = WrappingAdd(y, Row(kBarRow));
  const std::int32_t analyzed = actor.analyzed ? kAnalyzedBit : 0;
  DrawVitalBar(host, analyzed, actor.current_hp, actor.max_hp, y | kHpBarX);
  DrawVitalBar(host, analyzed | kMpColor, actor.current_mp, actor.max_mp,
               y | kMpBarX);
  y = WrappingAdd(y, Row(kRiskBarRow));
  DrawVitalBar(host, analyzed | kRiskColor, actor.risk, kMaxRisk,
               y | kRiskBarX);
  return true;
}

// decomp: MENU4.PRG _getLimbCount (120.c:1114).
std::int32_t LimbCount(std::span<const std::uint8_t> limb_name_indices) {
  const std::size_t limit =
      std::min(limb_name_indices.size(), kStatusLimbSlots);
  const auto named = limb_name_indices.first(limit);
  const auto unnamed = std::ranges::find(named, std::uint8_t{0});
  return static_cast<std::int32_t>(unnamed - named.begin());
}

// decomp: MENU4.PRG _getEquipmentListRowCount (120.c:1132).
std::int32_t EquipmentRowCount(std::span<const std::uint8_t> limb_name_indices,
                               std::uint16_t accessory_id) {
  constexpr std::int32_t kWeaponAndShieldRows = 2;
  return LimbCount(limb_name_indices) + kWeaponAndShieldRows +
         (accessory_id != 0 ? 1 : 0);
}
}  // namespace VS::Menu::Status
