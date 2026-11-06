#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Event/ScriptDialogCommands.hpp"
#include "Event/ScriptSources.hpp"
#include "Event/ScriptVm.hpp"

namespace {
namespace event = VS::Event;
using Bytes = std::vector<std::uint8_t>;
using Location = event::ScriptLocation;
using Region = event::ScriptRegion;
using Flow = event::ScriptFlow;
using Kind = event::ScriptFlowKind;
using Refusal = event::ScriptVmRefusal;
using Handler = Flow (*)(const event::ScriptInstruction&, event::ScriptVmHost&);

class RecordingHost final : public event::ScriptVmHost {
 public:
  std::array<Bytes, 4> regions;
  std::array<std::uint64_t, 4> generations{};
  mutable std::vector<event::ScriptRegion> region_reads;
  std::array<std::uint8_t, 256> sizes{};
  std::function<Flow(Location, int)> dispatch;
  std::vector<std::pair<Refusal, int>> refusals;
  std::vector<std::string> calls;
  std::vector<std::pair<Location, int>> instructions;
  Bytes palette_command = Bytes(8);
  Bytes sound_marker{0x82, 0x05, 0, 0};
  std::array<std::int16_t, 2> timer_modes{11, 22};
  std::array<std::int16_t, 4> timer_scopes{33, 44, 55, 66};
  std::array<std::int16_t, 4> pitches{0, -100, 200, -300};
  event::PlaybackPhase phase = event::PlaybackPhase::kIdle;
  event::EventSource source = event::EventSource::kEventFile;
  event::CutsceneSkip skip = event::CutsceneSkip::kNone;
  event::EventFileStatus file = event::EventFileStatus::kNone;
  event::EventBuffer queued_buffer = event::EventBuffer::kScript;
  bool queue_succeeds = true;
  bool buffers_allocate = true;
  bool chained_allocates = true;
  bool adoption_succeeds = true;
  bool owns_chained = false;
  bool door_binding_succeeds = true;
  int door_bindings = 0;
  event::ScriptSourceMounts* source_mounts = nullptr;
  event::RoomEventLease current_room;
  std::int16_t event_id = 0;
  std::int16_t room_change = 0;
  bool confirmed = false;
  bool main_finished = false;
  bool chained = false;
  bool camera_restore = false;
  bool scanline = false;
  bool backdrop = false;
  bool menu_transition = false;
  bool ambient_suspended = false;
  bool forced = false;
  bool stop_music = false;
  bool weapon_pending = false;
  bool selection_pending = false;
  bool effect_pending = false;
  std::array<Location, event::kScriptThreadCount> cursors{};
  std::array<std::int16_t, event::kScriptThreadCount> waits{};
  std::array<std::uint8_t, event::kLoopCounterCount> loops{};
  std::array<event::EasingCurve, event::kCustomEasingCurveCount> curves{};
  std::array<Location, event::kTextBoxFrameStyleCount> styles{};
  std::array<Location, event::kVibrationPatternSlots> patterns{};
  std::array<bool, event::kTextBoxCount> text_waits{};
  std::array<bool, event::kTextBoxCount> text_open{};
  std::array<bool, event::kScriptControlledActorCount> controlled{};
  std::array<bool, event::kScriptControlledActorCount> actors{};
  std::array<std::uint8_t, 1024> flags{};
  mutable std::vector<int> flag_reads;
  std::vector<std::pair<int, std::uint8_t>> flag_writes;
  Location return_to;
  Location strings;
  std::int8_t cached = 0;
  std::uint32_t buttons = 0;
  bool start = false;
  std::uint8_t pressed = 0;
  int previous = 0;
  int inventory_count = 0;
  int inventory_item = 0;
  std::tuple<int, int, int> timer{};
  int tick_speed = 0;
  bool rewards_pending = false;
  bool action_unlocked = false;
  int unlocked_action = -1;
  int disposition_count = 0;
  int model_result = 1;
  int requested_model = -1;
  std::vector<std::pair<event::SpecialOperation, int>> special;
  std::uint8_t camera_completion = 0;
  std::vector<Bytes> palette_passes;
  std::vector<event::TextureMove> moves;
  std::vector<int> reset_actors;
  std::vector<int> idle_actors;
  std::vector<int> skipped_threads;

  RecordingHost() { sizes.fill(1); }

