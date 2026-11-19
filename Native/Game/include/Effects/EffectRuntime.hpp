#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Effects/EffectInstances.hpp"
#include "Effects/PrimaryEffect.hpp"

struct Content_DiscQueueSlot;
struct Effects_EffectInstance;
struct Effects_RendererSlot;

// The effect runtime's start-up and frame update and the primary effect's
// action lifecycle (BATTLE.PRG 5BF94.c:5047-5070, 5170-5477). Each frame the
// primary effect walks its phase machine (PrimaryEffect.hpp): it loads the
// purge overlay, textures, plugins and package of an action, starts the
// package's tracks and plays their instances, then releases the package and
// reloads the main menu overlay. The secondary effects play beside it. The
// records stay in Runtime.cpp's globals and the game heap: EffectRuntimeLive
// binds the hosts below to them, and each host read returns the current
// record, as the original reread its globals after every call.
namespace VS::Effects {
// LiveActionEffectTarget.type of an endpoint on an actor's bone.
inline constexpr std::uint16_t kActorBoneEndpoint = 4;

// A track's script starts at its index times this in the package's script.
inline constexpr std::int32_t kTrackScriptStride = 3;

// The package SelectEffectResourceContext makes current.
enum class ActivePackage : std::uint8_t { kSecondary = 0, kAction = 1 };

// The runtime records ResetEffectInstances clears.
struct EffectRuntimeRecords {
  PrimaryEffectState primary{};
  std::uint8_t primary_token{};
  Effects_EffectInstance* primary_instances{};
  Effects_EffectInstance* secondary_instances{};
  Effects_EffectInstance* current_instance{};
  std::int32_t unused_primary_word{};
  std::int32_t first_curve_sample{};
  std::uint8_t action_variant{};
  Content_DiscQueueSlot* menu_reload_slot{};
  EffectTokenPool tokens{};
};

// func_800CE67C (5BF94.c:5054): idles the primary effect, empties both
// chains and the token pool and forgets the current instance, the menu
// reload and the action variant. The pending-track mask is kept, as in the
// original; the next package's tracks are added to it.
void ResetEffectInstances(EffectRuntimeRecords& records);

// What InitEffectRuntime starts.
class EffectRuntimeStartup {
 public:
  virtual ~EffectRuntimeStartup() = default;
  // ResetEffectInstances on the runtime records.
  virtual void ResetInstances() = 0;
  virtual void InitializeResources() = 0;
  virtual void ResetParticlePoolReferences() = 0;

 protected:
  EffectRuntimeStartup() = default;
  EffectRuntimeStartup(const EffectRuntimeStartup&) = default;
  EffectRuntimeStartup& operator=(const EffectRuntimeStartup&) = default;
  EffectRuntimeStartup(EffectRuntimeStartup&&) = default;
  EffectRuntimeStartup& operator=(EffectRuntimeStartup&&) = default;
};

// func_800CE64C (5BF94.c:5047): resets the instances, then the effect
// resources and the particle pool references.
void InitEffectRuntime(EffectRuntimeStartup& startup);

// What a frame of an instance uses, for either chain.
class EffectInstancePlayback {
 public:
  virtual ~EffectInstancePlayback() = default;
  // The instance after `instance` in its chain; null after the last.
  [[nodiscard]] virtual Effects_EffectInstance* Next(
      Effects_EffectInstance& instance) = 0;
  // Makes `instance` the one the script and particle code act on.
  virtual void SetCurrent(Effects_EffectInstance& instance) = 0;
  // Whether the current package has a curve block.
  [[nodiscard]] virtual bool HasCurves() = 0;
  // Samples every curve of the package at `frame`.
  virtual void SampleCurves(std::uint16_t frame) = 0;
  // Applies the package's gravity and drag to the particles.
  virtual void UpdateParticleForces() = 0;
  // Runs the instance's script for a frame; false once it has ended.
  [[nodiscard]] virtual bool RunScript(Effects_EffectInstance& instance) = 0;
  [[nodiscard]] virtual std::uint16_t Frame(
      Effects_EffectInstance& instance) = 0;
  virtual void SetFrame(Effects_EffectInstance& instance,
                        std::uint16_t frame) = 0;
  virtual void ReleaseParticlePool() = 0;
  virtual void SelectPackage(ActivePackage package) = 0;

