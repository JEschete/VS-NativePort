#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/Templates/ParticleCore.hpp"

// The particle-set control stage (decomp src/EFFECT/particleSetup.h:298):
// places each ribbon's two inner control points in the frame looking from its
// first point to its last, at the sampled radii, angles and speeds.
namespace VS::Effects::Templates {
void ParticleControl(ParticleSetFrame& frame, ParticleSetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (ParticleControl.cpp).
namespace VS::Effects {
namespace Plg045 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg045
namespace Plg057 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg057
namespace Plg065 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg065
namespace Plg082 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg082
namespace Plg087 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg087
namespace Plg114 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
void ParticleControl2(Templates::ParticleSetFrame& frame,
                      Templates::ParticleSetHost& host);
}  // namespace Plg114
namespace Plg146 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg146
namespace Plg147 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg147
namespace Plg152 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg152
namespace Plg153 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg153
namespace Plg156 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg156
namespace Plg168 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg168
namespace Plg224 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg224
namespace Plg255 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg255
namespace Plg263 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg263
namespace Plg265 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
void ParticleControl2(Templates::ParticleSetFrame& frame,
                      Templates::ParticleSetHost& host);
}  // namespace Plg265
namespace Plg266 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
void ParticleControl2(Templates::ParticleSetFrame& frame,
                      Templates::ParticleSetHost& host);
}  // namespace Plg266
namespace Plg300 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg300
namespace Plg301 {
void ParticleControl(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg301
}  // namespace VS::Effects
