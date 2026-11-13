// Hand-maintained (tools/port/owned.csv).
#include "Battle/HudRendering.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Battle {
namespace {
constexpr std::uint32_t kRiskNumberOffset = 0x00030036;
constexpr std::uint32_t kMaximumNumberOffset = 0xFFF80038;
constexpr std::uint32_t kCurrentNumberOffset = 0xFFFEFFF9;
constexpr std::uint32_t kGaugeLabelHeight = 0x00090000;
constexpr std::uint32_t kFixedPointRound = 255;
constexpr unsigned kFixedPointShift = 8;
constexpr unsigned kGaugeScaleShift = 6;
constexpr std::uint32_t kIconSide = 16;
constexpr std::uint32_t kIconLeft = 160;
constexpr unsigned kIconSlotShift = 4;
constexpr std::uint32_t kIconRow = 0x00080000;
constexpr std::uint32_t kIconSize = 0x00100000;
constexpr std::uint32_t kStatusClutMask = 0x0F0F906A;
constexpr std::uint32_t kStatusClut = 0x37F80000;
constexpr std::uint32_t kAlternateStatusClut = 0x37F90000;
constexpr std::uint32_t kStatusOverlayClut = 0x37FF0000;
constexpr std::uint32_t kOverlayIconBit = 16;
constexpr std::uint32_t kOverlaySide = 8;
constexpr std::uint32_t kOverlayAlignmentMask = 7;
constexpr std::uint32_t kOverlayCellMask = 3;
constexpr std::uint32_t kOverlayTextureBegin = 0x3068;
constexpr std::uint32_t kOverlayTextureEnd = 0x3070;
constexpr std::uint32_t kShortcutMarkerDrawMode = 0xE1000017;
constexpr std::uint32_t kShortcutMarkerTexture = 0x373D80C0;
constexpr int kShortcutMarkerSize = 0x00060006;
constexpr std::uint32_t kSpellRestrictions = 0xB7;
constexpr std::uint32_t kItemRestrictions = 7;
constexpr std::uint32_t kBreakArtRestrictions = 0x15F;
constexpr std::int32_t kItemShortcut = 4;
constexpr std::int32_t kDefenseShortcut = 5;
constexpr std::int32_t kChainShortcut = 6;
constexpr std::int32_t kBreakArtShortcut = 7;
constexpr std::array<int, 4> kShortcutSpellClasses{1, 2, 0, 3};

std::int32_t Signed(std::uint32_t value) {
  return std::bit_cast<std::int32_t>(value);
}

std::uint32_t Word(std::int32_t value) {
  return static_cast<std::uint32_t>(value);
}

std::int32_t GaugeFill(const HudGaugeValue& gauge) {
  const auto numerator = Signed((Word(gauge.current) << kGaugeScaleShift) +
                                Word(gauge.maximum) - 1U);
  const auto quotient = static_cast<std::int64_t>(numerator) / gauge.maximum;
  return Signed(static_cast<std::uint32_t>(quotient));
}

std::optional<std::int32_t> DrawGaugeNumbers(HudGaugeWorld& world,
                                             const HudGaugeValue& gauge,
                                             std::uint32_t position) {
  std::int32_t current = gauge.current;
  if ((static_cast<std::uint8_t>(gauge.kind) & 2U) != 0) {
    position += kRiskNumberOffset;
    if (gauge.kind == HudGaugeKind::kFixedPointRisk) {
      current = Signed(Word(current) + kFixedPointRound) >> kFixedPointShift;
    }
  } else {
    const auto maximum =
        world.DrawNumber({.font = 0,
                          .position = Signed(position + kMaximumNumberOffset),
                          .value = gauge.maximum});
    if (!maximum.has_value() || !world.DrawSeparator(maximum.value())) {
      return std::nullopt;
    }
    position = Word(maximum.value()) + kCurrentNumberOffset;
  }
  return world.DrawNumber(
      {.font = 1, .position = Signed(position), .value = current});
}

HudSprite StatusSprite(std::uint32_t x, std::uint32_t width,
                       std::uint32_t height, std::uint32_t texture) {
  return {
      .geometry = {.draw_mode = Render::kDefaultSpriteDrawMode,
                   .brightness_transparency = Render::kDefaultSpriteBrightness,
                   .packed_position = Signed(x | kIconRow),
                   .packed_size = Signed(width | height)},
      .texture = texture};
}

HudShortcutTone RestrictedTone(std::uint32_t restrictions, std::uint32_t mask) {
  return (restrictions & mask) != 0 ? HudShortcutTone::kRestricted
                                    : HudShortcutTone::kEnabled;
}
}  // namespace

