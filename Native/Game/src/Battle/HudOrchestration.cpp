// Hand-maintained (tools/port/owned.csv).
#include "Battle/HudOrchestration.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Render/Sprites.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;
constexpr std::uint8_t kHiddenShortcutStep = 10;
constexpr std::uint8_t kIndicatorOpenSteps = 4;
constexpr std::int32_t kHiddenSlide = 0x200000;
constexpr std::int32_t kSlideStep = 0x80000;
constexpr std::int32_t kShortcutRightOrigin = 160;
constexpr std::int32_t kSpellIconInset = 2;
constexpr std::uint32_t kHudDrawMode = 0xE1000017;
constexpr std::uint32_t kShortcutClut = 0x373D0000;
constexpr std::uint32_t kShortcutBadgeTexture = 0x37FBA0B8;
constexpr std::uint32_t kShortcutDigitTexture = 0x37FB8000;
constexpr std::int32_t kShortcutBadgeSize = 0x1E001E;
constexpr std::int32_t kShortcutBadgePosition = 0x660048;
constexpr std::int32_t kShortcutDigitSize = 0x100010;
constexpr std::uint32_t kPositionMask = 0xFFFF;
constexpr int kCoordinateShift = 16;
constexpr int kDigitCellShift = 4;
constexpr int kOpaqueBrightness = 0x80;
constexpr int kEnabledBrightness = 0x180;
constexpr int kIndicatorBrightness = 0x140;
constexpr int kIndicatorX = 0xEC;
constexpr std::int32_t kIndicatorSize = 0xA0049;
constexpr std::uint32_t kIndicatorTexture = 0x373D0040;
constexpr std::uint32_t kLimbSpriteColor = 0x64000000;
constexpr std::int32_t kLimbOrigin = 0xB80008;
constexpr std::int32_t kZeroRiskPosition = 0x16000C;
constexpr std::uint16_t kIndicatorPhaseMask = 0x7FF;
constexpr int kIndicatorSineShift = 5;
constexpr int kIndicatorPhaseShift = 6;
constexpr int kStatusBits = 32;
constexpr std::uint8_t kAbsentIcon = 255;
constexpr int kCarouselAdvanceStep = 12;
constexpr int kCarouselSlideStart = 9;
constexpr int kCarouselSlideWidth = 16;
constexpr int kCarouselIdleSlide = -8;
constexpr int kCarouselSecondSlideOrigin = 24;
constexpr int kCarouselFastMask = 14;
constexpr int kCarouselStepMask = 15;
constexpr int kCountOriginX = 0x94;
constexpr int kCountStride = 3;
constexpr int kCountSecondRow = 0x30000;
constexpr int kCountDotSize = 0x40004;
constexpr int kCountHeight = 0x40000;
constexpr int kCountFullRowWidth = 25;
constexpr unsigned kCountColumns = 8;
constexpr unsigned kFewStatuses = 3;
constexpr std::uint32_t kCountDotTexture = 0x37F40C1A;
constexpr std::uint32_t kCountBarTexture = 0x37F40C00;
constexpr std::uint8_t kMarkerPinned = 0x80;
constexpr std::uint8_t kMarkerUnpin = 0x40;
constexpr std::uint8_t kMarkerKindMask = 0x7F;
constexpr std::uint8_t kSaturatedClockPart = 59;
constexpr std::uint8_t kSaturatedHundredths = 99;
constexpr int kDecimalDigitMask = 15;
constexpr int kTimerDigitStride = 3;
constexpr int kTimerLastPair = 6;
constexpr std::size_t kTimerParts = 3;

std::int32_t PointPosition(HudPoint point, std::int32_t displacement) {
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(math::WrappingAdd(point.x, displacement)) &
       kPositionMask) |
      (static_cast<std::uint32_t>(point.y) << kCoordinateShift));
}

