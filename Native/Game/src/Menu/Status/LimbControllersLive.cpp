#include "Menu/Status/LimbControllersLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "Actors/SkeletonLive.hpp"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Battle/TargetingState.h"
#include "Content/StringTables.h"
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Sounds.hpp"
#include "Menu/Status/ActorStatus.h"
#include "Render/GameFontLive.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPacketsLive.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
constexpr std::size_t kSelectionDimmingIndex = 49;
constexpr int kStatusIconChain = -2;  // original byte offset -8
constexpr int kLeaderChain = -1;
constexpr int kPreviewActor = 1;
LimbPanelState panel_state;
StatusEffectNavigationState navigation_state;
std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, LimbControllerError error) {
  if (std::has_single_bit(++refused_calls)) {
    const auto message = LimbControllerErrorText(error);
    Core_Warnf("menu", "status limbs: %.*s refused: %.*s",
               static_cast<int>(call.size()), call.data(),
               static_cast<int>(message.size()), message.data());
  }
}
void Report(std::string_view call, const LimbControllerResult& result) {
  if (!result) Refuse(call, result.error());
}

[[nodiscard]] Battle_ActorStats* StatsOf(std::int32_t slot) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= std::size(Battle_actors)) {
    return nullptr;
  }
  const Battle_Actor* const actor =
      Battle_actors[static_cast<std::size_t>(slot)];
  return actor == nullptr ? nullptr : actor->stats;
}
[[nodiscard]] std::optional<std::int32_t> SlotOf(
    const Battle_ActorStats* stats) {
  if (stats == nullptr) return std::nullopt;
  for (std::size_t slot = 0; slot < std::size(Battle_actors); ++slot) {
    if (StatsOf(static_cast<std::int32_t>(slot)) == stats) {
      return static_cast<std::int32_t>(slot);
    }
  }
  return std::nullopt;
}
[[nodiscard]] std::expected<Battle_LimbStats*, LimbControllerError>
SelectedLimb() {
  const auto slot = std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(Menu_Status_selectedActor) - 1U);
  Battle_ActorStats* const stats = StatsOf(slot);
  if (stats == nullptr) {
    return std::unexpected(LimbControllerError::kMissingActor);
  }
  if (Menu_Status_selectedElement >= kStatusLimbSlots) {
    return std::unexpected(LimbControllerError::kInvalidLimb);
  }
  return &stats->limbs[Menu_Status_selectedElement];
}

[[nodiscard]] std::span<const std::uint8_t> TextAt(
    std::span<const std::uint16_t> words, std::size_t offset) {
  if (offset >= words.size()) return {};
  const auto tail = words.subspan(offset);
  const std::span bytes(reinterpret_cast<const std::uint8_t*>(tail.data()),
                        tail.size_bytes());
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (bytes[index] == Render::kTextTerminator) return bytes.first(index + 1);
    if (bytes[index] >= Render::kTextFirstParameterized) ++index;
  }
  return {};
}
[[nodiscard]] std::span<const std::uint8_t> IndexedText(
    std::span<const std::uint16_t> words, std::size_t index) {
  return index < words.size() ? TextAt(words, words[index])
                              : std::span<const std::uint8_t>{};
}

class LiveIcons final : public StatusIconHost {
 public:
  bool QueueStatusSprite(const VitalsSprite& sprite) override {
    Render::LivePacketQueue queue(Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kStatusIconChain));
    auto packet =
        Render::BuildSpritePacket({.draw_mode = Render::kDefaultSpriteDrawMode,
                                   .brightness_transparency = sprite.brightness,
                                   .packed_position = sprite.xy,
                                   .packed_size = sprite.wh},
                                  queue.ChainLink());
    packet[Render::kSpriteTextureWord] = sprite.uv_clut;
    return queue.Emit(packet);
  }
};
[[nodiscard]] LimbControllerResult DrawLiveIcons(Battle_ActorStats* actor,
                                                 int slide) {
  if (!SlotOf(actor)) {
    return std::unexpected(LimbControllerError::kMissingActor);
  }
  LiveIcons host;
  return DrawStatusEffectIcons(host, Battle::LiveStatusIconFlags(actor), slide,
                               std::span(Battle_statusIconTexOffsets));
}
[[nodiscard]] LimbControllerResult DrawLiveIndicator(
    const LimbIndicatorRequest& request) {
  Render::LivePacketQueue queue(
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0));
  return DrawLimbConditionIndicator(queue, Battle_limbStatusColors, request);
}
[[nodiscard]] LimbControllerResult DrawLiveLeader(
    const LimbLeaderRequest& request) {
  Render::LivePacketQueue queue(Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, kLeaderChain));
  return DrawLimbLeaderLine(queue, request);
}
void DrawLabel(const char* label, std::int32_t position,
               std::optional<std::uint32_t> color) {
  if (color) {
    Render::DrawLiveCompactTextColor(
        label, position, std::bit_cast<std::int32_t>(*color), nullptr);
  } else {
    Render::DrawLiveCompactText(label, position, nullptr);
  }
}

