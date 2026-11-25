#include "Event/ScriptVmLive.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <optional>
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
#include "Battle/RewardFlowLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Runtime.hpp"
#include "Diagnostics/SceneTrace.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Event/Actors.hpp"
#include "Event/EventCameraControlLive.hpp"
#include "Event/EventCameraFrameLive.hpp"
#include "Event/EventCameraOwnershipLive.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Event/EventScreenTweensLive.hpp"
#include "Event/Script.hpp"
#include "Event/ScriptActorCommands.hpp"
#include "Event/ScriptActorCommandsLive.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptAudio.hpp"
#include "Event/ScriptCamera.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptEffectCommandsLive.hpp"
#include "Event/ScriptEffects.hpp"
#include "Event/ScriptField.hpp"
#include "Event/ScriptRoomCommands.hpp"
#include "Event/ScriptScreen.hpp"
#include "Event/ScriptScreenCommands.hpp"
#include "Event/ScriptSplashCommands.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Field/BackgroundLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Input/Actions.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/GimImagesLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/ScriptFlagsLive.hpp"

using VS::Content::disc::Locate;

namespace VS::Event {
namespace {

using Buffer = std::vector<std::uint8_t>;
using MutableBuffer = std::shared_ptr<Buffer>;

// The palette reset instruction (opcode 0x70) a skip's end patches with each
// palette before running it.
// decomp: skipResetPaletteOp (D_800E9BE0, BATTLE.PRG file offset 0x813E0;
// src/BATTLE/BATTLE.PRG/4DEA8.c:306, used at :418)
constexpr std::array<std::uint8_t, 8> kSkipPaletteResetCommand = {
    0x70, 0x08, 0x01, 0x20, 0, 0, 0, 0,
};
// A skip handler holding this instruction ends without the sound reset.
// decomp: skipTailMarkerOp (D_800E9BE8, file offset 0x813E8;
// src/BATTLE/BATTLE.PRG/4DEA8.c:307, compared at :427)
constexpr std::array<std::uint8_t, 4> kSkipSoundRestoreMarker = {
    0x50,
    0x72,
    0xA8,
    0x01,
};
// The timer modes (counting up, counting down) and the reset scopes the
// timer opcode 0x5A selects.
// decomp: battleTimerModes (D_800E9BF4, file offset 0x813F4) and
// battleTimerLifetimes (D_800E9BF8, 0x813F8; src/BATTLE/BATTLE.PRG/4DEA8.c:311,
// used at :2390)
constexpr std::array<std::int16_t, 2> kSceneTimerModes{1, 2};
constexpr std::array<std::int16_t, 4> kSceneTimerResetScopes{0, 1, 2, 3};
// The camera pitches special operation 10 selects.
// decomp: D_800E9C00 (file offset 0x81400; src/BATTLE/BATTLE.PRG/4DEA8.c:313,
// used at :3352)
constexpr std::array<std::int16_t, 4> kCameraPitchPresets{0, -128, -768, 0x66};

struct ControlBinding {
  ScriptOpcodeHandler slot;
  ScriptControlOpcode typed;
};

// decomp: INITBTL.PRG initScriptFunctionTable
// (src/BATTLE/INITBTL.PRG/12AC.c:116; was _initScriptFunctionTable), BATTLE.PRG
// control handlers in 4A0A8.c and src/BATTLE/BATTLE.PRG/573B8.c. Match the
// selected table's target.
constexpr std::array kControlBindings{
    ControlBinding{ScriptReportInvalidOpcodeOpcode, ScriptReportInvalidOpcode},
    ControlBinding{ScriptNoOperationOpcode, ScriptNoOperation},
    ControlBinding{ScriptYieldFrameOpcode, ScriptYieldFrame},
    ControlBinding{ScriptWaitForEventFileOpcode, ScriptWaitForEventFile},
    ControlBinding{ScriptBeginSkippableCutsceneOpcode,
                   ScriptBeginSkippableCutscene},
    ControlBinding{ScriptEndCutsceneAndJumpOpcode, ScriptEndCutsceneAndJump},
    ControlBinding{ScriptEndCutsceneOpcode, ScriptEndCutscene},
    ControlBinding{ScriptWaitForButtonAndYieldOpcode,
                   ScriptWaitForButtonAndYield},
    ControlBinding{ScriptBranchOnFlagImmediateOpcode,
                   ScriptBranchOnFlagImmediate},
    ControlBinding{ScriptBranchOnFlagPairOpcode, ScriptBranchOnFlagPair},
    ControlBinding{ScriptJumpForwardOpcode, ScriptJumpForward},
    ControlBinding{ScriptCacheFlagValueOpcode, ScriptCacheFlagValue},
    ControlBinding{ScriptBranchOnCachedFlagOpcode, ScriptBranchOnCachedFlag},
    ControlBinding{ScriptReturnToSavedAddressOpcode,
                   ScriptReturnToSavedAddress},
    ControlBinding{ScriptSetLoopCounterOpcode, ScriptSetLoopCounter},
    ControlBinding{ScriptLoopBackwardOpcode, ScriptLoopBackward},
    ControlBinding{ScriptApplyImmediateFlagOperationOpcode,
                   ScriptApplyImmediateFlagOperation},
    ControlBinding{ScriptApplyFlagPairOperationOpcode,
                   ScriptApplyFlagPairOperation},
    ControlBinding{ScriptStoreInventoryItemPresenceOpcode,
                   ScriptStoreInventoryItemPresence},
    ControlBinding{ScriptStoreButtonStateOpcode, ScriptStoreButtonState},
    ControlBinding{ScriptConfigureTimerOpcode, ScriptConfigureTimer},
    ControlBinding{ScriptWaitForButtonOrTimeoutOpcode,
                   ScriptWaitForButtonOrTimeout},
    ControlBinding{ScriptConfigureEasingCurveOpcode,
                   ScriptConfigureEasingCurve},
    ControlBinding{CommandSpecialOperationOpcode, CommandSpecialOperation},
    ControlBinding{CommandWaitFramesOpcode, CommandWaitFrames},
    ControlBinding{CommandSetTickSpeedOpcode, CommandSetTickSpeed},
    ControlBinding{CommandWaitScriptContextOpcode, CommandWaitScriptContext},
    ControlBinding{CommandStartScriptContextOpcode, CommandStartScriptContext},
    ControlBinding{CommandStopScriptContextOpcode, CommandStopScriptContext},
    ControlBinding{CommandLoadNextEventOpcode, CommandLoadNextEvent},
    ControlBinding{CommandFinishEventOpcode, CommandFinishEvent},
    ControlBinding{ScriptSetGimmickVisibilityOpcode,
                   ScriptSetGimmickVisibility},
    ControlBinding{ScriptOverrideDepthCueDistanceOpcode,
                   ScriptOverrideDepthCueDistance},
    ControlBinding{ScriptSetRoomLightingModeOpcode, ScriptSetRoomLightingMode},
    ControlBinding{ScriptShowGeometryGroupOpcode, ScriptShowGeometryGroup},
    ControlBinding{ScriptHideGeometryGroupOpcode, ScriptHideGeometryGroup},
    ControlBinding{ScriptSwapGeometryGroupFlagsOpcode,
                   ScriptSwapGeometryGroupFlags},
    ControlBinding{ScriptSetCollisionTileModeOpcode,
                   ScriptSetCollisionTileMode},
    ControlBinding{ScriptAdjustGeometryDepthBiasOpcode,
                   ScriptAdjustGeometryDepthBias},
    ControlBinding{ScriptSetRoomBackgroundVisibleOpcode,
                   ScriptSetRoomBackgroundVisible},
    ControlBinding{ScriptLoadRoomBackgroundOpcode, ScriptLoadRoomBackground},
    ControlBinding{ScriptWaitForRoomBackgroundOpcode,
                   ScriptWaitForRoomBackground},
    ControlBinding{ScriptFreeRoomBackgroundOpcode, ScriptFreeRoomBackground},
    ControlBinding{ScriptSetBackgroundOffsetOpcode, ScriptSetBackgroundOffset},
    ControlBinding{ScriptSetFirstPersonViewOpcode, ScriptSetFirstPersonView},
    ControlBinding{ScriptSetBackgroundParallaxOpcode,
                   ScriptSetBackgroundParallax},
    ControlBinding{ScriptFadeOutAndWaitOpcode, ScriptFadeOutAndWait},
    ControlBinding{ScriptFadeInOpcode, ScriptFadeIn},
    ControlBinding{ScriptSetWipeAngleOpcode, ScriptSetWipeAngle},
    ControlBinding{ScriptConfigureScreenTransitionOpcode,
                   ScriptConfigureScreenTransition},
    ControlBinding{ScriptSetRoomDisplayFlagsOpcode, ScriptSetRoomDisplayFlags},
    ControlBinding{ScriptWaitForScreenTransitionOpcode,
                   ScriptWaitForScreenTransition},
    ControlBinding{ScriptTogglePaletteHookOpcode, ScriptTogglePaletteHook},
    ControlBinding{ScriptTransformPalettesOpcode, ScriptTransformPalettes},
    ControlBinding{ScriptWaitForSplashFileOpcode, ScriptWaitForSplashFile},
    ControlBinding{ScriptLoadSplashImageOpcode, ScriptLoadSplashImage},
    ControlBinding{ScriptConfigureSplashLayerOpcode,
                   ScriptConfigureSplashLayer},
    ControlBinding{ScriptTweenSplashLayerOpcode, ScriptTweenSplashLayer},
    ControlBinding{ScriptFreeSplashImageOpcode, ScriptFreeSplashImage},
    ControlBinding{ScriptWaitForSplashTweenOpcode, ScriptWaitForSplashTween},
    ControlBinding{ScriptSetEnvironmentParticlesOpcode,
                   ScriptSetEnvironmentParticles},
    ControlBinding{ScriptSetParticleVariationEnabledOpcode,
                   ScriptSetParticleVariationEnabled},
    ControlBinding{ScriptMuteEnvironmentSoundOpcode,
                   ScriptMuteEnvironmentSound},
    ControlBinding{ScriptControlRoomAnimationOpcode,
                   ScriptControlRoomAnimation},
};

ScriptControlOpcode NativeControl(ScriptOpcodeHandler handler) {
  for (const auto& binding : kControlBindings) {
    if (binding.slot == handler) {
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
    VS::Core::Warnf("event", "Script VM refused: reason=%d value=%d",
                    static_cast<int>(reason), value);
  }
  void NoteInvalidOpcode(const ScriptInstruction& instruction) override {
    VS::Core::Warnf("event", "Unknown opcode: thread=%d region=%d offset=%u",
                    instruction.thread,
                    static_cast<int>(instruction.location.region),
                    instruction.location.offset);
  }
  void NoteUndefinedSpecialOperation(int operation) override {
    VS::Core::Warnf("event", "Undefined special operation: %d", operation);
  }
  void NoteCutsceneSkip(int thread) override {
    VS::Core::Debugf("event", "Skip event=%d thread=%d", eventId, thread);
  }
  std::span<const std::uint8_t> Region(ScriptRegion region) const override {
    const auto bytes = sources.Borrow(region);
    if (!bytes) {
      VS::Core::Warnf(
          "event", "Script source borrow refused: region=%d reason=%d",
          static_cast<int>(region), static_cast<int>(bytes.error()));
      return {};
    }
    return *bytes;
  }
  std::uint64_t RegionGeneration(ScriptRegion region) const override {
    const auto source = sources.Lease(region);
    if (!source) {
      VS::Core::Warnf(
          "event", "Script source lease refused: region=%d reason=%d",
          static_cast<int>(region), static_cast<int>(source.error()));
      return 0;
    }
    return source->Generation();
  }
  std::span<const std::uint8_t> OpcodeSizes() const override {
    return opcodeSizes;
  }
  ScriptFlow RunOpcode(ScriptLocation location, int thread) override;
  std::span<const std::uint8_t> PaletteResetCommand() const override {
    return kSkipPaletteResetCommand;
  }
  std::span<const std::uint8_t> SoundRestoreMarker() const override {
    return kSkipSoundRestoreMarker;
  }
  std::span<const std::int16_t> SceneTimerModes() const override {
    return kSceneTimerModes;
  }
  std::span<const std::int16_t> SceneTimerScopes() const override {
    return kSceneTimerResetScopes;
  }
  std::span<const std::int16_t> CameraPitchPresets() const override {
    return kCameraPitchPresets;
  }
  PlaybackPhase Phase() const override {
    return static_cast<PlaybackPhase>(playbackPhase);
  }
  void SetPhase(PlaybackPhase phase) override {
    playbackPhase = static_cast<std::uint8_t>(phase);
  }
  bool StartConfirmed() const override { return isStartConfirmed != 0; }
  void SetStartConfirmed(bool value) override {
    isStartConfirmed = static_cast<std::uint8_t>(value);
  }
  EventSource Source() const override {
    return static_cast<EventSource>(eventSource);
  }
  void SetSource(EventSource source) override {
    eventSource = static_cast<std::uint16_t>(source);
  }
  std::int16_t EventId() const override { return eventId; }
  void SetEventId(std::int16_t id) override { eventId = id; }
  bool MainThreadFinished() const override { return isMainThreadFinished != 0; }
  void SetMainThreadFinished(bool value) override {
    isMainThreadFinished = value;
  }
  bool ChainedEventLoaded() const override { return isChainedEventLoaded != 0; }
  void SetChainedEventLoaded(bool value) override {
    isChainedEventLoaded = value;
  }
  std::int16_t RoomChangeState() const override { return roomChangeState; }
  void SetRoomChangeState(std::int16_t state) override {
    roomChangeState = state;
  }
  bool FieldCameraRestoreScripted() const override {
    return isFieldCameraRestoreScripted != 0;
  }
  void SetFieldCameraRestoreScripted(bool value) override {
    isFieldCameraRestoreScripted = value;
  }
  bool ScanlineOverlayEnabled() const override {
    return isScanlineOverlayEnabled != 0;
  }
  void SetScanlineOverlayEnabled(bool value) override {
    isScanlineOverlayEnabled = static_cast<std::uint8_t>(value);
  }
  // The original stores the spread sign bit of opcode 0x61's operand: 0xFF
  // while a scripted texture LOD depth is kept.
  void SetScriptBackdropEnabled(bool value) override {
    constexpr std::uint8_t kLodDepthKept = 0xFF;
    isScriptBackdropEnabled = value ? kLodDepthKept : std::uint8_t{0};
  }
  bool MenuTransitionActive() const override {
    return isMenuTransitionActive != 0;
  }
  void SetMenuTransitionActive(bool value) override {
    isMenuTransitionActive = static_cast<std::uint8_t>(value);
  }
  void SetAmbientSoundSuspended(bool value) override {
    isAmbientSoundSuspended = static_cast<char>(value);
  }
  ScriptLocation ThreadCursor(int thread) const override {
    return cursors.at(static_cast<std::size_t>(thread));
  }
  void SetThreadCursor(int thread, ScriptLocation cursor) override {
    cursors.at(static_cast<std::size_t>(thread)) = cursor;
  }
  std::int16_t WaitFrames(int thread) const override {
    return threadWaitFrames[thread];
  }
  void SetWaitFrames(int thread, std::int16_t frames_left) override {
    threadWaitFrames[thread] = frames_left;
  }
  ScriptLocation SubroutineReturn() const override { return return_to; }
  void SetSubroutineReturn(ScriptLocation location) override {
    return_to = location;
  }
  std::uint8_t LoopCounter(int counter) const override {
    return loopCounters[counter];
  }
  void SetLoopCounter(int counter, std::uint8_t value) override {
    loopCounters[counter] = value;
  }
  std::int8_t CachedFlag() const override { return cachedFlagValue; }
  void SetCachedFlag(std::int8_t value) override { cachedFlagValue = value; }
  CutsceneSkip SkipState() const override {
    return static_cast<CutsceneSkip>(skipState);
  }
  void SetSkipState(CutsceneSkip state) override {
    skipState = static_cast<char>(state);
  }
  bool SkipForced() const override { return isSkipForced != 0; }
  void SetSkipForced(bool value) override {
    isSkipForced = static_cast<std::uint8_t>(value);
  }
  bool StopsMusicOnSkip() const override { return stopsMusicOnSkip != 0; }
  void SetStopsMusicOnSkip(bool value) override {
    stopsMusicOnSkip = static_cast<std::uint8_t>(value);
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
    isTextBoxAwaitingClose[box] = static_cast<std::uint8_t>(value);
  }
  void SetWeaponTransitionPending(bool value) override {
    weaponTransitionPending = static_cast<std::uint8_t>(value);
  }
  void SetTextSelectionActive(bool value) override {
    isTextSelectionActive = static_cast<std::uint8_t>(value);
  }
  void SetEffectWaitPending(bool value) override {
    isEffectWaitPending = value;
  }
  void CancelImageTweens() override {
    weatherTween.duration_frames = 0;
    for (auto& tween : std::span{splashTweens}.first<kImageTweenCount>()) {
      tween.duration_frames = 0;
    }
  }
  void CaptureEventMusicSlot() override {
    musicSlot = static_cast<char>(Audio::LiveCurrentMusicSlot());
  }
  bool ScriptControlledActor(int actor) const override {
    return scriptControlledActors[actor] != 0;
  }
  void ClearScriptControlledActors() override {
    std::fill_n(scriptControlledActors, kScriptControlledActorCount, 0);
  }
  bool BindRoomDoorSource() override {
    const auto mounted = sources.MountRoom(ScriptRegion::kDoorEvents, room);
    if (!mounted) {
      VS::Core::Warnf("event", "Door source binding refused: reason=%d",
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
    return VS::Input::buttonsPressed;
  }
  void ClearHeldButtons() override { VS::Input::buttonsPressed = 0; }
  bool StartPressed() const override {
    return (VS::Input::buttonsPressed & VS::Input::Start) != 0;
  }
  std::uint8_t PressedButtonsLowByte() const override {
    return VS::Input::LowButtonByte(VS::Input::buttonsPressed);
  }
  int PreviousButtons() const override { return VS::Input::previousButtons; }
  int CountInventoryItem(int item) override {
    return Items::CountLiveInventoryItem(item);
  }
  void StartSceneTimer(int mode, int scope, int duration) override {
    State::StartLiveSceneTimer(mode, scope, duration);
  }
  void ResetSceneTimer() override { State::ResetLiveSceneTimer(); }
  void SetTickSpeed(int speed) override { State::SetLiveTickSpeed(speed); }
  bool DismissTextBox(int box) override {
    return VS::Menu::DismissLiveTextBox(box) != 0;
  }
  bool UpdateRewardSequence() override {
    return Battle::UpdateLiveRewardSequence() != 0;
  }
  void BeginEventRewardResults() override {
    Battle::BeginLiveEventRewardResults();
  }
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
  void InitializeEventCamera() override { InitializeEventCameraEntry(); }
  // F01215 (func_800BBDDC) only forwarded to the high-end free; retired.
  void FreeEventCamera() override {
    Core::Memory::FreeLiveFromHighEnd(cameraWorkspace);
  }
  void UpdateEventCameraOrigin() override { UpdateEventCameraOriginEntry(); }
  void RestoreEventFieldCamera() override {
    RestoreEventFieldCameraEntry(-1, -1);
  }
  void UpdateEventCameraTracks() override { UpdateEventCameraTracksEntry(); }
  void UpdateEventScreenTransforms() override {
    UpdateEventScreenTransformsEntry();
  }
  void ApplyEventCameraState() override { ApplyEventCameraStateEntry(); }
  std::uint8_t EventCameraCompletionMask() override {
    return static_cast<std::uint8_t>(EventCameraCompletionMaskEntry());
  }
  void UpdateImageTweens() override { UpdateLiveImageTweens(); }
  void UpdateWeatherTween() override { UpdateLiveWeatherTween(); }
  void DrawScanlineOverlay() override { DrawLiveScanlineOverlay(); }
  void TransformPalettes(ScriptCommand command) override {
    static_cast<void>(
        ScriptTransformPalettes(ScriptInstruction{.bytes = command}, *this));
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
  void SetMovingPlatformVisibility(int platform, int visibility) override {
    static_cast<void>(
        Field::SetLiveMovingPlatformVisibility(platform, visibility));
  }
  void SetRoomTextureLodDepth(int depth) override {
    Render::SetRoomTextureLodDepth(Render::LiveRoomClipState(), depth);
  }
  void SetRoomGeometryDrawMode(int mode) override {
    VS::Field::roomGeometryDrawMode = mode;
  }
  void ShowRoomGeometryGroup(int group) override {
    static_cast<void>(Field::LiveShowRoomGeometryGroup(group));
  }
  void HideRoomGeometryGroup(int group) override {
    static_cast<void>(Field::LiveHideRoomGeometryGroup(group));
  }
  void SwapRoomGeometryGroupFlags(int first_group, int second_group) override {
    static_cast<void>(
        Field::LiveSwapRoomGeometryGroupFlags(first_group, second_group));
  }
  void SetCollisionCellBlocked(const CollisionCellChange& change) override {
    static_cast<void>(Field::SetLiveCollisionCellBlocked(
        change.tile_x, change.tile_z, change.layer, change.blocked));
  }
  void AddRoomGeometryDepthBias(int group, int depth_bias) override {
    static_cast<void>(Field::LiveAddRoomGeometryDepthBias(group, depth_bias));
  }
  void SetTiledBackgroundEnabled(int enabled) override {
    Field::LiveSetTiledBackgroundEnabled(enabled);
  }
  void QueueTiledBackgroundLoad(int texture_page) override {
    static_cast<void>(Field::LiveQueueTiledBackgroundLoad(texture_page));
  }
  bool FinishTiledBackgroundLoad() override {
    return Field::LiveFinishTiledBackgroundLoad() != 0;
  }
  void ReleaseTiledBackground() override {
    static_cast<void>(Field::LiveReleaseTiledBackground());
  }
  void SetRoomBackgroundOffset(int offset_x, int offset_y) override {
    Field::LiveSetRoomBackgroundOffset(offset_x, offset_y);
  }
  void SetBackgroundParallaxScale(int parallax_x, int parallax_y) override {
    Field::LiveSetBackgroundParallaxScale(parallax_x, parallax_y);
  }
  void SetFirstPersonViewDisabled(bool disabled) override {
    static_cast<void>(Field::LiveSetFirstPersonViewDisabled(disabled ? 1 : 0));
  }
  int ScreenTransitionPhase() override {
    return Field::LiveScreenTransitionPhase();
  }
  void BeginScreenFadeOut(const ScreenTransitionRequest& request) override {
    Field::LiveBeginScreenFadeOut(request.effect, request.speed,
                                  request.wipe_angle);
  }
  void BeginScreenFadeIn(const ScreenTransitionRequest& request) override {
    Field::LiveBeginScreenFadeIn(request.effect, request.speed,
                                 request.wipe_angle);
  }
  void SetTransitionWipeAngle(int wipe_angle) override {
    Field::LiveSetTransitionWipeAngle(wipe_angle);
  }
  void ConfigureScreenTransition(
      const ScreenTransitionRequest& request) override {
    Field::LiveConfigureScreenTransition(request.effect, request.speed,
                                         request.wipe_angle);
  }
  void EnableDisplay() override { Render::EnableLiveDisplay(); }
  void ShowPrimaryRoomGeometry() override {
    Field::LiveShowPrimaryRoomGeometry();
  }
  void HidePrimaryRoomGeometry() override {
    Field::LiveHidePrimaryRoomGeometry();
  }
  void EnableAllActorModels() override { Actors::EnableAllLiveActorModels(); }
  void DisableAllActorModels() override { Actors::DisableAllLiveActorModels(); }
  // func_8007B0FC and func_8007B104 (146C.c:8237-8239) are both empty.
  void RunPaletteHook(PaletteHook /*hook*/) override { Core::NoOperation(); }
  void BeginScriptActorSelection(std::uint16_t selector) override {
    BuildLiveScriptActorSelection(selector, actor_selection_);
  }
  std::uint16_t NextScriptSelectedActor() override {
    return NextLiveScriptSelectedActor(actor_selection_);
  }
  void TransformActorColors(const PaletteTransform& transform,
                            int actor) override {
    Render::TransformLiveActorColors(
        transform.mode, transform.duration, actor, transform.red_offset,
        transform.green_offset, transform.blue_offset);
  }
  void TransformSceneColors(const PaletteTransform& transform) override {
    Render::TransformLiveSceneColors(
        transform.mode, transform.duration, transform.red_offset,
        transform.green_offset, transform.blue_offset);
  }
  void TransformBackgroundColors(const PaletteTransform& transform) override {
    Render::TransformLiveBackgroundColors(
        transform.mode, transform.duration, transform.red_offset,
        transform.green_offset, transform.blue_offset);
  }
  bool SplashImageLoading() override {
    return Render::LiveGimImageLoading() != 0;
  }
  bool SplashImagesPresent() const override {
    return Render::LiveGimImagesPresent();
  }
  void QueueSplashImage(int image_id, int first_page) override {
    Render::QueueLiveGimImage(image_id, first_page);
  }
  void ReleaseSplashImages() override { Render::ReleaseLiveGimImages(); }
  void ConfigureSplashLayer(int layer, int routine,
                            const ImageLayerPlacement& placement) override {
    Render::ConfigureLiveGimLayer(layer, routine, placement.x, placement.y,
                                  placement.scale, placement.brightness);
  }
  std::optional<ImageLayerPlacement> SplashLayerPlacement(int layer) override {
    const auto placement = Render::LiveGimLayerPlacement(layer);
    if (!placement.has_value()) {
      return std::nullopt;
    }
    return ImageLayerPlacement{.x = placement->offset_x,
                               .y = placement->offset_y,
                               .scale = placement->scale_and_order,
                               .brightness = placement->brightness};
  }
  SplashTween SplashImageTween(int layer) const override {
    return LiveSplashTween(layer);
  }
  void SetSplashImageTween(int layer, const SplashTween& tween) override {
    SetLiveSplashTween(layer, tween);
  }
  void SetRoomWeather(int kind) override { Field::LiveSetRoomWeather(kind); }
  void SetWeatherIntensity(int intensity) override {
    Field::LiveSetWeatherIntensity(intensity);
  }
  void SetWeatherWindAngles(int direction, int strength) override {
    Field::LiveSetWeatherWindAngles(direction, strength);
  }
  void SetWeatherVariationEnabled(int enabled) override {
    Field::LiveSetWeatherVariationEnabled(enabled);
  }
  WeatherTween EventWeatherTween() const override { return LiveWeatherTween(); }
  void SetEventWeatherTween(const WeatherTween& tween) override {
    StoreLiveWeatherTween(tween);
  }
  bool AmbientSoundSuspended() const override {
    return isAmbientSoundSuspended != 0;
  }
  std::int16_t SuspendedAmbientSound() const override {
    return suspendedAmbientSoundId;
  }
  void SetSuspendedAmbientSound(std::int16_t sound) override {
    suspendedAmbientSoundId = sound;
  }
  bool RoomAmbientSequenceAbsent() override {
    return Field::LiveIsRoomAudioSequenceAbsent() != 0;
  }
  int ExchangeWeatherAmbientSound(int sound) override {
    return Field::LiveSetWeatherAmbientSound(sound);
  }
  bool RoomEffectActive(int effect) override {
    return Field::LiveIsRoomEffectActive(effect) != 0;
  }
  void FinishRoomDoorRotation(int effect) override {
    Field::LiveFinishRoomDoorRotation(effect);
  }
  void StartRoomEffect(int effect) override {
    static_cast<void>(Field::LiveStartRoomEffect(effect));
  }
  void ToggleRoomEffectUpdates(int effect) override {
    static_cast<void>(Field::LiveToggleRoomEffectUpdates(effect));
  }

 private:
  // Opcode 0x70's actor selection (func_800B9C58's sp18), rebuilt by each
  // BeginScriptActorSelection.
  ActorSelection actor_selection_{};

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
  VS::Content::DiscQueueSlot* request_ = nullptr;
  std::uint64_t request_generation_ = 0;
  VS::Content::DiscFile pending_file_{};
  EventBuffer pending_destination_ = EventBuffer::kScript;
  PreparedScriptSource pending_publication_;
  bool chained_ready_ = false;
};

LiveVmHost liveHost;

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
    case VS::Content::DiscRequestState::kLoaded:
      return pending_buffer_ &&
                     request_->destination == pending_buffer_->data() &&
                     request_->file.lba == pending_file_.lba &&
                     request_->file.size == pending_file_.size
                 ? EventFileStatus::kReady
                 : EventFileStatus::kFailed;
    case VS::Content::DiscRequestState::kAllocated:
    case VS::Content::DiscRequestState::kReading:
    case VS::Content::DiscRequestState::kEnqueued:
      return EventFileStatus::kLoading;
    default:
      return EventFileStatus::kFailed;
  }
}

void LiveVmHost::PublishPrepared(PreparedScriptSource&& prepared) {
  const auto published = sources.Publish(std::move(prepared));
  if (!published) {
    VS::Core::Errorf("event",
                     "Prepared publication invariant failed: reason=%d",
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
      VS::Core::Warnf("event", "File source preparation refused: reason=%d",
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
  pending_file_ = {
      .lba = Locate("EVENT/0000.EVT").lba + id * kSectorsPerEventFile,
      .size = Locate("EVENT/0000.EVT").size};
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
    VS::Core::Errorf("event",
                     "Cannot release a script destination still writable");
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
    VS::Core::Warnf("event", "Chained source preparation refused: reason=%d",
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
    const std::uint8_t* subroutine = specialSubroutineScript;
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
      SetEventFieldCameraOverrideEntry(argument);
      break;
    case SpecialOperation::kPauseSceneTimer:
      State::SetLiveSceneTimerPaused(argument);
      break;
    case SpecialOperation::kStartCameraOrbitReturn:
      StartEventCameraOrbitReturnEntry(static_cast<char>(argument));
      break;
    case SpecialOperation::kSetCameraPitchPreset:
      Field::LiveSetCameraPitchPreset(static_cast<std::int16_t>(argument));
      break;
    case SpecialOperation::kScaleCameraDuration:
      ScaleEventCameraDurationEntry(static_cast<std::int16_t>(argument));
      break;
    case SpecialOperation::kFreezeMovingPlatforms:
      VS::Field::FreezeMovingPlatformsEntry(argument);
      break;
    case SpecialOperation::kClearGeometryOcclusion:
      Field::LiveClearGeometryOcclusion();
      break;
    case SpecialOperation::kPauseMovingPlatforms:
      VS::Field::SetMovingPlatformsPausedEntry(argument);
      break;
    case SpecialOperation::kSetVignette:
      VS::Effects::ScreenDistortion::SetLiveVignetteEnabled(argument);
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
  const auto trace_command = Game::RecordSceneCommand(
      EventId(), Source(), {command, location, thread},
      static_cast<std::uint32_t>(OpcodeSize(command.front(), *this)),
      SkipState());
  const auto complete = [trace_command](ScriptFlow result) {
    Game::RecordSceneResult(trace_command, result);
    return result;
  };
  const ScriptOpcodeHandler handler = SkipState() == CutsceneSkip::kSkipping
                                          ? skipOpcodeHandlers[command.front()]
                                          : opcodeHandlers[command.front()];
  if (handler == nullptr) {
    NoteInvalidOpcode({command, location, thread});
    return complete(ScriptFlow::Of(ScriptFlowKind::kStop));
  }
  if (const auto control = NativeControl(handler)) {
    return complete(control({command, location, thread}, *this));
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
      VS::Core::Warnf("event",
                      "Pointer-valued opcode result refused: %" PRIdPTR,
                      legacy_result);
      result = ScriptFlow::Of(ScriptFlowKind::kStop);
    } else {
      result = ScriptFlow::Of(static_cast<ScriptFlowKind>(legacy_result));
    }
  }
  invocation = previous;
  return complete(result);
}

template <typename Operation>
auto WithSourceUse(Operation operation) -> decltype(operation()) {
  static_assert(std::is_trivially_destructible_v<Operation>);
  const auto begun = liveHost.sources.BeginUse();
  if (!begun) {
    VS::Core::Warnf("event", "Script source use refused: reason=%d",
                    static_cast<int>(begun.error()));
    return {};
  }
  const auto result = operation();
  if (!liveHost.sources.EndUse()) {
    VS::Core::Errorf("event",
                     "Script source use ended without a matching begin");
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

ScriptVmHost& LiveScriptVm() { return liveHost; }

std::intptr_t DispatchLiveScriptControlOpcode(std::uint8_t* instruction,
                                              std::int16_t thread,
                                              ScriptControlOpcode opcode) {
  const ScriptInstruction current = liveHost.invocation.instruction;
  if (!liveHost.invocation.active || opcode == nullptr ||
      instruction != current.bytes.data() || thread != current.thread ||
      current.location.generation !=
          liveHost.RegionGeneration(current.location.region)) {
    liveHost.Refuse(ScriptVmRefusal::kMissingCursor, thread);
    return static_cast<std::intptr_t>(ScriptFlowKind::kStop);
  }
  liveHost.invocation.result = opcode(current, liveHost);
  liveHost.invocation.handled = true;
  return static_cast<std::intptr_t>(ScriptFlowKind::kAdvance);
}

std::expected<ScriptSource, ScriptSourceError> LeaseLiveScriptSource(
    ScriptRegion region) {
  return liveHost.sources.Lease(region);
}

std::expected<ScriptDataLease, ScriptSourceError> LeaseLiveDialogStrings() {
  return LeaseData(liveHost.strings);
}

std::expected<ScriptDataLease, ScriptSourceError> LeaseLiveTextBoxFrame(
    int style) {
  if (style < 0 || style >= kTextBoxFrameStyleCount) {
    return std::unexpected(ScriptSourceError::kExtent);
  }
  return LeaseData(liveHost.frames[static_cast<std::size_t>(style)]);
}

std::expected<ScriptDataLease, ScriptSourceError> LeaseLiveVibrationPattern(
    int pattern) {
  if (pattern < 0 || pattern >= kVibrationPatternSlots) {
    return std::unexpected(ScriptSourceError::kExtent);
  }
  return LeaseData(liveHost.patterns[static_cast<std::size_t>(pattern)]);
}

std::expected<void, ScriptSourceError> MountLiveScriptSource(
    ScriptRegion region, std::shared_ptr<const Buffer> source) {
  return liveHost.sources.MountBuffer(region, std::move(source));
}

std::expected<PreparedScriptSource, ScriptSourceError>
PrepareLiveRoomEntrySource(RoomEventLease room) {
  return liveHost.sources.PrepareRoom(ScriptRegion::kEventScript,
                                      std::move(room));
}

std::expected<void, ScriptSourceError> PublishLiveScriptSource(
    PreparedScriptSource&& prepared) {
  return liveHost.sources.Publish(std::move(prepared));
}

RoomEventLease LiveRoomScriptSections() { return liveHost.room; }

void PublishLiveRoomScriptSections(RoomEventLease room) {
  liveHost.room = std::move(room);
}

bool InitializeLiveEventScriptData(ScriptLocation source) {
  return WithSourceUse(
      [source] { return InitializeEventScriptData(source, liveHost); });
}

bool InitializeLiveScriptOpcodeTable() {
  return WithSourceUse([] { return InitializeScriptOpcodeTable(liveHost); });
}

int UpdateLiveEventPlayback() {
  return WithSourceUse([] { return UpdateEventPlayback(liveHost); });
}

bool ExecuteLiveScriptThread(int thread) {
  return WithSourceUse([thread] {
    return ExecuteScriptThread(static_cast<ScriptThreadId>(thread), liveHost);
  });
}

void ReleaseLiveScriptActorControl() { ReleaseScriptActorControl(liveHost); }

void ResetLiveScriptRuntime() {
  liveHost.CancelEventFile();
  liveHost.DiscardChainedEventBuffer();
  liveHost.sources.Reset();
  liveHost.room.reset();
  liveHost.cursors = {};
  liveHost.return_to = {};
  liveHost.strings = {};
  liveHost.frames = {};
  liveHost.patterns = {};
  liveHost.curves = {};
  liveHost.invocation = {};
  liveHost.SetPhase(PlaybackPhase::kIdle);
  liveHost.SetStartConfirmed(false);
  liveHost.SetMainThreadFinished(false);
  liveHost.SetChainedEventLoaded(false);
}

void AbandonLiveScriptUses() {
  liveHost.invocation = {};
  liveHost.sources.AbandonUses();
}

// decomp: BATTLE.PRG resolveActorId (src/BATTLE/BATTLE.PRG/573B8.c:110; was
// func_800BFE50).
int ResolveLiveScriptActorReference(std::uint16_t reference) {
  return ResolveScriptActorReference(reference, liveHost);
}

}  // namespace VS::Event

// decomp: BATTLE.PRG vs_battle_script_nop (src/BATTLE/BATTLE.PRG/4DEA8.c:368;
// was func_800B66E4).
std::intptr_t VS::Event::ScriptNoOperationOpcode(std::uint8_t* instruction,
                                                 std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptNoOperation);
}

// decomp: BATTLE.PRG vs_battle_script_yield (src/BATTLE/BATTLE.PRG/4DEA8.c:373;
// was func_800B66EC).
std::intptr_t VS::Event::ScriptYieldFrameOpcode(std::uint8_t* instruction,
                                                std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptYieldFrame);
}

// decomp: BATTLE.PRG vs_battle_script_F0 (src/BATTLE/BATTLE.PRG/4DEA8.c:3448;
// was func_800BB474).
std::intptr_t VS::Event::CommandWaitFramesOpcode(std::uint8_t* instruction,
                                                 std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::CommandWaitFrames);
}

// decomp: BATTLE.PRG vs_battle_script_F1 (src/BATTLE/BATTLE.PRG/4DEA8.c:3464;
// was func_800BB4CC).
std::intptr_t VS::Event::CommandSetTickSpeedOpcode(std::uint8_t* instruction,
                                                   std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::CommandSetTickSpeed);
}

// decomp: BATTLE.PRG vs_battle_script_F4 (src/BATTLE/BATTLE.PRG/4DEA8.c:3476;
// was func_800BB504).
std::intptr_t VS::Event::CommandWaitScriptContextOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::CommandWaitScriptContext);
}

// decomp: BATTLE.PRG vs_battle_script_F5 (src/BATTLE/BATTLE.PRG/4DEA8.c:3486;
// was func_800BB554).
std::intptr_t VS::Event::CommandStartScriptContextOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::CommandStartScriptContext);
}

