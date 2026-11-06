#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Effects/Templates/ParticleControl.hpp"
#include "Effects/Templates/ParticleCore.hpp"
#include "Effects/Templates/ParticleLineRender.hpp"
#include "Effects/Templates/ParticleOrigin.hpp"
#include "Effects/Templates/ParticleRender.hpp"
#include "Effects/Templates/ParticleTarget.hpp"

// The particle-set dispatcher (decomp src/EFFECT/particleDispatch.h:3): the
// renderer-slot commands of a set of up to eight ribbons. A tick samples the
// ribbon count, reruns the origin and target stages whenever the cooldown
// runs out, then runs the control and render stages.
namespace VS::Effects::Templates {
// One renderer command; false releases the slot.
bool ParticleDispatch(const ParticleDispatchOptions& options,
                      ParticleCommand command, std::int32_t packed,
                      ParticleSetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (ParticleDispatch.cpp) and the stages each binds, as
// the overlays' particleDispatch.h bindings name them (ParticlesLive.hpp binds
// the renderer slots).
namespace VS::Effects {
namespace Plg045 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg045
namespace Plg057 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg057
namespace Plg065 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg065
namespace Plg082 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg082
namespace Plg087 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg087
namespace Plg114 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
bool ParticleDispatch2(Templates::ParticleCommand command, std::int32_t packed,
                       Templates::ParticleSetHost& host);
}  // namespace Plg114
namespace Plg146 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg146
namespace Plg147 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg147
namespace Plg152 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg152
namespace Plg153 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg153
namespace Plg156 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg156
namespace Plg168 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg168
namespace Plg224 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg224
namespace Plg255 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg255
namespace Plg263 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg263
namespace Plg265 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
bool ParticleDispatch2(Templates::ParticleCommand command, std::int32_t packed,
                       Templates::ParticleSetHost& host);
}  // namespace Plg265
namespace Plg266 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
bool ParticleDispatch2(Templates::ParticleCommand command, std::int32_t packed,
                       Templates::ParticleSetHost& host);
}  // namespace Plg266
namespace Plg300 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg300
namespace Plg301 {
bool ParticleDispatch(Templates::ParticleCommand command, std::int32_t packed,
                      Templates::ParticleSetHost& host);
}  // namespace Plg301
namespace Plg045 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg045
namespace Plg057 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg057
namespace Plg065 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg065
namespace Plg082 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg082
namespace Plg087 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg087
namespace Plg114 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg114
namespace Plg146 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg146
namespace Plg147 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg147
namespace Plg152 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg152
namespace Plg153 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg153
namespace Plg156 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg156
namespace Plg168 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg168
namespace Plg224 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg224
namespace Plg255 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg255
namespace Plg263 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg263
namespace Plg265 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg265
namespace Plg266 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg266
namespace Plg300 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg300
namespace Plg301 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatchOptions{
    .origin = &ParticleOrigin,
    .target = &ParticleTarget,
    .control = &ParticleControl,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg301

// The second particle sets of the overlays that run two.
namespace Plg114 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatch2Options{
    .origin = &ParticleOrigin2,
    .target = &ParticleTarget2,
    .control = &ParticleControl2,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg114
namespace Plg265 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatch2Options{
    .origin = &ParticleOrigin2,
    .target = &ParticleTarget2,
    .control = &ParticleControl2,
    .render = &ParticleRender,
    .texturing = Templates::ParticleTexturing::kTextured,
};
}  // namespace Plg265
namespace Plg266 {
inline constexpr Templates::ParticleDispatchOptions kParticleDispatch2Options{
    .origin = &ParticleOrigin2,
    .target = &ParticleTarget2,
    .control = &ParticleControl2,
    .render = &ParticleLineRender,
    .texturing = Templates::ParticleTexturing::kUntextured,
};
}  // namespace Plg266
}  // namespace VS::Effects
