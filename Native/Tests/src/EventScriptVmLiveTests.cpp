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
#include "Input/ControllerLiveState.h"
#include "Render/UiPacketsLive.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Battle/Hud.h"
#include "DiscQueue.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/SceneTimer.h"
#include "State/Timers.h"
}

namespace {
namespace event = VS::Event;
namespace content = VS::Content;
using Bytes = std::vector<std::uint8_t>;
using Handler = std::intptr_t (*)(std::uint8_t*, std::int16_t);

struct ControlEntry {
  std::uint32_t address;
  Handler handler;
};

constexpr auto kControlEntries = std::to_array<ControlEntry>({
    {0x800B66E4, Event_ScriptNoOperationOpcode},
    {0x800B66EC, Event_ScriptYieldFrameOpcode},
    {0x800BB474, Event_CommandWaitFramesOpcode},
    {0x800BB4CC, Event_CommandSetTickSpeedOpcode},
    {0x800BB504, Event_CommandWaitScriptContextOpcode},
    {0x800BB554, Event_CommandStartScriptContextOpcode},
    {0x800BB5A4, Event_CommandStopScriptContextOpcode},
    {0x800B8BC4, Event_ScriptCacheFlagValueOpcode},
    {0x800B8BF8, Event_ScriptBranchOnCachedFlagOpcode},
    {0x800B8C54, Event_ScriptSetLoopCounterOpcode},
    {0x800B8C7C, Event_ScriptLoopBackwardOpcode},
    {0x800B8D50, Event_ScriptApplyImmediateFlagOperationOpcode},
    {0x800B8EDC, Event_ScriptApplyFlagPairOperationOpcode},
    {0x800B88D4, Event_ScriptBranchOnFlagImmediateOpcode},
    {0x800B8A10, Event_ScriptBranchOnFlagPairOpcode},
    {0x800B8B5C, Event_ScriptJumpForwardOpcode},
    {0x800B8B90, Event_ScriptJumpForwardOpcode},
    {0x800B8CE8, Event_ScriptJumpForwardOpcode},
    {0x800B8D1C, Event_ScriptJumpForwardOpcode},
    {0x800B8C44, Event_ScriptReturnToSavedAddressOpcode},
    {0x800B9170, Event_ScriptStoreInventoryItemPresenceOpcode},
    {0x800B9254, Event_ScriptStoreButtonStateOpcode},
    {0x800B6744, Event_ScriptBeginSkippableCutsceneOpcode},
    {0x800B6868, Event_ScriptEndCutsceneAndJumpOpcode},
    {0x800B68C4, Event_ScriptEndCutsceneOpcode},
    {0x800B6FB0, Event_ScriptWaitForButtonAndYieldOpcode},
    {0x800B96B0, Event_ScriptWaitForButtonOrTimeoutOpcode},
    {0x800B94D8, Event_ScriptConfigureTimerOpcode},
    {0x800B9724, Event_ScriptConfigureEasingCurveOpcode},
    {0x800B66A8, Event_ScriptReportInvalidOpcodeOpcode},
    {0x800B66F4, Event_ScriptWaitForEventFileOpcode},
    {0x800BB5AC, Event_CommandLoadNextEventOpcode},
    {0x800BB604, Event_CommandFinishEventOpcode},
    {0x800BB028, Event_CommandSpecialOperationOpcode},
});

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
  Input_PressedButtonsView pressed = Input_buttonsPressed;
  int previous = Input_previousButtons;
  char forced = Event_isSkipForced;
  char stops_music = Event_stopsMusicOnSkip;
  State_SceneTimer timer = State_sceneTimer;
  char timer_hud = Battle_lowerScreenUiState;
  std::int16_t event_id = Event_eventId;
  std::uint16_t event_source = Event_eventSource;

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
    Input_buttonsPressed = pressed;
    Input_previousButtons = previous;
    Event_isSkipForced = forced;
    Event_stopsMusicOnSkip = stops_music;
    State_sceneTimer = timer;
    Battle_lowerScreenUiState = timer_hud;
    Event_eventId = event_id;
    Event_eventSource = event_source;
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

void AddResultDigest(std::uint64_t& digest, std::uint32_t value);

std::array<std::uint8_t, 2> FrozenFlagResult(const FlagComparison& sample) {
  const int first = VS::State::LiveScriptFlag(sample.first);
  const auto second = VS::State::LiveScriptFlag(sample.second);
  const int operand = sample.pair ? second : sample.right;
  const int difference = first - operand;
  const std::array<int, 10> results{first + operand,
                                    first - operand,
                                    first * operand,
                                    operand == 0 ? -1 : first / operand,
                                    operand == 0 ? first : first % operand,
                                    first & operand,
                                    first | operand,
                                    operand,
                                    difference < 0 ? -difference : difference,
                                    operand};
  const auto operation = static_cast<std::size_t>(sample.operation >> 4);
  const bool supported = operation < (sample.pair ? 10 : 8);
  const auto result =
      static_cast<std::uint8_t>(supported ? results[operation] : first);
  const auto other =
      sample.pair && operation == 9 ? static_cast<std::uint8_t>(first) : second;
  return {result, sample.first == sample.second ? result : other};
}

bool CompareFrozenFlagOperation(
    const FlagComparison& sample, int mutation = 0,
    std::array<std::uint64_t, 2>* fingerprints = nullptr) {
  auto command = FlagCommand(sample);
  InitializeFlagSample(sample);
  const auto expected = FrozenFlagResult(sample);
  const auto first = expected[0];
  const auto second = expected[1];
  if (mutation == 1) command[1] ^= 0x10;
  if (mutation == 2) command[2] ^= 4;
  if (mutation == 3) ++command[3];
  const event::ScriptInstruction instruction{command, {}, 0};
  const auto result = sample.pair ? event::ScriptApplyFlagPairOperation(
                                        instruction, event::LiveScriptVm())
                                  : event::ScriptApplyImmediateFlagOperation(
                                        instruction, event::LiveScriptVm());
  if (fingerprints != nullptr) {
    const auto expected_value =
        static_cast<std::uint32_t>(first) |
        (sample.pair ? static_cast<std::uint32_t>(second) << 8 : 0);
    const auto native_value =
        static_cast<std::uint32_t>(VS::State::LiveScriptFlag(sample.first)) |
        (sample.pair ? static_cast<std::uint32_t>(
                           VS::State::LiveScriptFlag(sample.second))
                           << 8
                     : 0);
    AddResultDigest((*fingerprints)[0], expected_value);
    AddResultDigest((*fingerprints)[1], native_value);
  }
  return result.kind == event::ScriptFlowKind::kAdvance &&
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
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
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
      REQUIRE(CompareFrozenFlagOperation(
          {first, second, left, right,
           static_cast<std::uint8_t>(operation << 4), pair},
          0, &fingerprints));
    }
  }
  CHECK(fingerprints[0] == fingerprints[1]);
  CHECK(fingerprints[0] == (pair ? 0x593182F1F2B16919 : 0x41C1AFD9493A9DAF));
}

bool FrozenFlagBranchHolds(const FlagComparison& sample) {
  const int left = VS::State::LiveScriptFlag(sample.first);
  const int right =
      sample.pair ? VS::State::LiveScriptFlag(sample.second) : sample.right;
  const std::array<bool, 7> holds{
      left <= right,           left >= right,
      left == right,           left != right,
      left<right, left> right, !sample.pair && (left & right) != 0};
  const auto operation = static_cast<std::size_t>(sample.operation >> 4);
  return operation < holds.size() && holds[operation];
}

