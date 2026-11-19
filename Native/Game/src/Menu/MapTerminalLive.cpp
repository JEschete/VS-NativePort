#include "Menu/MapTerminalLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "Audio/Seq/Channel.h"
#include "Audio/SfxLive.hpp"
#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Battle/StatusEffects.h"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "DiscQueue.h"
#include "Field/Map.h"
#include "Field/MapLive.hpp"
#include "Field/MinimapScene.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Map.h"
#include "Menu/MapDrawingLive.hpp"
#include "Menu/MapLoadingLive.hpp"
#include "Menu/MapNavigationLive.hpp"
#include "Menu/MapSceneStorageLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/PageSpace.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

namespace VS::Menu::map {
namespace {
namespace memory = Core::Memory;
constexpr std::size_t kBackgroundBytes = 0xB600;
constexpr std::size_t kClutBytes = 0x200;
constexpr std::uint32_t kCaptionOffset = 0x64C4;
constexpr std::size_t kCaptionBytes = 0x20;
constexpr std::uint8_t kTextEnd = 0xE7;
constexpr std::size_t kStatePacketWords = 3;
constexpr std::size_t kStripPacketWords = 5;
constexpr std::size_t kStripUvWord = 3;
constexpr std::size_t kStripSizeWord = 4;
constexpr int kMaskSlot = 2047;
constexpr int kSceneCountLive = kSceneCount;
constexpr std::uint32_t kMaskCommand = 0xE6000001;
constexpr std::uint32_t kRawSpriteCommand = 0x67000000;
constexpr std::uint32_t kShadedSpriteCommand = 0x66808080;
constexpr std::uint32_t kColorMask = 0xFFFFFF;
constexpr std::uint32_t kClutMask = 0xFFFF0000;
constexpr std::uint32_t kTagLengthMask = 0xFF000000;
constexpr std::uint32_t kTexturePageCommand = 0xE1000200;
constexpr unsigned kTextureRowShift = 8;
constexpr int kMenuSound = 0x7E;
constexpr int kRoomTextY = 224;
constexpr int kRoomTextX = 60;
constexpr int kRoomTextColumns = 16;
constexpr std::size_t kRoomNameBytes = 32;
constexpr int kSceneReadPhase = 4;
constexpr Render::PageSpace::Rect kMapClut{768, 227, 256, 1};
constexpr Render::PageSpace::Rect kMapBackground{672, 256, 96, 240};
constexpr Render::PageSpace::Rect kLeftMargin{640, 256, 32, 240};
constexpr Render::PageSpace::Rect kRightMargin{768, 256, 32, 240};

struct PendingScene {
  Content_DiscQueueSlot* request = nullptr;
  std::uint64_t generation = 0;
  void* destination = nullptr;
  DiscFileRange file{};
  bool read_completed = false;
};

struct ScreenResources {
  void* background = nullptr;
  bool field_hidden = false;
  VS::Render::ScreenOffset projection_offset{};
};

MapScreenState screen;
ScreenResources resources;
PendingScene pending_scene;
std::array<std::uint8_t, kCaptionBytes> caption{};
std::size_t caption_start = 0;
bool caption_loaded = false;
std::uint64_t refused = 0;

void Refuse(std::string_view operation, std::string_view reason) {
  if (std::has_single_bit(++refused)) {
    VS::Core::LogWarn("menu.map", "{} refused: {} ({} refusals)", operation,
                      reason, refused);
  }
}

bool ValidScene(int scene) { return scene >= 0 && scene < kSceneCountLive; }

MapAngles AnglesOf(const SVECTOR& value) {
  return {value.vx, value.vy, value.vz, value.pad};
}

void StoreAngles(SVECTOR& target, MapAngles value) {
  target.vx = value.x;
  target.vy = value.y;
  target.vz = value.z;
  target.pad = value.pad;
}

MapMenuState ReadState() {
  return {.view = ReadLiveMapView(),
          .selected_map = Menu_Map_selectedMap,
          .label_y = Menu_Map_mapLabelY};
}

void StoreState(const MapMenuState& state) {
  WriteLiveMapView(state.view);
  Menu_Map_selectedMap = state.selected_map;
  Menu_Map_mapLabelY = state.label_y;
}

std::span<std::byte> Payload(void* pointer) {
  const auto capacity = memory::LivePayloadCapacity(pointer);
  return capacity ? std::span{static_cast<std::byte*>(pointer), *capacity}
                  : std::span<std::byte>{};
}

std::optional<std::span<std::byte>> LoadedScene() {
  const auto bound = BoundLiveMapScene(Battle_unusedSceneBuffer);
  const auto capacity = Payload(Battle_unusedSceneBuffer);
  if (!bound || bound->empty() || bound->size() > capacity.size()) {
    return std::nullopt;
  }
  return bound;
}

bool ReleaseScene() {
  if (Battle_unusedSceneBuffer == nullptr) return true;
  if (!LoadedScene()) return false;
  const auto bytes = Payload(Battle_unusedSceneBuffer);
  ClearLiveMapTextBorrow(bytes);
  if (!Content::CancelLiveDiscWrites(bytes)) return false;
  Menu_Map_roomNamesTable = nullptr;
  ForgetLiveMapScene(Battle_unusedSceneBuffer);
  memory::FreeLiveFromHighEnd(Battle_unusedSceneBuffer);
  Battle_unusedSceneBuffer = nullptr;
  return true;
}

bool ShowRoomText(std::int32_t room) {
  const auto bytes = LoadedScene();
  if (!bytes || bytes->size() < sizeof(std::int32_t) || room < 0 ||
      Menu_Map_roomNamesTable == nullptr) {
    return false;
  }
  std::int32_t count = 0;
  std::memcpy(&count, bytes->data(), sizeof(count));
  if (room >= count) return false;
  const auto first = reinterpret_cast<std::uintptr_t>(bytes->data());
  const auto names = reinterpret_cast<std::uintptr_t>(Menu_Map_roomNamesTable);
  if (names < first || names - first > bytes->size()) return false;
  const auto start = static_cast<std::size_t>(names - first);
  if (static_cast<std::size_t>(room) >=
      (bytes->size() - start) / Field::kMapSceneRoomNameBytes) {
    return false;
  }
  const auto name = bytes->subspan(
      start + static_cast<std::size_t>(room) * Field::kMapSceneRoomNameBytes,
      kRoomNameBytes);
  if (std::ranges::find(name, static_cast<std::byte>(kTextEnd)) == name.end()) {
    return false;
  }
  SetLiveTextBoxText(0, reinterpret_cast<char*>(name.data()));
  return true;
}

// AddPrim and the shared insertTPage macro preserve the chain head's upper
// byte. LivePacketQueue otherwise replaces the entire head with its handle.
class MapPacketQueue final : public Render::PacketQueue {
 public:
  explicit MapPacketQueue(u_long* entry) : entry_(entry), queue_(entry) {}
  std::uint32_t ChainLink() const override { return queue_.ChainLink(); }
  bool CanEmit(std::size_t words) const {
    return entry_ != nullptr && queue_.CanEmit(words);
  }
  bool Emit(std::span<const std::uint32_t> words) override {
    if (entry_ == nullptr) return false;
    const auto length = *entry_ & kTagLengthMask;
    if (!queue_.Emit(words)) return false;
    *entry_ = length | (*entry_ & Render::kPacketLinkMask);
    return true;
  }

