#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/CdDriveLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Content/ModuleData.h"
#include "Event/InterpolationCurveLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Event/TriggersLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "DiscQueue.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "State/Timers.h"
}

namespace {
namespace event = VS::Event;
namespace content = VS::Content;
using Bytes = std::vector<std::uint8_t>;
using Handler = std::intptr_t (*)(std::uint8_t*, std::int16_t);

struct LiveVmFixture {
  std::array<Content_DiscQueueSlot, 32> slots{};
  Content_DiscRequestCounts counts = Content_discRequestCounts;
  Content_DiscDrive drive = Content_discDriveState;
  std::array<Handler, 256> handlers{};
  std::array<Handler, 256> skip_handlers{};
  std::array<std::uint8_t, 256> sizes{};
  std::array<char, 64> loop_counters{};
  std::array<std::int16_t, 6> wait_frames{};
  std::array<std::uint16_t, 66> legacy_curves{};
  std::uintptr_t subroutine_return = Event_subroutineReturnCursor;
  char cached_flag = Event_cachedFlagValue;
  int tick_speed = State_tickSpeed;
  char phase = Event_playbackPhase;
  char confirmed = Event_isStartConfirmed;
  std::int16_t finished = Event_isMainThreadFinished;
  std::int16_t chained = Event_isChainedEventLoaded;
  char skip = Event_skipState;