bool CompareFrozenFlagBranch(
    const FlagComparison& sample, std::int16_t displacement, int mutation = 0,
    std::array<std::uint64_t, 2>* fingerprints = nullptr) {
  std::array<std::uint8_t, 64> storage{};
  auto* command = storage.data() + 32;
  std::ranges::copy(FlagCommand(sample), command);
  command[4] = static_cast<std::uint8_t>(displacement);
  command[5] =
      static_cast<std::uint8_t>(static_cast<std::uint16_t>(displacement) >> 8);
  InitializeFlagSample(sample);
  const bool expected_jump = !FrozenFlagBranchHolds(sample);
  const auto expected_flow =
      expected_jump ? event::ScriptFlow::JumpBy(displacement)
                    : event::ScriptFlow::Of(event::ScriptFlowKind::kAdvance);
  if (mutation == 1) command[1] ^= 0x10;
  if (mutation == 2) ++command[4];
  if (mutation == 3) command[3] ^= 0x80;
  const event::ScriptInstruction instruction{{command, 32}, {}, 0};
  const auto native = sample.pair ? event::ScriptBranchOnFlagPair(
                                        instruction, event::LiveScriptVm())
                                  : event::ScriptBranchOnFlagImmediate(
                                        instruction, event::LiveScriptVm());
  if (fingerprints != nullptr) {
    const auto expected_offset =
        static_cast<std::uint16_t>(expected_jump ? displacement : 0);
    const bool native_jump = native.kind == event::ScriptFlowKind::kJumpBy;
    AddResultDigest((*fingerprints)[0],
                    static_cast<std::uint32_t>(expected_jump) |
                        (static_cast<std::uint32_t>(expected_offset) << 16));
    AddResultDigest((*fingerprints)[1],
                    static_cast<std::uint32_t>(native_jump) |
                        (static_cast<std::uint32_t>(static_cast<std::uint16_t>(
                             native_jump ? native.displacement : 0))
                         << 16));
  }
  return native == expected_flow;
}

void CompareFlagBranchCorpus(bool pair) {
  FlagDifferentialFixture fixture;
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
  std::uint32_t random = 0xB8A10;
  for (std::uint8_t operation = 0; operation < 16; ++operation) {
    for (int iteration = 0; iteration < 2048; ++iteration) {
      const int first = iteration & 1023;
      const int second = (iteration & 1) == 0 ? first : (iteration * 13) & 1023;
      REQUIRE(CompareFrozenFlagBranch(
          {first, second, static_cast<std::uint8_t>(NextFlagSample(random)),
           static_cast<std::uint8_t>(NextFlagSample(random)),
           static_cast<std::uint8_t>(operation << 4), pair},
          static_cast<std::int16_t>((iteration & 63) - 32), 0, &fingerprints));
    }
  }
  CHECK(fingerprints[0] == fingerprints[1]);
  CHECK(fingerprints[0] == (pair ? 0xC89571B1A7F86BCD : 0xE1BCD4375C206433));
}

void AddResultDigest(std::uint64_t& digest, std::uint32_t value) {
  for (int byte = 0; byte < 4; ++byte) {
    digest ^= static_cast<std::uint8_t>(value >> (byte * 8));
    digest *= 0x100000001B3;
  }
}

int WaitResultCode(event::ScriptFlow flow) {
  switch (flow.kind) {
    case event::ScriptFlowKind::kAdvance:
      return 0;
    case event::ScriptFlowKind::kHold:
      return 1;
    case event::ScriptFlowKind::kAdvanceAndYield:
      return 4;
    default:
      return -1;
  }
}

struct ButtonWaitSample {
  std::uint8_t mask = 0;
  std::uint8_t duration = 0;
  std::int16_t remaining = 0;
  std::uint32_t buttons = 0;
  int thread = 0;
  bool timeout = false;
};

struct ButtonWaitExpected {
  int code = 0;
  std::int16_t wait = 0;
  std::uint32_t buttons = 0;
};

bool CompareFrozenEasing(std::uint8_t operand,
                         std::array<std::uint64_t, 2>& fingerprints,
                         std::uint8_t mutation = 0) {
  std::array<std::uint8_t, 2> command{0, operand};
  auto& host = event::LiveScriptVm();
  for (int slot = 0; slot < event::kCustomEasingCurveCount; ++slot) {
    host.SetCustomEasingCurve(slot, {.family = 3, .power = -4});
  }
  const int original = 0;
  std::array<std::uint16_t, event::kCustomEasingCurveCount> expected{};
  expected.fill(0xFC03);
  const auto power = static_cast<std::uint8_t>(
      (operand & 8) != 0 ? (operand & 7) | 0xF8 : operand & 7);
  expected[operand >> 6] = static_cast<std::uint16_t>(
      ((operand >> 4) & 3) | (static_cast<std::uint16_t>(power) << 8));
  command[1] ^= mutation;
  const auto native = event::ScriptConfigureEasingCurve({command, {}, 0}, host);
  AddResultDigest(fingerprints[0], static_cast<std::uint32_t>(original));
  AddResultDigest(fingerprints[1],
                  static_cast<std::uint32_t>(WaitResultCode(native)));
  bool equal = original == WaitResultCode(native);
  for (int slot = 0; slot < event::kCustomEasingCurveCount; ++slot) {
    const auto curve = host.CustomEasingCurve(slot);
    const auto value = static_cast<std::uint16_t>(
        curve.family |
        (static_cast<std::uint16_t>(static_cast<std::uint8_t>(curve.power))
         << 8));
    const auto original_value = expected[static_cast<std::size_t>(slot)];
    AddResultDigest(fingerprints[0], original_value);
    AddResultDigest(fingerprints[1], value);
    equal = equal && original_value == value;
  }
  return equal;
}

ButtonWaitExpected FrozenButtonWaitResult(const ButtonWaitSample& sample) {
  ButtonWaitExpected result{1, sample.remaining, sample.buttons};
  const bool pressed = (sample.buttons & sample.mask) != 0;
  if (!sample.timeout) {
    result.code = pressed ? 4 : 1;
  } else if (sample.remaining == 0) {
    result.wait = sample.duration;
  } else {
    result.wait = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(sample.remaining) - 1);
    if (pressed || result.wait <= 0) {
      result = {0, 0, 0};
    }
  }
  return result;
}

bool CompareFrozenButtonWait(const ButtonWaitSample& sample,
                             std::array<std::uint64_t, 2>& fingerprints,
                             int mutation = 0) {
  std::array<std::uint8_t, 3> command{0, sample.mask, sample.duration};
  const auto expected = FrozenButtonWaitResult(sample);
  const auto original = expected.code;
  const auto expected_wait = expected.wait;
  const auto expected_buttons = expected.buttons;
  Event_threadWaitFrames[sample.thread] = sample.remaining;
  Input_buttonsPressed.allButtons = sample.buttons;
  if (mutation == 1) command[1] ^= 4;
  if (mutation == 2) ++Event_threadWaitFrames[sample.thread];
  const event::ScriptInstruction instruction{command, {}, sample.thread};
  const auto native = sample.timeout ? event::ScriptWaitForButtonOrTimeout(
                                           instruction, event::LiveScriptVm())
                                     : event::ScriptWaitForButtonAndYield(
                                           instruction, event::LiveScriptVm());
  const auto result = WaitResultCode(native);
  const auto wait = Event_threadWaitFrames[sample.thread];
  if (mutation == 3) Input_buttonsPressed.allButtons = sample.buttons;
  const auto buttons = Input_buttonsPressed.allButtons;
  AddResultDigest(fingerprints[0], static_cast<std::uint32_t>(original));
  AddResultDigest(fingerprints[0], static_cast<std::uint16_t>(expected_wait));
  AddResultDigest(fingerprints[0], expected_buttons);
  AddResultDigest(fingerprints[1], static_cast<std::uint32_t>(result));
  AddResultDigest(fingerprints[1], static_cast<std::uint16_t>(wait));
  AddResultDigest(fingerprints[1], buttons);
  return original == result && wait == expected_wait &&
         buttons == expected_buttons;
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
    AddResultDigest((*fingerprints)[0],
                    expected_cache |
                        (static_cast<std::uint32_t>(expected_jump) << 8) |
                        (static_cast<std::uint32_t>(expected_offset) << 16));
    AddResultDigest((*fingerprints)[1],
                    native_value |
                        (static_cast<std::uint32_t>(native_jump) << 8) |
                        (static_cast<std::uint32_t>(native_offset) << 16));
  }
  return native_cache.kind == event::ScriptFlowKind::kAdvance &&
         static_cast<std::uint8_t>(event::LiveScriptVm().CachedFlag()) ==
             expected_cache &&
         native == expected_flow;
}