 private:
  u_long* entry_;
  Render::LivePacketQueue queue_;
};

class LiveFramebuffer final : public FramebufferHost {
 public:
  bool TexturePage(std::int32_t slot, std::uint16_t page) override {
    MapPacketQueue queue(
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, slot));
    // insertTPage.h emits a trailing zero; the generic draw-mode helper's
    // two-word packet has a different cursor and tag length.
    return queue.Emit(std::array<std::uint32_t, kStatePacketWords>{
        Render::PackTagWord(kStatePacketWords - 1, queue.ChainLink()),
        kTexturePageCommand | page, 0});
  }
  bool Strip(std::int32_t slot, const FramebufferStrip& strip) override {
    auto* entry =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, slot);
    MapPacketQueue queue(entry);
    if (entry == nullptr || !queue.CanEmit(kStripPacketWords)) return false;
    auto* destination = Render::ReserveLivePrimitiveWords(kStripPacketWords);
    if (destination == nullptr) return false;
    std::array<std::uint32_t, kStripPacketWords> words{};
    std::memcpy(words.data(), destination, sizeof(words));
    words[0] = Render::PackTagWord(kStripPacketWords - 1, queue.ChainLink());
    words[1] = strip.raw_texture ? (words[1] & kColorMask) | kRawSpriteCommand
                                 : kShadedSpriteCommand;
    words[2] =
        static_cast<std::uint32_t>(Render::PackScreenWord(strip.x, strip.y));
    words[kStripUvWord] =
        (strip.clear_clut ? 0U : words[kStripUvWord] & kClutMask) |
        (static_cast<std::uint32_t>(strip.texture_y) << kTextureRowShift);
    words[kStripSizeWord] = static_cast<std::uint32_t>(
        Render::PackScreenWord(strip.width, strip.height));
    return queue.Emit(words);
  }
};