// decomp: BATTLE.PRG vs_battle_script_F6 (src/BATTLE/BATTLE.PRG/4DEA8.c:3492;
// was func_800BB5A4).
std::intptr_t VS::Event::CommandStopScriptContextOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::CommandStopScriptContext);
}

// decomp: BATTLE.PRG vs_battle_script_46_switchOnFlag
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1972; was vs_battle_script_setFlagImmContext).
std::intptr_t VS::Event::ScriptCacheFlagValueOpcode(std::uint8_t* instruction,
                                                    std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptCacheFlagValue);
}

// decomp: BATTLE.PRG vs_battle_script_47_jumpFwdCase
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1985; was
// vs_battle_script_jumpFwdIfFlagContext).
std::intptr_t VS::Event::ScriptBranchOnCachedFlagOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptBranchOnCachedFlag);
}

// decomp: BATTLE.PRG vs_battle_script_49_setLoopCounter
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2009; was func_800B8C54).
std::intptr_t VS::Event::ScriptSetLoopCounterOpcode(std::uint8_t* instruction,
                                                    std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetLoopCounter);
}

// decomp: BATTLE.PRG vs_battle_script_4A_loopCondition
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2024; was vs_battle_script_jumpBackIf).
std::intptr_t VS::Event::ScriptLoopBackwardOpcode(std::uint8_t* instruction,
                                                  std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptLoopBackward);
}