class LivePanel final : public LimbPanelHost {
 public:
  std::optional<LimbNameIndices> Names(std::uint8_t actor_slot) override {
    const Battle_ActorStats* const stats = StatsOf(actor_slot);
    if (stats == nullptr) return std::nullopt;
    LimbNameIndices names{};
    for (std::size_t limb = 0; limb < names.size(); ++limb) {
      names[limb] = stats->limbs[limb].nameIndex;
    }
    return names;
  }
  bool PopulateComparisonStats(std::uint8_t actor_slot) override {
    if (StatsOf(actor_slot) == nullptr) return false;
    Framework::SetLiveActorComparisonStats(actor_slot);
    return true;
  }
  bool DrawIcons(std::uint8_t actor_slot, std::int32_t slide) override {
    return DrawLiveIcons(StatsOf(actor_slot), slide).has_value();
  }
  bool DrawName(std::uint8_t name, std::int32_t position,
                std::optional<std::uint32_t> color) override {
    if (name >= std::size(Battle_limbNames) ||
        Battle_limbNames[name] == nullptr) {
      return false;
    }
    DrawLabel(Battle_limbNames[name], position, color);
    return true;
  }
  std::optional<std::uint8_t> Condition(std::uint8_t actor_slot,
                                        std::size_t limb) override {
    Battle_ActorStats* const stats = StatsOf(actor_slot);
    if (stats == nullptr || limb >= std::size(stats->limbs)) {
      return std::nullopt;
    }
    const int condition = Battle::LiveLimbCondition(&stats->limbs[limb]);
    if (condition < 0 ||
        static_cast<std::size_t>(condition) >= kLimbConditionCount) {
      return std::nullopt;
    }
    return static_cast<std::uint8_t>(condition);
  }
  bool DrawCondition(std::uint8_t condition, std::int32_t position,
                     std::optional<std::uint32_t> color) override {
    if (condition >= kLimbConditionCount ||
        Menu_Status_limbStatuses[condition] == nullptr) {
      return false;
    }
    DrawLabel(Menu_Status_limbStatuses[condition], position, color);
    return true;
  }
  bool DrawIndicator(const LimbIndicatorRequest& request) override {
    return DrawLiveIndicator(request).has_value();
  }
  std::optional<std::int32_t> ProjectAnchor(std::uint8_t actor_slot,
                                            std::size_t limb) override {
    const Battle_ActorStats* const stats = StatsOf(actor_slot);
    if (stats == nullptr || limb >= std::size(stats->limbs)) {
      return std::nullopt;
    }
    std::array<int, 2> screen{};
    // The original uses LBU for this char, including anchors >= 128.
    const auto anchor =
        static_cast<std::uint8_t>(stats->limbs[limb].anchorBone);
    if (Actors::ProjectLiveActorBone(kPreviewActor, anchor, screen.data(), 0) <
        0) {
      return std::nullopt;
    }
    return screen.front();
  }
  bool DrawLeader(const LimbLeaderRequest& request) override {
    return DrawLiveLeader(request).has_value();
  }
};