class LiveFrame final : public MapFrameHost {
 public:
  void DrawControls() override { LiveDrawControls(); }
  bool EnableMaskWrite() override {
    auto* entry = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kMaskSlot);
    MapPacketQueue queue(entry);
    if (entry == nullptr) return false;
    return queue.Emit(std::array<std::uint32_t, kStatePacketWords>{
        Render::PackTagWord(kStatePacketWords - 1, queue.ChainLink()),
        kMaskCommand, 0});
  }
  void DrawDistortion() override { LiveDrawPalingDistortion(); }
  void SetPalingCaption() override {
    const auto text = LiveMapPalingText();
    if (!text.empty()) {
      SetLiveTextBoxText(
          0, reinterpret_cast<char*>(caption.data() + caption_start));
    }
  }
  void ApplyView() override { LiveApplyCamera(); }
  void Darken() override { LiveDarkenBackground(); }
  void DrawRooms() override {
    if (LoadedScene()) {
      LiveDrawVisitedRooms(
          static_cast<Field_MapScene*>(Battle_unusedSceneBuffer));
    } else {
      Refuse("DrawRooms", "no owned loaded scene");
    }
  }
};

class LiveMapMenu final : public MapMenuHost {
 public:
  explicit LiveMapMenu(MapMenuState& state) : state_(state) {}

