#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Effects/Templates/TrailCore.hpp"

// The control stage of the five- and nine-sample trails (decomp
// src/EFFECT/trails5.h:278 and trails9.h:278 VS_TRAIL_CONTROL_FUNCTION): the
// two inner spline points that bend each trail between its origin and its
// target.
namespace VS::Effects::Templates {
// For every trail, samples the mass, gravity-scale and speed planes and the
// direction, direction-jitter and force vectors into the scratch, then places
// spline points 1 and 2 in the frame that looks from point 0 to point 3: each
// sits on a circle about that axis (radius from the mass or gravity-scale
// plane plus a random share of its spread, angle from the direction plus a
// random jitter) at a speed-plane share of the spline's length along it,
// spread by the force. The random draws come in a fixed order: the first and
// second length spreads, the two radii, then the two angles, each only when
// its spread is nonzero. The scratch keeps the look-at matrix and the two
// transformed points.
void TrailControl(const TrailDefinition& definition, TrailScratch& scratch,
                  std::span<TrailRecord> trails, std::int32_t distance,
                  TrailHost& host);
// TrailControl over nine-sample trails. Every trails9.h overlay's instance is
// this definition.
void TrailNineControl(const TrailDefinition& definition, TrailScratch& scratch,
                      std::span<TrailNineRecord> trails, std::int32_t distance,
                      TrailNineHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (TrailControl.cpp); TrailsLive.hpp binds each
// overlay's set to its renderer slot.
namespace VS::Effects {
namespace Plg057 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg057
namespace Plg073 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg073
namespace Plg086 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg086
namespace Plg087 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg087
namespace Plg128 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg128
namespace Plg134 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg134
namespace Plg137 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg137
namespace Plg141 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg141
namespace Plg146 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg146
namespace Plg148 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg148
namespace Plg242 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg242
namespace Plg255 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg255
namespace Plg265 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg265
namespace Plg266 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg266
namespace Plg291 {
void TrailControl(const Templates::TrailDefinition& definition,
                  Templates::TrailScratch& scratch,
                  std::span<Templates::TrailRecord> trails,
                  std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg291
}  // namespace VS::Effects