  LiveVmFixture() {
    std::ranges::copy(Content_discRequestSlots, slots.begin());
    std::ranges::copy(Event_kOpcodeHandlers, handlers.begin());
    std::ranges::copy(Event_kSkipOpcodeHandlers, skip_handlers.begin());
    std::ranges::copy(Event_kOpcodeSizes, sizes.begin());
    std::ranges::copy(Event_loopCounters, loop_counters.begin());
    std::ranges::copy(Event_threadWaitFrames, wait_frames.begin());
    std::ranges::copy(Event_customEasingCurves, legacy_curves.begin());
    content::InitializeLiveDiscQueue();
    Content_discDriveState = {};
    Content_discDriveState.state = Content_DiscDriveState_Idle;
    event::ResetLiveScriptRuntime();
    Event_skipState = 0;
    std::ranges::fill(Event_kOpcodeSizes, 1);
  }
  ~LiveVmFixture() {
    event::ResetLiveScriptRuntime();
    std::ranges::copy(slots, Content_discRequestSlots);
    Content_discRequestCounts = counts;
    Content_discDriveState = drive;
    std::ranges::copy(handlers, Event_kOpcodeHandlers);
    std::ranges::copy(skip_handlers, Event_kSkipOpcodeHandlers);
    std::ranges::copy(sizes, Event_kOpcodeSizes);
    std::ranges::copy(loop_counters, Event_loopCounters);
    std::ranges::copy(wait_frames, Event_threadWaitFrames);
    std::ranges::copy(legacy_curves, Event_customEasingCurves);
    Event_subroutineReturnCursor = subroutine_return;
    Event_cachedFlagValue = cached_flag;
    State_tickSpeed = tick_speed;
    Event_playbackPhase = phase;
    Event_isStartConfirmed = confirmed;
    Event_isMainThreadFinished = finished;
    Event_isChainedEventLoaded = chained;
    Event_skipState = skip;
  }
  LiveVmFixture(const LiveVmFixture&) = delete;
  LiveVmFixture& operator=(const LiveVmFixture&) = delete;
  LiveVmFixture(LiveVmFixture&&) = delete;
  LiveVmFixture& operator=(LiveVmFixture&&) = delete;
};

struct FlagDifferentialFixture {
  std::array<std::uint8_t, VS::State::kScriptFlagCount> saved{};
  FlagDifferentialFixture() {
    for (std::size_t flag = 0; flag < saved.size(); ++flag) {
      saved[flag] = VS::State::LiveScriptFlag(static_cast<int>(flag));
    }
  }
  ~FlagDifferentialFixture() {
    for (std::size_t flag = 0; flag < saved.size(); ++flag) {
      VS::State::SetLiveScriptFlag(static_cast<int>(flag), saved[flag]);
    }
  }
};

struct FlagComparison {
  int first = 0;
  int second = 0;
  std::uint8_t left = 0;
  std::uint8_t right = 0;
  std::uint8_t operation = 0;
  bool pair = false;
};

std::array<std::uint8_t, 4> FlagCommand(const FlagComparison& sample) {
  return {
      0, static_cast<std::uint8_t>(sample.operation | (sample.first >> 6)),
      static_cast<std::uint8_t>(((sample.first & 63) << 2) |
                                (sample.pair ? sample.second >> 8 : 0)),
      sample.pair ? static_cast<std::uint8_t>(sample.second) : sample.right};
}

void InitializeFlagSample(const FlagComparison& sample) {
  VS::State::SetLiveScriptFlag(sample.first, sample.left);
  if (sample.pair) {
    VS::State::SetLiveScriptFlag(sample.second, sample.right);
  }
}

bool CompareOriginalFlagOperation(const FlagComparison& sample,
                                  int mutation = 0) {
  auto command = FlagCommand(sample);
  InitializeFlagSample(sample);
  const auto original =
      sample.pair ? Event_ScriptApplyFlagPairOperation(command.data(), 0)
                  : Event_ScriptApplyImmediateFlagOperation(command.data(), 0);
  const auto first = VS::State::LiveScriptFlag(sample.first);
  const auto second = VS::State::LiveScriptFlag(sample.second);
  InitializeFlagSample(sample);
  if (mutation == 1) command[1] ^= 0x10;
  if (mutation == 2) command[2] ^= 4;
  if (mutation == 3) ++command[3];
  const event::ScriptInstruction instruction{command, {}, 0};
  const auto result = sample.pair ? event::ScriptApplyFlagPairOperation(
                                        instruction, event::LiveScriptVm())
                                  : event::ScriptApplyImmediateFlagOperation(
                                        instruction, event::LiveScriptVm());
  return original == 0 && result.kind == event::ScriptFlowKind::kAdvance &&
         VS::State::LiveScriptFlag(sample.first) == first &&
         VS::State::LiveScriptFlag(sample.second) == second;
}

std::uint32_t NextFlagSample(std::uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

void CompareFlagCorpus(bool pair) {
  FlagDifferentialFixture fixture;
  std::uint32_t random = 0xF1A670;
  for (std::uint8_t operation = 0; operation < 16; ++operation) {
    for (int iteration = 0; iteration < 2048; ++iteration) {
      const int first = iteration & 1023;
      const int second = (iteration & 1) == 0 ? first : (iteration * 13) & 1023;
      const auto left = static_cast<std::uint8_t>(NextFlagSample(random));
      auto right = static_cast<std::uint8_t>(NextFlagSample(random));
      if ((operation == 3 || operation == 4) &&
          (right == 0 || (pair && first == second && left == 0))) {
        right = 1;
      }
      REQUIRE(CompareOriginalFlagOperation(
          {first, second, left, right,
           static_cast<std::uint8_t>(operation << 4), pair}));
    }
  }
}

bool SameBranchResult(std::intptr_t original, event::ScriptFlow native,
                      const std::uint8_t* instruction) {
  if (original == 0) {
    return native.kind == event::ScriptFlowKind::kAdvance;
  }
  return native.kind == event::ScriptFlowKind::kJumpBy &&
         original ==
             reinterpret_cast<std::intptr_t>(instruction) + native.displacement;
}

bool CompareOriginalFlagBranch(const FlagComparison& sample,
                               std::int16_t displacement, int mutation = 0) {
  std::array<std::uint8_t, 64> storage{};
  auto* command = storage.data() + 32;
  std::ranges::copy(FlagCommand(sample), command);
  command[4] = static_cast<std::uint8_t>(displacement);
  command[5] =
      static_cast<std::uint8_t>(static_cast<std::uint16_t>(displacement) >> 8);
  InitializeFlagSample(sample);
  const auto original = sample.pair
                            ? Event_ScriptBranchOnFlagPair(command, 0)
                            : Event_ScriptBranchOnFlagImmediate(command, 0);
  if (mutation == 1) command[1] ^= 0x10;
  if (mutation == 2) ++command[4];
  if (mutation == 3) command[3] ^= 0x80;
  const event::ScriptInstruction instruction{{command, 32}, {}, 0};
  const auto native = sample.pair ? event::ScriptBranchOnFlagPair(
                                        instruction, event::LiveScriptVm())
                                  : event::ScriptBranchOnFlagImmediate(
                                        instruction, event::LiveScriptVm());
  return SameBranchResult(original, native, command);
}

void CompareFlagBranchCorpus(bool pair) {
  FlagDifferentialFixture fixture;
  std::uint32_t random = 0xB8A10;
  for (std::uint8_t operation = 0; operation < 16; ++operation) {
    for (int iteration = 0; iteration < 2048; ++iteration) {
      const int first = iteration & 1023;
      const int second = (iteration & 1) == 0 ? first : (iteration * 13) & 1023;
      REQUIRE(CompareOriginalFlagBranch(
          {first, second, static_cast<std::uint8_t>(NextFlagSample(random)),
           static_cast<std::uint8_t>(NextFlagSample(random)),
           static_cast<std::uint8_t>(operation << 4), pair},
          static_cast<std::int16_t>((iteration & 63) - 32)));
    }
  }
}

void AddCachedDigest(std::uint64_t& digest, std::uint32_t value) {
  for (int byte = 0; byte < 4; ++byte) {
    digest ^= static_cast<std::uint8_t>(value >> (byte * 8));
    digest *= 0x100000001B3;
  }
}

bool CompareFrozenCachedBranch(
    std::uint8_t value, std::uint8_t operand,
    std::array<std::uint64_t, 2>* fingerprints = nullptr, int mutation = 0) {
  const int flag = value * 4 + (operand & 3);
  VS::State::SetLiveScriptFlag(flag, value);
  std::array<std::uint8_t, 3> cache{0, static_cast<std::uint8_t>(flag),
                                    static_cast<std::uint8_t>(flag >> 8)};
  const auto expected_cache = value;
  std::array<std::uint8_t, 64> storage{};
  auto* command = storage.data() + 32;
  const auto displacement = static_cast<std::int16_t>((operand & 63) - 32);
  command[1] = operand;
  command[2] = static_cast<std::uint8_t>(displacement);
  command[3] =
      static_cast<std::uint8_t>(static_cast<std::uint16_t>(displacement) >> 8);
  const bool expected_jump = value == operand;
  const auto expected_flow =
      expected_jump ? event::ScriptFlow::JumpBy(displacement)
                    : event::ScriptFlow::Of(event::ScriptFlowKind::kAdvance);
  const auto native_cache =
      event::ScriptCacheFlagValue({cache, {}, 0}, event::LiveScriptVm());
  if (mutation == 1) {
    event::LiveScriptVm().SetCachedFlag(static_cast<std::int8_t>(value ^ 0x80));
  }
  if (mutation == 2) command[1] ^= 0x80;
  if (mutation == 3) ++command[2];
  const auto native = event::ScriptBranchOnCachedFlag({{command, 32}, {}, 0},
                                                      event::LiveScriptVm());
  if (fingerprints != nullptr) {
    const auto expected_offset =
        static_cast<std::uint16_t>(expected_jump ? displacement : 0);
    const bool native_jump = native.kind == event::ScriptFlowKind::kJumpBy;
    const auto native_offset =
        static_cast<std::uint16_t>(native_jump ? native.displacement : 0);
    const auto native_value =
        static_cast<std::uint8_t>(event::LiveScriptVm().CachedFlag());
    AddCachedDigest((*fingerprints)[0],
                    expected_cache |
                        (static_cast<std::uint32_t>(expected_jump) << 8) |
                        (static_cast<std::uint32_t>(expected_offset) << 16));
    AddCachedDigest((*fingerprints)[1],
                    native_value |
                        (static_cast<std::uint32_t>(native_jump) << 8) |
                        (static_cast<std::uint32_t>(native_offset) << 16));
  }
  return native_cache.kind == event::ScriptFlowKind::kAdvance &&
         static_cast<std::uint8_t>(event::LiveScriptVm().CachedFlag()) ==
             expected_cache &&
         native == expected_flow;
}

bool CompareOriginalLoop(std::uint8_t counter, std::uint8_t value, bool setting,
                         int mutation = 0) {
  auto& host = event::LiveScriptVm();
  std::ranges::fill(Event_loopCounters, 0x55);
  for (int index = 0; index < event::kLoopCounterCount; ++index) {
    host.SetLoopCounter(index, 0x55);
  }
  if (!setting && counter != 0xFF) {
    Event_loopCounters[counter] = static_cast<char>(value);
    host.SetLoopCounter(counter, value);
  }
  std::array<std::uint8_t, 64> storage{};
  auto* command = storage.data() + 32;
  command[1] = counter;
  const auto displacement = static_cast<std::int16_t>((value & 63) - 31);
  command[2] = setting ? value : static_cast<std::uint8_t>(displacement);
  command[3] =
      static_cast<std::uint8_t>(static_cast<std::uint16_t>(displacement) >> 8);
  const auto original = setting ? Event_ScriptSetLoopCounter(command, 0)
                                : Event_ScriptLoopBackward(command, 0);
  std::array<std::uint8_t, event::kLoopCounterCount> expected{};
  for (int index = 0; index < event::kLoopCounterCount; ++index) {
    expected[static_cast<std::size_t>(index)] =
        static_cast<std::uint8_t>(Event_loopCounters[index]);
    host.SetLoopCounter(index, 0x55);
  }
  if (!setting && counter != 0xFF) {
    host.SetLoopCounter(counter, value);
  }
  if (mutation == 1) command[1] ^= 1;
  if (mutation == 2) ++command[2];
  if (mutation == 3 && counter != 0xFF) {
    host.SetLoopCounter(counter, static_cast<std::uint8_t>(value + 1));
  }
  const event::ScriptInstruction instruction{{command, 32}, {}, 0};
  const auto native = setting ? event::ScriptSetLoopCounter(instruction, host)
                              : event::ScriptLoopBackward(instruction, host);
  if (!SameBranchResult(original, native, command)) {
    return false;
  }
  for (int index = 0; index < event::kLoopCounterCount; ++index) {
    if (expected[static_cast<std::size_t>(index)] != host.LoopCounter(index)) {
      return false;
    }
  }
  return true;
}

void CompareLoopCorpus(bool setting) {
  const LiveVmFixture fixture;
  for (int index = 0; index <= event::kLoopCounterCount; ++index) {
    const auto counter = static_cast<std::uint8_t>(
        index == event::kLoopCounterCount ? 0xFF : index);
    for (int value = 0; value < 256; ++value) {
      CAPTURE(setting, counter, value);
      REQUIRE(CompareOriginalLoop(counter, static_cast<std::uint8_t>(value),
                                  setting));
    }
  }
}

struct LiveRoomFixture {
  LiveVmFixture vm;
  std::int16_t change = Event_roomChangeState;
  std::int16_t entry = Event_hasRoomEntryScript;
  std::array<std::uint8_t, 32> flags{};
  std::uint8_t loading = VS::State::LiveScriptFlag(0xA9);

  LiveRoomFixture() {
    for (std::size_t index = 0; index < flags.size(); ++index) {
      flags[index] = VS::State::LiveScriptFlag(32 + static_cast<int>(index));
    }
    Event_roomChangeState = 0;
    Event_hasRoomEntryScript = 0;
  }
  ~LiveRoomFixture() {
    Event_roomChangeState = change;
    Event_hasRoomEntryScript = entry;
    for (std::size_t index = 0; index < flags.size(); ++index) {
      VS::State::SetLiveScriptFlag(32 + static_cast<int>(index), flags[index]);
    }
    VS::State::SetLiveScriptFlag(0xA9, loading);
  }
  LiveRoomFixture(const LiveRoomFixture&) = delete;
  LiveRoomFixture& operator=(const LiveRoomFixture&) = delete;
  LiveRoomFixture(LiveRoomFixture&&) = delete;
  LiveRoomFixture& operator=(LiveRoomFixture&&) = delete;
};

event::ScriptLocation MountScript(
    Bytes bytes, std::weak_ptr<const Bytes>* lifetime = nullptr) {
  const auto owner = std::make_shared<const Bytes>(std::move(bytes));
  if (lifetime != nullptr) {
    *lifetime = owner;
  }
  REQUIRE(event::MountLiveScriptSource(event::ScriptRegion::kEventScript, owner)
              .has_value());
  const auto start =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript)
          .value()
          .At(0);
  if (!start) {
    throw std::logic_error("Mounted test script has no first instruction");
  }
  return *start;
}

Content_DiscQueueSlot& PendingRequest() {
  const auto found =
      std::ranges::find_if(Content_discRequestSlots, [](const auto& request) {
        return request.state == Content_DiscRequestState_Enqueued;
      });
  REQUIRE(found != std::end(Content_discRequestSlots));
  return *found;
}

void CompleteSyntheticFile(std::uint8_t first) {
  auto& request = PendingRequest();
  REQUIRE(request.cdFile.size == static_cast<int>(event::kEventScriptBytes));
  REQUIRE(request.vram != nullptr);
  auto bytes = std::span{static_cast<std::uint8_t*>(request.vram),
                         event::kEventScriptBytes};
  std::ranges::fill(bytes, first);
  request.state = Content_DiscRequestState_Loaded;
}

void CheckMountedByte(std::uint8_t value, std::uint64_t generation) {
  const auto source =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript).value();
  CHECK(source.Generation() == generation);
  REQUIRE_FALSE(source.Bytes().empty());
  CHECK(source.Bytes().front() == value);
}

