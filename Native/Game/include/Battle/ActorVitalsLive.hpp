#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/ActorVitals.hpp"

struct Battle_ActorStats;

namespace VS::Battle {
// Checked projection shared by the result and room-vitals bindings.
class LiveActorVitalsAccess final : public ActorVitalsAccess {
 public:
  explicit LiveActorVitalsAccess(Battle_ActorStats& stats);
  ActorVitalsState Read() override;
  void Write(const ActorVitalsState& state) override;
  Battle_ActorStats& Owner();

 private:
  Battle_ActorStats& stats_;
};

int ResolveLiveActorDefeat(int source, int target, int count_kill);
int UpdateLiveActorVitals();
int UpdateLiveRoomActorVitals();
}  // namespace VS::Battle
