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
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Content/StringTables.hpp"
#include "Core/Diagnostics.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/Status/ActorStatus.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPacketsLive.hpp"

using VS::Battle::LimbStats;

namespace menu4_status = VS::Content::text::menu4_prg::status;

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
constexpr int kStatusIconChain = -2;  // original byte offset -8
constexpr int kLeaderChain = -1;
constexpr int kPreviewActor = 1;
// decomp: limbStatuses (src/MENU/MENU4.PRG/120.c:290), the condition labels
// indexed by LiveLimbCondition (120.c:1213, 1258, 1288). The original's
// trailing NULL only padded the table.
constexpr std::array<const char*, kLimbConditionCount> kLimbStatuses{
    "DYING", "BAD", "AVERAGE", "GOOD", "EXCELLENT",
};
LimbPanelState panelState;
StatusEffectNavigationState navigationState;
std::uint64_t refusedCalls = 0;

void Refuse(std::string_view call, LimbControllerError error) {
  if (std::has_single_bit(++refusedCalls)) {
    const auto message = LimbControllerErrorText(error);
    VS::Core::Warnf("menu", "status limbs: %.*s refused: %.*s",
                    static_cast<int>(call.size()), call.data(),
                    static_cast<int>(message.size()), message.data());
  }
}
void Report(std::string_view call, const LimbControllerResult& result) {
  if (!result) Refuse(call, result.error());
}