std::array<event::ScriptLocation, 2> observed{};
int observations = 0;

event::ScriptFlow ObserveJump(const event::ScriptInstruction& instruction,
                              event::ScriptVmHost& host) {
  observed.at(static_cast<std::size_t>(observations++)) = instruction.location;
  return event::ScriptJumpForward(instruction, host);
}

event::ScriptFlow ObserveStop(const event::ScriptInstruction& instruction,
                              event::ScriptVmHost& host) {
  observed.at(static_cast<std::size_t>(observations++)) = instruction.location;
  return event::CommandStopScriptContext(instruction, host);
}

std::intptr_t JumpBridge(std::uint8_t* instruction, std::int16_t thread) {
  return event::DispatchLiveScriptControlOpcode(instruction, thread,
                                                ObserveJump);
}

std::intptr_t StopBridge(std::uint8_t* instruction, std::int16_t thread) {
  return event::DispatchLiveScriptControlOpcode(instruction, thread,
                                                ObserveStop);
}

std::intptr_t PointerResult(std::uint8_t*, std::int16_t) { return 0x12345678; }

std::intptr_t NoOperationSlotBridge(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return Event_ScriptNoOperationOpcode(instruction, thread);
}

std::intptr_t YieldSlotBridge(std::uint8_t* instruction, std::int16_t thread) {
  return Event_ScriptYieldFrameOpcode(instruction, thread);
}

std::intptr_t WaitSlotBridge(std::uint8_t* instruction, std::int16_t thread) {
  return Event_CommandWaitFramesOpcode(instruction, thread);
}

std::intptr_t TickSlotBridge(std::uint8_t* instruction, std::int16_t thread) {
  return Event_CommandSetTickSpeedOpcode(instruction, thread);
}

std::intptr_t StartContextSlotBridge(std::uint8_t* instruction,
                                     std::int16_t thread) {
  return Event_CommandStartScriptContextOpcode(instruction, thread);
}

std::intptr_t WaitContextSlotBridge(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return Event_CommandWaitScriptContextOpcode(instruction, thread);
}

std::intptr_t StopContextSlotBridge(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return Event_CommandStopScriptContextOpcode(instruction, thread);
}

std::intptr_t CacheSlotBridge(std::uint8_t* instruction, std::int16_t thread) {
  return Event_ScriptCacheFlagValueOpcode(instruction, thread);
}

std::intptr_t CachedBranchSlotBridge(std::uint8_t* instruction,
                                     std::int16_t thread) {
  return Event_ScriptBranchOnCachedFlagOpcode(instruction, thread);
}

event::ScriptLocation PrepareCachedScenario(int skip, bool guarded,
                                            std::uint8_t operand) {
  const auto start =
      MountScript({0xE7, 0xFF, 1, 0xE8, operand, 6, 0, 0xE3, 4, 0xF0});
  Event_skipState = static_cast<char>(skip);
  State_tickSpeed = 2;
  Event_kSkipOpcodeHandlers[0xE7] = Event_kOpcodeHandlers[0xE7] =
      guarded ? CacheSlotBridge : Event_ScriptCacheFlagValueOpcode;
  Event_kSkipOpcodeHandlers[0xE8] = Event_kOpcodeHandlers[0xE8] =
      guarded ? CachedBranchSlotBridge : Event_ScriptBranchOnCachedFlagOpcode;
  Event_kSkipOpcodeHandlers[0xE3] = Event_kOpcodeHandlers[0xE3] =
      Event_CommandSetTickSpeedOpcode;
  Event_kSkipOpcodeHandlers[0xF0] = Event_kOpcodeHandlers[0xF0] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE7] = 3;
  Event_kOpcodeSizes[0xE8] = 4;
  Event_kOpcodeSizes[0xE3] = 2;
  event::LiveScriptVm().SetThreadCursor(0, start);
  return start;
}

