#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Event/Camera.h"
#include "Event/Script.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptEffects.h"
#include "Event/ScriptVm.h"
#include "State/Timers.h"
}

namespace {
constexpr std::array<u_char, 4> kEyeReturn{0xD4, 0x10, 0x10, 0x55};
constexpr std::array<u_char, 3> kLookAtReturn{0xC2, 0x01, 0x55};

struct EventSessionFixture {
  static constexpr std::size_t kArenaBytes = std::size_t{16} * 1024;
  alignas(Core_HeapBlockHeader) std::array<std::byte, kArenaBytes> arena{};
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  decltype(Event_playbackPhase) saved_phase = Event_playbackPhase;
  decltype(Event_isStartConfirmed) saved_start = Event_isStartConfirmed;
  decltype(Event_isMainThreadFinished) saved_finished =
      Event_isMainThreadFinished;
  decltype(Event_roomChangeState) saved_room = Event_roomChangeState;
  decltype(Event_isAmbientSoundSuspended) saved_ambient =
      Event_isAmbientSoundSuspended;
  decltype(Event_scriptBuffer) saved_script = Event_scriptBuffer;
  decltype(Event_vibrationPatternBuffer) saved_vibration =
      Event_vibrationPatternBuffer;
  decltype(Event_legacySkipHandlerCopy) saved_dispatch =
      Event_legacySkipHandlerCopy;
  decltype(std::to_array(Event_threadCursors)) saved_threads =
      std::to_array(Event_threadCursors);
  decltype(std::to_array(Event_threadWaitFrames)) saved_waits =
      std::to_array(Event_threadWaitFrames);

  EventSessionFixture() {
    Core_Memory_InitializeHeap(
        reinterpret_cast<Core_HeapBlockHeader*>(arena.data()),
        static_cast<u_int>(arena.size()));
  }

  ~EventSessionFixture() {
    Event_playbackPhase = saved_phase;
    Event_isStartConfirmed = saved_start;
    Event_isMainThreadFinished = saved_finished;
    Event_roomChangeState = saved_room;
    Event_isAmbientSoundSuspended = saved_ambient;
    Event_scriptBuffer = saved_script;
    Event_vibrationPatternBuffer = saved_vibration;
    Event_legacySkipHandlerCopy = saved_dispatch;
    std::copy(saved_threads.begin(), saved_threads.end(), Event_threadCursors);
    std::copy(saved_waits.begin(), saved_waits.end(), Event_threadWaitFrames);
    Core_legacyHeapList = saved_heap;
  }
};

struct EventCameraFixture {
  Event_CameraState* saved_camera = Event_camera;
  decltype(State_tickSpeed) saved_speed = State_tickSpeed;
  Event_CameraState camera{};
  std::array<std::byte, sizeof(Event_CameraTrackScratch)> scratch{};
  std::array<u_char, sizeof Event_kOrbitReturnFirstCommand> saved_eye{};
  std::array<u_char, sizeof Event_kOrbitReturnSecondCommand> saved_look_at{};

  EventCameraFixture() {
    std::memcpy(saved_eye.data(), &Event_kOrbitReturnFirstCommand,
                saved_eye.size());
    std::memcpy(saved_look_at.data(), &Event_kOrbitReturnSecondCommand,
                saved_look_at.size());
    std::memcpy(scratch.data(),
                static_cast<std::byte*>(Core_ScratchBase()) + 0x88,
                scratch.size());
    Event_camera = &camera;
    State_tickSpeed = 2;
    camera.lookAt.currentPosition = {100, 200, 300, 0};
    camera.lookAt.destination = {400, 500, 600, 0};
    camera.lookAt.partnerPosition = {1000, 800, 900, 0};
    camera.eye.currentPosition = camera.lookAt.partnerPosition;
    camera.eye.partnerPosition = camera.lookAt.currentPosition;
    camera.eye.destination = {1200, 1000, 1100, 0};
  }

  ~EventCameraFixture() {
    std::memcpy(&Event_kOrbitReturnFirstCommand, saved_eye.data(),
                saved_eye.size());
    std::memcpy(&Event_kOrbitReturnSecondCommand, saved_look_at.data(),
                saved_look_at.size());
    std::memcpy(static_cast<std::byte*>(Core_ScratchBase()) + 0x88,
                scratch.data(), scratch.size());
    Event_camera = saved_camera;
    State_tickSpeed = saved_speed;
    Core_Gte_Reset();
  }

  static void LoadCommand(void* target, unsigned offset, unsigned size,
                          std::span<const u_char> image) {
    const auto* end = Content_moduleData + Content_moduleDataCount;
    const auto* entry = std::find_if(
        Content_moduleData, end,
        [target](const auto& item) { return item.target == target; });
    REQUIRE(entry != end);
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
    CHECK(camera.eye.motionMode == 4);
    CHECK(camera.lookAt.motionMode == 2);
    CHECK(camera.eye.durationFrames == 85);
    CHECK(camera.lookAt.durationFrames == 85);
    const std::array<u_char, 4> easing{
        camera.eye.easingPrimary, camera.eye.easingSecondary,
        camera.eye.easingTertiary, camera.lookAt.easingPrimary};
    CHECK(easing == std::array<u_char, 4>{1, 0, 1, 1});
  }

  void CheckFinished() const {
    CHECK(camera.eye.motionMode == 15);
    CHECK(camera.lookAt.motionMode == 15);
    CHECK(camera.eye.elapsedFrames == 85);
    CHECK(camera.lookAt.elapsedFrames == 85);
    const auto& current = camera.lookAt.currentPosition;
    const auto& destination = camera.lookAt.destination;
    const std::array current_position{current.vx, current.vy, current.vz};
    const std::array expected_position{destination.vx, destination.vy,
                                       destination.vz};
    CHECK(current_position == expected_position);
  }
};
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
  Event_CommandCameraMotion(
      reinterpret_cast<u_char*>(&Event_kOrbitReturnFirstCommand), 0);
  Event_CommandCameraMotion(
      reinterpret_cast<u_char*>(&Event_kOrbitReturnSecondCommand), 0);
  fixture.CheckStarted();
  for (int frame = 0; frame < 85; ++frame) {
    Event_UpdateEventCameraTrack(&fixture.camera.lookAt, &fixture.camera.eye);
    Event_UpdateEventCameraTrack(&fixture.camera.eye, &fixture.camera.lookAt);
  }
  fixture.CheckFinished();
}

TEST_CASE("Gameplay initialization cannot resume a previous Game Over event",
          "[event][session-reset]") {
  EventSessionFixture fixture;
  std::array<u_char, 16> old_script{};
  Event_playbackPhase = 4;
  Event_isStartConfirmed = 1;
  Event_isMainThreadFinished = 1;
  Event_roomChangeState = 3;
  std::fill(std::begin(Event_threadCursors), std::end(Event_threadCursors),
            old_script.data());
  std::fill(std::begin(Event_threadWaitFrames),
            std::end(Event_threadWaitFrames), 30);

  Event_InitializeScriptOpcodeTable();

  REQUIRE(Event_playbackPhase == 0);
  CHECK(Event_isStartConfirmed == 0);
  CHECK(Event_isMainThreadFinished == 0);
  CHECK(std::all_of(std::begin(Event_threadCursors),
                    std::end(Event_threadCursors),
                    [](const auto* cursor) { return cursor == nullptr; }));
  CHECK(std::all_of(std::begin(Event_threadWaitFrames),
                    std::end(Event_threadWaitFrames),
                    [](auto wait) { return wait == 0; }));
  CHECK(Event_UpdateEventPlayback() == 0);
}