// decomp: BATTLE.PRG vs_battle_script_50_modifyFlagImm
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2066; was vs_battle_script_setFlagImm).
std::intptr_t VS::Event::ScriptApplyImmediateFlagOperationOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptApplyImmediateFlagOperation);
}

// decomp: BATTLE.PRG vs_battle_script_51_modifyFlag
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2119; was vs_battle_script_setFlag).
std::intptr_t VS::Event::ScriptApplyFlagPairOperationOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptApplyFlagPairOperation);
}

// decomp: BATTLE.PRG vs_battle_script_40_jumpFwdUnlessFlagImm
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1835; was vs_battle_script_jumpFwdIfFlag).
std::intptr_t VS::Event::ScriptBranchOnFlagImmediateOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptBranchOnFlagImmediate);
}

// decomp: BATTLE.PRG vs_battle_script_41_jumpFwdUnlessFlag
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1899; was vs_battle_script_jumpFwdIfFlags).
std::intptr_t VS::Event::ScriptBranchOnFlagPairOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptBranchOnFlagPair);
}

// decomp: BATTLE.PRG jumpFwd aliases (4A0A8.c:1630/1635/1675/1680).
std::intptr_t VS::Event::ScriptJumpForwardOpcode(std::uint8_t* instruction,
                                                 std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptJumpForward);
}