bool CompareFrozenLoop(std::uint8_t counter, std::uint8_t value, bool setting,
                       int mutation = 0,
                       std::array<std::uint64_t, 2>* fingerprints = nullptr) {
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
  std::array<std::uint8_t, event::kLoopCounterCount> expected{};
  expected.fill(0x55);
  if (counter != 0xFF) {
    expected[counter] = setting ? value : static_cast<std::uint8_t>(value - 1);
  }
  const bool expected_jump =
      !setting && (counter == 0xFF || expected[counter] != 0);
  const auto expected_flow =
      expected_jump ? event::ScriptFlow::JumpBy(-displacement)
                    : event::ScriptFlow::Of(event::ScriptFlowKind::kAdvance);
  if (mutation == 1) command[1] ^= 1;
  if (mutation == 2) ++command[2];
  if (mutation == 3 && counter != 0xFF) {
    host.SetLoopCounter(counter, static_cast<std::uint8_t>(value + 1));
  }
  const event::ScriptInstruction instruction{{command, 32}, {}, 0};
  const auto native = setting ? event::ScriptSetLoopCounter(instruction, host)
                              : event::ScriptLoopBackward(instruction, host);
  if (fingerprints != nullptr) {
    const auto expected_offset =
        static_cast<std::uint16_t>(expected_jump ? -displacement : 0);
    const bool native_jump = native.kind == event::ScriptFlowKind::kJumpBy;
    AddResultDigest((*fingerprints)[0],
                    static_cast<std::uint32_t>(expected_jump) |
                        (static_cast<std::uint32_t>(expected_offset) << 16));
    AddResultDigest((*fingerprints)[1],
                    static_cast<std::uint32_t>(native_jump) |
                        (static_cast<std::uint32_t>(static_cast<std::uint16_t>(
                             native_jump ? native.displacement : 0))
                         << 16));
    for (int index = 0; index < event::kLoopCounterCount; ++index) {
      AddResultDigest((*fingerprints)[0],
                      expected[static_cast<std::size_t>(index)]);
      AddResultDigest((*fingerprints)[1], host.LoopCounter(index));
    }
  }
  if (native != expected_flow) {
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
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
  for (int index = 0; index <= event::kLoopCounterCount; ++index) {
    const auto counter = static_cast<std::uint8_t>(
        index == event::kLoopCounterCount ? 0xFF : index);
    for (int value = 0; value < 256; ++value) {
      CAPTURE(setting, counter, value);
      REQUIRE(CompareFrozenLoop(counter, static_cast<std::uint8_t>(value),
                                setting, 0, &fingerprints));
    }
  }
  CHECK(fingerprints[0] == fingerprints[1]);
  CHECK(fingerprints[0] == (setting ? 0x79BC34873972BB25 : 0xDF9CCA1BC970DBA5));
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

std::intptr_t SetLoopSlotBridge(std::uint8_t* instruction,
                                std::int16_t thread) {
  return Event_ScriptSetLoopCounterOpcode(instruction, thread);
}

std::intptr_t LoopBackSlotBridge(std::uint8_t* instruction,
                                 std::int16_t thread) {
  return Event_ScriptLoopBackwardOpcode(instruction, thread);
}

std::intptr_t ImmediateFlagSlotBridge(std::uint8_t* instruction,
                                      std::int16_t thread) {
  return Event_ScriptApplyImmediateFlagOperationOpcode(instruction, thread);
}

std::intptr_t PairFlagSlotBridge(std::uint8_t* instruction,
                                 std::int16_t thread) {
  return Event_ScriptApplyFlagPairOperationOpcode(instruction, thread);
}

std::intptr_t ImmediateBranchSlotBridge(std::uint8_t* instruction,
                                        std::int16_t thread) {
  return Event_ScriptBranchOnFlagImmediateOpcode(instruction, thread);
}

std::intptr_t PairBranchSlotBridge(std::uint8_t* instruction,
                                   std::int16_t thread) {
  return Event_ScriptBranchOnFlagPairOpcode(instruction, thread);
}

std::intptr_t NativeJumpSlotBridge(std::uint8_t* instruction,
                                   std::int16_t thread) {
  return Event_ScriptJumpForwardOpcode(instruction, thread);
}

std::intptr_t NativeReturnSlotBridge(std::uint8_t* instruction,
                                     std::int16_t thread) {
  return Event_ScriptReturnToSavedAddressOpcode(instruction, thread);
}

std::intptr_t ButtonStoreSlotBridge(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return Event_ScriptStoreButtonStateOpcode(instruction, thread);
}

std::intptr_t BeginCutsceneSlotBridge(std::uint8_t* instruction,
                                      std::int16_t thread) {
  return Event_ScriptBeginSkippableCutsceneOpcode(instruction, thread);
}

std::intptr_t EndCutsceneJumpSlotBridge(std::uint8_t* instruction,
                                        std::int16_t thread) {
  return Event_ScriptEndCutsceneAndJumpOpcode(instruction, thread);
}

std::intptr_t EndCutsceneSlotBridge(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return Event_ScriptEndCutsceneOpcode(instruction, thread);
}

std::intptr_t ButtonYieldSlotBridge(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return Event_ScriptWaitForButtonAndYieldOpcode(instruction, thread);
}

std::intptr_t ButtonTimeoutSlotBridge(std::uint8_t* instruction,
                                      std::int16_t thread) {
  return Event_ScriptWaitForButtonOrTimeoutOpcode(instruction, thread);
}

std::intptr_t TimerConfigSlotBridge(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return Event_ScriptConfigureTimerOpcode(instruction, thread);
}

std::intptr_t EasingConfigSlotBridge(std::uint8_t* instruction,
                                     std::int16_t thread) {
  return Event_ScriptConfigureEasingCurveOpcode(instruction, thread);
}

std::intptr_t InvalidOpcodeSlotBridge(std::uint8_t* instruction,
                                      std::int16_t thread) {
  return Event_ScriptReportInvalidOpcodeOpcode(instruction, thread);
}

std::intptr_t FileWaitSlotBridge(std::uint8_t* instruction,
                                 std::int16_t thread) {
  return Event_ScriptWaitForEventFileOpcode(instruction, thread);
}

std::intptr_t LoadEventSlotBridge(std::uint8_t* instruction,
                                  std::int16_t thread) {
  return Event_CommandLoadNextEventOpcode(instruction, thread);
}

std::intptr_t FinishEventSlotBridge(std::uint8_t* instruction,
                                    std::int16_t thread) {
  return Event_CommandFinishEventOpcode(instruction, thread);
}

std::intptr_t SpecialSlotBridge(std::uint8_t* instruction,
                                std::int16_t thread) {
  return Event_CommandSpecialOperationOpcode(instruction, thread);
}

void BindLiveSpecialSlot(bool guarded, int skip) {
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      guarded ? SpecialSlotBridge : Event_CommandSpecialOperationOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE9] = 3;
  Event_skipState = static_cast<char>(skip);
}

void CheckLiveSpecialSelector(bool guarded, int skip, std::uint8_t selector) {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE9, selector, 255, 0xE6});
  BindLiveSpecialSlot(guarded, skip);
  Event_isSkipForced = 0;
  auto& host = event::LiveScriptVm();
  host.SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(host.ThreadCursor(0).Present());
  CHECK(host.SkipForced() == (selector == 2));
}

void CheckLiveSpecialTimer(bool guarded, int skip) {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE9, 8, 1, 0xE9, 8, 0, 0xE6});
  BindLiveSpecialSlot(guarded, skip);
  State_sceneTimer = {.mode = 2,
                      .pausedMode = 0,
                      .resetScope = 3,
                      .frames = 12,
                      .seconds = 3,
                      .minutes = 4,
                      .hours = 5};
  Battle_lowerScreenUiState = 1;
  auto& host = event::LiveScriptVm();
  host.SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(host.ThreadCursor(0).Present());
  CHECK(State_sceneTimer.mode == 2);
  CHECK(State_sceneTimer.pausedMode == 2);
  CHECK(State_sceneTimer.seconds == 3);
  CHECK(Battle_lowerScreenUiState == 1);
}

void CheckLiveSpecialDiscard(bool guarded, int skip) {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE9, 6, 0, 0xE6});
  BindLiveSpecialSlot(guarded, skip);
  auto& host = event::LiveScriptVm();
  REQUIRE(host.AllocateChainedEventBuffer());
  REQUIRE(host.QueueEventFileLoad(1, event::EventBuffer::kChained));
  auto& request = PendingRequest();
  host.SetChainedEventLoaded(true);
  host.SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(host.ThreadCursor(0).Present());
  CHECK(host.EventFile() == event::EventFileStatus::kNone);
  CHECK(host.ChainedEventLoaded());
  CHECK(request.state == Content_DiscRequestState_Free);
  CHECK_FALSE(host.AdoptChainedEvent());
  CheckMountedByte(0xE9, start.generation);
}

