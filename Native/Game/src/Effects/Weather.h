// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_WEATHER_H_TYPES
#define VS_EFFECTS_WEATHER_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/insertTPage.h:3
#define _insertTPage(after, arg1)                                                              \
    do {                                                                                        \
        u_long* ot_ = (u_long*)Render_frameScratch.orderingTable + (after);                    \
        u_long* p_ = (u_long*)Render_frameScratch.primitiveCursor;                             \
        p_[1] = 0xE1000200 | ((arg1) & 0x1FF);                                                 \
        p_[2] = 0;                                                                             \
        p_[0] = 0x02000000 | (*ot_ & 0xFFFFFF);                                                \
        *ot_ = (*ot_ & 0xFF000000) | ((u_long)(uintptr_t)p_ & 0xFFFFFF);                      \
        Render_frameScratch.primitiveCursor = p_ + 3;                                          \
    } while (0)

#endif // VS_EFFECTS_WEATHER_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_WEATHER_H_DECLS)
#define VS_EFFECTS_WEATHER_H_DECLS
#include "Field/Weather.h"

void Effects_AllocateWeatherParticles(void);
int Effects_InitializeRoomWeather(int arg0);
void Effects_RoomWeatherState(int* arg0, int* arg1);
void Effects_InitializeFallingWeatherParticles(int arg0, int arg1);
void Effects_InitializeScreenWeatherParticles(int arg0, int arg1);
void Effects_DrawRainParticles(void);
void Effects_InitializeRisingEmberParticles(int arg0, int arg1);
void Effects_ResetRisingEmberParticle(Field_WeatherParticle* arg0);
void Effects_DrawRisingEmberParticles(void);
void Effects_DrawSnowParticles(void);
void Effects_DrawScreenWeatherParticles(void);
void Effects_ResetFallingWeatherParticle(Field_WeatherParticle* arg0);
void Effects_RecalculateWeatherVelocity(void);

#endif // VS_EFFECTS_WEATHER_H_DECLS