void CheckCachedOutcome(int value, bool equal) {
  CHECK(static_cast<std::uint8_t>(event::LiveScriptVm().CachedFlag()) == value);
  CHECK(State_tickSpeed == (equal ? 2 : 4));
}

event::ScriptLocation PrepareContextScenario(int skip, bool guarded) {
  const auto start =
      MountScript({0xE4, 3, 8, 0, 0xE2, 2, 0xE6, 0xE6, 0xE5, 3, 0xE3, 4, 0xE6});
  Event_skipState = static_cast<char>(skip);
  State_tickSpeed = 2;
  Event_kSkipOpcodeHandlers[0xE4] = Event_kOpcodeHandlers[0xE4] =
      guarded ? StartContextSlotBridge : Event_CommandStartScriptContextOpcode;
  Event_kSkipOpcodeHandlers[0xE5] = Event_kOpcodeHandlers[0xE5] =
      guarded ? WaitContextSlotBridge : Event_CommandWaitScriptContextOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      guarded ? StopContextSlotBridge : Event_CommandStopScriptContextOpcode;
  Event_kSkipOpcodeHandlers[0xE2] = Event_kOpcodeHandlers[0xE2] =
      Event_CommandWaitFramesOpcode;
  Event_kSkipOpcodeHandlers[0xE3] = Event_kOpcodeHandlers[0xE3] =
      Event_CommandSetTickSpeedOpcode;
  Event_kOpcodeSizes[0xE4] = 4;
  Event_kOpcodeSizes[0xE5] = Event_kOpcodeSizes[0xE2] =
      Event_kOpcodeSizes[0xE3] = 2;
  auto& host = event::LiveScriptVm();
  host.SetWaitFrames(3, 0);
  host.SetThreadCursor(0, start);
  return start;
}

event::ScriptLocation PrepareContextRelease(int operand, bool guarded) {
  const auto start =
      MountScript({0xE5, static_cast<std::uint8_t>(operand), 0xE6, 0xE6});
  Event_kOpcodeHandlers[0xE5] =
      guarded ? WaitContextSlotBridge : Event_CommandWaitScriptContextOpcode;
  Event_kOpcodeHandlers[0xE6] = Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE5] = 2;
  auto& host = event::LiveScriptVm();
  const event::ScriptLocation target{start.region, 3, start.generation};
  for (int thread = 0; thread < event::kScriptThreadCount; ++thread) {
    host.SetThreadCursor(thread, target);
  }
  host.SetThreadCursor(0, start);
  return start;
}

void CheckThreadCursorMatches(int thread, event::ScriptLocation expected) {
  CHECK(event::LiveScriptVm().ThreadCursor(thread) == expected);
}

struct WaitScenario {
  int skip;
  int thread;
  bool guarded;
  int duration;
};

void CheckWaitSnapshot(event::ScriptLocation start, int thread, int remaining,
                       bool waiting) {
  auto& host = event::LiveScriptVm();
  CHECK(host.WaitFrames(thread) == remaining);
  CHECK(host.ThreadCursor(thread) ==
        (waiting ? start : event::ScriptLocation{}));
  CHECK(State_tickSpeed == (waiting ? 2 : 4));
}

void CheckWaitDispatch(int thread) {
  CHECK_FALSE(event::ExecuteLiveScriptThread(thread));
}

void CheckUnreleasedWait(event::ScriptLocation start, int thread) {
  auto& host = event::LiveScriptVm();
  CHECK_FALSE(event::ExecuteLiveScriptThread(thread));
  CHECK(host.WaitFrames(thread) == 0);
  CHECK(host.ThreadCursor(thread) == start);
}

void CheckUnchangedTickSpeed() { CHECK(State_tickSpeed == 2); }

void CheckWaitScenario(WaitScenario scenario) {
  const LiveVmFixture fixture;
  const auto start = MountScript(
      {0xE2, static_cast<std::uint8_t>(scenario.duration), 0xE3, 4, 0xF0});
  Event_skipState = static_cast<char>(scenario.skip);
  State_tickSpeed = 2;
  Event_kSkipOpcodeHandlers[0xE2] = Event_kOpcodeHandlers[0xE2] =
      scenario.guarded ? WaitSlotBridge : Event_CommandWaitFramesOpcode;
  Event_kSkipOpcodeHandlers[0xE3] = Event_kOpcodeHandlers[0xE3] =
      scenario.guarded ? TickSlotBridge : Event_CommandSetTickSpeedOpcode;
  Event_kSkipOpcodeHandlers[0xF0] = Event_kOpcodeHandlers[0xF0] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE2] = Event_kOpcodeSizes[0xE3] = 2;
  auto& host = event::LiveScriptVm();
  host.SetWaitFrames(scenario.thread, 0);
  host.SetThreadCursor(scenario.thread, start);
  CheckWaitDispatch(scenario.thread);
  CheckWaitSnapshot(start, scenario.thread, scenario.duration, true);
  if (scenario.duration == 0) {
    for (int frame = 0; frame < 3; ++frame) {
      CheckUnreleasedWait(start, scenario.thread);
    }
    CheckUnchangedTickSpeed();
    return;
  }
  for (int remaining = scenario.duration - 1; remaining >= 0; --remaining) {
    CheckWaitDispatch(scenario.thread);
    CheckWaitSnapshot(start, scenario.thread, remaining, remaining != 0);
  }
}

void BindTestData(event::ScriptLocation start) {
  auto& host = event::LiveScriptVm();
  host.SetDialogStrings(start);
  ++start.offset;
  host.SetTextBoxFrameStyle(9, start);
  ++start.offset;
  host.SetVibrationPattern(7, start);
}

void CheckDataBytes(const event::ScriptDataLease& lease, std::uint8_t first,
                    std::size_t length) {
  const auto bytes = lease.Bytes();
  if (!bytes || bytes->empty()) {
    FAIL("Test data lease has no bytes");
    return;
  }
  CHECK(bytes->size() == length);
  CHECK(bytes->front() == first);
}

void CheckDataError(
    const std::expected<event::ScriptDataLease, event::ScriptSourceError>& data,
    event::ScriptSourceError error) {
  REQUIRE_FALSE(data.has_value());
  if (!data) {
    CHECK(data.error() == error);
  }
}

bool nested_finished = false;
std::intptr_t NestedJumpBridge(std::uint8_t* instruction, std::int16_t thread) {
  nested_finished = !event::ExecuteLiveScriptThread(1);
  return event::DispatchLiveScriptControlOpcode(instruction, thread,
                                                event::ScriptJumpForward);
}

