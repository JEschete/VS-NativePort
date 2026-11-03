// Hand-maintained (tools/port/owned.csv).
#include "Core/RuntimeLive.hpp"

#include "Core/Runtime.hpp"

namespace VS::Core {
void IgnoreLiveActorCallback(
    [[maybe_unused]] Battle_ActorStats* actor) noexcept {
  IgnoreCallback();
}
}  // namespace VS::Core