  void SetFarColorBlack() override { Core_Gte_SetFarColor(0, 0, 0); }
  void SetBackgroundPriority(std::int32_t slot,
                             std::int32_t brightness) override {
    Call([&] { Framework::SetLiveBackgroundPriority(slot, brightness); });
  }
  void SetVisible(bool visible) override {
    Call([&] { Framework::SetLiveMenuVisible(visible ? 1 : 0); });
  }
  void PlaySound(std::int32_t volume) override {
    Audio::LivePlaySfxCentered(kMenuSound, volume);
  }
  bool InitializeRoomText() override {
    InitLiveTextBox(0, 0, kRoomTextX, kRoomTextY, kRoomTextColumns, 1,
                    kRoomTextX, kRoomTextY);
    return true;
  }
  bool ShowRoomName(std::int32_t room) override {
    bool shown = false;
    Call([&] { shown = ShowRoomText(room); });
    return shown;
  }
  void DismissRoomText() override { DismissLiveTextBox(0); }
  std::int32_t PollAreas(bool initialize) override {
    int result = kAreaSelectionPending;
    Call([&] { result = LiveUpdateAreaSelection(initialize ? 1 : 0); });
    return result;
  }
  void ShowAreaName(std::int32_t scene, bool current) override {
    if (!ValidScene(scene)) return;
    const std::size_t offset = Menu_Map_mapNames[scene];
    if (offset >= std::size(Menu_Map_mapNames)) {
      Refuse("ShowAreaName", "offset outside names");
      return;
    }
    Call([&] {
      LiveShowAreaName(reinterpret_cast<char*>(&Menu_Map_mapNames[offset]),
                       static_cast<char>(current));
    });
  }
  bool BeginSceneRead(std::int32_t scene, DiscFileRange file) override {
    if (pending_scene.destination != nullptr ||
        pending_scene.request != nullptr || !ValidScene(scene)) {
      return false;
    }
    auto* destination =
        memory::AllocateLiveFromHighEnd(static_cast<std::size_t>(file.bytes));
    if (destination == nullptr) return false;
    const auto* original_file = &Menu_Map_kSceneArmFiles[scene];
    auto* request = Content::AllocateLiveDiscRequest(original_file);
    if (request != nullptr) {
      Content::EnqueueLiveDiscRequest(request, destination);
    } else {
      request =
          Content::CompleteLiveRefusedDiscRequest(original_file, destination);
    }
    pending_scene = {request, Content::LiveDiscRequestGeneration(request),
                     destination, file};
    Menu_Map_armFileBuff = destination;
    Menu_Map_sceneCdQueueSlot = request;
    return request != nullptr;
  }
  std::int32_t SceneReadState() override {
    if (pending_scene.read_completed && pending_scene.destination != nullptr) {
      return kDiscReadLoaded;
    }
    if (pending_scene.request == nullptr || pending_scene.generation == 0 ||
        Content::LiveDiscRequestGeneration(pending_scene.request) !=
            pending_scene.generation) {
      Refuse("SceneReadState", "missing or reused request");
      return Content_DiscRequestState_Error;
    }
    return pending_scene.request->state;
  }
  bool InstallSceneRead(std::int32_t scene, DiscFileRange file) override {
    if (!ValidScene(scene) || pending_scene.file != file ||
        SceneReadState() != kDiscReadLoaded) {
      return false;
    }
    const auto bytes = Payload(pending_scene.destination);
    if (file.bytes <= 0 ||
        static_cast<std::size_t>(file.bytes) > bytes.size() ||
        !Field::IsValidMapScene(
            bytes.first(static_cast<std::size_t>(file.bytes)))) {
      return false;
    }
    if (!pending_scene.read_completed) {
      Content::ReleaseLiveDiscRequest(pending_scene.request);
      pending_scene.request = nullptr;
      pending_scene.generation = 0;
      pending_scene.read_completed = true;
      Menu_Map_sceneCdQueueSlot = nullptr;
    }
    if (!ReleaseScene()) return false;
    auto* destination =
        memory::AllocateLiveFromHighEnd(static_cast<std::size_t>(file.bytes));
    if (destination == nullptr) return false;
    memory::CopyLiveAlignedWords(destination, pending_scene.destination,
                                 file.bytes);
    memory::FreeLiveFromHighEnd(pending_scene.destination);
    pending_scene = {};
    Menu_Map_armFileBuff = nullptr;
    Battle_unusedSceneBuffer = destination;
    BindLiveMapScene(destination, static_cast<std::size_t>(file.bytes));
    Menu_Map_roomNamesTable = Field::ResolveLiveSceneRoomData(
        destination, static_cast<std::size_t>(file.bytes));
    Field::RestoreLiveSceneRoomVisits(destination,
                                      static_cast<std::size_t>(file.bytes));
    Call([&] {
      LiveScaleRoomVertices(static_cast<Field_MapScene*>(destination),
                            kMapRoomScale);
    });
    return true;
  }
  bool FreeScene() override { return ReleaseScene(); }
  std::int32_t InitialRoom() override {
    int room = 0;
    Call([&] {
      room = LiveInitialRoomIndex(
          static_cast<Field_MapScene*>(Battle_unusedSceneBuffer));
    });
    return room;
  }
  void CenterOnRoom(std::int32_t room) override {
    Call([&] {
      LiveCenterOnRoom(static_cast<Field_MapScene*>(Battle_unusedSceneBuffer),
                       room);
    });
  }
  std::int32_t SelectRoom(std::int32_t direction,
                          std::int32_t current) override {
    state_.view.current_room = current;
    Call([&] { LiveSelectAdjacentVisitedRoom(direction); });
    return state_.view.current_room;
  }
  void SetCenterTarget(std::int32_t room) override {
    Call([&] {
      LiveSetRoomCenterTarget(
          static_cast<Field_MapScene*>(Battle_unusedSceneBuffer), room);
    });
  }
  void DrawFrame() override {
    Call([] { LiveDrawFrame(); });
  }
  void AnimateCentering() override {
    Call([] {
      LiveAnimateCentering(
          static_cast<Field_MapScene*>(Battle_unusedSceneBuffer));
    });
  }

