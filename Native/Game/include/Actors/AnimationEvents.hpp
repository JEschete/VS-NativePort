#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ActorMotion.hpp"
#include "Actors/AnimationPose.hpp"
#include "Actors/ModelRendering.hpp"
#include "Actors/SequencePlayback.hpp"
#include "Actors/TextureAnimation.hpp"
#include "Audio/Sfx.hpp"
#include "Render/ModelGeometry.hpp"

// The SEQ animation event script (BATTLE.PRG 44F14.c): the byte-coded events
// a clip's event word locates in its bank's motion data, each preceded by the
// frame it fires on. Events play animations, sounds, weapon trails, texture
// and palette changes, shadow blends and SHP vertex moves. Every script read
// is bounded by the bank, every table index by its table, and a run by a
// reload budget; a script that leaves its bank or a table stops the run.
namespace VS::Actors {
// The no-event frame a finished or stopped script leaves as the next event.
inline constexpr std::uint16_t kNoEventFrame = 0xFFFF;
// The reloads a run may take before it stops: each animation an event plays
// reloads the cursor from the new clip.
inline constexpr int kAnimationEventReloadLimit = 64;
// The events a run may dispatch before it stops.
inline constexpr int kAnimationEventDispatchLimit = 1024;
// The event points of event 60 (D_800E9A28: 64 bytes before
// Content_battleShadowVertices).
inline constexpr std::size_t kAnimationEventPointCount = 8;

enum class AnimationEventMode : std::uint8_t {
  // Plays every event (the per-frame clock, func_800AFA28).
  kPlay = 0,
  // Plays only the presentation events: texture, palette, shadow, weapon
  // pose and vertex events (the menu and preview models).
  kPresentation = 1,
  // Reverts the vertex events up to the clip's length (func_800AE47C).
  kUnwind = 2,
  // Returns the frame of the first impact event (the battle window).
  kFindImpact = 3,
};

// One event 60 shadow point: the expand rate, shrink rate and level, then a
// word event 60 does not read.
inline constexpr std::size_t kShadowPointWords = 4;
using ShadowPointWords = std::array<std::uint16_t, kShadowPointWords>;

// Opcode-indexed BATTLE.PRG tables, as the owner of the module image holds
// them.
struct AnimationEventTables {
  // The bytes each opcode takes (D_800E9298, 68 entries).
  std::span<const std::uint8_t> lengths;
  // The sound event 4 plays for a drawn weapon, by sequence model
  // (D_800E92DC, 12 entries).
  std::span<const std::uint8_t> weapon_sounds;
  // The shadow expand rate, shrink rate and level of event 60 (8 entries).
  std::span<const ShadowPointWords> points;
};

// An event script's position: the bank it lies in and its offset there.
struct EventScriptPosition {
  std::span<const std::uint8_t> bank;
  std::size_t offset = 0;
};

enum class AnimationEventStatus : std::uint8_t {
  kDone,
  // Stopped the run: the pose's script lies in no registered bank.
  kNoScript,
  // Stopped the run: an event or its frame runs past the bank.
  kScriptOverrun,
  // Stopped the run: the events reloaded the cursor more than
  // kAnimationEventReloadLimit times.
  kReloadLimit,
  // Stopped the run: it dispatched kAnimationEventDispatchLimit events.
  kDispatchLimit,
  // Stopped the run: the opcode lies past the length table.
  kTableOutOfRange,
  // Skipped the event: event 60's point lies past the point table.
  kPointOutOfRange,
  // Skipped the event: the vertex lies past the model's vertices, or the
  // actor has no model.
  kVertexOutOfRange,
  // Skipped the event: the drawn weapon's sound lies past its table.
  kSoundOutOfRange,
  // Skipped the event: the actor has no record to place the sound at.
  kNoSoundTarget,
  // Skipped the companion: event 58 names a companion with no record.
  kNoCompanion,
};

// Whether `status` stopped the run rather than one event.
[[nodiscard]] bool StopsAnimationEvents(AnimationEventStatus status) noexcept;

struct AnimationEventRun {
  // The stop, else the first skipped event's problem, else kDone.
  AnimationEventStatus status = AnimationEventStatus::kDone;
  // The impact event's frame for kFindImpact, else 0.
  int value = 0;
};

// The actor and pose an event run reads and changes. Events can replace the
// pose's clip, so every getter is read again after each event.
class AnimationEventHost {
 public:
  AnimationEventHost() = default;
  AnimationEventHost(const AnimationEventHost&) = delete;
  AnimationEventHost& operator=(const AnimationEventHost&) = delete;
  AnimationEventHost(AnimationEventHost&&) = delete;
  AnimationEventHost& operator=(AnimationEventHost&&) = delete;
  virtual ~AnimationEventHost() = default;

  // The pose's script and its cursor.
  [[nodiscard]] virtual std::optional<EventScriptPosition> Script() const = 0;
  virtual void StoreCursor(const EventScriptPosition& position,
                           std::uint16_t frame) = 0;
  [[nodiscard]] virtual std::uint16_t NextEventFrame() const = 0;
  virtual void SetNextEventFrame(std::uint16_t frame) = 0;
  [[nodiscard]] virtual std::int16_t Frame() const = 0;
  virtual void SetFrame(std::int16_t frame) = 0;
  [[nodiscard]] virtual std::uint8_t EventWait() const = 0;
  virtual void SetEventWait(std::uint8_t wait) = 0;

