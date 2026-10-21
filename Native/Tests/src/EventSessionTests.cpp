#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Event/Script.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptEffects.h"
#include "Event/ScriptVm.h"
}

namespace {
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
}  // namespace

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
