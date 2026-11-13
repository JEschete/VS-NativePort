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

#include "Battle/HudOrchestration.hpp"
#include "Battle/HudRenderingLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/GameFont.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Core/Diagnostics.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "State/SceneTimer.h"
#include "State/Timers.h"
}

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
  Core_Errorf("battle.hud", "%s", message);
  std::abort();
}

u_long* CheckedChain(u_long* chain) {
  if (chain == nullptr) {
    Refuse("HUD presentation has no bounded ordering entry");
  }
  return chain;
}

u_long* CurrentChain(Render::LiveOrderingBase base, int layer) {
  return CheckedChain(Render::LiveFrameOrderingEntry(base, layer));
}

bool DrawText(std::string_view text, std::int32_t position, int layer) {
  Render::LivePacketQueue queue(
      CurrentChain(Render::LiveOrderingBase::kScene, layer));
  const bool drawn =
      Render::DrawCompactText(queue,
                              {.widths = Render_compactGlyphWidths,
                               .left_trim = Render_compactGlyphLeftTrim,
                               .indices = Render_compactGlyphIndices},
                              text, position, Render::kCompactTextGray);
  if (!drawn) Refuse("HUD text has insufficient primitive capacity");
  return true;
}

class LiveHudSpriteConsumer final : public HudSpriteConsumer {
 public:
  void Bind(u_long* packet) { packet_ = packet; }
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
  u_long* packet_ = nullptr;
};

class LiveHudOrchestrationWorld final : public HudOrchestrationWorld {
 public:
  explicit LiveHudOrchestrationWorld(bool capture_actor) {
    if (capture_actor) {
      if (Battle_characterState == nullptr ||
          Battle_characterState->stats == nullptr) {
        Refuse("Battle HUD requires its current actor and stats");
      }
      stats_ = Battle_characterState->stats;
      limb_chain_ = Render::LiveFrameOrderingEntry(
          Render::LiveOrderingBase::kScene, kLimbLayer);
      count_chain_ = Render::LiveFrameOrderingEntry(
          Render::LiveOrderingBase::kScene, kCountLayer);
    }
  }

