#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>

struct Effects_EffectInstance;

// Effect instance lifecycle (BATTLE.PRG 5BF94.c:5074-5160, 5548, 5593-5665).
// The primary and the secondary effects each run a chain of instances ordered
// by priority; an instance's renderers form a chain of their own, newest
// first. The records stay in the game heap: EffectInstancesLive.hpp edits
// them through the chains below and keeps the original entry points.
namespace VS::Effects {
// LiveEffectInstanceTargets::phase of a secondary instance; the original
// stored the bare number.
enum class SecondaryEffectPhase : std::int16_t {
  kHeld = 2,      // the update leaves the instance as it is
  kStarting = 3,  // the update takes a particle pool, then plays the instance
  kPlaying = 4,   // the update runs the instance's renderers and script
};

// The phase numbered `value`; nullopt for a number the runtime never sets.
[[nodiscard]] std::optional<SecondaryEffectPhase> ToSecondaryEffectPhase(
    std::int32_t value);

// func_800CF484 (5BF94.c:5548): stores `phase` in a secondary instance's
// context phase.
void SetSecondaryEffectPhase(std::int16_t& context_phase,
                             SecondaryEffectPhase phase);

// What InitEffectInstance reads of a LiveEffectSpawnRequest besides its
// parent, script and endpoints.
struct EffectSpawn {
  std::int32_t priority{};
  std::int32_t script_offset{};
  std::uint8_t token{};
};

// What an instance keeps of its spawn: the priority's low byte and the script
// offset's low halfword, as its record fields hold them.
struct EffectInstanceIdentity {
  std::uint8_t priority{};
  std::uint16_t script_offset{};
  std::uint8_t token{};

  friend bool operator==(const EffectInstanceIdentity&,
                         const EffectInstanceIdentity&) = default;
};

inline constexpr std::size_t kEffectInstanceTransforms = 16;

// The instance record InitEffectInstance starts.
class EffectInstanceStart {
 public:
  virtual ~EffectInstanceStart() = default;
  // Takes the spawn's parent and script with `identity`, and clears the
  // particles, registers, children and frame.
  virtual void Begin(const EffectInstanceIdentity& identity) = 0;
  // Clears the placement words and actors of transform `transform`, keeping
  // its position and matrices.
  virtual void ClearTransform(std::size_t transform) = 0;
  // Copies the spawn's four endpoints and clears the timeline, renderers and
  // parameter.
  virtual void TakeEndpoints() = 0;

 protected:
  EffectInstanceStart() = default;
  EffectInstanceStart(const EffectInstanceStart&) = default;
  EffectInstanceStart& operator=(const EffectInstanceStart&) = default;
  EffectInstanceStart(EffectInstanceStart&&) = default;
  EffectInstanceStart& operator=(EffectInstanceStart&&) = default;
};

// func_800CE714 (5BF94.c:5074): starts an instance from its spawn. The busy
// bits, effect id, phase and link keep whatever the record held, as in the
// original.
void InitEffectInstance(const EffectSpawn& spawn, EffectInstanceStart& record);

// A chain of game heap records, each linked to the next.
template <typename Record>
class RecordChain {
 public:
  virtual ~RecordChain() = default;
  // The first record; null when the chain is empty.
  [[nodiscard]] virtual Record* First() = 0;
  virtual void SetFirst(Record* record) = 0;
  // The record after `record`; null after the last.
  [[nodiscard]] virtual Record* Next(Record& record) = 0;
  virtual void SetNext(Record& record, Record* next) = 0;

