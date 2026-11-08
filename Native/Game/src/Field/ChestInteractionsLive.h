#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_MpdLockTrigger;
struct Actors_ActorLocation;
struct Actors_ActorLocationQuery;
struct Effects_ActionEffectRequest;
void Field_TriggerActorTrapEntry(struct Field_MpdLockTrigger* address,
                                 struct Actors_ActorLocationQuery* location,
                                 int actor);
void Field_BeginObjectInteractionWaitEntry(void);
void Field_EndObjectInteractionWaitEntry(void);
int Field_InteractWithLockedRoomObjectEntry(
    struct Field_MpdLockTrigger* address);
void Field_TryOpenChestEntry(struct Field_MpdLockTrigger* address, int actor);
void Field_BeginChestInteractionEntry(struct Field_MpdLockTrigger* address,
                                      int actor);
void Field_FinishChestInteractionEntry(void);
void Field_OpenChestContentsEntry(void);
int Field_UpdateChestContentsSelectionEntry(void);
void Field_DispatchTrapOrChestInteractionEntry(void);
void Field_FinishTrapInteractionEntry(void);
void Field_BeginTrapInteractionEntry(struct Field_MpdLockTrigger* address,
                                     struct Actors_ActorLocation* location,
                                     int actor);
#ifdef __cplusplus
}
extern "C++" {
#include "Field/ChestInteractions.hpp"
#include "Field/InteractionTarget.hpp"
namespace VS::Field {
bool TryOpenLiveChest(ChestTarget target);
bool BeginLiveChestInteraction(ChestTarget target, std::int32_t actor);
bool TriggerLiveActorTrap(TrapTarget target, const TrapPosition& point,
                          std::int32_t actor);
Effects_ActionEffectRequest LiveTrapEffectRecord(
    const TrapEffectRequest& effect);
}  // namespace VS::Field
}
#endif
