#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Effects/Templates/TrailCore.hpp"

// The init stage of the five- and nine-sample trails (decomp
// src/EFFECT/trails5.h:380 and trails9.h:387 VS_TRAIL_INIT_FUNCTION): starts
// every trail at its origin.
namespace VS::Effects::Templates {
// Stores the lifetime plane's start values in the scratch and, for every
// trail, draws a step count between them (zero becomes one; the record keeps
// its low byte), records the origin as the only history sample, sets the
// collapse counter to its full four and the speed to the inner extent's start
// in 1/4096ths of a span, and keeps the first speed step. The curve distance
// is not read.
void TrailInit(const TrailDefinition& definition, TrailScratch& scratch,
               std::span<TrailRecord> trails, std::int32_t distance,
               TrailHost& host);
// TrailInit over nine-sample trails, whose full collapse is eight. Every
// trails9.h overlay's instance is this definition.
void TrailNineInit(const TrailDefinition& definition, TrailScratch& scratch,
                   std::span<TrailNineRecord> trails, std::int32_t distance,
                   TrailNineHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (TrailInit.cpp); TrailsLive.hpp binds each overlay's
// set to its renderer slot.
namespace VS::Effects {
namespace Plg057 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg057
namespace Plg073 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg073
namespace Plg086 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg086
namespace Plg087 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg087
namespace Plg128 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg128
namespace Plg134 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg134
namespace Plg137 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg137
namespace Plg141 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg141
namespace Plg146 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg146
namespace Plg148 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg148
namespace Plg242 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg242
namespace Plg255 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg255
namespace Plg265 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg265
namespace Plg266 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg266
namespace Plg291 {
void TrailInit(const Templates::TrailDefinition& definition,
               Templates::TrailScratch& scratch,
               std::span<Templates::TrailRecord> trails, std::int32_t distance,
               Templates::TrailHost& host);
}  // namespace Plg291
}  // namespace VS::Effects