bool DrawShortcut(HudOrchestrationWorld& world, HudShortcutRow row,
                  std::size_t slot, std::int32_t displacement) {
  const int shortcut =
      static_cast<int>(slot) + (row == HudShortcutRow::kAbilities
                                    ? static_cast<int>(kHudShortcutRowSize)
                                    : 0);
  const auto tone = world.ShortcutTone(shortcut);
  if (!tone.has_value()) return false;
  if (tone.value() == HudShortcutTone::kHidden) return true;
  const auto marker = world.ShortcutMarkerPosition(row, slot);
  if (!marker.has_value() ||
      !world.DrawShortcutMarker(PointPosition(marker.value(), displacement),
                                tone.value())) {
    return false;
  }
  const auto geometry = world.ShortcutIconGeometry(row, slot);
  if (!geometry.has_value()) return false;
  const auto position = static_cast<std::uint32_t>(geometry->position);
  const HudPoint point{
      .x = static_cast<std::uint8_t>(position),
      .y = static_cast<std::uint8_t>(position >> kCoordinateShift)};
  const int inset = row == HudShortcutRow::kSpells ? kSpellIconInset : 0;
  auto* const sprite = world.QueueSprite(
      HudPresentationLayer::kShortcut,
      {.draw_mode = kHudDrawMode,
       .brightness_transparency = static_cast<int>(tone.value()),
       .packed_position =
           PointPosition(point, math::WrappingSubtract(displacement, inset)),
       .packed_size = geometry->size});
  if (sprite == nullptr) return false;
  const auto texture = world.ShortcutIconTexture(row, slot);
  if (!texture.has_value()) return false;
  sprite->SetTexture(texture.value() | kShortcutClut);
  return true;
}

bool QueuePainted(HudOrchestrationWorld& world, HudPresentationLayer layer,
                  const Render::SpriteRequest& request, std::uint32_t texture) {
  auto* const sprite = world.QueueSprite(layer, request);
  if (sprite == nullptr) return false;
  sprite->SetTexture(texture);
  return true;
}

bool DrawIndicator(HudOrchestrationWorld& world) {
  auto state = world.Presentation();
  if (state.commands.battle_mode.value ==
      static_cast<std::uint8_t>(HudBattleMode::kWeaponDrawn)) {
    if (state.indicator_step < kIndicatorOpenSteps) {
      world.SetIndicatorPhase(0);
      world.SetIndicatorStep(state.indicator_step + 1);
    }
  } else if (state.indicator_step != 0) {
    world.SetIndicatorStep(state.indicator_step - 1);
  }
  state = world.Presentation();
  if (state.indicator_step == 0) return true;
  const auto y = static_cast<std::uint32_t>(
      static_cast<int>(state.indicator_step) * kCountStride -
      kIndicatorOpenSteps);
  const auto position =
      math::WrappingSubtract(static_cast<std::int32_t>(y << kCoordinateShift),
                             state.slide) |
      kIndicatorX;
  auto* const sprite =
      world.QueueSprite(HudPresentationLayer::kIndicator,
                        {.draw_mode = kHudDrawMode,
                         .brightness_transparency = kIndicatorBrightness,
                         .packed_position = position,
                         .packed_size = kIndicatorSize});
  if (sprite == nullptr) return false;
  const auto sine =
      world.Sine(world.Presentation().indicator_phase & kIndicatorPhaseMask);
  sprite->SetTexture(kIndicatorTexture);
  sprite->ModulateColor(sine >> kIndicatorSineShift);
  const auto speed = static_cast<std::uint32_t>(world.TickSpeed());
  world.SetIndicatorPhase(static_cast<std::uint16_t>(
      world.Presentation().indicator_phase + (speed << kIndicatorPhaseShift)));
  return true;
}

bool DrawLimbsAndSlide(HudOrchestrationWorld& world) {
  auto state = world.Presentation();
  if (state.commands.IsHidden()) {
    world.SetSlide(math::WrappingAdd(state.slide, kSlideStep));
    return true;
  }
  if (world.LimbDisplayAllowed()) {
    for (std::size_t limb = 0; limb < kHudDisplayedLimbs; ++limb) {
      const auto layout =
          limb + (world.Presentation().commands.battle_mode.value & 1U) *
                     kHudDisplayedLimbs;
      const auto geometry = world.LimbGeometry(layout);
      if (!geometry.has_value()) return false;
      auto* const sprite =
          world.QueueSprite(HudPresentationLayer::kLimbs,
                            {.draw_mode = Render::kDefaultSpriteDrawMode,
                             .brightness_transparency = 0,
                             .packed_position = math::WrappingAdd(
                                 kLimbOrigin, geometry->position),
                             .packed_size = geometry->size});
      if (sprite == nullptr) return false;
      const auto condition = world.CurrentLimbCondition(limb);
      if (!condition.has_value()) return false;
      const auto color = world.LimbColor(condition.value());
      if (!color.has_value()) return false;
      sprite->SetColor(color.value() | kLimbSpriteColor);
      const auto texture = world.LimbTexture(layout);
      if (!texture.has_value()) return false;
      sprite->SetTexture(texture.value());
    }
  }
  state = world.Presentation();
  if (state.slide != 0) {
    world.SetSlide(math::WrappingSubtract(state.slide, kSlideStep));
  }
  return true;
}

