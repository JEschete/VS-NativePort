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
#include "Content/DiscQueue.hpp"
#include "Content/OverlayFiles.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/CameraLive.hpp"
#include "Effects/CameraLiveState.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/EffectInstancesLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/EffectResourcesLiveState.hpp"
#include "Effects/EffectRuntime.hpp"
#include "Effects/EffectScriptLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/ParticlePoolLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/PrimaryEffect.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/SpawnRequest.hpp"
#include "Input/ControllerLive.hpp"

static_assert(std::extent_v<decltype(VS::Effects::tokenBitmaps)> >=
              std::size_t{VS::Effects::kEffectTokenWords});

namespace VS::Effects {
namespace {
using Instance = LiveEffectInstance;
using Slot = LiveRendererSlot;

// The particles LiveAcquireParticlePool reserves for a secondary
// instance.
constexpr int kSecondaryParticles = 0xFF;
// ReleaseLiveOverlayWorkspace's owner tag for the purge overlay.
constexpr int kPurgeOverlayOwner = 2;
// StopLiveVibrationPatterns: every pattern the battle started.
constexpr std::uint32_t kAllVibrationPatterns = 0;
constexpr int kBattleVibrationOwner = 1;

std::uint64_t refusedCalls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refusedCalls)) {
    VS::Core::LogWarn("effects", "effect runtime: {} refused: {} ({} refusals)",
                      call, reason, refusedCalls);
  }
}

// The current package; null, with a diagnostic, when there is none.
[[nodiscard]] LiveEffectPackage* CurrentPackage(std::string_view call) {
  if (VS::Effects::package == nullptr) {
    Refuse(call, "no current effect package");
  }
  return VS::Effects::package;
}

[[nodiscard]] std::uint8_t StoredToken() {
  return static_cast<std::uint8_t>(primaryToken);
}

// An instance's frame, as both chains play it.
template <class Host>
class LivePlayback : public Host {
 public:
  Instance* Next(Instance& instance) override { return instance.next; }
  void SetCurrent(Instance& instance) override { currentInstance = &instance; }
  bool HasCurves() override {
    const LiveEffectPackage* package = CurrentPackage("curve sampling");
    return package != nullptr && package->curve_block != nullptr;
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
  void ReleaseParticlePool() override { LiveReleaseParticlePool(); }
  void SelectPackage(ActivePackage package) override {
    LiveSelectEffectResourceContext(std::to_underlying(package));
  }
};

class LivePrimary final : public LivePlayback<PrimaryEffectRuntime> {
 public:
  LivePrimary() = default;
  explicit LivePrimary(const LiveActionEffectRequest& request)
      : request_(&request) {}

  PrimaryEffectState Primary() override { return LivePrimaryEffectState(); }
  void SetPrimary(const PrimaryEffectState& state) override {
    StoreLivePrimaryEffectState(state);
  }
  std::uint8_t PrimaryToken() override { return StoredToken(); }
  void SetPrimaryToken(std::uint8_t token) override {
    primaryToken = static_cast<char>(token);
  }
  Instance* FirstPrimary() override { return primaryInstances; }
  VS::Content::DiscQueueSlot* MenuReloadSlot() override {
    return menuReloadSlot;
  }
  void SetMenuReloadSlot(VS::Content::DiscQueueSlot* slot) override {
    menuReloadSlot = slot;
  }

