#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Actors/ActorPlacement.hpp"

namespace VS::Actors {
struct ActorLocation;
}
namespace VS::Actors {
struct ActorLocationQuery;
}
namespace VS::Battle {
struct Actor;
}  // namespace VS::Battle
namespace VS::Render {
struct ColorCode;
}  // namespace VS::Render
namespace VS::Core::Gte {
struct SVECTOR;
}  // namespace VS::Core::Gte

// Binding from the actor table, the battle actors, the slot gimmicks and the
// weapon models to the typed actor state rules (ActorFlags.hpp,
// BattleActorState.hpp, ActorLifecycle.hpp and ActorPlacement.hpp). Each
// adapter keeps the original argument order for the generated callers
// (copy_fixups.py OWNED_CALL_BINDINGS). An id outside its table is refused
// with a warning on the "actors" channel and does nothing (or reports
// nothing), where the originals read past the table.
namespace VS::Actors {
struct SpawnPlacement;

// decomp: BATTLE.PRG func_8007BF6C (src/BATTLE/BATTLE.PRG/146C.c:8566)
void EnableLiveActorModel(int actor_id);
// decomp: BATTLE.PRG func_8007BFC0 (src/BATTLE/BATTLE.PRG/146C.c:8573)
void EnableAllLiveActorModels();
// decomp: BATTLE.PRG func_8007BFF8 (src/BATTLE/BATTLE.PRG/146C.c:8582)
void DisableLiveActorModel(int actor_id);
// decomp: BATTLE.PRG func_8007C050 (src/BATTLE/BATTLE.PRG/146C.c:8589)
void DisableAllLiveActorModels();
// decomp: BATTLE.PRG func_8007C1D8 (src/BATTLE/BATTLE.PRG/146C.c:8629)
void SetLiveActorHostile(int actor_id, int hostile);
// decomp: BATTLE.PRG func_8007C218 (src/BATTLE/BATTLE.PRG/146C.c:8640)
// 0 for an empty slot, which the original read through.
int LiveActorHpPercentage(int actor_id);
// decomp: BATTLE.PRG fullyRestoreActor (src/BATTLE/BATTLE.PRG/146C.c:8661; was
// func_8007C28C)
void RestoreLiveActorVitals(int actor_id);
// decomp: BATTLE.PRG func_8007CF18 (src/BATTLE/BATTLE.PRG/146C.c:9093)
int FindLiveActorByModelId(int model_id);
// decomp: BATTLE.PRG getActor (src/BATTLE/BATTLE.PRG/146C.c:9105; was
// func_8007CF64) Null for an empty slot or an id outside VS::Battle::actors.
VS::Battle::Actor* LiveActorAtSlot(int actor_id);
// decomp: BATTLE.PRG func_8007CF80 (src/BATTLE/BATTLE.PRG/146C.c:9107)
int CountLiveActorsByDisposition(int actor_kind);
// decomp: BATTLE.PRG func_8007CFCC (src/BATTLE/BATTLE.PRG/146C.c:9121)
char IsLiveActorRetainedAfterDefeat(int actor_id);
// decomp: BATTLE.PRG setActorAwake (src/BATTLE/BATTLE.PRG/146C.c:9153; was
// func_8007D08C) Returns `enabled`, as the original did.
int SetLiveActorLogicEnabled(int actor_id, int enabled);
// decomp: BATTLE.PRG setActorsAwake (src/BATTLE/BATTLE.PRG/146C.c:9179; was
// func_8007D15C)
int SetOtherLiveActorLogicEnabled(int enabled);
// decomp: BATTLE.PRG setActorInactive (src/BATTLE/BATTLE.PRG/146C.c:9195; was
// func_8007D1A8)
void SetLiveActorDetached(int actor_id, int detached);

// decomp: BATTLE.PRG func_8009D468 (src/BATTLE/BATTLE.PRG/30DB0.c:2133)
// `unused` is the original's unread second argument.
void ResetLiveActorMotionState(int actor_id, int unused);
// decomp: BATTLE.PRG func_8009D6F4 (src/BATTLE/BATTLE.PRG/30DB0.c:2207)
void ResetAllLiveActorMovement();
// decomp: BATTLE.PRG startActorDeath (src/BATTLE/BATTLE.PRG/30DB0.c:3291; was
// func_8009F314) `context` is only passed down the original's companion
// recursion.
void BeginLiveActorDeath(int actor_id, void* context, int animate);
// decomp: BATTLE.PRG vs_battle_setActorEdgeHighlight
// (src/BATTLE/BATTLE.PRG/30DB0.c:3487; was func_8009F940) `companion_state`
// holds the four VS::Actors::CompanionOffsetRecord bytes; a nonzero slot
// without both arguments keeps the actor's current style, and a slot outside a
// byte is refused. NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the
// original order.
void SetLiveActorCompanion(int actor_id, int companion_slot,
                           const VS::Render::ColorCode* companion_state,
                           const VS::Render::ColorCode* companion_color);
// decomp: BATTLE.PRG attachActor (src/BATTLE/BATTLE.PRG/30DB0.c:3713; was
// func_8009FF08) A parent of -1 keeps the current one; `unused` is never read.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void AttachLiveActorToParentBone(int actor_id, int parent_actor_id,
                                 int parent_bone_index, int unused);
