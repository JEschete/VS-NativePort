#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/SphereGeometryLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/Particles.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraControl.hpp"
#include "Event/EventCameraLens.hpp"
#include "Event/EventCameraLensLive.hpp"
#include "Event/EventCameraTrack.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Event/Script.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptAudio.hpp"
#include "Event/ScriptCamera.hpp"
#include "Event/ScriptEffects.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/SceneNotifications.hpp"
#include "Menu/SceneNotificationsLive.hpp"
#include "Menu/TextBox.hpp"
#include "State/Timers.hpp"
#include "TestSupport/ScriptSources.hpp"

namespace {
struct EventSessionFixture {
  static constexpr std::size_t kArenaBytes = std::size_t{16} * 1024;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, kArenaBytes> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  decltype(VS::Event::playbackPhase) saved_phase = VS::Event::playbackPhase;
  decltype(VS::Event::isStartConfirmed) saved_start =
      VS::Event::isStartConfirmed;
  decltype(VS::Event::isMainThreadFinished) saved_finished =
      VS::Event::isMainThreadFinished;
  decltype(VS::Event::roomChangeState) saved_room = VS::Event::roomChangeState;
  decltype(VS::Event::isAmbientSoundSuspended) saved_ambient =
      VS::Event::isAmbientSoundSuspended;
  decltype(std::to_array(VS::Event::threadWaitFrames)) saved_waits =
      std::to_array(VS::Event::threadWaitFrames);
  decltype(std::to_array(VS::Event::opcodeHandlers)) saved_handlers =
      std::to_array(VS::Event::opcodeHandlers);
  decltype(std::to_array(VS::Event::opcodeSizes)) saved_sizes =
      std::to_array(VS::Event::opcodeSizes);
  char saved_skip = VS::Event::skipState;

  ~EventSessionFixture() {
    VS::Event::playbackPhase = saved_phase;
    VS::Event::isStartConfirmed = saved_start;
    VS::Event::isMainThreadFinished = saved_finished;
    VS::Event::roomChangeState = saved_room;
    VS::Event::isAmbientSoundSuspended = saved_ambient;
    std::copy(saved_waits.begin(), saved_waits.end(),
              VS::Event::threadWaitFrames);
    std::copy(saved_handlers.begin(), saved_handlers.end(),
              VS::Event::opcodeHandlers);
    std::copy(saved_sizes.begin(), saved_sizes.end(), VS::Event::opcodeSizes);
    VS::Event::skipState = saved_skip;
  }
};

struct EventCameraFixture {
  VS::Event::CameraState* saved_camera = VS::Event::cameraWorkspace;
  decltype(VS::State::tickSpeed) saved_speed = VS::State::tickSpeed;
  VS::Event::CameraState camera{};
  VS::Event::EventCameraScratch scratch = VS::Event::LiveEventCameraScratch();

  EventCameraFixture() {
    VS::Event::cameraWorkspace = &camera;
    VS::State::tickSpeed = 2;
    camera.look_at.current_position = {100, 200, 300, 0};
    camera.look_at.destination = {400, 500, 600, 0};
    camera.look_at.partner_position = {1000, 800, 900, 0};
    camera.eye.current_position = camera.look_at.partner_position;
    camera.eye.partner_position = camera.look_at.current_position;
    camera.eye.destination = {1200, 1000, 1100, 0};
  }

  ~EventCameraFixture() {
    VS::Event::LiveEventCameraScratch() = scratch;
    VS::Event::cameraWorkspace = saved_camera;
    VS::State::tickSpeed = saved_speed;
    VS::Core::Gte::Reset();
  }

