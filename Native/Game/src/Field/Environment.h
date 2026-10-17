// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ENVIRONMENT_H_TYPES
#define VS_FIELD_ENVIRONMENT_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_ENVIRONMENT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_ENVIRONMENT_H_DECLS)
#define VS_FIELD_ENVIRONMENT_H_DECLS
#include "Field/Background.h"
#include "Field/Weather.h"

extern short Field_weatherParameters[632];
extern short Field_weatherIntensityScale;
extern short Field_weatherAmbientSoundId;
extern short Field_weatherParticleCount;
extern short Field_weatherParticleCapacity;
extern short Field_weatherIntensity;
extern char Field_roomWeatherKind;
extern char Field_isWeatherVariationEnabled;
extern Field_WeatherParticle* Field_weatherParticles;
extern char const Field_kWeatherVolumeByIntensity[];
void Field_SetRoomWeather(int arg0);
void Field_UpdateRoomWeather(void);
void Field_ApplyWeatherIntensity(void);
void Field_SetWeatherVariationEnabled(int arg0);
void Field_SetWeatherIntensity(int arg0);
void Field_SetWeatherWindAngles(int arg0, int arg1);
void Field_ReleaseWeatherParticles(void);
int Field_SetWeatherAmbientSound(int arg0);
void Field_SetRoomBackgroundOffset(int arg0, int arg1);
void Field_DrawRoomBackgroundTiles(Field_BackgroundLayout* arg0, int arg1, int arg2, int arg3);
void Field_DuckWeatherAmbientSound(int arg0);

#endif // VS_FIELD_ENVIRONMENT_H_DECLS