 private:
  // Drawing, selection and navigation own parts of the same view. Publish
  // before calling them and read back their changes before continuing.
  template <typename Function>
  void Call(Function&& function) {
    StoreState(state_);
    std::forward<Function>(function)();
    state_ = ReadState();
  }
  MapMenuState& state_;
};

class LiveScreen final : public MapScreenHost {
 public:
  bool CaptureBackground() override {
    if (resources.background != nullptr) return false;
    Menu_Map_currentMapRow = -1;
    resources.background = memory::AllocateLiveFromHighEnd(kBackgroundBytes);
    if (resources.background == nullptr) return false;
    resources.projection_offset = Render::GetLiveProjectionScreenOffset();
    Render::GamePages().Download(
        kMapClut, static_cast<std::uint16_t*>(resources.background));
    Render::GamePages().Download(
        kMapBackground,
        reinterpret_cast<std::uint16_t*>(
            static_cast<std::byte*>(resources.background) + kClutBytes));
    return true;
  }
  std::int32_t LoadBackground(bool begin) override {
    return LiveLoadBackground(begin ? 1 : 0);
  }
  std::int32_t LoadScene() override { return LiveLoadSceneMap(); }
  void FadeBackground(bool fade) override {
    Framework::SetLiveBackgroundFade(fade ? 1 : 0);
  }
  void SetFieldRendering(bool enabled) override {
    Field::LiveSetFieldRenderingEnabled(enabled ? 1 : 0);
    resources.field_hidden = !enabled;
  }
  void SetHudHidden(bool hidden) override {
    Battle::SetLiveHudHidden(hidden ? 1 : 0);
  }
  void SetHudSlide(std::int32_t offset) override {
    Battle_hudSlideOffset = offset;
  }
  std::int32_t MenuRowsIdle() override {
    return Framework::AreLiveMenuRowsIdle();
  }
  std::int32_t UpdateMenu() override { return LiveUpdateMapMenu(); }
  void RestoreMenuPriority() override {
    Framework::SetLiveBackgroundPriority(kBackgroundOrderOffset,
                                         kBackgroundBrightness);
  }
  void ClearCommand() override {
    Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
  }
  void ClearRows() override {
    Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
  }
  bool RestoreBackground() override {
    const auto bytes = Payload(resources.background);
    if (bytes.size() < kBackgroundBytes) return false;
    Render::GamePages().Fill(kLeftMargin, 0, 0, 0);
    Render::GamePages().Fill(kRightMargin, 0, 0, 0);
    Render::LoadLiveClutColors(
        static_cast<const std::uint16_t*>(resources.background),
        kBackgroundClutRow, 0, kMapClut.w);
    Render::GamePages().Upload(
        kMapBackground,
        reinterpret_cast<const std::uint16_t*>(bytes.data() + kClutBytes));
    return true;
  }
  bool ReleaseBackground() override {
    if (Payload(resources.background).size() < kBackgroundBytes) {
      return false;
    }
    memory::FreeLiveFromHighEnd(resources.background);
    resources.background = nullptr;
    return true;
  }
  void SetDestination(std::uint8_t menu) override {
    VS::Menu::Framework::menuState.currentMenu = static_cast<char>(menu);
  }
  void ShowMenu() override { VS::Menu::Framework::hideMenu = 0; }
};

bool DrainPendingScene() {
  if (pending_scene.destination == nullptr) return true;
  const auto bytes = Payload(pending_scene.destination);
  if (bytes.empty() || !Content::CancelLiveDiscWrites(bytes)) return false;
  if (pending_scene.generation != 0 &&
      Content::LiveDiscRequestGeneration(pending_scene.request) ==
          pending_scene.generation) {
    Content::ReleaseLiveDiscRequest(pending_scene.request);
  }
  if (Menu_Map_sceneCdQueueSlot == pending_scene.request) {
    Menu_Map_sceneCdQueueSlot = nullptr;
  }
  if (Menu_Map_armFileBuff == pending_scene.destination) {
    Menu_Map_armFileBuff = nullptr;
  }
  memory::FreeLiveFromHighEnd(pending_scene.destination);
  pending_scene = {};
  return true;
}
}  // namespace