struct StatusCount {
  unsigned count = 0;
  unsigned selected = kAbsentIcon;
};

StatusCount PrepareCarousel(HudOrchestrationWorld& world, std::uint32_t flags) {
  StatusCount result;
  auto state = world.Presentation();
  for (int bit = 0; bit < kStatusBits; ++bit) {
    if (((flags >> bit) & 1U) != 0) {
      if (bit == state.carousel[1]) result.selected = result.count;
      ++result.count;
    }
  }
  for (std::size_t slot = 0; slot < state.carousel.size() - 1; ++slot) {
    // SRLV masks even a sentinel shift to its low five bits.
    if (((flags >> (state.carousel[slot] & (kStatusBits - 1))) & 1U) == 0) {
      world.SetCarousel(slot, kAbsentIcon);
    }
  }
  return result;
}

int AdvanceCarousel(HudOrchestrationWorld& world, std::uint32_t flags,
                    unsigned count) {
  auto state = world.Presentation();
  int step = state.carousel.back();
  if (step == kCarouselAdvanceStep) {
    world.SetCarousel(0, state.carousel[1]);
    world.SetCarousel(1, state.carousel[2]);
    world.SetCarousel(2, kAbsentIcon);
    const auto middle = world.Presentation().carousel[1];
    int icon = static_cast<std::uint8_t>(middle + 1);
    for (int checked = 0; checked < kStatusBits; ++checked, ++icon) {
      icon %= kStatusBits;
      if (((flags >> icon) & 1U) != 0 && icon != middle) {
        world.SetCarousel(2, static_cast<std::uint8_t>(icon));
        break;
      }
    }
  }
  state = world.Presentation();
  if (state.carousel[0] == state.carousel[2] &&
      state.carousel[1] != kAbsentIcon) {
    if (count == 1 && step < kCountStride) step = 0;
    if (count == 2 && state.carousel[0] != kAbsentIcon &&
        static_cast<unsigned>(step - kCarouselAdvanceStep) < kCountStride) {
      step = kCarouselAdvanceStep;
    }
  }
  int slide = kCarouselIdleSlide;
  if (step >= kCarouselSlideStart) {
    slide = (step < kCarouselAdvanceStep ? static_cast<int>(kCountColumns)
                                         : kCarouselSecondSlideOrigin) -
            step * 2;
  }
  if (++step != world.Presentation().carousel.back() &&
      world.TickSpeed() == State::kSlowedTickSpeed) {
    step = (step + 1) & kCarouselFastMask;
  }
  world.SetCarousel(kHudCarouselSize - 1,
                    static_cast<std::uint8_t>(step & kCarouselStepMask));
  return slide;
}

bool DrawCountSprite(HudOrchestrationWorld& world, std::int32_t position,
                     std::int32_t size, std::uint32_t texture,
                     bool default_page) {
  return QueuePainted(
      world, HudPresentationLayer::kStatusCount,
      {.draw_mode = default_page ? Render::kDefaultSpriteDrawMode : 0,
       .brightness_transparency = kOpaqueBrightness,
       .packed_position = position,
       .packed_size = size},
      texture);
}

