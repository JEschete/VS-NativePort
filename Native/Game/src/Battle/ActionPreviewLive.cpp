// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionPreviewLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include "Actors/SkeletonLive.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/ActionPreview.hpp"
#include "Battle/AttackPrerequisitesLive.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Field/SegmentCollisionLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Sphere.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
static_assert(sizeof(ActionHit) == sizeof(Battle_HitResult));
static_assert(sizeof(ActionRequest) == sizeof(Battle_ActionRequest));
static_assert(offsetof(ActionRequest, targets) ==
              offsetof(Battle_ActionRequest, targets));
static_assert(offsetof(ActionRequest, focus) ==
              offsetof(Battle_ActionRequest, focusPosition));
[[noreturn]] void Refuse() {
  Core_Errorf("battle",
              "Action preview/resolution refused used missing state "
              "or index; world physical-auto damage and redirects "
              "without an available source origin are unsupported");
  std::abort();
}
template <typename T>
T Checked(std::optional<T> value) {
  if (!value) Refuse();
  return value.value();
}
Battle_Actor* ActorAt(int slot) {
  return slot >= 0 && static_cast<std::size_t>(slot) < std::size(Battle_actors)
             ? Battle_actors[slot]
             : nullptr;
}
struct LiveActor final : ResolutionActor {
  explicit LiveActor(Battle_ActorStats& value) : stats(value) {}
  Battle_ActorStats& stats;
  std::uint8_t Slot() override { return stats.actorId; }
  std::uint8_t DefaultLimb() override { return stats.defaultTargetLimb; }
  bool Cancels() override { return stats.cancelsHitResults != 0; }
  bool HasBlade() override { return stats.weapon.blade.id != 0; }
  std::uint8_t DamageType() override {
    return static_cast<std::uint8_t>(stats.weapon.damageType);
  }
  std::array<std::int16_t, kActionAffinityCount> Affinities() override {
    std::array<std::int16_t, kActionAffinityCount> values{};
    std::copy_n(stats.weapon.classAffinityCurrent.affinities[0], values.size(),
                values.begin());
    return values;
  }
};
struct LiveDefinition final : ResolutionDefinition {
  explicit LiveDefinition(Battle_ActionDefinition& value) : definition(value) {}
  Battle_ActionDefinition& definition;
  ResolutionDescriptor Hit(int hit) override {
    const auto value = definition.hits[hit];
    return {static_cast<std::uint8_t>(value.prerequisiteFunctionId),
            static_cast<std::uint8_t>(value.statCalculatorId),
            static_cast<std::uint8_t>(value.damageType),
            static_cast<std::uint8_t>(value.affinity)};
  }
  bool Redirects() override { return definition.resetsHitResults != 0; }
  std::uint8_t Category() override { return definition.category; }
  PreviewVolume Range() override {
    return {definition.shape,
            1,
            0,
            {definition.rangeX, definition.rangeY, definition.rangeZ},
            0,
            {}};
  }
  PreviewVolume Area() override {
    return {
        definition.areaShape,
        1,
        0,
        {definition.areaRangeX, definition.areaRangeY, definition.areaRangeZ},
        definition.areaShape,
        {}};
  }
};
class LiveWorld final : public ActionPreviewWorld {
 public:
  LiveWorld() = default;
  LiveWorld(ActionRequest& result, ActionRequest& source,
            Battle_ActionRequest& live_result,
            Battle_ActionRequest& live_source)
      : result_(&result),
        source_(&source),
        live_result_(&live_result),
        live_source_(&live_source) {}
  const SVECTOR* source_position{};
  const SVECTOR* target_position{};
  Battle_ActionRequest* preview_result{};
  ResolutionDefinition* Definition(std::uint32_t action) override {
    if (action >= definitions_.size()) return nullptr;
    auto& definition = definitions_.at(action);
    if (!definition) definition.emplace(Battle_actionDefinitions[action]);
    return &definition.value();
  }
  std::optional<ResolutionActor*> Actor(int slot) override {
    auto* actor = ActorAt(slot);
    if (actor == nullptr) return std::nullopt;
    if (actor->stats == nullptr) return nullptr;
    for (auto& owner : owners_) {
      if (&owner->stats == actor->stats) return owner.get();
    }
    return owners_.emplace_back(std::make_unique<LiveActor>(*actor->stats))
        .get();
  }
  std::optional<std::uint32_t> WeaponDrawn(int slot) override {
    const auto* actor = ActorAt(slot);
    return actor ? std::optional{static_cast<std::uint32_t>(actor->weaponDrawn)}
                 : std::nullopt;
  }
  std::optional<std::uint8_t> SpawnId(int slot) override {
    const auto* actor = ActorAt(slot);
    return actor ? std::optional{actor->roomSpawnIndex} : std::nullopt;
  }
  std::optional<ActionPoint> SourceOrigin(int slot) override {
    Callback callback(*this);
    SVECTOR position{};
    return Actors::LiveActorBonePosition(slot, 0, &position, 0) >= 0
               ? std::optional{std::bit_cast<ActionPoint>(position)}
               : std::nullopt;
  }
  std::optional<ActionPoint> TargetPosition(int slot, int limb) override {
    const auto actor = Actor(slot);
    if (!actor || actor.value() == nullptr || limb < 0 ||
        static_cast<std::size_t>(limb) >= kActionLimbCount) {
      return std::nullopt;
    }
    Callback callback(*this);
    SVECTOR position{};
    Battle_ActionTargetLimbPosition(slot, limb, &position);
    return std::bit_cast<ActionPoint>(position);
  }
  std::optional<ActionPoint> LimbPosition(int slot, int limb) override {
    Callback callback(*this);
    SVECTOR position{};
    return Actors::LiveActorLowGroupBonePosition(slot, limb, &position, 2) >= 0
               ? std::optional{std::bit_cast<ActionPoint>(position)}
               : std::nullopt;
  }
  std::optional<ActionPoint> PreviewPosition(bool target) override {
    const auto* position = target ? target_position : source_position;
    return position ? std::optional{std::bit_cast<ActionPoint>(*position)}
                    : std::nullopt;
  }
  bool Contains(const PreviewVolume& volume, ActionPoint point) override {
    Callback callback(*this);
    auto live_volume = std::bit_cast<Battle_TargetVolume>(volume);
    auto components =
        std::bit_cast<std::array<std::uint16_t, kActionPointComponents>>(point);
    return Battle_IsPointInTargetVolume(&live_volume, components.data()) != 0;
  }
  std::optional<ActionPoint> Trace(ActionPoint origin, ActionPoint target,
                                   int source) override {
    Callback callback(*this);
    const auto from = std::bit_cast<SVECTOR>(origin);
    const auto to = std::bit_cast<SVECTOR>(target);
    SVECTOR impact{};
    return Field::TraceLiveActorAndTerrainCollision(&from, &to, &impact, source)
               ? std::optional{std::bit_cast<ActionPoint>(impact)}
               : std::nullopt;
  }
  void ResetHitState() override {
    Battle_lastActionStatValue = 0;
    Battle_previousHitResult = 0;
    Battle_hitPrerequisiteResult = 0;
  }
  void Prerequisite(std::uint32_t value) override {
    Battle_hitPrerequisiteResult = value;
  }
  void PreviousHit(std::uint32_t value) override {
    Battle_previousHitResult = value;
  }
  std::optional<std::uint32_t> QueueIndex() override {
    return Battle_actionState ? std::optional{Battle_actionState->queueIndex}
                              : std::nullopt;
  }
  std::uint32_t Check(ResolutionDefinition& definition, ActionHit& target,
                      int hit) override {
    Callback callback(*this);
    auto& owner = static_cast<LiveDefinition&>(definition);
    return static_cast<std::uint32_t>(LiveCheckActionHitPrerequisite(
        &owner.definition, reinterpret_cast<unsigned char*>(LiveHit(target)),
        hit));
  }
  std::uint32_t Roll(ResolutionDefinition& definition, ActionHit& source,
                     ActionHit& target, int hit, int execute) override {
    Callback callback(*this);
    auto& owner = static_cast<LiveDefinition&>(definition);
    return static_cast<std::uint32_t>(LiveRollActionHit(
        &owner.definition, LiveHit(source), LiveHit(target), hit, execute));
  }
  void Effect(ResolutionDefinition& definition, ActionHit& source,
              ActionHit& target, int execute, int hit) override {
    Callback callback(*this);
    auto& owner = static_cast<LiveDefinition&>(definition);
    Battle_ApplyActionHitEffect(&owner.definition, LiveHit(source),
                                LiveHit(target), execute, hit);
  }
  void Injuries(ActionHit& target) override {
    Callback callback(*this);
    Battle_DeriveLimbInjuryStatuses(LiveHit(target));
  }
  bool Recovery(std::uint32_t action, ResolutionActor& actor) override {
    Callback callback(*this);
    auto& owner = static_cast<LiveActor&>(actor);
    LiveAddActionRecoveryDelay(action, &owner.stats, 1);
    return true;
  }
  std::optional<std::uint32_t> Cost(std::uint32_t action,
                                    ResolutionActor* actor,
                                    int update) override {
    Callback callback(*this);
    auto* owner = static_cast<LiveActor*>(actor);
    return LiveActionCostAndAvailability(
        action, owner ? &owner->stats : nullptr, update);
  }
  bool Resolve(std::uint32_t action, ActionRequest& result,
               ActionRequest& source, int execute) override {
    if (preview_result == nullptr) Refuse();
    auto live_source = std::bit_cast<Battle_ActionRequest>(source);
    ResolveLiveActionTargets(std::bit_cast<int>(action), preview_result,
                             &live_source, execute);
    result = std::bit_cast<ActionRequest>(*preview_result);
    source = std::bit_cast<ActionRequest>(live_source);
    return true;
  }
  void Publish() override {
    if (live_result_ == nullptr) return;
    *live_result_ = std::bit_cast<Battle_ActionRequest>(*result_);
    if (live_source_ != live_result_) {
      *live_source_ = std::bit_cast<Battle_ActionRequest>(*source_);
    }
  }

