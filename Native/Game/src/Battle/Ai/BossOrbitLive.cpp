// Hand-maintained (tools/port/owned.csv).
// Live binding for the orbiting boss (BossOrbit.cpp).
#include "Battle/Ai/BossOrbitLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/BossOrbit.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Core/BiosRand.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using AiState = Battle_Ai_ActorAiState;
using Retreat = Battle_Ai_RadialRetreatView;
using Tracking = Battle_Ai_OrbitTrackingView;

// The arena progress script flag the boss and the event script share.
constexpr int kArenaProgressFlag = 0xA6;
constexpr std::uint8_t kCommandPending = 0x80;
// The module image loads the retreat path's first 31 entries; the last is
// the (0, -4096) entry after the (0, 0) end.
constexpr int kRetreatPathSteps = 31;
// The boss's core limb.
constexpr int kCoreLimb = 4;

static_assert(std::size(Battle_Ai_radialRetreatPath) >= kRetreatPathSteps);
static_assert(std::extent_v<decltype(Battle_ActorStats::limbs)> > kCoreLimb);

// The retreat and tracking views are the actor's AI state.
static_assert(offsetof(Retreat, x) == offsetof(AiState, position));
static_assert(offsetof(Retreat, model) == offsetof(AiState, model));
static_assert(offsetof(Retreat, moveX) == offsetof(AiState, moveX));
static_assert(offsetof(Retreat, moveZ) == offsetof(AiState, moveZ));
static_assert(offsetof(Retreat, moveY) == offsetof(AiState, moveY));
static_assert(offsetof(Retreat, candidateCount) ==
              offsetof(AiState, candidateCount));
static_assert(offsetof(Tracking, x) == offsetof(AiState, position));
static_assert(offsetof(Tracking, model) == offsetof(AiState, model));
static_assert(offsetof(Tracking, stats) == offsetof(AiState, stats));
static_assert(offsetof(Tracking, movementTarget) ==
              offsetof(AiState, movementTarget));
static_assert(offsetof(Tracking, moveX) == offsetof(AiState, moveX));
static_assert(offsetof(Tracking, moveZ) == offsetof(AiState, moveZ));
static_assert(offsetof(Tracking, moveY) == offsetof(AiState, moveY));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

// The AI state's model is the actor's record.
Actors_ActorRecord* RecordOf(const AiState& state) {
  return reinterpret_cast<Actors_ActorRecord*>(state.model);
}

// The position and stats are only read.
OrbitingBoss Load(const AiState& state) {
  OrbitingBoss boss;
  boss.x = state.position.vx;
  boss.y = state.position.vy;
  boss.z = state.position.vz;
  if (const Actors_ActorRecord* record = RecordOf(state)) {
    boss.model_flag = record->previewPose != 0;
  }
  if (const Battle_ActorStats* stats = state.stats) {
    boss.stats = BossStats{.current_hp = stats->currentHp,
                           .max_hp = stats->maxHp,
                           .core_hp = stats->limbs[kCoreLimb].hp};
  }
  boss.movement_target = state.movementTarget;
  boss.move_x = state.moveX;
  boss.move_z = state.moveZ;
  boss.move_y = state.moveY;
  boss.candidate_count = state.candidateCount;
  return boss;
}

void Store(const OrbitingBoss& boss, AiState& state) {
  if (Actors_ActorRecord* record = RecordOf(state);
      record != nullptr && boss.model_flag) {
    record->previewPose = *boss.model_flag ? 1 : 0;
  }
  state.movementTarget = boss.movement_target;
  state.moveX = boss.move_x;
  state.moveZ = boss.move_z;
  state.moveY = boss.move_y;
  state.candidateCount = boss.candidate_count;
}

ArenaOrbit LoadOrbit(const Battle_Ai_ArenaOrbitState& live) {
  return ArenaOrbit{.heading = live.heading,
                    .retreat_phase = live.phase,
                    .distance_cached = live.distanceCached,
                    .distance_squared = live.distanceSquared,
                    .is_orbiting = live.isOrbiting,
                    .path_step = live.pathStep,
                    .cooldown = live.cooldown,
                    .move_requested = live.requested,
                    .retreat_angle = live.orbitAngle,
                    .saved_height = live.savedHeight,
                    .health_phase = live.healthPhase,
                    .plan_saved = live.planSaved,
                    .saved_plan_count = live.savedPlanCount,
                    .start_x = live.startX,
                    .start_z = live.startZ,
                    .started = live.started,
                    .angular_velocity = Battle_Ai_orbitAngularVelocity,
                    .height_offset = Battle_Ai_orbitHeightOffset};
}

