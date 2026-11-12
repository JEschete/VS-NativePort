// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionPreview.hpp"

#include <algorithm>
#include <bit>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
constexpr int kBitsPerByte = 8;
constexpr std::uint8_t kNeutralImpact = 6;
constexpr std::uint8_t kCancelledImpact = 4;
constexpr std::uint8_t kRestoringImpact = 5;
constexpr std::uint8_t kFailedThreshold = 255;
constexpr int kOutcomeEffectLanes = 9;
constexpr std::uint32_t kEffectMask = 3;
constexpr std::uint32_t kMixedEffect = 3;
constexpr std::uint32_t kPositionKindLimit = 6;
constexpr int kAlternatePhysicalCalculator = 25;
constexpr int kTerrainImpactMarker = 0x80;
constexpr int kActorImpactTagLimit = 17;
constexpr int kTransientSpawn = 128;
constexpr int kFirstDirectPrerequisite = 6;
constexpr int kLastDirectPrerequisite = 8;
constexpr int kSelfPrerequisite = 3;
constexpr std::uint32_t kAvailabilityMask = 0xFF000000U;
constexpr std::uint8_t kShapeMask = 7;
constexpr std::uint8_t kPitchMask = 0xF8;
constexpr int kPitchDivisor = 16;
constexpr int kPitchBias = 64;
constexpr std::array kDirectionalCategories{11, 12};
constexpr std::array kAreaCategories{2, 4, 6, 8};
constexpr std::array kVolumeCategories{6, 8, 11, 12};
constexpr std::array kExplicitLimbCategories{5, 7};
constexpr std::array kDefaultLimbCategories{1, 2, 3, 4};
template <std::size_t N>
bool IsCategory(int category, const std::array<int, N>& categories) {
  return std::ranges::find(categories, category) != categories.end();
}
}  // namespace
ActionPoint ActionHit::Position() const {
  const std::array<std::uint16_t, kActionPointComponents> words{
      static_cast<std::uint16_t>(actor | limb << kBitsPerByte),
      static_cast<std::uint16_t>(threshold | impact << kBitsPerByte),
      static_cast<std::uint16_t>(hp), static_cast<std::uint16_t>(mp)};
  return std::bit_cast<ActionPoint>(words);
}
void ActionHit::SetPosition(ActionPoint point) {
  const auto words =
      std::bit_cast<std::array<std::uint16_t, kActionPointComponents>>(point);
  actor = static_cast<std::uint8_t>(words[0]);
  limb = static_cast<std::uint8_t>(words[0] >> kBitsPerByte);
  threshold = static_cast<std::uint8_t>(words[1]);
  impact = static_cast<std::uint8_t>(words[1] >> kBitsPerByte);
  hp = point.z;
  mp = point.tag;
}
// decomp: func_800856F8 (src/BATTLE/BATTLE.PRG/146C.c:13058).
void ClearActionRequest(ActionRequest& request) { request = {}; }
// decomp: func_80085718 (src/BATTLE/BATTLE.PRG/146C.c:13060).
void InitializeActionHit(ActionHit& hit) {
  hit = {};
  hit.impact = kNeutralImpact;
}
// decomp: func_80085A34 (src/BATTLE/BATTLE.PRG/146C.c:13186).
void FinalizeActionHit(ActionHit& hit) {
  if (hit.position_kind != 0) return;
  if (hit.threshold == kFailedThreshold) hit.threshold = 0;
  if (hit.impact != kNeutralImpact) return;
  auto effects = hit.effects;
  if (effects != 0) {
    for (int lane = 0; lane < kOutcomeEffectLanes; ++lane, effects >>= 2) {
      switch (effects & kEffectMask) {
        case 2:
          hit.impact = kRestoringImpact;
          return;
        case 1:
          hit.impact = 0;
          return;
        case kMixedEffect:
          hit.impact = 1;
          return;
        default:
          break;
      }
    }
  } else if ((hit.remove_statuses | hit.inflict_statuses) == 0) {
    hit.impact = kCancelledImpact;
    hit.threshold = 0;
  }
}
namespace {
void InitializeTarget(ActionHit& result, const ActionHit& source,
                      bool primary) {
  InitializeActionHit(result);
  result.position_kind = source.position_kind;
  if (result.position_kind == 0) {
    result.actor = source.actor;
    result.limb = source.limb;
  } else if (!primary && static_cast<std::uint32_t>(result.position_kind) <
                             kPositionKindLimit) {
    result.SetPosition(source.Position());
  }
}
bool SelectDamage(ResolutionDefinition& definition, ResolutionActor* attacker,
                  ActionRequest& result, ActionPreviewWorld& world) {
  const auto affinity = definition.Hit(0).affinity;
  if (affinity != 0) {
    result.affinity = static_cast<std::uint8_t>(affinity - 1);
  } else if (attacker != nullptr) {
    const auto drawn = world.WeaponDrawn(attacker->Slot());
    if (!drawn) return false;
    if ((drawn.value() & 1U) != 0 && attacker->HasBlade()) {
      std::int16_t maximum = 0;
      const auto affinities = attacker->Affinities();
      for (std::size_t i = 0; i < affinities.size(); ++i) {
        if (maximum < affinities.at(i)) {
          maximum = affinities.at(i);
          result.affinity = static_cast<std::uint8_t>(i);
        }
      }
    }
  }
  const auto descriptor = definition.Hit(0);
  result.damage_type = descriptor.damage_type;
  if ((descriptor.stat == 2 ||
       descriptor.stat == kAlternatePhysicalCalculator) &&
      descriptor.damage_type == 0) {
    if (attacker == nullptr) return false;
    result.damage_type = attacker->HasBlade() ? attacker->DamageType() : 1;
  }
  return true;
}
void CancelTarget(ActionHit& target) {
  const auto actor = target.actor;
  const auto limb = target.limb;
  InitializeActionHit(target);
  target.actor = actor;
  target.limb = limb;
  target.impact = kCancelledImpact;
}
bool ResolveSelf(ResolutionDefinition& definition, ActionHit& target, int hit,
                 int execute, ActionPreviewWorld& world) {
  if (target.position_kind != 0) return true;
  const auto owner = world.Actor(target.actor);
  if (!owner) return false;
  auto* stats = owner.value();
  world.Prerequisite(1);
  const auto rolled = world.Roll(definition, target, target, hit, execute);
  world.PreviousHit(rolled);
  if (rolled != 0 || execute == 0) {
    world.Effect(definition, target, target, execute, hit);
    if (execute != 0) {
      if (stats == nullptr) return false;
      if (stats->Cancels()) {
        InitializeActionHit(target);
        target.impact = kCancelledImpact;
      }
    }
    world.Injuries(target);
  }
  return true;
}
std::optional<bool> RedirectTarget(ActionRequest& result, ActionHit& target,
                                   std::optional<ActionPoint> origin,
                                   ActionPreviewWorld& world) {
  if (!origin) return std::nullopt;
  const auto position =
      target.position_kind == 0
          ? world.TargetPosition(target.actor,
                                 std::bit_cast<std::int8_t>(target.limb))
          : std::optional{target.Position()};
  if (!position) return std::nullopt;
  const auto impact =
      world.Trace(origin.value(), position.value(), result.primary.actor);
  if (!impact) return false;
  if (impact.value().tag == 0) {
    target.position_kind = 1;
    target.SetPosition(impact.value());
    target.mp = kTerrainImpactMarker;
    result.animation = 1;
    return true;
  }
  if (static_cast<std::uint16_t>(impact.value().tag) < kActorImpactTagLimit &&
      impact.value().tag - 1 != target.actor) {
    target.actor = static_cast<std::uint8_t>(impact.value().tag - 1);
    const auto actor = world.Actor(target.actor);
    if (!actor || actor.value() == nullptr) return std::nullopt;
    target.limb = actor.value()->DefaultLimb();
    result.animation = 1;
  }
  return false;
}
bool ResolveTarget(ResolutionDefinition& definition, ActionRequest& result,
                   ActionHit& target, int hit, int execute,
                   std::optional<ActionPoint> origin,
                   ActionPreviewWorld& world) {
  if (execute != 0 && definition.Redirects()) {
    const auto redirected = RedirectTarget(result, target, origin, world);
    if (!redirected) return false;
    if (redirected.value()) return true;
  }
  if (target.position_kind != 0) return true;
  const auto owner = world.Actor(target.actor);
  if (!owner) return false;
  auto* stats = owner.value();
  const auto prerequisite = world.Check(definition, target, hit);
  world.Prerequisite(prerequisite);
  if (prerequisite == 0) {
    if (hit == 0) target.threshold = kFailedThreshold;
    return true;
  }
  const auto rolled =
      world.Roll(definition, result.primary, target, hit, execute);
  world.PreviousHit(rolled);
  if (rolled == 0 && execute != 0) return true;
  world.Effect(definition, result.primary, target, execute, hit);
  const auto spawn = world.SpawnId(target.actor);
  if (!spawn) return false;
  if (spawn.value() == kTransientSpawn && target.actor != 0) {
    if (stats == nullptr) return false;
    if (stats->Cancels()) CancelTarget(target);
  }
  if (execute != 0) {
    if (stats == nullptr) return false;
    if (stats->Cancels()) CancelTarget(target);
  }
  world.Injuries(target);
  return true;
}
bool ResolveHits(ResolutionDefinition& definition, ActionRequest& result,
                 int execute, std::optional<ActionPoint> origin,
                 ActionPreviewWorld& world) {
  world.ResetHitState();
  for (int hit = 0; hit < 2; ++hit) {
    const auto prerequisite = definition.Hit(hit).prerequisite;
    if (prerequisite == 0) continue;
    if (prerequisite >= kFirstDirectPrerequisite &&
        prerequisite <= kLastDirectPrerequisite) {
      world.Effect(definition, result.primary, result.targets.front(), execute,
                   hit);
    } else if (prerequisite == kSelfPrerequisite) {
      if (!ResolveSelf(definition, result.primary, hit, execute, world)) {
        return false;
      }
    } else {
      for (std::size_t i = 0; i < result.count; ++i) {
        if (i >= result.targets.size() ||
            !ResolveTarget(definition, result, result.targets.at(i), hit,
                           execute, origin, world)) {
          return false;
        }
      }
    }
  }
  return true;
}
ActionRequest FreshPreview(std::uint32_t action, int source) {
  ActionRequest request{};
  request.action = static_cast<std::uint16_t>(action);
  request.primary.actor = static_cast<std::uint8_t>(source);
  return request;
}
void AddPreviewTarget(ActionRequest& request, int actor, int limb) {
  auto& target = request.targets.at(request.count++);
  target.actor = static_cast<std::uint8_t>(actor);
  target.limb = static_cast<std::uint8_t>(limb);
}
std::optional<std::uint32_t> FinishPreview(std::uint32_t action, int source,
                                           ActionRequest& result,
                                           ActionRequest& request,
                                           ActionPreviewWorld& world) {
  if (!world.Resolve(action, result, request, 0)) return std::nullopt;
  const auto actor = world.Actor(source);
  if (!actor) return std::nullopt;
  const auto cost = world.Cost(action, actor.value(), 0);
  return cost ? std::optional{cost.value() & kAvailabilityMask} : std::nullopt;
}
std::optional<PreviewVolume> BuildVolume(ResolutionDefinition& definition,
                                         ActionPreviewWorld& world) {
  const auto category = definition.Category();
  if (IsCategory(category, kDirectionalCategories)) {
    auto volume = definition.Range();
    const auto source = world.PreviewPosition(false);
    const auto target = world.PreviewPosition(true);
    if (!source || !target) return std::nullopt;
    const auto dx = target.value().x - source.value().x;
    const auto dz = target.value().z - source.value().z;
    const auto dy = target.value().y - source.value().y;
    namespace math = Core::Math;
    const auto distance = math::SquareRootInteger(math::WrappingAdd(
        math::WrappingMultiply(dx, dx), math::WrappingMultiply(dz, dz)));
    volume.shape &= kShapeMask;
    volume.kind = 1;
    volume.yaw = static_cast<std::int16_t>(math::Arctangent2(dx, dz));
    volume.pitch = static_cast<std::uint8_t>(
        math::Arctangent2(dy, distance) / kPitchDivisor + kPitchBias);
    volume.origin = source.value();
    return volume;
  }
  if (IsCategory(category, kAreaCategories)) {
    auto volume = definition.Area();
    const auto target = world.PreviewPosition(true);
    if (!target) return std::nullopt;
    volume.shape &= kShapeMask;
    volume.kind = 1;
    volume.yaw = 0;
    volume.pitch &= kPitchMask;
    volume.origin = target.value();
    return volume;
  }
  return PreviewVolume{};
}
}  // namespace
// decomp: func_80085B10 (src/BATTLE/BATTLE.PRG/146C.c:13221).
bool ResolveActionTargets(std::uint32_t action, ActionRequest& result,
                          ActionRequest& source, int execute,
                          ActionPreviewWorld& world) {
  auto* definition = world.Definition(action);
  if (definition == nullptr) return false;
  ClearActionRequest(result);
  result.action = static_cast<std::uint16_t>(action);
  InitializeTarget(result.primary, source.primary, true);
  ResolutionActor* attacker = nullptr;
  std::optional<ActionPoint> origin;
  if (result.primary.position_kind == 0) {
    const auto owner = world.Actor(result.primary.actor);
    if (!owner) return false;
    attacker = owner.value();
    origin = world.SourceOrigin(result.primary.actor);
  }
  result.count = source.count;
  for (std::size_t i = 0; i < source.count; ++i) {
    if (i >= result.targets.size()) return false;
    InitializeTarget(result.targets.at(i), source.targets.at(i), false);
  }
  result.focus = source.focus;
  if (!SelectDamage(*definition, attacker, result, world) ||
      !ResolveHits(*definition, result, execute, origin, world)) {
    return false;
  }
  FinalizeActionHit(result.primary);
  for (std::size_t i = 0; i < result.count; ++i) {
    if (i >= result.targets.size()) return false;
    FinalizeActionHit(result.targets.at(i));
  }
  const auto queue = world.QueueIndex();
  if (!queue) return false;
  if (queue.value() == 0 && execute != 0) source = result;
  world.Publish();
  return execute == 0 || attacker == nullptr ||
         (world.Recovery(action, *attacker) &&
          world.Cost(action, attacker, 1).has_value());
}
bool ActionPreviewWorld::Resolve(std::uint32_t action, ActionRequest& result,
                                 ActionRequest& source, int execute) {
  return ResolveActionTargets(action, result, source, execute, *this);
}
// decomp: func_8008631C (src/BATTLE/BATTLE.PRG/146C.c:13421).
std::optional<std::uint32_t> PreviewActionAgainstLimb(
    std::uint32_t action, int source, int target, int limb,
    ActionRequest& result, ActionPreviewWorld& world) {
  auto request = FreshPreview(action, source);
  AddPreviewTarget(request, target, limb);
  return FinishPreview(action, source, result, request, world);
}
// decomp: func_800863A4 (src/BATTLE/BATTLE.PRG/146C.c:13436).
std::optional<std::uint32_t> PreviewActionAgainstActor(
    std::uint32_t action, int source, int target, int limb,
    ActionRequest& result, ActionPreviewWorld& world) {
  auto request = FreshPreview(action, source);
  auto* definition = world.Definition(action);
  if (definition == nullptr) return std::nullopt;
  const auto volume = BuildVolume(*definition, world);
  if (!volume) return std::nullopt;
  const auto category = definition->Category();
  if (IsCategory(category, kVolumeCategories)) {
    for (int i = 0; i < static_cast<int>(kActionLimbCount); ++i) {
      const auto position = world.LimbPosition(target, i);
      if (position && world.Contains(volume.value(), position.value())) {
        AddPreviewTarget(request, target, i);
      }
    }
  } else if (IsCategory(category, kExplicitLimbCategories)) {
    AddPreviewTarget(request, target, limb);
  } else if (IsCategory(category, kDefaultLimbCategories)) {
    const auto actor = world.Actor(target);
    if (!actor || actor.value() == nullptr) return std::nullopt;
    AddPreviewTarget(request, target, actor.value()->DefaultLimb());
  }
  return FinishPreview(action, source, result, request, world);
}
}  // namespace VS::Battle