// decomp: F01354 _renderStatBar (src/BATTLE/BATTLE.PRG/5BF94.c:3088).
bool DrawHudStatGauge(HudGaugeWorld& world, const HudGaugeValue& gauge) {
  if (static_cast<std::size_t>(gauge.kind) >= kHudGaugeCount) return false;
  const auto anchor = world.GaugePosition(gauge.kind);
  if (!anchor.has_value()) return false;
  const auto position = Word(anchor.value()) - Word(world.SlideOffset());
  if (gauge.maximum == 0) return true;
  if (!world.DrawGauge({.color_index = static_cast<std::uint32_t>(gauge.kind),
                        .fill_width = GaugeFill(gauge),
                        .position = Signed(position)}) ||
      !DrawGaugeNumbers(world, gauge, position).has_value()) {
    return false;
  }
  const auto label = world.LabelGeometry(gauge.kind);
  if (!label.has_value()) return false;
  return world.DrawLabel(
      gauge.kind,
      {.draw_mode = Render::kDefaultSpriteDrawMode,
       .brightness_transparency = static_cast<int>(HudShortcutTone::kEnabled),
       .packed_position =
           Signed(Word(label.value().position) - Word(world.SlideOffset())),
       .packed_size = Signed(label.value().width | kGaugeLabelHeight)});
}

// decomp: F01355 func_800C9CB4 (src/BATTLE/BATTLE.PRG/5BF94.c:3112).
bool DrawSlidingStatusIcon(HudStatusIconWorld& world,
                           const SlidingHudStatusIcon& icon) {
  if (icon.icon == kAbsentHudStatusIcon || world.SlideOffset() != 0) {
    return true;
  }
  if (Word(icon.icon) >= kHudStatusIconCount) return false;
  const auto texture =
      world.StatusIconTexture(static_cast<std::size_t>(icon.icon));
  if (!texture.has_value()) return false;
  std::uint32_t uv = texture.value();
  std::uint32_t x = (Word(icon.slot) << kIconSlotShift) + kIconLeft;
  std::uint32_t width = kIconSide;
  if (icon.slide < 0) {
    uv -= Word(icon.slide);
    width += Word(icon.slide);
  } else {
    x += Word(icon.slide);
    width -= Word(icon.slide);
  }
  const auto clut = ((kStatusClutMask >> icon.icon) & 1U) != 0
                        ? kAlternateStatusClut
                        : kStatusClut;
  if (!world.DrawSprite(HudSpriteLayer::kStatus,
                        StatusSprite(x, width, kIconSize, uv | clut))) {
    return false;
  }
  if ((Word(icon.icon) & kOverlayIconBit) == 0 || (x & kOverlaySide) != 0) {
    return true;
  }
  const auto cell = (Word(icon.icon) & kOverlayCellMask) * kOverlaySide;
  uv = cell + kOverlayTextureBegin;
  if (Signed(width) >= static_cast<std::int32_t>(kOverlaySide + 1U)) {
    if ((x & kOverlayAlignmentMask) != 0) {
      x += kOverlaySide;
      width -= kOverlaySide;
    } else {
      x += width - kOverlaySide;
      width = kOverlaySide;
    }
  } else {
    uv = cell + kOverlayTextureEnd - width;
  }
  return world.DrawSprite(
      HudSpriteLayer::kStatus,
      StatusSprite(x, width, kIconRow, uv | kStatusOverlayClut));
}

// decomp: F01358 func_800C9EB8 (src/BATTLE/BATTLE.PRG/5BF94.c:3197).
std::optional<HudShortcutTone> ShortcutIconBrightness(HudShortcutWorld& world,
                                                      std::int32_t shortcut) {
  const auto restrictions = world.ActionRestrictions();
  if (!restrictions.has_value()) return std::nullopt;
  std::optional<bool> unlocked;
  std::uint32_t mask = 0;
  if (Word(shortcut) < kShortcutSpellClasses.size()) {
    unlocked = world.HasSpellClass(
        kShortcutSpellClasses.at(static_cast<std::size_t>(shortcut)));
    mask = kSpellRestrictions;
  } else if (shortcut == kItemShortcut) {
    return RestrictedTone(restrictions.value(), kItemRestrictions);
  } else if (shortcut == kDefenseShortcut || shortcut == kChainShortcut) {
    unlocked = world.HasBattleAbility(shortcut == kChainShortcut
                                          ? AbilityKind::kChain
                                          : AbilityKind::kDefense);
  } else if (shortcut == kBreakArtShortcut) {
    unlocked = world.HasWeaponBreakArt();
    mask = kBreakArtRestrictions;
  } else {
    return HudShortcutTone::kEnabled;
  }
  if (!unlocked.has_value()) return std::nullopt;
  return unlocked.value() ? RestrictedTone(restrictions.value(), mask)
                          : HudShortcutTone::kHidden;
}

// decomp: F01360 func_800C9F98 (src/BATTLE/BATTLE.PRG/5BF94.c:3241).
bool DrawShortcutButtonMarker(HudSpriteWorld& world, std::int32_t position,
                              std::int32_t brightness_transparency) {
  return world.DrawSprite(
      HudSpriteLayer::kShortcut,
      {.geometry = {.draw_mode = kShortcutMarkerDrawMode,
                    .brightness_transparency = brightness_transparency,
                    .packed_position = position,
                    .packed_size = kShortcutMarkerSize},
       .texture = kShortcutMarkerTexture});
}
}  // namespace VS::Battle