  void Refuse(Refusal refusal, int value) override {
    refusals.emplace_back(refusal, value);
  }
  void NoteInvalidOpcode(const event::ScriptInstruction&) override {
    calls.emplace_back("InvalidOpcode");
  }
  void NoteUndefinedSpecialOperation(int) override {
    calls.emplace_back("UndefinedSpecial");
  }
  void NoteCutsceneSkip(int thread) override {
    skipped_threads.push_back(thread);
  }
  std::span<const std::uint8_t> Region(
      event::ScriptRegion region) const override {
    region_reads.push_back(region);
    if (source_mounts != nullptr) {
      return source_mounts->Borrow(region).value();
    }
    return regions.at(static_cast<std::size_t>(region));
  }
  std::uint64_t RegionGeneration(event::ScriptRegion region) const override {
    if (source_mounts != nullptr) {
      return source_mounts->Lease(region).value().Generation();
    }
    return generations.at(static_cast<std::size_t>(region));
  }
  std::span<const std::uint8_t> OpcodeSizes() const override { return sizes; }
  Flow RunOpcode(Location location, int thread) override {
    instructions.emplace_back(location, thread);
    return dispatch(location, thread);
  }
  std::span<const std::uint8_t> PaletteResetCommand() const override {
    return palette_command;
  }
  std::span<const std::uint8_t> SoundRestoreMarker() const override {
    return sound_marker;
  }
  std::span<const std::int16_t> SceneTimerModes() const override {
    return timer_modes;
  }
  std::span<const std::int16_t> SceneTimerScopes() const override {
    return timer_scopes;
  }
  std::span<const std::int16_t> CameraPitchPresets() const override {
    return pitches;
  }
  event::PlaybackPhase Phase() const override { return phase; }
  void SetPhase(event::PlaybackPhase value) override { phase = value; }
  bool StartConfirmed() const override { return confirmed; }
  void SetStartConfirmed(bool value) override { confirmed = value; }
  event::EventSource Source() const override { return source; }
  void SetSource(event::EventSource value) override { source = value; }
  std::int16_t EventId() const override { return event_id; }
  void SetEventId(std::int16_t value) override { event_id = value; }
  bool MainThreadFinished() const override { return main_finished; }
  void SetMainThreadFinished(bool value) override { main_finished = value; }
  bool ChainedEventLoaded() const override { return chained; }
  void SetChainedEventLoaded(bool value) override { chained = value; }
  std::int16_t RoomChangeState() const override { return room_change; }
  void SetRoomChangeState(std::int16_t value) override { room_change = value; }
  bool FieldCameraRestoreScripted() const override { return camera_restore; }
  void SetFieldCameraRestoreScripted(bool value) override {
    camera_restore = value;
  }
  bool ScanlineOverlayEnabled() const override { return scanline; }
  void SetScanlineOverlayEnabled(bool value) override { scanline = value; }
  void SetScriptBackdropEnabled(bool value) override { backdrop = value; }
  bool MenuTransitionActive() const override { return menu_transition; }
  void SetMenuTransitionActive(bool value) override { menu_transition = value; }
  void SetAmbientSoundSuspended(bool value) override {
    ambient_suspended = value;
  }
  Location ThreadCursor(int thread) const override {
    return cursors.at(static_cast<std::size_t>(thread));
  }
  void SetThreadCursor(int thread, Location value) override {
    cursors.at(static_cast<std::size_t>(thread)) = value;
  }
  std::int16_t WaitFrames(int thread) const override {
    return waits.at(static_cast<std::size_t>(thread));
  }
  void SetWaitFrames(int thread, std::int16_t value) override {
    waits.at(static_cast<std::size_t>(thread)) = value;
  }
  Location SubroutineReturn() const override { return return_to; }
  void SetSubroutineReturn(Location value) override { return_to = value; }
  std::uint8_t LoopCounter(int counter) const override {
    return loops.at(static_cast<std::size_t>(counter));
  }
  void SetLoopCounter(int counter, std::uint8_t value) override {
    loops.at(static_cast<std::size_t>(counter)) = value;
  }
  std::int8_t CachedFlag() const override { return cached; }
  void SetCachedFlag(std::int8_t value) override { cached = value; }
  event::CutsceneSkip SkipState() const override { return skip; }
  void SetSkipState(event::CutsceneSkip value) override { skip = value; }
  bool SkipForced() const override { return forced; }
  void SetSkipForced(bool value) override { forced = value; }
  bool StopsMusicOnSkip() const override { return stop_music; }
  void SetStopsMusicOnSkip(bool value) override { stop_music = value; }
  void SetDialogStrings(Location value) override { strings = value; }
  void SetTextBoxFrameStyle(int style, Location value) override {
    styles.at(static_cast<std::size_t>(style)) = value;
  }
  void SetVibrationPattern(int pattern, Location value) override {
    patterns.at(static_cast<std::size_t>(pattern)) = value;
  }
  event::EasingCurve CustomEasingCurve(int curve) const override {
    return curves.at(static_cast<std::size_t>(curve));
  }
  void SetCustomEasingCurve(int curve, event::EasingCurve value) override {
    curves.at(static_cast<std::size_t>(curve)) = value;
  }
  void SetTextBoxAwaitingClose(int box, bool value) override {
    text_waits.at(static_cast<std::size_t>(box)) = value;
  }
  void SetWeaponTransitionPending(bool value) override {
    weapon_pending = value;
  }
  void SetTextSelectionActive(bool value) override {
    selection_pending = value;
  }
  void SetEffectWaitPending(bool value) override { effect_pending = value; }
  void CancelImageTweens() override { calls.emplace_back("CancelTweens"); }
  void CaptureEventMusicSlot() override { calls.emplace_back("CaptureMusic"); }
  bool ScriptControlledActor(int actor) const override {
    return controlled.at(static_cast<std::size_t>(actor));
  }
  void ClearScriptControlledActors() override { controlled.fill(false); }
  bool BindRoomDoorSource() override {
    ++door_bindings;
    if (!door_binding_succeeds) {
      return false;
    }
    return source_mounts == nullptr ||
           source_mounts
               ->MountRoom(event::ScriptRegion::kDoorEvents, current_room)
               .has_value();
  }
  event::EventFileStatus EventFile() const override { return file; }
  bool QueueEventFileLoad(int id, event::EventBuffer destination) override {
    CHECK(id == event_id);
    calls.emplace_back("QueueFile");
    if (!queue_succeeds) {
      return false;
    }
    queued_buffer = destination;
    file = event::EventFileStatus::kLoading;
    return true;
  }
  void ReleaseEventFile() override {
    REQUIRE(file != event::EventFileStatus::kLoading);
    file = event::EventFileStatus::kNone;
    calls.emplace_back("ReleaseFile");
  }
  void ClearEventFile() override {
    file = event::EventFileStatus::kNone;
    calls.emplace_back("ClearFile");
  }
  void CancelEventFile() override {
    file = event::EventFileStatus::kNone;
    calls.emplace_back("CancelFile");
  }
  bool AllocateChainedEventBuffer() override {
    calls.emplace_back("AllocateChained");
    if (chained_allocates) {
      owns_chained = true;
    }
    return chained_allocates;
  }
  bool AdoptChainedEvent() override {
    calls.emplace_back("AdoptChained");
    if (adoption_succeeds) {
      owns_chained = false;
    }
    return adoption_succeeds;
  }
  void DiscardChainedEventBuffer() override {
    REQUIRE(file != event::EventFileStatus::kLoading);
    owns_chained = false;
    calls.emplace_back("DiscardChained");
  }
  bool AllocateEventBuffers() override {
    calls.emplace_back("AllocateBuffers");
    return buffers_allocate;
  }
  std::uint8_t Flag(int flag) const override {
    flag_reads.push_back(flag);
    return flags.at(static_cast<std::size_t>(flag));
  }
  void SetFlag(int flag, std::uint8_t value) override {
    flags.at(static_cast<std::size_t>(flag)) = value;
    flag_writes.emplace_back(flag, value);
  }
  std::uint32_t HeldButtons() const override { return buttons; }
  void ClearHeldButtons() override { buttons = 0; }
  bool StartPressed() const override { return start; }
  std::uint8_t PressedButtonsLowByte() const override { return pressed; }
  int PreviousButtons() const override { return previous; }
  int CountInventoryItem(int item) override {
    inventory_item = item;
    return inventory_count;
  }
  void StartSceneTimer(int mode, int scope, int duration) override {
    timer = {mode, scope, duration};
    calls.emplace_back("StartTimer");
  }
  void ResetSceneTimer() override { calls.emplace_back("ResetTimer"); }
  void SetTickSpeed(int value) override { tick_speed = value; }
  bool DismissTextBox(int box) override {
    return text_open.at(static_cast<std::size_t>(box));
  }
  bool UpdateRewardSequence() override { return rewards_pending; }
  void BeginEventRewardResults() override {
    calls.emplace_back("BeginRewards");
  }
  void RunEndingAndReturnToTitle() override { calls.emplace_back("Ending"); }
  bool IsActionUnlocked(int) override { return action_unlocked; }
  void UnlockAction(int action) override {
    unlocked_action = action;
    action_unlocked = true;
  }
  int CountActorsByDisposition(int disposition) override {
    CHECK(disposition == 4);
    return disposition_count;
  }
  int FindActorByModelId(int model) override {
    requested_model = model;
    return model_result;
  }
  void ApplySpecialOperation(event::SpecialOperation operation,
                             int argument) override {
    special.emplace_back(operation, argument);
  }
  void InitializeEventCamera() override {
    calls.emplace_back("InitializeCamera");
  }
  void FreeEventCamera() override { calls.emplace_back("FreeCamera"); }
  void UpdateEventCameraOrigin() override {
    calls.emplace_back("CameraOrigin");
  }
  void RestoreEventFieldCamera() override {
    calls.emplace_back("RestoreCamera");
  }
  void UpdateEventCameraTracks() override {
    calls.emplace_back("CameraTracks");
  }
  void UpdateEventScreenTransforms() override {
    calls.emplace_back("ScreenTransforms");
  }
  void ApplyEventCameraState() override { calls.emplace_back("ApplyCamera"); }
  std::uint8_t EventCameraCompletionMask() override {
    return camera_completion;
  }
  void UpdateImageTweens() override { calls.emplace_back("ImageTweens"); }
  void UpdateWeatherTween() override { calls.emplace_back("WeatherTween"); }
  void DrawScanlineOverlay() override { calls.emplace_back("Scanlines"); }
  void TransformPalettes(event::ScriptCommand command) override {
    palette_passes.emplace_back(command.begin(), command.end());
  }
  void MoveTextureRectangle(const event::TextureMove& move) override {
    moves.push_back(move);
  }
  void BeginSkipFadeOut() override { calls.emplace_back("Fade"); }
  void DisableDisplay() override { calls.emplace_back("DisableDisplay"); }
  void ReturnAllActorsToIdle() override { calls.emplace_back("ActorsIdle"); }
  void ResetActorMovement(int actor) override { reset_actors.push_back(actor); }
  bool ActorPresent(int actor) override {
    return actors.at(static_cast<std::size_t>(actor));
  }
  void PlayActorIdleAnimation(int actor) override {
    idle_actors.push_back(actor);
  }
  void StopEventMusic() override { calls.emplace_back("StopMusic"); }
  void StopAllSfx() override { calls.emplace_back("StopSfx"); }
  void StopScriptVibration() override { calls.emplace_back("StopVibration"); }
  void RestartRoomEffectSound() override { calls.emplace_back("RoomSound"); }
};

Location At(std::uint32_t offset, Region region = Region::kEventScript,
            std::uint64_t generation = 0) {
  return {.region = region, .offset = offset, .generation = generation};
}

Flow Run(Handler handler, RecordingHost& host, const Bytes& bytes,
         int thread = 0, Location location = At(0)) {
  return handler({.bytes = bytes, .location = location, .thread = thread},
                 host);
}

void Half(Bytes& bytes, std::size_t at, std::uint16_t value) {
  bytes.at(at) = static_cast<std::uint8_t>(value);
  bytes.at(at + 1) = static_cast<std::uint8_t>(value >> 8);
}

Bytes FlagCommand(std::uint8_t operation, int first, int second) {
  return {0,
          static_cast<std::uint8_t>(operation | (first >> 6)),
          static_cast<std::uint8_t>(((first & 63) << 2) | (second >> 8)),
          static_cast<std::uint8_t>(second),
          6,
          0};
}

void Header(RecordingHost& host) {
  Bytes& bytes = host.regions[1];
  bytes.assign(128, 0);
  Half(bytes, 2, 80);
  Half(bytes, 4, 32);
  Half(bytes, 6, 48);
  Half(bytes, 34, 32);
  Half(bytes, 46, 36);
  Half(bytes, 48, 48);
  Half(bytes, 62, 52);
}
}  // namespace

TEST_CASE("VM instructions reject every truncated operand span", "[event-vm]") {
  const std::array commands{
      std::pair{event::ScriptBeginSkippableCutscene, 2},
      std::pair{event::ScriptEndCutsceneAndJump, 3},
      std::pair{event::ScriptWaitForButtonAndYield, 2},
      std::pair{event::ScriptBranchOnFlagImmediate, 6},
      std::pair{event::ScriptBranchOnFlagPair, 6},
      std::pair{event::ScriptJumpForward, 3},
      std::pair{event::ScriptCacheFlagValue, 3},
      std::pair{event::ScriptBranchOnCachedFlag, 4},
      std::pair{event::ScriptSetLoopCounter, 3},
      std::pair{event::ScriptLoopBackward, 4},
      std::pair{event::ScriptApplyImmediateFlagOperation, 4},
      std::pair{event::ScriptApplyFlagPairOperation, 4},
      std::pair{event::ScriptStoreInventoryItemPresence, 5},
      std::pair{event::ScriptStoreButtonState, 3},
      std::pair{event::ScriptConfigureTimer, 7},
      std::pair{event::ScriptWaitForButtonOrTimeout, 3},
      std::pair{event::ScriptConfigureEasingCurve, 2},
      std::pair{event::CommandSpecialOperation, 3},
      std::pair{event::CommandWaitFrames, 2},
      std::pair{event::CommandSetTickSpeed, 2},
      std::pair{event::CommandWaitScriptContext, 2},
      std::pair{event::CommandStartScriptContext, 4},
      std::pair{event::CommandLoadNextEvent, 3},
  };
  for (const auto& [handler, length] : commands) {
    for (int size = 0; size < length; ++size) {
      RecordingHost host;
      CHECK(Run(handler, host, Bytes(static_cast<std::size_t>(size))).kind ==
            Kind::kAdvance);
      REQUIRE(host.refusals.size() == 1);
      CHECK(host.refusals[0] == std::pair{Refusal::kShortInstruction, size});
      CHECK(host.calls.empty());
      CHECK(host.flag_writes.empty());
    }
  }
}

namespace {
void CheckFlagBranches(RecordingHost& host, int left, int right) {
  host.flags[1000] = static_cast<std::uint8_t>(left);
  host.flags[513] = static_cast<std::uint8_t>(right);
  const std::array matches{left <= right, left >= right, left == right,
                           left != right, left<right, left> right};
  for (std::size_t op = 0; op < matches.size(); ++op) {
    const auto operation = static_cast<std::uint8_t>(op << 4);
    const auto expected =
        matches[op] ? Flow::Of(Kind::kAdvance) : Flow::JumpBy(6);
    CHECK(Run(event::ScriptBranchOnFlagPair, host,
              FlagCommand(operation, 1000, 513)) == expected);
    CHECK(Run(event::ScriptBranchOnFlagImmediate, host,
              FlagCommand(operation, 1000, right)) == expected);
  }
}

void CheckImmediateArithmetic(RecordingHost& host, int initial, int operand) {
  const std::array operations{0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70};
  const std::array expected{initial + operand,
                            initial - operand,
                            initial * operand,
                            operand == 0 ? -1 : initial / operand,
                            operand == 0 ? initial : initial % operand,
                            initial & operand,
                            initial | operand,
                            operand};
  for (std::size_t op = 0; op < operations.size(); ++op) {
    host.flags[777] = static_cast<std::uint8_t>(initial);
    CHECK(Run(event::ScriptApplyImmediateFlagOperation, host,
              FlagCommand(static_cast<std::uint8_t>(operations[op]), 777,
                          operand))
              .kind == Kind::kAdvance);
    CHECK(host.flags[777] == static_cast<std::uint8_t>(expected[op]));
  }
}  // namespace

TEST_CASE("VM flag branches preserve signed comparisons and branch direction",
          "[event-vm]") {
  RecordingHost host;
  const std::array values{0, 1, 127, 128, 255};
  for (int left : values) {
    for (int right : values) {
      CheckFlagBranches(host, left, right);
    }
  }
}

TEST_CASE("VM immediate flag arithmetic uses bytes and console zero division",
          "[event-vm]") {
  RecordingHost host;
  for (int initial : {0, 1, 127, 128, 255}) {
    for (int operand : {0, 1, 2, 127, 255}) {
      CheckImmediateArithmetic(host, initial, operand);
    }
  }
}
}  // namespace

