#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/ReactionDelay.hpp"
#include "Content/ModuleData.h"

namespace {
namespace ai = VS::Battle::Ai;

class RecordingDelays final : public ai::ActorReactionDelays {
 public:
  struct Write {
    int actor_id;
    std::uint8_t frames;
  };

  bool SetMinimumActionDelay(int actor_id, std::uint8_t frames) override {
    writes.push_back({actor_id, frames});
    return actor_id != empty_slot;
  }

  int empty_slot = -1;
  std::vector<Write> writes;
};
}  // namespace

TEST_CASE(
    "A reaction-delay reset writes thirty frames to every AI slot but Ashley's",
    "[ai][decisions]") {
  RecordingDelays delays;
  delays.empty_slot = 5;
  for (int id = 0; id < ai::kAiActorSlotCount; ++id) {
    ai::ResetActorAiReactionDelay(delays, id);
  }
  REQUIRE(delays.writes.size() == ai::kAiActorSlotCount - 1);
  for (std::size_t i = 0; i < delays.writes.size(); ++i) {
    CHECK(delays.writes[i].actor_id == static_cast<int>(i) + 1);
    CHECK(delays.writes[i].frames == 30);
  }
}

TEST_CASE("Promoted reaction-delay reset has no original module address",
          "[ai][decisions]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // func_800E4C64 is reached only by direct jal, so the BATTLE.PRG address
  // never resolves and callers use the typed API by name.
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E4C64u) == nullptr);
}
