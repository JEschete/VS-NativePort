#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Effects/Templates/TrailCore.hpp"

// The update stage of the five- and nine-sample trails (decomp
// src/EFFECT/trails5.h:407 and trails9.h:414 VS_TRAIL_UPDATE_FUNCTION): moves
// every live trail one step along its spline.
namespace VS::Effects::Templates {
// Skips trails whose collapse counter is zero. Once the curve distance
// reaches a trail's step count it counts the collapse down, and drops the
// oldest history sample whenever the counter falls below the step count (the
// sample count wraps as the original's byte did). Before that it adds the
// speed step to the speed and the speed to the progress, and records the
// spline's Lagrange sample at that progress as the newest of up to five
// history entries. Only the trails and the curve distance are read.
void TrailUpdate(const TrailDefinition& definition, TrailScratch& scratch,
                 std::span<TrailRecord> trails, std::int32_t distance,
                 TrailHost& host);
// TrailUpdate over a history of up to nine entries. Every trails9.h overlay's
// instance is this definition.
void TrailNineUpdate(const TrailDefinition& definition, TrailScratch& scratch,
                     std::span<TrailNineRecord> trails, std::int32_t distance,
                     TrailNineHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (TrailUpdate.cpp); TrailsLive.hpp binds each overlay's
// set to its renderer slot.
namespace VS::Effects {
namespace Plg057 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg057
namespace Plg073 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg073
namespace Plg086 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg086
namespace Plg087 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg087
namespace Plg128 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg128
namespace Plg134 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg134
namespace Plg137 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg137
namespace Plg141 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg141
namespace Plg146 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg146
namespace Plg148 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg148
namespace Plg242 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg242
namespace Plg255 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg255
namespace Plg265 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg265
namespace Plg266 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg266
namespace Plg291 {
void TrailUpdate(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg291
}  // namespace VS::Effects