void CheckLiveFileWait(bool guarded, int skip, int state) {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE9, 0xE3, 4, 0xE6});
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      guarded ? FileWaitSlotBridge : Event_ScriptWaitForEventFileOpcode;
  Event_kSkipOpcodeHandlers[0xE3] = Event_kOpcodeHandlers[0xE3] =
      Event_CommandSetTickSpeedOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE3] = 2;
  Event_skipState = static_cast<char>(skip);
  State_tickSpeed = 2;
  auto& host = event::LiveScriptVm();
  if (state != 0) {
    REQUIRE(host.QueueEventFileLoad(0, event::EventBuffer::kScript));
    if (state == 2) CompleteSyntheticFile(0xAA);
    if (state == 3) PendingRequest().state = Content_DiscRequestState_Error;
  }
  host.SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(host.ThreadCursor(0) == (state == 1 ? start : event::ScriptLocation{}));
  CHECK(State_tickSpeed == ((state == 0 || state == 2) ? 4 : 2));
  CheckMountedByte(0xE9, start.generation);
}

void CheckLiveChainedLoad(bool guarded, int skip) {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE9, 1, 0, 0xE6});
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      guarded ? LoadEventSlotBridge : Event_CommandLoadNextEventOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE9] = 3;
  Event_skipState = static_cast<char>(skip);
  auto& host = event::LiveScriptVm();
  host.SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(host.ThreadCursor(0).Present());
  CHECK(host.EventId() == 1);
  CHECK(host.ChainedEventLoaded());
  CHECK(host.EventFile() == event::EventFileStatus::kLoading);
  CheckMountedByte(0xE9, start.generation);
  CHECK_FALSE(host.AdoptChainedEvent());
  CompleteSyntheticFile(0x77);
  host.ReleaseEventFile();
  CheckMountedByte(0xE9, start.generation);
  REQUIRE(host.AdoptChainedEvent());
  CheckMountedByte(0x77, start.generation + 1);
}

void CheckLiveFinishEvent(bool guarded, int skip) {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE9});
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      guarded ? FinishEventSlotBridge : Event_CommandFinishEventOpcode;
  Event_skipState = static_cast<char>(skip);
  State_tickSpeed = 4;
  auto& host = event::LiveScriptVm();
  host.SetThreadCursor(0, start);
  CHECK(event::ExecuteLiveScriptThread(0));
  CHECK(State_tickSpeed == 2);
  CHECK(host.ThreadCursor(0) == start);
}

void CheckLiveEasingConfiguration(bool guarded, int skip) {
  const LiveVmFixture fixture;
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      guarded ? EasingConfigSlotBridge : Event_ScriptConfigureEasingCurveOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE9] = 2;
  auto& host = event::LiveScriptVm();
  for (int operand = 0; operand < 256; ++operand) {
    for (int slot = 0; slot < event::kCustomEasingCurveCount; ++slot) {
      host.SetCustomEasingCurve(slot, {.family = 3, .power = -4});
    }
    const auto start =
        MountScript({0xE9, static_cast<std::uint8_t>(operand), 0xE6});
    Event_skipState = static_cast<char>(skip);
    host.SetThreadCursor(0, start);
    CHECK_FALSE(event::ExecuteLiveScriptThread(0));
    CHECK_FALSE(host.ThreadCursor(0).Present());
    for (int slot = 0; slot < event::kCustomEasingCurveCount; ++slot) {
      const int power = operand & 15;
      const event::EasingCurve expected =
          slot == (operand >> 6)
              ? event::EasingCurve{.family = static_cast<std::uint8_t>(
                                       (operand >> 4) & 3),
                                   .power = static_cast<std::int8_t>(
                                       power >= 8 ? power - 16 : power)}
              : event::EasingCurve{.family = 3, .power = -4};
      CHECK(host.CustomEasingCurve(slot) == expected);
    }
  }
}

struct TimerConfigScenario {
  std::uint8_t mode = 0;
  std::uint8_t scope = 0;
  std::uint32_t time = 0;
  bool reset = false;
  bool guarded = false;
  int skip = 0;
};

VS::State::SceneTimer ExpectedTimerConfiguration(TimerConfigScenario scenario,
                                                 bool valid,
                                                 event::ScriptVmHost& host) {
  auto expected = VS::State::LiveSceneTimer();
  if (scenario.reset) {
    expected.mode = 0;
    expected.time = {};
  } else if (valid) {
    expected.mode =
        static_cast<std::uint8_t>(host.SceneTimerModes()[scenario.mode]);
    expected.reset_scope =
        static_cast<std::uint16_t>(host.SceneTimerScopes()[scenario.scope]);
    expected.time = expected.mode == VS::State::kSceneTimerCountsUp
                        ? VS::State::SceneTime{}
                        : VS::State::UnpackSceneTime(
                              static_cast<std::int32_t>(scenario.time));
  }
  return expected;
}

void CheckTimerFlags(event::ScriptVmHost& host, VS::State::SceneTime time) {
  CHECK(host.Flag(0xA0) == static_cast<std::uint8_t>(time.frames));
  CHECK(host.Flag(0xA1) == static_cast<std::uint8_t>(time.seconds));
  CHECK(host.Flag(0xA2) == static_cast<std::uint8_t>(time.minutes));
  CHECK(host.Flag(0xA3) == static_cast<std::uint8_t>(time.hours));
}

void CheckLiveTimerConfiguration(TimerConfigScenario scenario) {
  const LiveVmFixture fixture;
  const FlagDifferentialFixture flags;
  const auto start = MountScript(
      {0xE9, static_cast<std::uint8_t>(scenario.reset ? 0 : 1), scenario.scope,
       scenario.mode, static_cast<std::uint8_t>(scenario.time >> 24),
       static_cast<std::uint8_t>(scenario.time >> 16),
       static_cast<std::uint8_t>(scenario.time >> 8), 0xE6});
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      scenario.guarded ? TimerConfigSlotBridge
                       : Event_ScriptConfigureTimerOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE9] = 7;
  Event_skipState = static_cast<char>(scenario.skip);
  State_sceneTimer = {.mode = 2,
                      .pausedMode = 1,
                      .resetScope = 0x1234,
                      .frames = 12,
                      .seconds = 3,
                      .minutes = 4,
                      .hours = 5};
  Battle_lowerScreenUiState = 1;
  auto& host = event::LiveScriptVm();
  const bool valid = scenario.mode < host.SceneTimerModes().size() &&
                     scenario.scope < host.SceneTimerScopes().size();
  const auto expected = ExpectedTimerConfiguration(scenario, valid, host);
  host.SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(host.ThreadCursor(0).Present());
  CHECK(VS::State::LiveSceneTimer() == expected);
  CHECK(Battle_lowerScreenUiState == (scenario.reset ? 0 : 1));
  if (scenario.reset || valid) {
    CheckTimerFlags(host, expected.time);
  }
}

void CheckTimerConfigurationMatrix(bool guarded, int skip) {
  for (const std::uint32_t time : {0U, 0x01234500U, 0x81234500U, 0xFFFFFF00U}) {
    for (int mode = 0; mode < 2; ++mode) {
      for (int scope = 0; scope < 4; ++scope) {
        CheckLiveTimerConfiguration({static_cast<std::uint8_t>(mode),
                                     static_cast<std::uint8_t>(scope), time,
                                     false, guarded, skip});
      }
    }
  }
}

