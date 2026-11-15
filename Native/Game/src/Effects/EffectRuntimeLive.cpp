// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectRuntimeLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Audio/SoundBanksLive.hpp"
#include "Audio/StreamControlLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/OverlayFiles.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/CameraLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/EffectInstancesLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/EffectRuntime.hpp"
#include "Effects/EffectScriptLive.hpp"
#include "Effects/PrimaryEffect.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Input/ControllerLive.hpp"

extern "C" {
#include "DiscQueue.h"
#include "Effects/CameraLiveState.h"
#include "Effects/EffectResourcesLiveState.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
}

static_assert(std::extent_v<decltype(Effects_tokenBitmaps)> >=
              std::size_t{VS::Effects::kEffectTokenWords});

namespace VS::Effects {
namespace {
using Instance = Effects_EffectInstance;
using Slot = Effects_RendererSlot;

// The particles Effects_AcquireParticlePool reserves for a secondary
// instance.
constexpr int kSecondaryParticles = 0xFF;
// ReleaseLiveOverlayWorkspace's owner tag for the purge overlay.
constexpr int kPurgeOverlayOwner = 2;
// StopLiveVibrationPatterns: every pattern the battle started.
constexpr std::uint32_t kAllVibrationPatterns = 0;
constexpr int kBattleVibrationOwner = 1;

std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refused_calls)) {
    VS_LOG_WARN("effects", "effect runtime: {} refused: {} ({} refusals)", call,
                reason, refused_calls);
  }
}

// The current package; null, with a diagnostic, when there is none.
[[nodiscard]] Effects_EffectPackage* CurrentPackage(std::string_view call) {
  if (Effects_package == nullptr) {
    Refuse(call, "no current effect package");
  }
  return Effects_package;
}

[[nodiscard]] std::uint8_t StoredToken() {
  return static_cast<std::uint8_t>(Effects_primaryToken);
}

// An instance's frame, as both chains play it.
template <class Host>
class LivePlayback : public Host {
 public:
  Instance* Next(Instance& instance) override { return instance.next; }
  void SetCurrent(Instance& instance) override {
    Effects_currentInstance = &instance;
  }
  bool HasCurves() override {
    const Effects_EffectPackage* package = CurrentPackage("curve sampling");
    return package != nullptr && package->curveBlock != nullptr;
  }
  void SampleCurves(std::uint16_t frame) override {
    LiveSampleAllEffectCurves(frame);
  }
  void UpdateParticleForces() override { LiveUpdateParticleGravityAndDrag(); }
  bool RunScript(Instance& instance) override {
    return LiveRunEffectScript(&instance) != 0;
  }
  std::uint16_t Frame(Instance& instance) override {
    return static_cast<std::uint16_t>(instance.frame);
  }
  void SetFrame(Instance& instance, std::uint16_t frame) override {
    instance.frame = frame;
  }
  void ReleaseParticlePool() override { Effects_ReleaseParticlePool(); }
  void SelectPackage(ActivePackage package) override {
    LiveSelectEffectResourceContext(std::to_underlying(package));
  }
};

class LivePrimary final : public LivePlayback<PrimaryEffectRuntime> {
 public:
  LivePrimary() = default;
  explicit LivePrimary(const Effects_ActionEffectRequest& request)
      : request_(&request) {}

  PrimaryEffectState Primary() override { return LivePrimaryEffectState(); }
  void SetPrimary(const PrimaryEffectState& state) override {
    StoreLivePrimaryEffectState(state);
  }
  std::uint8_t PrimaryToken() override { return StoredToken(); }
  void SetPrimaryToken(std::uint8_t token) override {
    Effects_primaryToken = static_cast<char>(token);
  }
  Instance* FirstPrimary() override { return Effects_primaryInstances; }
  Content_DiscQueueSlot* MenuReloadSlot() override {
    return Effects_menuReloadSlot;
  }
  void SetMenuReloadSlot(Content_DiscQueueSlot* slot) override {
    Effects_menuReloadSlot = slot;
  }

