#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/Templates/ParticleCore.hpp"

// The line particle-set render stage (decomp src/EFFECT/particleLineRender.h:
// 13): draws each ribbon as eight semi-transparent lines between nine
// projected points of its Lagrange curve.
namespace VS::Effects::Templates {
void ParticleLineRender(ParticleSetFrame& frame, ParticleSetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (ParticleLineRender.cpp).
namespace VS::Effects {
namespace Plg065 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg065
namespace Plg087 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg087
namespace Plg114 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg114
namespace Plg146 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg146
namespace Plg147 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg147
namespace Plg168 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg168
namespace Plg224 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg224
namespace Plg255 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg255
namespace Plg265 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg265
namespace Plg266 {
void ParticleLineRender(Templates::ParticleSetFrame& frame,
                        Templates::ParticleSetHost& host);
}  // namespace Plg266
}  // namespace VS::Effects