void MarkRoomStopPending() {
  auto& host = event::LiveScriptVm();
  host.SetRoomChangeState(3);
  host.SetPhase(event::PlaybackPhase::kRunning);
  host.SetStartConfirmed(true);
  VS::State::SetLiveScriptFlag(32, 0x55);
  VS::State::SetLiveScriptFlag(0xA9, 0x66);
}

void CheckRoomEntryState(event::PlaybackPhase phase, std::int16_t change,
                         bool confirmed) {
  auto& host = event::LiveScriptVm();
  CHECK(host.RoomChangeState() == change);
  CHECK(host.Phase() == phase);
  CHECK(host.StartConfirmed() == confirmed);
  CHECK(Event_hasRoomEntryScript == 1);
}

void CheckContinuingRoomSources(std::uint64_t main_generation,
                                std::uint64_t door_generation) {
  const auto room = event::LiveRoomScriptSections();
  REQUIRE(room);
  if (room) {
    CHECK(room->EntryScript().empty());
  }
  CheckMountedByte(0x11, main_generation);
  CHECK(event::LeaseLiveScriptSource(event::ScriptRegion::kDoorEvents)
            .value()
            .Generation() == door_generation);
}

struct MountedRoomSources {
  std::weak_ptr<const event::RoomEventSections> room;
  std::uint64_t main_generation;
  std::uint64_t door_generation;
};

MountedRoomSources ImportTestRoomSources() {
  Bytes entry{0x11};
  const Bytes doors{0x22};
  VS::State::SetLiveScriptFlag(32, 0x7F);
  REQUIRE(event::ImportLiveRoomScripts(
              {.entry_script = entry, .doors = doors, .first_entry = true}) ==
          event::RoomEventLoadResult::kReady);
  CHECK(VS::State::LiveScriptFlag(32) == 0x7F);
  const auto main_generation =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript)
          .value()
          .Generation();
  entry.front() = 0x99;
  CheckMountedByte(0x11, main_generation);
  REQUIRE(event::LiveScriptVm().BindRoomDoorSource());
  const auto door_generation =
      event::LeaseLiveScriptSource(event::ScriptRegion::kDoorEvents)
          .value()
          .Generation();
  return {event::LiveRoomScriptSections(), main_generation, door_generation};
}
}  // namespace

TEST_CASE("Production room event boundary publishes exact owned script sources",
          "[event-vm-live][room-triggers]") {
  LiveRoomFixture fixture;
  Bytes entry{0x55, 0x66};
  const Bytes triggers{0xFF, 0xFF};
  const Bytes doors{0x22, 0x33};
  const auto loaded = event::LoadLiveRoomEventSections({.triggers = triggers,
                                                        .entry_script = entry,
                                                        .doors = doors,
                                                        .first_entry = true});
  REQUIRE(loaded);
  CHECK(*loaded == event::RoomEventLoadResult::kReady);
  const auto source =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript);
  REQUIRE(source);
  CHECK(source->Bytes().size() == entry.size());
  entry[0] = 0;
  CHECK(source->Bytes().front() == 0x55);
  const auto room = event::LiveRoomScriptSections();
  REQUIRE(room);
  CHECK(std::ranges::equal(room->Doors(), doors));
  event::FreeLiveRoomScripts();
  CHECK(source->Bytes().front() == 0x55);
}

TEST_CASE("Native immediate flag arithmetic matches actual safe original cases",
          "[event-vm-live][event-differential]") {
  CompareFlagCorpus(false);
}

TEST_CASE("Native flag pair arithmetic matches actual safe original cases",
          "[event-vm-live][event-differential]") {
  CompareFlagCorpus(true);
}

TEST_CASE(
    "Original flag differential detects planted operator target and operand "
    "mutants",
    "[event-vm-live][event-differential]") {
  FlagDifferentialFixture fixture;
  const FlagComparison sample{32, 33, 5, 3, 0, false};
  REQUIRE(CompareOriginalFlagOperation(sample));
  for (int mutation = 1; mutation <= 3; ++mutation) {
    CHECK_FALSE(CompareOriginalFlagOperation(sample, mutation));
  }
}

TEST_CASE("Flag branches compare high bytes as unsigned like the original ASM",
          "[event-vm-live][event-branch-differential]") {
  FlagDifferentialFixture fixture;
  VS::State::SetLiveScriptFlag(32, 0x80);
  VS::State::SetLiveScriptFlag(33, 0x7F);
  const std::array<std::uint8_t, 8> immediate{0, 0, 0x80, 0x7F, 6, 0};
  const std::array<std::uint8_t, 8> pair{0, 0, 0x80, 33, 6, 0};
  const auto expected = event::ScriptFlow::JumpBy(6);
  CHECK(event::ScriptBranchOnFlagImmediate({immediate, {}, 0},
                                           event::LiveScriptVm()) == expected);
  CHECK(event::ScriptBranchOnFlagPair({pair, {}, 0}, event::LiveScriptVm()) ==
        expected);
}

TEST_CASE("Native immediate flag branches match actual bounded original cases",
          "[event-vm-live][event-branch-differential]") {
  CompareFlagBranchCorpus(false);
}

TEST_CASE("Native flag pair branches match actual bounded original cases",
          "[event-vm-live][event-branch-differential]") {
  CompareFlagBranchCorpus(true);
}

TEST_CASE("Native cached flag branches match all original byte pairs",
          "[event-vm-live][event-branch-differential]") {
  const LiveVmFixture vm;
  FlagDifferentialFixture fixture;
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
  for (int value = 0; value < 256; ++value) {
    for (int operand = 0; operand < 256; ++operand) {
      REQUIRE(CompareFrozenCachedBranch(static_cast<std::uint8_t>(value),
                                        static_cast<std::uint8_t>(operand),
                                        &fingerprints));
    }
  }
  CHECK(fingerprints[0] == fingerprints[1]);
  CHECK(fingerprints[0] == 0xA21BEE19C943E325);
}

TEST_CASE(
    "Original branch differential detects comparison target and operand "
    "mutants",
    "[event-vm-live][event-branch-differential]") {
  FlagDifferentialFixture fixture;
  const FlagComparison sample{32, 33, 0x80, 0x7F, 0, false};
  REQUIRE(CompareOriginalFlagBranch(sample, 6));
  for (int mutation = 1; mutation <= 3; ++mutation) {
    CHECK_FALSE(CompareOriginalFlagBranch(sample, 6, mutation));
  }
}

TEST_CASE("Native loop counter stores match all safe original byte values",
          "[event-vm-live][event-loop-differential]") {
  CompareLoopCorpus(true);
}

TEST_CASE("Native loop back matches original byte wraps and signed targets",
          "[event-vm-live][event-loop-differential]") {
  CompareLoopCorpus(false);
}

TEST_CASE("Original loop differential detects counter value and target mutants",
          "[event-vm-live][event-loop-differential]") {
  const LiveVmFixture fixture;
  REQUIRE(CompareOriginalLoop(3, 7, true));
  for (int mutation = 1; mutation <= 2; ++mutation) {
    CHECK_FALSE(CompareOriginalLoop(3, 7, true, mutation));
  }
  REQUIRE(CompareOriginalLoop(3, 7, false));
  for (int mutation = 1; mutation <= 3; ++mutation) {
    CHECK_FALSE(CompareOriginalLoop(3, 7, false, mutation));
  }
}