  bool PollPurgeOverlay() override { return LiveLoadEffectPurgeOverlay() == 0; }
  bool PollTextures() override { return LiveUpdateEffectTextureLoading() == 0; }
  bool PollPlugins() override { return LiveUpdateEffectPluginLoading() == 0; }
  bool PollPackage() override { return LiveLoadEffectPackage() == 0; }
  void WaitForOverlay() override { Boot::WaitForOverlayReadyLive(); }
  void ConfigureResources() override { LiveConfigureEffectResources(); }
  bool ActionHasEffect() override {
    return Effects_actionRequest.effectId != 0;
  }
  bool PackageOnDisc() override { return Effects_packageFile.lba != 0; }
  bool PurgeOverlayHeld() override { return Effects_purgeOverlayLoaded != 0; }
  void ReleasePurgeOverlay() override {
    Content::ReleaseLiveOverlayWorkspace(kPurgeOverlayOwner);
    Effects_purgeOverlayLoaded = 0;
  }
  void BindPackage() override {
    LiveBindEffectPackageBlocks(Effects_packageBuffer);
  }
  std::uint8_t PackageTrackCount() override {
    const Effects_EffectPackage* package = CurrentPackage("track count");
    if (package == nullptr) {
      return 0;
    }
    if (package->block9Data == nullptr) {
      Refuse("track count", "the package has no particle block");
      return 0;
    }
    return package->block9Data->frameCount;
  }
  Content_DiscQueueSlot* RequestMenuReload() override {
    const Content_DiscFile file{.lba = Content::MainMenuOverlayDiscSector(),
                                .size = 0};
    return Content::AllocateLiveDiscRequest(&file);
  }
  void EnqueueMenuReload(Content_DiscQueueSlot& slot) override {
    Content::EnqueueLiveDiscRequest(&slot, nullptr);
  }
  bool MenuReloadLoaded(Content_DiscQueueSlot& slot) override {
    return slot.state == std::to_underlying(Content::DiscRequestState::kLoaded);
  }
  void ReleaseMenuReload(Content_DiscQueueSlot& slot) override {
    Content::ReleaseLiveDiscRequest(&slot);
  }

  bool SpawnTrack(std::int32_t script_offset, std::uint8_t token) override {
    const Effects_EffectPackage* package = CurrentPackage("track start");
    if (package == nullptr) {
      return false;
    }
    Effects_EffectSpawnRequest spawn{};
    spawn.source = &Effects_actionRequest.source;
    spawn.emitFrom = &Effects_actionRequest.source;
    spawn.target = std::data(Effects_actionRequest.targets);
    spawn.emitTo = std::data(Effects_actionRequest.targets);
    spawn.priority = 0;
    spawn.script = package->block8Data;
    spawn.scriptOffset = script_offset;
    spawn.parent = nullptr;
    spawn.token = token;
    LiveCreatePrimaryEffectInstance(&spawn);
    return true;
  }
  void DestroyPrimary(Instance& instance) override {
    LiveDestroyPrimaryEffectInstance(&instance);
  }

  bool HasSfxBank() override {
    const Effects_EffectPackage* package = CurrentPackage("sound release");
    return package != nullptr && package->block12Data != nullptr;
  }
  void UnbindSfxBank() override {
    Audio::LiveUnbindRoomSfxBank();
    Effects_package->block12Data = nullptr;
  }
  bool HasCutsceneStream() override {
    const Effects_EffectPackage* package = CurrentPackage("stream release");
    return package != nullptr && package->block13Data != nullptr;
  }
  void UnbindCutsceneStream() override {
    Audio::UnbindLiveRoomCutsceneStream();
    Effects_package->block13Data = nullptr;
    Effects_package->auxiliarySoundHandle = 0;
  }
  void StopVibration() override {
    Input::StopLiveVibrationPatterns(kAllVibrationPatterns,
                                     kBattleVibrationOwner);
  }
  void ReleasePackageResources() override {
    LiveReleaseEffectPackageResources();
  }
  void ReleaseToken(std::uint8_t token) override {
    LiveReleaseEffectToken(token);
  }
  void ResetCameraMotion() override { LiveResetEffectCameraMotion(); }

  void StoreActionRequest() override {
    if (request_ != nullptr) {
      Effects_actionRequest = *request_;
    }
  }
  void SetActionVariant(std::uint8_t variant, std::uint8_t argument) override {
    Effects_actionVariant = variant;
    Effects_actionVariantArgument = static_cast<char>(argument);
  }
  std::uint8_t AllocateToken() override {
    return static_cast<std::uint8_t>(LiveAllocateEffectToken());
  }
  void CaptureSavedCamera() override {
    LiveCaptureEffectCamera(&Effects_savedActionCamera);
  }
  void CaptureEffectCamera() override {
    LiveCaptureEffectCamera(&Effects_effectCamera);
  }

  bool HasTimeline(Instance& instance) override {
    return instance.context.timeline != nullptr;
  }
  void FreeTimeline(Instance& instance) override {
    Core::Memory::FreeLiveFromHighEnd(instance.context.timeline);
    instance.context.timeline = nullptr;
  }

 private:
  const Effects_ActionEffectRequest* request_ = nullptr;
};