// decomp: BATTLE.PRG vs_battle_script_48_endSceneReset
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1997; was func_800B8C44).
std::intptr_t VS::Event::ScriptReturnToSavedAddressOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptReturnToSavedAddress);
}

// decomp: BATTLE.PRG vs_battle_script_52_setFlagToItemCount
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2181; was
// vs_battle_script_setFlagIfItemInInventory).
std::intptr_t VS::Event::ScriptStoreInventoryItemPresenceOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptStoreInventoryItemPresence);
}

// decomp: BATTLE.PRG vs_battle_script_56_setFlagToButtons
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2237; was vs_battle_script_setFlagIfButton).
std::intptr_t VS::Event::ScriptStoreButtonStateOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptStoreButtonState);
}

// decomp: BATTLE.PRG vs_battle_script_0D_beginSkippable
// (src/BATTLE/BATTLE.PRG/4DEA8.c:400; was func_800B6744).
std::intptr_t VS::Event::ScriptBeginSkippableCutsceneOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptBeginSkippableCutscene);
}

// decomp: BATTLE.PRG vs_battle_script_0E_endSkippableBody
// (src/BATTLE/BATTLE.PRG/4DEA8.c:447; was func_800B6868).
std::intptr_t VS::Event::ScriptEndCutsceneAndJumpOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptEndCutsceneAndJump);
}