TEST_CASE("VM paired flag arithmetic preserves aliasing and asymmetric writes",
          "[event-vm]") {
  RecordingHost host;
  for (int operation :
       {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90}) {
    host.flags[900] = 23;
    host.flags[513] = 7;
    const std::array expected{30, 16, 161, 3, 2, 7, 23, 7, 16, 7};
    Run(event::ScriptApplyFlagPairOperation, host,
        FlagCommand(static_cast<std::uint8_t>(operation), 900, 513));
    CHECK(host.flags[900] ==
          expected.at(static_cast<std::size_t>(operation >> 4)));
    CHECK(host.flags[513] == (operation == 0x90 ? 23 : 7));
    host.flags[900] = 23;
    Run(event::ScriptApplyFlagPairOperation, host,
        FlagCommand(static_cast<std::uint8_t>(operation), 900, 900));
    const std::array aliases{46, 0, 17, 1, 0, 23, 23, 23, 0, 23};
    CHECK(host.flags[900] ==
          aliases.at(static_cast<std::size_t>(operation >> 4)));
  }
  host.flag_reads.clear();
  Run(event::ScriptApplyImmediateFlagOperation, host,
      FlagCommand(0x70, 900, 45));
  CHECK(host.flag_reads.empty());
  Run(event::ScriptApplyFlagPairOperation, host, FlagCommand(0xF0, 900, 513));
  CHECK(host.flag_reads.empty());
}

TEST_CASE("VM loop counters reject invalid slots and retain byte wraparound",
          "[event-vm]") {
  RecordingHost host;
  for (int counter : {16, 63, 254}) {
    Run(event::ScriptSetLoopCounter, host,
        {0, static_cast<std::uint8_t>(counter), 9});
    CHECK(Run(event::ScriptLoopBackward, host,
              {0, static_cast<std::uint8_t>(counter), 7, 0})
              .kind == Kind::kAdvance);
  }
  CHECK(host.refusals.size() == 6);
  Run(event::ScriptSetLoopCounter, host, {0, 15, 2});
  CHECK(Run(event::ScriptLoopBackward, host, {0, 15, 7, 0}) ==
        Flow::JumpBy(-7));
  CHECK(Run(event::ScriptLoopBackward, host, {0, 15, 7, 0}).kind ==
        Kind::kAdvance);
  CHECK(host.loops[15] == 0);
  CHECK(Run(event::ScriptLoopBackward, host, {0, 15, 7, 0}) ==
        Flow::JumpBy(-7));
  CHECK(host.loops[15] == 255);
  Run(event::ScriptSetLoopCounter, host, {0, 255, 9});
  CHECK(Run(event::ScriptLoopBackward, host, {0, 255, 0xFE, 0xFF}) ==
        Flow::JumpBy(2));
  CHECK(host.refusals.size() == 6);
}

TEST_CASE("VM button waits initialize before consuming buttons", "[event-vm]") {
  RecordingHost host;
  host.buttons = 4;
  CHECK(Run(event::ScriptWaitForButtonOrTimeout, host, {0, 4, 3}, 2).kind ==
        Kind::kHold);
  CHECK(host.waits[2] == 3);
  CHECK(host.buttons == 4);
  CHECK(Run(event::ScriptWaitForButtonOrTimeout, host, {0, 4, 3}, 2).kind ==
        Kind::kAdvance);
  CHECK(host.waits[2] == 0);
  CHECK(host.buttons == 0);
}

TEST_CASE("VM frame waits expire precisely and reject invalid threads",
          "[event-vm]") {
  RecordingHost host;
  for (Kind expected : {Kind::kHold, Kind::kHold, Kind::kAdvance}) {
    CHECK(Run(event::CommandWaitFrames, host, {0, 2}, 1).kind == expected);
  }
  for (int thread : {-1, 4, 255}) {
    Run(event::CommandWaitFrames, host, {0, 2}, thread);
    CHECK(host.refusals.back() == std::pair{Refusal::kScriptThread, thread});
  }
  CHECK(Run(event::CommandWaitFrames, host, {0, 0}, 1).kind == Kind::kHold);
  CHECK(Run(event::CommandWaitFrames, host, {0, 0}, 1).kind == Kind::kHold);
}

TEST_CASE("VM timers use bounded mode and scope tables and packed durations",
          "[event-vm]") {
  RecordingHost host;
  Run(event::ScriptConfigureTimer, host, {0, 1, 3, 1, 0x81, 0x23, 0x45});
  CHECK(host.timer == std::tuple{22, 66, static_cast<int>(0x81234500U)});
  Run(event::ScriptConfigureTimer, host, {0, 1, 3, 2, 0, 0, 0});
  Run(event::ScriptConfigureTimer, host, {0, 1, 4, 1, 0, 0, 0});
  CHECK(host.refusals == std::vector{std::pair{Refusal::kTimerMode, 2},
                                     std::pair{Refusal::kTimerScope, 4}});
  Run(event::ScriptConfigureTimer, host, {0, 0, 255, 255, 0, 0, 0});
  CHECK(host.calls == std::vector<std::string>{"StartTimer", "ResetTimer"});
}

TEST_CASE("VM script headers bind table-relative entries", "[event-vm]") {
  RecordingHost host;
  Header(host);
  CHECK(event::InitializeEventScriptData(At(0), host));
  CHECK(host.cursors[0] == At(16));
  CHECK(host.strings == At(80));
  CHECK(host.styles[9] == At(64));
  CHECK(host.styles[15] == At(68));
  CHECK_FALSE(host.styles[10].Present());
  CHECK(host.patterns[0] == At(96));
  CHECK(host.patterns[7] == At(100));
  CHECK(host.refusals.empty());
}

TEST_CASE("VM short headers clear previously bound views", "[event-vm]") {
  RecordingHost host;
  Header(host);
  REQUIRE(event::InitializeEventScriptData(At(0), host));
  host.regions[1].resize(7);
  CHECK_FALSE(event::InitializeEventScriptData(At(0), host));
  CHECK_FALSE(host.cursors[0].Present());
  CHECK_FALSE(host.strings.Present());
  CHECK(std::ranges::none_of(host.styles, &Location::Present));
  CHECK(std::ranges::none_of(host.patterns, &Location::Present));
  REQUIRE(host.refusals.size() == 1);
  CHECK(host.refusals[0].first == Refusal::kScriptHeader);
}

TEST_CASE(
    "VM header bindings refuse offsets and partial tables past the region",
    "[event-vm]") {
  RecordingHost host;
  Header(host);
  Half(host.regions[1], 2, 128);
  Half(host.regions[1], 4, 127);
  Half(host.regions[1], 48, 255);
  CHECK_FALSE(event::InitializeEventScriptData(At(0), host));
  CHECK_FALSE(host.strings.Present());
  CHECK(std::ranges::none_of(host.styles, &Location::Present));
  CHECK_FALSE(host.patterns[0].Present());
  CHECK_FALSE(host.patterns[7].Present());
  CHECK_FALSE(host.cursors[0].Present());
  CHECK(host.refusals.size() == 9);
}

namespace {
void ResetDirtyEvent(RecordingHost& host) {
  Header(host);
  host.flags.fill(77);
  host.waits.fill(9);
  host.cursors.fill(At(7));
  host.curves.fill({.family = 3, .power = -4});
  host.text_waits.fill(true);
  host.loops.fill(11);
  host.forced = host.main_finished = host.scanline = host.camera_restore = true;
  host.menu_transition = host.weapon_pending = host.selection_pending = true;
  host.effect_pending = true;
  host.room_change = 3;
  host.skip = event::CutsceneSkip::kSkipping;
  REQUIRE(event::InitializeEventScriptData(At(0), host));
}
}  // namespace

TEST_CASE("VM event initialization resets local flags waits and curves",
          "[event-vm]") {
  RecordingHost host;
  ResetDirtyEvent(host);
  CHECK(host.flags[15] == 77);
  CHECK(host.flags[32] == 77);
  CHECK(host.flags[event::kSkipActiveFlag] == 0);
  CHECK(std::all_of(host.flags.begin() + 16, host.flags.begin() + 32,
                    [](auto flag) { return flag == 0; }));
  CHECK(host.waits == std::array<std::int16_t, 4>{});
  CHECK(host.curves == std::array<event::EasingCurve, 4>{});
  CHECK(host.text_waits == std::array<bool, 8>{});
  CHECK(host.loops[0] == 11);
}

TEST_CASE("VM event initialization clears transitions and per-event waits",
          "[event-vm]") {
  RecordingHost host;
  ResetDirtyEvent(host);
  CHECK_FALSE((host.forced || host.main_finished || host.scanline ||
               host.camera_restore));
  CHECK_FALSE((host.menu_transition || host.weapon_pending ||
               host.selection_pending || host.effect_pending));
  CHECK(host.room_change == 0);
  CHECK(host.skip == event::CutsceneSkip::kNone);
  CHECK(host.calls ==
        std::vector<std::string>{"CancelTweens", "CaptureMusic", "ClearFile"});
}