void CheckLiveButtonWait(ButtonWaitSample sample, bool guarded, int skip) {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE9, sample.mask, sample.duration, 0xE6});
  const Handler handler =
      sample.timeout ? (guarded ? ButtonTimeoutSlotBridge
                                : Event_ScriptWaitForButtonOrTimeoutOpcode)
                     : (guarded ? ButtonYieldSlotBridge
                                : Event_ScriptWaitForButtonAndYieldOpcode);
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] = handler;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE9] = sample.timeout ? 3 : 2;
  Event_skipState = static_cast<char>(skip);
  auto& host = event::LiveScriptVm();
  host.SetWaitFrames(sample.thread, sample.remaining);
  Input_buttonsPressed.allButtons = sample.buttons;
  host.SetThreadCursor(sample.thread, start);
  const auto expected = FrozenButtonWaitResult(sample);
  CHECK_FALSE(event::ExecuteLiveScriptThread(sample.thread));
  CHECK(host.WaitFrames(sample.thread) == expected.wait);
  CHECK(Input_buttonsPressed.allButtons == expected.buttons);
  CHECK(host.ThreadCursor(sample.thread) ==
        (expected.code == 0
             ? event::ScriptLocation{}
             : event::ScriptLocation{start.region, expected.code == 4 ? 2U : 0U,
                                     start.generation}));
}

void CheckLiveYieldMasks(bool guarded, int skip) {
  for (const int mask : {0, 1, 2, 4, 127, 128, 255}) {
    for (const int buttons : {0, 1, 4, 128, 255}) {
      CheckLiveButtonWait(
          {static_cast<std::uint8_t>(mask), 0xE6, 17,
           0xA5CD0000U | static_cast<std::uint32_t>(buttons), mask & 3, false},
          guarded, skip);
    }
  }
}

void CheckLiveTimeoutCounters(bool guarded, int skip, int thread) {
  for (const int remaining : {0, 1, 2, 3, 32767, -1, -32768}) {
    for (const bool pressed : {false, true}) {
      for (const std::uint8_t duration : {0, 3, 255}) {
        CheckLiveButtonWait({4, duration, static_cast<std::int16_t>(remaining),
                             0xA5CD0000U | (pressed ? 4U : 0U), thread, true},
                            guarded, skip);
      }
    }
  }
}

void CheckLiveCutsceneBegin(bool guarded, int skip) {
  const LiveVmFixture fixture;
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      guarded ? BeginCutsceneSlotBridge
              : Event_ScriptBeginSkippableCutsceneOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE9] = 2;
  auto& host = event::LiveScriptVm();
  for (int value = 0; value < 256; ++value) {
    for (const int force : {0, 1, 128, 255}) {
      const auto location =
          MountScript({0xE9, static_cast<std::uint8_t>(value), 0xE6});
      Event_skipState = static_cast<char>(skip);
      Event_isSkipForced = static_cast<char>(force);
      host.SetThreadCursor(0, location);
      CHECK_FALSE(event::ExecuteLiveScriptThread(0));
      CHECK(host.ThreadCursor(0) ==
            (force != 0 ? event::ScriptLocation{location.region, 2,
                                                location.generation}
                        : event::ScriptLocation{}));
      CHECK(host.SkipState() == event::CutsceneSkip::kSkippable);
      CHECK(host.StopsMusicOnSkip() == ((value & 1) == 0));
    }
  }
}

struct CutsceneEndScenario {
  int previous = 0;
  int displacement = 0;
  bool guarded = false;
  bool jump = false;
};

struct CutsceneFrameFixture {
  Render_FrameScratch saved = Render_frameScratch;
  void* saved_ordering = Render_legacyCurrentOrderingTable;
  void* saved_primitives = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  std::array<u_long, 0x2088 / sizeof(u_long)> ordering{};
  std::array<u_long, 18> storage{};
  std::uint64_t dropped = VS::Render::DroppedLiveUiPacketCount();

  CutsceneFrameFixture() {
    storage.fill(0xDEADBEEF);
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_frameBuf = 0;
    Render_legacyCurrentOrderingTable = ordering.data();
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    Render_frameScratch.primitiveCursor = storage.data() + 1;
  }
  ~CutsceneFrameFixture() {
    Render_frameScratch = saved;
    Render_legacyCurrentOrderingTable = saved_ordering;
    Render_legacyPrimitiveBuffers[0] = saved_primitives;
    Render_frameBuf = saved_framebuffer;
  }
  void CheckEmission(bool emitted) const {
    CHECK(VS::Render::DroppedLiveUiPacketCount() == dropped);
    CHECK((Render_frameScratch.primitiveCursor != storage.data() + 1) ==
          emitted);
    CHECK(storage.front() == 0xDEADBEEF);
    CHECK(storage.back() == 0xDEADBEEF);
  }
};

void CheckLiveCutsceneEnd(CutsceneEndScenario scenario) {
  const LiveVmFixture fixture;
  const FlagDifferentialFixture flags;
  const CutsceneFrameFixture frame;
  Bytes bytes(64, 0x0F);
  bytes[32] = 0xE9;
  bytes[33] = static_cast<std::uint8_t>(scenario.displacement);
  bytes[34] = static_cast<std::uint8_t>(
      static_cast<std::uint16_t>(scenario.displacement) >> 8);
  const int target = scenario.jump ? 32 + scenario.displacement : 33;
  bytes[static_cast<std::size_t>(target)] = 0xE6;
  auto start = MountScript(std::move(bytes));
  start.offset = 32;
  const Handler handler =
      scenario.jump ? (scenario.guarded ? EndCutsceneJumpSlotBridge
                                        : Event_ScriptEndCutsceneAndJumpOpcode)
                    : (scenario.guarded ? EndCutsceneSlotBridge
                                        : Event_ScriptEndCutsceneOpcode);
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] = handler;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] = StopBridge;
  Event_kOpcodeSizes[0xE9] = scenario.jump ? 3 : 1;
  Event_skipState = static_cast<char>(scenario.previous);
  observations = 0;
  auto& host = event::LiveScriptVm();
  host.SetFlag(0xA8, 1);
  host.SetThreadCursor(0, start);
  const bool skipping = scenario.previous == 2;
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(host.SkipState() == event::CutsceneSkip::kNone);
  CHECK(host.Flag(0xA8) == (skipping ? 0 : 1));
  CHECK(observations == (skipping ? 0 : 1));
  frame.CheckEmission(skipping);
  if (skipping) {
    CHECK(host.ThreadCursor(0) ==
          event::ScriptLocation{
              start.region, static_cast<std::uint32_t>(scenario.jump ? 35 : 33),
              start.generation});
  } else {
    CHECK_FALSE(host.ThreadCursor(0).Present());
    CHECK(observed[0] == event::ScriptLocation{
                             start.region, static_cast<std::uint32_t>(target),
                             start.generation});
  }
}

void CheckLiveButtonStore(int operand, bool guarded, int skip) {
  const LiveVmFixture fixture;
  const FlagDifferentialFixture flags;
  const auto start =
      MountScript({0xE9, static_cast<std::uint8_t>(operand),
                   static_cast<std::uint8_t>(operand >> 8), 0xE6});
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      guarded ? ButtonStoreSlotBridge : Event_ScriptStoreButtonStateOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE9] = 3;
  Event_skipState = static_cast<char>(skip);
  auto& host = event::LiveScriptVm();
  for (int value = 0; value < 256; ++value) {
    Input_previousButtons = 0x345600 | value;
    Input_buttonsPressed.allButtons = 0xABCD0000 | (value ^ 0x80);
    host.SetFlag(operand & 1023, 0x55);
    host.SetThreadCursor(0, start);
    CHECK_FALSE(event::ExecuteLiveScriptThread(0));
    CHECK_FALSE(host.ThreadCursor(0).Present());
    CHECK(host.Flag(operand & 1023) ==
          ((operand & 0x8000) == 0 ? value : (value ^ 0x80)));
  }
}

void CheckNativeJumpTarget(int displacement, bool guarded, int skip) {
  const LiveVmFixture fixture;
  Bytes bytes(64, 0xE6);
  bytes[32] = 0xE7;
  bytes[33] = static_cast<std::uint8_t>(displacement);
  bytes[34] =
      static_cast<std::uint8_t>(static_cast<std::uint16_t>(displacement) >> 8);
  auto start = MountScript(std::move(bytes));
  start.offset = 32;
  Event_skipState = static_cast<char>(skip);
  Event_kSkipOpcodeHandlers[0xE7] = Event_kOpcodeHandlers[0xE7] =
      guarded ? NativeJumpSlotBridge : Event_ScriptJumpForwardOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] = StopBridge;
  Event_kOpcodeSizes[0xE7] = 3;
  observations = 0;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(observations == 1);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region,
                              static_cast<std::uint32_t>(32 + displacement),
                              start.generation});
  CHECK_FALSE(event::LiveScriptVm().ThreadCursor(0).Present());
}