// decomp: BATTLE.PRG vs_battle_script_0F_endSkipHandler
// (src/BATTLE/BATTLE.PRG/4DEA8.c:464; was func_800B68C4).
std::intptr_t VS::Event::ScriptEndCutsceneOpcode(std::uint8_t* instruction,
                                                 std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptEndCutscene);
}

// decomp: BATTLE.PRG vs_battle_script_15_waitButtonPressed
// (src/BATTLE/BATTLE.PRG/4DEA8.c:693; was vs_battle_script_buttonPressed).
std::intptr_t VS::Event::ScriptWaitForButtonAndYieldOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptWaitForButtonAndYield);
}

// decomp: BATTLE.PRG vs_battle_script_5E_waitFramesOrButton
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2458; was func_800B96B0).
std::intptr_t VS::Event::ScriptWaitForButtonOrTimeoutOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptWaitForButtonOrTimeout);
}

// decomp: BATTLE.PRG vs_battle_script_5A_setBattleTimer
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2387; was func_800B94D8).
std::intptr_t VS::Event::ScriptConfigureTimerOpcode(std::uint8_t* instruction,
                                                    std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptConfigureTimer);
}

// decomp: BATTLE.PRG vs_battle_script_5F_defineEasing
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2483; was func_800B9724).
std::intptr_t VS::Event::ScriptConfigureEasingCurveOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptConfigureEasingCurve);
}