TEST_CASE("VM dispatch distinguishes hold yield stop finish and relative jumps",
          "[event-vm]") {
  for (Kind kind :
       {Kind::kHold, Kind::kAdvanceAndYield, Kind::kStop, Kind::kFinish}) {
    RecordingHost host;
    host.regions[1].resize(20);
    host.sizes[0] = 3;
    host.cursors[1] = At(5);
    host.dispatch = [kind](Location, int) { return Flow::Of(kind); };
    CHECK(event::ExecuteScriptThread(event::ScriptThreadId::kContext1, host) ==
          (kind == Kind::kFinish));
    const auto expected = kind == Kind::kAdvanceAndYield ? At(8)
                          : kind == Kind::kStop          ? Location{}
                                                         : At(5);
    CHECK(host.cursors[1] == expected);
    CHECK(host.instructions.size() == 1);
  }
  RecordingHost host;
  host.regions[1].resize(20);
  host.cursors[0] = At(5);
  host.dispatch = [](Location location, int) {
    return location.offset == 5 ? Flow::JumpBy(-3) : Flow::Of(Kind::kHold);
  };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK(host.cursors[0] == At(2));
  CHECK(host.instructions.size() == 2);
}

TEST_CASE("VM dispatch stops invalid jumps", "[event-vm]") {
  for (int displacement : {-6, 15, std::numeric_limits<int>::max()}) {
    RecordingHost host;
    host.regions[1].resize(20);
    host.cursors[0] = At(5);
    host.dispatch = [displacement](Location, int) {
      return Flow::JumpBy(displacement);
    };
    CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
    CHECK_FALSE(host.cursors[0].Present());
    REQUIRE(host.refusals.size() == 1);
    CHECK(host.refusals[0].first == Refusal::kJumpOutsideScript);
  }
}

TEST_CASE("VM dispatch stops before a truncated instruction", "[event-vm]") {
  RecordingHost host;
  host.regions[1].resize(3);
  host.cursors[0] = At(1);
  host.sizes[0] = 3;
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK(host.instructions.empty());
  CHECK(host.refusals.back().first == Refusal::kCursorOutsideScript);
}

TEST_CASE("VM dispatch stops on a zero-sized advancing opcode", "[event-vm]") {
  RecordingHost host;
  host.regions[1].resize(3);
  host.cursors[0] = At(1);
  host.sizes[0] = 0;
  host.dispatch = [](Location, int) { return Flow::Of(Kind::kAdvance); };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK_FALSE(host.cursors[0].Present());
  CHECK(host.refusals.back().first == Refusal::kZeroLengthOpcode);
}

TEST_CASE("VM dispatch refuses an in-region self-jump at the exact budget",
          "[event-vm][event-vm-budget]") {
  RecordingHost host;
  host.regions[1].resize(1);
  host.cursors[0] = At(0);
  std::size_t steps = 0;
  host.dispatch = [&steps](Location, int) {
    if (++steps > event::kScriptDispatchBudget) {
      throw std::runtime_error("VM exceeded its dispatch budget");
    }
    return Flow::JumpBy(0);
  };
  CHECK_NOTHROW(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK(steps == event::kScriptDispatchBudget);
  CHECK_FALSE(host.cursors[0].Present());
  CHECK(host.refusals ==
        std::vector{std::pair{Refusal::kDispatchBudget,
                              static_cast<int>(event::kScriptDispatchBudget)}});
}

TEST_CASE("VM context continuation refuses offset wrap before publication",
          "[event-vm][event-vm-budget]") {
  RecordingHost host;
  host.cursors[1] = At(17);
  const Location source = At(std::numeric_limits<std::uint32_t>::max() - 3);
  CHECK(Run(event::CommandStartScriptContext, host, {0, 1, 0, 0}, 0, source)
            .kind == Kind::kStop);
  CHECK(host.cursors[1] == At(17));
  CHECK(host.refusals ==
        std::vector{std::pair{Refusal::kJumpOutsideScript, 4}});
}

TEST_CASE("VM subroutine continuation refuses offset wrap before saving",
          "[event-vm][event-vm-budget]") {
  RecordingHost host;
  host.return_to = At(17);
  const Location source = At(std::numeric_limits<std::uint32_t>::max() - 5);
  CHECK(
      Run(event::CommandSpecialOperation, host, {0, 16, 0, 0, 0, 0}, 0, source)
          .kind == Kind::kStop);
  CHECK(host.return_to == At(17));
  CHECK(host.refusals ==
        std::vector{std::pair{Refusal::kJumpOutsideScript, 6}});
}

TEST_CASE("VM permits all terminal flows on the last budgeted instruction",
          "[event-vm][event-vm-budget]") {
  for (Kind terminal :
       {Kind::kHold, Kind::kAdvanceAndYield, Kind::kStop, Kind::kFinish}) {
    RecordingHost host;
    host.regions[1].resize(2);
    host.cursors[0] = At(0);
    std::size_t steps = 0;
    host.dispatch = [&steps, terminal](Location, int) {
      return ++steps == event::kScriptDispatchBudget ? Flow::Of(terminal)
                                                     : Flow::JumpBy(0);
    };
    CHECK(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host) ==
          (terminal == Kind::kFinish));
    CHECK(steps == event::kScriptDispatchBudget);
    const Location expected = terminal == Kind::kStop              ? Location{}
                              : terminal == Kind::kAdvanceAndYield ? At(1)
                                                                   : At(0);
    CHECK(host.cursors[0] == expected);
    CHECK(host.refusals.empty());
  }
}

TEST_CASE("VM dispatch budgets renew independently for each thread and frame",
          "[event-vm][event-vm-budget]") {
  RecordingHost host;
  host.regions[1].resize(1);
  host.cursors.fill(At(0));
  std::array<std::size_t, event::kScriptThreadCount> steps{};
  host.dispatch = [&steps](Location, int thread) {
    return ++steps.at(thread) % event::kScriptDispatchBudget == 0
               ? Flow::Of(Kind::kHold)
               : Flow::JumpBy(0);
  };
  for (std::size_t frame = 1; frame <= 2; ++frame) {
    for (int thread : {0, 3}) {
      CHECK_FALSE(event::ExecuteScriptThread(
          static_cast<event::ScriptThreadId>(thread), host));
      CHECK(steps.at(thread) == frame * event::kScriptDispatchBudget);
      CHECK(host.cursors.at(thread) == At(0));
    }
  }
  CHECK(host.refusals.empty());
}

TEST_CASE("VM cross-region jumps cannot renew a thread dispatch budget",
          "[event-vm][event-vm-budget]") {
  RecordingHost host;
  host.regions[1].resize(1);
  host.regions[2].resize(1);
  host.cursors[0] = At(0);
  std::size_t steps = 0;
  host.dispatch = [&steps](Location location, int) {
    if (++steps > event::kScriptDispatchBudget) {
      throw std::runtime_error("VM renewed its budget across a region jump");
    }
    return Flow::JumpTo(At(0, location.region == Region::kEventScript
                                  ? Region::kDoorEvents
                                  : Region::kEventScript));
  };
  CHECK_NOTHROW(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK(steps == event::kScriptDispatchBudget);
  CHECK_FALSE(host.cursors[0].Present());
  CHECK(host.refusals ==
        std::vector{std::pair{Refusal::kDispatchBudget,
                              static_cast<int>(event::kScriptDispatchBudget)}});
}

TEST_CASE("VM continuation arithmetic preserves the last representable offset",
          "[event-vm][event-vm-budget]") {
  RecordingHost host;
  constexpr auto kLastOffset = std::numeric_limits<std::uint32_t>::max();
  host.generations[1] = 9;
  host.generations[3] = 11;
  CHECK(Run(event::CommandStartScriptContext, host, {0, 1, 0, 0}, 0,
            At(kLastOffset - 4, Region::kEventScript, 9))
            .kind == Kind::kJumpBy);
  CHECK(host.cursors[1] == At(kLastOffset, Region::kEventScript, 9));
  CHECK(Run(event::CommandSpecialOperation, host, {0, 16, 0, 0, 0, 0}, 0,
            At(kLastOffset - 6, Region::kEventScript, 9)) ==
        Flow::JumpTo(At(0, Region::kSubroutine, 11)));
  CHECK(host.return_to == At(kLastOffset, Region::kEventScript, 9));
  CHECK(host.refusals.empty());
}

TEST_CASE("VM thread contexts start wait release and reject invalid indices",
          "[event-vm]") {
  RecordingHost host;
  CHECK(Run(event::CommandStartScriptContext, host, {0, 3, 12, 0}, 0, At(20)) ==
        Flow::JumpBy(12));
  CHECK(host.cursors[3] == At(24));
  CHECK(Run(event::CommandWaitScriptContext, host, {0, 3}).kind == Kind::kHold);
  CHECK(Run(event::CommandWaitScriptContext, host, {0, 0x83}).kind ==
        Kind::kAdvance);
  CHECK_FALSE(host.cursors[3].Present());
  CHECK(Run(event::CommandWaitScriptContext, host, {0, 3}).kind ==
        Kind::kAdvance);
  Run(event::CommandStartScriptContext, host, {0, 4, 12, 0});
  CHECK(host.refusals.back() == std::pair{Refusal::kScriptThread, 4});
}

TEST_CASE("VM subroutines share typed return locations", "[event-vm]") {
  RecordingHost host;
  CHECK(Run(event::CommandSpecialOperation, host, {0, 16, 0}, 0, At(7)) ==
        Flow::JumpTo(At(0, Region::kSubroutine)));
  CHECK(host.return_to == At(13));
  CHECK(Run(event::ScriptReturnToSavedAddress, host, {0}) ==
        Flow::JumpTo(At(13)));
}

TEST_CASE("VM skip completion restores the three palettes and texture",
          "[event-vm]") {
  RecordingHost host;
  host.regions[1] = {0, 0x82, 5, 0, 0, 0x0F};
  event::FinishSkippedCutscene(At(0), host);
  CHECK(host.calls.empty());
  REQUIRE(host.palette_passes.size() == 3);
  CHECK(host.palette_passes[0][1] == 0x08);
  CHECK(host.palette_passes[1][1] == 0x18);
  CHECK(host.palette_passes[2][1] == 0x28);
  REQUIRE(host.moves.size() == 1);
  CHECK(host.moves[0] == event::TextureMove{470, 384, 18, 36, 16, 400});
  CHECK(host.refusals.empty());
}

TEST_CASE("VM skip sound scans stop at the end opcode or invalid extent",
          "[event-vm]") {
  RecordingHost host;
  host.regions[1] = {0x0F};
  event::FinishSkippedCutscene(At(0), host);
  CHECK(host.calls == std::vector<std::string>{"RoomSound"});
  host.regions[1] = {0x82, 5};
  host.sizes[0x82] = 0;
  event::FinishSkippedCutscene(At(0), host);
  CHECK(host.refusals.back().first == Refusal::kUnboundedSkipScan);
}

namespace {
void SkipMainThread(RecordingHost& host) {
  host.regions[1].resize(4);
  host.cursors[0] = At(0);
  host.skip = event::CutsceneSkip::kSkippable;
  host.start = host.stop_music = host.forced = true;
  host.waits.fill(5);
  host.text_waits.fill(true);
  host.actors[0] = host.actors[16] = true;
  host.dispatch = [](Location, int) { return Flow::Of(Kind::kHold); };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
}
}  // namespace

TEST_CASE("VM main-thread skip clears only the original waits", "[event-vm]") {
  RecordingHost host;
  SkipMainThread(host);
  CHECK(host.skip == event::CutsceneSkip::kSkipping);
  CHECK_FALSE(host.forced);
  CHECK(host.waits == std::array<std::int16_t, 4>{});
  CHECK(host.text_waits ==
        std::array{false, false, false, false, true, true, true, true});
  CHECK(host.flags[event::kSkipActiveFlag] == 1);
}

TEST_CASE("VM main-thread skip idles actors stops sound and fades the display",
          "[event-vm]") {
  RecordingHost host;
  SkipMainThread(host);
  CHECK(host.idle_actors == std::vector{0, 16});
  CHECK(host.calls == std::vector<std::string>{"StopMusic", "StopSfx",
                                               "StopVibration", "Fade",
                                               "DisableDisplay"});
  CHECK(host.skipped_threads == std::vector{0});
}

TEST_CASE(
    "VM playback requires confirmation before initializing camera and actors",
    "[event-vm]") {
  RecordingHost host;
  Header(host);
  CHECK(event::UpdateEventPlayback(host) == 0);
  CHECK(event::RequestEventStart(host) == 1);
  CHECK_FALSE(host.confirmed);
  host.phase = event::PlaybackPhase::kRequested;
  CHECK(event::UpdateEventPlayback(host) == 0);
  event::RequestEventStart(host);
  CHECK(event::UpdateEventPlayback(host) == 2);
  CHECK(host.phase == event::PlaybackPhase::kStarting);
  CHECK_FALSE(host.confirmed);
  CHECK(host.calls ==
        std::vector<std::string>{"InitializeCamera", "ActorsIdle"});
}

TEST_CASE("VM file playback waits for loading and adopts chained data",
          "[event-vm]") {
  RecordingHost host;
  Header(host);
  host.phase = event::PlaybackPhase::kStarting;
  CHECK(event::UpdateEventPlayback(host) == 2);
  CHECK(host.phase == event::PlaybackPhase::kLoading);
  CHECK(host.file == event::EventFileStatus::kLoading);
  CHECK(event::UpdateEventPlayback(host) == 2);
  host.file = event::EventFileStatus::kReady;
  host.chained = true;
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kRunning);
  CHECK(host.cursors[0] == At(16));
  CHECK_FALSE(host.chained);
  const auto released = std::ranges::find(host.calls, "ReleaseFile");
  const auto adopted = std::ranges::find(host.calls, "AdoptChained");
  CHECK(released < adopted);
}

