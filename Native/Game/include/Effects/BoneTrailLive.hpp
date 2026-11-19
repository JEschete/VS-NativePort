#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Effects {
struct LiveRendererSlot;
}  // namespace VS::Effects
namespace VS::Render {
struct TrailPacket;
}  // namespace VS::Render

// Live binding for bone trails (BoneTrail.hpp): the trail state in a
// renderer slot's game heap block, the actors' bones, the GTE, the module's
// colour tables, the frame's primitive buffer and ordering table, and the
// secondary effect chain. The module pointer table records each entry's
// address; the effect packages reach the trail renderer through
// func_800CE174's address. A missing record is refused with a diagnostic.
namespace VS::Effects {
struct BoneTrail;

// func_800CDCBC (5BF94.c:4898): draws the trail's current segment.
void LiveDrawTrailSegment(const BoneTrail* trail, int base_weight,
                          int tip_weight);
// func_800CE174 (5BF94.c:4958): the trail renderer (EffectRenderer).
int LiveUpdateBoneTrail(LiveRendererSlot* slot, unsigned int command,
                        int parameter);
// func_800CF7A8 (5BF94.c:5666): begins an endless trail behind `bone`.
void LiveSpawnBoneTrail(int actor, int bone, int color, int texture);
// func_800CF830 (5BF94.c:5681): fades every secondary bone trail.
void LiveFadeBoneTrails();
// func_800CF92C (5BF94.c:5714): stores the clamped texel in `uv`.
void LiveClampTrailTexturePoint(int u_offset, int v_offset, int screen,
                                std::uint16_t* uv);
// func_800CF988 (5BF94.c:5738): maps the packet onto the frame on display.
void LiveMapTrailFramebufferTexture(VS::Render::TrailPacket* packet,
                                    int u_offset, int v_offset, int flags);
// func_8006C480 (146C.c:1923): begins a weapon trail behind `bone`.
void LiveStartWeaponTrail(int actor, int bone);
// func_8006C4A4 (146C.c:1925): fades every weapon trail; the original's
// actor and bone arguments selected nothing.
void LiveFadeWeaponTrails();
}  // namespace VS::Effects
