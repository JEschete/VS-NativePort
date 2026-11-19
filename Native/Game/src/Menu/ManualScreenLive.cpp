// Hand-maintained (tools/port/owned.csv).
// Live binding for the manual's screen and help pages (ManualScreen.cpp):
// MENUE.PRG's statics, the menu framework, the HUD, the game heap, the disc
// queue, the display state, the buttons and the frame's ordering table.
#include "Menu/ManualScreenLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "Audio/SfxLive.hpp"
#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Field/Manual.h"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Manual.h"
#include "Menu/ManualIllustrationLive.hpp"
#include "Menu/ManualPageLive.hpp"
#include "Menu/ManualScreen.hpp"
#include "Menu/ManualTopics.hpp"
#include "Menu/ManualTopicsLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"

namespace VS::Menu::manual {
namespace {
static_assert(kHelpFileLoaded == Content_DiscRequestState_Loaded);
static_assert(kHelpFileFailed == Content_DiscRequestState_Error);
static_assert(kNoMenuItem == Framework::kNoRow);
static_assert(
    std::extent_v<decltype(Field_ManualDisplayState::scrollPositions)> ==
    std::size_t{kTopicCount});

constexpr std::uint32_t kRefusalLogLimit = 16;
constexpr std::uint32_t kRefusalLogInterval = 1024;
// D_800F4EE8.cursorMemories: the manual's list cursor row and first row.
constexpr std::size_t kCursorRowMemory = 4;
constexpr std::size_t kCursorScrollMemory = 5;
std::uint64_t g_refusals = 0;
ScreenState g_screen;

void RefuseLive(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kRefusalLogLimit || g_refusals % kRefusalLogInterval == 0) {
    VS::Core::LogWarn("menu.manual", "{} (refusal {})", reason, g_refusals);
  }
}

class LiveScreenHost final : public ScreenHost {
 public:
  void ClearMenuExcept(std::int32_t row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  [[nodiscard]] std::int32_t MenuRowsIdle() override {
    return Framework::AreLiveMenuRowsIdle();
  }
  void SetBackgroundFade(std::int32_t fade) override {
    Framework::SetLiveBackgroundFade(fade);
  }
  [[nodiscard]] std::int32_t InitializeHelp() override {
    return LiveInitHelpState();
  }
  [[nodiscard]] std::int32_t UpdateHelpPages() override {
    return LiveUpdateHelpPages();
  }
  void SetFieldRenderingEnabled(std::int32_t enabled) override {
    VS::Field::LiveSetFieldRenderingEnabled(enabled);
  }
  void SetHudHidden(std::int32_t hidden) override {
    VS::Battle::SetLiveHudHidden(hidden);
  }
  void SetHudSlideOffset(std::int32_t offset) override {
    Battle_hudSlideOffset = offset;
  }
  void SetBackgroundPriority(std::int32_t layer,
                             std::int32_t brightness) override {
    Framework::SetLiveBackgroundPriority(layer, brightness);
  }
  void SetMenuCommand(Framework::MenuCommand command) override {
    Framework::SetLiveCommandLabel(command);
  }
  [[nodiscard]] std::int32_t BackgroundFadeStep() const override {
    return VS::Menu::Framework::backgroundFadeStep;
  }
  void SetCurrentMenu(std::int32_t menu) override {
    VS::Menu::Framework::menuState.control.current_menu =
        static_cast<std::uint8_t>(menu);
  }
  void RevealMenu() override { VS::Menu::Framework::hideMenu = 0; }
};

[[nodiscard]] HelpPagesState LoadHelpState() {
  return {.step = Menu_Manual_showMenuState,
          .scroll = Menu_Manual_scrollPosition,
          .line_count = Menu_Manual_lineCount,
          .arrow_frame = Menu_Manual_pageArrowAnimState,
          .load_counter = Menu_Manual_helpPageLoadCounter};
}

void StoreHelpState(const HelpPagesState& state) {
  Menu_Manual_showMenuState = state.step;
  Menu_Manual_scrollPosition = state.scroll;
  Menu_Manual_lineCount = state.line_count;
  Menu_Manual_pageArrowAnimState = state.arrow_frame;
  Menu_Manual_helpPageLoadCounter = state.load_counter;
}

[[nodiscard]] void* HelpBuffer(HelpFile file) {
  return file == HelpFile::kText ? static_cast<void*>(Menu_Manual_helpText)
                                 : static_cast<void*>(Menu_Manual_helpAssets);
}

void SetHelpBuffer(HelpFile file, void* buffer) {
  if (file == HelpFile::kText) {
    Menu_Manual_helpText = static_cast<int*>(buffer);
  } else {
    Menu_Manual_helpAssets = static_cast<u_long*>(buffer);
  }
}

[[nodiscard]] Content_DiscQueueSlot*& HelpRequest(HelpFile file) {
  return file == HelpFile::kText ? Menu_Manual_helpTextCdQueue
                                 : Menu_Manual_helpAssetsCdQueue;
}

// _helpFileCdFiles[topic * 2 + file]; the core checks the topic.
[[nodiscard]] const Content_DiscFile& HelpFileOf(HelpFile file,
                                                 std::int32_t topic) {
  const auto index = (static_cast<std::size_t>(topic) * kHelpFilesPerTopic) +
                     static_cast<std::size_t>(file);
  return Menu_Manual_kHelpFileCdFiles[index];
}

// The asset buffer's bytes, or nothing when it is missing or not a live
// game heap payload.
[[nodiscard]] std::optional<std::size_t> HelpAssetBytes() {
  if (Menu_Manual_helpAssets == nullptr) {
    return std::nullopt;
  }
  const auto capacity =
      Core::Memory::LivePayloadCapacity(Menu_Manual_helpAssets);
  if (!capacity) {
    return std::nullopt;
  }
  return *capacity;
}

class LiveHelpPagesHost final : public HelpPagesHost {
 public:
  explicit LiveHelpPagesHost(const HelpPagesState& state) : state_(state) {}

