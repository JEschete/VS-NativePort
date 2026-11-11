#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Battle/Ai/AreaRoute.hpp"

struct Battle_Ai_AreaRouteView;
struct Battle_Ai_RouteGateTile;

// Binding from the room's AI frame, route table, route links, moving
// platforms and the actor AI state to the typed area routing
// (AreaRoute.cpp). The battle module's pointer table stores the four entries
// at their original addresses; each aborts with a diagnostic where the typed
// routine refuses.
namespace VS::Battle::Ai {
// The ground end nearest the platform's link (FindNearestRouteGateEntry);
// nothing without the room's route links or for a platform outside 0..3.
[[nodiscard]] std::optional<RouteGateEnd> FindLiveNearestRouteGateEnd(
    int platform, std::int32_t world_x, std::int32_t world_z);

// The route link at the tile (FindRouteGateAtTile), or -1; nothing without
// the room's route links or with more than four platform reports.
[[nodiscard]] std::optional<int> FindLiveRouteGateLink(std::uint32_t tile,
                                                       bool match_entry_end);

// func_800DF9A8. Writes the exit's three bytes, or the whole four-byte entry
// word when no link starts at the entry tile, as the original did.
void ResolveLiveRouteGateExit(Battle_Ai_RouteGateTile* entry_tile,
                              Battle_Ai_RouteGateTile* exit_tile);

// func_800DFA54. Writes the entry's x and z bytes.
void FindLiveNearestRouteGateEntry(int platform, int world_x, int world_z,
                                   Battle_Ai_RouteGateTile* entry_tile);

// func_800DFAF8. The original ignored its first argument.
int FindLiveRouteGateAtTile(std::intptr_t state, unsigned int tile,
                            int match_entry_end);

// func_800DFBCC. Returns the command intent for the actor AI state.
int ResolveLiveNavigationAreaRoute(Battle_Ai_AreaRouteView* state, int index,
                                   int area);
}  // namespace VS::Battle::Ai
