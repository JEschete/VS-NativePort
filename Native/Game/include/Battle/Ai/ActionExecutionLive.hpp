#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Battle::Ai {
struct ActorAiState;
struct ActionCandidateRecord;

[[noreturn]] void RefuseLiveDecisionInputs(const char* message);

template <typename T>
T RequireDecisionValue(const std::optional<T>& value, const char* message) {
  if (!value) RefuseLiveDecisionInputs(message);
  return value.value();
}

std::size_t LiveDecisionListHead();
std::optional<int> LiveDecisionListId(std::size_t node);
std::size_t LiveDecisionListNext(std::size_t node);

template <typename World>
class LiveDecisionList : public World {
 public:
  std::size_t ActorHead() const override { return LiveDecisionListHead(); }
  std::optional<int> ActorId(std::size_t node) const override {
    return LiveDecisionListId(node);
  }
  std::size_t ActorNext(std::size_t node) const override {
    return LiveDecisionListNext(node);
  }
};

std::optional<int> TryExecuteCheckedLiveActionCandidate(
    VS::Battle::Ai::ActorAiState* state,
    VS::Battle::Ai::ActionCandidateRecord* candidate);
std::optional<int> TryExecuteCheckedLivePlannedActions(
    VS::Battle::Ai::ActorAiState* state);
int TryExecuteLiveActionCandidate(
    VS::Battle::Ai::ActorAiState* state,
    VS::Battle::Ai::ActionCandidateRecord* candidate);
int TryExecuteLivePlannedActions(VS::Battle::Ai::ActorAiState* state);
bool UpdateCheckedLiveActorDecisions();
void UpdateLiveActorDecisions();
void ResetLiveDecisionMovement(VS::Battle::Ai::ActorAiState& state,
                               std::uint8_t area);
}  // namespace VS::Battle::Ai
