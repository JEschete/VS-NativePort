#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/TargetingState.h"
#include "Boot/SessionLive.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Event/Script.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptAudio.hpp"
#include "Event/ScriptCamera.h"
#include "Event/ScriptEffects.hpp"
#include "Event/ScriptVm.h"
#include "Event/ScriptVmLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/SceneNotifications.hpp"
#include "Menu/SceneNotificationsLive.hpp"
#include "Menu/TextBox.hpp"
#include "State/Timers.hpp"
#include "TestSupport/ScriptSources.hpp"

namespace {
constexpr std::array<u_char, 4> kEyeReturn{0xD4, 0x10, 0x10, 0x55};
constexpr std::array<u_char, 3> kLookAtReturn{0xC2, 0x01, 0x55};

struct EventSessionFixture {
  static constexpr std::size_t kArenaBytes = std::size_t{16} * 1024;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, kArenaBytes> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  decltype(Event_playbackPhase) saved_phase = Event_playbackPhase;
  decltype(Event_isStartConfirmed) saved_start = Event_isStartConfirmed;
  decltype(Event_isMainThreadFinished) saved_finished =
      Event_isMainThreadFinished;
  decltype(Event_roomChangeState) saved_room = Event_roomChangeState;
  decltype(VS::Event::isAmbientSoundSuspended) saved_ambient =
      VS::Event::isAmbientSoundSuspended;
  decltype(Event_scriptBuffer) saved_script = Event_scriptBuffer;
  decltype(std::to_array(Event_threadCursors)) saved_threads =
      std::to_array(Event_threadCursors);
  decltype(std::to_array(Event_threadWaitFrames)) saved_waits =
      std::to_array(Event_threadWaitFrames);
  decltype(std::to_array(Event_kOpcodeHandlers)) saved_handlers =
      std::to_array(Event_kOpcodeHandlers);
  decltype(std::to_array(Event_kOpcodeSizes)) saved_sizes =
      std::to_array(Event_kOpcodeSizes);
  char saved_skip = VS::Event::skipState;

  ~EventSessionFixture() {
    Event_playbackPhase = saved_phase;
    Event_isStartConfirmed = saved_start;
    Event_isMainThreadFinished = saved_finished;
    Event_roomChangeState = saved_room;
    VS::Event::isAmbientSoundSuspended = saved_ambient;
    Event_scriptBuffer = saved_script;
    std::copy(saved_threads.begin(), saved_threads.end(), Event_threadCursors);
    std::copy(saved_waits.begin(), saved_waits.end(), Event_threadWaitFrames);
    std::copy(saved_handlers.begin(), saved_handlers.end(),
              Event_kOpcodeHandlers);
    std::copy(saved_sizes.begin(), saved_sizes.end(), Event_kOpcodeSizes);
    VS::Event::skipState = saved_skip;
  }
};

struct EventCameraFixture {
  // The camera track rules use the scratch words from 0x1F800088 (decomp
  // cameraScratch, 0x90 bytes).
  static constexpr std::size_t kCameraScratchOffset = 0x88;
  static constexpr std::size_t kCameraScratchBytes = 0x90;
  VS::Event::CameraState* saved_camera = Event_camera;
  decltype(VS::State::tickSpeed) saved_speed = VS::State::tickSpeed;
  VS::Event::CameraState camera{};
  std::array<std::byte, kCameraScratchBytes> scratch{};
  std::array<u_char, sizeof Event_kOrbitReturnFirstCommand> saved_eye{};
  std::array<u_char, sizeof Event_kOrbitReturnSecondCommand> saved_look_at{};

