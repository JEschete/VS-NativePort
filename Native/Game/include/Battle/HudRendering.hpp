#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/Abilities.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Battle {
inline constexpr std::size_t kHudStatusIconCount = 32;
inline constexpr std::int32_t kAbsentHudStatusIcon = 255;

enum class HudGaugeKind : std::uint8_t {
  kHp,
  kMp,
  kRisk,
  kFixedPointRisk,
  kCount,
};
inline constexpr std::size_t kHudGaugeCount = 4;

struct HudGaugeValue {
  HudGaugeKind kind = HudGaugeKind::kHp;
  std::int32_t current = 0;
  std::int32_t maximum = 0;
};

struct HudGaugeLabelGeometry {
  std::int32_t position = 0;
  std::uint8_t width = 0;
};

class HudGaugeWorld {
 public:
  virtual ~HudGaugeWorld() = default;
  virtual std::optional<std::int32_t> GaugePosition(HudGaugeKind kind) = 0;
  virtual std::int32_t SlideOffset() = 0;
  virtual bool DrawGauge(const Render::StatGaugeRequest& gauge) = 0;
  virtual std::optional<std::int32_t> DrawNumber(
      const Render::NumberRequest& number) = 0;
  virtual bool DrawSeparator(std::int32_t position) = 0;
  virtual std::optional<HudGaugeLabelGeometry> LabelGeometry(
      HudGaugeKind kind) = 0;
  // Resolve the label's texture after queuing its sprite, not before.
  virtual bool DrawLabel(HudGaugeKind kind,
                         const Render::SpriteRequest& sprite) = 0;
};

enum class HudSpriteLayer : std::uint8_t { kStatus, kShortcut };

struct HudSprite {
  Render::SpriteRequest geometry{};
  std::uint32_t texture = 0;
};

class HudSpriteWorld {
 public:
  virtual ~HudSpriteWorld() = default;
  virtual bool DrawSprite(HudSpriteLayer layer, const HudSprite& sprite) = 0;
};

class HudStatusIconWorld : public HudSpriteWorld {
 public:
  virtual std::int32_t SlideOffset() = 0;
  virtual std::optional<std::uint16_t> StatusIconTexture(std::size_t icon) = 0;
};

struct SlidingHudStatusIcon {
  std::int32_t icon = kAbsentHudStatusIcon;
  std::int32_t slide = 0;
  std::int32_t slot = 0;
};

enum class HudShortcutTone : std::uint32_t {
  kHidden = 0,
  kEnabled = 0x180,
  kRestricted = 0x140,
};

class HudShortcutWorld {
 public:
  virtual ~HudShortcutWorld() = default;
  virtual std::optional<std::uint32_t> ActionRestrictions() = 0;
  virtual std::optional<bool> HasSpellClass(int spell_class) = 0;
  virtual std::optional<bool> HasBattleAbility(AbilityKind kind) = 0;
  virtual std::optional<bool> HasWeaponBreakArt() = 0;
};

// Packed coordinate arithmetic carries and wraps across the two halfwords.
// The maximum-number renderer's returned pen is consumed unchanged.
// decomp: F01354 _renderStatBar (src/BATTLE/BATTLE.PRG/5BF94.c:3088).
bool DrawHudStatGauge(HudGaugeWorld& world, const HudGaugeValue& gauge);
// Suppressed icons do not require a texture or a render target. Negative and
// overshooting slides retain the original packed widths, without clamping.
// decomp: F01355 func_800C9CB4 (src/BATTLE/BATTLE.PRG/5BF94.c:3112).
bool DrawSlidingStatusIcon(HudStatusIconWorld& world,
                           const SlidingHudStatusIcon& icon);
// Restrictions are sampled first, including for a noncanonical shortcut.
// Only the selected unlock service is called.
// decomp: F01358 func_800C9EB8 (src/BATTLE/BATTLE.PRG/5BF94.c:3197).
std::optional<HudShortcutTone> ShortcutIconBrightness(HudShortcutWorld& world,
                                                      std::int32_t shortcut);
// Brightness includes its original semi-transparency flag.
// decomp: F01360 func_800C9F98 (src/BATTLE/BATTLE.PRG/5BF94.c:3241).
bool DrawShortcutButtonMarker(HudSpriteWorld& world, std::int32_t position,
                              std::int32_t brightness_transparency);
}  // namespace VS::Battle