// decomp: BATTLE.PRG detachActor (src/BATTLE/BATTLE.PRG/30DB0.c:3735; was
// func_8009FFA8) AttachmentRelease as an int: 0 released, -1 parent not ready,
// -2 no parent. NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the
// original order.
int ReleaseLiveActorAttachmentLock(int actor_id, int unused);
// decomp: BATTLE.PRG vs_battle_setActorFlags
// (src/BATTLE/BATTLE.PRG/30DB0.c:4069; was func_800A087C)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveActorDisplayFlags(int actor_id, int display_flags);
// decomp: BATTLE.PRG vs_battle_getActorFlags
// (src/BATTLE/BATTLE.PRG/30DB0.c:4184; was func_800A0BE0)
std::uint32_t LiveActorStateFlags(int actor_id);

// decomp: BATTLE.PRG vs_battle_getActorPosition
// (src/BATTLE/BATTLE.PRG/30DB0.c:4299; was func_800A1108)
[[nodiscard]] std::optional<ActorPlacementView> LiveActorPlacementView(
    int actor_id);
// The original call shape: writes the packed tile and the position, with the
// facing in the position's pad; -1 (nothing written) for a slot with neither
// an actor nor a gimmick, else 0. The query overload leaves actorIndex.
int LiveActorPlacement(int actor_id, VS::Actors::ActorLocation* placement_out);
int LiveActorPlacement(int actor_id,
                       VS::Actors::ActorLocationQuery* placement_out);
// decomp: BATTLE.PRG func_800A30A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:274)
// `placement` is the tile, its layer and the facing in sixteenths.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void PlaceLiveActorInRoom(int actor_id, const SpawnPlacement* placement,
                          int drop_height, int movement_class);
// decomp: BATTLE.PRG func_800A3310 (src/BATTLE/BATTLE.PRG/3A1A0.c:350)
// The flyer's slot, or 0 when none is there.
int FindLiveActorAtFlightPosition(int actor_id,
                                  const VS::Core::Gte::SVECTOR* position);
// decomp: BATTLE.PRG func_800A3394 (src/BATTLE/BATTLE.PRG/3A1A0.c:365)
// Raises position->vy above the stacked flyers.
void SeparateLiveActorFlightPlacement(int actor_id,
                                      VS::Core::Gte::SVECTOR* position);
// decomp: BATTLE.PRG func_800A36E0 (src/BATTLE/BATTLE.PRG/3A1A0.c:436)
// Writes only the packed tile, and only for an actor with a record.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveActorSavedPlacement(int actor_id, int movement_class,
                             VS::Actors::ActorLocationQuery* placement_out);
// decomp: BATTLE.PRG func_800A3760 (src/BATTLE/BATTLE.PRG/3A1A0.c:460)
// The nearest actor's slot, or -1.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int FindLiveNearestActorAhead(int actor_id, int horizontal_range,
                              int vertical_range);
}  // namespace VS::Actors
