// Native side of the event-script room opcode differentials (event_room_commands_common.py, #276).
// One vector per line: mode, the backdrop byte and draw mode word before the opcode, the cutscene
// skip byte, the word the background callees return, whether the room lacks an ambient section, the
// previous ambient sound, the muted ambience byte and saved sound, the weather tween's eleven fields,
// the instruction's length and its bytes. Prints the flow kind, the refusals, the backdrop byte, the
// draw mode word, the tween's fields, the muted ambience byte and the saved sound after the opcode,
// then the room calls as [count, kind and arguments of each call...].
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Event/ScriptRoomCommands.hpp"
#include "Event/ScriptVm.hpp"

namespace {
using namespace VS::Event;

// The handlers event_room_commands_common.py compares, by mode.
constexpr ScriptFlow (*kHandlers[])(const ScriptInstruction&, ScriptVmHost&) = {
    ScriptSetGimmickVisibility,
    ScriptOverrideDepthCueDistance,
    ScriptSetRoomLightingMode,
    ScriptShowGeometryGroup,
    ScriptHideGeometryGroup,
    ScriptSwapGeometryGroupFlags,
    ScriptSetCollisionTileMode,
    ScriptAdjustGeometryDepthBias,
    ScriptSetRoomBackgroundVisible,
    ScriptLoadRoomBackground,
    ScriptWaitForRoomBackground,
    ScriptFreeRoomBackground,
    ScriptSetBackgroundOffset,
    ScriptSetFirstPersonView,
    ScriptSetBackgroundParallax,
    ScriptSetEnvironmentParticles,
    ScriptSetParticleVariationEnabled,
    ScriptMuteEnvironmentSound,
    ScriptControlRoomAnimation,
};

// The call kinds the harness records for the stubbed callees.
enum Call : int {
    kCallPlatform = 1,
    kCallLodDepth,
    kCallShow,
    kCallHide,
    kCallSwap,
    kCallCollision,
    kCallDepthBias,
    kCallBackgroundEnabled,
    kCallBackgroundLoad,
    kCallBackgroundFinish,
    kCallBackgroundRelease,
    kCallBackgroundOffset,
    kCallFirstPerson,
    kCallParallax,
    kCallWeather,
    kCallIntensity,
    kCallWind,
    kCallVariation,
    kCallAmbientAbsent,
    kCallAmbientSound,
    kCallEffectActive,
    kCallFinishRotation,
    kCallStartEffect,
    kCallToggleEffect,
};

// The backdrop byte stores 0xFF or 0 as LiveVmHost does; the draw mode word takes the mode as passed.
// The background load reports its last step as func_8008F0FC does, by a nonzero result, and so does
// func_80093764 for an active room effect. The muted ambience byte stores 1 or 0 as LiveVmHost does.
class RoomHost final : public ScriptVmHost {
public:
    int refusals = 0;
    int backdrop = 0;
    std::int32_t draw_mode = 0;
    std::uint8_t skip_state = 0;
    std::int32_t result = 0;
    int absent = 0;
    std::int32_t previous = 0;
    int suspended = 0;
    std::int16_t saved_sound = 0;
    WeatherTween tween;
    std::vector<std::vector<long long>> calls;

    void Refuse(ScriptVmRefusal, int) override { ++refusals; }
    void SetScriptBackdropEnabled(bool enabled) override { backdrop = enabled ? 0xFF : 0; }
    void SetMovingPlatformVisibility(int platform, int visibility) override
    {
        calls.push_back({ kCallPlatform, platform, visibility });
    }
    void SetRoomTextureLodDepth(int depth) override { calls.push_back({ kCallLodDepth, depth }); }
    void SetRoomGeometryDrawMode(int mode) override { draw_mode = mode; }
    void ShowRoomGeometryGroup(int group) override { calls.push_back({ kCallShow, group }); }
    void HideRoomGeometryGroup(int group) override { calls.push_back({ kCallHide, group }); }
    void SwapRoomGeometryGroupFlags(int first_group, int second_group) override
    {
        calls.push_back({ kCallSwap, first_group, second_group });
    }
    void SetCollisionCellBlocked(const CollisionCellChange& change) override
    {
        calls.push_back({ kCallCollision, change.tile_x, change.tile_z, change.layer, change.blocked });
    }
    void AddRoomGeometryDepthBias(int group, int depth_bias) override
    {
        calls.push_back({ kCallDepthBias, group, depth_bias });
    }
    void SetTiledBackgroundEnabled(int enabled) override { calls.push_back({ kCallBackgroundEnabled, enabled }); }
    void QueueTiledBackgroundLoad(int texture_page) override
    {
        calls.push_back({ kCallBackgroundLoad, texture_page });
    }
    bool FinishTiledBackgroundLoad() override
    {
        calls.push_back({ kCallBackgroundFinish });
        return result != 0;
    }
    void ReleaseTiledBackground() override { calls.push_back({ kCallBackgroundRelease }); }
    void SetRoomBackgroundOffset(int offset_x, int offset_y) override
    {
        calls.push_back({ kCallBackgroundOffset, offset_x, offset_y });
    }
    void SetBackgroundParallaxScale(int parallax_x, int parallax_y) override
    {
        calls.push_back({ kCallParallax, parallax_x, parallax_y });
    }
    void SetFirstPersonViewDisabled(bool disabled) override
    {
        calls.push_back({ kCallFirstPerson, disabled ? 1 : 0 });
    }
    CutsceneSkip SkipState() const override { return static_cast<CutsceneSkip>(skip_state); }
    void SetRoomWeather(int kind) override { calls.push_back({ kCallWeather, kind }); }
    void SetWeatherIntensity(int intensity) override { calls.push_back({ kCallIntensity, intensity }); }
    void SetWeatherWindAngles(int direction, int strength) override
    {
        calls.push_back({ kCallWind, direction, strength });
    }
    void SetWeatherVariationEnabled(int enabled) override { calls.push_back({ kCallVariation, enabled }); }
    WeatherTween EventWeatherTween() const override { return tween; }
    void SetEventWeatherTween(const WeatherTween& value) override { tween = value; }
    bool AmbientSoundSuspended() const override { return suspended != 0; }
    void SetAmbientSoundSuspended(bool value) override { suspended = value ? 1 : 0; }
    std::int16_t SuspendedAmbientSound() const override { return saved_sound; }
    void SetSuspendedAmbientSound(std::int16_t sound) override { saved_sound = sound; }
    bool RoomAmbientSequenceAbsent() override
    {
        calls.push_back({ kCallAmbientAbsent });
        return absent != 0;
    }
    int ExchangeWeatherAmbientSound(int sound) override
    {
        calls.push_back({ kCallAmbientSound, sound });
        return previous;
    }
    bool RoomEffectActive(int effect) override
    {
        calls.push_back({ kCallEffectActive, effect });
        return result != 0;
    }
    void FinishRoomDoorRotation(int effect) override { calls.push_back({ kCallFinishRotation, effect }); }
    void StartRoomEffect(int effect) override { calls.push_back({ kCallStartEffect, effect }); }
    void ToggleRoomEffectUpdates(int effect) override { calls.push_back({ kCallToggleEffect, effect }); }