  bool PollPurgeOverlay() override { return LiveLoadEffectPurgeOverlay() == 0; }
  bool PollTextures() override { return LiveUpdateEffectTextureLoading() == 0; }
  bool PollPlugins() override { return LiveUpdateEffectPluginLoading() == 0; }
  bool PollPackage() override { return LiveLoadEffectPackage() == 0; }
  void WaitForOverlay() override { Boot::WaitForOverlayReadyLive(); }
  void ConfigureResources() override { LiveConfigureEffectResources(); }
  bool ActionHasEffect() override { return actionRequest.effect_id != 0; }
  bool PackageOnDisc() override { return packageFile.lba != 0; }
  bool PurgeOverlayHeld() override { return purgeOverlayLoaded != 0; }
  void ReleasePurgeOverlay() override {
    Content::ReleaseLiveOverlayWorkspace(kPurgeOverlayOwner);
    purgeOverlayLoaded = 0;
  }
  void BindPackage() override { LiveBindEffectPackageBlocks(packageBuffer); }
  std::uint8_t PackageTrackCount() override {
    const LiveEffectPackage* package = CurrentPackage("track count");
    if (package == nullptr) {
      return 0;
    }
    if (package->parameters == nullptr) {
      Refuse("track count", "the package has no particle block");
      return 0;
    }
    return package->parameters->track_count;
  }
  VS::Content::DiscQueueSlot* RequestMenuReload() override {
    const VS::Content::DiscFile file{
        .lba = Content::MainMenuOverlayDiscSector(), .size = 0};
    return Content::AllocateLiveDiscRequest(&file);
  }
  void EnqueueMenuReload(VS::Content::DiscQueueSlot& slot) override {
    Content::EnqueueLiveDiscRequest(&slot, nullptr);
  }
  bool MenuReloadLoaded(VS::Content::DiscQueueSlot& slot) override {
    return slot.state == Content::DiscRequestState::kLoaded;
  }
  void ReleaseMenuReload(VS::Content::DiscQueueSlot& slot) override {
    Content::ReleaseLiveDiscRequest(&slot);
  }

  bool SpawnTrack(std::int32_t script_offset, std::uint8_t token) override {
    const LiveEffectPackage* package = CurrentPackage("track start");
    if (package == nullptr) {
      return false;
    }
    LiveEffectSpawnRequest spawn{};
    spawn.source = &actionRequest.source;
    spawn.emit_from = &actionRequest.source;
    spawn.target = std::data(actionRequest.targets);
    spawn.emit_to = std::data(actionRequest.targets);
    spawn.priority = 0;
    spawn.script = package->programs;
    spawn.script_offset = script_offset;
    spawn.parent = nullptr;
    spawn.token = token;
    LiveCreatePrimaryEffectInstance(&spawn);
    return true;
  }
  void DestroyPrimary(Instance& instance) override {
    LiveDestroyPrimaryEffectInstance(&instance);
  }

  bool HasSfxBank() override {
    const LiveEffectPackage* package = CurrentPackage("sound release");
    return package != nullptr && package->sound_bank != nullptr;
  }
  void UnbindSfxBank() override {
    Audio::LiveUnbindRoomSfxBank();
    VS::Effects::package->sound_bank = nullptr;
  }
  bool HasCutsceneStream() override {
    const LiveEffectPackage* package = CurrentPackage("stream release");
    return package != nullptr && package->auxiliary_sound != nullptr;
  }
  void UnbindCutsceneStream() override {
    Audio::UnbindLiveRoomCutsceneStream();
    VS::Effects::package->auxiliary_sound = nullptr;
    VS::Effects::package->auxiliary_sound_handle = 0;
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
      actionRequest = *request_;
    }
  }
  void SetActionVariant(std::uint8_t variant) override {
    actionVariant = variant;
  }
  std::uint8_t AllocateToken() override {
    return static_cast<std::uint8_t>(LiveAllocateEffectToken());
  }
  void CaptureSavedCamera() override {
    LiveCaptureEffectCamera(&savedActionCamera);
  }
  void CaptureEffectCamera() override {
    LiveCaptureEffectCamera(&effectCamera);
  }

  bool HasTimeline(Instance& instance) override {
    return instance.context.timeline != nullptr;
  }
  void FreeTimeline(Instance& instance) override {
    Core::Memory::FreeLiveFromHighEnd(instance.context.timeline);
    instance.context.timeline = nullptr;
  }

