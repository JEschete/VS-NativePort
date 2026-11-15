// Native side of the event-script splash image differentials (event_splash_commands_common.py, #276).
// One vector per line: mode, whether the image set is present, each layer's flag word, x, y and scale,
// each tween record's fields after its reserved byte, the skip byte, the instruction's length and its
// bytes. Prints the flow kind, the refusals, the layers and tween records after the opcode, then the
// image calls as [count, kind and arguments of each call...].
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Event/ScriptSplashCommands.hpp"
#include "Event/ScriptVm.hpp"

namespace {
using namespace VS::Event;

// The handlers event_splash_commands_common.py compares, by mode.
constexpr ScriptFlow (*kHandlers[])(const ScriptInstruction&, ScriptVmHost&) = {
    ScriptWaitForSplashFile,
    ScriptLoadSplashImage,
    ScriptConfigureSplashLayer,
    ScriptTweenSplashLayer,
    ScriptFreeSplashImage,
    ScriptWaitForSplashTween,
};

// The call kinds the harness records for the stubbed callees.
enum Call : int {
    kCallLoad = 1,
    kCallActivate,
    kCallRelease,
};

constexpr std::size_t kLayers = 3;
constexpr std::uint32_t kLoadingBit = 1U << 1;
constexpr std::uint32_t kRoutineMask = 7U << 3;
constexpr int kRoutineShift = 3;
constexpr int kBrightnessShift = 24;

// A layer as the original's 0x3F8-byte record holds it: the flag word (routine in bits 3-5, brightness
// in bits 24-31) and the placement halfwords.
struct Layer {
    std::uint32_t word = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t scale = 0;
};

// The image set and tween records stored as the original's memory held them.
class SplashHost final : public ScriptVmHost {
public:
    int refusals = 0;
    bool present = false;
    std::array<Layer, kLayers> layers {};
    std::array<SplashTween, kLayers> tweens {};
    int skip = 0;
    std::vector<std::vector<long long>> calls;

    void Refuse(ScriptVmRefusal, int) override { ++refusals; }
    bool SplashImageLoading() override { return (layers[0].word & kLoadingBit) != 0; }
    bool SplashImagesPresent() const override { return present; }
    void QueueSplashImage(int image_id, int first_page) override
    {
        calls.push_back({ kCallLoad, image_id, first_page });
    }
    void ReleaseSplashImages() override { calls.push_back({ kCallRelease }); }
    void ConfigureSplashLayer(int layer, int routine, const ImageLayerPlacement& placement) override
    {
        Layer& target = layers.at(static_cast<std::size_t>(layer));
        target.word = (target.word & ~kRoutineMask) | (static_cast<std::uint32_t>(routine) << kRoutineShift);
        target.word = (target.word & 0x00FFFFFFU) | (static_cast<std::uint32_t>(placement.brightness & 0xFF) << kBrightnessShift);
        target.x = static_cast<std::int16_t>(placement.x);
        target.y = static_cast<std::int16_t>(placement.y);
        target.scale = static_cast<std::int16_t>(placement.scale);
        calls.push_back({ kCallActivate, layer });
    }
    std::optional<ImageLayerPlacement> SplashLayerPlacement(int layer) override
    {
        if (!present || layer < 0 || layer >= static_cast<int>(kLayers))
            return std::nullopt;
        const Layer& source = layers[static_cast<std::size_t>(layer)];
        return ImageLayerPlacement { .x = source.x,
            .y = source.y,
            .scale = source.scale,
            .brightness = static_cast<int>(source.word >> kBrightnessShift) };
    }
    SplashTween SplashImageTween(int layer) const override { return tweens.at(static_cast<std::size_t>(layer)); }
    void SetSplashImageTween(int layer, const SplashTween& tween) override
    {
        tweens.at(static_cast<std::size_t>(layer)) = tween;
    }
    CutsceneSkip SkipState() const override { return static_cast<CutsceneSkip>(skip); }

    // The room opcodes' services.
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

    // Everything else the splash opcodes must not touch.
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
};

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

bool ReadTween(SplashTween& tween)
{
    return Read(tween.easing_mode) && Read(tween.elapsed) && Read(tween.duration) && Read(tween.start_x)
        && Read(tween.start_y) && Read(tween.delta_x) && Read(tween.delta_y) && Read(tween.start_scale)
        && Read(tween.delta_scale) && Read(tween.start_brightness) && Read(tween.delta_brightness);
}

void PrintTween(const SplashTween& tween)
{
    std::cout << ' ' << int { tween.easing_mode } << ' ' << int { tween.elapsed } << ' ' << int { tween.duration }
              << ' ' << tween.start_x << ' ' << tween.start_y << ' ' << tween.delta_x << ' ' << tween.delta_y << ' '
              << tween.start_scale << ' ' << tween.delta_scale << ' ' << tween.start_brightness << ' '
              << tween.delta_brightness;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::size_t mode = 0;
    while (Read(mode)) {
        SplashHost host;
        if (mode >= std::size(kHandlers) || !Read(host.present))
            return 1;
        for (Layer& layer : host.layers) {
            if (!Read(layer.word) || !Read(layer.x) || !Read(layer.y) || !Read(layer.scale))
                return 1;
        }
        for (SplashTween& tween : host.tweens) {
            if (!ReadTween(tween))
                return 1;
        }
        std::size_t length = 0;
        if (!Read(host.skip) || !Read(length))
            return 1;
        std::vector<std::uint8_t> bytes(length);
        for (std::uint8_t& byte : bytes) {
            if (!Read(byte))
                return 1;
        }
        const ScriptInstruction instruction { .bytes = std::span<const std::uint8_t>(bytes) };
        const ScriptFlow flow = kHandlers[mode](instruction, host);
        std::cout << static_cast<int>(flow.kind) << ' ' << host.refusals;
        for (const Layer& layer : host.layers)
            std::cout << ' ' << layer.word << ' ' << layer.x << ' ' << layer.y << ' ' << layer.scale;
        for (const SplashTween& tween : host.tweens)
            PrintTween(tween);
        std::cout << ' ' << host.calls.size();
        for (const auto& call : host.calls) {
            for (const long long word : call)
                std::cout << ' ' << word;
        }
        std::cout << '\n';
    }
    return 0;
}