 protected:
  EffectInstancePlayback() = default;
  EffectInstancePlayback(const EffectInstancePlayback&) = default;
  EffectInstancePlayback& operator=(const EffectInstancePlayback&) = default;
  EffectInstancePlayback(EffectInstancePlayback&&) = default;
  EffectInstancePlayback& operator=(EffectInstancePlayback&&) = default;
};

// The primary effect's records and the loaders, heap, sound and camera it
// drives.
class PrimaryEffectRuntime : public EffectInstancePlayback {
 public:
  [[nodiscard]] virtual PrimaryEffectState Primary() = 0;
  virtual void SetPrimary(const PrimaryEffectState& state) = 0;
  [[nodiscard]] virtual std::uint8_t PrimaryToken() = 0;
  virtual void SetPrimaryToken(std::uint8_t token) = 0;
  [[nodiscard]] virtual Effects_EffectInstance* FirstPrimary() = 0;
  [[nodiscard]] virtual Content_DiscQueueSlot* MenuReloadSlot() = 0;
  virtual void SetMenuReloadSlot(Content_DiscQueueSlot* slot) = 0;

  // Each poll advances its loader; true once it has finished.
  [[nodiscard]] virtual bool PollPurgeOverlay() = 0;
  [[nodiscard]] virtual bool PollTextures() = 0;
  [[nodiscard]] virtual bool PollPlugins() = 0;
  [[nodiscard]] virtual bool PollPackage() = 0;
  virtual void WaitForOverlay() = 0;
  virtual void ConfigureResources() = 0;
  // Whether the stored action request names an effect.
  [[nodiscard]] virtual bool ActionHasEffect() = 0;
  // Whether the action's package has a file to load.
  [[nodiscard]] virtual bool PackageOnDisc() = 0;
  // Whether the purge overlay holds its workspace; the release frees it.
  [[nodiscard]] virtual bool PurgeOverlayHeld() = 0;
  virtual void ReleasePurgeOverlay() = 0;
  // Binds the loaded package's blocks.
  virtual void BindPackage() = 0;
  // The bound package's track count (block 9's frame count).
  [[nodiscard]] virtual std::uint8_t PackageTrackCount() = 0;
  // A disc request for the main menu overlay; null when the queue is full.
  [[nodiscard]] virtual Content_DiscQueueSlot* RequestMenuReload() = 0;
  virtual void EnqueueMenuReload(Content_DiscQueueSlot& slot) = 0;
  [[nodiscard]] virtual bool MenuReloadLoaded(Content_DiscQueueSlot& slot) = 0;
  virtual void ReleaseMenuReload(Content_DiscQueueSlot& slot) = 0;

  // Starts a primary instance at `script_offset` in the package's script
  // with the action's source and first target as its endpoints. False
  // (refused) when there is no package; an instance the heap has no room for
  // still counts as started, as in the original.
  [[nodiscard]] virtual bool SpawnTrack(std::int32_t script_offset,
                                        std::uint8_t token) = 0;
  virtual void DestroyPrimary(Effects_EffectInstance& instance) = 0;

  // The package's room sound bank and cutscene stream; each unbind clears
  // the package's reference.
  [[nodiscard]] virtual bool HasSfxBank() = 0;
  virtual void UnbindSfxBank() = 0;
  [[nodiscard]] virtual bool HasCutsceneStream() = 0;
  virtual void UnbindCutsceneStream() = 0;
  virtual void StopVibration() = 0;
  virtual void ReleasePackageResources() = 0;
  virtual void ReleaseToken(std::uint8_t token) = 0;
  virtual void ResetCameraMotion() = 0;

  // Copies the action request into the runtime's.
  virtual void StoreActionRequest() = 0;
  virtual void SetActionVariant(std::uint8_t variant) = 0;
  // A free effect token; 0 when the pool is full.
  [[nodiscard]] virtual std::uint8_t AllocateToken() = 0;
  // Captures the camera into the saved action camera, then into the effect
  // camera.
  virtual void CaptureSavedCamera() = 0;
  virtual void CaptureEffectCamera() = 0;