TEST_CASE("Native control opcode entries resolve to owned table bindings",
          "[event-vm-live][content][module][control-opcode]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800B66E4) ==
      reinterpret_cast<void*>(&Event_ScriptNoOperationOpcode));
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800B66EC) ==
      reinterpret_cast<void*>(&Event_ScriptYieldFrameOpcode));
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800BB474) ==
      reinterpret_cast<void*>(&Event_CommandWaitFramesOpcode));
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800BB4CC) ==
      reinterpret_cast<void*>(&Event_CommandSetTickSpeedOpcode));
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800BB504) ==
      reinterpret_cast<void*>(&Event_CommandWaitScriptContextOpcode));
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800BB554) ==
      reinterpret_cast<void*>(&Event_CommandStartScriptContextOpcode));
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800BB5A4) ==
      reinterpret_cast<void*>(&Event_CommandStopScriptContextOpcode));
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800B8BC4) ==
      reinterpret_cast<void*>(&Event_ScriptCacheFlagValueOpcode));
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800B8BF8) ==
      reinterpret_cast<void*>(&Event_ScriptBranchOnCachedFlagOpcode));
}

TEST_CASE("Native control bindings preserve normal and skip yield behavior",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      const LiveVmFixture fixture;
      const auto start = MountScript({0xE0, 0xE1, 0xF0});
      Event_skipState = static_cast<char>(skip);
      Event_kSkipOpcodeHandlers[0xE0] = Event_kOpcodeHandlers[0xE0] =
          guarded ? NoOperationSlotBridge : Event_ScriptNoOperationOpcode;
      Event_kSkipOpcodeHandlers[0xE1] = Event_kOpcodeHandlers[0xE1] =
          guarded ? YieldSlotBridge : Event_ScriptYieldFrameOpcode;
      Event_kSkipOpcodeHandlers[0xF0] = Event_kOpcodeHandlers[0xF0] =
          Event_CommandStopScriptContextOpcode;
      auto& host = event::LiveScriptVm();
      host.SetThreadCursor(0, start);
      CHECK_FALSE(event::ExecuteLiveScriptThread(0));
      CHECK(host.ThreadCursor(0) ==
            event::ScriptLocation{start.region, 2, start.generation});
      CHECK_FALSE(event::ExecuteLiveScriptThread(0));
      CHECK_FALSE(host.ThreadCursor(0).Present());
    }
  }
}

TEST_CASE("Native control opcode bindings refuse detached command borrows",
          "[event-vm-live][control-opcode]") {
  const LiveVmFixture fixture;
  std::array<std::uint8_t, 1> command{};
  const auto expected =
      static_cast<std::intptr_t>(event::ScriptFlowKind::kStop);
  CHECK(Event_ScriptNoOperationOpcode(command.data(), 0) == expected);
  CHECK(Event_ScriptYieldFrameOpcode(command.data(), 0) == expected);
  CHECK(Event_CommandWaitFramesOpcode(command.data(), 0) == expected);
  CHECK(Event_CommandSetTickSpeedOpcode(command.data(), 0) == expected);
  CHECK(Event_CommandWaitScriptContextOpcode(command.data(), 0) == expected);
  CHECK(Event_CommandStartScriptContextOpcode(command.data(), 0) == expected);
  CHECK(Event_CommandStopScriptContextOpcode(command.data(), 0) == expected);
  CHECK(Event_ScriptCacheFlagValueOpcode(command.data(), 0) == expected);
  CHECK(Event_ScriptBranchOnCachedFlagOpcode(command.data(), 0) == expected);
}

TEST_CASE("Native wait bindings preserve countdown and tick resume state",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const int thread : {0, 3}) {
      for (const bool guarded : {false, true}) {
        for (const int duration : {0, 1, 2, 255}) {
          CheckWaitScenario({skip, thread, guarded, duration});
        }
      }
    }
  }
}

TEST_CASE("Native tick binding preserves the existing byte selector policy",
          "[event-vm-live][control-opcode]") {
  for (const bool guarded : {false, true}) {
    for (int operand = 0; operand < 256; ++operand) {
      const LiveVmFixture fixture;
      const auto start =
          MountScript({0xE3, static_cast<std::uint8_t>(operand), 0xF0});
      State_tickSpeed = 2;
      Event_kOpcodeHandlers[0xE3] =
          guarded ? TickSlotBridge : Event_CommandSetTickSpeedOpcode;
      Event_kOpcodeHandlers[0xF0] = Event_CommandStopScriptContextOpcode;
      Event_kOpcodeSizes[0xE3] = 2;
      auto& host = event::LiveScriptVm();
      host.SetThreadCursor(0, start);
      CHECK_FALSE(event::ExecuteLiveScriptThread(0));
      CHECK_FALSE(host.ThreadCursor(0).Present());
      CHECK(State_tickSpeed == (operand == 4 ? 4 : 2));
    }
  }
}

TEST_CASE("Native context bindings preserve embedded child scheduling",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      const LiveVmFixture fixture;
      const auto start = PrepareContextScenario(skip, guarded);
      const event::ScriptLocation caller{start.region, 8, start.generation};
      const event::ScriptLocation child{start.region, 4, start.generation};
      CheckWaitDispatch(0);
      CheckThreadCursorMatches(0, caller);
      CheckThreadCursorMatches(3, child);
      for (const int remaining : {2, 1, 0}) {
        CheckWaitDispatch(3);
        CheckWaitDispatch(0);
        CheckWaitSnapshot(child, 3, remaining, remaining != 0);
        CheckThreadCursorMatches(
            0, remaining != 0 ? caller : event::ScriptLocation{});
      }
    }
  }
}

TEST_CASE("Native context wait bindings preserve all encoded release selectors",
          "[event-vm-live][control-opcode]") {
  for (const bool guarded : {false, true}) {
    for (int operand = 0; operand < 256; ++operand) {
      const LiveVmFixture fixture;
      const auto start = PrepareContextRelease(operand, guarded);
      const event::ScriptLocation target{start.region, 3, start.generation};
      CheckWaitDispatch(0);
      const bool released = (operand & 0x80) != 0;
      CheckThreadCursorMatches(0, released ? event::ScriptLocation{} : start);
      for (int thread = 1; thread < event::kScriptThreadCount; ++thread) {
        const bool cleared = released && thread == (operand & 3);
        CheckThreadCursorMatches(thread,
                                 cleared ? event::ScriptLocation{} : target);
      }
    }
  }
}

TEST_CASE("Native cached bindings preserve byte boundaries and branch targets",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (const int value : {0, 127, 128, 255}) {
        for (const bool equal : {false, true}) {
          const LiveVmFixture fixture;
          FlagDifferentialFixture flags;
          const auto operand =
              static_cast<std::uint8_t>(equal ? value : value ^ 1);
          PrepareCachedScenario(skip, guarded, operand);
          VS::State::SetLiveScriptFlag(511, static_cast<std::uint8_t>(value));
          CheckWaitDispatch(0);
          CheckThreadCursorMatches(0, {});
          CheckCachedOutcome(value, equal);
        }
      }
    }
  }
}

