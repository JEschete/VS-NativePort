#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/Templates/ParticleCore.hpp"

// The particle-set origin stage (decomp src/EFFECT/particleSetup.h:36): places
// each ribbon's first control point around the sampled position, on a sphere,
// on a box's faces or corners, or on a ring, optionally turned by the effect
// transform's world rotation.
namespace VS::Effects::Templates {
// The origin stage of one tick. `counter` names the overlay global the box
// corners and the rings step through.
void ParticleOrigin(ParticleEmissionCounter counter, ParticleSetFrame& frame,
                    ParticleSetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (ParticleOrigin.cpp): each binds the template to its
// overlay's emission cursor.
namespace VS::Effects {
namespace Plg045 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg045
namespace Plg057 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg057
namespace Plg065 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg065
namespace Plg082 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg082
namespace Plg087 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg087
namespace Plg114 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
void ParticleOrigin2(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg114
namespace Plg146 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg146
namespace Plg147 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg147
namespace Plg152 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg152
namespace Plg153 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg153
namespace Plg156 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg156
namespace Plg168 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg168
namespace Plg224 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg224
namespace Plg255 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg255
namespace Plg263 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg263
namespace Plg265 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
void ParticleOrigin2(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg265
namespace Plg266 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
void ParticleOrigin2(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg266
namespace Plg300 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg300
namespace Plg301 {
void ParticleOrigin(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg301
}  // namespace VS::Effects