void CheckLiveFlagBranch(FlagComparison sample, bool guarded, int skip) {
  const LiveVmFixture fixture;
  FlagDifferentialFixture flags;
  auto command = FlagCommand(sample);
  command[0] = 0xEF;
  Bytes bytes(command.begin(), command.end());
  bytes.insert(bytes.end(), {8, 0, 0xE3, 4, 0xE6});
  const auto start = MountScript(std::move(bytes));
  InitializeFlagSample(sample);
  const bool fallthrough = FrozenFlagBranchHolds(sample);
  const Handler handler =
      sample.pair ? (guarded ? PairBranchSlotBridge
                             : Event_ScriptBranchOnFlagPairOpcode)
                  : (guarded ? ImmediateBranchSlotBridge
                             : Event_ScriptBranchOnFlagImmediateOpcode);
  Event_skipState = static_cast<char>(skip);
  State_tickSpeed = 2;
  Event_kSkipOpcodeHandlers[0xEF] = Event_kOpcodeHandlers[0xEF] = handler;
  Event_kSkipOpcodeHandlers[0xE3] = Event_kOpcodeHandlers[0xE3] =
      Event_CommandSetTickSpeedOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xEF] = 6;
  Event_kOpcodeSizes[0xE3] = 2;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(event::LiveScriptVm().ThreadCursor(0).Present());
  CHECK(State_tickSpeed == (fallthrough ? 4 : 2));
}

struct LiveFlagScenario {
  int skip;
  bool guarded;
  bool pair;
  bool alias;
};

void CheckLiveBranchOperations(LiveFlagScenario scenario) {
  for (int operation = 0; operation < 16; ++operation) {
    for (const std::uint8_t value : {0, 127, 128, 255}) {
      CheckLiveFlagBranch(
          {1023, scenario.alias ? 1023 : 511, value, 128,
           static_cast<std::uint8_t>(operation << 4), scenario.pair},
          scenario.guarded, scenario.skip);
    }
  }
}

void CheckLiveFlagOperation(FlagComparison sample, bool guarded, int skip) {
  const LiveVmFixture fixture;
  FlagDifferentialFixture flags;
  auto command = FlagCommand(sample);
  command[0] = 0xED;
  Bytes bytes(command.begin(), command.end());
  bytes.push_back(0xE6);
  const auto start = MountScript(std::move(bytes));
  InitializeFlagSample(sample);
  const auto expected = FrozenFlagResult(sample);
  const Handler handler =
      sample.pair ? (guarded ? PairFlagSlotBridge
                             : Event_ScriptApplyFlagPairOperationOpcode)
                  : (guarded ? ImmediateFlagSlotBridge
                             : Event_ScriptApplyImmediateFlagOperationOpcode);
  Event_skipState = static_cast<char>(skip);
  Event_kSkipOpcodeHandlers[0xED] = Event_kOpcodeHandlers[0xED] = handler;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xED] = 4;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(event::LiveScriptVm().ThreadCursor(0).Present());
  CHECK(VS::State::LiveScriptFlag(sample.first) == expected[0]);
  CHECK(VS::State::LiveScriptFlag(sample.second) == expected[1]);
}

void CheckAllLiveFlagOperations(LiveFlagScenario scenario) {
  for (int operation = 0; operation < 16; ++operation) {
    CheckLiveFlagOperation(
        {1023, scenario.alias ? 1023 : 511, 255, 17,
         static_cast<std::uint8_t>(operation << 4), scenario.pair},
        scenario.guarded, scenario.skip);
  }
}

struct LoopScenario {
  int skip;
  bool guarded;
  std::uint8_t counter;
  std::uint8_t initial;
};

event::ScriptLocation PrepareLoopScenario(LoopScenario scenario) {
  const auto start =
      MountScript({0xE9, scenario.counter, scenario.initial, 0xE1, 0xEA,
                   scenario.counter, 1, 0, 0xE3, 4, 0xE6});
  Event_skipState = static_cast<char>(scenario.skip);
  State_tickSpeed = 2;
  Event_kSkipOpcodeHandlers[0xE9] = Event_kOpcodeHandlers[0xE9] =
      scenario.guarded ? SetLoopSlotBridge : Event_ScriptSetLoopCounterOpcode;
  Event_kSkipOpcodeHandlers[0xEA] = Event_kOpcodeHandlers[0xEA] =
      scenario.guarded ? LoopBackSlotBridge : Event_ScriptLoopBackwardOpcode;
  Event_kSkipOpcodeHandlers[0xE1] = Event_kOpcodeHandlers[0xE1] =
      Event_ScriptYieldFrameOpcode;
  Event_kSkipOpcodeHandlers[0xE3] = Event_kOpcodeHandlers[0xE3] =
      Event_CommandSetTickSpeedOpcode;
  Event_kSkipOpcodeHandlers[0xE6] = Event_kOpcodeHandlers[0xE6] =
      Event_CommandStopScriptContextOpcode;
  Event_kOpcodeSizes[0xE9] = 3;
  Event_kOpcodeSizes[0xEA] = 4;
  Event_kOpcodeSizes[0xE3] = 2;
  event::LiveScriptVm().SetThreadCursor(0, start);
  return start;
}

void CheckLoopResult(std::uint8_t counter, std::uint8_t remaining,
                     bool finished) {
  CHECK(event::LiveScriptVm().LoopCounter(counter) == remaining);
  CHECK(State_tickSpeed == (finished ? 4 : 2));
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

void CheckLoopScenario(LoopScenario scenario) {
  const LiveVmFixture fixture;
  const auto start = PrepareLoopScenario(scenario);
  const event::ScriptLocation back{start.region, 4, start.generation};
  CheckWaitDispatch(0);
  CheckThreadCursorMatches(0, back);
  CheckLoopResult(scenario.counter, scenario.initial, false);
  const int iterations = scenario.initial == 0 ? 256 : scenario.initial;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    const auto remaining =
        static_cast<std::uint8_t>(scenario.initial - iteration - 1);
    CheckWaitDispatch(0);
    CheckThreadCursorMatches(0,
                             remaining == 0 ? event::ScriptLocation{} : back);
    CheckLoopResult(scenario.counter, remaining, remaining == 0);
  }
}

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
  REQUIRE(CompareFrozenFlagOperation(sample));
  for (int mutation = 1; mutation <= 3; ++mutation) {
    CHECK_FALSE(CompareFrozenFlagOperation(sample, mutation));
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
  REQUIRE(CompareFrozenFlagBranch(sample, 6));
  for (int mutation = 1; mutation <= 3; ++mutation) {
    CHECK_FALSE(CompareFrozenFlagBranch(sample, 6, mutation));
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
  REQUIRE(CompareFrozenLoop(3, 7, true));
  for (int mutation = 1; mutation <= 2; ++mutation) {
    CHECK_FALSE(CompareFrozenLoop(3, 7, true, mutation));
  }
  REQUIRE(CompareFrozenLoop(3, 7, false));
  for (int mutation = 1; mutation <= 3; ++mutation) {
    CHECK_FALSE(CompareFrozenLoop(3, 7, false, mutation));
  }
}

TEST_CASE("Native control opcode entries resolve to owned table bindings",
          "[event-vm-live][content][module][control-opcode]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const auto& entry : kControlEntries) {
    CAPTURE(entry.address);
    CHECK(content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        entry.address) ==
          reinterpret_cast<void*>(entry.handler));
  }
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
  for (const auto& entry : kControlEntries) {
    CAPTURE(entry.address);
    CHECK(entry.handler(command.data(), 0) == expected);
  }
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

TEST_CASE("Native loop bindings preserve wrapping countdown and frame yield",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (const int counter : {0, 15}) {
        for (const int initial : {0, 1, 2, 255}) {
          CheckLoopScenario({skip, guarded, static_cast<std::uint8_t>(counter),
                             static_cast<std::uint8_t>(initial)});
        }
      }
    }
  }
}

