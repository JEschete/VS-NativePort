#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/ActorVitals.hpp"

namespace VS::Battle {

struct ActorStats;
// Checked projection shared by the result and room-vitals bindings.
class LiveActorVitalsAccess final : public ActorVitalsAccess {
 public:
  explicit LiveActorVitalsAccess(VS::Battle::ActorStats& stats);
  ActorVitalsState Read() override;
  void Write(const ActorVitalsState& state) override;
  VS::Battle::ActorStats& Owner();

 private:
  VS::Battle::ActorStats& stats_;
};

int ResolveLiveActorDefeat(int source, int target, int count_kill);
int UpdateLiveActorVitals();
int UpdateLiveRoomActorVitals();
}  // namespace VS::Battle
