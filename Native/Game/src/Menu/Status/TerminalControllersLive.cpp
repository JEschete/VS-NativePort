#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Battle/Hud.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Settings.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/Status/ActorStatus.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLiveInternal.hpp"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/Display.hpp"
#include "Render/EncodedText.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPrimitives.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status::terminal_live {
EquipmentDetailState detail_state;
EquipmentListState list_state;
StatusTerminalState screen_state;
namespace {
std::uint64_t refused = 0;
constexpr int kWorkspaceOwner = 6;
constexpr int kPreviewSlot = 1;
constexpr int kBackgroundEntry = 2045;
constexpr std::size_t kGradientWords = 14;
constexpr std::size_t kBackgroundWords = 37;

[[nodiscard]] bool InPayload(const void* pointer, std::size_t bytes) {
  if (pointer == nullptr) {
    return false;
  }
  const auto capacity = Core::Memory::LivePayloadCapacity(pointer);
  return capacity && bytes <= *capacity;
}
}  // namespace

void Refuse(std::string_view reason) {
  ++refused;
  VS::Core::Warnf("menu", "status terminal refused: %.*s",
                  static_cast<int>(reason.size()), reason.data());
}
int Report(std::string_view entry, const TerminalResult& result) {
  if (result) {
    return *result;
  }
  ++refused;
  const auto reason = TerminalErrorText(result.error());
  VS::Core::Warnf("menu", "status terminal %.*s refused: %.*s",
                  static_cast<int>(entry.size()), entry.data(),
                  static_cast<int>(reason.size()), reason.data());
  return 0;
}

