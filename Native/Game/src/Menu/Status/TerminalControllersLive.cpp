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
#include "Battle/Hud.h"
#include "Core/Diagnostics.h"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLiveInternal.hpp"
#include "OverlayWorkspaceLiveState.h"
#include "Render/Camera.h"
#include "Render/CameraViewLive.hpp"
#include "Render/Display.h"
#include "Render/EncodedText.hpp"
#include "Render/Render.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"

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
  Core_Warnf("menu", "status terminal refused: %.*s",
             static_cast<int>(reason.size()), reason.data());
}
int Report(std::string_view entry, const TerminalResult& result) {
  if (result) {
    return *result;
  }
  ++refused;
  const auto reason = TerminalErrorText(result.error());
  Core_Warnf("menu", "status terminal %.*s refused: %.*s",
             static_cast<int>(entry.size()), entry.data(),
             static_cast<int>(reason.size()), reason.data());
  return 0;
}

TerminalFrame LoadFrame() {
  return {
      .selected_actor = Menu_Status_selectedActor,
      .previous_actor = Menu_Status_previousActor,
      .animation_step = Menu_Status_animationStep,
      .yaw = Menu_Status_xPos,
      .pitch = Menu_Status_yPos,
      .background_first = Menu_Status_drawBackgroundFirst,
      .fade = Menu_Status_fadeScreen,
      .vitals_phase =
          static_cast<std::uint8_t>(Menu_Status_renderBasicStatsState),
      .vitals_slide =
          static_cast<std::uint8_t>(Menu_Status_basicStatsAnimState),
      .initialized_actor =
          static_cast<std::uint8_t>(Menu_Status_initializedActor),
      .model_delay = static_cast<std::uint8_t>(Menu_Status_delayScreenUpdate),
      .selected_element = Menu_Status_selectedElement,
      .selected_stat = static_cast<std::uint8_t>(Menu_Status_selectedStatRow),
      .cursor_color = static_cast<std::uint8_t>(Menu_Framework_cursorColor),
      .freeze_tabs = static_cast<std::uint8_t>(Menu_Inventory_freezeTabArrows),
      .stat_page = static_cast<std::uint8_t>(Menu_Framework_itemStatPage),
      .enabled_pages =
          static_cast<std::uint8_t>(Menu_Framework_enabledStatPages),
      .selected_equipment = static_cast<std::uint8_t>(
          Menu_Framework_menuMemory.selectedEquipment),
      .cursor_memory = static_cast<std::uint8_t>(Input_settings.cursorMemory),
      .hud_hidden = static_cast<std::uint8_t>(Battle_hudStatusIconsHidden),
      .current_menu =
          static_cast<std::uint8_t>(Menu_Framework_menuState.currentMenu),
      .pressed = Input_buttonsPressed.allButtons,
      .held = static_cast<std::uint32_t>(Input_buttonsState),
      .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
      .stick_x = Input_stickPosBuf.rStickX,
      .stick_y = Input_stickPosBuf.rStickY};
}
void StoreFrame(const TerminalFrame& frame) {
  Menu_Status_selectedActor = frame.selected_actor;
  Menu_Status_previousActor = frame.previous_actor;
  Menu_Status_animationStep = frame.animation_step;
  Menu_Status_xPos = frame.yaw;
  Menu_Status_yPos = frame.pitch;
  Menu_Status_drawBackgroundFirst = frame.background_first;
  Menu_Status_fadeScreen = frame.fade;
  Menu_Status_renderBasicStatsState = static_cast<char>(frame.vitals_phase);
  Menu_Status_basicStatsAnimState = static_cast<char>(frame.vitals_slide);
  Menu_Status_initializedActor = static_cast<char>(frame.initialized_actor);
  Menu_Status_delayScreenUpdate = static_cast<char>(frame.model_delay);
  Menu_Status_selectedElement = frame.selected_element;
  Menu_Status_selectedStatRow = static_cast<char>(frame.selected_stat);
  Menu_Framework_cursorColor = static_cast<char>(frame.cursor_color);
  Menu_Inventory_freezeTabArrows = static_cast<char>(frame.freeze_tabs);
  Menu_Framework_itemStatPage = static_cast<char>(frame.stat_page);
  Menu_Framework_enabledStatPages = static_cast<char>(frame.enabled_pages);
  Menu_Framework_menuMemory.selectedEquipment =
      static_cast<char>(frame.selected_equipment);
  Input_settings.cursorMemory = static_cast<char>(frame.cursor_memory);
  Battle_hudStatusIconsHidden = static_cast<char>(frame.hud_hidden);
  Menu_Framework_menuState.currentMenu = static_cast<char>(frame.current_menu);
}
TerminalView LoadView() {
  const MATRIX& view = Render_frameScratch.viewMatrix;
  TerminalView result;
  static_assert(sizeof(TerminalCamera) == sizeof(Render_Camera));
  result.camera = std::bit_cast<TerminalCamera>(Render_frameScratch.camera);
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
  MATRIX& target = Render_frameScratch.viewMatrix;
  for (std::size_t row = 0; row < view.rotation.m.size(); ++row) {
    std::ranges::copy(view.rotation.m[row], target.m[row]);
  }
  constexpr auto kPaddingOffset = offsetof(MATRIX, t) - sizeof(std::int16_t);
  std::memcpy(reinterpret_cast<std::byte*>(&target) + kPaddingOffset,
              &view.rotation_padding, sizeof(view.rotation_padding));
  target.t[0] = view.translation.x;
  target.t[1] = view.translation.y;
  target.t[2] = view.translation.z;
  Render_frameScratch.camera = std::bit_cast<Render_Camera>(view.camera);
}
bool Workspace() {
  constexpr std::size_t kRowsBytes =
      Framework::kRowCount * sizeof(Menu_Framework_MenuRow);
  constexpr std::size_t kWorkspaceBytes =
      kRowsBytes + Framework::kStringBufferBytes +
      Framework::kRowTypeCount * sizeof(int);
  if (Content_overlayWorkspaceOwner != kWorkspaceOwner ||
      !InPayload(Menu_Framework_menuItems, kWorkspaceBytes)) {
    Refuse("missing or foreign MAINMENU workspace owner");
    return false;
  }
  const auto* expected =
      reinterpret_cast<const char*>(Menu_Framework_menuItems) + kRowsBytes;
  if (Menu_Framework_stringBuf != expected ||
      reinterpret_cast<const char*>(Menu_Framework_rowTypeBuf) !=
          expected + Framework::kStringBufferBytes) {
    Refuse(
        "description or row-type buffer is not the MAINMENU workspace borrow");
    return false;
  }
  return true;
}
bool ItemText() {
  if (!InPayload(Menu_Framework_itemNames, Framework::kItemTextBytes)) {
    Refuse("missing or expired MAINMENU item text allocation");
    return false;
  }
  const auto* names =
      reinterpret_cast<const std::uint8_t*>(Menu_Framework_itemNames);
  if (reinterpret_cast<const std::uint8_t*>(Menu_Framework_itemHelp) !=
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
          static_cast<std::uint8_t>(stats->weapon.grip.gemSlots),
      .shield_gem_slots =
          static_cast<std::uint8_t>(stats->shield.base.gemSlots),
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
      static_cast<std::size_t>(index) >= std::size(Menu_Status_statusStrings)) {
    return {};
  }
  const std::size_t offset = indexed ? Menu_Status_statusStrings[index]
                                     : static_cast<std::size_t>(index);
  if (offset >= std::size(Menu_Status_statusStrings)) {
    return {};
  }
  const auto words = std::span(Menu_Status_statusStrings).subspan(offset);
  return Encoded({reinterpret_cast<const std::uint8_t*>(words.data()),
                  words.size_bytes()});
}
bool DrawFadeChecked() {
  if (Menu_Status_fadeScreen == 0) {
    return true;
  }
  auto* chain =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, -1);
  Render::LivePacketQueue queue(chain);
  return chain != nullptr && queue.CanEmit(kGradientWords) &&
         DrawModelSlideFade(queue, true, Render_frameBuf != 0);
}
bool DrawBackgroundChecked() {
  auto* chain = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene,
      Menu_Status_drawBackgroundFirst != 0 ? -1 : kBackgroundEntry);
  Render::LivePacketQueue queue(chain);
  return chain != nullptr && queue.CanEmit(kBackgroundWords) &&
         DrawBackground(queue, Render_frameBuf != 0);
}
bool VitalsChecked() {
  const auto selected =
      Core::Math::WrappingSubtract(Menu_Status_selectedActor, 1);
  if (Stats(selected) == nullptr || !RenderSpace()) {
    return false;
  }
  const auto phase =
      static_cast<std::uint8_t>(Menu_Status_renderBasicStatsState);
  if ((phase == vitals_phase::kHiding || phase == vitals_phase::kSwapping) &&
      static_cast<std::uint8_t>(Menu_Status_basicStatsAnimState) > 1 &&
      Stats(Core::Math::WrappingSubtract(Menu_Status_previousActor, 1)) ==
          nullptr) {
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
    static_cast<void>(
        Framework::DrawLiveActorPreview(&Render_frameScratch.viewMatrix));
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
  Menu_Status_selectedStatRow = 0;
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
