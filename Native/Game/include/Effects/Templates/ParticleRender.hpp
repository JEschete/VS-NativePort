#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/Templates/ParticleCore.hpp"

// The textured particle-set render stage (decomp src/EFFECT/particleRender.h:
// 17): draws each ribbon as eight textured quads along nine projected points
// of its Lagrange curve, widened across the screen tangent.
namespace VS::Effects::Templates {
// Draws nothing for a set without a texture animation or when the host has
// no texture frame.
void ParticleRender(ParticleSetFrame& frame, ParticleSetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (ParticleRender.cpp).
namespace VS::Effects {
namespace Plg045 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg045
namespace Plg057 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg057
namespace Plg082 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg082
namespace Plg114 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg114
namespace Plg152 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg152
namespace Plg153 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg153
namespace Plg156 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg156
namespace Plg263 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg263
namespace Plg265 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg265
namespace Plg266 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg266
namespace Plg300 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg300
namespace Plg301 {
void ParticleRender(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg301
}  // namespace VS::Effects