class LiveNavigation final : public StatusEffectNavigationHost {
 public:
  void PlayChangeSound() override { Menu::PlayChangeSound(); }
  bool ShowStatusDescription(std::uint8_t effect) override {
    const auto text = IndexedText(
        Menu_Status_statusStrings,
        static_cast<std::size_t>(Text_Menu4_status_Index_strDown) + effect);
    if (text.empty()) return false;
    Framework::SetLiveInformationMessageText(text);
    return true;
  }
  std::int32_t DrawCursor(std::uint8_t animation,
                          std::int32_t position) override {
    return Framework::DrawLiveCursor(animation, position);
  }
};

[[nodiscard]] LimbPanelFrame PanelFrame() {
  return {.selected_actor = Menu_Status_selectedActor,
          .selection_and_dimming = static_cast<std::uint8_t>(
              Menu_Framework_menuMemory.opaque51[kSelectionDimmingIndex]),
          .fade_screen = Menu_Status_fadeScreen,
          .background_first = Menu_Status_drawBackgroundFirst,
          .row_offsets = {reinterpret_cast<const std::uint8_t*>(
                              Menu_Framework_rowAnimationSteps),
                          std::size(Menu_Framework_rowAnimationSteps)}};
}
[[nodiscard]] std::expected<std::int32_t, LimbControllerError> UpdateLivePanel(
    int request) {
  LimbPanelFrame frame = PanelFrame();
  LivePanel host;
  const auto result = UpdateLimbPanel(panel_state, frame, request, host);
  Menu_Framework_menuMemory.opaque51[kSelectionDimmingIndex] =
      static_cast<char>(frame.selection_and_dimming);
  return result;
}

class LiveSelection final : public SelectedLimbHost {
 public:
  explicit LiveSelection(
      const Render::EncodedTextArguments* arguments = nullptr)
      : arguments_(arguments) {}
  bool ShowInformation(std::span<const std::uint8_t> text) override {
    if (arguments_ == nullptr) return false;
    for (std::size_t index = 0; index < 2; ++index) {
      // These views borrow immutable module tables; the legacy context does
      // not own or modify them, but its ABI still has mutable char pointers.
      Battle_stringContext.strings[index] = reinterpret_cast<char*>(
          const_cast<std::uint8_t*>(arguments_->strings[index].data()));
    }
    Framework::SetLiveInformationMessageText(text);
    return true;
  }
  bool SelectLimb(std::int32_t request) override {
    return UpdateLivePanel(request).has_value();
  }

 private:
  const Render::EncodedTextArguments* arguments_;
};

[[nodiscard]] StatusEffectNavigationFrame NavigationFrame() {
  return {.selected_element = Menu_Status_selectedElement,
          .cursor_color = static_cast<std::uint8_t>(Menu_Framework_cursorColor),
          .yaw = Menu_Status_xPos,
          .pitch = Menu_Status_yPos,
          .pressed = Input_buttonsPressed.allButtons,
          .held = static_cast<std::uint32_t>(Input_buttonsState),
          .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
          .stick_x = Input_stickPosBuf.rStickX,
          .stick_y = Input_stickPosBuf.rStickY};
}
void StoreNavigationFrame(const StatusEffectNavigationFrame& frame) {
  Menu_Status_selectedElement = frame.selected_element;
  Menu_Framework_cursorColor = static_cast<char>(frame.cursor_color);
  Menu_Status_xPos = frame.yaw;
  Menu_Status_yPos = frame.pitch;
}
}  // namespace

