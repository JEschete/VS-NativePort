#pragma once
// The actor AI tables and bindings: the settings, perception range and wander
// timing tables read from the module image, the decision callbacks, the view
// and facing tables, the per-actor AI states and the room's AI state.
// ActorState.cpp defines them.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Battle::Ai {
struct ActorAiState;
struct RoomAiState;

// One wander setting's move period, rest period and rate.
// decomp: D_80069C3C_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:58)
struct WanderTiming {
  std::uint16_t move_period;
  std::uint16_t rest_period;
  std::uint8_t rate;
  std::uint8_t reserved5;
};
inline constexpr std::size_t kWanderTimingBytes = 0x6;
static_assert(sizeof(WanderTiming) == kWanderTimingBytes,
              "WanderTiming must keep its PS1 layout");

using DecisionCallback = int (*)(ActorAiState*);

inline constexpr std::size_t kTerrainRejectMaskBytes = 1284;
inline constexpr std::size_t kPerceptionRangeTableWords = 320;
inline constexpr std::size_t kWanderTimingPresetCount = 208;
inline constexpr std::size_t kDecisionCallbackCount = 9;
inline constexpr std::size_t kViewAngleCount = 45;
inline constexpr std::size_t kStepHeightCount = 41;
inline constexpr std::size_t kFacingDelayScaleCount = 148;
inline constexpr std::size_t kActorAiStateSlots = 16;

using WordPair = std::array<std::uint16_t, 2>;
using TerrainMaskTable = std::array<std::uint8_t, kTerrainRejectMaskBytes>;
using RangeTable = std::array<std::uint32_t, kPerceptionRangeTableWords>;

// decomp: D_80069C18 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:510)
extern TerrainMaskTable kTerrainRejectMasks;
// decomp: D_80069C1C (src/BATTLE/BATTLE.PRG/6E4CCShared.h:511)
extern RangeTable kPerceptionRangesSquared;
// decomp: D_80069C3C (src/BATTLE/BATTLE.PRG/6E4CCShared.h:512)
extern std::array<WanderTiming, kWanderTimingPresetCount> kWanderTimingPresets;
// decomp: D_800F1790 (src/BATTLE/BATTLE.PRG/7CD68.c:66)
extern std::array<DecisionCallback, kDecisionCallbackCount> decisionCallbacks;
// decomp: D_800F17B8 (src/BATTLE/BATTLE.PRG/7CD68.c:67)
extern std::array<WordPair, kViewAngleCount> viewAngles;
// decomp: D_800F17C8 (src/BATTLE/BATTLE.PRG/7CD68.c:68)
extern std::array<WordPair, kStepHeightCount> stepHeights;
// decomp: D_800F17D8 (src/BATTLE/BATTLE.PRG/7CD68.c:69)
extern std::array<std::uint8_t, kFacingDelayScaleCount> facingDelayScales;
// decomp: D_800F5878 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:522)
extern std::array<ActorAiState*, kActorAiStateSlots> actorAiStates;
// decomp: D_800F58BC (src/BATTLE/BATTLE.PRG/6E4CCShared.h:523)
extern RoomAiState* roomAiState;
// decomp: D_800F58C0 (src/BATTLE/BATTLE.PRG/7CD68.c:71)
extern void* roomAuxiliaryData;
// decomp: D_800F590C (src/BATTLE/BATTLE.PRG/7CD68.c:72)
extern std::int32_t attributeActorMask;
// decomp: D_80069C08 (src/BATTLE/BATTLE.PRG/7CD68.c:207)
extern const std::array<std::uint8_t, 4> kPatrolRates;

}  // namespace VS::Battle::Ai
