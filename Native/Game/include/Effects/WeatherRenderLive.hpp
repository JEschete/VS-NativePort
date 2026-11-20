#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/WeatherRender.hpp"

namespace VS::Field {
struct WeatherParticleRecord;
}  // namespace VS::Field

// Live binding for the weather draws (WeatherRender.hpp) over the copied
// weather globals, the room record's weather area, the GTE, the room's
// collision map and floor heights, and the frame's view matrix, ordering table
// and primitive buffer. The entries keep the original call shapes (BATTLE.PRG
// 146C.c:17758-18197) for the room weather and the module pointers. A draw
// over more particles than the buffer holds draws only those it holds; the
// rising embers need the room record (the original read it without a check)
// and are refused outside a room; a packet without an ordering entry (the
// ember flame's depth is unchecked) or buffer room is dropped. Each refusal
// and drop is reported.
namespace VS::Effects {
// func_8008F440 (146C.c:17758).
void LiveDrawRainParticles();
// func_8008F9A4 (146C.c:17864).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveInitializeRisingEmberParticles(int particle_count, int weather_kind);
// func_8008FA14 (146C.c:17878).
void LiveResetRisingEmberParticle(
    VS::Field::WeatherParticleRecord* weather_particle);
// func_8008FAC8 (146C.c:17889).
void LiveDrawRisingEmberParticles();
// func_8008FDC4 (146C.c:17938).
void LiveDrawSnowParticles();
// func_80090434 (146C.c:18072).
void LiveDrawScreenWeatherParticles();
}  // namespace VS::Effects