 private:
  const LiveActionEffectRequest* request_ = nullptr;
};

class LiveSecondary final : public LivePlayback<SecondaryEffectRuntime> {
 public:
  Instance* FirstSecondary() override { return secondaryInstances; }
  std::int16_t PhaseOf(Instance& instance) override {
    return instance.context.phase;
  }
  void SetPhase(Instance& instance, SecondaryEffectPhase phase) override {
    LiveSetSecondaryEffectPhase(std::to_underlying(phase), &instance);
  }
  void AcquireParticlePool() override {
    LiveAcquireParticlePool(kSecondaryParticles);
  }
  Slot* FirstRenderer(Instance& instance) override {
    return instance.context.renderer;
  }
  Slot* NextRenderer(Slot& slot) override { return slot.next; }
  bool UpdateRenderer(Slot& slot) override {
    return LiveUpdateEffectRenderer(slot);
  }
  void DetachRenderer(Instance& instance, Slot& slot) override {
    LiveDetachEffectRenderer(&instance, &slot);
  }
  std::uint8_t BusyBits(Instance& instance) override {
    return static_cast<std::uint8_t>(instance.busy_bits);
  }
  void DestroySecondary(Instance& instance) override {
    LiveDestroySecondaryEffectInstance(&instance);
  }
};

// The secondary instances of a request: their endpoints point into the
// caller's request, which the instances copy as they start.
class LiveSecondarySpawner final : public SecondaryEffectSpawner {
 public:
  explicit LiveSecondarySpawner(LiveActionEffectRequest& request)
      : request_(&request) {}

  bool HasPackage() override {
    return CurrentPackage("secondary spawn") != nullptr;
  }
  std::uint8_t AllocateToken() override {
    return static_cast<std::uint8_t>(LiveAllocateEffectToken());
  }
  void StoreRequest() override { secondaryRequest = *request_; }
  std::uint16_t TargetType(std::size_t target) override {
    return std::span(request_->targets)[target].type;
  }
  Instance* CreateSecondary(const SecondarySpawn& spawn) override {
    LiveActionEffectTarget& target = std::span(request_->targets)[spawn.target];
    LiveEffectSpawnRequest request{};
    request.source = &request_->source;
    request.emit_from = spawn.toward_source ? &target : &request_->source;
    request.target = &target;
    request.emit_to = spawn.toward_source ? &request_->source : &target;
    request.priority = spawn.priority;
    request.script = VS::Effects::package->programs;
    request.script_offset = spawn.script_offset;
    request.parent = nullptr;
    request.token = spawn.token;
    Instance* instance = LiveCreateSecondaryEffectInstance(&request);
    if (instance == nullptr) {
      Refuse("secondary spawn", "no free secondary instance");
    }
    return instance;
  }
  void StartSecondary(Instance& instance, std::int32_t parameter,
                      std::uint16_t effect_id,
                      std::uint8_t busy_bits) override {
    instance.context.parameter = parameter;
    instance.context.effect_id = static_cast<std::int16_t>(effect_id);
    instance.busy_bits = busy_bits;
  }
  void SetPhase(Instance& instance, SecondaryEffectPhase phase) override {
    LiveSetSecondaryEffectPhase(std::to_underlying(phase), &instance);
  }
  void SelectPackage(ActivePackage package) override {
    LiveSelectEffectResourceContext(std::to_underlying(package));
  }
  void CaptureSavedCamera() override {
    LiveCaptureEffectCamera(&savedActionCamera);
  }
  void CaptureEffectCamera() override {
    LiveCaptureEffectCamera(&effectCamera);
  }

 private:
  LiveActionEffectRequest* request_;
};

// The request, or null with a diagnostic; a count past the request's targets
// is reported and clamped.
[[nodiscard]] LiveActionEffectRequest* CheckedSecondaryRequest(
    LiveActionEffectRequest* request, std::string_view call) {
  if (request == nullptr) {
    Refuse(call, "no effect request");
  } else if (request->count > kActionEffectTargets) {
    Refuse(call, "more targets than the request holds; the rest are skipped");
  }
  return request;
}