bool DrawStatusCount(HudOrchestrationWorld& world,
                     const StatusCount& statuses) {
  const auto top =
      math::WrappingSubtract(kSlideStep, world.Presentation().slide);
  if (statuses.count < kFewStatuses) {
    for (unsigned index = 0; index < statuses.count; ++index) {
      if (!DrawCountSprite(
              world,
              (kCountOriginX + static_cast<int>(index) * kCountStride) | top,
              kCountDotSize, kCountDotTexture, true)) {
        return false;
      }
    }
    return true;
  }
  const auto lower = math::WrappingAdd(top, kCountSecondRow);
  if (statuses.selected != kAbsentIcon &&
      !DrawCountSprite(
          world,
          (static_cast<int>(statuses.selected & (kCountColumns - 1)) *
               kCountStride +
           kCountOriginX) |
              ((statuses.selected & kCountColumns) == 0 ? top : lower),
          kCountDotSize, kCountDotTexture, false)) {
    return false;
  }
  int width = kCountFullRowWidth;
  if (statuses.count < kCountColumns + 1) {
    width = static_cast<int>(statuses.count) * kCountStride + 1;
  } else if (!DrawCountSprite(world, lower | kCountOriginX,
                              ((static_cast<int>(statuses.count) -
                                static_cast<int>(kCountColumns)) *
                                   kCountStride +
                               1) |
                                  kCountHeight,
                              kCountBarTexture, false)) {
    return false;
  }
  return DrawCountSprite(world, top | kCountOriginX, width | kCountHeight,
                         kCountBarTexture, true);
}
}  // namespace

// decomp: F00729 func_80095B70 (src/BATTLE/BATTLE.PRG/2842C.c:2379).
void SetActorMarkersHidden(HudFeedbackState& state, std::int32_t command) {
  state.marker_visibility_command =
      math::WrapSigned16(static_cast<std::uint32_t>(command));
}

// decomp: F00730 func_80095B7C (src/BATTLE/BATTLE.PRG/2842C.c:2381).
bool SetActorMarker(std::span<std::byte, kActorMarkerCount> markers,
                    std::int32_t slot, std::int32_t command) {
  if (slot == 0) {
    std::ranges::fill(markers, static_cast<std::byte>(command));
    return true;
  }
  if (slot < 0 || static_cast<std::size_t>(slot) >= markers.size()) {
    return false;
  }
  auto& value = markers[static_cast<std::size_t>(slot)];
  const auto previous = std::to_integer<std::uint8_t>(value);
  if ((command & kMarkerPinned) != 0) {
    value = static_cast<std::byte>(previous | kMarkerPinned);
  } else if ((command & kMarkerUnpin) != 0) {
    value = static_cast<std::byte>(previous & kMarkerKindMask);
  } else {
    value = static_cast<std::byte>((previous & kMarkerPinned) |
                                   (command & kMarkerKindMask));
  }
  return true;
}

// decomp: F00743 func_8009722C (src/BATTLE/BATTLE.PRG/2842C.c:2893).
void SuppressAshleyStatusLabels(HudFeedbackState& state) {
  state.ashley_label_suppression = 1;
}

// decomp: F01361 func_800C9FE8 (src/BATTLE/BATTLE.PRG/5BF94.c:3249).
bool DrawShortcutHud(HudOrchestrationWorld& world) {
  const auto before = world.Presentation();
  if (before.commands.shortcut != 0) {
    if (before.shortcut_step != 0) {
      world.SetShortcutStep(before.shortcut_step - 1);
    }
  } else {
    const auto step = world.ClosingShortcutStep(before.shortcut_step);
    if (!step.has_value()) return false;
    world.SetShortcutStep(step.value());
  }
  const auto step = world.Presentation().shortcut_step;
  if (step != kHiddenShortcutStep) {
    const auto amount = world.ShortcutDisplacement(step);
    if (!amount.has_value()) return false;
    const auto left = -static_cast<std::int32_t>(amount.value());
    for (std::size_t slot = 0; slot < kHudShortcutRowSize; ++slot) {
      if (!DrawShortcut(world, HudShortcutRow::kAbilities, slot, left)) {
        return false;
      }
    }
    if (!QueuePainted(
            world, HudPresentationLayer::kShortcut,
            {.draw_mode = Render::kDefaultSpriteDrawMode,
             .brightness_transparency = kEnabledBrightness,
             .packed_position = static_cast<std::int32_t>(
                 (static_cast<std::uint32_t>(left + kShortcutBadgePosition) &
                  kPositionMask) |
                 (static_cast<std::uint32_t>(kShortcutBadgePosition) &
                  ~kPositionMask)),
             .packed_size = kShortcutBadgeSize},
            kShortcutBadgeTexture)) {
      return false;
    }
    const auto right = kShortcutRightOrigin - left;
    for (std::size_t slot = 0; slot < kHudShortcutRowSize; ++slot) {
      if (!DrawShortcut(world, HudShortcutRow::kSpells, slot, right)) {
        return false;
      }
      const auto digit = kHudShortcutRowSize - 1 - slot;
      const auto position = world.ShortcutDigitPosition(digit);
      if (!position.has_value() ||
          !QueuePainted(
              world, HudPresentationLayer::kShortcut,
              {.draw_mode = Render::kDefaultSpriteDrawMode,
               .brightness_transparency = kOpaqueBrightness,
               .packed_position = math::WrappingAdd(right, position.value()),
               .packed_size = kShortcutDigitSize},
              static_cast<std::uint32_t>(digit << kDigitCellShift) |
                  kShortcutDigitTexture)) {
        return false;
      }
    }
  }
  world.SetShortcutRequest(0);
  return true;
}