  void CheckStarted() const {
    CHECK(camera.eye.motion_mode == 4);
    CHECK(camera.look_at.motion_mode == 2);
    CHECK(camera.eye.duration_frames == 85);
    CHECK(camera.look_at.duration_frames == 85);
    const std::array<std::uint8_t, 4> easing{
        camera.eye.easing_primary, camera.eye.easing_secondary,
        camera.eye.easing_tertiary, camera.look_at.easing_primary};
    CHECK(easing == std::array<std::uint8_t, 4>{1, 0, 1, 1});
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
  VS::Menu::StringContext saved_context = VS::Battle::stringContext;
  decltype(std::to_array(VS::Event::isTextBoxAwaitingClose)) saved_waits =
      std::to_array(VS::Event::isTextBoxAwaitingClose);
  int* saved_notification = VS::Menu::sceneNotificationBuffer;
  std::uint8_t saved_timer = VS::Menu::sceneNotificationTimer;
  std::uint8_t saved_timed = VS::Menu::sceneNotificationTimed;

  UiSessionFixture() { VS::Menu::sceneNotificationBuffer = nullptr; }

  ~UiSessionFixture() {
    if (!VS::Menu::CleanupLiveSceneNotificationOwnedState()) std::terminate();
    std::copy(saved_boxes.begin(), saved_boxes.end(), VS::Menu::textBoxes);
    VS::Menu::textBoxSelector = saved_selector;
    VS::Battle::stringContext = saved_context;
    std::copy(saved_waits.begin(), saved_waits.end(),
              VS::Event::isTextBoxAwaitingClose);
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
  std::fill_n(VS::Battle::stringContext.strings, 10, std::begin(text));
  std::ranges::fill(VS::Event::isTextBoxAwaitingClose, 1);
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
         std::all_of(std::begin(VS::Battle::stringContext.strings),
                     std::end(VS::Battle::stringContext.strings),
                     [](const auto* text) { return text == nullptr; }) &&
         std::all_of(std::begin(VS::Event::isTextBoxAwaitingClose),
                     std::end(VS::Event::isTextBoxAwaitingClose),
                     [](std::uint8_t wait) { return wait == 0; }) &&
         VS::Menu::sceneNotificationBuffer == nullptr &&
         VS::Menu::sceneNotificationTimer == 0 &&
         VS::Menu::sceneNotificationTimed == 0;
}

int CheckSessionUi() { return SessionUiIsClear() ? 41 : -1; }

int ReturnWithSessionUi() {
  BorrowSessionUiText();
  return 42;
}

int uiTitleEntries = 0;
bool uiReturnToTitle = false;

int EnterUiTitle() {
  if (!SessionUiIsClear()) return -1;
  if (++uiTitleEntries > 1) return 41;
  BorrowSessionUiText();
  VS::Boot::JumpToLiveGameLoop();
  return -1;
}

int EnterUiBattle() {
  if (!SessionUiIsClear()) return -1;
  if (!uiReturnToTitle) return 41;
  BorrowSessionUiText();
  VS::Boot::JumpToLiveTitle();
  return -1;
}

// Restores the workspaces that replaced the PS1 scratchpad (#45).
struct SessionWorkspaceFixture {
  VS::Event::EventCameraScratch event = VS::Event::LiveEventCameraScratch();
  VS::Effects::LiveParticleEmitterScratch emitter =
      VS::Effects::EmitterWorkspace();
  VS::Battle::SphereWorkspace sphere = VS::Battle::LiveSphereWorkspace();

  SessionWorkspaceFixture() = default;
  SessionWorkspaceFixture(const SessionWorkspaceFixture&) = delete;
  SessionWorkspaceFixture& operator=(const SessionWorkspaceFixture&) = delete;
  SessionWorkspaceFixture(SessionWorkspaceFixture&&) = delete;
  SessionWorkspaceFixture& operator=(SessionWorkspaceFixture&&) = delete;
  ~SessionWorkspaceFixture() {
    VS::Event::LiveEventCameraScratch() = event;
    VS::Effects::EmitterWorkspace() = emitter;
    VS::Battle::LiveSphereWorkspace() = sphere;
  }
};

constexpr std::int32_t kStaleEnvelope = 0x800;
constexpr std::uint16_t kStaleEmitterFlags = 0x1234;
constexpr std::int16_t kStaleSine = 99;

void DirtySessionWorkspaces() {
  VS::Event::LiveEventCameraScratch().envelope = kStaleEnvelope;
  VS::Effects::EmitterWorkspace().saved_flags = kStaleEmitterFlags;
  VS::Battle::LiveSphereWorkspace().sine_samples.back() = kStaleSine;
}

bool SessionWorkspacesAreDirty() {
  return VS::Event::LiveEventCameraScratch().envelope == kStaleEnvelope &&
         VS::Effects::EmitterWorkspace().saved_flags == kStaleEmitterFlags &&
         VS::Battle::LiveSphereWorkspace().sine_samples.back() == kStaleSine;
}

bool SessionWorkspacesAreClear() {
  constexpr VS::Event::EventCameraScratch kColdEventCamera{};
  return VS::Event::LiveEventCameraScratch() == kColdEventCamera &&
         VS::Effects::EmitterWorkspace().saved_flags == 0 &&
         VS::Battle::LiveSphereWorkspace().sine_samples.back() == 0;
}

int workspaceTitleEntries = 0;

int EnterWorkspaceTitle() {
  if (++workspaceTitleEntries > 1) return SessionWorkspacesAreClear() ? 41 : -1;
  DirtySessionWorkspaces();
  VS::Boot::JumpToLiveGameLoop();
  return -1;
}

// The game loop continues the session, so the workspaces carry into it.
int EnterWorkspaceBattle() {
  if (!SessionWorkspacesAreDirty()) return -1;
  VS::Boot::JumpToLiveTitle();
  return -1;
}

std::weak_ptr<const std::vector<std::uint8_t>> scriptSessionOwner;
int scriptTitleEntries = 0;
int scriptFallthrough = 0;

void PrepareSessionScript() {
  const auto source = std::make_shared<const std::vector<std::uint8_t>>(
      std::initializer_list<std::uint8_t>{0xFE});
  scriptSessionOwner = source;
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
  CHECK_FALSE(scriptSessionOwner.expired());
  VS::Boot::JumpToLiveTitle();
}

std::intptr_t SessionTransferOpcode(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return VS::Event::DispatchLiveScriptControlOpcode(instruction, thread,
                                                    TransferScriptToTitle);
}

int EnterScriptTitle() {
  if (++scriptTitleEntries > 1) {
    return scriptSessionOwner.expired() && SessionUiIsClear() ? 41 : -1;
  }
  PrepareSessionScript();
  static_cast<void>(VS::Event::ExecuteLiveScriptThread(0));
  ++scriptFallthrough;
  return -1;
}
}  // namespace

TEST_CASE(
    "Orbit return constants retain both complete original camera commands",
    "[event][camera-return]") {
  // Each command is as long as its opcode's motion command (#163), so the
  // motion entry reads no byte past it.
  CHECK(VS::Event::CameraMotionCommandLength(
            VS::Event::kOrbitReturnEyeCommand.front()) ==
        VS::Event::kOrbitReturnEyeCommand.size());
  CHECK(VS::Event::CameraMotionCommandLength(
            VS::Event::kOrbitReturnLookAtCommand.front()) ==
        VS::Event::kOrbitReturnLookAtCommand.size());
}

TEST_CASE("Orbit return tracks retain original easing and 85-tick completion",
          "[event][camera-return]") {
  EventCameraFixture fixture;
  VS::Event::OrbitCommand eye = VS::Event::kOrbitReturnEyeCommand;
  VS::Event::LinearCommand look_at = VS::Event::kOrbitReturnLookAtCommand;
  VS::Event::CommandCameraMotionEntry(eye.data(), 0);
  VS::Event::CommandCameraMotionEntry(look_at.data(), 0);
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
  VS::Event::playbackPhase = 4;
  VS::Event::isStartConfirmed = 1;
  VS::Event::isMainThreadFinished = 1;
  VS::Event::roomChangeState = 3;
  for (int thread = 0; thread < VS::Event::kScriptThreadCount; ++thread) {
    host.SetThreadCursor(thread, old_script);
  }
  std::fill(std::begin(VS::Event::threadWaitFrames),
            std::end(VS::Event::threadWaitFrames), 30);

  REQUIRE(VS::Event::InitializeLiveScriptOpcodeTable());

  REQUIRE(VS::Event::playbackPhase == 0);
  CHECK(VS::Event::isStartConfirmed == 0);
  CHECK(VS::Event::isMainThreadFinished == 0);
  for (int thread = 0; thread < VS::Event::kScriptThreadCount; ++thread) {
    CHECK_FALSE(host.ThreadCursor(thread).Present());
  }
  CHECK(std::all_of(std::begin(VS::Event::threadWaitFrames),
                    std::end(VS::Event::threadWaitFrames),
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
  uiReturnToTitle = GENERATE(false, true);
  uiTitleEntries = 0;
  CHECK(VS::Boot::RunLiveSessionLoop(EnterUiTitle, EnterUiBattle) == 41);
  CHECK(uiTitleEntries == (uiReturnToTitle ? 2 : 1));
  CHECK(SessionUiIsClear());
}

TEST_CASE("Native title jump returns the scratchpad workspaces to cold boot",
          "[event][session-reset][workspaces]") {
  const UiSessionFixture ui_fixture;
  const SessionWorkspaceFixture workspace_fixture;
  workspaceTitleEntries = 0;
  CHECK(VS::Boot::RunLiveSessionLoop(EnterWorkspaceTitle,
                                     EnterWorkspaceBattle) == 41);
  CHECK(workspaceTitleEntries == 2);
  CHECK(SessionWorkspacesAreClear());
}

TEST_CASE("Native session state reset clears every scratchpad workspace",
          "[event][session-reset][workspaces]") {
  const SessionWorkspaceFixture fixture;
  DirtySessionWorkspaces();
  VS::Event::LiveEventCameraScratch().eye_shake.x = 7;
  VS::Effects::EmitterWorkspace().flags = 0x55;
  VS::Battle::LiveSphereWorkspace().camera_basis.front() = 0x1000;
  VS::Battle::LiveSphereWorkspace().staged_volumes.back().angle = 0x200;
  VS::Battle::LiveSphereWorkspace().basis.back() = -1;
  VS::Battle::LiveSphereWorkspace().volume_transform.m[1][1] = 0x1000;
  VS::Boot::ResetLiveSessionState();
  const VS::Battle::SphereWorkspace& sphere = VS::Battle::LiveSphereWorkspace();
  CHECK(SessionWorkspacesAreClear());
  CHECK(VS::Effects::EmitterWorkspace().flags == 0);
  CHECK(
      std::ranges::all_of(sphere.camera_basis, [](auto v) { return v == 0; }));
  CHECK(std::ranges::all_of(sphere.basis, [](auto v) { return v == 0; }));
  CHECK(
      std::ranges::all_of(sphere.sine_samples, [](auto v) { return v == 0; }));
  CHECK(sphere.staged_volumes.back().angle == 0);
  CHECK(sphere.volume_transform.m[1][1] == 0);
}

TEST_CASE("Native session transfer abandons the active owned VM source",
          "[event-vm-live][session-reset][session-ui]") {
  const EventSessionFixture vm_fixture;
  const UiSessionFixture ui_fixture;
  scriptTitleEntries = 0;
  scriptFallthrough = 0;
  VS::Event::skipState = 0;
  VS::Event::opcodeHandlers[0xFE] = SessionTransferOpcode;
  VS::Event::opcodeSizes[0xFE] = 1;
  CHECK(VS::Boot::RunLiveSessionLoop(EnterScriptTitle, EnterScriptTitle) == 41);
  CHECK(scriptTitleEntries == 2);
  CHECK(scriptFallthrough == 0);
  CHECK(scriptSessionOwner.expired());
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
    uiReturnToTitle = true;
    uiTitleEntries = 0;
    CHECK(VS::Boot::RunLiveSessionLoop(EnterUiTitle, EnterUiBattle) == 41);
    CHECK(uiTitleEntries == 2);
    CHECK(SessionUiIsClear());
  }
}
