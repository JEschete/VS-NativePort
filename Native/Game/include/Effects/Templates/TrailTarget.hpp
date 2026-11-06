#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Effects/Templates/TrailCore.hpp"

// The target stage of the five- and nine-sample trails (decomp
// src/EFFECT/trails5.h:243 and trails9.h:243 VS_TRAIL_TARGET_FUNCTION): the
// attraction target every new trail heads for, its last spline point.
namespace VS::Effects::Templates {
// Samples the attraction offset and jitter into the scratch (as signed
// halfwords), moves the offset by the attraction transform's translation or
// the action request's position as the scratch flags choose, and sets every
// trail's last spline point to the offset plus a random value between each
// jitter component and its negation (x, then y, then z).
void TrailTarget(const TrailDefinition& definition, TrailScratch& scratch,
                 std::span<TrailRecord> trails, std::int32_t distance,
                 TrailHost& host);
// TrailTarget over nine-sample trails. Every trails9.h overlay's instance is
// this definition.
void TrailNineTarget(const TrailDefinition& definition, TrailScratch& scratch,
                     std::span<TrailNineRecord> trails, std::int32_t distance,
                     TrailNineHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (TrailTarget.cpp); TrailsLive.hpp binds each overlay's
// set to its renderer slot.
namespace VS::Effects {
namespace Plg057 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg057
namespace Plg073 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg073
namespace Plg086 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg086
namespace Plg087 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg087
namespace Plg128 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg128
namespace Plg134 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg134
namespace Plg137 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg137
namespace Plg141 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg141
namespace Plg146 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg146
namespace Plg148 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg148
namespace Plg242 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg242
namespace Plg255 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg255
namespace Plg265 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg265
namespace Plg266 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg266
namespace Plg291 {
void TrailTarget(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg291
}  // namespace VS::Effects