// decomp: F01362 func_800CA2DC (src/BATTLE/BATTLE.PRG/5BF94.c:3307).
bool DrawBattleHud(HudOrchestrationWorld& world) {
  world.DrawShortcutPanel();
  if (!DrawIndicator(world)) return false;
  const auto state = world.Presentation();
  if (state.commands.IsHidden() && state.slide == kHiddenSlide) return true;
  if (!DrawLimbsAndSlide(world) || !world.DrawActorGauge(HudGaugeKind::kHp) ||
      !world.DrawActorGauge(HudGaugeKind::kMp)) {
    return false;
  }
  const auto risk = world.ActorRisk();
  if (risk != 0) {
    if (!world.DrawRiskGauge(risk)) return false;
  } else if (!world.DrawZeroRisk(math::WrappingSubtract(
                 kZeroRiskPosition, world.Presentation().slide))) {
    return false;
  }
  if (world.Presentation().status_icons_hidden != 0) return true;
  const auto flags = world.ActorStatusFlags();
  if (!flags.has_value()) return false;
  const auto statuses = PrepareCarousel(world, flags.value());
  if (statuses.count == 0) return true;
  const int left = AdvanceCarousel(world, flags.value(), statuses.count);
  const int right = left + kCarouselSlideWidth;
  if (!world.DrawStatusIcon({.icon = world.Presentation().carousel[2],
                             .slide = right,
                             .slot = 1}) ||
      !world.DrawStatusIcon({.icon = world.Presentation().carousel[1],
                             .slide = left,
                             .slot = 1}) ||
      !world.DrawStatusIcon({.icon = world.Presentation().carousel[1],
                             .slide = right,
                             .slot = 0}) ||
      !world.DrawStatusIcon({.icon = world.Presentation().carousel[0],
                             .slide = left,
                             .slot = 0})) {
    return false;
  }
  return DrawStatusCount(world, statuses);
}

// decomp: F01384 _renderTimer (src/BATTLE/BATTLE.PRG/5BF94.c:3844).
void FormatPuzzleTimer(const State::SceneTime& time, std::uint8_t hundredths,
                       std::span<char, kPuzzleTimerTextBytes> text) {
  std::array<std::uint8_t, kTimerParts> parts{
      hundredths, static_cast<std::uint8_t>(time.seconds),
      static_cast<std::uint8_t>(time.minutes)};
  if (static_cast<std::uint8_t>(time.hours) != 0) {
    parts = {kSaturatedHundredths, kSaturatedClockPart, kSaturatedClockPart};
  }
  for (std::size_t index = 0; index < parts.size(); ++index) {
    const int digits = math::SplitDecimalDigit(parts[index]);
    const auto target = kTimerLastPair - index * kTimerDigitStride;
    text[target] = static_cast<char>((digits >> kDigitCellShift) + '0');
    text[target + 1] = static_cast<char>((digits & kDecimalDigitMask) + '0');
  }
}
}  // namespace VS::Battle
