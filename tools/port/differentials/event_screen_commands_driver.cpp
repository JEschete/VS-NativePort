// Native side of the event-script fade, display and palette opcode differentials
// (event_screen_commands_common.py, #276). One vector per line: mode, the transition phase, the
// actor selection's id count and ids (the sentinel included), the instruction's length and its
// bytes. Prints the flow kind, the refusals, then the screen calls as
// [count, kind and arguments of each call...].
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "Event/ScriptScreenCommands.hpp"
#include "Event/ScriptVm.hpp"

namespace {
using namespace VS::Event;

// The handlers event_screen_commands_common.py compares, by mode.
constexpr ScriptFlow (*kHandlers[])(const ScriptInstruction&, ScriptVmHost&) = {
    ScriptFadeOutAndWait,
    ScriptFadeIn,
    ScriptSetWipeAngle,
    ScriptConfigureScreenTransition,
    ScriptSetRoomDisplayFlags,
    ScriptWaitForScreenTransition,
    ScriptTogglePaletteHook,
    ScriptTransformPalettes,
};

// The call kinds the harness records for the stubbed callees.
enum Call : int {
    kCallFadeOut = 1,
    kCallFadeIn,
    kCallWipeAngle,
    kCallConfigure,
    kCallDisableDisplay,
    kCallEnableDisplay,
    kCallHideRoom,
    kCallShowRoom,
    kCallHideActors,
    kCallShowActors,
    kCallNonzeroHook,
    kCallZeroHook,
    kCallSelect,
    kCallActorColors,
    kCallSceneColors,
    kCallBackgroundColors,
    kCallPhase,
    kCallNextActor,
};

// The selection returns the vector's ids as the live selection's 16-bit result, then the sentinel.
class ScreenHost final : public ScriptVmHost {
public:
    int refusals = 0;
    int phase = 0;
    std::vector<long long> actors;
    std::size_t cursor = 0;
    std::vector<std::vector<long long>> calls;

    void Refuse(ScriptVmRefusal, int) override { ++refusals; }
    int ScreenTransitionPhase() override
    {
        calls.push_back({ kCallPhase });
        return phase;
    }
    void BeginScreenFadeOut(const ScreenTransitionRequest& request) override
    {
        calls.push_back({ kCallFadeOut, request.effect, request.speed, request.wipe_angle });
    }
    void BeginScreenFadeIn(const ScreenTransitionRequest& request) override
    {
        calls.push_back({ kCallFadeIn, request.effect, request.speed, request.wipe_angle });
    }
    void SetTransitionWipeAngle(int wipe_angle) override { calls.push_back({ kCallWipeAngle, wipe_angle }); }
    void ConfigureScreenTransition(const ScreenTransitionRequest& request) override
    {
        calls.push_back({ kCallConfigure, request.effect, request.speed, request.wipe_angle });
    }
    void DisableDisplay() override { calls.push_back({ kCallDisableDisplay }); }
    void EnableDisplay() override { calls.push_back({ kCallEnableDisplay }); }
    void HidePrimaryRoomGeometry() override { calls.push_back({ kCallHideRoom }); }
    void ShowPrimaryRoomGeometry() override { calls.push_back({ kCallShowRoom }); }
    void DisableAllActorModels() override { calls.push_back({ kCallHideActors }); }
    void EnableAllActorModels() override { calls.push_back({ kCallShowActors }); }
    void RunPaletteHook(PaletteHook hook) override
    {
        calls.push_back({ hook == PaletteHook::kNonzeroOperand ? kCallNonzeroHook : kCallZeroHook });
    }
    void BeginScriptActorSelection(std::uint16_t selector) override
    {
        calls.push_back({ kCallSelect, selector });
        cursor = 0;
    }
    std::uint16_t NextScriptSelectedActor() override
    {
        calls.push_back({ kCallNextActor });
        return static_cast<std::uint16_t>(cursor < actors.size() ? actors[cursor++] : 1);
    }
    void TransformActorColors(const PaletteTransform& transform, int actor) override
    {
        calls.push_back({ kCallActorColors, transform.mode, transform.duration, actor, transform.red_offset,
            transform.green_offset, transform.blue_offset });
    }
    void TransformSceneColors(const PaletteTransform& transform) override
    {
        calls.push_back({ kCallSceneColors, transform.mode, transform.duration, transform.red_offset,
            transform.green_offset, transform.blue_offset });
    }
    void TransformBackgroundColors(const PaletteTransform& transform) override
    {
        calls.push_back({ kCallBackgroundColors, transform.mode, transform.duration, transform.red_offset,
            transform.green_offset, transform.blue_offset });
    }