TEST_CASE(
    "VM room and door playback validate header locations and missing files",
    "[event-vm]") {
  RecordingHost host;
  Header(host);
  host.source = event::EventSource::kRoomEntryScript;
  host.phase = event::PlaybackPhase::kStarting;
  event::UpdateEventPlayback(host);
  CHECK(host.cursors[0] == At(16));
  host.regions[2] = host.regions[1];
  Half(host.regions[2], 0, 0);
  host.source = event::EventSource::kDoorScript;
  host.phase = event::PlaybackPhase::kStarting;
  event::UpdateEventPlayback(host);
  CHECK(host.cursors[0] == At(16, Region::kDoorEvents));
  for (int selector : {-1, 64, 32767}) {
    host.event_id = static_cast<std::int16_t>(selector);
    host.phase = event::PlaybackPhase::kStarting;
    event::UpdateEventPlayback(host);
    CHECK_FALSE(host.cursors[0].Present());
  }
  host.phase = event::PlaybackPhase::kLoading;
  host.file = event::EventFileStatus::kNone;
  event::UpdateEventPlayback(host);
  CHECK(host.refusals.back().first == Refusal::kMissingEventFile);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
}

TEST_CASE("VM finished playback waits for the camera before releasing actors",
          "[event-vm]") {
  RecordingHost host;
  host.regions[1].resize(5);
  host.cursors[0] = At(0);
  host.cursors[1] = At(1);
  host.phase = event::PlaybackPhase::kRunning;
  host.controlled[0] = host.controlled[16] = true;
  host.scanline = true;
  host.dispatch = [](Location, int thread) {
    return Flow::Of(thread == 0 ? Kind::kFinish : Kind::kStop);
  };
  event::UpdateEventPlayback(host);
  CHECK(host.main_finished);
  CHECK(host.phase == event::PlaybackPhase::kRunning);
  CHECK(host.reset_actors.empty());
  CHECK_FALSE(host.cursors[1].Present());
  host.camera_completion = 255;
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK_FALSE(host.main_finished);
  CHECK(host.reset_actors == std::vector{0, 16});
  CHECK(host.calls.back() == "FreeCamera");
}

TEST_CASE("VM easing curves decode every packed mode and signed power",
          "[event-vm]") {
  RecordingHost host;
  for (unsigned operand = 0; operand < 256; ++operand) {
    Run(event::ScriptConfigureEasingCurve, host,
        {0, static_cast<std::uint8_t>(operand)});
    const auto curve = host.curves[operand >> 6];
    CHECK(curve.family == ((operand >> 4) & 3));
    const int power = static_cast<int>(operand & 15);
    CHECK(curve.power == (power >= 8 ? power - 16 : power));
  }
  for (int mode : {-1, 8, 32767}) {
    CHECK(event::EvaluateInterpolationCurve(static_cast<std::int16_t>(mode), 3,
                                            4, host) == 3072);
    CHECK(host.refusals.back() == std::pair{Refusal::kEasingMode, mode});
  }
  CHECK(event::EvaluateInterpolationCurve(0, 100, 0, host) == -1);
  CHECK(event::EvaluateInterpolationCurve(0, -1, 0, host) == 1);
  CHECK(event::EvaluateInterpolationCurve(0, 99, 4, host) == 4096);
}

TEST_CASE("VM easing families preserve original fixed-point samples",
          "[event-vm]") {
  RecordingHost host;
  const std::array<std::int16_t, 7> frames{-10, 0, 1, 7, 15, 30, 100};
  for (auto frame : frames) {
    const int position = std::min<int>(frame, 30);
    const std::array expected{
        position * 4096 / 30, VS::Core::Math::Sine(position * 1024 / 30),
        (VS::Core::Math::Cosine(position * 2048 / 30 + 2048) + 4096) >> 1,
        VS::Core::Math::Sine(position * 1024 / 30 - 1024) + 4096};
    for (std::int16_t mode = 0; mode < 4; ++mode) {
      CHECK(event::EvaluateInterpolationCurve(mode, frame, 30, host) ==
            expected[static_cast<std::size_t>(mode)]);
    }
  }
  host.curves[0] = {.family = 0, .power = 2};
  CHECK(event::EvaluateInterpolationCurve(4, 1, 2, host) == 256);
  host.curves[0].power = -2;
  CHECK(event::EvaluateInterpolationCurve(4, 1, 2, host) == 3840);
}

TEST_CASE(
    "VM actor references preserve every selector nibble without addresses",
    "[event-vm]") {
  RecordingHost host;
  host.model_result = 7;
  CHECK(event::ResolveScriptActorReference(0x0123, host) == 7);
  CHECK(host.requested_model == 0x123);
  CHECK(event::ResolveScriptActorReference(0x1025, host) == 5);
  for (unsigned nibble = 2; nibble < 16; ++nibble) {
    const auto reference = static_cast<std::uint16_t>((nibble << 12) | 0x123);
    CHECK(event::ResolveScriptActorReference(reference, host) == reference);
  }
}

TEST_CASE("VM special operations preserve reward phases", "[event-vm]") {
  RecordingHost host;
  CHECK(Run(event::CommandSpecialOperation, host, {0, 1, 0}).kind ==
        Kind::kHold);
  CHECK(host.flags[0xB6] == 3);
  CHECK(host.menu_transition);
  host.rewards_pending = true;
  CHECK(Run(event::CommandSpecialOperation, host, {0, 1, 0}).kind ==
        Kind::kHold);
  host.rewards_pending = false;
  CHECK(Run(event::CommandSpecialOperation, host, {0, 1, 0}).kind ==
        Kind::kAdvance);
  CHECK_FALSE(host.menu_transition);
  CHECK(host.flags[0xB6] == 0);
  CHECK(host.calls == std::vector<std::string>{"BeginRewards", "Ending"});
}

TEST_CASE("VM special operations reject pitch presets past their table",
          "[event-vm]") {
  RecordingHost host;
  Run(event::CommandSpecialOperation, host, {0, 10, 3});
  REQUIRE(host.special.size() == 1);
  CHECK(host.special[0] ==
        std::pair{event::SpecialOperation::kSetCameraPitchPreset, -300});
  Run(event::CommandSpecialOperation, host, {0, 10, 4});
  CHECK(host.special.size() == 1);
  CHECK(host.refusals.back() == std::pair{Refusal::kPitchPreset, 4});
}

TEST_CASE(
    "VM event chaining and finish retain allocation queue and dialog ordering",
    "[event-vm]") {
  RecordingHost host;
  Run(event::CommandLoadNextEvent, host, {0, 0x34, 0x12});
  CHECK(host.event_id == 0x1234);
  CHECK(host.chained);
  CHECK(host.queued_buffer == event::EventBuffer::kChained);
  CHECK(host.calls == std::vector<std::string>{"AllocateChained", "QueueFile"});
  host.text_open[7] = true;
  CHECK(Run(event::CommandFinishEvent, host, {0}).kind == Kind::kHold);
  CHECK(host.tick_speed == 0);
  host.text_open[7] = false;
  CHECK(Run(event::CommandFinishEvent, host, {0}).kind == Kind::kFinish);
  CHECK(host.tick_speed == 2);
}

