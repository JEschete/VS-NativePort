#include "Event/ScriptVmLive.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Event/ScriptActorCommandsLive.hpp"
#include "Event/ScriptEffectCommandsLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Battle/Rewards.h"
#include "Content/DiscLayout.h"
#include "Core/Diagnostics.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Event/Playback.h"
#include "Event/Script.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptEffects.h"
#include "Event/ScriptField.h"
#include "Event/ScriptScreen.h"
#include "Event/ScriptVm.h"
#include "Event/Weather.h"
#include "Field/Gimmicks.h"
#include "Input/ControllerLiveState.h"
#include "Menu/TextBox.h"
}

namespace VS::Event {
namespace {

using Buffer = std::vector<std::uint8_t>;
using MutableBuffer = std::shared_ptr<Buffer>;
using OpcodeHandler = std::intptr_t (*)(std::uint8_t*, std::int16_t);
constexpr std::size_t kSceneTimerModeCount = 2;
constexpr std::size_t kSceneTimerScopeCount = 4;
constexpr std::size_t kCameraPitchPresetCount = 4;

struct ControlBinding {
  OpcodeHandler legacy;
  ScriptControlOpcode typed;
};

// decomp: INITBTL.PRG _initScriptFunctionTable (12AC.c:69), BATTLE.PRG
// control handlers in 4A0A8.c and 573B8.c. Match the selected table's target.
constexpr std::array kControlBindings{
    ControlBinding{Event_ScriptReportInvalidOpcode, ScriptReportInvalidOpcode},
    ControlBinding{Event_ScriptNoOperation, ScriptNoOperation},
    ControlBinding{Event_ScriptYieldFrame, ScriptYieldFrame},
    ControlBinding{Event_ScriptWaitForEventFile, ScriptWaitForEventFile},
    ControlBinding{Event_ScriptBeginSkippableCutscene,
                   ScriptBeginSkippableCutscene},
    ControlBinding{Event_ScriptEndCutsceneAndJump, ScriptEndCutsceneAndJump},
    ControlBinding{Event_ScriptEndCutscene, ScriptEndCutscene},
    ControlBinding{Event_ScriptWaitForButtonAndYield,
                   ScriptWaitForButtonAndYield},
    ControlBinding{Event_ScriptBranchOnFlagImmediate,
                   ScriptBranchOnFlagImmediate},
    ControlBinding{Event_ScriptBranchOnFlagPair, ScriptBranchOnFlagPair},
    ControlBinding{Event_ScriptJumpForward, ScriptJumpForward},
    ControlBinding{Event_ScriptCacheFlagValue, ScriptCacheFlagValue},
    ControlBinding{Event_ScriptBranchOnCachedFlag, ScriptBranchOnCachedFlag},
    ControlBinding{Event_ScriptReturnToSavedAddress,
                   ScriptReturnToSavedAddress},
    ControlBinding{Event_ScriptSetLoopCounter, ScriptSetLoopCounter},
    ControlBinding{Event_ScriptLoopBackward, ScriptLoopBackward},
    ControlBinding{Event_ScriptApplyImmediateFlagOperation,
                   ScriptApplyImmediateFlagOperation},
    ControlBinding{Event_ScriptApplyFlagPairOperation,
                   ScriptApplyFlagPairOperation},
    ControlBinding{Event_ScriptStoreInventoryItemPresence,
                   ScriptStoreInventoryItemPresence},
    ControlBinding{Event_ScriptStoreButtonState, ScriptStoreButtonState},
    ControlBinding{Event_ScriptConfigureTimer, ScriptConfigureTimer},
    ControlBinding{Event_ScriptWaitForButtonOrTimeout,
                   ScriptWaitForButtonOrTimeout},
    ControlBinding{Event_ScriptConfigureEasingCurve,
                   ScriptConfigureEasingCurve},
    ControlBinding{Event_CommandSpecialOperation, CommandSpecialOperation},
    ControlBinding{Event_CommandWaitFrames, CommandWaitFrames},
    ControlBinding{Event_CommandSetTickSpeed, CommandSetTickSpeed},
    ControlBinding{Event_CommandWaitScriptContext, CommandWaitScriptContext},
    ControlBinding{Event_CommandStartScriptContext, CommandStartScriptContext},
    ControlBinding{Event_CommandStopScriptContext, CommandStopScriptContext},
    ControlBinding{Event_CommandLoadNextEvent, CommandLoadNextEvent},
    ControlBinding{Event_CommandFinishEvent, CommandFinishEvent},
};

ScriptControlOpcode NativeControl(OpcodeHandler handler) {
  for (const auto& binding : kControlBindings) {
    if (binding.legacy == handler) {
      return binding.typed;
    }
  }
  return nullptr;
}

struct OpcodeInvocation {
  ScriptInstruction instruction{};
  ScriptFlow result = ScriptFlow::Of(ScriptFlowKind::kStop);
  bool active = false;
  bool handled = false;
};

// 4A0A8.c / 573B8.c. Only scalar compatibility state remains in the module;
// source ownership, file destinations, return positions and cursors do not.
class LiveVmHost final : public ScriptVmHost {
 public:
  LiveVmHost() noexcept = default;
  mutable ScriptSourceMounts sources;
  RoomEventLease room;
  std::array<ScriptLocation, kScriptThreadCount> cursors{};
  ScriptLocation return_to;
  ScriptDataLease strings;
  std::array<ScriptDataLease, kTextBoxFrameStyleCount> frames{};
  std::array<ScriptDataLease, kVibrationPatternSlots> patterns{};
  std::array<EasingCurve, kCustomEasingCurveCount> curves{};
  OpcodeInvocation invocation;