  // The actor.
  [[nodiscard]] virtual int AnimationId() const = 0;
  [[nodiscard]] virtual std::uint16_t AnimationLength() const = 0;
  // Set while the pose plays the clip an event restarted; it holds back the
  // effect events.
  [[nodiscard]] virtual bool EventFlag() const = 0;
  virtual void SetEventFlag(bool flag) = 0;
  [[nodiscard]] virtual bool AdvanceRequested() const = 0;
  virtual void ClearAdvanceRequest() = 0;
  [[nodiscard]] virtual bool WeaponDrawn() const = 0;
  [[nodiscard]] virtual std::uint8_t SequenceModelId() const = 0;
  [[nodiscard]] virtual bool EventPlaybackActive() const = 0;
  [[nodiscard]] virtual bool EffectsBusy() const = 0;
  [[nodiscard]] virtual ActorMotion LoadMotion() const = 0;
  virtual void StoreMotion(const ActorMotion& motion) = 0;
  [[nodiscard]] virtual std::uint8_t CompanionSlot() const = 0;
  // Ends the companion's action animation; false when it has no record.
  virtual bool FinishCompanionAction(std::uint8_t slot) = 0;
  virtual void SetFastAnimation(bool fast) = 0;

  // Animation.
  // Queues the action's impact effects; false when it queued none.
  virtual bool QueueImpactEffects() = 0;
  virtual void PlayAnimation(int animation, int blend_frames) = 0;
  // Restarts the pose at the actor's current animation.
  virtual void InitializePose() = 0;

  // Sound.
  // The actor's sound pan and volume, projecting its sound position first;
  // nothing when it has no record.
  [[nodiscard]] virtual std::optional<Audio::SfxMix> SoundMix() = 0;
  virtual void PlaySfx(std::int32_t group, int effect,
                       const Audio::SfxMix& mix) = 0;
  virtual void StopSfx(std::int32_t group, int effect) = 0;
  virtual void PlaySoundPair(int sound) = 0;
  virtual void PlayModelSound(int sound) = 0;
  virtual void PlaySurfaceContactSound(int contact) = 0;

  // Weapons, textures, palette and model.
  // Starts the fade of each weapon attached to the actor; `show` draws the
  // weapon again first.
  virtual void FadeAttachedWeapons(const WeaponFade& fade, bool show) = 0;
  virtual void SetTextureCellSheet(int channel,
                                   const TextureCellSheet& sheet) = 0;
  virtual void SetTextureAnimation(int channel, int timeline,
                                   TextureAnimationCommand command) = 0;
  virtual void UploadTextureOverlay(const TextureOverlayCopy& copy) = 0;
  virtual void StartPaletteFade() = 0;
  // Adds `delta` to vertex `vertex` of the actor's model; false when the
  // vertex lies past the model's vertices or the actor has no model.
  virtual bool MoveModelVertex(std::size_t vertex,
                               const Render::ModelVertex& delta) = 0;
  // Marks the model as moved by events, so the next clip start reverts them.
  virtual void MarkPoseOverridden() = 0;
  [[nodiscard]] virtual ShadowBlend LoadShadow() const = 0;
  virtual void StoreShadow(const ShadowBlend& shadow) = 0;
  virtual void LockBonePose(int bone, bool locked) = 0;

  // Trails and effects.
  virtual void StartWeaponTrail(int trail) = 0;
  virtual void FadeWeaponTrails(int frames) = 0;
  virtual void StartEffectTrack(int track) = 0;
  virtual void SpawnSourceEffect() = 0;
  virtual void ApplyGimmickInteraction(int command) = 0;

  virtual void ReportEventProblem(AnimationEventStatus status,
                                  std::uint8_t op) = 0;
};

// decomp: BATTLE.PRG func_800AD714. Plays the pose's due events in `mode`
// until the next event's frame lies past the pose's frame (or, unwinding or
// finding the impact, the clip's length), a terminating event, a wait or a
// stop. The first event plays unconditionally. Events the mode filters out
// are stepped over; a terminating opcode (0, 3, 16-18, 42, 43, 45-50 or 65
// and above) ends the script in every mode, leaving kNoEventFrame. A stop
// reports its problem and also leaves kNoEventFrame.
AnimationEventRun RunAnimationEvents(AnimationEventHost& host,
                                     const AnimationEventTables& tables,
                                     AnimationEventMode mode);

// decomp: BATTLE.PRG func_800AE46C. Events 40 and 41 do nothing.
void IgnoreAnimationEvent();

enum class EventRestart : std::uint8_t {
  // The blend pose's script was reverted to the clip's length.
  kUnwound,
  // The clip has no events to revert.
  kNoEvents,
  // The clip, its events or, for a moved model, any events are missing.
  kUnavailable,
};

// decomp: BATTLE.PRG func_800AE47C. Reverts the vertex events of the clip
// bound to the blend pose (`host` acts on the blend pose): the script starts
// over and runs in kUnwind mode. A model moved by events whose clip has none
// cannot be reverted.
EventRestart RestartActorAnimationEvents(
    AnimationEventHost& host, const AnimationEventTables& tables,
    const std::optional<AnimationClipRef>& bound, bool pose_overridden);
}  // namespace VS::Actors
