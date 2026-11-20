// Hand-maintained (tools/port/owned.csv).
// Live binding for the orbiting boss (BossOrbit.cpp).
#include "Battle/Ai/BossOrbitLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Actors/ActorRecord.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/BossOrbit.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/RandomLive.hpp"
#include "State/ScriptFlagsLive.hpp"

namespace VS::Battle::Ai {
namespace {
using AiState = VS::Battle::Ai::ActorAiState;
using Retreat = VS::Battle::Ai::RadialRetreatView;
using Tracking = VS::Battle::Ai::OrbitTrackingView;

// The arena progress script flag the boss and the event script share.
constexpr int kArenaProgressFlag = 0xA6;
constexpr std::uint8_t kCommandPending = 0x80;
// The module image loads the retreat path's first 31 entries; the last is
// the (0, -4096) entry after the (0, 0) end.
constexpr int kRetreatPathSteps = 31;
// The boss's core limb.
constexpr int kCoreLimb = 4;

static_assert(std::size(VS::Battle::Ai::radialRetreatPath) >=
              kRetreatPathSteps);
static_assert(std::tuple_size_v<decltype(VS::Battle::ActorStats::limbs)> >
              kCoreLimb);

// The retreat and tracking views are the actor's AI state.
static_assert(offsetof(Retreat, x) == offsetof(AiState, position));
static_assert(offsetof(Retreat, model) == offsetof(AiState, model));
static_assert(offsetof(Retreat, move_x) == offsetof(AiState, move_x));
static_assert(offsetof(Retreat, move_z) == offsetof(AiState, move_z));
static_assert(offsetof(Retreat, move_y) == offsetof(AiState, move_y));
static_assert(offsetof(Retreat, candidate_count) ==
              offsetof(AiState, candidate_count));
static_assert(offsetof(Tracking, x) == offsetof(AiState, position));
static_assert(offsetof(Tracking, model) == offsetof(AiState, model));
static_assert(offsetof(Tracking, stats) == offsetof(AiState, stats));
static_assert(offsetof(Tracking, movement_target) ==
              offsetof(AiState, movement_target));
static_assert(offsetof(Tracking, move_x) == offsetof(AiState, move_x));
static_assert(offsetof(Tracking, move_z) == offsetof(AiState, move_z));
static_assert(offsetof(Tracking, move_y) == offsetof(AiState, move_y));

// The AI state's model is the actor's record.
VS::Actors::ActorRecord* RecordOf(const AiState& state) {
  return reinterpret_cast<VS::Actors::ActorRecord*>(state.model);
}

// The position and stats are only read.
OrbitingBoss Load(const AiState& state) {
  OrbitingBoss boss;
  boss.x = state.position.vx;
  boss.y = state.position.vy;
  boss.z = state.position.vz;
  if (const VS::Actors::ActorRecord* record = RecordOf(state)) {
    boss.model_flag = record->preview_pose != 0;
  }
  if (const VS::Battle::ActorStats* stats = state.stats) {
    boss.stats = BossStats{.current_hp = stats->current_hp,
                           .max_hp = stats->max_hp,
                           .core_hp = stats->limbs[kCoreLimb].hp};
  }
  boss.movement_target = state.movement_target;
  boss.move_x = state.move_x;
  boss.move_z = state.move_z;
  boss.move_y = state.move_y;
  boss.candidate_count = state.candidate_count;
  return boss;
}

void Store(const OrbitingBoss& boss, AiState& state) {
  if (VS::Actors::ActorRecord* record = RecordOf(state);
      record != nullptr && boss.model_flag) {
    record->preview_pose = *boss.model_flag ? 1 : 0;
  }
  state.movement_target = boss.movement_target;
  state.move_x = boss.move_x;
  state.move_z = boss.move_z;
  state.move_y = boss.move_y;
  state.candidate_count = boss.candidate_count;
}

ArenaOrbit LoadOrbit(const VS::Battle::Ai::ArenaOrbitState& live) {
  return ArenaOrbit{.heading = live.heading,
                    .retreat_phase = live.phase,
                    .distance_cached = live.distance_cached,
                    .distance_squared = live.distance_squared,
                    .is_orbiting = live.is_orbiting,
                    .path_step = live.path_step,
                    .cooldown = live.cooldown,
                    .move_requested = live.requested,
                    .retreat_angle = live.orbit_angle,
                    .saved_height = live.saved_height,
                    .health_phase = live.health_phase,
                    .plan_saved = live.plan_saved,
                    .saved_plan_count = live.saved_plan_count,
                    .start_x = live.start_x,
                    .start_z = live.start_z,
                    .started = live.started,
                    .angular_velocity = VS::Battle::Ai::orbitAngularVelocity,
                    .height_offset = VS::Battle::Ai::orbitHeightOffset};
}

void StoreOrbit(const ArenaOrbit& orbit,
                VS::Battle::Ai::ArenaOrbitState& live) {
  live.heading = orbit.heading;
  live.phase = orbit.retreat_phase;
  live.distance_cached = orbit.distance_cached;
  live.distance_squared = orbit.distance_squared;
  live.is_orbiting = orbit.is_orbiting;
  live.path_step = orbit.path_step;
  live.cooldown = orbit.cooldown;
  live.requested = orbit.move_requested;
  live.orbit_angle = orbit.retreat_angle;
  live.saved_height = orbit.saved_height;
  live.health_phase = orbit.health_phase;
  live.plan_saved = orbit.plan_saved;
  live.saved_plan_count = orbit.saved_plan_count;
  live.start_x = orbit.start_x;
  live.start_z = orbit.start_z;
  live.started = orbit.started;
  VS::Battle::Ai::orbitAngularVelocity = orbit.angular_velocity;
  VS::Battle::Ai::orbitHeightOffset = orbit.height_offset;
}

VS::Battle::Ai::ArenaOrbitState& LiveOrbit() {
  Require(VS::Battle::Ai::arenaOrbit != nullptr,
          "AI orbiting boss requires the arena orbit");
  return *VS::Battle::Ai::arenaOrbit;
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
    const auto& entry = VS::Battle::Ai::radialRetreatPath[step];
    return RetreatStep{.distance = entry[0], .height = entry[1]};
  }
  [[nodiscard]] std::optional<BossPlayer> Player() const override {
    const VS::Actors::ActorRecord* record = VS::Actors::actorRecords[0];
    if (record == nullptr) return std::nullopt;
    const auto& instance = record->instance;
    return BossPlayer{.moving = instance.movement_mode != 0,
                      .x = instance.position.vx,
                      .y = instance.position.vy,
                      .z = instance.position.vz,
                      .collision_height = record->collision_height};
  }
  [[nodiscard]] std::optional<bool> IsFirstCandidateCooldownReady(
      OrbitingBoss& boss) override {
    const VS::Battle::Ai::ActionCandidateRecord* candidate =
        state_.candidate_order[0];
    if (candidate == nullptr) return std::nullopt;
    bool ready = false;
    Run(boss, [&] { ready = IsLiveActionCooldownReady(&state_, candidate); });
    return ready;
  }
  [[nodiscard]] std::optional<std::uint16_t> FirstCandidateThreshold()
      const override {
    const VS::Battle::Ai::ActionCandidateRecord* candidate =
        state_.candidate_order[0];
    if (candidate == nullptr) return std::nullopt;
    return static_cast<std::uint16_t>(candidate->threshold);
  }
  [[nodiscard]] std::optional<std::uint16_t> ReadinessLevel() const override {
    if (VS::Battle::Ai::roomAiState == nullptr) return std::nullopt;
    return VS::Battle::Ai::roomAiState->readiness_level;
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
  [[nodiscard]] int Random() override { return VS::Core::NextBiosRand(); }
  [[nodiscard]] std::optional<BossBattleActor> BattleActor() const override {
    const VS::Battle::Actor* actor = Actor();
    if (actor == nullptr || actor->stats == nullptr) return std::nullopt;
    return BossBattleActor{
        .core_hp = actor->stats->limbs[kCoreLimb].hp,
        .command = {.opcode = static_cast<CommandOpcode>(actor->command_opcode),
                    .arg0 = actor->command_arg0,
                    .arg1 = actor->command_arg1.word,
                    .arg2 = actor->command_arg2}};
  }
  void QueueBattleCommand(const ActorCommandPacket& packet) override {
    VS::Battle::Actor* actor = Actor();
    Require(actor != nullptr, "AI orbiting boss attack requires its actor");
    actor->command_arg0 = packet.arg0;
    actor->command_arg1.word = packet.arg1;
    actor->command_arg2 = packet.arg2;
    actor->command_opcode = static_cast<std::uint8_t>(packet.opcode);
    actor->command_state = kCommandPending;
  }

 private:
  // The AI state's actor is the battle actor.
  [[nodiscard]] VS::Battle::Actor* Actor() const {
    return reinterpret_cast<VS::Battle::Actor*>(state_.actor);
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

int UpdateLiveBossRadialRetreat(VS::Battle::Ai::RadialRetreatView* state) {
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

int IsLiveBossActionReady(VS::Battle::Ai::ActorAiState* state) {
  AiState& actor = StateOf(state);
  LiveBossOrbitWorld world(actor, nullptr);
  OrbitingBoss boss = Load(actor);
  const std::optional<bool> ready = IsBossActionReady(world, boss);
  Store(boss, actor);
  Require(ready.has_value(),
          "AI boss readiness requires its first candidate and the room state");
  return *ready ? 1 : 0;
}

void UpdateLiveBossOrbitTracking(VS::Battle::Ai::OrbitTrackingView* state) {
  (void)UpdateWithOrbit(
      StateOf(state),
      "AI boss orbit requires a model, stats, the player's record, its first "
      "candidate and the room state",
      [](BossOrbitWorld& world, OrbitingBoss& boss, ArenaOrbit& orbit) {
        return Succeeded(UpdateBossOrbitTracking(world, boss, orbit));
      });
}

int UpdateLiveOrbitingBossBehavior(VS::Battle::Ai::ActorAiState* state) {
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