TEST_CASE("Native loop sentinel never touches the bounded counter bank",
          "[event-vm-live][control-opcode]") {
  for (const bool guarded : {false, true}) {
    const LiveVmFixture fixture;
    auto& host = event::LiveScriptVm();
    for (int counter = 0; counter < event::kLoopCounterCount; ++counter) {
      host.SetLoopCounter(counter, 0x55);
    }
    const auto start = PrepareLoopScenario({2, guarded, 0xFF, 7});
    const event::ScriptLocation back{start.region, 4, start.generation};
    for (int frame = 0; frame < 3; ++frame) {
      CheckWaitDispatch(0);
      CheckThreadCursorMatches(0, back);
      for (int counter = 0; counter < event::kLoopCounterCount; ++counter) {
        CheckLoopResult(static_cast<std::uint8_t>(counter), 0x55, false);
      }
    }
  }
}

TEST_CASE("Native arithmetic bindings preserve all operations and aliasing",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (const bool pair : {false, true}) {
        for (const bool alias : {false, true}) {
          CheckAllLiveFlagOperations({skip, guarded, pair, alias});
        }
      }
    }
  }
}

TEST_CASE("Native arithmetic bindings keep R3000 zero divisor byte results",
          "[event-vm-live][control-opcode]") {
  for (const bool guarded : {false, true}) {
    for (const bool pair : {false, true}) {
      for (const std::uint8_t operation : {0x30, 0x40}) {
        CheckLiveFlagOperation({1023, 511, 255, 0, operation, pair}, guarded,
                               0);
      }
    }
  }
}

TEST_CASE("Native flag branch bindings preserve all unsigned comparison modes",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (const bool pair : {false, true}) {
        for (const bool alias : {false, true}) {
          CheckLiveBranchOperations({skip, guarded, pair, alias});
        }
      }
    }
  }
}

TEST_CASE("Native shared jump binding preserves signed target boundaries",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (const int displacement : {-32, -17, -1, 3, 17, 31}) {
        CheckNativeJumpTarget(displacement, guarded, skip);
      }
    }
  }
}

TEST_CASE("Native button yields match all original mask and byte pairs",
          "[event-vm-live][event-button-differential]") {
  const LiveVmFixture fixture;
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
  for (int mask = 0; mask < 256; ++mask) {
    for (int value = 0; value < 256; ++value) {
      REQUIRE(CompareFrozenButtonWait(
          {static_cast<std::uint8_t>(mask), 0, 17,
           0xA5CD0000U | static_cast<std::uint32_t>(value), mask & 3, false},
          fingerprints));
    }
  }
  CHECK(fingerprints[0] == fingerprints[1]);
  CHECK(fingerprints[0] == 0x45DB1AECCD72B51C);
}

TEST_CASE("Native button timeouts match every original halfword counter",
          "[event-vm-live][event-button-differential]") {
  const LiveVmFixture fixture;
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
  for (int remaining = 0; remaining < 65536; ++remaining) {
    for (const bool pressed : {false, true}) {
      REQUIRE(CompareFrozenButtonWait(
          {4, 255,
           static_cast<std::int16_t>(static_cast<std::uint16_t>(remaining)),
           0xA5CD0000U | (pressed ? 4U : 0U), remaining & 3, true},
          fingerprints));
    }
  }
  CHECK(fingerprints[0] == fingerprints[1]);
  CHECK(fingerprints[0] == 0x3E94AE9F58CCF5A6);
}

TEST_CASE("Native button timeouts match original initialization for every byte",
          "[event-vm-live][event-button-differential]") {
  const LiveVmFixture fixture;
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
  for (int duration = 0; duration < 256; ++duration) {
    for (const std::uint32_t buttons : {0U, 4U, 255U}) {
      REQUIRE(
          CompareFrozenButtonWait({4, static_cast<std::uint8_t>(duration), 0,
                                   0xA5CD0000U | buttons, duration & 3, true},
                                  fingerprints));
    }
  }
  CHECK(fingerprints[0] == fingerprints[1]);
  CHECK(fingerprints[0] == 0x482D3AE966711625);
}

TEST_CASE(
    "Original button wait differential detects mask counter and clear mutants",
    "[event-vm-live][event-button-differential]") {
  const LiveVmFixture fixture;
  for (const auto& sample : {ButtonWaitSample{4, 3, 3, 0xA5CD0004U, 0, false},
                             ButtonWaitSample{4, 3, 3, 0xA5CD0004U, 3, true}}) {
    std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                              0xCBF29CE484222325};
    CHECK_FALSE(CompareFrozenButtonWait(sample, fingerprints, 1));
  }
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
  CHECK_FALSE(CompareFrozenButtonWait({4, 3, 3, 0xA5CD0000U, 1, true},
                                      fingerprints, 2));
  CHECK_FALSE(CompareFrozenButtonWait({4, 3, 1, 0xA5CD0004U, 2, true},
                                      fingerprints, 3));
}

TEST_CASE("Native button yield bindings preserve pressed masks and cursors",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      CheckLiveYieldMasks(guarded, skip);
    }
  }
}

TEST_CASE(
    "Native button timeout bindings preserve initialization and consumption",
    "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (int thread = 0; thread < 4; ++thread) {
        CheckLiveTimeoutCounters(guarded, skip, thread);
      }
    }
  }
}

TEST_CASE("Native easing configuration matches every original packed byte",
          "[event-vm-live][event-easing-differential]") {
  const LiveVmFixture fixture;
  std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                            0xCBF29CE484222325};
  for (int operand = 0; operand < 256; ++operand) {
    REQUIRE(
        CompareFrozenEasing(static_cast<std::uint8_t>(operand), fingerprints));
  }
  CHECK(fingerprints[0] == fingerprints[1]);
  CHECK(fingerprints[0] == 0x9DB56A489D7AD325);
}

TEST_CASE("Original easing differential detects slot family and sign mutants",
          "[event-vm-live][event-easing-differential]") {
  const LiveVmFixture fixture;
  for (const std::uint8_t mutation : {0x40, 0x10, 0x08}) {
    std::array<std::uint64_t, 2> fingerprints{0xCBF29CE484222325,
                                              0xCBF29CE484222325};
    CHECK_FALSE(CompareFrozenEasing(0x8D, fingerprints, mutation));
  }
}

TEST_CASE("Native easing configuration bindings preserve all encoded slots",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      CheckLiveEasingConfiguration(guarded, skip);
    }
  }
}

TEST_CASE("Native timer configuration bindings publish bounded timer records",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      CheckTimerConfigurationMatrix(guarded, skip);
    }
  }
}

TEST_CASE(
    "Native timer configuration bindings refuse invalid selectors before "
    "callbacks",
    "[event-vm-live][control-opcode]") {
  for (const bool guarded : {false, true}) {
    CheckLiveTimerConfiguration({2, 0, 0x01020300U, false, guarded, 0});
    CheckLiveTimerConfiguration({255, 0, 0x01020300U, false, guarded, 2});
    CheckLiveTimerConfiguration({0, 4, 0x01020300U, false, guarded, 0});
    CheckLiveTimerConfiguration({0, 255, 0x01020300U, false, guarded, 2});
    CheckLiveTimerConfiguration({255, 255, 0x01020300U, true, guarded, 0});
  }
}

TEST_CASE(
    "Native file wait bindings preserve pending ready and failed transitions",
    "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (int state = 0; state < 4; ++state) {
        CheckLiveFileWait(guarded, skip, state);
      }
    }
  }
}

TEST_CASE(
    "Native chained load bindings retain staging until completed adoption",
    "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      CheckLiveChainedLoad(guarded, skip);
    }
  }
}

TEST_CASE("Native finish bindings report completion and restore field ticks",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      CheckLiveFinishEvent(guarded, skip);
    }
  }
}

TEST_CASE("Native invalid opcode bindings preserve diagnostic continuation",
          "[event-vm-live][control-opcode]") {
  for (const bool guarded : {false, true}) {
    const LiveVmFixture fixture;
    const auto start = MountScript({0xE9, 0xE3, 4, 0xE6});
    Event_kOpcodeHandlers[0xE9] = guarded
                                      ? InvalidOpcodeSlotBridge
                                      : Event_ScriptReportInvalidOpcodeOpcode;
    Event_kOpcodeHandlers[0xE3] = Event_CommandSetTickSpeedOpcode;
    Event_kOpcodeHandlers[0xE6] = Event_CommandStopScriptContextOpcode;
    Event_kOpcodeSizes[0xE3] = 2;
    State_tickSpeed = 2;
    event::LiveScriptVm().SetThreadCursor(0, start);
    CHECK_FALSE(event::ExecuteLiveScriptThread(0));
    CHECK_FALSE(event::LiveScriptVm().ThreadCursor(0).Present());
    CHECK(State_tickSpeed == 4);
  }
}