class LiveSecondary final : public LivePlayback<SecondaryEffectRuntime> {
 public:
  Instance* FirstSecondary() override { return Effects_secondaryInstances; }
  std::int16_t PhaseOf(Instance& instance) override {
    return instance.context.phase;
  }
  void SetPhase(Instance& instance, SecondaryEffectPhase phase) override {
    LiveSetSecondaryEffectPhase(std::to_underlying(phase), &instance);
  }
  void AcquireParticlePool() override {
    Effects_AcquireParticlePool(kSecondaryParticles);
  }
  Slot* FirstRenderer(Instance& instance) override {
    return instance.context.renderer;
  }
  Slot* NextRenderer(Slot& slot) override { return slot.next; }
  bool UpdateRenderer(Slot& slot) override {
    if (slot.renderer == nullptr) {
      Refuse("renderer update", "the slot has no renderer; detached");
      return false;
    }
    return slot.renderer(&slot, std::to_underlying(RendererCommand::kUpdate),
                         0) != 0;
  }
  void DetachRenderer(Instance& instance, Slot& slot) override {
    LiveDetachEffectRenderer(&instance, &slot);
  }
  std::uint8_t BusyBits(Instance& instance) override {
    return static_cast<std::uint8_t>(instance.busyBits);
  }
  void DestroySecondary(Instance& instance) override {
    LiveDestroySecondaryEffectInstance(&instance);
  }
};

class LiveStartup final : public EffectRuntimeStartup {
 public:
  void ResetInstances() override { LiveResetEffectInstances(); }
  void InitializeResources() override { LiveInitializeEffectResources(); }
  void ResetParticlePoolReferences() override {
    Effects_ResetParticlePoolReferences();
  }
};

class LiveFrame final : public EffectRuntimeFrame {
 public:
  std::uint8_t BufferIndex() override { return Effects_effectBufferIndex; }
  void SetBufferIndex(std::uint8_t index) override {
    Effects_effectBufferIndex = index;
  }
  void SelectPackage(ActivePackage package) override {
    LiveSelectEffectResourceContext(std::to_underlying(package));
  }
  bool UpdatePrimary() override { return LiveUpdatePrimaryEffects() != 0; }
  std::uint8_t UpdateSecondary() override {
    return static_cast<std::uint8_t>(LiveUpdateSecondaryEffects());
  }
  PrimaryEffectPhase PrimaryPhase() override {
    return LivePrimaryEffectState().phase;
  }
  void UpdateCameraMotion() override { LiveUpdateEffectCameraMotion(); }
};
}  // namespace

void LiveInitEffectRuntime() {
  LiveStartup startup;
  InitEffectRuntime(startup);
}

void LiveResetEffectInstances() {
  EffectRuntimeRecords records{.primary = LivePrimaryEffectState()};
  ResetEffectInstances(records);
  StoreLivePrimaryEffectState(records.primary);
  Effects_primaryToken = static_cast<char>(records.primary_token);
  Effects_secondaryInstances = records.secondary_instances;
  Effects_currentInstance = records.current_instance;
  Effects_unusedPrimaryWord = records.unused_primary_word;
  Effects_primaryInstances = records.primary_instances;
  std::span(Effects_curveSamples).front() = records.first_curve_sample;
  Effects_actionVariant = records.action_variant;
  Effects_menuReloadSlot = records.menu_reload_slot;
  std::ranges::transform(
      records.tokens, std::span(Effects_tokenBitmaps).begin(),
      [](std::uint32_t word) { return static_cast<int>(word); });
}

int LiveUpdatePrimaryEffects() {
  LivePrimary primary;
  return UpdatePrimaryEffects(primary) ? 1 : 0;
}

int LiveUpdateSecondaryEffects() {
  LiveSecondary secondary;
  return UpdateSecondaryEffects(secondary);
}

int LiveStartEffectTrack(int track) {
  LivePrimary primary;
  const TrackStart start = StartEffectTrack(track, primary);
  if (start == TrackStart::kRefused) {
    Refuse("track start", "track outside the pending mask or no package");
  }
  return start == TrackStart::kNotReady ? 0 : 1;
}

int LiveUpdateEffectRuntime() {
  LiveFrame frame;
  return UpdateEffectRuntime(frame);
}

int LiveBeginActionEffect(Effects_ActionEffectRequest* request,
                          int action_variant, int variant_argument) {
  if (request == nullptr) {
    Refuse("action effect", "no action request");
    return 0;
  }
  LivePrimary primary(*request);
  return BeginActionEffect({.source_type = request->source.type,
                            .variant = action_variant,
                            .argument = variant_argument},
                           primary);
}

void LiveClearPrimaryEffects() {
  LivePrimary primary;
  ClearPrimaryEffects(primary);
}
}  // namespace VS::Effects