[[nodiscard]] VS::Battle::ActorStats* StatsOf(std::int32_t slot) {
  if (slot < 0 ||
      static_cast<std::size_t>(slot) >= std::size(VS::Battle::actors)) {
    return nullptr;
  }
  const VS::Battle::Actor* const actor =
      VS::Battle::actors[static_cast<std::size_t>(slot)];
  return actor == nullptr ? nullptr : actor->stats;
}
[[nodiscard]] std::optional<std::int32_t> SlotOf(
    const VS::Battle::ActorStats* stats) {
  if (stats == nullptr) return std::nullopt;
  for (std::size_t slot = 0; slot < std::size(VS::Battle::actors); ++slot) {
    if (StatsOf(static_cast<std::int32_t>(slot)) == stats) {
      return static_cast<std::int32_t>(slot);
    }
  }
  return std::nullopt;
}
[[nodiscard]] std::expected<LimbStats*, LimbControllerError> SelectedLimb() {
  const auto slot = std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(selectedActor) - 1U);
  VS::Battle::ActorStats* const stats = StatsOf(slot);
  if (stats == nullptr) {
    return std::unexpected(LimbControllerError::kMissingActor);
  }
  if (selectedElement >= kStatusLimbSlots) {
    return std::unexpected(LimbControllerError::kInvalidLimb);
  }
  return &stats->limbs[selectedElement];
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
[[nodiscard]] LimbControllerResult DrawLiveIcons(VS::Battle::ActorStats* actor,
                                                 int slide) {
  if (!SlotOf(actor)) {
    return std::unexpected(LimbControllerError::kMissingActor);
  }
  LiveIcons host;
  return DrawStatusEffectIcons(host, Battle::LiveStatusIconFlags(actor), slide,
                               std::span(VS::Battle::statusIconTexOffsets));
}
[[nodiscard]] LimbControllerResult DrawLiveIndicator(
    const LimbIndicatorRequest& request) {
  Render::LivePacketQueue queue(
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0));
  return DrawLimbConditionIndicator(queue, VS::Battle::limbStatusColors,
                                    request);
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
    const VS::Battle::ActorStats* const stats = StatsOf(actor_slot);
    if (stats == nullptr) return std::nullopt;
    LimbNameIndices names{};
    for (std::size_t limb = 0; limb < names.size(); ++limb) {
      names[limb] = stats->limbs[limb].name_index;
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
    if (name >= std::size(VS::Battle::limbNames) ||
        VS::Battle::limbNames[name] == nullptr) {
      return false;
    }
    DrawLabel(VS::Battle::limbNames[name], position, color);
    return true;
  }
  std::optional<std::uint8_t> Condition(std::uint8_t actor_slot,
                                        std::size_t limb) override {
    VS::Battle::ActorStats* const stats = StatsOf(actor_slot);
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
    if (condition >= kLimbConditionCount) return false;
    DrawLabel(kLimbStatuses[condition], position, color);
    return true;
  }
  bool DrawIndicator(const LimbIndicatorRequest& request) override {
    return DrawLiveIndicator(request).has_value();
  }
  std::optional<std::int32_t> ProjectAnchor(std::uint8_t actor_slot,
                                            std::size_t limb) override {
    const VS::Battle::ActorStats* const stats = StatsOf(actor_slot);
    if (stats == nullptr || limb >= std::size(stats->limbs)) {
      return std::nullopt;
    }
    std::array<int, 2> screen{};
    // The original uses LBU for this char, including anchors >= 128.
    const auto anchor =
        static_cast<std::uint8_t>(stats->limbs[limb].anchor_bone);
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
        statusStrings,
        static_cast<std::size_t>(menu4_status::index::kStrDown) + effect);
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
  return {.selected_actor = selectedActor,
          .selection_and_dimming =
              VS::Menu::Framework::menuMemory.limb_selection_and_dimming,
          .fade_screen = fadeScreen,
          .background_first = drawBackgroundFirst,
          .row_offsets = VS::Menu::Framework::rowAnimationSteps};
}
[[nodiscard]] std::expected<std::int32_t, LimbControllerError> UpdateLivePanel(
    int request) {
  LimbPanelFrame frame = PanelFrame();
  LivePanel host;
  const auto result = UpdateLimbPanel(panelState, frame, request, host);
  VS::Menu::Framework::menuMemory.limb_selection_and_dimming =
      frame.selection_and_dimming;
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
      VS::Battle::stringContext.strings[index] = reinterpret_cast<char*>(
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
  return {.selected_element = selectedElement,
          .cursor_color =
              static_cast<std::uint8_t>(VS::Menu::Framework::cursorColor),
          .yaw = screenXPos,
          .pitch = screenYPos,
          .pressed = VS::Input::buttonsPressed,
          .held = static_cast<std::uint32_t>(VS::Input::buttonsState),
          .repeat = static_cast<std::uint32_t>(VS::Input::buttonRepeat),
          .stick_x = VS::Input::stickPosBuf.r_stick_x,
          .stick_y = VS::Input::stickPosBuf.r_stick_y};
}
void StoreNavigationFrame(const StatusEffectNavigationFrame& frame) {
  selectedElement = frame.selected_element;
  VS::Menu::Framework::cursorColor = static_cast<char>(frame.cursor_color);
  screenXPos = frame.yaw;
  screenYPos = frame.pitch;
}
}  // namespace