  EventCameraFixture() {
    std::memcpy(saved_eye.data(), &Event_kOrbitReturnFirstCommand,
                saved_eye.size());
    std::memcpy(saved_look_at.data(), &Event_kOrbitReturnSecondCommand,
                saved_look_at.size());
    std::memcpy(
        scratch.data(),
        static_cast<std::byte*>(VS::Core::ScratchBase()) + kCameraScratchOffset,
        scratch.size());
    Event_camera = &camera;
    VS::State::tickSpeed = 2;
    camera.look_at.current_position = {100, 200, 300, 0};
    camera.look_at.destination = {400, 500, 600, 0};
    camera.look_at.partner_position = {1000, 800, 900, 0};
    camera.eye.current_position = camera.look_at.partner_position;
    camera.eye.partner_position = camera.look_at.current_position;
    camera.eye.destination = {1200, 1000, 1100, 0};
  }

  ~EventCameraFixture() {
    std::memcpy(&Event_kOrbitReturnFirstCommand, saved_eye.data(),
                saved_eye.size());
    std::memcpy(&Event_kOrbitReturnSecondCommand, saved_look_at.data(),
                saved_look_at.size());
    std::memcpy(
        static_cast<std::byte*>(VS::Core::ScratchBase()) + kCameraScratchOffset,
        scratch.data(), scratch.size());
    Event_camera = saved_camera;
    VS::State::tickSpeed = saved_speed;
    Core_Gte_Reset();
  }

  static void LoadCommand(void* target, unsigned offset, unsigned size,
                          std::span<const u_char> image) {
    const auto entries = VS::Content::ModuleDataEntries();
    const auto entry = std::ranges::find_if(
        entries, [target](const auto& item) { return item.target == target; });
    REQUIRE(entry != entries.end());
    CHECK(entry->offset == offset);
    CHECK(entry->size == size);
    CHECK(entry->capacity == size);
    REQUIRE(VS::Content::CopyModuleDataEntry(*entry, image));
  }

  void LoadCommands() {
    std::vector<u_char> image(0x81430);
    std::copy(kEyeReturn.begin(), kEyeReturn.end(), image.begin() + 0x81428);
    std::copy(kLookAtReturn.begin(), kLookAtReturn.end(),
              image.begin() + 0x8142C);
    LoadCommand(&Event_kOrbitReturnFirstCommand, 0x81428, kEyeReturn.size(),
                image);
    LoadCommand(&Event_kOrbitReturnSecondCommand, 0x8142C, kLookAtReturn.size(),
                image);
  }

  void CheckStarted() const {
    CHECK(camera.eye.motion_mode == 4);
    CHECK(camera.look_at.motion_mode == 2);
    CHECK(camera.eye.duration_frames == 85);
    CHECK(camera.look_at.duration_frames == 85);
    const std::array<u_char, 4> easing{
        camera.eye.easing_primary, camera.eye.easing_secondary,
        camera.eye.easing_tertiary, camera.look_at.easing_primary};
    CHECK(easing == std::array<u_char, 4>{1, 0, 1, 1});
  }

  void CheckFinished() const {
    CHECK(camera.eye.motion_mode == 15);
    CHECK(camera.look_at.motion_mode == 15);
    CHECK(camera.eye.elapsed_frames == 85);
    CHECK(camera.look_at.elapsed_frames == 85);
    const auto& current = camera.look_at.current_position;
    const auto& destination = camera.look_at.destination;
    const std::array current_position{current.vx, current.vy, current.vz};
    const std::array expected_position{destination.vx, destination.vy,
                                       destination.vz};
    CHECK(current_position == expected_position);
  }
};

struct UiSessionFixture {
  static constexpr std::size_t kArenaBytes = std::size_t{16} * 1024;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, kArenaBytes> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<VS::Menu::TextBoxState, 8> saved_boxes =
      std::to_array(VS::Menu::textBoxes);
  VS::Menu::TextBoxSelectorState saved_selector = VS::Menu::textBoxSelector;
  VS::Menu::StringContext saved_context = Battle_stringContext;
  std::array<char, 80> saved_waits =
      std::to_array(Event_isTextBoxAwaitingClose);
  int* saved_notification = VS::Menu::sceneNotificationBuffer;
  std::uint8_t saved_timer = VS::Menu::sceneNotificationTimer;
  std::uint8_t saved_timed = VS::Menu::sceneNotificationTimed;