TEST_CASE("VM table initialization clears every runnable thread and wait",
          "[event-vm]") {
  RecordingHost host;
  host.cursors.fill(At(7));
  host.waits.fill(13);
  host.phase = event::PlaybackPhase::kRunning;
  host.ambient_suspended = host.confirmed = host.main_finished = true;
  host.room_change = 3;
  CHECK(event::InitializeScriptOpcodeTable(host));
  CHECK(host.cursors == std::array<Location, 4>{});
  CHECK(host.waits == std::array<std::int16_t, 4>{});
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK_FALSE((host.ambient_suspended || host.confirmed || host.main_finished));
  CHECK(host.room_change == 0);
  CHECK(host.calls ==
        std::vector<std::string>{"DiscardChained", "AllocateBuffers"});
}

TEST_CASE("VM simple opcodes retain their control results and diagnostics",
          "[event-vm]") {
  RecordingHost host;
  CHECK(Run(event::ScriptNoOperation, host, {0}).kind == Kind::kAdvance);
  CHECK(Run(event::ScriptYieldFrame, host, {0}).kind == Kind::kAdvanceAndYield);
  CHECK(Run(event::ScriptReportInvalidOpcode, host, {0}).kind ==
        Kind::kAdvance);
  CHECK(host.calls == std::vector<std::string>{"InvalidOpcode"});
  CHECK(Run(event::ScriptWaitForEventFile, host, {0}).kind == Kind::kAdvance);
  host.file = event::EventFileStatus::kLoading;
  CHECK(Run(event::ScriptWaitForEventFile, host, {0}).kind == Kind::kHold);
  host.file = event::EventFileStatus::kReady;
  CHECK(Run(event::ScriptWaitForEventFile, host, {0}).kind == Kind::kAdvance);
}

TEST_CASE("VM missing file startup terminates its loading wait",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.phase = event::PlaybackPhase::kLoading;
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK(host.refusals == std::vector{std::pair{Refusal::kMissingEventFile, 0}});
}

TEST_CASE("VM refuses to run a truncated starting script",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.phase = event::PlaybackPhase::kStarting;
  host.source = event::EventSource::kRoomEntryScript;
  host.regions[1].resize(7);
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK_FALSE(host.cursors[0].Present());
}

TEST_CASE("VM refuses to run a script with malformed header tables",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  Header(host);
  Half(host.regions[1], 48, 255);
  host.phase = event::PlaybackPhase::kStarting;
  host.source = event::EventSource::kRoomEntryScript;
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK_FALSE(host.cursors[0].Present());
  CHECK_FALSE(host.strings.Present());
  CHECK(std::ranges::none_of(host.patterns, &Location::Present));
}

TEST_CASE("VM startup queue refusal clears borrowed views and every cursor",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  Header(host);
  REQUIRE(event::InitializeEventScriptData(At(0), host));
  host.phase = event::PlaybackPhase::kStarting;
  host.queue_succeeds = false;
  host.return_to = At(20);
  host.waits.fill(9);
  CHECK(event::UpdateEventPlayback(host) == 2);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK(host.file == event::EventFileStatus::kNone);
  CHECK_FALSE(host.return_to.Present());
  CHECK_FALSE(host.strings.Present());
  CHECK(host.cursors == std::array<Location, 4>{});
  CHECK(host.waits == std::array<std::int16_t, 4>{});
  CHECK(host.refusals == std::vector{std::pair{Refusal::kEventFileQueue, 0}});
}

TEST_CASE("VM failed startup releases camera and actor control exactly once",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.phase = event::PlaybackPhase::kStarting;
  host.queue_succeeds = false;
  host.controlled[0] = host.controlled[16] = true;
  event::UpdateEventPlayback(host);
  CHECK(host.reset_actors == std::vector{0, 16});
  CHECK(
      std::ranges::none_of(host.controlled, [](bool value) { return value; }));
  CHECK(std::ranges::count(host.calls, "FreeCamera") == 1);
  const auto calls = host.calls;
  CHECK(event::UpdateEventPlayback(host) == 0);
  CHECK(host.calls == calls);
}

TEST_CASE("VM failed file reads release the request before chained storage",
          "[event-vm][event-vm-failure]") {
  for (bool chained : {false, true}) {
    RecordingHost host;
    Header(host);
    host.phase = event::PlaybackPhase::kLoading;
    host.file = event::EventFileStatus::kFailed;
    host.chained = host.owns_chained = chained;
    event::UpdateEventPlayback(host);
    CHECK(host.phase == event::PlaybackPhase::kIdle);
    CHECK(host.file == event::EventFileStatus::kNone);
    CHECK_FALSE((host.chained || host.owns_chained));
    CHECK(std::ranges::find(host.calls, "ReleaseFile") <
          std::ranges::find(host.calls, "DiscardChained"));
    CHECK(std::ranges::count(host.calls, "AdoptChained") == 0);
    CHECK(host.region_reads.empty());
    CHECK(host.refusals == std::vector{std::pair{Refusal::kEventFileRead, 0}});
  }
}

TEST_CASE("VM pending reads retain their destination and all loading state",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.phase = event::PlaybackPhase::kLoading;
  host.file = event::EventFileStatus::kLoading;
  host.chained = host.owns_chained = true;
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kLoading);
  CHECK(host.file == event::EventFileStatus::kLoading);
  CHECK((host.chained && host.owns_chained));
  CHECK(host.calls.empty());
  CHECK(host.refusals.empty());
}

TEST_CASE("VM adoption failure never starts or reads the previous main script",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  Header(host);
  const auto previous = host.regions[1];
  host.phase = event::PlaybackPhase::kLoading;
  host.file = event::EventFileStatus::kReady;
  host.chained = host.owns_chained = true;
  host.adoption_succeeds = false;
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK(host.regions[1] == previous);
  CHECK(host.region_reads.empty());
  CHECK_FALSE((host.chained || host.owns_chained));
  CHECK(host.calls == std::vector<std::string>{"ReleaseFile", "AdoptChained",
                                               "DiscardChained", "FreeCamera"});
  CHECK(host.refusals ==
        std::vector{std::pair{Refusal::kChainedEventAdoption, 0}});
}

TEST_CASE("VM completed malformed file headers do not enter running playback",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.phase = event::PlaybackPhase::kLoading;
  host.file = event::EventFileStatus::kReady;
  host.regions[1].resize(16);
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK(host.file == event::EventFileStatus::kNone);
  CHECK(host.cursors == std::array<Location, 4>{});
  CHECK(std::ranges::count(host.calls, "ReleaseFile") == 1);
  CHECK(host.refusals == std::vector{std::pair{Refusal::kScriptHeader, 0}});
}

TEST_CASE("VM chain allocation refusal preserves the selector and never queues",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.event_id = 17;
  host.chained_allocates = false;
  CHECK(Run(event::CommandLoadNextEvent, host, {0, 0x34, 0x12}).kind ==
        Kind::kStop);
  CHECK(host.event_id == 17);
  CHECK_FALSE((host.chained || host.owns_chained));
  CHECK(host.file == event::EventFileStatus::kNone);
  CHECK(host.calls == std::vector<std::string>{"AllocateChained"});
  CHECK(host.refusals ==
        std::vector{std::pair{Refusal::kEventBufferAllocation, 0x1234}});
}

TEST_CASE("VM chain queue refusal discards prepared storage",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.event_id = 17;
  host.queue_succeeds = false;
  CHECK(Run(event::CommandLoadNextEvent, host, {0, 0x34, 0x12}).kind ==
        Kind::kStop);
  CHECK(host.event_id == 17);
  CHECK_FALSE((host.chained || host.owns_chained));
  CHECK(host.file == event::EventFileStatus::kNone);
  CHECK(host.calls == std::vector<std::string>{"AllocateChained", "QueueFile",
                                               "DiscardChained"});
  CHECK(host.refusals ==
        std::vector{std::pair{Refusal::kEventFileQueue, 0x1234}});
}

TEST_CASE("VM chain queue refusal permits a successful retry",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.queue_succeeds = false;
  REQUIRE(Run(event::CommandLoadNextEvent, host, {0, 0x34, 0x12}).kind ==
          Kind::kStop);
  host.queue_succeeds = true;
  CHECK(Run(event::CommandLoadNextEvent, host, {0, 0x34, 0x12}).kind ==
        Kind::kAdvance);
  CHECK(host.event_id == 0x1234);
  CHECK((host.chained && host.owns_chained));
  CHECK(host.file == event::EventFileStatus::kLoading);
}

TEST_CASE("VM repeated chain commands preserve the outstanding request",
          "[event-vm][event-vm-failure]") {
  for (auto status :
       {event::EventFileStatus::kLoading, event::EventFileStatus::kReady,
        event::EventFileStatus::kFailed}) {
    RecordingHost host;
    host.event_id = 17;
    host.file = status;
    host.chained = host.owns_chained = true;
    CHECK(Run(event::CommandLoadNextEvent, host, {0, 0x34, 0x12}).kind ==
          Kind::kStop);
    CHECK(host.event_id == 17);
    CHECK(host.file == status);
    CHECK((host.chained && host.owns_chained));
    CHECK(host.calls.empty());
    CHECK(host.refusals == std::vector{std::pair{Refusal::kEventFileBusy, 17}});
  }
}

TEST_CASE("VM file wait stops on a read failure without advancing",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.file = event::EventFileStatus::kFailed;
  CHECK(Run(event::ScriptWaitForEventFile, host, {0}).kind == Kind::kStop);
  CHECK(host.refusals == std::vector{std::pair{Refusal::kEventFileRead, 0}});
  CHECK(host.calls.empty());
}

TEST_CASE("VM buffer initialization reports allocation refusal",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.buffers_allocate = false;
  host.cursors.fill(At(7));
  host.waits.fill(12);
  host.phase = event::PlaybackPhase::kRunning;
  CHECK_FALSE(event::InitializeScriptOpcodeTable(host));
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK(host.cursors == std::array<Location, 4>{});
  CHECK(host.waits == std::array<std::int16_t, 4>{});
  CHECK(host.refusals ==
        std::vector{std::pair{Refusal::kEventBufferAllocation,
                              static_cast<int>(event::kEventScriptBytes)}});
  host.buffers_allocate = true;
  CHECK(event::InitializeScriptOpcodeTable(host));
}