  void Refuse(ScriptVmRefusal reason, int value) override {
    Core_Warnf("event", "Script VM refused: reason=%d value=%d",
               static_cast<int>(reason), value);
  }
  void NoteInvalidOpcode(const ScriptInstruction& instruction) override {
    Core_Warnf("event", "Unknown opcode: thread=%d region=%d offset=%u",
               instruction.thread,
               static_cast<int>(instruction.location.region),
               instruction.location.offset);
  }
  void NoteUndefinedSpecialOperation(int operation) override {
    Core_Warnf("event", "Undefined special operation: %d", operation);
  }
  void NoteCutsceneSkip(int thread) override {
    Core_Debugf("event", "Skip event=%d thread=%d", Event_eventId, thread);
  }
  std::span<const std::uint8_t> Region(ScriptRegion region) const override {
    const auto bytes = sources.Borrow(region);
    if (!bytes) {
      Core_Warnf("event", "Script source borrow refused: region=%d reason=%d",
                 static_cast<int>(region), static_cast<int>(bytes.error()));
      return {};
    }
    return *bytes;
  }
  std::uint64_t RegionGeneration(ScriptRegion region) const override {
    const auto source = sources.Lease(region);
    if (!source) {
      Core_Warnf("event", "Script source lease refused: region=%d reason=%d",
                 static_cast<int>(region), static_cast<int>(source.error()));
      return 0;
    }
    return source->Generation();
  }
  std::span<const std::uint8_t> OpcodeSizes() const override {
    return Event_kOpcodeSizes;
  }
  ScriptFlow RunOpcode(ScriptLocation location, int thread) override;
  std::span<const std::uint8_t> PaletteResetCommand() const override {
    constexpr std::size_t kCommandBytes = 8;
    return {
        reinterpret_cast<const std::uint8_t*>(Event_skipPaletteResetCommand),
        kCommandBytes};
  }
  std::span<const std::uint8_t> SoundRestoreMarker() const override {
    constexpr std::size_t kMarkerBytes = 4;
    return {
        reinterpret_cast<const std::uint8_t*>(Event_kSkipSoundRestoreMarker),
        kMarkerBytes};
  }
  std::span<const std::int16_t> SceneTimerModes() const override {
    return std::span{Event_kSceneTimerModes}.first<kSceneTimerModeCount>();
  }
  std::span<const std::int16_t> SceneTimerScopes() const override {
    return std::span{Event_kSceneTimerResetScopes}
        .first<kSceneTimerScopeCount>();
  }
  std::span<const std::int16_t> CameraPitchPresets() const override {
    return std::span{Event_kSpecialOperationTenArguments}
        .first<kCameraPitchPresetCount>();
  }
  PlaybackPhase Phase() const override {
    return static_cast<PlaybackPhase>(Event_playbackPhase);
  }
  void SetPhase(PlaybackPhase phase) override {
    Event_playbackPhase = static_cast<char>(phase);
  }
  bool StartConfirmed() const override { return Event_isStartConfirmed != 0; }
  void SetStartConfirmed(bool value) override {
    Event_isStartConfirmed = static_cast<char>(value);
  }
  EventSource Source() const override {
    return static_cast<EventSource>(Event_eventSource);
  }
  void SetSource(EventSource source) override {
    Event_eventSource = static_cast<std::uint16_t>(source);
  }
  std::int16_t EventId() const override { return Event_eventId; }
  void SetEventId(std::int16_t id) override { Event_eventId = id; }
  bool MainThreadFinished() const override {
    return Event_isMainThreadFinished != 0;
  }
  void SetMainThreadFinished(bool value) override {
    Event_isMainThreadFinished = value;
  }
  bool ChainedEventLoaded() const override {
    return Event_isChainedEventLoaded != 0;
  }
  void SetChainedEventLoaded(bool value) override {
    Event_isChainedEventLoaded = value;
  }
  std::int16_t RoomChangeState() const override {
    return Event_roomChangeState;
  }
  void SetRoomChangeState(std::int16_t state) override {
    Event_roomChangeState = state;
  }
  bool FieldCameraRestoreScripted() const override {
    return Event_isFieldCameraRestoreScripted != 0;
  }
  void SetFieldCameraRestoreScripted(bool value) override {
    Event_isFieldCameraRestoreScripted = value;
  }
  bool ScanlineOverlayEnabled() const override {
    return Event_isScanlineOverlayEnabled != 0;
  }
  void SetScanlineOverlayEnabled(bool value) override {
    Event_isScanlineOverlayEnabled = static_cast<char>(value);
  }
  void SetScriptBackdropEnabled(bool value) override {
    Event_isScriptBackdropEnabled = static_cast<char>(value);
  }
  bool MenuTransitionActive() const override {
    return Event_isMenuTransitionActive != 0;
  }
  void SetMenuTransitionActive(bool value) override {
    Event_isMenuTransitionActive = static_cast<char>(value);
  }
  void SetAmbientSoundSuspended(bool value) override {
    Event_isAmbientSoundSuspended = static_cast<char>(value);
  }
  ScriptLocation ThreadCursor(int thread) const override {
    return cursors.at(static_cast<std::size_t>(thread));
  }
  void SetThreadCursor(int thread, ScriptLocation cursor) override {
    cursors.at(static_cast<std::size_t>(thread)) = cursor;
  }
  std::int16_t WaitFrames(int thread) const override {
    return Event_threadWaitFrames[thread];
  }
  void SetWaitFrames(int thread, std::int16_t frames_left) override {
    Event_threadWaitFrames[thread] = frames_left;
  }
  ScriptLocation SubroutineReturn() const override { return return_to; }
  void SetSubroutineReturn(ScriptLocation location) override {
    return_to = location;
  }
  std::uint8_t LoopCounter(int counter) const override {
    return static_cast<std::uint8_t>(Event_loopCounters[counter]);
  }
  void SetLoopCounter(int counter, std::uint8_t value) override {
    Event_loopCounters[counter] = static_cast<char>(value);
  }
  std::int8_t CachedFlag() const override { return Event_cachedFlagValue; }
  void SetCachedFlag(std::int8_t value) override {
    Event_cachedFlagValue = value;
  }
  CutsceneSkip SkipState() const override {
    return static_cast<CutsceneSkip>(Event_skipState);
  }
  void SetSkipState(CutsceneSkip state) override {
    Event_skipState = static_cast<char>(state);
  }
  bool SkipForced() const override { return Event_isSkipForced != 0; }
  void SetSkipForced(bool value) override {
    Event_isSkipForced = static_cast<char>(value);
  }
  bool StopsMusicOnSkip() const override { return Event_stopsMusicOnSkip != 0; }
  void SetStopsMusicOnSkip(bool value) override {
    Event_stopsMusicOnSkip = static_cast<char>(value);
  }
  void SetDialogStrings(ScriptLocation location) override {
    BindData(strings, location);
  }
  void SetTextBoxFrameStyle(int style, ScriptLocation location) override {
    BindData(frames.at(static_cast<std::size_t>(style)), location);
  }
  void SetVibrationPattern(int pattern, ScriptLocation location) override {
    BindData(patterns.at(static_cast<std::size_t>(pattern)), location);
  }
  EasingCurve CustomEasingCurve(int curve) const override {
    return curves.at(static_cast<std::size_t>(curve));
  }
  void SetCustomEasingCurve(int curve, EasingCurve value) override {
    curves.at(static_cast<std::size_t>(curve)) = value;
  }
  void SetTextBoxAwaitingClose(int box, bool value) override {
    Event_isTextBoxAwaitingClose[box] = static_cast<char>(value);
  }
  void SetWeaponTransitionPending(bool value) override {
    Event_weaponTransitionPending = static_cast<char>(value);
  }
  void SetTextSelectionActive(bool value) override {
    Event_isTextSelectionActive = static_cast<char>(value);
  }
  void SetEffectWaitPending(bool value) override {
    Event_isEffectWaitPending = value;
  }
  void CancelImageTweens() override {
    Event_weatherTweenDuration = 0;
    for (auto& tween : std::span{Event_splashTweens}.first<3>()) {
      tween.durationFrames = 0;
    }
  }
  void CaptureEventMusicSlot() override {
    Event_musicSlot = static_cast<char>(Audio::LiveCurrentMusicSlot());
  }
  bool ScriptControlledActor(int actor) const override {
    return Event_scriptControlledActors[actor] != 0;
  }
  void ClearScriptControlledActors() override {
    std::fill_n(Event_scriptControlledActors, kScriptControlledActorCount, 0);
  }
  bool BindRoomDoorSource() override {
    const auto mounted = sources.MountRoom(ScriptRegion::kDoorEvents, room);
    if (!mounted) {
      Core_Warnf("event", "Door source binding refused: reason=%d",
                 static_cast<int>(mounted.error()));
    }
    return mounted.has_value();
  }
  EventFileStatus EventFile() const override;
  bool QueueEventFileLoad(int id, EventBuffer destination) override;
  void ReleaseEventFile() override;
  void CancelEventFile() override;
  void ClearEventFile() override;
  bool AllocateChainedEventBuffer() override;
  bool AdoptChainedEvent() override;
  void DiscardChainedEventBuffer() override;
  bool AllocateEventBuffers() override;
  std::uint8_t Flag(int flag) const override {
    return State::LiveScriptFlag(flag);
  }
  void SetFlag(int flag, std::uint8_t value) override {
    State::SetLiveScriptFlag(flag, value);
  }
  std::uint32_t HeldButtons() const override {
    return Input_buttonsPressed.allButtons;
  }
  void ClearHeldButtons() override { Input_buttonsPressed.allButtons = 0; }
  bool StartPressed() const override {
    return (Input_buttonsPressed.allButtons & PADstart) != 0;
  }
  std::uint8_t PressedButtonsLowByte() const override {
    return Input_buttonsPressed.ports[0].lowByte;
  }
  int PreviousButtons() const override { return Input_previousButtons; }
  int CountInventoryItem(int item) override {
    return Items::CountLiveInventoryItem(item);
  }
  void StartSceneTimer(int mode, int scope, int duration) override {
    State::StartLiveSceneTimer(mode, scope, duration);
  }
  void ResetSceneTimer() override { State::ResetLiveSceneTimer(); }
  void SetTickSpeed(int speed) override { State::SetLiveTickSpeed(speed); }
  bool DismissTextBox(int box) override {
    return Menu_DismissTextBox(box) != 0;
  }
  bool UpdateRewardSequence() override {
    return Battle_UpdateRewardSequence() != 0;
  }
  void BeginEventRewardResults() override { Battle_BeginEventRewardResults(); }
  void RunEndingAndReturnToTitle() override {
    Boot::RunEndingAndReturnToTitleLive();
  }
  bool IsActionUnlocked(int action) override {
    return Battle::LiveIsActionUnlocked(action) != 0;
  }
  void UnlockAction(int action) override { Battle::LiveUnlockAction(action); }
  int CountActorsByDisposition(int disposition) override {
    return Actors::CountLiveActorsByDisposition(disposition);
  }
  int FindActorByModelId(int model) override {
    return Actors::FindLiveActorByModelId(model);
  }
  void ApplySpecialOperation(SpecialOperation operation, int argument) override;
  void InitializeEventCamera() override { Event_InitializeEventCamera(); }
  void FreeEventCamera() override { Event_FreeEventCamera(); }
  void UpdateEventCameraOrigin() override { Event_UpdateEventCameraOrigin(); }
  void RestoreEventFieldCamera() override {
    Event_RestoreEventFieldCamera(-1, -1);
  }
  void UpdateEventCameraTracks() override { Event_UpdateEventCameraTracks(); }
  void UpdateEventScreenTransforms() override {
    Event_UpdateEventScreenTransforms();
  }
  void ApplyEventCameraState() override { Event_ApplyEventCameraState(); }
  std::uint8_t EventCameraCompletionMask() override {
    return static_cast<std::uint8_t>(Event_EventCameraCompletionMask());
  }
  void UpdateImageTweens() override { UpdateLiveImageTweens(); }
  void UpdateWeatherTween() override { UpdateLiveWeatherTween(); }
  void DrawScanlineOverlay() override { DrawLiveScanlineOverlay(); }
  void TransformPalettes(ScriptCommand command) override {
    Event_ScriptTransformPalettes(const_cast<std::uint8_t*>(command.data()), 0);
  }
  void MoveTextureRectangle(const TextureMove& move) override {
    const Render::ScreenRect source{move.x, move.y, move.width, move.height};
    Render::QueueLiveTextureRectangleMove(source, move.destination_x,
                                          move.destination_y);
  }
  void BeginSkipFadeOut() override {
    constexpr int kFadeOpacity = 0xFF;
    Field::LiveBeginScreenFadeOut(0, kFadeOpacity, 0);
  }
  void DisableDisplay() override { Render::DisableLiveDisplay(); }
  void ReturnAllActorsToIdle() override { Actors::ReturnAllLiveActorsToIdle(); }
  void ResetActorMovement(int actor) override {
    Actors::ResetLiveActorMovement(actor);
  }
  bool ActorPresent(int actor) override {
    return Actors::LiveActorAtSlot(actor) != nullptr;
  }
  void PlayActorIdleAnimation(int actor) override {
    Actors::PlayLiveActorAnimation(actor, 1, 0, 0);
  }
  void StopEventMusic() override { Audio::LiveStopMusicSlotPlayback(0); }
  void StopAllSfx() override { Audio::LiveStopAllSfxPlayback(); }
  void StopScriptVibration() override {
    constexpr int kEventOwner = 2;
    Input::StopLiveVibrationPatterns(0, kEventOwner);
  }
  void RestartRoomEffectSound() override {
    Field::LiveRestartRoomEffectSound();
  }

