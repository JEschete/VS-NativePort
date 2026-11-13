#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/HudRendering.hpp"
#include "Battle/HudState.hpp"
#include "State/SceneTimer.hpp"

namespace VS::Battle {
inline constexpr std::size_t kActorMarkerCount = 16;
inline constexpr std::size_t kHudShortcutRowSize = 4;
inline constexpr std::size_t kHudDisplayedLimbs = 5;
inline constexpr std::size_t kHudCarouselSize = 4;
inline constexpr std::size_t kPuzzleTimerTextBytes = 9;

struct HudFeedbackState {
  std::int16_t marker_visibility_command = 0;
  std::int16_t ashley_label_suppression = 0;
};

struct HudPresentationState {
  HudState commands{};
  std::int32_t slide = 0;
  std::uint8_t shortcut_step = 0;
  std::uint8_t indicator_step = 0;
  std::uint8_t status_icons_hidden = 0;
  std::uint16_t indicator_phase = 0;
  std::array<std::uint8_t, kHudCarouselSize> carousel{};
};

enum class HudShortcutRow : std::uint8_t { kAbilities, kSpells };
enum class HudPresentationLayer : std::uint8_t {
  kShortcut,
  kIndicator,
  kLimbs,
  kStatusCount,
};

struct HudPoint {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
};

struct HudElementGeometry {
  std::int32_t position = 0;
  std::int32_t size = 0;
};

// A queued sprite remains bound while a later callback changes its owners.
// In particular, modulation consumes its current packed color, not a copy
// of the original request. Carries are not clamped to individual channels.
class HudSpriteConsumer {
 public:
  virtual ~HudSpriteConsumer() = default;
  virtual void SetColor(std::uint32_t color) = 0;
  virtual void SetTexture(std::uint32_t texture) = 0;
  virtual void ModulateColor(std::int32_t delta) = 0;
};

class HudOrchestrationWorld {
 public:
  virtual ~HudOrchestrationWorld() = default;
  virtual HudPresentationState Presentation() = 0;
  virtual void SetShortcutRequest(std::uint8_t command) = 0;
  virtual void SetShortcutStep(std::uint8_t step) = 0;
  virtual void SetIndicatorStep(std::uint8_t step) = 0;
  virtual void SetIndicatorPhase(std::uint16_t phase) = 0;
  virtual void SetSlide(std::int32_t slide) = 0;
  virtual void SetCarousel(std::size_t slot, std::uint8_t icon) = 0;
  virtual std::optional<std::uint8_t> ClosingShortcutStep(
      std::uint8_t step) = 0;
  virtual std::optional<std::uint8_t> ShortcutDisplacement(
      std::uint8_t step) = 0;
  virtual std::optional<HudShortcutTone> ShortcutTone(int shortcut) = 0;
  virtual std::optional<HudPoint> ShortcutMarkerPosition(HudShortcutRow row,
                                                         std::size_t slot) = 0;
  virtual std::optional<HudElementGeometry> ShortcutIconGeometry(
      HudShortcutRow row, std::size_t slot) = 0;
  virtual std::optional<std::uint16_t> ShortcutIconTexture(
      HudShortcutRow row, std::size_t slot) = 0;
  virtual std::optional<std::int32_t> ShortcutDigitPosition(
      std::size_t digit) = 0;
  virtual bool DrawShortcutMarker(std::int32_t position,
                                  HudShortcutTone tone) = 0;
  virtual HudSpriteConsumer* QueueSprite(
      HudPresentationLayer layer, const Render::SpriteRequest& sprite) = 0;
  virtual void DrawShortcutPanel() = 0;
  virtual std::int32_t Sine(std::uint16_t phase) = 0;
  virtual std::int32_t TickSpeed() = 0;
  virtual bool LimbDisplayAllowed() = 0;
  virtual std::optional<HudElementGeometry> LimbGeometry(
      std::size_t layout) = 0;
  virtual std::optional<HudLimbCondition> CurrentLimbCondition(
      std::size_t limb) = 0;
  virtual std::optional<std::uint32_t> LimbColor(
      HudLimbCondition condition) = 0;
  virtual std::optional<std::uint32_t> LimbTexture(std::size_t layout) = 0;
  // The binding captures the stats owner before DrawShortcutPanel. It
  // refreshes values from that owner separately for each subsequent call.
  virtual bool DrawActorGauge(HudGaugeKind kind) = 0;
  virtual std::uint16_t ActorRisk() = 0;
  virtual bool DrawRiskGauge(std::uint16_t risk) = 0;
  virtual bool DrawZeroRisk(std::int32_t position) = 0;
  virtual std::optional<std::uint32_t> ActorStatusFlags() = 0;
  virtual bool DrawStatusIcon(const SlidingHudStatusIcon& icon) = 0;
};

// decomp: F00729 func_80095B70 (src/BATTLE/BATTLE.PRG/2842C.c:2379).
void SetActorMarkersHidden(HudFeedbackState& state, std::int32_t command);
// Slot zero fills all sixteen command bytes. The two flag commands have
// priority over replacing a nonzero slot's low seven bits.
// decomp: F00730 func_80095B7C (src/BATTLE/BATTLE.PRG/2842C.c:2381).
bool SetActorMarker(std::span<std::byte, kActorMarkerCount> markers,
                    std::int32_t slot, std::int32_t command);
// decomp: F00743 func_8009722C (src/BATTLE/BATTLE.PRG/2842C.c:2893).
void SuppressAshleyStatusLabels(HudFeedbackState& state);
// decomp: F01361 func_800C9FE8 (src/BATTLE/BATTLE.PRG/5BF94.c:3249).
bool DrawShortcutHud(HudOrchestrationWorld& world);
// decomp: F01362 func_800CA2DC (src/BATTLE/BATTLE.PRG/5BF94.c:3307).
bool DrawBattleHud(HudOrchestrationWorld& world);
// The clock's bytes are zero-extended, including nominally signed fields.
// The punctuation and terminator belong to the native text owner and survive.
// decomp: F01384 _renderTimer (src/BATTLE/BATTLE.PRG/5BF94.c:3844).
void FormatPuzzleTimer(const State::SceneTime& time, std::uint8_t hundredths,
                       std::span<char, kPuzzleTimerTextBytes> text);
}  // namespace VS::Battle
