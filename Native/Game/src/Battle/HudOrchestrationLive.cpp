// Hand-maintained (tools/port/owned.csv).
#include "Battle/HudOrchestrationLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudOrchestration.hpp"
#include "Battle/HudRenderingLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBox.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFont.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"

namespace VS::Battle {
namespace {
constexpr std::size_t kQueuedHudSprites = 32;
constexpr std::size_t kHudLimbLayouts = kHudDisplayedLimbs * 2;
constexpr std::size_t kHudLimbColors = 5;
constexpr std::size_t kShortcutClosingSteps = 12;
constexpr std::size_t kShortcutRowSteps = 20;
constexpr std::size_t kLimbCoveringTextBox = 7;
constexpr int kLimbLayer = -1;
constexpr int kCountLayer = -2;
constexpr int kIndicatorLayer = -3;
constexpr int kTimerLayer = -1;
constexpr int kTimerPosition = 0xC80080;
constexpr int kRiskMaximum = 100;
constexpr std::size_t kTimerCharacters = 8;
constexpr int kCoordinateShift = 16;

[[noreturn]] void Refuse(const char* message) {
  VS::Core::Errorf("battle.hud", "%s", message);
  std::abort();
}

std::uint32_t* CheckedChain(std::uint32_t* chain) {
  if (chain == nullptr) {
    Refuse("HUD presentation has no bounded ordering entry");
  }
  return chain;
}

std::uint32_t* CurrentChain(Render::LiveOrderingBase base, int layer) {
  return CheckedChain(Render::LiveFrameOrderingEntry(base, layer));
}

bool DrawText(std::string_view text, std::int32_t position, int layer) {
  Render::LivePacketQueue queue(
      CurrentChain(Render::LiveOrderingBase::kScene, layer));
  const bool drawn =
      Render::DrawCompactText(queue,
                              {.widths = VS::Render::compactGlyphWidths,
                               .left_trim = VS::Render::compactGlyphLeftTrim,
                               .indices = VS::Render::compactGlyphIndices},
                              text, position, Render::kCompactTextGray);
  if (!drawn) Refuse("HUD text has insufficient primitive capacity");
  return true;
}

class LiveHudSpriteConsumer final : public HudSpriteConsumer {
 public:
  void Bind(std::uint32_t* packet) { packet_ = packet; }
  void SetColor(std::uint32_t color) override {
    packet_[Render::kSpriteColorWord] = color;
  }
  void SetTexture(std::uint32_t texture) override {
    packet_[Render::kSpriteTextureWord] = texture;
  }
  void ModulateColor(std::int32_t delta) override {
    packet_[Render::kSpriteColorWord] += static_cast<std::uint32_t>(delta);
  }

 private:
  std::uint32_t* packet_ = nullptr;
};

class LiveHudOrchestrationWorld final : public HudOrchestrationWorld {
 public:
  explicit LiveHudOrchestrationWorld(bool capture_actor) {
    if (capture_actor) {
      if (VS::Battle::characterState == nullptr ||
          VS::Battle::characterState->stats == nullptr) {
        Refuse("Battle HUD requires its current actor and stats");
      }
      stats_ = VS::Battle::characterState->stats;
      limb_chain_ = Render::LiveFrameOrderingEntry(
          Render::LiveOrderingBase::kScene, kLimbLayer);
      count_chain_ = Render::LiveFrameOrderingEntry(
          Render::LiveOrderingBase::kScene, kCountLayer);
    }
  }