  UiSessionFixture() { VS::Menu::sceneNotificationBuffer = nullptr; }

  ~UiSessionFixture() {
    if (!VS::Menu::CleanupLiveSceneNotificationOwnedState()) std::terminate();
    std::copy(saved_boxes.begin(), saved_boxes.end(), VS::Menu::textBoxes);
    VS::Menu::textBoxSelector = saved_selector;
    Battle_stringContext = saved_context;
    std::copy(saved_waits.begin(), saved_waits.end(),
              Event_isTextBoxAwaitingClose);
    VS::Menu::sceneNotificationBuffer = saved_notification;
    VS::Menu::sceneNotificationTimer = saved_timer;
    VS::Menu::sceneNotificationTimed = saved_timed;
  }
};

void BorrowSessionUiText() {
  static char text[] = {'A', 0};
  REQUIRE(VS::Menu::sceneNotificationBuffer == nullptr);
  auto* notification =
      static_cast<int*>(VS::Core::Memory::AllocateLiveFromHighEnd(
          VS::Menu::kSceneNotificationBytes));
  REQUIRE(notification != nullptr);
  for (auto& box : VS::Menu::textBoxes) {
    box.state = 4;
    box.open_duration = 3;
    box.text = text;
  }
  VS::Menu::textBoxSelector.count = 2;
  VS::Menu::textBoxSelector.selected_index = 1;
  std::fill_n(Battle_stringContext.strings, 10, std::begin(text));
  std::fill_n(Event_isTextBoxAwaitingClose, 80, 1);
  VS::Menu::sceneNotificationBuffer = notification;
  VS::Menu::sceneNotificationTimer = 17;
  VS::Menu::sceneNotificationTimed = 1;
}

bool SessionUiIsClear() {
  return std::all_of(std::begin(VS::Menu::textBoxes),
                     std::end(VS::Menu::textBoxes),
                     [](const auto& box) {
                       // NOLINTNEXTLINE(build/include_what_you_use)
                       const auto* text = box.text;
                       return box.state == 0 && text == nullptr;
                     }) &&
         VS::Menu::textBoxSelector.count == 0 &&
         VS::Menu::textBoxSelector.selected_index == 0 &&
         std::all_of(std::begin(Battle_stringContext.strings),
                     std::end(Battle_stringContext.strings),
                     [](const auto* text) { return text == nullptr; }) &&
         std::all_of(std::begin(Event_isTextBoxAwaitingClose),
                     std::end(Event_isTextBoxAwaitingClose),
                     [](char wait) { return wait == 0; }) &&
         VS::Menu::sceneNotificationBuffer == nullptr &&
         VS::Menu::sceneNotificationTimer == 0 &&
         VS::Menu::sceneNotificationTimed == 0;
}

int CheckSessionUi() { return SessionUiIsClear() ? 41 : -1; }

int ReturnWithSessionUi() {
  BorrowSessionUiText();
  return 42;
}

int ui_title_entries = 0;
bool ui_return_to_title = false;

int EnterUiTitle() {
  if (!SessionUiIsClear()) return -1;
  if (++ui_title_entries > 1) return 41;
  BorrowSessionUiText();
  VS::Boot::JumpToLiveGameLoop();
  return -1;
}

int EnterUiBattle() {
  if (!SessionUiIsClear()) return -1;
  if (!ui_return_to_title) return 41;
  BorrowSessionUiText();
  VS::Boot::JumpToLiveTitle();
  return -1;
}

std::weak_ptr<const std::vector<std::uint8_t>> script_session_owner;
int script_title_entries = 0;
int script_fallthrough = 0;

void PrepareSessionScript() {
  const auto source = std::make_shared<const std::vector<std::uint8_t>>(
      std::initializer_list<std::uint8_t>{0xFE});
  script_session_owner = source;
  REQUIRE(VS::Event::MountLiveScriptSource(
              VS::Event::ScriptRegion::kEventScript, source)
              .has_value());
  const auto start =
      VS::Event::LeaseLiveScriptSource(VS::Event::ScriptRegion::kEventScript)
          .value()
          .At(0);
  if (!start) {
    throw std::logic_error("Session test script has no first instruction");
  }
  VS::Event::LiveScriptVm().SetThreadCursor(0, *start);
}

VS::Event::ScriptFlow TransferScriptToTitle(const VS::Event::ScriptInstruction&,
                                            VS::Event::ScriptVmHost&) {
  CHECK_FALSE(script_session_owner.expired());
  VS::Boot::JumpToLiveTitle();
}

std::intptr_t SessionTransferOpcode(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return VS::Event::DispatchLiveScriptControlOpcode(instruction, thread,
                                                    TransferScriptToTitle);
}

int EnterScriptTitle() {
  if (++script_title_entries > 1) {
    return script_session_owner.expired() && SessionUiIsClear() ? 41 : -1;
  }
  PrepareSessionScript();
  static_cast<void>(VS::Event::ExecuteLiveScriptThread(0));
  ++script_fallthrough;
  return -1;
}
}  // namespace