[[nodiscard]] SecondaryEffectRequest SecondaryRequestOf(
    const LiveActionEffectRequest& request) {
  return {.effect_id = request.effect_id, .target_count = request.count};
}

class LiveStartup final : public EffectRuntimeStartup {
 public:
  void ResetInstances() override { LiveResetEffectInstances(); }
  void InitializeResources() override { LiveInitializeEffectResources(); }
  void ResetParticlePoolReferences() override {
    LiveResetParticlePoolReferences();
  }
};

class LiveFrame final : public EffectRuntimeFrame {
 public:
  std::uint8_t BufferIndex() override { return effectBufferIndex; }
  void SetBufferIndex(std::uint8_t index) override {
    effectBufferIndex = index;
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
  primaryToken = static_cast<char>(records.primary_token);
  secondaryInstances = records.secondary_instances;
  currentInstance = records.current_instance;
  unusedPrimaryWord = records.unused_primary_word;
  primaryInstances = records.primary_instances;
  std::span(curveSamples).front() = records.first_curve_sample;
  actionVariant = records.action_variant;
  menuReloadSlot = records.menu_reload_slot;
  std::ranges::transform(
      records.tokens, std::span(tokenBitmaps).begin(),
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

int LiveBeginActionEffect(LiveActionEffectRequest* request, int action_variant,
                          int /*variant_argument*/) {
  if (request == nullptr) {
    Refuse("action effect", "no action request");
    return 0;
  }
  LivePrimary primary(*request);
  return BeginActionEffect(
      {.source_type = request->source.type, .variant = action_variant},
      primary);
}

void LiveClearPrimaryEffects() {
  LivePrimary primary;
  ClearPrimaryEffects(primary);
}

int LiveSpawnSecondaryEffects(LiveActionEffectRequest* request, int /*unused*/,
                              int parameter) {
  if (CheckedSecondaryRequest(request, "secondary spawn") == nullptr) {
    return 0;
  }
  LiveSecondarySpawner spawner(*request);
  return SpawnSecondaryEffects(SecondaryRequestOf(*request), parameter,
                               spawner);
}

int LiveBeginSecondaryEffects(LiveActionEffectRequest* request, int parameter) {
  if (CheckedSecondaryRequest(request, "secondary effects") == nullptr) {
    return 0;
  }
  LiveSecondarySpawner spawner(*request);
  return BeginSecondaryEffects(SecondaryRequestOf(*request), parameter,
                               spawner);
}

int LiveSpawnSecondaryEffectRequest(LiveActionEffectRequest* request,
                                    int unused, int parameter) {
  return LiveSpawnSecondaryEffects(request, unused, parameter);
}

void LiveSpawnSourceActorEffect() {
  const SourceActorEffect effect = MakeSourceActorEffect(
      static_cast<std::uint8_t>(actionRequest.source.Actor().actor));
  LiveActionEffectRequest request{};
  request.effect_id = effect.effect_id;
  request.count = effect.target_count;
  request.other_count = effect.other_count;
  request.source.type = effect.source_type;
  request.source.SetActor({.actor = effect.actor,
                           .event_actor_slot = 0,
                           .part = effect.part,
                           .reserved3 = 0});
  LiveActionEffectTarget& target = std::span{request.targets}.front();
  target.type = effect.target_type;
  target.position = {.vx = effect.target_x,
                     .vy = effect.target_y,
                     .vz = effect.target_z,
                     .pad = 0};
  static_cast<void>(LiveBeginSecondaryEffects(&request, 0));
}

void LiveClearActionEffectMode() {
  std::uint8_t variant = actionVariant;
  ClearActionEffectMode(variant);
  actionVariant = variant;
}
}  // namespace VS::Effects