void StoreOrbit(const ArenaOrbit& orbit, Battle_Ai_ArenaOrbitState& live) {
  live.heading = orbit.heading;
  live.phase = orbit.retreat_phase;
  live.distanceCached = orbit.distance_cached;
  live.distanceSquared = orbit.distance_squared;
  live.isOrbiting = orbit.is_orbiting;
  live.pathStep = orbit.path_step;
  live.cooldown = orbit.cooldown;
  live.requested = orbit.move_requested;
  live.orbitAngle = orbit.retreat_angle;
  live.savedHeight = orbit.saved_height;
  live.healthPhase = orbit.health_phase;
  live.planSaved = orbit.plan_saved;
  live.savedPlanCount = orbit.saved_plan_count;
  live.startX = orbit.start_x;
  live.startZ = orbit.start_z;
  live.started = orbit.started;
  Battle_Ai_orbitAngularVelocity = orbit.angular_velocity;
  Battle_Ai_orbitHeightOffset = orbit.height_offset;
}

Battle_Ai_ArenaOrbitState& LiveOrbit() {
  Require(Battle_Ai_arenaOrbit != nullptr,
          "AI orbiting boss requires the arena orbit");
  return *Battle_Ai_arenaOrbit;
}

// The retreat path, the player's record, the plan, the battle actor and the
// actor's movement code. Each call taking the boss stores the boss and the
// orbit in progress first and reloads both after.
class LiveBossOrbitWorld final : public BossOrbitWorld {
 public:
  LiveBossOrbitWorld(AiState& state, ArenaOrbit* orbit)
      : state_(state), orbit_(orbit) {}

  [[nodiscard]] std::optional<RetreatStep> RetreatPathStep(
      int step) const override {
    if (step < 0 || step >= kRetreatPathSteps) return std::nullopt;
    const auto& entry = Battle_Ai_radialRetreatPath[step];
    return RetreatStep{.distance = entry[0], .height = entry[1]};
  }
  [[nodiscard]] std::optional<BossPlayer> Player() const override {
    const Actors_ActorRecord* record = Actors_actorRecords[0];
    if (record == nullptr) return std::nullopt;
    const auto& instance = record->instance;
    return BossPlayer{.moving = instance.movementMode != 0,
                      .x = instance.position.vx,
                      .y = instance.position.vy,
                      .z = instance.position.vz,
                      .collision_height = record->collisionHeight};
  }
  [[nodiscard]] std::optional<bool> IsFirstCandidateCooldownReady(
      OrbitingBoss& boss) override {
    const Battle_Ai_ActionCandidate* candidate = state_.candidateOrder[0];
    if (candidate == nullptr) return std::nullopt;
    bool ready = false;
    Run(boss, [&] { ready = IsLiveActionCooldownReady(&state_, candidate); });
    return ready;
  }
  [[nodiscard]] std::optional<std::uint16_t> FirstCandidateThreshold()
      const override {
    const Battle_Ai_ActionCandidate* candidate = state_.candidateOrder[0];
    if (candidate == nullptr) return std::nullopt;
    return static_cast<std::uint16_t>(candidate->threshold);
  }
  [[nodiscard]] std::optional<std::uint16_t> ReadinessLevel() const override {
    if (Battle_Ai_roomAiState == nullptr) return std::nullopt;
    return Battle_Ai_roomAiState->readinessLevel;
  }
  [[nodiscard]] bool IsEngagementReady(OrbitingBoss& boss) override {
    bool ready = false;
    Run(boss, [&] { ready = IsLiveEngagementReady(&state_); });
    return ready;
  }
  void TryEngage(OrbitingBoss& boss) override {
    Run(boss, [&] { (void)TryEngageLiveActor(&state_); });
  }
  void Disengage(OrbitingBoss& boss) override {
    Run(boss, [&] { DisengageLiveActor(&state_); });
  }
  void IssueCommand(OrbitingBoss& boss, CommandIntent intent) override {
    Run(boss,
        [&] { IssueLiveActorCommand(&state_, static_cast<int>(intent)); });
  }
  void RecoverMovement(OrbitingBoss& boss) override {
    Run(boss, [&] { RecoverLiveMovement(&state_); });
  }
  [[nodiscard]] std::uint8_t Progress() const override {
    return State::LiveScriptFlag(kArenaProgressFlag);
  }
  void SetProgress(std::uint8_t progress) override {
    State::SetLiveScriptFlag(kArenaProgressFlag, progress);
  }
  [[nodiscard]] int Random() override { return Core_Random_Rand(); }
  [[nodiscard]] std::optional<BossBattleActor> BattleActor() const override {
    const Battle_Actor* actor = Actor();
    if (actor == nullptr || actor->stats == nullptr) return std::nullopt;
    return BossBattleActor{
        .core_hp = actor->stats->limbs[kCoreLimb].hp,
        .command = {.opcode = static_cast<CommandOpcode>(actor->commandOpcode),
                    .arg0 = actor->commandArg0,
                    .arg1 = actor->commandArg1.word,
                    .arg2 = actor->commandArg2}};
  }
  void QueueBattleCommand(const ActorCommandPacket& packet) override {
    Battle_Actor* actor = Actor();
    Require(actor != nullptr, "AI orbiting boss attack requires its actor");
    actor->commandArg0 = packet.arg0;
    actor->commandArg1.word = packet.arg1;
    actor->commandArg2 = packet.arg2;
    actor->commandOpcode = static_cast<u_char>(packet.opcode);
    actor->commandState = kCommandPending;
  }