TEST_CASE(
    "Native special bindings preserve force and undefined selector "
    "continuation",
    "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (const std::uint8_t selector : {2, 18, 255}) {
        CheckLiveSpecialSelector(guarded, skip, selector);
      }
    }
  }
}

TEST_CASE(
    "Native special bindings retain timer and chained ownership callbacks",
    "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      CheckLiveSpecialTimer(guarded, skip);
      CheckLiveSpecialDiscard(guarded, skip);
    }
  }
}

TEST_CASE("Native playback queries preserve every phase and start latch",
          "[event-vm-live][event-query]") {
  const LiveVmFixture fixture;
  auto& host = event::LiveScriptVm();
  for (int phase = 0; phase < 256; ++phase) {
    Event_playbackPhase = static_cast<char>(phase);
    for (const bool confirmed : {false, true}) {
      Event_isStartConfirmed = static_cast<char>(confirmed);
      CHECK(static_cast<int>(event::EventPlaybackState(host)) == phase);
      CHECK(event::RequestEventStart(host) == 1);
      CHECK(host.StartConfirmed() == (confirmed || phase == 1));
    }
  }
}

TEST_CASE("Native pending selectors preserve source and halfword bits",
          "[event-vm-live][event-query]") {
  const LiveVmFixture fixture;
  auto& host = event::LiveScriptVm();
  for (int source = 0; source < 16; ++source) {
    for (const int id : {0, 1, 0xFFF, 0x1000, 0x7FFF, 0x8000, 0xFFFF}) {
      Event_eventSource = static_cast<std::uint16_t>(source);
      Event_eventId = static_cast<std::int16_t>(static_cast<std::uint16_t>(id));
      CHECK(static_cast<std::uint16_t>(event::PendingEventSelector(host)) ==
            static_cast<std::uint16_t>((source << 12) | id));
    }
  }
}

TEST_CASE("Native actor reference adapter preserves slot and special selectors",
          "[event-vm-live][event-actor-reference]") {
  const LiveVmFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", 0x800BFE50) ==
      reinterpret_cast<const void*>(&event::ResolveLiveScriptActorReference));
  for (int reference = 0x1000; reference < 65536; ++reference) {
    const int expected = (reference >> 12) == 1 ? reference & 31 : reference;
    CHECK(event::ResolveLiveScriptActorReference(
              static_cast<std::uint16_t>(reference)) == expected);
  }
}

TEST_CASE("Native queue helper preserves owned destinations and busy staging",
          "[event-vm-live][event-queue-init]") {
  for (const auto destination :
       {event::EventBuffer::kScript, event::EventBuffer::kChained}) {
    const LiveVmFixture fixture;
    const auto start = MountScript({0x55});
    auto& host = event::LiveScriptVm();
    host.SetEventId(1);
    if (destination == event::EventBuffer::kChained) {
      REQUIRE(host.AllocateChainedEventBuffer());
    }
    REQUIRE(event::QueueEventScriptLoad(destination, host));
    auto& request = PendingRequest();
    void* const owner_buffer = request.vram;
    CHECK(host.EventFile() == event::EventFileStatus::kLoading);
    CHECK_FALSE(event::QueueEventScriptLoad(destination, host));
    CHECK(PendingRequest().vram == owner_buffer);
    CheckMountedByte(0x55, start.generation);
    CompleteSyntheticFile(0xAA);
    host.ReleaseEventFile();
    if (destination == event::EventBuffer::kChained) {
      REQUIRE(host.AdoptChainedEvent());
    }
    CheckMountedByte(0xAA, start.generation + 1);
  }
}

namespace {
void CheckInitializedLiveScriptState(event::ScriptVmHost& host,
                                     const Content_DiscQueueSlot& request) {
  CHECK(request.state == Content_DiscRequestState_Free);
  CHECK(host.EventFile() == event::EventFileStatus::kNone);
  CHECK(host.Phase() == event::PlaybackPhase::kIdle);
  CHECK_FALSE(host.StartConfirmed());
  CHECK_FALSE(host.MainThreadFinished());
  CHECK_FALSE(host.ChainedEventLoaded());
  CHECK_FALSE(host.SubroutineReturn().Present());
}

void CheckInitializedLiveThread(event::ScriptVmHost& host, int thread) {
  CHECK_FALSE(host.ThreadCursor(thread).Present());
  CHECK(host.WaitFrames(thread) == 0);
}
}  // namespace

TEST_CASE("Native opcode initialization cancels staging and resets all threads",
          "[event-vm-live][event-queue-init]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x55});
  auto& host = event::LiveScriptVm();
  REQUIRE(host.AllocateChainedEventBuffer());
  REQUIRE(host.QueueEventFileLoad(1, event::EventBuffer::kChained));
  auto& request = PendingRequest();
  host.SetPhase(event::PlaybackPhase::kRunning);
  host.SetStartConfirmed(true);
  host.SetMainThreadFinished(true);
  host.SetChainedEventLoaded(true);
  host.SetSubroutineReturn(start);
  for (int thread = 0; thread < 4; ++thread) {
    host.SetThreadCursor(thread, start);
    host.SetWaitFrames(thread, 17);
  }
  REQUIRE(event::InitializeLiveScriptOpcodeTable());
  CheckInitializedLiveScriptState(host, request);
  for (int thread = 0; thread < 4; ++thread) {
    CheckInitializedLiveThread(host, thread);
  }
}

TEST_CASE("Native button store bindings preserve selectors and input bytes",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      for (const int operand :
           {0, 511, 1023, 0x7FFF, 0x8000, 0x81FF, 0x83FF, 0xFFFF}) {
        CheckLiveButtonStore(operand, guarded, skip);
      }
    }
  }
}

TEST_CASE("Native cutscene begin bindings preserve music bits and forced yield",
          "[event-vm-live][control-opcode]") {
  for (const int skip : {0, 2}) {
    for (const bool guarded : {false, true}) {
      CheckLiveCutsceneBegin(guarded, skip);
    }
  }
}

TEST_CASE("Native cutscene end bindings preserve skip snapshots and targets",
          "[event-vm-live][control-opcode]") {
  for (const bool guarded : {false, true}) {
    for (const int previous : {0, 1, 2, 3, 128, 255}) {
      for (const int displacement : {-32, -1, 3, 31}) {
        CheckLiveCutsceneEnd({previous, displacement, guarded, true});
      }
      CheckLiveCutsceneEnd({previous, 0, guarded, false});
    }
  }
}

TEST_CASE("Native saved return binding preserves missing and stale locations",
          "[event-vm-live][control-opcode]") {
  for (const bool guarded : {false, true}) {
    for (const int mode : {0, 1, 2}) {
      const LiveVmFixture fixture;
      const auto start = MountScript({0xE8, 0xE3, 4, 0xE6});
      Event_kOpcodeHandlers[0xE8] =
          guarded ? NativeReturnSlotBridge
                  : Event_ScriptReturnToSavedAddressOpcode;
      Event_kOpcodeHandlers[0xE3] = Event_CommandSetTickSpeedOpcode;
      Event_kOpcodeHandlers[0xE6] = Event_CommandStopScriptContextOpcode;
      Event_kOpcodeSizes[0xE3] = 2;
      State_tickSpeed = 2;
      auto& host = event::LiveScriptVm();
      if (mode != 0) {
        host.SetSubroutineReturn(
            {start.region, 3, start.generation + (mode == 2 ? 1 : 0)});
      }
      host.SetThreadCursor(0, start);
      CHECK_FALSE(event::ExecuteLiveScriptThread(0));
      CHECK_FALSE(host.ThreadCursor(0).Present());
      CHECK(State_tickSpeed == (mode == 0 ? 4 : 2));
    }
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
  Event_kOpcodeHandlers[0x42] = Event_ScriptJumpForwardOpcode;
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
  Event_kSkipOpcodeHandlers[0x42] = Event_ScriptJumpForwardOpcode;
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
  Event_kOpcodeHandlers[0xE1] = Event_ScriptSetLoopCounterOpcode;
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
  Event_kOpcodeHandlers[0xE1] = Event_ScriptReturnToSavedAddressOpcode;
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