// decomp: BATTLE.PRG vs_battle_script_invalidOpcode
// (src/BATTLE/BATTLE.PRG/4DEA8.c:356; was vs_battle_invalidOpcode).
std::intptr_t VS::Event::ScriptReportInvalidOpcodeOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptReportInvalidOpcode);
}

// decomp: BATTLE.PRG vs_battle_script_0A_waitEvtLoad
// (src/BATTLE/BATTLE.PRG/4DEA8.c:378; was func_800B66F4).
std::intptr_t VS::Event::ScriptWaitForEventFileOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptWaitForEventFile);
}

// decomp: BATTLE.PRG vs_battle_script_F7 (src/BATTLE/BATTLE.PRG/4DEA8.c:3494;
// was func_800BB5AC).
std::intptr_t VS::Event::CommandLoadNextEventOpcode(std::uint8_t* instruction,
                                                    std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::CommandLoadNextEvent);
}

// decomp: BATTLE.PRG vs_battle_script_FE_FF
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3503; was func_800BB604).
std::intptr_t VS::Event::CommandFinishEventOpcode(std::uint8_t* instruction,
                                                  std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::CommandFinishEvent);
}

// decomp: BATTLE.PRG vs_battle_script_B7_specialOp
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3300; was vs_battle_script_specialOp).
std::intptr_t VS::Event::CommandSpecialOperationOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::CommandSpecialOperation);
}