 private:
  // The AI state's actor is the battle actor.
  [[nodiscard]] Battle_Actor* Actor() const {
    return reinterpret_cast<Battle_Actor*>(state_.actor);
  }

  template <typename Call>
  void Run(OrbitingBoss& boss, Call&& call) {
    Store(boss, state_);
    if (orbit_ != nullptr) StoreOrbit(*orbit_, LiveOrbit());
    call();
    boss = Load(state_);
    if (orbit_ != nullptr) *orbit_ = LoadOrbit(LiveOrbit());
  }

  AiState& state_;
  ArenaOrbit* orbit_;
};

AiState& StateOf(void* state) {
  auto* actor = static_cast<AiState*>(state);
  Require(actor != nullptr, "AI orbiting boss requires an AI state");
  return *actor;
}

// Runs a typed routine over the state's boss and the arena orbit, stores
// both back and aborts when the routine refused.
template <typename Routine>
bool UpdateWithOrbit(AiState& state, const char* refusal, Routine routine) {
  ArenaOrbit orbit = LoadOrbit(LiveOrbit());
  LiveBossOrbitWorld world(state, &orbit);
  OrbitingBoss boss = Load(state);
  const std::optional<bool> result = routine(world, boss, orbit);
  Store(boss, state);
  StoreOrbit(orbit, LiveOrbit());
  Require(result.has_value(), refusal);
  return result.value_or(false);
}

std::optional<bool> Succeeded(bool done) {
  return done ? std::optional<bool>(true) : std::nullopt;
}
}  // namespace

int UpdateLiveBossRadialRetreat(Battle_Ai_RadialRetreatView* state) {
  return UpdateWithOrbit(
             StateOf(state),
             "AI boss retreat requires a model and a path step "
             "inside the retreat path",
             [](BossOrbitWorld& world, OrbitingBoss& boss, ArenaOrbit& orbit) {
               return UpdateBossRadialRetreat(world, boss, orbit);
             })
             ? 1
             : 0;
}

int IsLiveBossActionReady(Battle_Ai_ActorAiState* state) {
  AiState& actor = StateOf(state);
  LiveBossOrbitWorld world(actor, nullptr);
  OrbitingBoss boss = Load(actor);
  const std::optional<bool> ready = IsBossActionReady(world, boss);
  Store(boss, actor);
  Require(ready.has_value(),
          "AI boss readiness requires its first candidate and the room state");
  return *ready ? 1 : 0;
}

void UpdateLiveBossOrbitTracking(Battle_Ai_OrbitTrackingView* state) {
  (void)UpdateWithOrbit(
      StateOf(state),
      "AI boss orbit requires a model, stats, the player's record, its first "
      "candidate and the room state",
      [](BossOrbitWorld& world, OrbitingBoss& boss, ArenaOrbit& orbit) {
        return Succeeded(UpdateBossOrbitTracking(world, boss, orbit));
      });
}

int UpdateLiveOrbitingBossBehavior(Battle_Ai_ActorAiState* state) {
  (void)UpdateWithOrbit(
      StateOf(state),
      "AI orbiting boss refused its model, stats, path step, player's record, "
      "first candidate, room state or battle actor",
      [](BossOrbitWorld& world, OrbitingBoss& boss, ArenaOrbit& orbit) {
        return Succeeded(UpdateOrbitingBossBehavior(world, boss, orbit));
      });
  return 1;
}
}  // namespace VS::Battle::Ai