 private:
  // Legacy callbacks can touch either request through globals. Publish all
  // native writes before entry, then reload the complete native-capacity
  // records, retaining only the explicitly cached definition/stats owners.
  class Callback {
   public:
    explicit Callback(LiveWorld& world) : world_(world) { world_.Publish(); }
    ~Callback() { world_.Reload(); }

   private:
    LiveWorld& world_;
  };
  void Reload() {
    if (live_result_ == nullptr) return;
    *result_ = std::bit_cast<ActionRequest>(*live_result_);
    if (live_source_ != live_result_) {
      *source_ = std::bit_cast<ActionRequest>(*live_source_);
    }
  }
  Battle_HitResult* LiveHit(ActionHit& hit) {
    if (result_ == nullptr) Refuse();
    if (&hit == &result_->primary) return &live_result_->primaryTarget;
    for (std::size_t i = 0; i < result_->targets.size(); ++i) {
      if (&hit == &result_->targets.at(i)) return &live_result_->targets[i];
    }
    Refuse();
  }
  ActionRequest* result_{};
  ActionRequest* source_{};
  Battle_ActionRequest* live_result_{};
  Battle_ActionRequest* live_source_{};
  std::array<std::optional<LiveDefinition>, std::size(Battle_actionDefinitions)>
      definitions_{};
  std::vector<std::unique_ptr<LiveActor>> owners_;
};
}  // namespace
// decomp: func_800856F8 (src/BATTLE/BATTLE.PRG/146C.c:13058).
void ClearLiveActionRequest(void* request) {
  if (request == nullptr) Refuse();
  ActionRequest value;
  ClearActionRequest(value);
  std::memcpy(request, &value, sizeof value);
}
// decomp: func_80085718 (src/BATTLE/BATTLE.PRG/146C.c:13060).
void InitializeLiveActionHit(Battle_HitResult* hit) {
  if (hit == nullptr) Refuse();
  ActionHit value;
  InitializeActionHit(value);
  *hit = std::bit_cast<Battle_HitResult>(value);
}
// decomp: func_80085A34 (src/BATTLE/BATTLE.PRG/146C.c:13186).
void FinalizeLiveActionHit(Battle_HitStatusView* hit) {
  if (hit == nullptr) Refuse();
  auto value = std::bit_cast<ActionHit>(*hit);
  FinalizeActionHit(value);
  *hit = std::bit_cast<Battle_HitStatusView>(value);
}
// decomp: func_80085B10 (src/BATTLE/BATTLE.PRG/146C.c:13221).
void ResolveLiveActionTargets(int action, Battle_ActionRequest* result,
                              Battle_ActionRequest* source, int execute) {
  if (result == nullptr || source == nullptr) Refuse();
  ActionRequest native_result{};
  ActionRequest native_source{};
  if (result != source) native_source = std::bit_cast<ActionRequest>(*source);
  auto& input = result == source ? native_result : native_source;
  LiveWorld world(native_result, input, *result, *source);
  if (!ResolveActionTargets(static_cast<std::uint32_t>(action), native_result,
                            input, execute, world)) {
    Refuse();
  }
  world.Publish();
}
// decomp: func_8008631C (src/BATTLE/BATTLE.PRG/146C.c:13421).
int PreviewLiveActionAgainstLimb(int action, int source, int target, int limb,
                                 void* result) {
  if (result == nullptr) Refuse();
  LiveWorld world;
  world.preview_result = static_cast<Battle_ActionRequest*>(result);
  ActionRequest output{};
  return std::bit_cast<int>(
      Checked(PreviewActionAgainstLimb(static_cast<std::uint32_t>(action),
                                       source, target, limb, output, world)));
}
// decomp: func_800863A4 (src/BATTLE/BATTLE.PRG/146C.c:13436).
int PreviewLiveActionAgainstActor(int action, int source, int target, int limb,
                                  SVECTOR* source_position,
                                  SVECTOR* target_position,
                                  Battle_ActionRequest* result) {
  if (result == nullptr) Refuse();
  LiveWorld world;
  world.preview_result = result;
  world.source_position = source_position;
  world.target_position = target_position;
  ActionRequest output{};
  return std::bit_cast<int>(
      Checked(PreviewActionAgainstActor(static_cast<std::uint32_t>(action),
                                        source, target, limb, output, world)));
}
}  // namespace VS::Battle