  HudPresentationState Presentation() override {
    return {.commands = ReadLiveHudState(),
            .slide = VS::Battle::hudSlideOffset,
            .shortcut_step = VS::Battle::shortcutHudAnimationStep,
            .indicator_step =
                static_cast<std::uint8_t>(VS::Battle::battleModeIndicatorStep),
            .status_icons_hidden =
                static_cast<std::uint8_t>(VS::Battle::hudStatusIconsHidden),
            .indicator_phase = VS::Battle::battleModeIndicatorWobble,
            .carousel = VS::Battle::statusIconCarousel.bytes};
  }
  void SetShortcutRequest(std::uint8_t command) override {
    VS::Battle::shortcutHudRequested = static_cast<char>(command);
  }
  void SetShortcutStep(std::uint8_t step) override {
    VS::Battle::shortcutHudAnimationStep = step;
  }
  void SetIndicatorStep(std::uint8_t step) override {
    VS::Battle::battleModeIndicatorStep = static_cast<char>(step);
  }
  void SetIndicatorPhase(std::uint16_t phase) override {
    VS::Battle::battleModeIndicatorWobble = phase;
  }
  void SetSlide(std::int32_t slide) override {
    VS::Battle::hudSlideOffset = slide;
  }
  void SetCarousel(std::size_t slot, std::uint8_t icon) override {
    if (slot >= std::size(VS::Battle::statusIconCarousel.bytes)) {
      Refuse("Status carousel slot is outside its owner");
    }
    std::span(VS::Battle::statusIconCarousel.bytes)[slot] = icon;
  }
  std::optional<std::uint8_t> ClosingShortcutStep(std::uint8_t step) override {
    const auto steps = std::span(VS::Menu::Framework::animationIndices)
                           .first<kShortcutClosingSteps>();
    if (step >= steps.size()) return std::nullopt;
    return static_cast<std::uint8_t>(steps[step]);
  }
  std::optional<std::uint8_t> ShortcutDisplacement(std::uint8_t step) override {
    const auto steps = std::span(VS::Menu::Framework::rowAnimationSteps)
                           .first<kShortcutRowSteps>();
    if (step >= steps.size()) return std::nullopt;
    return static_cast<std::uint8_t>(steps[step]);
  }
  std::optional<HudShortcutTone> ShortcutTone(int shortcut) override {
    return static_cast<HudShortcutTone>(LiveShortcutIconBrightness(shortcut));
  }
  std::optional<HudPoint> ShortcutMarkerPosition(HudShortcutRow row,
                                                 std::size_t slot) override {
    if (slot >= kHudShortcutRowSize) return std::nullopt;
    const std::span<const std::uint8_t> positions =
        row == HudShortcutRow::kAbilities
            ? std::span(VS::Battle::secondShortcutMarkerPositions)
                  .first<kHudShortcutRowSize * 2>()
            : std::span(VS::Battle::firstShortcutMarkerPositions)
                  .first<kHudShortcutRowSize * 2>();
    return HudPoint{.x = positions[slot],
                    .y = positions[slot + kHudShortcutRowSize]};
  }
  std::optional<HudElementGeometry> ShortcutIconGeometry(
      HudShortcutRow row, std::size_t slot) override {
    if (slot >= kHudShortcutRowSize) return std::nullopt;
    const std::span<const std::uint8_t> positions =
        row == HudShortcutRow::kAbilities
            ? std::span(VS::Battle::secondShortcutIconPositions)
                  .first<kHudShortcutRowSize * 2>()
            : std::span(VS::Battle::firstShortcutIconPositions)
                  .first<kHudShortcutRowSize * 2>();
    const std::span<const int> sizes =
        row == HudShortcutRow::kAbilities
            ? std::span(VS::Battle::secondShortcutIconSizes)
                  .first<kHudShortcutRowSize>()
            : std::span(VS::Battle::firstShortcutIconSizes)
                  .first<kHudShortcutRowSize>();
    return HudElementGeometry{
        .position = positions[slot] |
                    (positions[slot + kHudShortcutRowSize] << kCoordinateShift),
        .size = sizes[slot]};
  }
  std::optional<std::uint16_t> ShortcutIconTexture(HudShortcutRow row,
                                                   std::size_t slot) override {
    if (slot >= kHudShortcutRowSize) return std::nullopt;
    return row == HudShortcutRow::kAbilities
               ? std::span(VS::Battle::secondShortcutIconTexCoords)
                     .first<kHudShortcutRowSize>()[slot]
               : std::span(VS::Battle::firstShortcutIconTexCoords)
                     .first<kHudShortcutRowSize>()[slot];
  }
  std::optional<std::int32_t> ShortcutDigitPosition(
      std::size_t digit) override {
    if (digit >= kHudShortcutRowSize) return std::nullopt;
    return std::span(VS::Battle::firstShortcutDigitOffsets)
        .first<kHudShortcutRowSize>()[digit];
  }
  bool DrawShortcutMarker(std::int32_t position,
                          HudShortcutTone tone) override {
    DrawLiveShortcutButtonMarker(position, static_cast<int>(tone));
    return true;
  }
  HudSpriteConsumer* QueueSprite(HudPresentationLayer layer,
                                 const Render::SpriteRequest& sprite) override {
    std::uint32_t* chain = nullptr;
    switch (layer) {
      case HudPresentationLayer::kShortcut:
        chain = CurrentChain(Render::LiveOrderingBase::kOverlay, 0);
        break;
      case HudPresentationLayer::kIndicator:
        chain = CurrentChain(Render::LiveOrderingBase::kScene, kIndicatorLayer);
        break;
      case HudPresentationLayer::kLimbs:
        chain = CheckedChain(limb_chain_);
        break;
      case HudPresentationLayer::kStatusCount:
        chain = CheckedChain(count_chain_);
        break;
    }
    if (used_sprites_ >= sprites_.size() ||
        Render::ReserveLivePrimitiveWords(Render::kSpritePacketWords) ==
            nullptr) {
      Refuse("HUD sprite has insufficient primitive capacity");
    }
    const bool page =
        layer != HudPresentationLayer::kIndicator && sprite.draw_mode != 0;
    std::uint32_t* const packet =
        page ? Render::QueueLiveSpriteWithTexturePage(
                   sprite.brightness_transparency, sprite.packed_position,
                   sprite.packed_size, chain)
             : Render::QueueLiveSprite(sprite.brightness_transparency,
                                       sprite.packed_position,
                                       sprite.packed_size, chain);
    if (packet == nullptr) Refuse("HUD sprite renderer returned no consumer");
    packet[Render::kSpriteDrawModeWord] = sprite.draw_mode;
    auto& consumer = sprites_.at(used_sprites_++);
    consumer.Bind(packet);
    return &consumer;
  }
  void DrawShortcutPanel() override { DrawLiveShortcutHud(); }
  std::int32_t Sine(std::uint16_t phase) override {
    return Core::Math::Sine(phase);
  }
  std::int32_t TickSpeed() override { return VS::State::tickSpeed; }
  bool LimbDisplayAllowed() override {
    const auto& box = std::span(VS::Menu::textBoxes)[kLimbCoveringTextBox];
    return !ReadLiveHudState().SuppressesLimbDisplay() &&
           (box.state == 0 ||
            box.state != static_cast<std::uint8_t>(box.open_duration) + 1);
  }
  std::optional<HudElementGeometry> LimbGeometry(std::size_t layout) override {
    if (layout >= kHudLimbLayouts) return std::nullopt;
    return HudElementGeometry{
        .position = std::span(VS::Battle::limbDisplaySpriteOffsets)
                        .first<kHudLimbLayouts>()[layout],
        .size = std::span(VS::Battle::limbDisplaySpriteSizes)
                    .first<kHudLimbLayouts>()[layout]};
  }
  std::optional<HudLimbCondition> CurrentLimbCondition(
      std::size_t limb) override {
    auto* const actor = VS::Battle::characterState;
    if (actor == nullptr || actor->stats == nullptr ||
        limb >= std::size(actor->stats->limbs)) {
      return std::nullopt;
    }
    return static_cast<HudLimbCondition>(
        LiveLimbCondition(&std::span(actor->stats->limbs)[limb]));
  }
  std::optional<std::uint32_t> LimbColor(HudLimbCondition condition) override {
    const auto index = static_cast<std::size_t>(condition);
    if (index >= kHudLimbColors) return std::nullopt;
    return std::span(VS::Battle::limbStatusColors)
        .first<kHudLimbColors>()[index];
  }
  std::optional<std::uint32_t> LimbTexture(std::size_t layout) override {
    if (layout >= kHudLimbLayouts) return std::nullopt;
    return std::span(VS::Battle::limbDisplaySpriteTexCoords)
        .first<kHudLimbLayouts>()[layout];
  }
  bool DrawActorGauge(HudGaugeKind kind) override {
    if (stats_ == nullptr) return false;
    if (kind == HudGaugeKind::kHp) {
      DrawLiveHudStatGauge(static_cast<int>(kind), stats_->current_hp,
                           stats_->max_hp);
    } else {
      DrawLiveHudStatGauge(static_cast<int>(kind), stats_->current_mp,
                           stats_->max_mp);
    }
    return true;
  }
  std::uint16_t ActorRisk() override {
    if (stats_ == nullptr) Refuse("HUD risk has no captured stats owner");
    return stats_->risk;
  }
  bool DrawRiskGauge(std::uint16_t risk) override {
    DrawLiveHudStatGauge(static_cast<int>(HudGaugeKind::kRisk), risk,
                         kRiskMaximum);
    return true;
  }
  bool DrawZeroRisk(std::int32_t position) override {
    return DrawText("RISK  ZERO", position, kIndicatorLayer);
  }
  std::optional<std::uint32_t> ActorStatusFlags() override {
    if (stats_ == nullptr) return std::nullopt;
    return static_cast<std::uint32_t>(LiveStatusIconFlags(stats_));
  }
  bool DrawStatusIcon(const SlidingHudStatusIcon& icon) override {
    DrawLiveSlidingStatusIcon(icon.icon, icon.slide, icon.slot);
    return true;
  }

