#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_GimmickSnapshot;
int Field_GimmickPlacement(int gimmick_slot,
                           struct Field_GimmickSnapshot* output_snapshot);
#ifdef __cplusplus
}
#endif
