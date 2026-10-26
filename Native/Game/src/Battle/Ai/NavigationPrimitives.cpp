// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:685/1238,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/NavigationPrimitives.h"

#include <cstdlib>
#include <cstring>
#include <iterator>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Field/TerrainHeight.h"
}

namespace {
constexpr unsigned kTileMask = 0xFF;
constexpr int kTileZShift = 16;
constexpr unsigned kReservedTileBit = 0x80;
constexpr int kDirectionComponentMask = 3;
constexpr int kDirectionZShift = 4;
constexpr int kDirectionComponentModulus = 4;
constexpr int kMaximumGroundStep = 64;

void RequireTile(int x, int z) {
  if (x < 0 || z < 0 || x >= kAiFrameGridSide || z >= kAiFrameGridSide) {
    Core_Debugf("ai", "AI navigation tile (%d,%d) is out of bounds", x, z);
    std::abort();
  }
}

int SignedDirection(int value) {
  return value > 1 ? value - kDirectionComponentModulus : value;
}

void RequireActorId(int actor_id) {
  if (actor_id < 0 ||
      static_cast<unsigned>(actor_id) >= std::size(Battle_Ai_actorAiStates)) {
    Core_Debugf("ai", "AI navigation actor ID %d is out of bounds", actor_id);
    std::abort();
  }
}

Battle_Ai_ActorAiState* ActorNavigationState(int actor_id) {
  RequireActorId(actor_id);
  return Battle_Ai_tileFlagMap == nullptr ? nullptr
                                          : Battle_Ai_actorAiStates[actor_id];
}
}  // namespace

// decomp: func_800E4C1C (src/BATTLE/BATTLE.PRG/7BDD4.s:542).
void Battle_Ai_ReserveAiDestinationTile(int x, int z) {
  if (Battle_Ai_tileFlagMap == nullptr) return;
  RequireTile(x, z);
  Battle_Ai_tileFlagMap[z][x] |= kReservedTileBit;
}

// decomp: func_800E4690 (src/BATTLE/BATTLE.PRG/7BDD4.s:72).
int Battle_Ai_IsAdjacentTileStepBlocked(unsigned int tile, int direction) {
  const auto* frame = Battle_Ai_FrameState();
  if (direction < 0 ||
      static_cast<unsigned>(direction) >= std::size(frame->directions)) {
    Core_Debugf("ai", "AI neighbor direction %d is out of bounds", direction);
    std::abort();
  }
  const int x = static_cast<int>(tile & kTileMask);
  const int z = static_cast<int>((tile >> kTileZShift) & kTileMask);
  RequireTile(x, z);
  const auto packed = frame->directions[direction];
  const auto dx = SignedDirection(packed & kDirectionComponentMask);
  const auto dz =
      SignedDirection((packed >> kDirectionZShift) & kDirectionComponentMask);
  if ((dx == 0 && dz == 0) || dx < -1 || dz < -1) {
    Core_Debugf("ai", "AI neighbor direction %d has invalid delta (%d,%d)",
                direction, dx, dz);
    std::abort();
  }
  const auto next_x = x + dx;
  const auto next_z = z + dz;
  if (next_x < 0 || next_z < 0 || next_x >= kAiFrameGridSide ||
      next_z >= kAiFrameGridSide) {
    return 1;
  }
  const int raw = Field_GroundHeightFlagsAtTile(next_x, next_z);
  if (raw < 0) return 1;
  return std::abs(Field_GroundHeightAtTile(x, z) -
                  Field_GroundHeightAtTile(next_x, next_z)) >=
         kMaximumGroundStep;
}

// decomp: func_800E5158 (src/BATTLE/BATTLE.PRG/7C7E0.s:127), $a3 = 0.
int Battle_Ai_HasActorNavigationState(int actor_id) {
  return ActorNavigationState(actor_id) != nullptr;
}

// decomp: func_800E5154 (src/BATTLE/BATTLE.PRG/7C7E0.s:122), shared $a3 = 1.
int Battle_Ai_ActorNavigationOverrideStatus(int actor_id) {
  const auto* state = ActorNavigationState(actor_id);
  if (state == nullptr || state->nativePointer18C == nullptr) return 2;
  const auto* route =
      static_cast<const Battle_Ai_ActorAiState*>(state->nativePointer18C);
  if (route->nativePointer64 == nullptr) {
    Core_Debugf("ai", "AI navigation override for actor %d requires a tile",
                actor_id);
    std::abort();
  }
  u_int tile;
  std::memcpy(&tile, route->nativePointer64, sizeof(tile));
  constexpr unsigned kHorizontalTileMask = 0xFF00FF;
  return (tile & kHorizontalTileMask) !=
         (route->destination.word & kHorizontalTileMask);
}

// decomp: func_800E51C8 (src/BATTLE/BATTLE.PRG/7C7E0.s:159).
void Battle_Ai_CopyActorNavigationOverrideState(
    int actor_id, Battle_Ai_ActorAiState* destination) {
  RequireActorId(actor_id);
  const auto* source = Battle_Ai_actorAiStates[actor_id];
  if (source == nullptr || destination == nullptr) {
    Core_Debugf("ai", "AI navigation copy for actor %d requires both states",
                actor_id);
    std::abort();
  }
  *destination = *source;
  destination->actorId = static_cast<u_char>(actor_id);
  constexpr u_short kOverrideStepHeight = 0x140;
  constexpr u_char kOverrideTileStepLimit = 3;
  constexpr u_char kOverrideSettings = 3;
  std::memcpy(destination->step_height_bytes, &kOverrideStepHeight,
              sizeof(kOverrideStepHeight));
  destination->reservedB2 = kOverrideTileStepLimit;
  destination->navigationFlags = 0;
  destination->opaque19A = 0;
  destination->settingsBit14 = kOverrideSettings;
  destination->movementEnabled = 1;
}
