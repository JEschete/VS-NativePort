#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Effects/Templates/TrailCore.hpp"

// The render stage of the five- and nine-sample trails (decomp
// src/EFFECT/trails5.h:449 and trails9.h:456 VS_TRAIL_RENDER_FUNCTION): draws
// every trail as a ribbon of textured, semi-transparent quads through its
// history samples, newest first.
namespace VS::Effects::Templates {
// Samples the acceleration vector into the scratch (its x, halved, is the
// ribbon's half width) and the colour curves (each doubled and saturated at
// 255; a zero curve gives 128), loads the projection and steps the texture
// animation. A trail with two or more history samples projects them and
// turns each segment's screen angle a quarter turn to its normal; every
// sample then becomes an edge either side of the ribbon at the mean normal of
// its segments (the first and last at their segment's own). The edge `j`
// samples behind the newest uses step collapse - j and the oldest edge step
// 0: it fades by that transparency sample and moves u by that many quarters
// of the texture's width. A quad is drawn between
// neighbouring edges when its mean depth lies in front of the far depth and
// beyond the near clip. A trail whose history counters would have read or
// written outside the original's five-entry arrays is reported and left
// undrawn.
void TrailRender(const TrailDefinition& definition, TrailScratch& scratch,
                 std::span<TrailRecord> trails, std::int32_t distance,
                 TrailHost& host);
// TrailRender over nine-sample trails: u moves by eighths of the texture's
// width, the edges fade by the nine transparency samples, and the fault
// checks bound the original's nine-entry arrays. Every trails9.h overlay's
// instance is this definition.
void TrailNineRender(const TrailDefinition& definition, TrailScratch& scratch,
                     std::span<TrailNineRecord> trails, std::int32_t distance,
                     TrailNineHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (TrailRender.cpp); TrailsLive.hpp binds each overlay's
// set to its renderer slot.
namespace VS::Effects {
namespace Plg057 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg057
namespace Plg073 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg073
namespace Plg086 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg086
namespace Plg087 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg087
namespace Plg128 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg128
namespace Plg134 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg134
namespace Plg137 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg137
namespace Plg141 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg141
namespace Plg146 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg146
namespace Plg148 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg148
namespace Plg242 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg242
namespace Plg255 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg255
namespace Plg265 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg265
namespace Plg266 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg266
namespace Plg291 {
void TrailRender(const Templates::TrailDefinition& definition,
                 Templates::TrailScratch& scratch,
                 std::span<Templates::TrailRecord> trails,
                 std::int32_t distance, Templates::TrailHost& host);
}  // namespace Plg291
}  // namespace VS::Effects
