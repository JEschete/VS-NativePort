#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <string>

#include "Actors/ActorInstances.hpp"
#include "Battle/CombatAction.hpp"

namespace VS::Battle {
enum class CombatActionId : std::uint64_t { kNone };

// Owns delayed results until completion or cancellation. Effect callbacks
// retain an action id, never a queue slot or a pointer into a request.
class CombatActionSession {
 public:
  CombatActionSession() = default;
  CombatActionSession(const CombatActionSession&) = delete;
  CombatActionSession& operator=(const CombatActionSession&) = delete;

  [[nodiscard]] std::expected<CombatActionId, std::string> Submit(
      CombatActionResult result);
  [[nodiscard]] const CombatActionResult* Find(CombatActionId action) const;
  [[nodiscard]] std::expected<CombatHitResult, std::string> TargetImpact(
      CombatActionId action, std::size_t target,
      Actors::SpawnedActor current_source,
      Actors::SpawnedActor current_target) const;
  [[nodiscard]] std::expected<CombatHitResult, std::string> SourceImpact(
      CombatActionId action, Actors::SpawnedActor current_source) const;
  void Forget(CombatActionId action);
  // Ids are not reused after reset.
  void Reset();

 private:
  [[nodiscard]] std::expected<CombatHitResult, std::string> Impact(
      CombatActionId action, std::optional<std::size_t> target,
      Actors::SpawnedActor current_source,
      Actors::SpawnedActor current_target) const;
  std::map<CombatActionId, CombatActionResult> pending_;
  std::uint64_t next_id_{};
};
}  // namespace VS::Battle