TEST_CASE("Frozen cached oracle rejects cache byte operand and target mutants",
          "[event-vm-live][event-branch-differential]") {
  const LiveVmFixture fixture;
  FlagDifferentialFixture flags;
  REQUIRE(CompareFrozenCachedBranch(128, 128));
  for (int mutation = 1; mutation <= 3; ++mutation) {
    CHECK_FALSE(CompareFrozenCachedBranch(128, 128, nullptr, mutation));
  }
}

TEST_CASE("Live VM dispatch carries typed jumps without pointer results",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x42, 3, 0, 0xF0});
  observations = 0;
  Event_kOpcodeHandlers[0x42] = JumpBridge;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0x42] = 3;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(observations == 2);
  CHECK(observed[0] == start);
  CHECK(observed[1] ==
        event::ScriptLocation{start.region, 3, start.generation});
  CHECK_FALSE(event::LiveScriptVm().ThreadCursor(0).Present());
}

TEST_CASE("Live VM routes the original control handler through typed dispatch",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x42, 3, 0, 0xF0});
  observations = 0;
  Event_kOpcodeHandlers[0x42] = Event_ScriptJumpForward;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0x42] = 3;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  REQUIRE(observations == 1);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region, 3, start.generation});
}

TEST_CASE("Live VM selects typed control handlers from the skip table",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x42, 3, 0, 0xF0});
  observations = 0;
  Event_skipState = 2;
  Event_kOpcodeHandlers[0x42] = PointerResult;
  Event_kSkipOpcodeHandlers[0x42] = Event_ScriptJumpForward;
  Event_kSkipOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0x42] = 3;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  REQUIRE(observations == 1);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region, 3, start.generation});
}

TEST_CASE("Live VM original loop handler obeys the bounded counter contract",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE1, 20, 7, 0xF0});
  observations = 0;
  Event_loopCounters[20] = 0x55;
  Event_kOpcodeHandlers[0xE1] = Event_ScriptSetLoopCounter;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0xE1] = 3;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(Event_loopCounters[20] == 0x55);
  CHECK(observations == 1);
}

TEST_CASE("Live VM original return handler uses the owned native location",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE1, 0xF3, 0xF0});
  observations = 0;
  Event_subroutineReturnCursor = 0;
  Event_kOpcodeHandlers[0xE1] = Event_ScriptReturnToSavedAddress;
  Event_kOpcodeHandlers[0xF3] = StopBridge;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  auto& host = event::LiveScriptVm();
  host.SetSubroutineReturn({start.region, 2, start.generation});
  host.SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  REQUIRE(observations == 1);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region, 2, start.generation});
}

TEST_CASE("Legacy easing entry reads the native owned custom curve",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  Event_customEasingCurves[0] = 0;
  event::LiveScriptVm().SetCustomEasingCurve(0, {0, 1});
  CHECK(Event_EvaluateInterpolationCurve(4, 1, 2) == 1024);
}

TEST_CASE("Live VM refuses pointer results and detached control adapters",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  std::array<std::uint8_t, 3> detached{0x42, 0, 0};
  CHECK(JumpBridge(detached.data(), 0) == 3);
  const auto start = MountScript({0x42, 0, 0});
  Event_kOpcodeHandlers[0x42] = PointerResult;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(event::LiveScriptVm().ThreadCursor(0).Present());
}

TEST_CASE("Live VM file publication waits for completed owned staging",
          "[event-vm-live][disc-lifetime]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x55});
  auto& host = event::LiveScriptVm();
  REQUIRE(host.QueueEventFileLoad(0, event::EventBuffer::kScript));
  CHECK(host.EventFile() == event::EventFileStatus::kLoading);
  CheckMountedByte(0x55, start.generation);
  CompleteSyntheticFile(0xAA);
  host.ReleaseEventFile();
  CHECK(host.EventFile() == event::EventFileStatus::kNone);
  CheckMountedByte(0xAA, start.generation + 1);
}

TEST_CASE("Live VM cancellation stops deferred writes and releases its slot",
          "[event-vm-live][disc-lifetime]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x55});
  auto& host = event::LiveScriptVm();
  REQUIRE(host.QueueEventFileLoad(0, event::EventBuffer::kScript));
  auto& request = PendingRequest();
  REQUIRE(content::BeginLiveDiscRead(-1, 4, request.vram));
  host.CancelEventFile();
  CHECK(host.EventFile() == event::EventFileStatus::kNone);
  CHECK(request.state == Content_DiscRequestState_Free);
  CHECK(Content_discDriveState.destination == nullptr);
  content::UpdateLiveDiscCommands();
  CHECK(content::LiveDiscState() == Content_DiscDriveState_Idle);
  CheckMountedByte(0x55, start.generation);
}

TEST_CASE("Live VM cancellation cannot release a reused terminal queue slot",
          "[event-vm-live][disc-lifetime]") {
  const LiveVmFixture fixture;
  auto& host = event::LiveScriptVm();
  REQUIRE(host.QueueEventFileLoad(0, event::EventBuffer::kScript));
  auto& original = PendingRequest();
  const auto file = original.cdFile;
  content::ReleaseLiveDiscRequest(&original);
  auto* replacement = content::AllocateLiveDiscRequest(&file);
  REQUIRE(replacement == &original);
  std::array<std::byte, event::kEventScriptBytes> other{};
  content::EnqueueLiveDiscRequest(replacement, other.data());
  REQUIRE(content::CancelLiveDiscWrites(other));
  CHECK(host.EventFile() == event::EventFileStatus::kFailed);
  host.CancelEventFile();
  CHECK(replacement->state == Content_DiscRequestState_Error);
  content::ReleaseLiveDiscRequest(replacement);
}

TEST_CASE("Live VM chained storage publishes only after completed adoption",
          "[event-vm-live][disc-lifetime]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x55});
  auto& host = event::LiveScriptVm();
  REQUIRE(host.AllocateChainedEventBuffer());
  REQUIRE(host.QueueEventFileLoad(1, event::EventBuffer::kChained));
  CHECK_FALSE(host.AdoptChainedEvent());
  CompleteSyntheticFile(0x77);
  host.ReleaseEventFile();
  CheckMountedByte(0x55, start.generation);
  REQUIRE(host.AdoptChainedEvent());
  CheckMountedByte(0x77, start.generation + 1);
}

