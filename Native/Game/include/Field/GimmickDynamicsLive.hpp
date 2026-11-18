#pragma once

struct Field_GimmickInstance;
struct Field_GimmickPhysicsView;

namespace VS::Field {
bool UpdateLiveGimmickMotion(Field_GimmickInstance* record);
// The module image entry (BATTLE.PRG 0x800B0908) that the copied gimmick
// update calls.
void UpdateGimmickMotionEntry(Field_GimmickPhysicsView* record, int unused);
}  // namespace VS::Field