 private:
  void BindData(ScriptDataLease& binding, ScriptLocation location) {
    if (!location.Present()) {
      binding = {};
      return;
    }
    auto source = sources.Lease(location.region);
    if (!source || !source->View(location)) {
      Refuse(ScriptVmRefusal::kStaleScriptRegion,
             static_cast<int>(location.region));
      binding = {};
      return;
    }
    binding = {std::move(*source), location};
  }
  bool PreparePendingFile(int id, EventBuffer destination);
  bool RequestMatches() const;
  void PublishPrepared(PreparedScriptSource&& prepared);
  MutableBuffer pending_buffer_;
  MutableBuffer chained_buffer_;
  Content_DiscQueueSlot* request_ = nullptr;
  std::uint64_t request_generation_ = 0;
  Content_DiscFile pending_file_{};
  EventBuffer pending_destination_ = EventBuffer::kScript;
  PreparedScriptSource pending_publication_;
  bool chained_ready_ = false;
};

LiveVmHost live_host;

bool LiveVmHost::RequestMatches() const {
  return request_ != nullptr && request_generation_ != 0 &&
         Content::LiveDiscRequestGeneration(request_) == request_generation_;
}

EventFileStatus LiveVmHost::EventFile() const {
  if (request_ == nullptr) {
    return EventFileStatus::kNone;
  }
  if (!RequestMatches()) {
    return EventFileStatus::kFailed;
  }
  switch (request_->state) {
    case Content_DiscRequestState_Loaded:
      return pending_buffer_ && request_->vram == pending_buffer_->data() &&
                     request_->cdFile.lba == pending_file_.lba &&
                     request_->cdFile.size == pending_file_.size
                 ? EventFileStatus::kReady
                 : EventFileStatus::kFailed;
    case Content_DiscRequestState_Allocated:
    case Content_DiscRequestState_Reading:
    case Content_DiscRequestState_Enqueued:
      return EventFileStatus::kLoading;
    default:
      return EventFileStatus::kFailed;
  }
}

void LiveVmHost::PublishPrepared(PreparedScriptSource&& prepared) {
  const auto published = sources.Publish(std::move(prepared));
  if (!published) {
    Core_Errorf("event", "Prepared publication invariant failed: reason=%d",
                static_cast<int>(published.error()));
    std::abort();
  }
}

bool LiveVmHost::PreparePendingFile(int id, EventBuffer destination) {
  constexpr int kEventFileCount = 512;
  constexpr int kSectorsPerEventFile = 3;
  if (id < 0 || id >= kEventFileCount || request_ != nullptr ||
      pending_buffer_) {
    Refuse(ScriptVmRefusal::kEventFileQueue, id);
    return false;
  }
  if (destination == EventBuffer::kChained) {
    if (!chained_buffer_ || chained_ready_) {
      Refuse(ScriptVmRefusal::kEventFileQueue, id);
      return false;
    }
    pending_buffer_ = chained_buffer_;
  } else if (destination == EventBuffer::kScript) {
    try {
      pending_buffer_ = std::make_shared<Buffer>(kEventScriptBytes);
    } catch (const std::bad_alloc&) {
      Refuse(ScriptVmRefusal::kEventBufferAllocation, id);
      return false;
    }
    auto prepared =
        sources.PrepareBuffer(ScriptRegion::kEventScript, pending_buffer_);
    if (!prepared) {
      Core_Warnf("event", "File source preparation refused: reason=%d",
                 static_cast<int>(prepared.error()));
      pending_buffer_.reset();
      return false;
    }
    pending_publication_ = std::move(*prepared);
  } else {
    Refuse(ScriptVmRefusal::kEventFileQueue, id);
    return false;
  }
  pending_destination_ = destination;
  pending_file_ = {.lba = VS_0000_EVT_LBA + id * kSectorsPerEventFile,
                   .size = VS_0000_EVT_SIZE};
  return true;
}

// 573B8.c:223 func_800C00E8. The request only sees owned, unpublished storage.
bool LiveVmHost::QueueEventFileLoad(int id, EventBuffer destination) {
  if (!PreparePendingFile(id, destination)) {
    return false;
  }
  try {
    request_ = Content::AllocateLiveDiscRequest(&pending_file_);
    if (request_ != nullptr) {
      request_generation_ = Content::LiveDiscRequestGeneration(request_);
      Content::EnqueueLiveDiscRequest(request_, pending_buffer_->data());
    } else {
      // No automatic owning object may span this blocking legacy boundary.
      request_ = Content::CompleteLiveRefusedDiscRequest(
          &pending_file_, pending_buffer_->data());
      if (request_ != nullptr) {
        request_generation_ = Content::LiveDiscRequestGeneration(request_);
      }
    }
  } catch (const std::bad_alloc&) {
    Refuse(ScriptVmRefusal::kEventBufferAllocation, id);
    CancelEventFile();
    return false;
  }
  if (EventFile() == EventFileStatus::kLoading ||
      EventFile() == EventFileStatus::kReady) {
    return true;
  }
  Refuse(ScriptVmRefusal::kEventFileQueue, id);
  CancelEventFile();
  return false;
}

void LiveVmHost::CancelEventFile() {
  if (pending_buffer_ && !Content::CancelLiveDiscWrites(std::as_writable_bytes(
                             std::span{*pending_buffer_}))) {
    Core_Errorf("event", "Cannot release a script destination still writable");
    std::abort();
  }
  if (RequestMatches()) {
    Content::ReleaseLiveDiscRequest(request_);
  }
  request_ = nullptr;
  request_generation_ = 0;
  pending_publication_ = {};
  pending_buffer_.reset();
}

void LiveVmHost::ReleaseEventFile() {
  const auto status = EventFile();
  if (status == EventFileStatus::kLoading) {
    Refuse(ScriptVmRefusal::kEventFileBusy, EventId());
    return;
  }
  if (status != EventFileStatus::kReady) {
    CancelEventFile();
    return;
  }
  Content::ReleaseLiveDiscRequest(request_);
  request_ = nullptr;
  request_generation_ = 0;
  if (pending_destination_ == EventBuffer::kScript) {
    PublishPrepared(std::move(pending_publication_));
  } else {
    chained_ready_ = true;
  }
  pending_buffer_.reset();
}

void LiveVmHost::ClearEventFile() { CancelEventFile(); }

bool LiveVmHost::AllocateChainedEventBuffer() {
  if (chained_buffer_) {
    Refuse(ScriptVmRefusal::kEventFileBusy, EventId());
    return false;
  }
  try {
    chained_buffer_ = std::make_shared<Buffer>(kEventScriptBytes);
  } catch (const std::bad_alloc&) {
    Refuse(ScriptVmRefusal::kEventBufferAllocation, EventId());
    return false;
  }
  chained_ready_ = false;
  return true;
}

bool LiveVmHost::AdoptChainedEvent() {
  if (!chained_buffer_ || !chained_ready_) {
    Refuse(ScriptVmRefusal::kChainedEventAdoption, EventId());
    return false;
  }
  auto prepared =
      sources.PrepareBuffer(ScriptRegion::kEventScript, chained_buffer_);
  if (!prepared) {
    Core_Warnf("event", "Chained source preparation refused: reason=%d",
               static_cast<int>(prepared.error()));
    return false;
  }
  PublishPrepared(std::move(*prepared));
  chained_buffer_.reset();
  chained_ready_ = false;
  return true;
}

void LiveVmHost::DiscardChainedEventBuffer() {
  if (chained_buffer_ && pending_buffer_ == chained_buffer_) {
    CancelEventFile();
  }
  chained_buffer_.reset();
  chained_ready_ = false;
}

bool LiveVmHost::AllocateEventBuffers() {
  try {
    const auto main = std::make_shared<Buffer>(kEventScriptBytes);
    const auto* subroutine =
        reinterpret_cast<const std::uint8_t*>(&Event_specialSubroutineScript);
    const auto sub = std::make_shared<Buffer>(
        subroutine, subroutine + kSubroutineScriptBytes);
    auto main_source = sources.PrepareBuffer(ScriptRegion::kEventScript, main);
    auto sub_source = sources.PrepareBuffer(ScriptRegion::kSubroutine, sub);
    if (!main_source || !sub_source) {
      Refuse(ScriptVmRefusal::kEventBufferAllocation, EventId());
      return false;
    }
    PublishPrepared(std::move(*main_source));
    PublishPrepared(std::move(*sub_source));
    return true;
  } catch (const std::bad_alloc&) {
    Refuse(ScriptVmRefusal::kEventBufferAllocation, EventId());
    return false;
  }
}

void LiveVmHost::ApplySpecialOperation(SpecialOperation operation,
                                       int argument) {
  switch (operation) {
    case SpecialOperation::kQueueAshleyModel:
      Actors::QueueLiveAshleyModelLoad(argument);
      break;
    case SpecialOperation::kSetEventActorActivity:
      SetLiveEventActorActivity(static_cast<std::int16_t>(argument));
      break;
    case SpecialOperation::kFreeChainedEvent:
      DiscardChainedEventBuffer();
      break;
    case SpecialOperation::kSetFieldCameraOverride:
      Event_SetEventFieldCameraOverride(argument);
      break;
    case SpecialOperation::kPauseSceneTimer:
      State::SetLiveSceneTimerPaused(argument);
      break;
    case SpecialOperation::kStartCameraOrbitReturn:
      Event_StartEventCameraOrbitReturn(static_cast<char>(argument));
      break;
    case SpecialOperation::kSetCameraPitchPreset:
      Field::LiveSetCameraPitchPreset(static_cast<std::int16_t>(argument));
      break;
    case SpecialOperation::kScaleCameraDuration:
      Event_ScaleEventCameraDuration(static_cast<std::int16_t>(argument));
      break;
    case SpecialOperation::kFreezeMovingPlatforms:
      Field_FreezeMovingPlatforms(argument);
      break;
    case SpecialOperation::kClearGeometryOcclusion:
      Field::LiveClearGeometryOcclusion();
      break;
    case SpecialOperation::kPauseMovingPlatforms:
      Field_SetMovingPlatformsPaused(argument);
      break;
    case SpecialOperation::kSetVignette:
      Effects_ScreenDistortion_SetVignetteEnabled(argument);
      break;
    case SpecialOperation::kCancelRoomVisit:
      Field::LiveCancelPendingRoomVisit();
      break;
    default:
      NoteUndefinedSpecialOperation(static_cast<int>(operation));
      break;
  }
}

ScriptFlow LiveVmHost::RunOpcode(ScriptLocation location, int thread) {
  if (location.generation != RegionGeneration(location.region)) {
    Refuse(ScriptVmRefusal::kStaleScriptRegion,
           static_cast<int>(location.region));
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  const auto bytes = Region(location.region);
  if (location.offset >= bytes.size()) {
    Refuse(ScriptVmRefusal::kCursorOutsideScript,
           static_cast<int>(location.offset));
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  const auto command = bytes.subspan(location.offset);
  const OpcodeHandler handler = SkipState() == CutsceneSkip::kSkipping
                                    ? Event_kSkipOpcodeHandlers[command.front()]
                                    : Event_kOpcodeHandlers[command.front()];
  if (handler == nullptr) {
    NoteInvalidOpcode({command, location, thread});
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  if (const auto control = NativeControl(handler)) {
    return control({command, location, thread}, *this);
  }
  static_assert(std::is_trivially_destructible_v<OpcodeInvocation>);
  const OpcodeInvocation previous = invocation;
  invocation = {.instruction = {command, location, thread}, .active = true};
  const auto legacy_result = handler(const_cast<std::uint8_t*>(command.data()),
                                     static_cast<std::int16_t>(thread));
  ScriptFlow result = invocation.result;
  if (!invocation.handled) {
    constexpr std::intptr_t kLastStatus = 4;
    if (legacy_result < 0 || legacy_result > kLastStatus) {
      Core_Warnf("event", "Pointer-valued opcode result refused: %" PRIdPTR,
                 legacy_result);
      result = ScriptFlow::Of(ScriptFlowKind::kStop);
    } else {
      result = ScriptFlow::Of(static_cast<ScriptFlowKind>(legacy_result));
    }
  }
  invocation = previous;
  return result;
}

template <typename Operation>
auto WithSourceUse(Operation operation) -> decltype(operation()) {
  static_assert(std::is_trivially_destructible_v<Operation>);
  const auto begun = live_host.sources.BeginUse();
  if (!begun) {
    Core_Warnf("event", "Script source use refused: reason=%d",
               static_cast<int>(begun.error()));
    return {};
  }
  const auto result = operation();
  if (!live_host.sources.EndUse()) {
    Core_Errorf("event", "Script source use ended without a matching begin");
    std::abort();
  }
  return result;
}

std::expected<ScriptDataLease, ScriptSourceError> LeaseData(
    const ScriptDataLease& binding) {
  if (!binding.location.Present()) {
    return std::unexpected(ScriptSourceError::kMissingOwner);
  }
  if (!binding.Bytes()) {
    return std::unexpected(ScriptSourceError::kExtent);
  }
  return binding;
}

}  // namespace

ScriptVmHost& LiveScriptVm() { return live_host; }

std::intptr_t DispatchLiveScriptControlOpcode(std::uint8_t* instruction,
                                              std::int16_t thread,
                                              ScriptControlOpcode opcode) {
  const ScriptInstruction current = live_host.invocation.instruction;
  if (!live_host.invocation.active || opcode == nullptr ||
      instruction != current.bytes.data() || thread != current.thread ||
      current.location.generation !=
          live_host.RegionGeneration(current.location.region)) {
    live_host.Refuse(ScriptVmRefusal::kMissingCursor, thread);
    return static_cast<std::intptr_t>(ScriptFlowKind::kStop);
  }
  live_host.invocation.result = opcode(current, live_host);
  live_host.invocation.handled = true;
  return static_cast<std::intptr_t>(ScriptFlowKind::kAdvance);
}

std::expected<ScriptSource, ScriptSourceError> LeaseLiveScriptSource(
    ScriptRegion region) {
  return live_host.sources.Lease(region);
}

std::expected<ScriptDataLease, ScriptSourceError> LeaseLiveDialogStrings() {
  return LeaseData(live_host.strings);
}

std::expected<ScriptDataLease, ScriptSourceError> LeaseLiveTextBoxFrame(
    int style) {
  if (style < 0 || style >= kTextBoxFrameStyleCount) {
    return std::unexpected(ScriptSourceError::kExtent);
  }
  return LeaseData(live_host.frames[static_cast<std::size_t>(style)]);
}

std::expected<ScriptDataLease, ScriptSourceError> LeaseLiveVibrationPattern(
    int pattern) {
  if (pattern < 0 || pattern >= kVibrationPatternSlots) {
    return std::unexpected(ScriptSourceError::kExtent);
  }
  return LeaseData(live_host.patterns[static_cast<std::size_t>(pattern)]);
}

std::expected<void, ScriptSourceError> MountLiveScriptSource(
    ScriptRegion region, std::shared_ptr<const Buffer> source) {
  return live_host.sources.MountBuffer(region, std::move(source));
}

std::expected<PreparedScriptSource, ScriptSourceError>
PrepareLiveRoomEntrySource(RoomEventLease room) {
  return live_host.sources.PrepareRoom(ScriptRegion::kEventScript,
                                       std::move(room));
}

std::expected<void, ScriptSourceError> PublishLiveScriptSource(
    PreparedScriptSource&& prepared) {
  return live_host.sources.Publish(std::move(prepared));
}

RoomEventLease LiveRoomScriptSections() { return live_host.room; }

void PublishLiveRoomScriptSections(RoomEventLease room) {
  live_host.room = std::move(room);
}

bool InitializeLiveEventScriptData(ScriptLocation source) {
  return WithSourceUse(
      [source] { return InitializeEventScriptData(source, live_host); });
}

bool InitializeLiveScriptOpcodeTable() {
  return WithSourceUse([] { return InitializeScriptOpcodeTable(live_host); });
}

int UpdateLiveEventPlayback() {
  return WithSourceUse([] { return UpdateEventPlayback(live_host); });
}

bool ExecuteLiveScriptThread(int thread) {
  return WithSourceUse([thread] {
    return ExecuteScriptThread(static_cast<ScriptThreadId>(thread), live_host);
  });
}

void ReleaseLiveScriptActorControl() { ReleaseScriptActorControl(live_host); }

void ResetLiveScriptRuntime() {
  live_host.CancelEventFile();
  live_host.DiscardChainedEventBuffer();
  live_host.sources.Reset();
  live_host.room.reset();
  live_host.cursors = {};
  live_host.return_to = {};
  live_host.strings = {};
  live_host.frames = {};
  live_host.patterns = {};
  live_host.curves = {};
  live_host.invocation = {};
  live_host.SetPhase(PlaybackPhase::kIdle);
  live_host.SetStartConfirmed(false);
  live_host.SetMainThreadFinished(false);
  live_host.SetChainedEventLoaded(false);
}

void AbandonLiveScriptUses() {
  live_host.invocation = {};
  live_host.sources.AbandonUses();
}

}  // namespace VS::Event
