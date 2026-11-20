#pragma once

namespace VS::Field {
struct GimmickInstance;
struct GimmickPhysicsView;
}  // namespace VS::Field

namespace VS::Field {
bool UpdateLiveGimmickMotion(VS::Field::GimmickInstance* record);
// The module image entry (BATTLE.PRG 0x800B0908) that the copied gimmick
// update calls.
void UpdateGimmickMotionEntry(VS::Field::GimmickPhysicsView* record,
                              int unused);
}  // namespace VS::Field