// decomp: BATTLE.PRG vs_battle_script_60_setCloudstoneMoving
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2508; was func_800B9770).
std::intptr_t VS::Event::ScriptSetGimmickVisibilityOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetGimmickVisibility);
}

// decomp: BATTLE.PRG vs_battle_script_61_overrideFogDepth
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2520; was func_800B979C).
std::intptr_t VS::Event::ScriptOverrideDepthCueDistanceOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptOverrideDepthCueDistance);
}

// decomp: BATTLE.PRG vs_battle_script_62_setFirstPersonRendering
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2539; was func_800B97E4).
std::intptr_t VS::Event::ScriptSetRoomLightingModeOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetRoomLightingMode);
}

// decomp: BATTLE.PRG vs_battle_script_64_showGeometry
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2559; was func_800B9818).
std::intptr_t VS::Event::ScriptShowGeometryGroupOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptShowGeometryGroup);
}

// decomp: BATTLE.PRG vs_battle_script_65_hideGeometry
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2571; was func_800B983C).
std::intptr_t VS::Event::ScriptHideGeometryGroupOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptHideGeometryGroup);
}

// decomp: BATTLE.PRG vs_battle_script_66_swapGeometryFlags
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2583; was func_800B9860).
std::intptr_t VS::Event::ScriptSwapGeometryGroupFlagsOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSwapGeometryGroupFlags);
}

// decomp: BATTLE.PRG vs_battle_script_67_setTileBlocked
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2598; was func_800B988C).
std::intptr_t VS::Event::ScriptSetCollisionTileModeOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetCollisionTileMode);
}

// decomp: BATTLE.PRG vs_battle_script_7D (src/BATTLE/BATTLE.PRG/4DEA8.c:2916;
// was func_800BA288).
std::intptr_t VS::Event::ScriptAdjustGeometryDepthBiasOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptAdjustGeometryDepthBias);
}

// decomp: BATTLE.PRG vs_battle_script_73 (src/BATTLE/BATTLE.PRG/4DEA8.c:2805;
// was func_800B9F28).
std::intptr_t VS::Event::ScriptSetRoomBackgroundVisibleOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetRoomBackgroundVisible);
}