TEST_CASE("VM unknown event sources refuse rather than waiting forever",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  // An imported fixed-underlying enum can contain an unnamed value.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  host.source = static_cast<event::EventSource>(3);
  host.phase = event::PlaybackPhase::kStarting;
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK(host.refusals == std::vector{std::pair{Refusal::kEventSource, 3}});
}

TEST_CASE("VM interrupted startup cancels writes before discarding their owner",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  host.phase = event::PlaybackPhase::kStarting;
  host.file = event::EventFileStatus::kLoading;
  host.chained = host.owns_chained = true;
  event::UpdateEventPlayback(host);
  CHECK(host.phase == event::PlaybackPhase::kIdle);
  CHECK(host.file == event::EventFileStatus::kNone);
  CHECK_FALSE((host.chained || host.owns_chained));
  CHECK(host.calls ==
        std::vector<std::string>{"CancelFile", "DiscardChained", "FreeCamera"});
  CHECK(host.refusals == std::vector{std::pair{Refusal::kEventFileBusy, 0}});
}

TEST_CASE("VM direct load and header entry cannot discard a pending request",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  Header(host);
  host.file = event::EventFileStatus::kLoading;
  host.cursors[0] = At(20);
  CHECK_FALSE(event::QueueEventScriptLoad(event::EventBuffer::kScript, host));
  CHECK_FALSE(event::InitializeEventScriptData(At(0), host));
  CHECK(host.file == event::EventFileStatus::kLoading);
  CHECK(host.cursors[0] == At(20));
  CHECK(host.region_reads.empty());
  CHECK(host.calls.empty());
  CHECK(host.refusals == std::vector(2, std::pair{Refusal::kEventFileBusy, 0}));
}

TEST_CASE("VM repeat initialization cancels writes before replacing buffers",
          "[event-vm][event-vm-failure]") {
  RecordingHost host;
  Header(host);
  REQUIRE(event::InitializeEventScriptData(At(0), host));
  host.calls.clear();
  host.file = event::EventFileStatus::kLoading;
  host.chained = host.owns_chained = true;
  host.return_to = At(20);
  REQUIRE(event::InitializeScriptOpcodeTable(host));
  CHECK(host.file == event::EventFileStatus::kNone);
  CHECK_FALSE((host.chained || host.owns_chained));
  CHECK_FALSE(host.return_to.Present());
  CHECK_FALSE(host.strings.Present());
  CHECK(std::ranges::none_of(host.patterns, &Location::Present));
  CHECK(host.calls == std::vector<std::string>{"CancelFile", "DiscardChained",
                                               "AllocateBuffers"});
}

TEST_CASE("VM cached flag equality preserves every unsigned byte value",
          "[event-vm]") {
  RecordingHost host;
  for (int value : {0, 127, 128, 255}) {
    host.flags[511] = static_cast<std::uint8_t>(value);
    Run(event::ScriptCacheFlagValue, host, {0, 0xFF, 1});
    CHECK(Run(event::ScriptBranchOnCachedFlag, host,
              {0, static_cast<std::uint8_t>(value), 0xF9, 0xFF}) ==
          Flow::JumpBy(-7));
    CHECK(Run(event::ScriptBranchOnCachedFlag, host,
              {0, static_cast<std::uint8_t>(value ^ 1), 0xF9, 0xFF}) ==
          Flow::Of(Kind::kAdvance));
  }
  CHECK(Run(event::ScriptJumpForward, host, {0, 0xF9, 0xFF}) ==
        Flow::JumpBy(-7));
}

TEST_CASE("VM button and inventory stores retain their encoded flag selectors",
          "[event-vm]") {
  RecordingHost host;
  host.inventory_count = 23;
  Run(event::ScriptStoreInventoryItemPresence, host, {0, 0x34, 0x12, 0xFF, 3});
  CHECK(host.inventory_item == 0x1234);
  CHECK(host.flags[1023] == 23);
  host.previous = 0x3456;
  host.pressed = 0xA5;
  Run(event::ScriptStoreButtonState, host, {0, 0xFF, 3});
  CHECK(host.flags[1023] == 0x56);
  Run(event::ScriptStoreButtonState, host, {0, 0xFE, 0x83});
  CHECK(host.flags[1022] == 0xA5);
  CHECK(Run(event::ScriptWaitForButtonAndYield, host, {0, 4}).kind ==
        Kind::kHold);
  host.buttons = 4;
  CHECK(Run(event::ScriptWaitForButtonAndYield, host, {0, 4}).kind ==
        Kind::kAdvanceAndYield);
}

TEST_CASE("VM end-cutscene opcodes retain branches unless finishing a skip",
          "[event-vm]") {
  RecordingHost host;
  Run(event::ScriptBeginSkippableCutscene, host, {0, 0});
  CHECK(host.skip == event::CutsceneSkip::kSkippable);
  CHECK(host.stop_music);
  CHECK(Run(event::ScriptEndCutsceneAndJump, host, {0, 7, 0}) ==
        Flow::JumpBy(7));
  CHECK(host.skip == event::CutsceneSkip::kNone);
  host.forced = true;
  CHECK(Run(event::ScriptBeginSkippableCutscene, host, {0, 1}).kind ==
        Kind::kAdvanceAndYield);
  CHECK_FALSE(host.stop_music);
  host.skip = event::CutsceneSkip::kSkipping;
  host.regions[1] = {0x0F};
  CHECK(Run(event::ScriptEndCutscene, host, {0}).kind ==
        Kind::kAdvanceAndYield);
}

TEST_CASE("VM special unlock and actor-count operations store their results",
          "[event-vm]") {
  RecordingHost host;
  Run(event::CommandSpecialOperation, host, {0, 3, 41});
  CHECK(host.unlocked_action == 41);
  CHECK(host.flags[0xB1] == 1);
  Run(event::CommandSpecialOperation, host, {0, 3, 41});
  CHECK(host.flags[0xB1] == 0);
  host.disposition_count = 12;
  Run(event::CommandSpecialOperation, host, {0, 4, 0});
  CHECK(host.flags[0xB1] == 12);
  Run(event::CommandSpecialOperation, host, {0, 2, 0});
  CHECK(host.forced);
  Run(event::CommandSpecialOperation, host, {0, 255, 0});
  CHECK(host.calls.back() == "UndefinedSpecial");
}

TEST_CASE(
    "VM forward-only special operations retain their operation and argument",
    "[event-vm]") {
  RecordingHost host;
  for (int operation : {0, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 17}) {
    CHECK(Run(event::CommandSpecialOperation, host,
              {0, static_cast<std::uint8_t>(operation), 201})
              .kind == Kind::kAdvance);
    CHECK(host.special.back() ==
          std::pair{static_cast<event::SpecialOperation>(operation), 201});
  }
  host.source = event::EventSource::kDoorScript;
  host.event_id = 0x123;
  CHECK(event::PendingEventSelector(host) == 0x2123);
  CHECK(event::EventPlaybackState(host) == event::PlaybackPhase::kIdle);
  Run(event::CommandSetTickSpeed, host, {0, 17});
  CHECK(host.tick_speed == 17);
}

TEST_CASE("VM refuses a cursor after its region is replaced in place",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.regions[1] = {0x22};
  host.cursors[0] = {Region::kEventScript, 0};
  const auto* original_address = host.regions[1].data();
  host.regions[1][0] = 0x23;
  ++host.generations[1];
  REQUIRE(host.regions[1].data() == original_address);
  host.dispatch = [](Location, int) { return Flow::Of(Kind::kStop); };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK(host.instructions.empty());
  CHECK(host.region_reads.empty());
  CHECK(host.refusals ==
        std::vector<std::pair<Refusal, int>>{{Refusal::kStaleScriptRegion, 1}});
  CHECK_FALSE(host.cursors[0].Present());
}

TEST_CASE("VM headers preserve their mounted generation in every bound view",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  Header(host);
  host.generations[1] = 7;
  CHECK(event::InitializeEventScriptData(At(0, Region::kEventScript, 7), host));
  CHECK(host.cursors[0] == At(16, Region::kEventScript, 7));
  CHECK(host.strings == At(80, Region::kEventScript, 7));
  CHECK(host.styles[9] == At(64, Region::kEventScript, 7));
  CHECK(host.styles[15] == At(68, Region::kEventScript, 7));
  CHECK(host.patterns[0] == At(96, Region::kEventScript, 7));
  CHECK(host.patterns[7] == At(100, Region::kEventScript, 7));
  CHECK(host.refusals.empty());
}

TEST_CASE("VM stale headers clear views without reading the replacement",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  Header(host);
  REQUIRE(event::InitializeEventScriptData(At(0), host));
  host.region_reads.clear();
  ++host.generations[1];
  CHECK_FALSE(event::InitializeEventScriptData(At(0), host));
  CHECK(host.region_reads.empty());
  CHECK_FALSE(host.cursors[0].Present());
  CHECK_FALSE(host.strings.Present());
  CHECK(std::ranges::none_of(host.styles, &Location::Present));
  CHECK(std::ranges::none_of(host.patterns, &Location::Present));
  CHECK(host.refusals ==
        std::vector<std::pair<Refusal, int>>{{Refusal::kStaleScriptRegion, 1}});
}

TEST_CASE("VM relative jumps and yields retain a full-width generation",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.regions[1].resize(5);
  host.generations[1] = 0x100000001;
  host.cursors[0] = At(0, Region::kEventScript, 0x100000001);
  host.dispatch = [](Location location, int) {
    return location.offset == 0 ? Flow::JumpBy(2)
                                : Flow::Of(Kind::kAdvanceAndYield);
  };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK(host.cursors[0] == At(3, Region::kEventScript, 0x100000001));
  CHECK(host.instructions ==
        std::vector<std::pair<Location, int>>{
            {At(0, Region::kEventScript, 0x100000001), 0},
            {At(2, Region::kEventScript, 0x100000001), 0}});
  CHECK(host.refusals.empty());
}

TEST_CASE("VM generation checks do not discard the upper 32 bits",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.regions[1].resize(5);
  host.generations[1] = 0x100000001;
  host.cursors[0] = At(0, Region::kEventScript, 1);
  host.dispatch = [](Location, int) { return Flow::Of(Kind::kStop); };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK(host.instructions.empty());
  CHECK(host.region_reads.empty());
  REQUIRE(host.refusals.size() == 1);
  CHECK(host.refusals.front().first == Refusal::kStaleScriptRegion);
}

