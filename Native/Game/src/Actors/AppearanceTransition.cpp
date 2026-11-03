// Hand-maintained (tools/port/owned.csv).
// Palette pulse and appearance transitions over typed palette subjects
// (BATTLE.PRG 40564.c).
#include "Actors/AppearanceTransition.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Actors {
namespace {
namespace Math = VS::Core::Math;

constexpr int kPulseStep = 2;
constexpr std::uint8_t kPulseDescending = 2;
constexpr int kDoubleTickSpeed = 4;
constexpr int kChannelMaximum = 31;
constexpr int kGreenShift = 5;
constexpr int kBlueShift = 10;
constexpr std::uint16_t kOpaqueBit = 0x8000;
constexpr int kPeakRed = 8;
constexpr int kPeakGreen = 18;
constexpr int kPeakBlue = 30;
constexpr int kColorStep = 4;
constexpr int kReturnColorStep = 8;
constexpr int kMaximumColor = 255;
constexpr int kDelayFrames = 15;
constexpr int kByteRange = 256;
constexpr int kSignedByteMaximum = 127;

int SignedByte(std::uint8_t value) {
  return value <= kSignedByteMaximum ? value : value - kByteRange;
}

std::uint8_t Byte(int value) { return static_cast<std::uint8_t>(value); }

int ClampChannel(int channel) {
  return std::clamp(channel, 0, kChannelMaximum);
}

// func_800AC168's colour arithmetic: a 15-bit colour's channels as the
// original's word registers.
constexpr int kChannelBits = 0x1F;
constexpr int kColorBits = 0x7FFF;
constexpr std::uint16_t kWhiteColor = 0xFFFF;
constexpr int kDarkenScale = 4;

struct ColorWords {
  int red = 0;
  int green = 0;
  int blue = 0;
};

ColorWords WordsOf(std::uint16_t color) {
  return {.red = color & kChannelBits,
          .green = (color >> kGreenShift) & kChannelBits,
          .blue = (color >> kBlueShift) & kChannelBits};
}

// r | g << 5 | b << 10 over words: a channel outside 0 to 31 carries into its
// neighbour as in the original.
int PackWords(const ColorWords& words) {
  return words.red | (words.green << kGreenShift) | (words.blue << kBlueShift);
}

std::uint16_t Halfword(int value) { return static_cast<std::uint16_t>(value); }

int Lowered(int channel, int delta, int floor) {
  return std::max(Math::WrappingSubtract(channel, delta), floor);
}

int Raised(int channel, int delta) {
  return std::min(Math::WrappingAdd(channel, delta), kChannelMaximum);
}

bool RemoveRedGreen(std::uint16_t& color, int amount) {
  const ColorWords words = WordsOf(color);
  const int delta = Math::WrappingMultiply(amount, 2);
  const int red_green = PackWords({.red = Lowered(words.red, delta, 0),
                                   .green = Lowered(words.green, delta, 0)});
  color = Halfword(red_green | (words.blue << kBlueShift) | kOpaqueBit);
  return red_green != 0;
}

bool FadeToBlack(std::uint16_t& color, int delta) {
  const ColorWords words = WordsOf(color);
  const int packed = PackWords({.red = Lowered(words.red, delta, 0),
                                .green = Lowered(words.green, delta, 0),
                                .blue = Lowered(words.blue, delta, 0)});
  color = Halfword(packed | kOpaqueBit);
  return packed != 0;
}

void BrightenGreenBlue(std::uint16_t& color, int amount) {
  const ColorWords words = WordsOf(color);
  const int twice = Math::WrappingMultiply(amount, 2);
  color = Halfword(PackWords({.red = Raised(words.red, amount),
                              .green = Raised(words.green, twice),
                              .blue = Raised(words.blue, twice)}) |
                   kOpaqueBit);
}

bool RestoreToward(std::uint16_t& color, std::uint16_t reference, int amount) {
  const ColorWords words = WordsOf(color);
  const ColorWords floor = WordsOf(reference);
  const int delta = Math::WrappingMultiply(amount, 2);
  const int packed =
      PackWords({.red = Lowered(words.red, delta, floor.red),
                 .green = Lowered(words.green, delta, floor.green),
                 .blue = Lowered(words.blue, delta, floor.blue)});
  color = Halfword((reference & kOpaqueBit) | packed);
  return packed != (reference & kColorBits);
}

bool ApplyToColor(std::uint16_t& color, std::uint16_t reference,
                  PaletteTransition mode, int amount) {
  switch (mode) {
    case PaletteTransition::kRemoveRedGreen:
      return RemoveRedGreen(color, amount);
    case PaletteTransition::kDarken:
      return FadeToBlack(color, Math::WrappingMultiply(amount, kDarkenScale));
    case PaletteTransition::kWhite:
      color = kWhiteColor;
      return false;
    case PaletteTransition::kBrightenGreenBlue:
      BrightenGreenBlue(color, amount);
      return false;
    case PaletteTransition::kRestore:
      return RestoreToward(color, reference, amount);
    case PaletteTransition::kSlowDarken:
      return FadeToBlack(color, amount);
    case PaletteTransition::kBlack:
      color = kOpaqueBit;
      return false;
  }
  return false;
}

void SetRgb(AppearanceColors& colors, int value) {
  colors.rgb.fill(Byte(value));
}

bool IncreaseFade(AppearanceTransition& transition) {
  const int level =
      std::min(SignedByte(transition.colors.fade_level) + transition.speed * 2,
               kAppearanceFadeMaximum);
  transition.colors.fade_level = Byte(level);
  return level != kAppearanceFadeMaximum;
}

void FinishPaletteStep(AppearanceTransition& transition,
                       AppearanceSubject& subject, bool changed) {
  subject.UploadPalettes(transition.state);
  if (!changed) {
    ++transition.state;
    subject.PublishState(transition.state);
  }
}

bool Apply(AppearanceTransition& transition, AppearanceSubject& subject,
           PaletteTransition mode) {
  return subject.ApplyPalettes(mode, transition.speed);
}

// func_800AB9A4 state 4: the model darkens to black while its fade rises.
void AdvanceDisappearance(AppearanceTransition& transition,
                          AppearanceSubject& subject) {
  subject.ReleaseCarriedGimmick();
  const int value =
      std::max(transition.colors.rgb[0] - transition.speed * kColorStep, 0);
  SetRgb(transition.colors, value);
  bool changed = value != 0;
  changed |= IncreaseFade(transition);
  FinishPaletteStep(transition, subject, changed);
}

// func_800AB9A4 state 10: the fade and color return to the saved tint.
void AdvanceRestore(AppearanceTransition& transition,
                    AppearanceSubject& subject) {
  AppearanceColors& colors = transition.colors;
  const int target = SignedByte(colors.fade_target);
  const int level =
      std::max(SignedByte(colors.fade_level) - transition.speed * 2, target);
  colors.fade_level = Byte(level);
  bool changed = level != target;
  changed |= StepRgbTowardTarget(colors.rgb, colors.tint,
                                 -transition.speed * kReturnColorStep);
  changed |= Apply(transition, subject, PaletteTransition::kRestore);
  FinishPaletteStep(transition, subject, changed);
}

void AdvanceDeathDarken(AppearanceTransition& transition,
                        AppearanceSubject& subject) {
  const bool changed = Apply(transition, subject, PaletteTransition::kDarken);
  if (!changed) {
    SetRgb(transition.colors, kAppearanceFadeMaximum);
    transition.colors.delay = 0;
    (void)Apply(transition, subject, PaletteTransition::kWhite);
  }
  FinishPaletteStep(transition, subject, changed);
}

void AdvanceDelay(AppearanceTransition& transition,
                  AppearanceSubject& subject) {
  ++transition.colors.delay;
  bool changed = transition.colors.delay < kDelayFrames;
  (void)IncreaseFade(transition);
  changed |= Apply(transition, subject, PaletteTransition::kBrightenGreenBlue);
  FinishPaletteStep(transition, subject, changed);
}
}  // namespace