 private:
  VS::Battle::ActorStats* stats_ = nullptr;
  std::uint32_t* limb_chain_ = nullptr;
  std::uint32_t* count_chain_ = nullptr;
  std::array<LiveHudSpriteConsumer, kQueuedHudSprites> sprites_{};
  std::size_t used_sprites_ = 0;
};
}  // namespace

// decomp: F00729 func_80095B70 (src/BATTLE/BATTLE.PRG/2842C.c:2379).
void SetLiveActorMarkersHidden(int command) {
  HudFeedbackState state;
  SetActorMarkersHidden(state, command);
  VS::Battle::actorMarkersHidden = state.marker_visibility_command;
}

// decomp: F00730 func_80095B7C (src/BATTLE/BATTLE.PRG/2842C.c:2381).
void SetLiveActorMarker(int actor_slot, int command) {
  auto markers = std::as_writable_bytes(
      std::span(VS::Battle::actorMarkers).first<kActorMarkerCount>());
  if (!SetActorMarker(markers, actor_slot, command)) {
    Refuse("Actor marker slot is outside its sixteen-byte owner");
  }
}

// decomp: F00743 func_8009722C (src/BATTLE/BATTLE.PRG/2842C.c:2893).
void SuppressLiveAshleyStatusLabels() {
  HudFeedbackState state;
  SuppressAshleyStatusLabels(state);
  VS::Battle::ashleyStatusLabelsSuppressed = state.ashley_label_suppression;
}

// decomp: F01361 func_800C9FE8 (src/BATTLE/BATTLE.PRG/5BF94.c:3249).
void DrawLiveShortcutHud() {
  LiveHudOrchestrationWorld world(false);
  if (!DrawShortcutHud(world)) {
    Refuse("Shortcut HUD requires its used animation, layout and services");
  }
}

// decomp: F01362 func_800CA2DC (src/BATTLE/BATTLE.PRG/5BF94.c:3307).
void DrawLiveBattleHud() {
  LiveHudOrchestrationWorld world(true);
  if (!DrawBattleHud(world)) {
    Refuse("Battle HUD requires its used actor, layout and services");
  }
}

// decomp: F01384 _renderTimer (src/BATTLE/BATTLE.PRG/5BF94.c:3844).
void DrawLivePuzzleTimer(const State::SceneTimer* timer) {
  if (timer == nullptr) Refuse("Puzzle HUD requires its scene timer");
  const State::SceneTime& time = timer->time;
  const auto hundredths =
      static_cast<std::uint8_t>(State::stateFlags.puzzle_timer_hundredths);
  FormatPuzzleTimer(time, hundredths, std::span(VS::Battle::puzzleTimerText));
  VS::Core::Debugf("hud.timer",
                   "text=%s seconds=%d minutes=%d hours=%d frames=%d",
                   VS::Battle::puzzleTimerText.data(), time.seconds,
                   time.minutes, time.hours, time.frames);
  DrawText(
      std::string_view(VS::Battle::puzzleTimerText.data(), kTimerCharacters),
      kTimerPosition, kTimerLayer);
}
}  // namespace VS::Battle