TEST_CASE("VM explicit jumps refuse stale destination generations",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.regions[1].resize(5);
  host.regions[2].resize(5);
  host.generations[2] = 2;
  host.cursors[0] = At(0);
  host.dispatch = [](Location, int) {
    return Flow::JumpTo(At(0, Region::kDoorEvents, 1));
  };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  REQUIRE(host.instructions.size() == 1);
  CHECK(host.region_reads == std::vector{Region::kEventScript});
  CHECK(host.refusals ==
        std::vector<std::pair<Refusal, int>>{{Refusal::kStaleScriptRegion, 2}});
}

TEST_CASE("VM subroutine calls and returns preserve source generations",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.generations[1] = 7;
  host.generations[3] = 9;
  CHECK(Run(event::CommandSpecialOperation, host, {0, 16, 0}, 0,
            At(7, Region::kEventScript, 7)) ==
        Flow::JumpTo(At(0, Region::kSubroutine, 9)));
  CHECK(host.return_to == At(13, Region::kEventScript, 7));
  CHECK(Run(event::ScriptReturnToSavedAddress, host, {0}) ==
        Flow::JumpTo(At(13, Region::kEventScript, 7)));
  ++host.generations[1];
  CHECK(Run(event::ScriptReturnToSavedAddress, host, {0}).kind == Kind::kStop);
  REQUIRE(host.refusals.size() == 1);
  CHECK(host.refusals.front().first == Refusal::kStaleScriptRegion);
}

TEST_CASE("VM started contexts inherit the instruction source generation",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.generations[1] = 7;
  CHECK(Run(event::CommandStartScriptContext, host, {0, 3, 12, 0}, 0,
            At(20, Region::kEventScript, 7)) == Flow::JumpBy(12));
  CHECK(host.cursors[3] == At(24, Region::kEventScript, 7));
  host.cursors[3] = {};
  ++host.generations[1];
  CHECK(Run(event::CommandStartScriptContext, host, {0, 3, 12, 0}, 0,
            At(20, Region::kEventScript, 7))
            .kind == Kind::kStop);
  CHECK_FALSE(host.cursors[3].Present());
}

TEST_CASE("VM skip scans refuse stale sources before reading replacement bytes",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.regions[1] = {0x82, 5, 0, 0, 0x0F};
  host.generations[1] = 1;
  event::FinishSkippedCutscene(At(0), host);
  CHECK(host.region_reads.empty());
  CHECK(host.refusals ==
        std::vector<std::pair<Refusal, int>>{{Refusal::kStaleScriptRegion, 1}});
}

TEST_CASE("VM source replacement during dispatch cannot retain the old cursor",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.regions[1] = {0};
  host.cursors[0] = At(0);
  host.dispatch = [&](Location, int) {
    ++host.generations[1];
    return Flow::Of(Kind::kHold);
  };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  REQUIRE(host.instructions.size() == 1);
  CHECK_FALSE(host.cursors[0].Present());
  CHECK(host.refusals ==
        std::vector<std::pair<Refusal, int>>{{Refusal::kStaleScriptRegion, 1}});
}

TEST_CASE("VM dispatch can explicitly jump to a freshly mounted source",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.regions[1] = {0};
  host.cursors[0] = At(0);
  host.dispatch = [&](Location location, int) {
    if (location.generation == 0) {
      ++host.generations[1];
      return Flow::JumpTo(At(0, Region::kEventScript, 1));
    }
    return Flow::Of(Kind::kStop);
  };
  CHECK_FALSE(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  CHECK(host.instructions ==
        std::vector<std::pair<Location, int>>{
            {At(0), 0}, {At(0, Region::kEventScript, 1), 0}});
  CHECK(host.refusals.empty());
}

TEST_CASE("VM completion does not reuse a source released by its final opcode",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  host.regions[1] = {0};
  host.cursors[0] = At(0);
  host.dispatch = [&](Location, int) {
    ++host.generations[1];
    return Flow::Of(Kind::kFinish);
  };
  CHECK(event::ExecuteScriptThread(event::ScriptThreadId::kMain, host));
  REQUIRE(host.instructions.size() == 1);
  CHECK(host.refusals.empty());
}

TEST_CASE("VM typed thread ids refuse invalid values before reading sources",
          "[event-vm]") {
  for (const int thread :
       {-1, event::kScriptThreadCount, std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()}) {
    RecordingHost host;
    CHECK(event::ExecuteScriptThread(static_cast<event::ScriptThreadId>(thread),
                                     host));
    CHECK(host.instructions.empty());
    CHECK(host.region_reads.empty());
    CHECK(host.refusals ==
          std::vector{std::pair{Refusal::kScriptThread, thread}});
  }
}

TEST_CASE("VM playback starts each script from its mounted generation",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  Header(host);
  host.generations[1] = 7;
  host.source = event::EventSource::kRoomEntryScript;
  host.phase = event::PlaybackPhase::kStarting;
  CHECK(event::UpdateEventPlayback(host) == 2);
  CHECK(host.cursors[0] == At(16, Region::kEventScript, 7));
  host.source = event::EventSource::kEventFile;
  host.phase = event::PlaybackPhase::kLoading;
  host.file = event::EventFileStatus::kReady;
  host.generations[1] = 8;
  CHECK(event::UpdateEventPlayback(host) == 2);
  CHECK(host.cursors[0] == At(16, Region::kEventScript, 8));
  host.regions[2] = host.regions[1];
  host.generations[2] = 9;
  host.source = event::EventSource::kDoorScript;
  host.event_id = 0;
  host.phase = event::PlaybackPhase::kStarting;
  CHECK(event::UpdateEventPlayback(host) == 2);
  CHECK(host.cursors[0] == At(16, Region::kDoorEvents, 9));
  CHECK(host.refusals.empty());
}

namespace {
void CheckDoorPlaybackStep(RecordingHost& host, event::PlaybackPhase phase,
                           int bindings) {
  CHECK(event::UpdateEventPlayback(host) == 2);
  CHECK(host.phase == phase);
  CHECK(host.door_bindings == bindings);
}

void CheckStoppedDoorStartup(const RecordingHost& host) {
  CHECK_FALSE(host.confirmed);
  CHECK_FALSE(host.return_to.Present());
  CHECK(std::ranges::all_of(host.cursors,
                            [](Location cursor) { return !cursor.Present(); }));
  CHECK(std::ranges::all_of(host.waits, [](int wait) { return wait == 0; }));
}

void RequireSourceChange(std::expected<void, event::ScriptSourceError> result) {
  REQUIRE(result.has_value());
}

void CheckDoorInstruction(RecordingHost& host, Location cursor,
                          std::uint64_t generation, std::uint8_t opcode) {
  CHECK(cursor == At(16, Region::kDoorEvents, generation));
  CHECK(host.Region(Region::kDoorEvents)[cursor.offset] == opcode);
}
}  // namespace

TEST_CASE("VM door binding failure stops startup before reading the old source",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  Header(host);
  host.regions[2] = host.regions[1];
  host.source = event::EventSource::kDoorScript;
  host.phase = event::PlaybackPhase::kStarting;
  host.door_binding_succeeds = false;
  host.cursors[0] = At(16, Region::kDoorEvents);
  host.return_to = At(16, Region::kDoorEvents);
  host.confirmed = true;
  host.waits.fill(4);
  CheckDoorPlaybackStep(host, event::PlaybackPhase::kIdle, 1);
  CHECK(host.region_reads.empty());
  CheckStoppedDoorStartup(host);
  CHECK(host.refusals ==
        std::vector<std::pair<Refusal, int>>{{Refusal::kDoorScript, 0}});
}

TEST_CASE("VM door source binding is not used for room entry or file startup",
          "[event-vm][event-vm-lifetime]") {
  RecordingHost host;
  Header(host);
  host.door_binding_succeeds = false;
  host.source = event::EventSource::kRoomEntryScript;
  host.phase = event::PlaybackPhase::kStarting;
  CheckDoorPlaybackStep(host, event::PlaybackPhase::kRunning, 0);
  host.source = event::EventSource::kEventFile;
  host.phase = event::PlaybackPhase::kStarting;
  CheckDoorPlaybackStep(host, event::PlaybackPhase::kLoading, 0);
  host.source = event::EventSource::kDoorScript;
  host.phase = event::PlaybackPhase::kStarting;
  CheckDoorPlaybackStep(host, event::PlaybackPhase::kIdle, 0);
  CHECK(host.refusals ==
        std::vector<std::pair<Refusal, int>>{{Refusal::kEventFileBusy, 0}});
}

TEST_CASE("VM keeps executing old room doors until a new door event starts",
          "[event-vm][event-vm-lifetime][script-sources]") {
  RecordingHost host;
  Header(host);
  auto doors = host.regions[1];
  doors[16] = 0x22;
  auto old_room = event::RoomEventSections::Copy({.doors = doors}).value();
  const std::weak_ptr<const event::RoomEventSections> old_owner = old_room;
  event::ScriptSourceMounts mounts;
  RequireSourceChange(mounts.MountRoom(Region::kDoorEvents, old_room));
  const auto old_generation =
      mounts.Lease(Region::kDoorEvents).value().Generation();
  old_room.reset();
  doors[16] = 0x33;
  host.current_room = event::RoomEventSections::Copy({.doors = doors}).value();
  host.source_mounts = &mounts;
  host.source = event::EventSource::kDoorScript;
  host.phase = event::PlaybackPhase::kRunning;
  host.cursors[0] = At(16, Region::kDoorEvents, old_generation);
  host.dispatch = [&](Location cursor, int) {
    CheckDoorInstruction(host, cursor, old_generation, 0x22);
    return Flow::Of(Kind::kHold);
  };
  RequireSourceChange(mounts.BeginUse());
  CheckDoorPlaybackStep(host, event::PlaybackPhase::kRunning, 0);
  CHECK_FALSE(old_owner.expired());
  host.phase = event::PlaybackPhase::kStarting;
  CheckDoorPlaybackStep(host, event::PlaybackPhase::kRunning, 1);
  CheckDoorInstruction(host, host.cursors[0], old_generation + 1, 0x33);
  CHECK_FALSE(old_owner.expired());
  RequireSourceChange(mounts.EndUse());
  CHECK(old_owner.expired());
  CHECK(host.refusals.empty());
}