int AdvancePalettePulse(PalettePulse& pulse, int tick_speed) {
  int level = pulse.level;
  const int steps = tick_speed == kDoubleTickSpeed ? 2 : 1;
  for (int step = 0; step < steps; ++step) {
    if ((pulse.flags & kPulseDescending) == 0) {
      level += kPulseStep;
      if (level >= kPalettePulsePeak) {
        pulse.flags |= kPulseDescending;
      }
    } else {
      level -= kPulseStep;
      if (level <= 0) {
        pulse.flags &= static_cast<std::uint8_t>(~kPulseDescending);
      }
    }
  }
  pulse.level = Byte(level);
  return level;
}

std::uint16_t PulsePaletteColor(std::uint16_t color, int level) {
  const int red = color & kChannelMaximum;
  const int green = (color >> kGreenShift) & kChannelMaximum;
  const int blue = (color >> kBlueShift) & kChannelMaximum;
  const bool peak = level == kPalettePulsePeak;
  const int r = ClampChannel(red + (peak ? kPeakRed : -level / 2));
  const int g = ClampChannel(green + (peak ? kPeakGreen : level / 2));
  const int b = ClampChannel(blue + (peak ? kPeakBlue : level));
  return static_cast<std::uint16_t>((color & kOpaqueBit) | r |
                                    (g << kGreenShift) | (b << kBlueShift));
}