    // Everything else the room opcodes must not touch.
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
    void DisableDisplay() override { }
    void ReturnAllActorsToIdle() override { }
    void ResetActorMovement(int) override { }
    bool ActorPresent(int) override { return {}; }
    void PlayActorIdleAnimation(int) override { }
    void StopEventMusic() override { }
    void StopAllSfx() override { }
    void StopScriptVibration() override { }
    void RestartRoomEffectSound() override { }
    int ScreenTransitionPhase() override { return {}; }
    void BeginScreenFadeOut(const ScreenTransitionRequest&) override { }
    void BeginScreenFadeIn(const ScreenTransitionRequest&) override { }
    void SetTransitionWipeAngle(int) override { }
    void ConfigureScreenTransition(const ScreenTransitionRequest&) override { }
    void EnableDisplay() override { }
    void ShowPrimaryRoomGeometry() override { }
    void HidePrimaryRoomGeometry() override { }
    void EnableAllActorModels() override { }
    void DisableAllActorModels() override { }
    void RunPaletteHook(PaletteHook) override { }
    void BeginScriptActorSelection(std::uint16_t) override { }
    std::uint16_t NextScriptSelectedActor() override { return 1; }
    void TransformActorColors(const PaletteTransform&, int) override { }
    void TransformSceneColors(const PaletteTransform&) override { }
    void TransformBackgroundColors(const PaletteTransform&) override { }
    bool SplashImageLoading() override { return {}; }
    bool SplashImagesPresent() const override { return {}; }
    void QueueSplashImage(int, int) override { }
    void ReleaseSplashImages() override { }
    void ConfigureSplashLayer(int, int, const ImageLayerPlacement&) override { }
    std::optional<ImageLayerPlacement> SplashLayerPlacement(int) override { return {}; }
    SplashTween SplashImageTween(int) const override { return {}; }
    void SetSplashImageTween(int, const SplashTween&) override { }
};

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

bool ReadTween(WeatherTween& tween)
{
    return Read(tween.elapsed) && Read(tween.duration) && Read(tween.wind_direction)
        && Read(tween.start_wind_direction) && Read(tween.delta_wind_direction) && Read(tween.wind_strength)
        && Read(tween.start_wind_strength) && Read(tween.delta_wind_strength) && Read(tween.intensity)
        && Read(tween.start_intensity) && Read(tween.delta_intensity);
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::size_t mode = 0;
    while (Read(mode)) {
        RoomHost host;
        std::size_t length = 0;
        if (mode >= std::size(kHandlers) || !Read(host.backdrop) || !Read(host.draw_mode) || !Read(host.skip_state)
            || !Read(host.result) || !Read(host.absent) || !Read(host.previous) || !Read(host.suspended)
            || !Read(host.saved_sound) || !ReadTween(host.tween) || !Read(length))
            return 1;
        std::vector<std::uint8_t> bytes(length);
        for (std::uint8_t& byte : bytes) {
            if (!Read(byte))
                return 1;
        }
        const ScriptInstruction instruction { .bytes = std::span<const std::uint8_t>(bytes) };
        const ScriptFlow flow = kHandlers[mode](instruction, host);
        std::cout << static_cast<int>(flow.kind) << ' ' << host.refusals << ' ' << host.backdrop << ' '
                  << host.draw_mode;
        const WeatherTween& tween = host.tween;
        for (const int field : { int { tween.elapsed }, int { tween.duration }, int { tween.wind_direction },
                 int { tween.start_wind_direction }, int { tween.delta_wind_direction }, int { tween.wind_strength },
                 int { tween.start_wind_strength }, int { tween.delta_wind_strength }, int { tween.intensity },
                 int { tween.start_intensity }, int { tween.delta_intensity } })
            std::cout << ' ' << field;
        std::cout << ' ' << host.suspended << ' ' << host.saved_sound << ' ' << host.calls.size();
        for (const auto& call : host.calls) {
            for (const long long word : call)
                std::cout << ' ' << word;
        }
        std::cout << '\n';
    }
    return 0;
}
