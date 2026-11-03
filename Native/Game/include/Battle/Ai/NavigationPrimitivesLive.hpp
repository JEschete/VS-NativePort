#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/NavigationPrimitives.hpp"

struct Battle_Ai_ActorAiState;
struct Field_GimmickInstance;

// Bindings from the room's AI tile maps, route table, AI state table and
// battle actor list to the typed navigation primitives
// (NavigationPrimitives.cpp). Each aborts with a diagnostic when the typed
// routine refuses its input; the original read or wrote past its tables.
namespace VS::Battle::Ai {
void ReserveLiveAiDestinationTile(int x, int z);
void MarkLiveAiTileDynamicObstacle(int x, int z);
void AddLiveAiTileOccupant(const Field_GimmickInstance* gimmick);
void RemoveLiveAiTileOccupant(const Field_GimmickInstance* gimmick);
// Packed tile: x in bits 0-7, z in bits 16-23.
int IsLiveAdjacentTileStepBlocked(unsigned int tile, int direction);
void BuildLiveAiTerrainHeightsWithWorkspace();
void UpdateLiveRouteGateAvailability();
int HasLiveActorNavigationState(int actor_id);
int LiveActorNavigationOverrideStatus(int actor_id);
void CopyLiveActorNavigationOverrideState(int actor_id,
                                          Battle_Ai_ActorAiState* destination);
void SetLiveActorNavigationOverride(int actor_id, int x, int z);
void UpdateLiveActorNavigationOverrides();
}  // namespace VS::Battle::Ai
