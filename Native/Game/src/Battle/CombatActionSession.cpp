#include "Battle/CombatActionSession.hpp"

#include <limits>
#include <string>
#include <utility>
#include <variant>

namespace VS::Battle {
std::expected<CombatActionId, std::string> CombatActionSession::Submit(
    CombatActionResult result) {
  if (result.intent.source == Actors::SpawnedActor::kNone) {
    return std::unexpected("Combat action has no source identity");
  }
  if (next_id_ == std::numeric_limits<std::uint64_t>::max()) {
    return std::unexpected("Combat action identity space is exhausted");
  }
  // The fixed underlying type carries the complete ticket range.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto id = static_cast<CombatActionId>(++next_id_);
  pending_.emplace(id, std::move(result));
  return id;
}

const CombatActionResult* CombatActionSession::Find(
    CombatActionId action) const {
  const auto found = pending_.find(action);
  return found == pending_.end() ? nullptr : &found->second;
}

std::expected<CombatHitResult, std::string> CombatActionSession::Impact(
    CombatActionId action, std::optional<std::size_t> target,
    Actors::SpawnedActor current_source,
    Actors::SpawnedActor current_target) const {
  const auto found = pending_.find(action);
  if (found == pending_.end()) {
    return std::unexpected("Combat impact belongs to an inactive action");
  }
  const auto& result = found->second;
  if (current_source != result.intent.source) {
    return std::unexpected("Combat impact source has been replaced");
  }
  if (target && *target >= result.hits.size()) {
    return std::unexpected("Combat impact target is outside the action");
  }
  const auto& hit = target ? result.hits.at(*target) : result.source;
  const auto* actor = std::get_if<CombatActorTarget>(&hit.target);
  if (actor == nullptr || actor->actor != current_target ||
      current_target == Actors::SpawnedActor::kNone) {
    return std::unexpected("Combat impact target has been replaced");
  }
  return hit;
}

std::expected<CombatHitResult, std::string> CombatActionSession::TargetImpact(
    CombatActionId action, std::size_t target,
    Actors::SpawnedActor current_source,
    Actors::SpawnedActor current_target) const {
  return Impact(action, target, current_source, current_target);
}

std::expected<CombatHitResult, std::string> CombatActionSession::SourceImpact(
    CombatActionId action, Actors::SpawnedActor current_source) const {
  return Impact(action, std::nullopt, current_source, current_source);
}

void CombatActionSession::Forget(CombatActionId action) {
  pending_.erase(action);
}

void CombatActionSession::Reset() { pending_.clear(); }
}  // namespace VS::Battle