int UpdatePalettePulse(PalettePulse& pulse, int tick_speed,
                       std::span<const std::uint16_t> source,
                       std::span<std::uint16_t> destination) {
  const int level = AdvancePalettePulse(pulse, tick_speed);
  const std::size_t count = std::min(source.size(), destination.size());
  for (std::size_t index = 0; index < count; ++index) {
    if (source[index] != 0) {
      destination[index] = PulsePaletteColor(source[index], level);
    }
  }
  return level;
}

// func_800AC0D4 (BATTLE.PRG 40564.c:2003): the channels are bytes, so a step
// past zero wraps before the clamp compares them.
bool StepRgbTowardTarget(std::span<std::uint8_t, kRgbChannels> current,
                         std::span<const std::uint8_t, kRgbChannels> target,
                         int signed_step) {
  bool changed = false;
  for (std::size_t channel = 0; channel < current.size(); ++channel) {
    const std::uint8_t goal = target[channel];
    std::uint8_t& value = current[channel];
    const bool short_of_goal = signed_step < 0 ? goal < value : value < goal;
    if (!short_of_goal) {
      continue;
    }
    changed = true;
    value = Byte(value + signed_step);
    if (signed_step < 0 ? value < goal : goal < value) {
      value = goal;
    }
  }
  return changed;
}

bool ApplyPaletteTransition(std::span<std::uint16_t> colors,
                            std::span<const std::uint16_t> reference,
                            PaletteTransition mode, int amount) {
  const std::size_t count = std::min(colors.size(), reference.size());
  bool changed = false;
  for (std::size_t index = 0; index < count; ++index) {
    changed |= ApplyToColor(colors[index], reference[index], mode, amount);
  }
  return changed;
}

// func_800AB9A4 (BATTLE.PRG 40564.c:1769) after the death motion.
void AdvanceAppearanceTransition(AppearanceTransition& transition,
                                 AppearanceSubject& subject) {
  switch (transition.state) {
    case kAppearanceDeathFade:
      FinishPaletteStep(
          transition, subject,
          Apply(transition, subject, PaletteTransition::kRemoveRedGreen));
      break;
    case kAppearanceDeathDarken:
      AdvanceDeathDarken(transition, subject);
      break;
    case kAppearanceDisappearance:
      AdvanceDisappearance(transition, subject);
      break;
    case kAppearanceDelay:
      AdvanceDelay(transition, subject);
      break;
    case kAppearanceDarken:
      (void)IncreaseFade(transition);
      FinishPaletteStep(transition, subject,
                        Apply(transition, subject, PaletteTransition::kDarken));
      break;
    case kAppearanceWhite:
      SetRgb(transition.colors, kMaximumColor);
      FinishPaletteStep(transition, subject,
                        Apply(transition, subject, PaletteTransition::kWhite));
      break;
    case kAppearanceRestore:
      AdvanceRestore(transition, subject);
      break;
    case kAppearanceRestoreDone:
      transition.state = 0;
      break;
    case kAppearanceSlowDarken:
      FinishPaletteStep(
          transition, subject,
          Apply(transition, subject, PaletteTransition::kSlowDarken));
      break;
    case kAppearanceBlack:
      transition.colors.fade_level = Byte(kAppearanceFadeMaximum);
      FinishPaletteStep(transition, subject,
                        Apply(transition, subject, PaletteTransition::kBlack));
      break;
    default:
      break;
  }
}

void UpdateActorAppearanceTransition(ActorAppearance& actor) {
  if (actor.TransitionState() < kAppearanceDelay && actor.MovementMode() != 0) {
    actor.SettleDeathMotion();
    constexpr int kFallingMovementMode = 4;
    if (actor.MovementMode() >= kFallingMovementMode && !actor.IsPlayer()) {
      actor.PlayFallingDeathAnimation();
    }
  }
  AppearanceTransition transition = actor.LoadTransition();
  AdvanceAppearanceTransition(transition, actor);
  actor.StoreTransition(transition);
}
}  // namespace VS::Actors
