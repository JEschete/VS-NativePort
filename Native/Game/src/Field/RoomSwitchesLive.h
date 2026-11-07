#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_MpdLockTrigger;
void Field_ToggleRoomSwitchEntry(struct Field_MpdLockTrigger* switch_trigger);
void Field_BeginSwitchToggleEntry(struct Field_MpdLockTrigger* switch_trigger);
#ifdef __cplusplus
}
extern "C++" {
#include "Field/InteractionTarget.hpp"
namespace VS::Field {
bool BeginLiveChestSwitch(ChestTarget target);
bool ToggleLiveChestSwitch(ChestTarget target);
}  // namespace VS::Field
}
#endif