// F02689: _renderScreen, src/MENU/MENU5.PRG/4D8.c:905.
void LiveDrawFrame() {
  if (!ValidScene(Menu_Map_currentScene)) {
    Refuse("DrawFrame", "scene outside Paling flags");
    return;
  }
  LiveFrame host;
  if (!DrawFrame(
          host,
          Audio_Seq_unusedStateFlags.mapPaling[Menu_Map_currentScene] != 0,
          Menu_Map_isPaling != 0)) {
    Refuse("DrawFrame", "mask command has no frame capacity");
  }
}

// F02690: _setCamera, src/MENU/MENU5.PRG/4D8.c:948.
void LiveApplyCamera() {
  const auto camera =
      ApplyCamera(AnglesOf(Menu_Map_cameraAngles), Menu_Map_cameraDistance,
                  AnglesOf(Menu_Map_cameraPos));
  StoreAngles(Menu_Map_cameraPos, camera.position);
  MATRIX matrix{};
  Core::Math::StoreTransform(matrix, camera.transform);
  Core_Gte_SetRotMatrix(&matrix);
  Core_Gte_SetTransMatrix(&matrix);
}

// F02699: _darkenBackground, src/MENU/MENU5.PRG/4D8.c:1788.
void LiveDarkenBackground() {
  LiveFramebuffer host;
  if (!DarkenBackground(host, VS::Render::frameBuf != 0)) {
    Refuse("DarkenBackground", "packet capacity or ordering table");
  }
}

// F02702: _applyPalingScreenEffect, src/MENU/MENU5.PRG/4D8.c:1880.
void LiveDrawPalingDistortion() {
  LiveFramebuffer host;
  if (!DrawPalingDistortion(host, VS::Render::frameBuf != 0,
                            Menu_Map_menuFramesCounter)) {
    Refuse("DrawPalingDistortion", "packet capacity or ordering table");
  }
}

// F04151: _menuManager, src/MENU/MENU5.PRG/4D8.c:523.
int LiveUpdateMapMenu() {
  try {
    if (!LoadedScene() && !(Menu_Map_menuState == kSceneReadPhase &&
                            pending_scene.read_completed &&
                            !Payload(pending_scene.destination).empty())) {
      Refuse("UpdateMapMenu",
             "no owned loaded scene or completed staging read");
      return 0;
    }
    MapMenuState state = ReadState();
    LiveMapMenu host(state);
    const MapInput input{
        .pressed = VS::Input::buttonsPressed,
        .held = static_cast<std::uint32_t>(VS::Input::previousButtons),
        .repeated = static_cast<std::uint32_t>(VS::Input::buttonRepeat),
        .left_x = static_cast<std::uint8_t>(VS::Input::stickPosBuf.l_stick_x),
        .left_y = static_cast<std::uint8_t>(VS::Input::stickPosBuf.l_stick_y),
        .right_x = static_cast<std::uint8_t>(VS::Input::stickPosBuf.r_stick_x),
        .right_y = static_cast<std::uint8_t>(VS::Input::stickPosBuf.r_stick_y),
        .field_yaw = static_cast<std::uint16_t>(
            VS::Render::FrameScratchpad().camera.angles.vy)};
    std::array<DiscFileRange, kSceneCount> files{};
    for (std::size_t index = 0; index < files.size(); ++index) {
      files[index] = {Menu_Map_kSceneArmFiles[index].lba,
                      Menu_Map_kSceneArmFiles[index].size};
    }
    const auto result =
        UpdateMapMenu(host, state, input,
                      {reinterpret_cast<const std::uint8_t*>(
                           Audio_Seq_unusedStateFlags.mapPaling),
                       kSceneCount},
                      Menu_Map_sceneRowIdMap, files);
    StoreState(state);
    if (!result) Refuse("UpdateMapMenu", "scene, selection or owned resource");
    return result.value_or(0);
  } catch (const std::exception& error) {
    Refuse("UpdateMapMenu", error.what());
    return 0;
  }
}