TEST_CASE("Live VM data leases retain exact bytes across rebinding and reset",
          "[event-vm-live][script-sources]") {
  const LiveVmFixture fixture;
  std::weak_ptr<const Bytes> old_lifetime;
  BindTestData(MountScript({0x11, 0x22, 0x33}, &old_lifetime));
  std::array leases{event::LeaseLiveDialogStrings().value(),
                    event::LeaseLiveTextBoxFrame(9).value(),
                    event::LeaseLiveVibrationPattern(7).value()};
  std::weak_ptr<const Bytes> new_lifetime;
  BindTestData(MountScript({0x44, 0x55, 0x66}, &new_lifetime));
  CheckDataBytes(event::LeaseLiveDialogStrings().value(), 0x44, 3);
  CheckDataBytes(event::LeaseLiveTextBoxFrame(9).value(), 0x55, 2);
  CheckDataBytes(event::LeaseLiveVibrationPattern(7).value(), 0x66, 1);
  event::ResetLiveScriptRuntime();
  CHECK(new_lifetime.expired());
  CHECK_FALSE(old_lifetime.expired());
  CheckDataBytes(leases[0], 0x11, 3);
  CheckDataBytes(leases[1], 0x22, 2);
  CheckDataBytes(leases[2], 0x33, 1);
  leases = {};
  CHECK(old_lifetime.expired());
}

TEST_CASE("Live VM data access rejects absent slots and invalid indices",
          "[event-vm-live][script-sources]") {
  const LiveVmFixture fixture;
  CheckDataError(event::LeaseLiveDialogStrings(),
                 event::ScriptSourceError::kMissingOwner);
  CheckDataError(event::LeaseLiveTextBoxFrame(9),
                 event::ScriptSourceError::kMissingOwner);
  CheckDataError(event::LeaseLiveVibrationPattern(7),
                 event::ScriptSourceError::kMissingOwner);
  for (const int style : {-1, event::kTextBoxFrameStyleCount}) {
    CheckDataError(event::LeaseLiveTextBoxFrame(style),
                   event::ScriptSourceError::kExtent);
  }
  for (const int pattern : {-1, event::kVibrationPatternSlots}) {
    CheckDataError(event::LeaseLiveVibrationPattern(pattern),
                   event::ScriptSourceError::kExtent);
  }
}

TEST_CASE("Live VM data bindings reject stale and out of range locations",
          "[event-vm-live][script-sources]") {
  const LiveVmFixture fixture;
  auto location = MountScript({0x11, 0x22, 0x33});
  BindTestData(location);
  auto lease = event::LeaseLiveDialogStrings().value();
  ++location.generation;
  event::LiveScriptVm().SetDialogStrings(location);
  CheckDataError(event::LeaseLiveDialogStrings(),
                 event::ScriptSourceError::kMissingOwner);
  CheckDataBytes(lease, 0x11, 3);
  lease.location = location;
  CHECK_FALSE(lease.Bytes().has_value());
  --location.generation;
  location.offset = 3;
  event::LiveScriptVm().SetTextBoxFrameStyle(9, location);
  CheckDataError(event::LeaseLiveTextBoxFrame(9),
                 event::ScriptSourceError::kMissingOwner);
  lease.location = location;
  CHECK_FALSE(lease.Bytes().has_value());
  event::LiveScriptVm().SetVibrationPattern(7, {});
  CheckDataError(event::LeaseLiveVibrationPattern(7),
                 event::ScriptSourceError::kMissingOwner);
}

TEST_CASE("Live VM restores the outer typed invocation after nested dispatch",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x42, 3, 0, 0xF0, 0xF0});
  observations = 0;
  nested_finished = false;
  Event_kOpcodeHandlers[0x42] = NestedJumpBridge;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0x42] = 3;
  auto& host = event::LiveScriptVm();
  host.SetThreadCursor(0, start);
  host.SetThreadCursor(1, {start.region, 4, start.generation});
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(nested_finished);
  CHECK(observations == 2);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region, 4, start.generation});
  CHECK(observed[1] ==
        event::ScriptLocation{start.region, 3, start.generation});
  CHECK_FALSE(host.ThreadCursor(0).Present());
  CHECK_FALSE(host.ThreadCursor(1).Present());
}

TEST_CASE("Live room import owns entry data and preserves ongoing room scripts",
          "[event-vm-live][room-triggers][script-sources]") {
  const LiveRoomFixture fixture;
  const auto mounted = ImportTestRoomSources();
  Event_roomChangeState = 1;
  const Bytes ignored_entry(event::kEventScriptBytes + 1, 0x55);
  const Bytes next_doors{0x33};
  REQUIRE(event::ImportLiveRoomScripts(
              {.entry_script = ignored_entry, .doors = next_doors}) ==
          event::RoomEventLoadResult::kContinuingEvent);
  CHECK(VS::State::LiveScriptFlag(32) == 0);
  CheckRoomEntryState(event::PlaybackPhase::kIdle, 2, false);
  const std::weak_ptr new_room = event::LiveRoomScriptSections();
  CheckContinuingRoomSources(mounted.main_generation, mounted.door_generation);
  event::FreeLiveRoomScripts();
  CHECK_FALSE(event::LiveRoomScriptSections());
  CHECK(new_room.expired());
  CHECK_FALSE(mounted.room.expired());
  event::ResetLiveScriptRuntime();
  CHECK(mounted.room.expired());
}

TEST_CASE("Live room import rejects invalid entry before changing state",
          "[event-vm-live][room-triggers][script-sources]") {
  const LiveRoomFixture fixture;
  const Bytes entry{0x11};
  REQUIRE(event::ImportLiveRoomScripts({.entry_script = entry}) ==
          event::RoomEventLoadResult::kReady);
  const auto room = event::LiveRoomScriptSections();
  const auto generation =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript)
          .value()
          .Generation();
  MarkRoomStopPending();
  const Bytes oversized(event::kEventScriptBytes + 1, 0);
  const auto refused =
      event::ImportLiveRoomScripts({.entry_script = oversized});
  REQUIRE_FALSE(refused.has_value());
  if (!refused) {
    CHECK(refused.error().reason ==
          event::RoomEventRefusal::kEntryScriptExtent);
  }
  CHECK(event::LiveRoomScriptSections() == room);
  CheckMountedByte(0x11, generation);
  CheckRoomEntryState(event::PlaybackPhase::kRunning, 3, true);
  CHECK(VS::State::LiveScriptFlag(32) == 0x55);
  CHECK(VS::State::LiveScriptFlag(0xA9) == 0x66);
}

TEST_CASE("Live room import commits stop transitions and accepts empty rooms",
          "[event-vm-live][room-triggers][script-sources]") {
  const LiveRoomFixture fixture;
  std::weak_ptr<const Bytes> previous;
  const auto old_source = MountScript({0x11}, &previous);
  MarkRoomStopPending();
  const Bytes entry{0x22};
  REQUIRE(event::ImportLiveRoomScripts({.entry_script = entry}) ==
          event::RoomEventLoadResult::kReady);
  CHECK(previous.expired());
  CheckMountedByte(0x22, old_source.generation + 1);
  CheckRoomEntryState(event::PlaybackPhase::kIdle, 0, false);
  CHECK(VS::State::LiveScriptFlag(32) == 0);
  CHECK(VS::State::LiveScriptFlag(0xA9) == 0);
  REQUIRE(event::ImportLiveRoomScripts({}) ==
          event::RoomEventLoadResult::kReady);
  CHECK_FALSE(event::LiveRoomScriptSections());
  CHECK(Event_hasRoomEntryScript == 0);
  CheckMountedByte(0x22, old_source.generation + 1);
}