 protected:
  RecordChain() = default;
  RecordChain(const RecordChain&) = default;
  RecordChain& operator=(const RecordChain&) = default;
  RecordChain(RecordChain&&) = default;
  RecordChain& operator=(RecordChain&&) = default;
};

// The commands an effect renderer takes (EffectRenderer's second argument).
enum class RendererCommand : std::uint32_t {
  kStart = 1,   // the slot was attached; the parameters are the attach's
  kUpdate = 2,  // each frame; the renderer returns 0 once it has finished
  kFade = 3,    // the renderer winds down (bone trails; FadeBoneTrails)
  kStop = 4,    // the slot's instance is being destroyed
};

struct LiveRendererSlot;

// decomp: effectExec (src/BATTLE/BATTLE.PRG/battleEffect.h:37): a renderer,
// given its slot, a command and the command's parameters.
using EffectRenderer = int (*)(LiveRendererSlot* slot, unsigned int command,
                               int parameters);

// decomp: func_800D4910_t (src/BATTLE/BATTLE.PRG/battleEffect.h:39): one of
// an instance's renderers, in the game heap.
struct LiveRendererSlot {
  LiveRendererSlot* next;
  EffectRenderer renderer;
  // What the renderer keeps between frames (a bone trail, or a template's
  // state); null when it keeps nothing. Freed with the slot.
  void* renderer_state;
};

// An instance's renderers, newest first (LiveEffectInstanceTargets::renderer).
class RendererChain : public RecordChain<LiveRendererSlot> {
 public:
  // A slot running `renderer`, without renderer state; null (refused) when
  // the heap has no room.
  [[nodiscard]] virtual LiveRendererSlot* NewSlot(EffectRenderer renderer) = 0;
  // Sends `command` to the slot's renderer.
  virtual void Run(LiveRendererSlot& slot, RendererCommand command,
                   std::int32_t parameters) = 0;
  // Frees the slot's renderer state, when it has one, then the slot.
  virtual void Free(LiveRendererSlot& slot) = 0;
};

// An effect chain (Effects_primaryInstances or Effects_secondaryInstances),
// ordered by priority.
class EffectInstanceChain : public RecordChain<Effects_EffectInstance> {
 public:
  // Effects_EffectInstance.priority, unsigned as the original read it.
  [[nodiscard]] virtual std::uint8_t PriorityOf(
      Effects_EffectInstance& instance) = 0;
  // A new instance started from the spawn (InitEffectInstance); null
  // (refused) when the heap has no room.
  [[nodiscard]] virtual Effects_EffectInstance* StartInstance() = 0;
  // Returns its particles to the pool (ReleaseAllEffectParticles).
  virtual void ReleaseParticles(Effects_EffectInstance& instance) = 0;
  // The instance's renderers.
  [[nodiscard]] virtual RendererChain& Renderers(
      Effects_EffectInstance& instance) = 0;
  // Returns the instance's effect token to the pool.
  virtual void ReleaseToken(Effects_EffectInstance& instance) = 0;
  virtual void Free(Effects_EffectInstance& instance) = 0;
};

// func_800CE83C (5BF94.c:5114): starts a primary instance and links it after
// every instance of no greater priority. The spawn's full `priority` is
// compared with the unsigned byte each instance keeps, as in the original. Null
// (refused) when the heap has no room.
[[nodiscard]] Effects_EffectInstance* CreatePrimaryEffectInstance(
    std::int32_t priority, EffectInstanceChain& primaries);

// func_800CF55C (5BF94.c:5593): CreatePrimaryEffectInstance for the
// secondary chain.
[[nodiscard]] Effects_EffectInstance* CreateSecondaryEffectInstance(
    std::int32_t priority, EffectInstanceChain& secondaries);

// func_800CE8F4 (5BF94.c:5139): unlinks the instance, releases its particles,
// stops and detaches each renderer, newest first, and frees the instance. An
// instance missing from the chain is still ended, as in the original.
void DestroyPrimaryEffectInstance(Effects_EffectInstance& instance,
                                  EffectInstanceChain& primaries);

// func_800CF614 (5BF94.c:5617): unlinks the instance, releases its particles
// and token and frees it. Its renderers stay attached, as in the original;
// the secondary update detaches each one as it finishes.
void DestroySecondaryEffectInstance(Effects_EffectInstance& instance,
                                    EffectInstanceChain& secondaries);

// func_800CF694 (5BF94.c:5636): attaches a slot running `renderer` as the
// newest and starts it with `parameters`. Null (refused) when the heap has no
// room.
[[nodiscard]] LiveRendererSlot* AttachEffectRenderer(RendererChain& renderers,
                                                     EffectRenderer renderer,
                                                     std::int32_t parameters);

// func_800CF70C (5BF94.c:5647): frees the slot's renderer state, unlinks the
// slot and frees it. False (refused), leaving the slot, when it is not
// attached; the original searched past the chain's end.
[[nodiscard]] bool DetachEffectRenderer(RendererChain& renderers,
                                        LiveRendererSlot& slot);

// An instance's renderers, as a frame updates them.
class EffectRendererUpdates {
 public:
  virtual ~EffectRendererUpdates() = default;
  [[nodiscard]] virtual LiveRendererSlot* FirstRenderer(
      Effects_EffectInstance& instance) = 0;
  [[nodiscard]] virtual LiveRendererSlot* NextRenderer(
      LiveRendererSlot& slot) = 0;
  // Sends the slot's renderer RendererCommand::kUpdate; false once it has
  // finished.
  [[nodiscard]] virtual bool UpdateRenderer(LiveRendererSlot& slot) = 0;
  virtual void DetachRenderer(Effects_EffectInstance& instance,
                              LiveRendererSlot& slot) = 0;

 protected:
  EffectRendererUpdates() = default;
  EffectRendererUpdates(const EffectRendererUpdates&) = default;
  EffectRendererUpdates& operator=(const EffectRendererUpdates&) = default;
  EffectRendererUpdates(EffectRendererUpdates&&) = default;
  EffectRendererUpdates& operator=(EffectRendererUpdates&&) = default;
};

// func_800D4910 (5BF94.c:7582), and the renderer pass of func_800CED60
// (5BF94.c:5321): updates each renderer, newest first, and detaches each one
// that has finished. The next slot is read before the update, as in the
// original, so a renderer may detach itself.
void UpdateEffectRenderers(Effects_EffectInstance& instance,
                           EffectRendererUpdates& renderers);
}  // namespace VS::Effects