// decomp: BATTLE.PRG vs_battle_script_74 (src/BATTLE/BATTLE.PRG/4DEA8.c:2811;
// was func_800B9F4C).
std::intptr_t VS::Event::ScriptLoadRoomBackgroundOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptLoadRoomBackground);
}

// decomp: BATTLE.PRG vs_battle_script_75 (src/BATTLE/BATTLE.PRG/4DEA8.c:2817;
// was func_800B9F70).
std::intptr_t VS::Event::ScriptWaitForRoomBackgroundOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptWaitForRoomBackground);
}

// decomp: BATTLE.PRG vs_battle_script_76 (src/BATTLE/BATTLE.PRG/4DEA8.c:2822;
// was func_800B9F90).
std::intptr_t VS::Event::ScriptFreeRoomBackgroundOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptFreeRoomBackground);
}

// decomp: BATTLE.PRG vs_battle_script_7B (src/BATTLE/BATTLE.PRG/4DEA8.c:2904;
// was func_800BA218).
std::intptr_t VS::Event::ScriptSetBackgroundOffsetOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetBackgroundOffset);
}

// decomp: BATTLE.PRG vs_battle_script_7C (src/BATTLE/BATTLE.PRG/4DEA8.c:2910;
// was func_800BA264).
std::intptr_t VS::Event::ScriptSetFirstPersonViewOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetFirstPersonView);
}

// decomp: BATTLE.PRG vs_battle_script_7E (src/BATTLE/BATTLE.PRG/4DEA8.c:2922;
// was func_800BA2B4).
std::intptr_t VS::Event::ScriptSetBackgroundParallaxOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetBackgroundParallax);
}

// decomp: BATTLE.PRG vs_battle_script_69 (src/BATTLE/BATTLE.PRG/4DEA8.c:2636;
// was func_800B9A40).
std::intptr_t VS::Event::ScriptFadeOutAndWaitOpcode(std::uint8_t* instruction,
                                                    std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptFadeOutAndWait);
}

// decomp: BATTLE.PRG vs_battle_script_6A (src/BATTLE/BATTLE.PRG/4DEA8.c:2648;
// was func_800B9A94).
std::intptr_t VS::Event::ScriptFadeInOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(instruction, thread_index,
                                                    VS::Event::ScriptFadeIn);
}

// decomp: BATTLE.PRG vs_battle_script_6B (src/BATTLE/BATTLE.PRG/4DEA8.c:2654;
// was func_800B9AC4).
std::intptr_t VS::Event::ScriptSetWipeAngleOpcode(std::uint8_t* instruction,
                                                  std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetWipeAngle);
}

// decomp: BATTLE.PRG vs_battle_script_6C (src/BATTLE/BATTLE.PRG/4DEA8.c:2660;
// was func_800B9AE8).
std::intptr_t VS::Event::ScriptConfigureScreenTransitionOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptConfigureScreenTransition);
}

// decomp: BATTLE.PRG vs_battle_script_6D (src/BATTLE/BATTLE.PRG/4DEA8.c:2666;
// was func_800B9B18).
std::intptr_t VS::Event::ScriptSetRoomDisplayFlagsOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetRoomDisplayFlags);
}

// decomp: BATTLE.PRG vs_battle_script_6E (src/BATTLE/BATTLE.PRG/4DEA8.c:2697;
// was func_800B9BF8).
std::intptr_t VS::Event::ScriptWaitForScreenTransitionOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptWaitForScreenTransition);
}

// decomp: BATTLE.PRG vs_battle_script_6F (src/BATTLE/BATTLE.PRG/4DEA8.c:2702;
// was func_800B9C18).
std::intptr_t VS::Event::ScriptTogglePaletteHookOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptTogglePaletteHook);
}

// decomp: BATTLE.PRG vs_battle_script_70 (src/BATTLE/BATTLE.PRG/4DEA8.c:2712;
// was func_800B9C58).
std::intptr_t VS::Event::ScriptTransformPalettesOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptTransformPalettes);
}

// decomp: BATTLE.PRG vs_battle_script_0C_waitGimLoad
// (src/BATTLE/BATTLE.PRG/4DEA8.c:390; was func_800B6724).
std::intptr_t VS::Event::ScriptWaitForSplashFileOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptWaitForSplashFile);
}

// decomp: BATTLE.PRG vs_battle_script_16_loadGim
// (src/BATTLE/BATTLE.PRG/4DEA8.c:708; was vs_battle_script_loadGim).
std::intptr_t VS::Event::ScriptLoadSplashImageOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptLoadSplashImage);
}

// decomp: BATTLE.PRG vs_battle_script_17_showGimImage
// (src/BATTLE/BATTLE.PRG/4DEA8.c:725; was func_800B7014).
std::intptr_t VS::Event::ScriptConfigureSplashLayerOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptConfigureSplashLayer);
}

// decomp: BATTLE.PRG vs_battle_script_18_tweenGimImage
// (src/BATTLE/BATTLE.PRG/4DEA8.c:762; was func_800B70E8).
std::intptr_t VS::Event::ScriptTweenSplashLayerOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptTweenSplashLayer);
}

// decomp: BATTLE.PRG vs_battle_script_19_unloadGim
// (src/BATTLE/BATTLE.PRG/4DEA8.c:791; was func_800B71C4).
std::intptr_t VS::Event::ScriptFreeSplashImageOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptFreeSplashImage);
}

// decomp: BATTLE.PRG vs_battle_script_1A_waitGimTweens
// (src/BATTLE/BATTLE.PRG/4DEA8.c:801; was func_800B71F4).
std::intptr_t VS::Event::ScriptWaitForSplashTweenOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptWaitForSplashTween);
}

// decomp: BATTLE.PRG vs_battle_script_77 (src/BATTLE/BATTLE.PRG/4DEA8.c:2828;
// was func_800B9FC0).
std::intptr_t VS::Event::ScriptSetEnvironmentParticlesOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetEnvironmentParticles);
}

// decomp: BATTLE.PRG vs_battle_script_78 (src/BATTLE/BATTLE.PRG/4DEA8.c:2859;
// was func_800BA0E4).
std::intptr_t VS::Event::ScriptSetParticleVariationEnabledOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptSetParticleVariationEnabled);
}

// decomp: BATTLE.PRG vs_battle_script_7A (src/BATTLE/BATTLE.PRG/4DEA8.c:2865;
// was func_800BA108).
std::intptr_t VS::Event::ScriptMuteEnvironmentSoundOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptMuteEnvironmentSound);
}

// decomp: BATTLE.PRG vs_battle_script_79 (src/BATTLE/BATTLE.PRG/4DEA8.c:2886;
// was func_800BA194).
std::intptr_t VS::Event::ScriptControlRoomAnimationOpcode(
    std::uint8_t* instruction, std::int16_t thread_index) {
  return VS::Event::DispatchLiveScriptControlOpcode(
      instruction, thread_index, VS::Event::ScriptControlRoomAnimation);
}
