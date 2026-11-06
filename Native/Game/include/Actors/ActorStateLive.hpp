#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Actors/ActorPlacement.hpp"

struct Actors_ActorLocation;
struct Actors_ActorLocationQuery;
struct Battle_Actor;
struct Field_ActorPlacement;
struct Render_ColorCode;
struct SVECTOR;

// Binding from the actor table, the battle actors, the slot gimmicks and the
// weapon models to the typed actor state rules (ActorFlags.hpp,
// BattleActorState.hpp, ActorLifecycle.hpp and ActorPlacement.hpp). Each
// adapter keeps the original argument order for the generated callers
// (copy_fixups.py OWNED_CALL_BINDINGS). An id outside its table is refused
// with a warning on the "actors" channel and does nothing (or reports
// nothing), where the originals read past the table.
namespace VS::Actors {
// decomp: BATTLE.PRG func_8007BF6C (src/BATTLE/BATTLE.PRG/146C.c:8623)
void EnableLiveActorModel(int actor_id);
// decomp: BATTLE.PRG func_8007BFC0 (src/BATTLE/BATTLE.PRG/146C.c:8630)
void EnableAllLiveActorModels();
// decomp: BATTLE.PRG func_8007BFF8 (src/BATTLE/BATTLE.PRG/146C.c:8639)
void DisableLiveActorModel(int actor_id);
// decomp: BATTLE.PRG func_8007C050 (src/BATTLE/BATTLE.PRG/146C.c:8646)
void DisableAllLiveActorModels();
// decomp: BATTLE.PRG func_8007C1D8 (src/BATTLE/BATTLE.PRG/146C.c:8686)
void SetLiveActorHostile(int actor_id, int hostile);
// decomp: BATTLE.PRG func_8007C218 (src/BATTLE/BATTLE.PRG/146C.c:8697)
// 0 for an empty slot, which the original read through.
int LiveActorHpPercentage(int actor_id);
// decomp: BATTLE.PRG func_8007C28C (src/BATTLE/BATTLE.PRG/146C.c:8714)
void RestoreLiveActorVitals(int actor_id);
// decomp: BATTLE.PRG func_8007CF18 (src/BATTLE/BATTLE.PRG/146C.c:9127)
int FindLiveActorByModelId(int model_id);
// decomp: BATTLE.PRG func_8007CF64 (src/BATTLE/BATTLE.PRG/146C.c:9139)
// Null for an empty slot or an id outside Battle_actors.
Battle_Actor* LiveActorAtSlot(int actor_id);
// decomp: BATTLE.PRG func_8007CF80 (src/BATTLE/BATTLE.PRG/146C.c:9141)
int CountLiveActorsByDisposition(int actor_kind);
// decomp: BATTLE.PRG func_8007CFCC (src/BATTLE/BATTLE.PRG/146C.c:9155)
char IsLiveActorRetainedAfterDefeat(int actor_id);
// decomp: BATTLE.PRG func_8007D08C (src/BATTLE/BATTLE.PRG/146C.c:9179)
// Returns `enabled`, as the original did.
int SetLiveActorLogicEnabled(int actor_id, int enabled);
// decomp: BATTLE.PRG func_8007D15C (src/BATTLE/BATTLE.PRG/146C.c:9199)
int SetOtherLiveActorLogicEnabled(int enabled);
// decomp: BATTLE.PRG func_8007D1A8 (src/BATTLE/BATTLE.PRG/146C.c:9209)
void SetLiveActorDetached(int actor_id, int detached);

// decomp: BATTLE.PRG func_8009D468 (src/BATTLE/BATTLE.PRG/30DB0.c:2073)
// `unused` is the original's unread second argument.
void ResetLiveActorMotionState(int actor_id, int unused);
// decomp: BATTLE.PRG func_8009D6F4 (src/BATTLE/BATTLE.PRG/30DB0.c:2147)
void ResetAllLiveActorMovement();
// decomp: BATTLE.PRG func_8009F314 (src/BATTLE/BATTLE.PRG/30DB0.c:3202)
// `context` is only passed down the original's companion recursion.
void BeginLiveActorDeath(int actor_id, void* context, int animate);
// decomp: BATTLE.PRG func_8009F940 (src/BATTLE/BATTLE.PRG/30DB0.c:3388)
// `companion_state` holds the four Actors_CompanionOffset bytes; a nonzero
// slot without both arguments keeps the actor's current style, and a slot
// outside a byte is refused.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveActorCompanion(int actor_id, int companion_slot,
                           const Render_ColorCode* companion_state,
                           const Render_ColorCode* companion_color);
// decomp: BATTLE.PRG func_8009FF08 (src/BATTLE/BATTLE.PRG/30DB0.c:3613)
// A parent of -1 keeps the current one; `unused` is never read.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void AttachLiveActorToParentBone(int actor_id, int parent_actor_id,
                                 int parent_bone_index, int unused);
// decomp: BATTLE.PRG func_8009FFA8 (src/BATTLE/BATTLE.PRG/30DB0.c:3635)
// AttachmentRelease as an int: 0 released, -1 parent not ready, -2 no parent.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int ReleaseLiveActorAttachmentLock(int actor_id, int unused);
// decomp: BATTLE.PRG func_800A087C (src/BATTLE/BATTLE.PRG/30DB0.c:3950)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveActorDisplayFlags(int actor_id, int display_flags);
// decomp: BATTLE.PRG func_800A0BE0 (src/BATTLE/BATTLE.PRG/30DB0.c:4065)
std::uint32_t LiveActorStateFlags(int actor_id);

// decomp: BATTLE.PRG func_800A1108 (src/BATTLE/BATTLE.PRG/30DB0.c:4182)
[[nodiscard]] std::optional<ActorPlacementView> LiveActorPlacementView(
    int actor_id);
// The original call shape: writes the packed tile and the position, with the
// facing in the position's pad; -1 (nothing written) for a slot with neither
// an actor nor a gimmick, else 0. The query overload leaves actorIndex.
int LiveActorPlacement(int actor_id, Actors_ActorLocation* placement_out);
int LiveActorPlacement(int actor_id, Actors_ActorLocationQuery* placement_out);
// decomp: BATTLE.PRG func_800A30A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:274)
// `placement` is the tile, its layer and the facing in sixteenths.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void PlaceLiveActorInRoom(int actor_id, const Field_ActorPlacement* placement,
                          int drop_height, int movement_class);
// decomp: BATTLE.PRG func_800A3310 (src/BATTLE/BATTLE.PRG/3A1A0.c:350)
// The flyer's slot, or 0 when none is there.
int FindLiveActorAtFlightPosition(int actor_id, const SVECTOR* position);
// decomp: BATTLE.PRG func_800A3394 (src/BATTLE/BATTLE.PRG/3A1A0.c:365)
// Raises position->vy above the stacked flyers.
void SeparateLiveActorFlightPlacement(int actor_id, SVECTOR* position);
// decomp: BATTLE.PRG func_800A36E0 (src/BATTLE/BATTLE.PRG/3A1A0.c:448)
// Writes only the packed tile, and only for an actor with a record.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveActorSavedPlacement(int actor_id, int movement_class,
                             Actors_ActorLocationQuery* placement_out);
// decomp: BATTLE.PRG func_800A3760 (src/BATTLE/BATTLE.PRG/3A1A0.c:460)
// The nearest actor's slot, or -1.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int FindLiveNearestActorAhead(int actor_id, int horizontal_range,
                              int vertical_range);
}  // namespace VS::Actors