// decomp: drawStatusIndicator (MENU4.PRG src/MENU/MENU4.PRG/120.c:851; was
// _drawStatusIndicator).
void LiveDrawLimbConditionIndicator(int condition, int position, int dimming) {
  Report(
      "DrawLimbConditionIndicator",
      DrawLiveIndicator(
          {.condition = condition, .position = position, .dimming = dimming}));
}
// decomp: drawLimbLeaderLine (MENU4.PRG src/MENU/MENU4.PRG/120.c:874; was
// _drawLimbLeaderLine).
void LiveDrawLimbLeaderLine(int limb, int position, int unused_fade,
                            int opacity) {
  (void)unused_fade;
  Report(
      "DrawLimbLeaderLine",
      DrawLiveLeader({.limb = limb, .position = position, .opacity = opacity}));
}
// decomp: renderStatusIcons (MENU4.PRG src/MENU/MENU4.PRG/120.c:916; was
// _renderStatusIcons).
void LiveDrawStatusEffectIcons(VS::Battle::ActorStats* actor,
                               int animation_step) {
  Report("DrawStatusEffectIcons", DrawLiveIcons(actor, animation_step));
}
// decomp: navigateStatusModifiers (MENU4.PRG src/MENU/MENU4.PRG/120.c:964; was
// _navigateStatusModifiers).
int LiveNavigateStatusEffects(VS::Battle::ActorStats* actor,
                              int selection_active) {
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
  const auto result = NavigateStatusEffects(navigationState, frame, target,
                                            selection_active != 0, host);
  StoreNavigationFrame(frame);
  if (!result) {
    Refuse("NavigateStatusEffects", result.error());
    return 1;
  }
  return *result;
}
// decomp: renderLimbUi (MENU4.PRG src/MENU/MENU4.PRG/120.c:1149; was
// _renderLimbUi).
int LiveUpdateLimbPanel(int request) {
  const auto result = UpdateLivePanel(request);
  if (!result) {
    Refuse("UpdateLimbPanel", result.error());
    return 0;
  }
  return *result;
}
// decomp: renderSelectedLimbCondition (MENU4.PRG src/MENU/MENU4.PRG/120.c:2231;
// was _renderSelectedLimbCondition).
void LiveShowSelectedLimbCondition() {
  const auto selected = SelectedLimb();
  if (!selected) {
    Refuse("ShowSelectedLimbCondition", selected.error());
    return;
  }
  if (VS::Menu::Framework::stringBuf == nullptr) {
    Refuse("ShowSelectedLimbCondition", LimbControllerError::kInvalidText);
    return;
  }
  LimbStats* const limb = *selected;
  const int condition = Battle::LiveLimbCondition(limb);
  if (condition < 0 ||
      static_cast<std::size_t>(condition) >= kLimbConditionCount) {
    Refuse("ShowSelectedLimbCondition", LimbControllerError::kInvalidCondition);
    return;
  }
  const SelectedLimbText text{
      .name = IndexedText(VS::Battle::statusStrings, limb->name_index),
      .condition =
          IndexedText(statusStrings,
                      static_cast<std::size_t>(menu4_status::index::kCritical) +
                          static_cast<std::size_t>(condition)),
      .format = TextAt(statusStrings, menu4_status::offset::kValues)};
  if (text.name.empty() || text.condition.empty() || text.format.empty()) {
    Refuse("ShowSelectedLimbCondition", LimbControllerError::kInvalidText);
    return;
  }
  Render::EncodedTextArguments arguments;
  LiveSelection host(&arguments);
  const std::span output(
      reinterpret_cast<std::uint8_t*>(VS::Menu::Framework::stringBuf),
      Framework::kStringBufferBytes);
  Report("ShowSelectedLimbCondition",
         ShowSelectedLimbCondition(selectedElement, text, arguments, output,
                                   host));
}
// decomp: renderSelectedLimbStats (MENU4.PRG src/MENU/MENU4.PRG/120.c:2248; was
// _renderSelectedLimbStats).
void LivePopulateSelectedLimbDefense() {
  const auto selected = SelectedLimb();
  if (!selected) {
    Refuse("PopulateSelectedLimbDefense", selected.error());
    return;
  }
  const LimbStats* const limb = *selected;
  LiveSelection host;
  Report(
      "PopulateSelectedLimbDefense",
      PopulateSelectedLimbDefense(selectedElement,
                                  {.affinities = std::span(limb->affinities),
                                   .damage_types = std::span(limb->types)},
                                  VS::Menu::Framework::equipmentStats, host));
}

void ResetLiveLimbControllersModuleData() {
  panelState = {};
  navigationState = {};
  selectedElement = 0;
}
const LimbPanelState& LiveLimbPanelControllerState() { return panelState; }
const StatusEffectNavigationState& LiveStatusEffectNavigationControllerState() {
  return navigationState;
}
std::uint64_t RefusedLiveLimbControllerCount() { return refusedCalls; }
}  // namespace VS::Menu::Status