  // The instance's timeline, which the clear frees from the heap's high end.
  [[nodiscard]] virtual bool HasTimeline(Effects_EffectInstance& instance) = 0;
  virtual void FreeTimeline(Effects_EffectInstance& instance) = 0;
};

// func_800CE9B0 (5BF94.c:5170): advances the primary effect a frame. False
// once it idles: when idle, and on the frame its menu reload completes.
[[nodiscard]] bool UpdatePrimaryEffects(PrimaryEffectRuntime& runtime);

// What StartEffectTrack did.
enum class TrackStart : std::uint8_t {
  kNotReady,    // no track can start (CanStartTrack)
  kStarted,     // the track's instance was spawned
  kNotPending,  // the track is not pending
  kRefused,     // a track outside the pending mask, or no package
};

// func_800CEF74 (5BF94.c:5399): selects the action package and, when a track
// can start and `track` is pending, spawns its instance, plays the tracks,
// counts it as started and clears it from the mask. The original returned
// whether a track could start; it shifted by the track's low five bits, where
// a track outside the mask is refused.
[[nodiscard]] TrackStart StartEffectTrack(int track,
                                          PrimaryEffectRuntime& runtime);

// What BeginActionEffect reads of the request and its arguments.
struct ActionEffectStart {
  std::uint16_t source_type{};
  std::int32_t variant{};
};

// func_800CF0E8 (5BF94.c:5444): stores the action request, keeps a nonzero
// variant's low byte for an actor-bone source, takes a token, selects the
// action package, captures both cameras and starts loading. Returns the
// token. The original also kept the third argument's low byte at D_800F54A9,
// which nothing reads (#445).
[[nodiscard]] std::uint8_t BeginActionEffect(const ActionEffectStart& start,
                                             PrimaryEffectRuntime& runtime);

// func_800CF1A8 (5BF94.c:5462): stops the vibration, then frees each primary
// instance's timeline and destroys the instance, first to last.
void ClearPrimaryEffects(PrimaryEffectRuntime& runtime);

// The secondary effects' chain and the renderers and particles it drives.
class SecondaryEffectRuntime : public EffectInstancePlayback,
                               public EffectRendererUpdates {
 public:
  [[nodiscard]] virtual Effects_EffectInstance* FirstSecondary() = 0;
  // Effects_EffectInstanceTargets.phase, as stored.
  [[nodiscard]] virtual std::int16_t PhaseOf(
      Effects_EffectInstance& instance) = 0;
  virtual void SetPhase(Effects_EffectInstance& instance,
                        SecondaryEffectPhase phase) = 0;
  // Takes a particle pool for the instance.
  virtual void AcquireParticlePool() = 0;
  // The instance's three busy bits.
  [[nodiscard]] virtual std::uint8_t BusyBits(
      Effects_EffectInstance& instance) = 0;
  virtual void DestroySecondary(Effects_EffectInstance& instance) = 0;
};

// func_800CED60 (5BF94.c:5321): advances each secondary instance a frame and
// returns the union of their busy bits. A starting instance takes a particle
// pool and plays; a playing one samples the curves, applies the particle
// forces, updates its renderers, detaching each finished one, and runs its
// script, ending (and returning its pool) once the script has. An ended
// instance's busy bits are read before it is destroyed; the original read
// them from the freed record.
[[nodiscard]] std::uint8_t UpdateSecondaryEffects(
    SecondaryEffectRuntime& runtime);

// What UpdateEffectRuntime drives.
class EffectRuntimeFrame {
 public:
  virtual ~EffectRuntimeFrame() = default;
  [[nodiscard]] virtual std::uint8_t BufferIndex() = 0;
  virtual void SetBufferIndex(std::uint8_t index) = 0;
  virtual void SelectPackage(ActivePackage package) = 0;
  // UpdatePrimaryEffects and UpdateSecondaryEffects.
  [[nodiscard]] virtual bool UpdatePrimary() = 0;
  [[nodiscard]] virtual std::uint8_t UpdateSecondary() = 0;
  [[nodiscard]] virtual PrimaryEffectPhase PrimaryPhase() = 0;
  virtual void UpdateCameraMotion() = 0;