// decomp: _drawStatusIndicator (MENU4.PRG 120.c:847).
void LiveDrawLimbConditionIndicator(int condition, int position, int dimming) {
  Report(
      "DrawLimbConditionIndicator",
      DrawLiveIndicator(
          {.condition = condition, .position = position, .dimming = dimming}));
}
// decomp: _drawLimbLeaderLine (MENU4.PRG 120.c:870).
void LiveDrawLimbLeaderLine(int limb, int position, int unused_fade,
                            int opacity) {
  (void)unused_fade;
  Report(
      "DrawLimbLeaderLine",
      DrawLiveLeader({.limb = limb, .position = position, .opacity = opacity}));
}
// decomp: _renderStatusIcons (MENU4.PRG 120.c:913).
void LiveDrawStatusEffectIcons(Battle_ActorStats* actor, int animation_step) {
  Report("DrawStatusEffectIcons", DrawLiveIcons(actor, animation_step));
}
// decomp: _navigateStatusModifiers (MENU4.PRG 120.c:961).
int LiveNavigateStatusEffects(Battle_ActorStats* actor, int selection_active) {
  std::optional<StatusEffectTarget> target;
  if (actor != nullptr) {
    const auto slot = SlotOf(actor);
    if (!slot) {
      Refuse("NavigateStatusEffects", LimbControllerError::kMissingActor);
      return 1;
    }
    target = StatusEffectTarget{.actor_slot = *slot,
                                .flags = static_cast<std::uint32_t>(
                                    Battle::LiveStatusIconFlags(actor))};
  }
  auto frame = NavigationFrame();
  LiveNavigation host;
  const auto result = NavigateStatusEffects(navigation_state, frame, target,
                                            selection_active != 0, host);
  StoreNavigationFrame(frame);
  if (!result) {
    Refuse("NavigateStatusEffects", result.error());
    return 1;
  }
  return *result;
}
// decomp: _renderLimbUi (MENU4.PRG 120.c:1146).
int LiveUpdateLimbPanel(int request) {
  const auto result = UpdateLivePanel(request);
  if (!result) {
    Refuse("UpdateLimbPanel", result.error());
    return 0;
  }
  return *result;
}
// decomp: _renderSelectedLimbCondition (MENU4.PRG 120.c:2228).
void LiveShowSelectedLimbCondition() {
  const auto selected = SelectedLimb();
  if (!selected) {
    Refuse("ShowSelectedLimbCondition", selected.error());
    return;
  }
  if (Menu_Framework_stringBuf == nullptr) {
    Refuse("ShowSelectedLimbCondition", LimbControllerError::kInvalidText);
    return;
  }
  Battle_LimbStats* const limb = *selected;
  const int condition = Battle::LiveLimbCondition(limb);
  if (condition < 0 ||
      static_cast<std::size_t>(condition) >= kLimbConditionCount) {
    Refuse("ShowSelectedLimbCondition", LimbControllerError::kInvalidCondition);
    return;
  }
  const SelectedLimbText text{
      .name = IndexedText(Battle_statusStrings, limb->nameIndex),
      .condition = IndexedText(
          Menu_Status_statusStrings,
          static_cast<std::size_t>(Text_Menu4_status_Index_critical) +
              static_cast<std::size_t>(condition)),
      .format =
          TextAt(Menu_Status_statusStrings, Text_Menu4_status_Offset_values)};
  if (text.name.empty() || text.condition.empty() || text.format.empty()) {
    Refuse("ShowSelectedLimbCondition", LimbControllerError::kInvalidText);
    return;
  }
  Render::EncodedTextArguments arguments;
  LiveSelection host(&arguments);
  const std::span output(
      reinterpret_cast<std::uint8_t*>(Menu_Framework_stringBuf),
      Framework::kStringBufferBytes);
  Report("ShowSelectedLimbCondition",
         ShowSelectedLimbCondition(Menu_Status_selectedElement, text, arguments,
                                   output, host));
}
// decomp: _renderSelectedLimbStats (MENU4.PRG 120.c:2246).
void LivePopulateSelectedLimbDefense() {
  const auto selected = SelectedLimb();
  if (!selected) {
    Refuse("PopulateSelectedLimbDefense", selected.error());
    return;
  }
  const Battle_LimbStats* const limb = *selected;
  LiveSelection host;
  Report("PopulateSelectedLimbDefense",
         PopulateSelectedLimbDefense(Menu_Status_selectedElement,
                                     {.affinities = std::span(limb->affinities),
                                      .damage_types = std::span(limb->types)},
                                     Menu_Framework_equipmentStats, host));
}

void ResetLiveLimbControllersModuleData() {
  panel_state = {};
  navigation_state = {};
  Menu_Status_selectedElement = 0;
}
const LimbPanelState& LiveLimbPanelControllerState() { return panel_state; }
const StatusEffectNavigationState& LiveStatusEffectNavigationControllerState() {
  return navigation_state;
}
std::uint64_t RefusedLiveLimbControllerCount() { return refused_calls; }
}  // namespace VS::Menu::Status
