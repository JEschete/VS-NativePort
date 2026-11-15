// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectRuntime.hpp"

#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/PrimaryEffect.hpp"

namespace VS::Effects {
namespace {
void SetPhase(PrimaryEffectRuntime& runtime, PrimaryEffectPhase phase) {
  PrimaryEffectState state = runtime.Primary();
  SetPrimaryEffectPhase(state, phase);
  runtime.SetPrimary(state);
}

// The pending-track bits of a package with `count` tracks: the original set
// bit `track & 31` of each, so 32 or more tracks set every bit.
[[nodiscard]] std::uint32_t PendingMask(std::uint8_t count) {
  if (count >= kPrimaryEffectTracks) {
    return ~std::uint32_t{0};
  }
  return (std::uint32_t{1} << count) - 1U;
}

// The curve sampling and particle forces that open an instance's frame.
void BeginInstanceFrame(Effects_EffectInstance& instance,
                        EffectInstancePlayback& playback) {
  if (playback.HasCurves()) {
    playback.SampleCurves(playback.Frame(instance));
  }
  playback.UpdateParticleForces();
}

void AdvanceFrame(Effects_EffectInstance& instance,
                  EffectInstancePlayback& playback) {
  playback.SetFrame(instance,
                    static_cast<std::uint16_t>(playback.Frame(instance) + 1U));
}

// Phase kLoadingPurgeOverlay (5BF94.c:5184).
void FinishPurgeOverlay(PrimaryEffectRuntime& runtime) {
  if (!runtime.PollPurgeOverlay()) {
    return;
  }
  runtime.WaitForOverlay();
  runtime.ConfigureResources();
  PrimaryEffectState state = runtime.Primary();
  if (runtime.ActionHasEffect() && !runtime.PackageOnDisc()) {
    SetPrimaryEffectPhase(state, PrimaryEffectPhase::kPlayingTracks);
    state.track_count = 0;
    state.started_tracks = 0;
  } else {
    SetPrimaryEffectPhase(state, PrimaryEffectPhase::kLoadingTextures);
  }
  runtime.SetPrimary(state);
  if (runtime.PurgeOverlayHeld()) {
    runtime.ReleasePurgeOverlay();
  }
}

// Phase kLoadingPackage (5BF94.c:5217).
void FinishPackage(PrimaryEffectRuntime& runtime) {
  if (!runtime.PollPackage()) {
    return;
  }
  runtime.BindPackage();
  SetPhase(runtime, PrimaryEffectPhase::kAwaitingTrack);
  const std::uint8_t count = runtime.PackageTrackCount();
  PrimaryEffectState state = runtime.Primary();
  state.track_count = count;
  state.started_tracks = 0;
  state.pending_tracks |= PendingMask(count);
  state.loaded = true;
  runtime.SetPrimary(state);
  Content_DiscQueueSlot* slot = runtime.RequestMenuReload();
  runtime.SetMenuReloadSlot(slot);
  // The original enqueued a refused (null) request too; it reads nothing and
  // the reload skips it, so it is only kept from the queue (#282).
  if (slot != nullptr) {
    runtime.EnqueueMenuReload(*slot);
  }
}

// The end of phase kPlayingTracks once every track has played
// (5BF94.c:5258).
void ReleaseAction(PrimaryEffectRuntime& runtime) {
  if (runtime.HasSfxBank()) {
    runtime.UnbindSfxBank();
  }
  if (runtime.HasCutsceneStream()) {
    runtime.UnbindCutsceneStream();
  }
  runtime.StopVibration();
  runtime.ReleasePackageResources();
  runtime.ReleaseParticlePool();
  runtime.ReleaseToken(runtime.PrimaryToken());
  PrimaryEffectState state = runtime.Primary();
  state.loaded = false;
  runtime.SetPrimary(state);
  runtime.SetPrimaryToken(0);
  if (runtime.PurgeOverlayHeld()) {
    runtime.ReleasePurgeOverlay();
  }
  runtime.ResetCameraMotion();
  SetPhase(runtime, PrimaryEffectPhase::kReloadingMenu);
}

// Phase kPlayingTracks (5BF94.c:5236).
void PlayTracks(PrimaryEffectRuntime& runtime) {
  Effects_EffectInstance* instance = runtime.FirstPrimary();
  while (instance != nullptr) {
    Effects_EffectInstance* next = runtime.Next(*instance);
    runtime.SetCurrent(*instance);
    BeginInstanceFrame(*instance, runtime);
    if (runtime.RunScript(*instance)) {
      AdvanceFrame(*instance, runtime);
    } else {
      runtime.DestroyPrimary(*instance);
    }
    instance = next;
  }
  if (runtime.FirstPrimary() != nullptr) {
    return;
  }
  const PrimaryEffectState state = runtime.Primary();
  if (state.started_tracks < state.track_count) {
    SetPhase(runtime, PrimaryEffectPhase::kAwaitingTrack);
    return;
  }
  ReleaseAction(runtime);
}

// Phase kReloadingMenu (5BF94.c:5296): false once the reload completes.
[[nodiscard]] bool FinishMenuReload(PrimaryEffectRuntime& runtime) {
  Content_DiscQueueSlot* slot = runtime.MenuReloadSlot();
  if (slot != nullptr) {
    if (!runtime.MenuReloadLoaded(*slot)) {
      return true;
    }
    runtime.ReleaseMenuReload(*slot);
    runtime.SetMenuReloadSlot(nullptr);
  }
  SetPhase(runtime, PrimaryEffectPhase::kIdle);
  return false;
}

// One frame of a secondary instance; returns its busy bits.
[[nodiscard]] std::uint8_t PlaySecondary(Effects_EffectInstance& instance,
                                         SecondaryEffectRuntime& runtime) {
  const std::optional<SecondaryEffectPhase> phase =
      ToSecondaryEffectPhase(runtime.PhaseOf(instance));
  if (phase == SecondaryEffectPhase::kStarting) {
    runtime.AcquireParticlePool();
    runtime.SetPhase(instance, SecondaryEffectPhase::kPlaying);
  } else if (phase == SecondaryEffectPhase::kPlaying) {
    BeginInstanceFrame(instance, runtime);
    Effects_RendererSlot* slot = runtime.FirstRenderer(instance);
    while (slot != nullptr) {
      Effects_RendererSlot* next = runtime.NextRenderer(*slot);
      if (!runtime.UpdateRenderer(*slot)) {
        runtime.DetachRenderer(instance, *slot);
      }
      slot = next;
    }
    if (!runtime.RunScript(instance)) {
      const std::uint8_t busy = runtime.BusyBits(instance);
      runtime.DestroySecondary(instance);
      runtime.ReleaseParticlePool();
      return busy;
    }
    AdvanceFrame(instance, runtime);
  }
  return runtime.BusyBits(instance);
}
}  // namespace

// decomp: func_800CE67C (src/BATTLE/BATTLE.PRG/5BF94.c:5054)
void ResetEffectInstances(EffectRuntimeRecords& records) {
  SetPrimaryEffectPhase(records.primary, PrimaryEffectPhase::kIdle);
  records.primary.started_tracks = 0;
  records.primary.track_count = 0;
  records.primary.loaded = false;
  records.primary_token = 0;
  records.secondary_instances = nullptr;
  records.current_instance = nullptr;
  records.unused_primary_word = 0;
  records.primary_instances = nullptr;
  records.first_curve_sample = 0;
  records.action_variant = 0;
  records.menu_reload_slot = nullptr;
  records.tokens = {};
}

// decomp: func_800CE64C (src/BATTLE/BATTLE.PRG/5BF94.c:5047)
void InitEffectRuntime(EffectRuntimeStartup& startup) {
  startup.ResetInstances();
  startup.InitializeResources();
  startup.ResetParticlePoolReferences();
}

// decomp: func_800CE9B0 (src/BATTLE/BATTLE.PRG/5BF94.c:5170)
bool UpdatePrimaryEffects(PrimaryEffectRuntime& runtime) {
  switch (runtime.Primary().phase) {
    case PrimaryEffectPhase::kIdle:
      return false;
    case PrimaryEffectPhase::kLoadingPurgeOverlay:
      FinishPurgeOverlay(runtime);
      return true;
    case PrimaryEffectPhase::kLoadingTextures:
      if (runtime.PollTextures()) {
        SetPhase(runtime, PrimaryEffectPhase::kLoadingPlugins);
      }
      return true;
    case PrimaryEffectPhase::kLoadingPlugins:
      if (runtime.PollPlugins()) {
        SetPhase(runtime, PrimaryEffectPhase::kLoadingPackage);
        runtime.WaitForOverlay();
      }
      return true;
    case PrimaryEffectPhase::kLoadingPackage:
      FinishPackage(runtime);
      return true;
    case PrimaryEffectPhase::kPlayingTracks:
      PlayTracks(runtime);
      return true;
    case PrimaryEffectPhase::kReloadingMenu:
      return FinishMenuReload(runtime);
    case PrimaryEffectPhase::kAwaitingTrack:
      return true;
  }
  // A phase number the runtime never sets.
  return true;
}

// decomp: func_800CED60 (src/BATTLE/BATTLE.PRG/5BF94.c:5321)
std::uint8_t UpdateSecondaryEffects(SecondaryEffectRuntime& runtime) {
  std::uint8_t busy = 0;
  Effects_EffectInstance* instance = runtime.FirstSecondary();
  while (instance != nullptr) {
    runtime.SetCurrent(*instance);
    Effects_EffectInstance* next = runtime.Next(*instance);
    busy |= PlaySecondary(*instance, runtime);
    instance = next;
  }
  return busy;
}

// decomp: func_800CEF74 (src/BATTLE/BATTLE.PRG/5BF94.c:5399)
TrackStart StartEffectTrack(int track, PrimaryEffectRuntime& runtime) {
  runtime.SelectPackage(ActivePackage::kAction);
  if (!CanStartTrack(runtime.Primary())) {
    return TrackStart::kNotReady;
  }
  if (track < 0 || track >= kPrimaryEffectTracks) {
    return TrackStart::kRefused;
  }
  const std::uint32_t bit = std::uint32_t{1} << static_cast<unsigned>(track);
  if ((runtime.Primary().pending_tracks & bit) == 0) {
    return TrackStart::kNotPending;
  }
  if (!runtime.SpawnTrack(track * kTrackScriptStride, runtime.PrimaryToken())) {
    return TrackStart::kRefused;
  }
  SetPhase(runtime, PrimaryEffectPhase::kPlayingTracks);
  PrimaryEffectState state = runtime.Primary();
  state.started_tracks = Core::Math::WrappingAdd(state.started_tracks, 1);
  state.pending_tracks &= ~bit;
  runtime.SetPrimary(state);
  return TrackStart::kStarted;
}

// decomp: func_800CF060 (src/BATTLE/BATTLE.PRG/5BF94.c:5427)
std::uint8_t UpdateEffectRuntime(EffectRuntimeFrame& frame) {
  frame.SetBufferIndex(static_cast<std::uint8_t>(1U - frame.BufferIndex()));
  frame.SelectPackage(ActivePackage::kAction);
  const bool primary_busy = frame.UpdatePrimary();
  frame.SelectPackage(ActivePackage::kSecondary);
  const std::uint8_t secondary_busy = frame.UpdateSecondary();
  frame.SelectPackage(ActivePackage::kAction);
  if (frame.PrimaryPhase() == PrimaryEffectPhase::kPlayingTracks) {
    frame.UpdateCameraMotion();
  }
  return static_cast<std::uint8_t>((primary_busy ? 1U : 0U) | secondary_busy);
}

// decomp: func_800CF0E8 (src/BATTLE/BATTLE.PRG/5BF94.c:5444)
std::uint8_t BeginActionEffect(const ActionEffectStart& start,
                               PrimaryEffectRuntime& runtime) {
  runtime.StoreActionRequest();
  if (start.variant != 0 && start.source_type == kActorBoneEndpoint) {
    runtime.SetActionVariant(static_cast<std::uint8_t>(start.variant),
                             static_cast<std::uint8_t>(start.argument));
  }
  runtime.SetPrimaryToken(runtime.AllocateToken());
  runtime.SelectPackage(ActivePackage::kAction);
  runtime.CaptureSavedCamera();
  runtime.CaptureEffectCamera();
  SetPhase(runtime, PrimaryEffectPhase::kLoadingPurgeOverlay);
  return runtime.PrimaryToken();
}

// decomp: func_800CF1A8 (src/BATTLE/BATTLE.PRG/5BF94.c:5462)
void ClearPrimaryEffects(PrimaryEffectRuntime& runtime) {
  Effects_EffectInstance* instance = runtime.FirstPrimary();
  runtime.StopVibration();
  while (instance != nullptr) {
    Effects_EffectInstance* next = runtime.Next(*instance);
    if (runtime.HasTimeline(*instance)) {
      runtime.FreeTimeline(*instance);
    }
    runtime.DestroyPrimary(*instance);
    instance = next;
  }
}
}  // namespace VS::Effects