 protected:
  EffectRuntimeFrame() = default;
  EffectRuntimeFrame(const EffectRuntimeFrame&) = default;
  EffectRuntimeFrame& operator=(const EffectRuntimeFrame&) = default;
  EffectRuntimeFrame(EffectRuntimeFrame&&) = default;
  EffectRuntimeFrame& operator=(EffectRuntimeFrame&&) = default;
};

// func_800CF060 (5BF94.c:5427): flips the double buffer index (1 minus it,
// as a byte), updates the primary effect with the action package and the
// secondary effects with theirs, reselects the action package and moves the
// effect camera while the tracks play. Returns the primary effect's busy
// flag (bit 0) with the secondary busy bits.
[[nodiscard]] std::uint8_t UpdateEffectRuntime(EffectRuntimeFrame& frame);

// The targets an action effect request holds
// (LiveActionEffectRequest.targets).
inline constexpr std::size_t kActionEffectTargets = 16;

// LiveActionEffectTarget.type of an endpoint at a position.
inline constexpr std::uint16_t kPositionEndpoint = 5;

// Whether `effect_id`'s secondary instances emit from each target toward the
// source (effects 23, 25, 40-41 and 44-53) rather than from the source
// toward each target.
[[nodiscard]] bool EmitsTowardSource(std::uint16_t effect_id);

// The busy bits of `effect_id`'s secondary instances: 4 for effect 54, 2 for
// every other.
[[nodiscard]] std::uint8_t SecondaryBusyBits(std::uint16_t effect_id);

// What SpawnSecondaryEffects reads of its request.
struct SecondaryEffectRequest {
  std::uint16_t effect_id{};
  std::uint8_t target_count{};
};

// A secondary instance SpawnSecondaryEffects creates for one of the
// request's targets.
struct SecondarySpawn {
  // The target, which is also the instance's target endpoint.
  std::size_t target{};
  // Emits from the target toward the request's source, rather than from the
  // source toward the target.
  bool toward_source{};
  // The target's type.
  std::uint16_t priority{};
  // The effect id times kTrackScriptStride, in the package's script.
  std::int32_t script_offset{};
  std::uint8_t token{};
};

// The secondary request, package, token pool and camera a secondary spawn
// uses.
class SecondaryEffectSpawner {
 public:
  virtual ~SecondaryEffectSpawner() = default;
  // Whether a package is current to read the script from.
  [[nodiscard]] virtual bool HasPackage() = 0;
  // A free effect token; 0 when the pool is full.
  [[nodiscard]] virtual std::uint8_t AllocateToken() = 0;
  // Copies the request into the runtime's secondary request.
  virtual void StoreRequest() = 0;
  // The type of the request's `target`.
  [[nodiscard]] virtual std::uint16_t TargetType(std::size_t target) = 0;
  // A secondary instance with the request's source and the target as its
  // endpoints, running the package's script; null when it was refused.
  [[nodiscard]] virtual Effects_EffectInstance* CreateSecondary(
      const SecondarySpawn& spawn) = 0;
  // Stores the parameter and effect id in the instance and sets its busy
  // bits.
  virtual void StartSecondary(Effects_EffectInstance& instance,
                              std::int32_t parameter, std::uint16_t effect_id,
                              std::uint8_t busy_bits) = 0;
  virtual void SetPhase(Effects_EffectInstance& instance,
                        SecondaryEffectPhase phase) = 0;
  virtual void SelectPackage(ActivePackage package) = 0;
  // Captures the camera into the saved action camera, then into the effect
  // camera.
  virtual void CaptureSavedCamera() = 0;
  virtual void CaptureEffectCamera() = 0;

 protected:
  SecondaryEffectSpawner() = default;
  SecondaryEffectSpawner(const SecondaryEffectSpawner&) = default;
  SecondaryEffectSpawner& operator=(const SecondaryEffectSpawner&) = default;
  SecondaryEffectSpawner(SecondaryEffectSpawner&&) = default;
  SecondaryEffectSpawner& operator=(SecondaryEffectSpawner&&) = default;
};

// func_800CF218 (5BF94.c:5479): takes a token, stores the request and starts
// a secondary instance of its effect for each target, with `parameter` and
// the effect's busy bits. Returns the token. A request without a package is
// refused before the token (0); the original read the script through the
// null package. Targets past kActionEffectTargets are not spawned, and a
// refused instance is skipped; the original read past the request and wrote
// through the null instance.
[[nodiscard]] std::uint8_t SpawnSecondaryEffects(
    const SecondaryEffectRequest& request, std::int32_t parameter,
    SecondaryEffectSpawner& spawner);

// func_800CF3F8 (5BF94.c:5532): selects the secondary package, captures both
// cameras and spawns the request's secondary instances. Returns the token.
[[nodiscard]] std::uint8_t BeginSecondaryEffects(
    const SecondaryEffectRequest& request, std::int32_t parameter,
    SecondaryEffectSpawner& spawner);

// The request func_800CF8BC begins: effect 0x38 from the acting actor's root
// part to a single target at the origin.
struct SourceActorEffect {
  std::uint16_t effect_id{};
  std::uint8_t target_count{};
  std::uint8_t other_count{};
  std::uint16_t source_type{};
  std::uint8_t actor{};
  std::uint8_t part{};
  std::uint16_t target_type{};
  std::int16_t target_x{};
  std::int16_t target_y{};
  std::int16_t target_z{};
};

// func_800CF8BC (5BF94.c:5697): the request that begins the source actor
// effect on `actor`. The original left the colour presets and the unused
// targets uninitialized.
[[nodiscard]] SourceActorEffect MakeSourceActorEffect(std::uint8_t actor);

// func_800CF920 (5BF94.c:5712): clears the action variant.
void ClearActionEffectMode(std::uint8_t& action_variant);
}  // namespace VS::Effects