  HudPresentationState Presentation() override {
    return {.commands = ReadLiveHudState(),
            .slide = Battle_hudSlideOffset,
            .shortcut_step = Battle_shortcutHudAnimationStep,
            .indicator_step =
                static_cast<std::uint8_t>(Battle_battleModeIndicatorStep),
            .status_icons_hidden =
                static_cast<std::uint8_t>(Battle_hudStatusIconsHidden),
            .indicator_phase = Battle_battleModeIndicatorWobble,
            .carousel = std::to_array(Battle_statusIconCarousel.bytes)};
  }
  void SetShortcutRequest(std::uint8_t command) override {
    Battle_shortcutHudRequested = static_cast<char>(command);
  }
  void SetShortcutStep(std::uint8_t step) override {
    Battle_shortcutHudAnimationStep = step;
  }
  void SetIndicatorStep(std::uint8_t step) override {
    Battle_battleModeIndicatorStep = static_cast<char>(step);
  }
  void SetIndicatorPhase(std::uint16_t phase) override {
    Battle_battleModeIndicatorWobble = phase;
  }
  void SetSlide(std::int32_t slide) override { Battle_hudSlideOffset = slide; }
  void SetCarousel(std::size_t slot, std::uint8_t icon) override {
    if (slot >= std::size(Battle_statusIconCarousel.bytes)) {
      Refuse("Status carousel slot is outside its owner");
    }
    std::span(Battle_statusIconCarousel.bytes)[slot] = icon;
  }
  std::optional<std::uint8_t> ClosingShortcutStep(std::uint8_t step) override {
    const auto steps = std::span(Menu_Framework_animationIndices)
                           .first<kShortcutClosingSteps>();
    if (step >= steps.size()) return std::nullopt;
    return static_cast<std::uint8_t>(steps[step]);
  }
  std::optional<std::uint8_t> ShortcutDisplacement(std::uint8_t step) override {
    const auto steps =
        std::span(Menu_Framework_rowAnimationSteps).first<kShortcutRowSteps>();
    if (step >= steps.size()) return std::nullopt;
    return static_cast<std::uint8_t>(steps[step]);
  }
  std::optional<HudShortcutTone> ShortcutTone(int shortcut) override {
    return static_cast<HudShortcutTone>(LiveShortcutIconBrightness(shortcut));
  }
  std::optional<HudPoint> ShortcutMarkerPosition(HudShortcutRow row,
                                                 std::size_t slot) override {
    if (slot >= kHudShortcutRowSize) return std::nullopt;
    const std::span<const u_char> positions =
        row == HudShortcutRow::kAbilities
            ? std::span(Battle_secondShortcutMarkerPositions)
                  .first<kHudShortcutRowSize * 2>()
            : std::span(Battle_firstShortcutMarkerPositions)
                  .first<kHudShortcutRowSize * 2>();
    return HudPoint{.x = positions[slot],
                    .y = positions[slot + kHudShortcutRowSize]};
  }
  std::optional<HudElementGeometry> ShortcutIconGeometry(
      HudShortcutRow row, std::size_t slot) override {
    if (slot >= kHudShortcutRowSize) return std::nullopt;
    const std::span<const u_char> positions =
        row == HudShortcutRow::kAbilities
            ? std::span(Battle_secondShortcutIconPositions)
                  .first<kHudShortcutRowSize * 2>()
            : std::span(Battle_firstShortcutIconPositions)
                  .first<kHudShortcutRowSize * 2>();
    const std::span<const int> sizes =
        row == HudShortcutRow::kAbilities
            ? std::span(Battle_secondShortcutIconSizes)
                  .first<kHudShortcutRowSize>()
            : std::span(Battle_firstShortcutIconSizes)
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
               ? std::span(Battle_secondShortcutIconTexCoords)
                     .first<kHudShortcutRowSize>()[slot]
               : std::span(Battle_firstShortcutIconTexCoords)
                     .first<kHudShortcutRowSize>()[slot];
  }
  std::optional<std::int32_t> ShortcutDigitPosition(
      std::size_t digit) override {
    if (digit >= kHudShortcutRowSize) return std::nullopt;
    return std::span(Battle_firstShortcutDigitOffsets)
        .first<kHudShortcutRowSize>()[digit];
  }
  bool DrawShortcutMarker(std::int32_t position,
                          HudShortcutTone tone) override {
    DrawLiveShortcutButtonMarker(position, static_cast<int>(tone));
    return true;
  }
  HudSpriteConsumer* QueueSprite(HudPresentationLayer layer,
                                 const Render::SpriteRequest& sprite) override {
    u_long* chain = nullptr;
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
    u_long* const packet =
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
  std::int32_t TickSpeed() override { return State_tickSpeed; }
  bool LimbDisplayAllowed() override {
    const auto& box = std::span(Menu_textBoxes)[kLimbCoveringTextBox];
    return !ReadLiveHudState().SuppressesLimbDisplay() &&
           (box.state == 0 ||
            box.state != static_cast<std::uint8_t>(box.openDuration) + 1);
  }
  std::optional<HudElementGeometry> LimbGeometry(std::size_t layout) override {
    if (layout >= kHudLimbLayouts) return std::nullopt;
    return HudElementGeometry{
        .position = std::span(Battle_limbDisplaySpriteOffsets)
                        .first<kHudLimbLayouts>()[layout],
        .size = std::span(Battle_limbDisplaySpriteSizes)
                    .first<kHudLimbLayouts>()[layout]};
  }
  std::optional<HudLimbCondition> CurrentLimbCondition(
      std::size_t limb) override {
    auto* const actor = Battle_characterState;
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
    return std::span(Battle_limbStatusColors).first<kHudLimbColors>()[index];
  }
  std::optional<std::uint32_t> LimbTexture(std::size_t layout) override {
    if (layout >= kHudLimbLayouts) return std::nullopt;
    return std::span(Battle_limbDisplaySpriteTexCoords)
        .first<kHudLimbLayouts>()[layout];
  }
  bool DrawActorGauge(HudGaugeKind kind) override {
    if (stats_ == nullptr) return false;
    if (kind == HudGaugeKind::kHp) {
      DrawLiveHudStatGauge(static_cast<int>(kind), stats_->currentHp,
                           stats_->maxHp);
    } else {
      DrawLiveHudStatGauge(static_cast<int>(kind), stats_->currentMp,
                           stats_->maxMp);
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
  Battle_ActorStats* stats_ = nullptr;
  u_long* limb_chain_ = nullptr;
  u_long* count_chain_ = nullptr;
  std::array<LiveHudSpriteConsumer, kQueuedHudSprites> sprites_{};
  std::size_t used_sprites_ = 0;
};
}  // namespace

// decomp: F00729 func_80095B70 (src/BATTLE/BATTLE.PRG/2842C.c:2379).
void SetLiveActorMarkersHidden(int command) {
  HudFeedbackState state;
  SetActorMarkersHidden(state, command);
  Battle_actorMarkersHidden = state.marker_visibility_command;
}

// decomp: F00730 func_80095B7C (src/BATTLE/BATTLE.PRG/2842C.c:2381).
void SetLiveActorMarker(int actor_slot, int command) {
  auto markers = std::as_writable_bytes(
      std::span(Battle_actorMarkers).first<kActorMarkerCount>());
  if (!SetActorMarker(markers, actor_slot, command)) {
    Refuse("Actor marker slot is outside its sixteen-byte owner");
  }
}

// decomp: F00743 func_8009722C (src/BATTLE/BATTLE.PRG/2842C.c:2893).
void SuppressLiveAshleyStatusLabels() {
  HudFeedbackState state;
  SuppressAshleyStatusLabels(state);
  Battle_ashleyStatusLabelsSuppressed = state.ashley_label_suppression;
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
void DrawLivePuzzleTimer(const State_SceneTimer* timer) {
  if (timer == nullptr) Refuse("Puzzle HUD requires its scene timer");
  const State::SceneTime time{.frames = timer->frames,
                              .seconds = timer->seconds,
                              .minutes = timer->minutes,
                              .hours = timer->hours};
  FormatPuzzleTimer(time,
                    static_cast<std::uint8_t>(
                        Audio_Seq_unusedStateFlags.puzzleTimerHundredths),
                    std::span(Battle_puzzleTimerText));
  Core_Debugf("hud.timer", "text=%s seconds=%d minutes=%d hours=%d frames=%d",
              Battle_puzzleTimerText, timer->seconds, timer->minutes,
              timer->hours, timer->frames);
  DrawText(std::string_view(Battle_puzzleTimerText, kTimerCharacters),
           kTimerPosition, kTimerLayer);
}
}  // namespace VS::Battle