TEST_CASE(
    "Orbit return constants retain both complete original camera commands",
    "[event][camera-return]") {
  EventCameraFixture fixture;
  CHECK(sizeof Event_kOrbitReturnFirstCommand == kEyeReturn.size());
  CHECK(sizeof Event_kOrbitReturnSecondCommand == kLookAtReturn.size());
  if (sizeof Event_kOrbitReturnFirstCommand < kEyeReturn.size() ||
      sizeof Event_kOrbitReturnSecondCommand < kLookAtReturn.size()) {
    return;
  }
  fixture.LoadCommands();
  CHECK(std::memcmp(&Event_kOrbitReturnFirstCommand, kEyeReturn.data(),
                    kEyeReturn.size()) == 0);
  CHECK(std::memcmp(&Event_kOrbitReturnSecondCommand, kLookAtReturn.data(),
                    kLookAtReturn.size()) == 0);
}

TEST_CASE("Orbit return tracks retain original easing and 85-tick completion",
          "[event][camera-return]") {
  EventCameraFixture fixture;
  REQUIRE(sizeof Event_kOrbitReturnFirstCommand == 4);
  REQUIRE(sizeof Event_kOrbitReturnSecondCommand == 3);
  fixture.LoadCommands();
  VS::Event::CommandCameraMotionEntry(
      reinterpret_cast<u_char*>(&Event_kOrbitReturnFirstCommand), 0);
  VS::Event::CommandCameraMotionEntry(
      reinterpret_cast<u_char*>(&Event_kOrbitReturnSecondCommand), 0);
  fixture.CheckStarted();
  for (int frame = 0; frame < 85; ++frame) {
    VS::Event::UpdateEventCameraTrackEntry(&fixture.camera.look_at,
                                           &fixture.camera.eye);
    VS::Event::UpdateEventCameraTrackEntry(&fixture.camera.eye,
                                           &fixture.camera.look_at);
  }
  fixture.CheckFinished();
}

TEST_CASE("Gameplay initialization cannot resume a previous Game Over event",
          "[event][session-reset]") {
  EventSessionFixture fixture;
  const VS::test_support::LiveScriptRuntimeScope runtime;
  const auto old_script =
      VS::test_support::MountLiveEventScript(std::vector<std::uint8_t>(16));
  auto& host = VS::Event::LiveScriptVm();
  Event_playbackPhase = 4;
  Event_isStartConfirmed = 1;
  Event_isMainThreadFinished = 1;
  Event_roomChangeState = 3;
  for (int thread = 0; thread < VS::Event::kScriptThreadCount; ++thread) {
    host.SetThreadCursor(thread, old_script);
  }
  std::fill(std::begin(Event_threadWaitFrames),
            std::end(Event_threadWaitFrames), 30);

  REQUIRE(VS::Event::InitializeLiveScriptOpcodeTable());

  REQUIRE(Event_playbackPhase == 0);
  CHECK(Event_isStartConfirmed == 0);
  CHECK(Event_isMainThreadFinished == 0);
  for (int thread = 0; thread < VS::Event::kScriptThreadCount; ++thread) {
    CHECK_FALSE(host.ThreadCursor(thread).Present());
  }
  CHECK(std::all_of(std::begin(Event_threadWaitFrames),
                    std::end(Event_threadWaitFrames),
                    [](auto wait) { return wait == 0; }));
  CHECK(VS::Event::UpdateLiveEventPlayback() == 0);
}