  // The other manual bindings read the help page statics.
  void Publish() const { StoreHelpState(state_); }

  [[nodiscard]] std::uint8_t CursorMemorySetting() const override {
    return static_cast<std::uint8_t>(VS::Input::settings.cursor_memory);
  }
  void ClearDisplayState() override {
    Field_manualDisplayState = Field_ManualDisplayState{};
  }
  void ReserveTexturePages(std::uint32_t first, std::uint32_t count,
                           std::int32_t owner) override {
    static_cast<void>(Render::ReserveLiveTexturePageRange(first, count, owner));
  }
  void RetireTexturePages(std::uint32_t first, std::uint32_t count,
                          std::int32_t owner) override {
    static_cast<void>(Render::RetireLiveTexturePageRange(first, count, owner));
  }
  void SetBackgroundPriority(std::int32_t layer,
                             std::int32_t brightness) override {
    Framework::SetLiveBackgroundPriority(layer, brightness);
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  [[nodiscard]] std::int32_t UpdateTopicMenu(std::int32_t init) override {
    Publish();
    return LiveUpdateTopicMenu(init);
  }
  [[nodiscard]] TopicCursor TopicMenuCursor() const override {
    return {
        .row =
            VS::Menu::Framework::menuMemory.cursor_memories[kCursorRowMemory],
        .scroll = VS::Menu::Framework::menuMemory
                      .cursor_memories[kCursorScrollMemory]};
  }
  [[nodiscard]] std::uint16_t CurrentManual() const override {
    return Field_manualDisplayState.currentManual;
  }
  void SetCurrentManual(std::uint16_t manual) override {
    Field_manualDisplayState.currentManual = manual;
  }
  [[nodiscard]] std::uint16_t ScrollMemory(std::size_t topic) const override {
    return Field_manualDisplayState.scrollPositions[topic];
  }
  void SetScrollMemory(std::size_t topic, std::uint16_t scroll) override {
    Field_manualDisplayState.scrollPositions[topic] = scroll;
  }
  [[nodiscard]] bool HelpBufferHeld(HelpFile file) const override {
    return HelpBuffer(file) != nullptr;
  }
  void FreeHelpBuffer(HelpFile file) override {
    Core::Memory::FreeLiveFromHighEnd(HelpBuffer(file));
  }
  void ForgetHelpBuffer(HelpFile file) override {
    SetHelpBuffer(file, nullptr);
  }
  void AllocateHelpBuffer(HelpFile file, std::int32_t topic) override {
    const auto bytes = static_cast<std::uint32_t>(HelpFileOf(file, topic).size);
    SetHelpBuffer(file, Core::Memory::AllocateLiveFromHighEnd(bytes));
  }
  // A full queue reads the file at once (#282), as the other copied callers
  // do.
  void RequestHelpFile(HelpFile file, std::int32_t topic) override {
    const Content_DiscFile& disc_file = HelpFileOf(file, topic);
    Content_DiscQueueSlot*& request = HelpRequest(file);
    request = Content::AllocateLiveDiscRequest(&disc_file);
    if (request != nullptr) {
      Content::EnqueueLiveDiscRequest(request, HelpBuffer(file));
    } else {
      request =
          Content::CompleteLiveRefusedDiscRequest(&disc_file, HelpBuffer(file));
    }
  }
  // A missing request can never load, so it reads as failed and the core
  // returns to the topic list.
  [[nodiscard]] std::int16_t HelpFileState(HelpFile file) const override {
    const Content_DiscQueueSlot* const request = HelpRequest(file);
    if (request == nullptr) {
      RefuseLive("Manual help file has no disc request");
      return Content_DiscRequestState_Error;
    }
    return request->state;
  }
  // The original keeps the released request's pointer; nothing reads it
  // before the next request replaces it, so the binding drops it.
  void ReleaseHelpFile(HelpFile file) override {
    Content_DiscQueueSlot*& request = HelpRequest(file);
    Content::ReleaseLiveDiscRequest(request);
    request = nullptr;
  }
  void CancelHelpFile(HelpFile file) override {
    void* const buffer = HelpBuffer(file);
    if (buffer == nullptr) {
      return;
    }
    const auto bytes = Core::Memory::LivePayloadCapacity(buffer);
    if (!bytes || *bytes == 0 ||
        !Content::CancelLiveDiscWrites(
            {static_cast<std::byte*>(buffer), *bytes})) {
      RefuseLive("Manual help read could not be cancelled");
    }
  }
  [[nodiscard]] std::optional<HelpAssetHeader> HelpAssets() const override {
    const std::optional<std::size_t> bytes = HelpAssetBytes();
    if (!bytes || *bytes < kHelpAssetHeaderBytes) {
      return std::nullopt;
    }
    HelpAssetHeader header;
    std::memcpy(&header.sprite_tiles, Menu_Manual_helpAssets,
                sizeof header.sprite_tiles);
    std::memcpy(&header.cluts,
                reinterpret_cast<const std::byte*>(Menu_Manual_helpAssets) +
                    sizeof header.sprite_tiles,
                sizeof header.cluts);
    return header;
  }
  void UploadSpriteTiles(std::uint32_t byte_offset,
                         std::uint32_t count) override {
    if (u_long* const first = AssetsAt(byte_offset); first != nullptr) {
      LiveUploadSpriteTiles(first, static_cast<int>(count));
    }
  }
  void UploadCluts(std::uint32_t byte_offset, std::uint32_t count) override {
    if (u_long* const first = AssetsAt(byte_offset); first != nullptr) {
      LiveUploadCluts(first, static_cast<int>(count));
    }
  }
  void InitTextBox(const TextBoxPlacement& placement) override {
    Menu::InitLiveTextBox(placement.id, placement.flags, placement.x,
                          placement.y, placement.chars_per_line,
                          placement.line_count, placement.center_x,
                          placement.center_y);
  }
  void HideTextBox(std::int32_t id) override {
    Menu_TextBoxState* const box = Menu::LiveTextBox(id);
    if (box == nullptr) {
      RefuseLive("Manual page text box is missing");
      return;
    }
    box->state = 0;
  }
  [[nodiscard]] std::int32_t LastPageLineOffset() override {
    Publish();
    return LiveLastPageLineOffset();
  }
  [[nodiscard]] std::uint32_t PressedButtons() const override {
    return VS::Input::buttonsPressed;
  }
  [[nodiscard]] std::uint32_t PreviousButtons() const override {
    return static_cast<std::uint32_t>(VS::Input::previousButtons);
  }
  [[nodiscard]] std::uint32_t RepeatedButtons() const override {
    return static_cast<std::uint32_t>(VS::Input::buttonRepeat);
  }
  void PlaySfx(std::int32_t group, std::int32_t effect) override {
    Audio::LivePlaySfxCentered(group, effect);
  }
  void DrawButtonIcon(Framework::ButtonIcon button, std::int32_t x,
                      std::int32_t y, std::int32_t layer) override {
    if (u_long* const entry = OrderingEntry(layer); entry != nullptr) {
      Framework::DrawLiveButtonIcon(button, x, y, entry);
    }
  }
  void DrawText(std::string_view text, std::int32_t xy, std::int32_t color,
                std::int32_t layer) override {
    if (u_long* const entry = OrderingEntry(layer); entry != nullptr) {
      const std::string terminated(text);
      Render::DrawLiveCompactTextColor(terminated.c_str(), xy, color, entry);
    }
  }
  void DrawControlsBackground(const ScreenRect& rect) override {
    Publish();
    LiveDrawControlsBackground(rect.x, rect.y, rect.w, rect.h);
  }
  void DrawPageBackground(const ScreenRect& rect, std::int32_t color) override {
    Publish();
    LiveDrawPageBackground(rect.x, rect.y, rect.w, rect.h, color);
  }
  void DrawUpperScrollFade() override {
    Publish();
    LiveDrawUpperScrollFade();
  }
  void DrawLowerScrollFade() override {
    Publish();
    LiveDrawLowerScrollFade();
  }
  void DrawPageContent() override {
    Publish();
    LiveDrawPageContent();
  }
  void CopyPageText() override {
    Publish();
    LiveCopyPageText();
  }
  void Refuse(std::string_view reason) override { RefuseLive(reason); }

 private:
  // The original's scratch[1] + layer: the frame's scene ordering table.
  [[nodiscard]] static u_long* OrderingEntry(std::int32_t layer) {
    u_long* const entry =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, layer);
    if (entry == nullptr) {
      RefuseLive("Manual controls have no frame ordering table");
    }
    return entry;
  }

