#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

#include "Battle/CombatActionSession.hpp"
#include "Battle/CombatImpact.hpp"

namespace VS::Battle {
struct ActionRequestRecord;

struct LiveCombatResolution {
  CombatActionResult result;
  CombatActionId id = CombatActionId::kNone;
};

[[nodiscard]] std::expected<CombatActorState, std::string> ReadLiveCombatActor(
    Actors::SpawnedActor actor);

// Targets have already been queried. This boundary uses owned actor components,
// native catalogs and the game RNG, but no camera, menu, skeleton or renderer.
[[nodiscard]] std::expected<LiveCombatResolution, std::string>
ResolveLiveCombatAction(const CombatActionIntent& intent,
                        std::uint32_t chain_step, bool execute);

// The compatibility ingress only selects the adopted normal physical attack.
// An empty optional means a different, not-yet-migrated action, not a refusal.
[[nodiscard]] std::expected<std::optional<std::uint32_t>, std::string>
TryResolveLiveCombatRequest(int action, ActionRequestRecord& result,
                            ActionRequestRecord& source, bool execute);

[[nodiscard]] CombatActionId LiveCombatActionForRequest(
    const ActionRequestRecord& request);
[[nodiscard]] CombatActionId CurrentLiveCombatAction();
// Borrow only until the next session mutation; consumers spanning callbacks
// copy the result and retain its id.
[[nodiscard]] const CombatActionResult* FindLiveCombatAction(CombatActionId id);
[[nodiscard]] ActionTargetHit LiveCombatTarget(const CombatHitResult& hit);
[[nodiscard]] ActionHit ProjectLiveCombatHit(const CombatHitResult& hit);
[[nodiscard]] std::optional<std::uint8_t> LiveCombatPresentationSlot(
    Actors::SpawnedActor actor);
[[nodiscard]] bool LiveCombatPresentationAvailable(CombatActionId action);
void ForgetLiveCombatAction(CombatActionId action);
void ForgetLiveCombatRequest(const ActionRequestRecord& request);
void ResetLiveCombatActions();

// Commits current target vitals and returns presentation events separately.
[[nodiscard]] std::expected<CombatImpactResult, std::string>
CommitLiveCombatImpact(CombatActionId action,
                       std::optional<std::size_t> target);
void PresentLiveCombatImpact(const CombatImpactResult& impact);
void ApplyLiveCombatTargetEffects(CombatActionId action, int actor,
                                  std::optional<ActionPoint> position);
void ApplyLiveCombatSourceEffects(CombatActionId action, int actor);
void ApplyLiveCombatPrimaryResult(CombatActionId action);
}  // namespace VS::Battle
