#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Battle/Ai/Waypoints.hpp"

struct Battle_Ai_PatrolView;
struct Battle_Ai_WaypointSearchView;
struct Battle_Ai_WaypointTile;

// Binding from the room's waypoints, patrol routes and special terrain
// heights and the actor AI state to the typed waypoint routines
// (Waypoints.cpp). The battle module's pointer table stores both entries at
// their original addresses; each aborts with a diagnostic where the typed
// routine refuses.
namespace VS::Battle::Ai {
// The room's patrol route rows; empty without the room's auxiliary section.
[[nodiscard]] std::span<const std::uint16_t> LivePatrolRoutes();

// func_800DD7CC. Writes the nearest active waypoint's x and z bytes to the
// destination; leaves it alone when no waypoint is active.
void FindLiveNearestActiveWaypoint(Battle_Ai_WaypointSearchView* state,
                                   Battle_Ai_WaypointTile* destination);

// func_800E23AC. Moves the patrol cursor, writes the destination tile and
// dispatches the actor's locomotion.
void UpdateLiveWaypointPatrol(Battle_Ai_PatrolView* state);
}  // namespace VS::Battle::Ai