    // Everything else the screen opcodes must not touch.
    void NoteInvalidOpcode(const ScriptInstruction&) override { }
    void NoteUndefinedSpecialOperation(int) override { }
    void NoteCutsceneSkip(int) override { }
    std::span<const std::uint8_t> Region(ScriptRegion) const override { return {}; }
    std::uint64_t RegionGeneration(ScriptRegion) const override { return {}; }
    std::span<const std::uint8_t> OpcodeSizes() const override { return {}; }
    ScriptFlow RunOpcode(ScriptLocation, int) override { return {}; }
    std::span<const std::uint8_t> PaletteResetCommand() const override { return {}; }
    std::span<const std::uint8_t> SoundRestoreMarker() const override { return {}; }
    std::span<const std::int16_t> SceneTimerModes() const override { return {}; }
    std::span<const std::int16_t> SceneTimerScopes() const override { return {}; }
    std::span<const std::int16_t> CameraPitchPresets() const override { return {}; }
    PlaybackPhase Phase() const override { return {}; }
    void SetPhase(PlaybackPhase) override { }
    bool StartConfirmed() const override { return {}; }
    void SetStartConfirmed(bool) override { }
    EventSource Source() const override { return {}; }
    void SetSource(EventSource) override { }
    std::int16_t EventId() const override { return {}; }
    void SetEventId(std::int16_t) override { }
    bool MainThreadFinished() const override { return {}; }
    void SetMainThreadFinished(bool) override { }
    bool ChainedEventLoaded() const override { return {}; }
    void SetChainedEventLoaded(bool) override { }
    std::int16_t RoomChangeState() const override { return {}; }
    void SetRoomChangeState(std::int16_t) override { }
    bool FieldCameraRestoreScripted() const override { return {}; }
    void SetFieldCameraRestoreScripted(bool) override { }
    bool ScanlineOverlayEnabled() const override { return {}; }
    void SetScanlineOverlayEnabled(bool) override { }
    bool MenuTransitionActive() const override { return {}; }
    void SetMenuTransitionActive(bool) override { }
    void SetAmbientSoundSuspended(bool) override { }
    ScriptLocation ThreadCursor(int) const override { return {}; }
    void SetThreadCursor(int, ScriptLocation) override { }
    std::int16_t WaitFrames(int) const override { return {}; }
    void SetWaitFrames(int, std::int16_t) override { }
    ScriptLocation SubroutineReturn() const override { return {}; }
    void SetSubroutineReturn(ScriptLocation) override { }
    std::uint8_t LoopCounter(int) const override { return {}; }
    void SetLoopCounter(int, std::uint8_t) override { }
    std::int8_t CachedFlag() const override { return {}; }
    void SetCachedFlag(std::int8_t) override { }
    CutsceneSkip SkipState() const override { return {}; }
    void SetSkipState(CutsceneSkip) override { }
    bool SkipForced() const override { return {}; }
    void SetSkipForced(bool) override { }
    bool StopsMusicOnSkip() const override { return {}; }
    void SetStopsMusicOnSkip(bool) override { }
    void SetDialogStrings(ScriptLocation) override { }
    void SetTextBoxFrameStyle(int, ScriptLocation) override { }
    void SetVibrationPattern(int, ScriptLocation) override { }
    EasingCurve CustomEasingCurve(int) const override { return {}; }
    void SetCustomEasingCurve(int, EasingCurve) override { }
    void SetTextBoxAwaitingClose(int, bool) override { }
    void SetWeaponTransitionPending(bool) override { }
    void SetTextSelectionActive(bool) override { }
    void SetEffectWaitPending(bool) override { }
    void CancelImageTweens() override { }
    void CaptureEventMusicSlot() override { }
    bool ScriptControlledActor(int) const override { return {}; }
    void ClearScriptControlledActors() override { }
    bool BindRoomDoorSource() override { return {}; }
    EventFileStatus EventFile() const override { return {}; }
    bool QueueEventFileLoad(int, EventBuffer) override { return {}; }
    void ReleaseEventFile() override { }
    void CancelEventFile() override { }
    void ClearEventFile() override { }
    bool AllocateChainedEventBuffer() override { return {}; }
    bool AdoptChainedEvent() override { return {}; }
    void DiscardChainedEventBuffer() override { }
    bool AllocateEventBuffers() override { return {}; }
    std::uint8_t Flag(int) const override { return {}; }
    void SetFlag(int, std::uint8_t) override { }
    std::uint32_t HeldButtons() const override { return {}; }
    void ClearHeldButtons() override { }
    bool StartPressed() const override { return {}; }
    std::uint8_t PressedButtonsLowByte() const override { return {}; }
    int PreviousButtons() const override { return {}; }
    int CountInventoryItem(int) override { return {}; }
    void StartSceneTimer(int, int, int) override { }
    void ResetSceneTimer() override { }
    void SetTickSpeed(int) override { }
    bool DismissTextBox(int) override { return {}; }
    bool UpdateRewardSequence() override { return {}; }
    void BeginEventRewardResults() override { }
    void RunEndingAndReturnToTitle() override { }
    bool IsActionUnlocked(int) override { return {}; }
    void UnlockAction(int) override { }
    int CountActorsByDisposition(int) override { return {}; }
    int FindActorByModelId(int) override { return {}; }
    void ApplySpecialOperation(SpecialOperation, int) override { }
    void InitializeEventCamera() override { }
    void FreeEventCamera() override { }
    void UpdateEventCameraOrigin() override { }
    void RestoreEventFieldCamera() override { }
    void UpdateEventCameraTracks() override { }
    void UpdateEventScreenTransforms() override { }
    void ApplyEventCameraState() override { }
    std::uint8_t EventCameraCompletionMask() override { return {}; }
    void UpdateImageTweens() override { }
    void UpdateWeatherTween() override { }
    void DrawScanlineOverlay() override { }
    void TransformPalettes(ScriptCommand) override { }
    void MoveTextureRectangle(const TextureMove&) override { }
    void BeginSkipFadeOut() override { }
    void ReturnAllActorsToIdle() override { }
    void ResetActorMovement(int) override { }
    bool ActorPresent(int) override { return {}; }
    void PlayActorIdleAnimation(int) override { }
    void StopEventMusic() override { }
    void StopAllSfx() override { }
    void StopScriptVibration() override { }
    void RestartRoomEffectSound() override { }
    void SetScriptBackdropEnabled(bool) override { }
    void SetMovingPlatformVisibility(int, int) override { }
    void SetRoomTextureLodDepth(int) override { }
    void SetRoomGeometryDrawMode(int) override { }
    void ShowRoomGeometryGroup(int) override { }
    void HideRoomGeometryGroup(int) override { }
    void SwapRoomGeometryGroupFlags(int, int) override { }
    void SetCollisionCellBlocked(const CollisionCellChange&) override { }
    void AddRoomGeometryDepthBias(int, int) override { }
    void SetTiledBackgroundEnabled(int) override { }
    void QueueTiledBackgroundLoad(int) override { }
    bool FinishTiledBackgroundLoad() override { return false; }
    void ReleaseTiledBackground() override { }
    void SetRoomBackgroundOffset(int, int) override { }
    void SetBackgroundParallaxScale(int, int) override { }
    void SetFirstPersonViewDisabled(bool) override { }
};

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::size_t mode = 0;
    while (Read(mode)) {
        ScreenHost host;
        std::size_t count = 0;
        if (mode >= std::size(kHandlers) || !Read(host.phase) || !Read(count))
            return 1;
        host.actors.resize(count);
        for (long long& actor : host.actors) {
            if (!Read(actor))
                return 1;
        }
        std::size_t length = 0;
        if (!Read(length))
            return 1;
        std::vector<std::uint8_t> bytes(length);
        for (std::uint8_t& byte : bytes) {
            if (!Read(byte))
                return 1;
        }
        const ScriptInstruction instruction { .bytes = std::span<const std::uint8_t>(bytes) };
        const ScriptFlow flow = kHandlers[mode](instruction, host);
        std::cout << static_cast<int>(flow.kind) << ' ' << host.refusals << ' ' << host.calls.size();
        for (const auto& call : host.calls) {
            for (const long long word : call)
                std::cout << ' ' << word;
        }
        std::cout << '\n';
    }
    return 0;
}
