#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/Templates/ParticleCore.hpp"

// The particle-set target stage (decomp src/EFFECT/particleSetup.h:262):
// places each ribbon's last control point at the sampled target offset,
// optionally anchored to the effect origin or a transform, plus a jitter.
namespace VS::Effects::Templates {
void ParticleTarget(ParticleSetFrame& frame, ParticleSetHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (ParticleTarget.cpp).
namespace VS::Effects {
namespace Plg045 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg045
namespace Plg057 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg057
namespace Plg065 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg065
namespace Plg082 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg082
namespace Plg087 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg087
namespace Plg114 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
void ParticleTarget2(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg114
namespace Plg146 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg146
namespace Plg147 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg147
namespace Plg152 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg152
namespace Plg153 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg153
namespace Plg156 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg156
namespace Plg168 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg168
namespace Plg224 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg224
namespace Plg255 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg255
namespace Plg263 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg263
namespace Plg265 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
void ParticleTarget2(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg265
namespace Plg266 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
void ParticleTarget2(Templates::ParticleSetFrame& frame,
                     Templates::ParticleSetHost& host);
}  // namespace Plg266
namespace Plg300 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg300
namespace Plg301 {
void ParticleTarget(Templates::ParticleSetFrame& frame,
                    Templates::ParticleSetHost& host);
}  // namespace Plg301
}  // namespace VS::Effects