// F04149: vs_menu5_exec, src/MENU/MENU5.PRG/4D8.c:240.
int LiveUpdateScreen(unsigned char* step) {
  if (step == nullptr) {
    Refuse("UpdateScreen", "missing step");
    return 0;
  }
  try {
    LiveScreen host;
    const auto result = UpdateScreen(host, screen, *step,
                                     VS::Menu::Framework::backgroundFadeStep);
    if (!result) Refuse("UpdateScreen", "background is not owned");
    return result.value_or(0);
  } catch (const std::exception& error) {
    Refuse("UpdateScreen", error.what());
    return 0;
  }
}

std::span<const std::uint8_t> LiveMapPalingText() {
  if (!caption_loaded) {
    const auto* store = Content::DataStore::Get();
    const auto* entry = store ? store->Find("MENU/MENU5.PRG") : nullptr;
    std::array<std::uint8_t, kCaptionBytes> bytes{};
    if (entry == nullptr || !store->Read(*entry, kCaptionOffset, bytes)) {
      Refuse("PalingText", "original MENU5 caption is unavailable");
      return {};
    }
    const std::size_t start =
        (bytes[0] | (static_cast<std::size_t>(bytes[1]) << 8)) * 2;
    if (start < sizeof(std::uint16_t) || start >= bytes.size() ||
        std::find(bytes.begin() + static_cast<std::ptrdiff_t>(start),
                  bytes.end(), kTextEnd) == bytes.end()) {
      Refuse("PalingText", "caption offset or terminator");
      return {};
    }
    caption = bytes;
    caption_start = start;
    caption_loaded = true;
  }
  return std::span(caption).subspan(caption_start);
}

bool DrainMapTerminalLive() {
  if (Battle_unusedSceneBuffer != nullptr && !LoadedScene()) {
    Refuse("Drain", "scene alias does not identify the bound allocation");
    return false;
  }
  ClearLiveMapTextBorrow(Payload(Battle_unusedSceneBuffer));
  if (!DrainMapLoadingLive() || !DrainPendingScene() || !ReleaseScene()) {
    Refuse("Drain", "cancel writes before releasing their destination");
    return false;
  }
  LiveScreen host;
  if (resources.background != nullptr) {
    if (!host.RestoreBackground() || !host.ReleaseBackground()) {
      Refuse("Drain", "background is not owned");
      return false;
    }
    host.RestoreMenuPriority();
    host.FadeBackground(false);
    host.ShowMenu();
    Render::SetLiveProjectionScreenOffset(resources.projection_offset);
    Render::LoadLiveFrameViewMatrix();
  }
  if (resources.field_hidden) {
    host.SetHudHidden(false);
    host.SetHudSlide(0);
    host.SetFieldRendering(true);
    host.ClearCommand();
  }
  Menu_Map_roomNamesTable = nullptr;
  return true;
}

bool ResetMapTerminalModuleData() {
  if (!DrainMapTerminalLive() || !ResetMapLoadingModuleData()) return false;
  screen = {};
  resources = {};
  StoreState({});
  Menu_Map_currentMapRow = -1;
  Menu_Map_isCurrentScene = 0;
  Menu_Map_centerPoint = {};
  Menu_Map_sceneCdQueueSlot = nullptr;
  Menu_Map_armFileBuff = nullptr;
  std::ranges::fill(Menu_Map_sceneRowIdMap, 0);
  std::ranges::fill(Menu_Map_layoutPadding1, 0);
  std::ranges::fill(Menu_Map_layoutPadding2, 0);
  std::ranges::fill(Menu_Map_compassBounds, Menu_Map_CompassBound{});
  std::ranges::fill(Menu_Map_mapLabels, Menu_Map_MapLabel{});
  Menu_Map_mapLabelCount = 0;
  ResetMapDrawingModuleData();
  return true;
}

bool ResetMapTerminalColdState() {
  if (!ResetMapTerminalModuleData()) return false;
  ClearLiveMapTextBorrow(std::as_bytes(std::span(caption)));
  caption = {};
  caption_start = 0;
  caption_loaded = false;
  ResetMapDrawingLive();
  ResetMapDrawingModuleData();
  return true;
}

const MapScreenState& LiveMapScreenState() noexcept { return screen; }

std::uint64_t MapTerminalLiveRefusalCount() noexcept { return refused; }
}  // namespace VS::Menu::map