TerminalFrame LoadFrame() {
  return {
      .selected_actor = selectedActor,
      .previous_actor = previousActor,
      .animation_step = screenAnimationStep,
      .yaw = screenXPos,
      .pitch = screenYPos,
      .background_first = drawBackgroundFirst,
      .fade = fadeScreen,
      .vitals_phase = renderBasicStatsState,
      .vitals_slide = basicStatsAnimState,
      .initialized_actor = initializedActor,
      .model_delay = delayScreenUpdate,
      .selected_element = selectedElement,
      .selected_stat = selectedStatRow,
      .cursor_color =
          static_cast<std::uint8_t>(VS::Menu::Framework::cursorColor),
      .freeze_tabs = VS::Menu::Inventory::freezeTabArrows,
      .stat_page = static_cast<std::uint8_t>(VS::Menu::Framework::itemStatPage),
      .enabled_pages =
          static_cast<std::uint8_t>(VS::Menu::Framework::enabledStatPages),
      .selected_equipment = VS::Menu::Framework::menuMemory.selected_equipment,
      .cursor_memory =
          static_cast<std::uint8_t>(VS::Input::settings.cursor_memory),
      .hud_hidden = static_cast<std::uint8_t>(VS::Battle::hudStatusIconsHidden),
      .current_menu = VS::Menu::Framework::menuState.control.current_menu,
      .pressed = VS::Input::buttonsPressed,
      .held = static_cast<std::uint32_t>(VS::Input::buttonsState),
      .repeat = static_cast<std::uint32_t>(VS::Input::buttonRepeat),
      .stick_x = VS::Input::stickPosBuf.r_stick_x,
      .stick_y = VS::Input::stickPosBuf.r_stick_y};
}
void StoreFrame(const TerminalFrame& frame) {
  selectedActor = frame.selected_actor;
  previousActor = frame.previous_actor;
  screenAnimationStep = frame.animation_step;
  screenXPos = frame.yaw;
  screenYPos = frame.pitch;
  drawBackgroundFirst = frame.background_first;
  fadeScreen = frame.fade;
  renderBasicStatsState = frame.vitals_phase;
  basicStatsAnimState = frame.vitals_slide;
  initializedActor = frame.initialized_actor;
  delayScreenUpdate = frame.model_delay;
  selectedElement = frame.selected_element;
  selectedStatRow = frame.selected_stat;
  VS::Menu::Framework::cursorColor = static_cast<char>(frame.cursor_color);
  VS::Menu::Inventory::freezeTabArrows = frame.freeze_tabs;
  VS::Menu::Framework::itemStatPage = static_cast<char>(frame.stat_page);
  VS::Menu::Framework::enabledStatPages =
      static_cast<char>(frame.enabled_pages);
  VS::Menu::Framework::menuMemory.selected_equipment = frame.selected_equipment;
  VS::Input::settings.cursor_memory = static_cast<char>(frame.cursor_memory);
  VS::Battle::hudStatusIconsHidden = static_cast<char>(frame.hud_hidden);
  VS::Menu::Framework::menuState.control.current_menu = frame.current_menu;
}
TerminalView LoadView() {
  const MATRIX& view = VS::Render::FrameScratchpad().view_matrix;
  TerminalView result;
  static_assert(sizeof(TerminalCamera) == sizeof(VS::Render::Camera));
  result.camera =
      std::bit_cast<TerminalCamera>(VS::Render::FrameScratchpad().camera);
  for (std::size_t row = 0; row < result.rotation.m.size(); ++row) {
    std::ranges::copy(view.m[row], result.rotation.m[row].begin());
  }
  constexpr auto kPaddingOffset = offsetof(MATRIX, t) - sizeof(std::int16_t);
  std::memcpy(&result.rotation_padding,
              reinterpret_cast<const std::byte*>(&view) + kPaddingOffset,
              sizeof(result.rotation_padding));
  result.translation = {view.t[0], view.t[1], view.t[2]};
  return result;
}
void StoreView(const TerminalView& view) {
  MATRIX& target = VS::Render::FrameScratchpad().view_matrix;
  for (std::size_t row = 0; row < view.rotation.m.size(); ++row) {
    std::ranges::copy(view.rotation.m[row], target.m[row]);
  }
  constexpr auto kPaddingOffset = offsetof(MATRIX, t) - sizeof(std::int16_t);
  std::memcpy(reinterpret_cast<std::byte*>(&target) + kPaddingOffset,
              &view.rotation_padding, sizeof(view.rotation_padding));
  target.t[0] = view.translation.x;
  target.t[1] = view.translation.y;
  target.t[2] = view.translation.z;
  VS::Render::FrameScratchpad().camera =
      std::bit_cast<VS::Render::Camera>(view.camera);
}
bool Workspace() {
  constexpr std::size_t kRowsBytes =
      Framework::kRowCount * sizeof(Framework::MenuRow);
  constexpr std::size_t kWorkspaceBytes =
      kRowsBytes + Framework::kStringBufferBytes +
      Framework::kRowTypeCount * sizeof(int);
  if (VS::Content::overlayWorkspaceOwner != kWorkspaceOwner ||
      !InPayload(VS::Menu::Framework::menuItems, kWorkspaceBytes)) {
    Refuse("missing or foreign MAINMENU workspace owner");
    return false;
  }
  const auto* expected =
      reinterpret_cast<const char*>(VS::Menu::Framework::menuItems) +
      kRowsBytes;
  if (VS::Menu::Framework::stringBuf != expected ||
      reinterpret_cast<const char*>(VS::Menu::Framework::rowTypeBuf) !=
          expected + Framework::kStringBufferBytes) {
    Refuse(
        "description or row-type buffer is not the MAINMENU workspace borrow");
    return false;
  }
  return true;
}
bool ItemText() {
  if (!InPayload(VS::Menu::Framework::itemNames, Framework::kItemTextBytes)) {
    Refuse("missing or expired MAINMENU item text allocation");
    return false;
  }
  const auto* names =
      reinterpret_cast<const std::uint8_t*>(VS::Menu::Framework::itemNames);
  if (reinterpret_cast<const std::uint8_t*>(VS::Menu::Framework::itemHelp) !=
      names + Framework::kItemNamesBytes) {
    Refuse("ITEMHELP is not the current MAINMENU text owner's view");
    return false;
  }
  return true;
}
bool RenderSpace() {
  auto* chain =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0);
  Render::LivePacketQueue queue(chain);
  if (chain == nullptr || !queue.CanEmit(1)) {
    Refuse("missing active frame ordering table or primitive storage");
    return false;
  }
  return true;
}
Battle_ActorStats* Stats(int slot) {
  if (slot < 0 || slot >= kInspectableActorSlots) {
    return nullptr;
  }
  auto* actor = Battle_actors[slot];
  return actor == nullptr ? nullptr : actor->stats;
}
std::optional<TerminalActor> ActorView(int slot) {
  auto* stats = Stats(slot);
  if (stats == nullptr) {
    return std::nullopt;
  }
  TerminalActor result{
      .accessory_id = stats->accessory.accessory.id,
      .weapon_gem_slots =
          static_cast<std::uint8_t>(stats->weapon.grip.gem_slots),
      .shield_gem_slots =
          static_cast<std::uint8_t>(stats->shield.base.gem_slots),
      .analyzed = stats->isAnalyzed != 0,
      .statuses =
          static_cast<std::uint32_t>(Battle::LiveStatusIconFlags(stats))};
  result.equipment_ids[0] = stats->weapon.blade.id;
  result.equipment_ids[1] = stats->shield.base.id;
  for (std::size_t limb = 0; limb < kStatusLimbSlots; ++limb) {
    result.limb_names[limb] = stats->limbs[limb].nameIndex;
    result.equipment_ids[limb + 2] = stats->limbs[limb].armor.armor.id;
  }
  return result;
}
std::span<const std::uint8_t> Encoded(std::span<const std::uint8_t> bytes) {
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (bytes[index] == Render::kTextTerminator) {
      return bytes.first(index + 1);
    }
    if (bytes[index] >= Render::kTextFirstParameterized) {
      ++index;
    }
  }
  return {};
}
std::span<const std::uint8_t> StatusText(int index, bool indexed) {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= std::size(statusStrings)) {
    return {};
  }
  const std::size_t offset =
      indexed ? statusStrings[index] : static_cast<std::size_t>(index);
  if (offset >= std::size(statusStrings)) {
    return {};
  }
  const auto words = std::span(statusStrings).subspan(offset);
  return Encoded({reinterpret_cast<const std::uint8_t*>(words.data()),
                  words.size_bytes()});
}
bool DrawFadeChecked() {
  if (fadeScreen == 0) {
    return true;
  }
  auto* chain =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, -1);
  Render::LivePacketQueue queue(chain);
  return chain != nullptr && queue.CanEmit(kGradientWords) &&
         DrawModelSlideFade(queue, true, VS::Render::frameBuf != 0);
}
bool DrawBackgroundChecked() {
  auto* chain = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene,
      drawBackgroundFirst != 0 ? -1 : kBackgroundEntry);
  Render::LivePacketQueue queue(chain);
  return chain != nullptr && queue.CanEmit(kBackgroundWords) &&
         DrawBackground(queue, VS::Render::frameBuf != 0);
}
bool VitalsChecked() {
  const auto selected = Core::Math::WrappingSubtract(selectedActor, 1);
  if (Stats(selected) == nullptr || !RenderSpace()) {
    return false;
  }
  const auto phase = renderBasicStatsState;
  if ((phase == vitals_phase::kHiding || phase == vitals_phase::kSwapping) &&
      basicStatsAnimState > 1 &&
      Stats(Core::Math::WrappingSubtract(previousActor, 1)) == nullptr) {
    return false;
  }
  const auto dropped = Render::DroppedLiveUiPacketCount();
  const auto sprites = Render::DroppedLiveSpriteCount();
  LiveDrawVitals();
  return Render::DroppedLiveUiPacketCount() == dropped &&
         Render::DroppedLiveSpriteCount() == sprites;
}
namespace {
class FrameHost final : public TerminalFrameHost {
 public:
  FrameHost() : frame_(LoadFrame()), view_(LoadView()) {}
  ~FrameHost() override { Publish(); }
  TerminalFrame& Frame() override { return frame_; }
  TerminalCamera& Camera() override { return view_.camera; }
  std::optional<TerminalModelCamera> ModelCamera() override {
    const auto* record = Actors_actorRecords[kPreviewSlot];
    if (!InPayload(record, sizeof(Actors_ActorRecord))) {
      return std::nullopt;
    }
    return TerminalModelCamera{record->menuCameraDistance,
                               record->collisionHeight};
  }
  bool BuildCamera() override {
    Publish();
    Render::BuildLiveCameraViewMatrix();
    Refresh();
    return true;
  }
  bool DrawPreview() override {
    if (!InPayload(Actors_actorRecords[kPreviewSlot],
                   sizeof(Actors_ActorRecord)) ||
        !RenderSpace()) {
      return false;
    }
    Publish();
    static_cast<void>(Framework::DrawLiveActorPreview(
        &VS::Render::FrameScratchpad().view_matrix));
    Refresh();
    return true;
  }
  bool Background() override { return DrawBackgroundChecked(); }
  bool Fade() override { return DrawFadeChecked(); }
  bool LimbPanel() override {
    Publish();
    const auto refused_before = RefusedLiveLimbControllerCount();
    static_cast<void>(LiveUpdateLimbPanel(0));
    Refresh();
    return RefusedLiveLimbControllerCount() == refused_before;
  }
  bool Vitals() override {
    Publish();
    const bool result = VitalsChecked();
    Refresh();
    return result;
  }