TEST_CASE("Native session entry invalidates every borrowed textbox",
          "[event][session-reset][session-ui]") {
  UiSessionFixture fixture;
  BorrowSessionUiText();
  CHECK(VS::Boot::RunLiveSessionLoop(CheckSessionUi, CheckSessionUi) == 41);
  CHECK(SessionUiIsClear());
}

TEST_CASE("Native session return releases borrowed UI without freeing its data",
          "[event][session-reset][session-ui]") {
  UiSessionFixture fixture;
  CHECK(VS::Boot::RunLiveSessionLoop(ReturnWithSessionUi, CheckSessionUi) ==
        42);
  CHECK(SessionUiIsClear());
}

TEST_CASE("Native title and battle jumps invalidate interrupted scene UI",
          "[event][session-reset][session-ui]") {
  UiSessionFixture fixture;
  ui_return_to_title = GENERATE(false, true);
  ui_title_entries = 0;
  CHECK(VS::Boot::RunLiveSessionLoop(EnterUiTitle, EnterUiBattle) == 41);
  CHECK(ui_title_entries == (ui_return_to_title ? 2 : 1));
  CHECK(SessionUiIsClear());
}

TEST_CASE("Native session transfer abandons the active owned VM source",
          "[event-vm-live][session-reset][session-ui]") {
  const EventSessionFixture vm_fixture;
  const UiSessionFixture ui_fixture;
  script_title_entries = 0;
  script_fallthrough = 0;
  VS::Event::skipState = 0;
  Event_kOpcodeHandlers[0xFE] = SessionTransferOpcode;
  Event_kOpcodeSizes[0xFE] = 1;
  CHECK(VS::Boot::RunLiveSessionLoop(EnterScriptTitle, EnterScriptTitle) == 41);
  CHECK(script_title_entries == 2);
  CHECK(script_fallthrough == 0);
  CHECK(script_session_owner.expired());
  CHECK_FALSE(
      VS::Event::LeaseLiveScriptSource(VS::Event::ScriptRegion::kEventScript)
          .has_value());
}

TEST_CASE("Retired event lifecycle entries have no native module bindings",
          "[event-vm-live][content][module][retired-entry]") {
  const auto symbols = VS::Content::ModuleSymbols();
  constexpr std::array<std::uint32_t, 15> kEntries{
      0x800BE98C, 0x800BEB34, 0x800BEBEC, 0x800BF5F0, 0x800BF850,
      0x800BFBB8, 0x800BFD9C, 0x800B6778, 0x800BEB00, 0x800BEB10,
      0x800BEC30, 0x800AACDC, 0x800C0150, 0x800BFE34, 0x800C00E8};
  for (const auto address : kEntries) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}

TEST_CASE("Retired stack capture and room code jumps have no native bindings",
          "[event][session-reset][retired-entry]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", 0x80010AA4) ==
        nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80098B1C) == nullptr);
  UiSessionFixture fixture;
  for (int iteration = 0; iteration < 32; ++iteration) {
    ui_return_to_title = true;
    ui_title_entries = 0;
    CHECK(VS::Boot::RunLiveSessionLoop(EnterUiTitle, EnterUiBattle) == 41);
    CHECK(ui_title_entries == 2);
    CHECK(SessionUiIsClear());
  }
}
