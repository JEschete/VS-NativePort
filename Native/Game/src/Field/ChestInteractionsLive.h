#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_MpdLockTrigger;
struct Actors_ActorLocation;
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
#include "Field/InteractionTarget.hpp"
namespace VS::Field {
bool TryOpenLiveChest(ChestTarget target);
bool BeginLiveChestInteraction(ChestTarget target, std::int32_t actor);
}  // namespace VS::Field
}
#endif