 private:
  TerminalFrame frame_;
  TerminalView view_;
  void Publish() {
    StoreFrame(frame_);
    StoreView(view_);
  }
  void Refresh() {
    frame_ = LoadFrame();
    view_ = LoadView();
  }
};
}  // namespace
bool DrawFrameChecked() {
  FrameHost host;
  const auto result = DrawFrame(host);
  if (!result) {
    static_cast<void>(Report("frame", result));
    return false;
  }
  return true;
}
}  // namespace VS::Menu::Status::terminal_live

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
// decomp: _fadeScreenLeft (src/MENU/MENU4.PRG/120.c:2081)
void LiveDrawModelSlideFade() {
  if (!terminal_live::DrawFadeChecked()) {
    terminal_live::Refuse("model fade frame storage");
  }
}
// decomp: _drawMenuBackground (src/MENU/MENU4.PRG/120.c:2121)
void LiveDrawBackground() {
  if (!terminal_live::DrawBackgroundChecked()) {
    terminal_live::Refuse("background frame storage");
  }
}
// decomp: _drawScreen (src/MENU/MENU4.PRG/120.c:2190)
void LiveDrawFrame() { static_cast<void>(terminal_live::DrawFrameChecked()); }

void ResetLiveTerminalControllersModuleData() {
  terminal_live::detail_state = {};
  terminal_live::list_state = {};
  terminal_live::screen_state = {};
  selectedStatRow = 0;
}
const EquipmentDetailState& LiveEquipmentDetailControllerState() {
  return terminal_live::detail_state;
}
const EquipmentListState& LiveEquipmentListControllerState() {
  return terminal_live::list_state;
}
const StatusTerminalState& LiveStatusTerminalControllerState() {
  return terminal_live::screen_state;
}
std::uint64_t RefusedLiveTerminalControllerCount() {
  return terminal_live::refused;
}
}  // namespace VS::Menu::Status