  // The asset buffer at `byte_offset`, which must lie inside it; the
  // uploads check their counts against the rest of it.
  [[nodiscard]] static u_long* AssetsAt(std::uint32_t byte_offset) {
    const std::optional<std::size_t> bytes = HelpAssetBytes();
    if (!bytes || byte_offset >= *bytes) {
      RefuseLive("Manual help upload starts past its asset buffer");
      return nullptr;
    }
    return reinterpret_cast<u_long*>(
        reinterpret_cast<std::byte*>(Menu_Manual_helpAssets) + byte_offset);
  }

  const HelpPagesState& state_;
};
}  // namespace

// decomp: MENUE.PRG func_80102E9C (src/MENU/MENUE.PRG/494.c:132), 0x80102E9C
void LiveClearMenuExceptRow8() {
  LiveScreenHost host;
  ClearMenuExceptRow8(host);
}

// decomp: MENUE.PRG _menuReady (src/MENU/MENUE.PRG/494.c:134), 0x80102EBC
void LivePollMenuReady() {
  LiveScreenHost host;
  PollMenuReady(host);
}

// decomp: MENUE.PRG vs_menuE_exec (src/MENU/MENUE.PRG/494.c:142), 0x80102EDC
int LiveUpdateScreen(u_char* state) {
  if (state == nullptr) {
    RefuseLive("Manual screen step is null");
    return 0;
  }
  LiveScreenHost host;
  return UpdateScreen(host, *state, g_screen);
}

// decomp: MENUE.PRG _initMenuState (src/MENU/MENUE.PRG/494.c:219), 0x801030A4
int LiveInitHelpState() {
  HelpPagesState state = LoadHelpState();
  LiveHelpPagesHost host(state);
  const std::int32_t result = InitHelpState(host, state);
  StoreHelpState(state);
  return result;
}

// decomp: MENUE.PRG _showMenu (src/MENU/MENUE.PRG/494.c:245), 0x80103110
int LiveUpdateHelpPages() {
  HelpPagesState state = LoadHelpState();
  LiveHelpPagesHost host(state);
  const std::int32_t result = UpdateHelpPages(host, state);
  StoreHelpState(state);
  return result;
}

void ResetLiveManualScreenStatics() { g_screen = {}; }

ScreenState& LiveManualScreenState() noexcept { return g_screen; }

std::uint64_t LiveManualScreenRefusalCount() noexcept { return g_refusals; }
}  // namespace VS::Menu::manual
