// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Sfx.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Heap.h"
#include "Effects/Weather.h"
#include "Field/Background.h"
#include "Field/Doors.h"
#include "Field/Environment.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomRendering.h"
#include "Field/Weather.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Render/Render.h"

// decomp: D_800F1D98 (D_800F1D98@BATTLE/BATTLE.PRG)
short Field_weatherParameters[632];

// decomp: D_800F1DA4 (D_800F1DA4@BATTLE/BATTLE.PRG)
short Field_weatherIntensityScale;

// decomp: D_800F1DBC (D_800F1DBC@BATTLE/BATTLE.PRG)
short Field_weatherAmbientSoundId;

// decomp: D_800F1DC0 (D_800F1DC0@BATTLE/BATTLE.PRG)
short Field_weatherParticleCount;

// decomp: D_800F1DC2 (D_800F1DC2@BATTLE/BATTLE.PRG)
short Field_weatherParticleCapacity;

// decomp: D_800F1DC4 (D_800F1DC4@BATTLE/BATTLE.PRG)
short Field_weatherIntensity;

// decomp: D_800F1DCA (D_800F1DCA@BATTLE/BATTLE.PRG)
char Field_roomWeatherKind;

// decomp: D_800F1DCB (D_800F1DCB@BATTLE/BATTLE.PRG)
char Field_isWeatherVariationEnabled;

// decomp: D_800F1DD4 (D_800F1DD4@BATTLE/BATTLE.PRG)
Field_WeatherParticle* Field_weatherParticles;

// decomp: D_80068EB4 (D_80068EB4@src/BATTLE/BATTLE.PRG/2842C.c)
char const Field_kWeatherVolumeByIntensity[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40, 0x5E,
    0x67, 0x6E, 0x74, 0x79, 0x7C, 0x7E, 0x7F };

// decomp: BATTLE.PRG func_80090C2C (src/BATTLE/BATTLE.PRG/2842C.c:266)
void Field_SetRoomWeather(int arg0)
{
    Field_MpdEnvironment* room = Field_roomData.environment;

    if (room != NULL) {
        int sound = -2;

        Field_weatherParameters[7] = 1;
        Field_weatherParameters[9] = 1;
        Field_weatherParameters[8] = 1;
        Field_weatherParameters[4] = room->windStrength;
        Field_weatherParameters[5] = room->windDirection;
        Field_roomWeatherKind = arg0;
        Field_weatherIntensity = room->weatherIntensity;
        Field_ApplyWeatherIntensity();
        Effects_RecalculateWeatherVelocity();

        switch (arg0) {
        case 0:
            break;
        case 1:
            Effects_InitializeFallingWeatherParticles(Core_Math_ClampSignedShort((room->weatherArea[1].halves[0] / 128)
                                              * (room->weatherArea[1].halves[1] / 128) / 4,
                              4, 128),
                arg0);
            sound = 21;
            break;
        case 2:
            Effects_InitializeFallingWeatherParticles(Core_Math_ClampSignedShort((room->weatherArea[1].halves[0] / 128)
                                              * (room->weatherArea[1].halves[1] / 128) / 4,
                              4, 128),
                arg0);
            break;
        case 3:
            Effects_InitializeScreenWeatherParticles(Core_Math_ClampSignedShort((room->weatherArea[1].halves[0] / 128)
                                              * (room->weatherArea[1].halves[1] / 128) / 4,
                              4, 128),
                arg0);
            break;
        case 4:
            Effects_InitializeRisingEmberParticles(Core_Math_ClampSignedShort((room->weatherArea[1].halves[0] / 128)
                                              * (room->weatherArea[1].halves[1] / 128) / 2,
                              4, 128),
                arg0);
            break;
        }

        if (Field_weatherAmbientSoundId != -1) {
            if ((Field_weatherAmbientSoundId >= 0) && (Field_weatherAmbientSoundId != sound)) {
                Audio_StopSfx(0x200, Field_weatherAmbientSoundId);
            }
            if (sound >= 0) {
                Audio_PlaySfxCentered(0x200, sound);
            }

            Field_weatherAmbientSoundId = sound;
        }
    } else if (Field_weatherAmbientSoundId >= 0) {
        Audio_StopSfx(0x200, Field_weatherAmbientSoundId);
        Field_weatherAmbientSoundId = -2;
    }
}

// decomp: BATTLE.PRG func_80090EEC (src/BATTLE/BATTLE.PRG/2842C.c:329)
void Field_UpdateRoomWeather(void)
{
    int masks[] = { 0xF, 0x1F, 0x3F, 0x7F };
    Field_MpdEnvironment* room = Field_roomData.environment;

    if ((room == NULL) || (Field_roomWeatherKind == 0)) {
        return;
    }

    if ((Field_areRoomUpdatesPaused == 0) && (Field_isWeatherVariationEnabled != 0)) {
        int changed = 0;

        if ((Field_roomAnimationTicks & masks[room->windDirectionInterval]) == 0) {
            if ((Core_Random_Rand() & 7) < 3) {
                Field_weatherParameters[7] *= -1;
            }

            Field_weatherParameters[5] += Field_weatherParameters[7];
            Field_weatherParameters[5] &= 0xFFF;
            Field_weatherParameters[5] = Core_Math_ClampSignedShort(Field_weatherParameters[5], room->windDirection, room->windDirectionLimit);
            changed = 1;
        }
        if (((Field_roomAnimationTicks + 30) & masks[room->windStrengthInterval]) == 0) {
            if ((Core_Random_Rand() & 7) < 3) {
                Field_weatherParameters[9] *= -1;
            }

            Field_weatherParameters[4] += Field_weatherParameters[9];
            Field_weatherParameters[4] = Core_Math_ClampSignedShort(Field_weatherParameters[4], room->windStrength, room->windStrengthLimit);
            changed = 1;
        }
        if (((Field_roomAnimationTicks + 60) & masks[room->weatherIntensityInterval]) == 0) {
            if ((Core_Random_Rand() & 7) < 3) {
                Field_weatherParameters[8] *= -1;
            }

            Field_weatherIntensity += Field_weatherParameters[8];
            Field_weatherIntensity = Core_Math_ClampSignedShort(Field_weatherIntensity, room->weatherIntensity, room->weatherIntensityLimit);
            Field_ApplyWeatherIntensity();
            changed = 1;
        }
        if (changed != 0) {
            Effects_RecalculateWeatherVelocity();
        }
    }
    switch (Field_roomWeatherKind) {
    case 1:
        Effects_DrawRainParticles();
        break;
    case 2:
        Effects_DrawSnowParticles();
        break;
    case 3:
        Effects_DrawScreenWeatherParticles();
        break;
    case 4:
        Effects_DrawRisingEmberParticles();
        break;
    }
}

// decomp: BATTLE.PRG func_8009121C (src/BATTLE/BATTLE.PRG/2842C.c:411)
void Field_ApplyWeatherIntensity(void)
{
    short scale;

    Field_weatherParticleCount = Core_Math_ClampSignedShort(Field_weatherIntensity * 8, 4, Field_weatherParticleCapacity);
    if (Field_weatherAmbientSoundId >= 0) {
        if (Field_kWeatherVolumeByIntensity[Field_weatherIntensity] != 0) {
            Audio_PlaySfxCentered(0x200, Field_weatherAmbientSoundId);
            Audio_FadeSfxVolume(0x200, Field_weatherAmbientSoundId, Field_kWeatherVolumeByIntensity[Field_weatherIntensity], 8);
        } else {
            Audio_StopSfx(0x200, Field_weatherAmbientSoundId);
        }
    }
    scale = Core_Math_ClampSignedShort((Field_weatherIntensity * 4) + (Field_weatherIntensity * 2), 12, 90);
    Field_weatherIntensityScale = scale + (scale >> 1) + (scale >> 2);
}

// decomp: BATTLE.PRG func_80091314 (src/BATTLE/BATTLE.PRG/2842C.c:428)
void Field_SetWeatherVariationEnabled(int arg0) { Field_isWeatherVariationEnabled = arg0; }

// decomp: BATTLE.PRG func_80091320 (src/BATTLE/BATTLE.PRG/2842C.c:430)
void Field_SetWeatherIntensity(int arg0)
{
    Field_weatherIntensity = arg0;
    Field_ApplyWeatherIntensity();
    Effects_RecalculateWeatherVelocity();
}

// decomp: BATTLE.PRG func_8009134C (src/BATTLE/BATTLE.PRG/2842C.c:437)
void Field_SetWeatherWindAngles(int arg0, int arg1)
{
    Field_weatherParameters[5] = arg0;
    Field_weatherParameters[4] = arg1;
    Effects_RecalculateWeatherVelocity();
}

// decomp: BATTLE.PRG func_80091378 (src/BATTLE/BATTLE.PRG/2842C.c:444)
void Field_ReleaseWeatherParticles(void)
{
    Field_weatherParticleCount = 0;
    Field_weatherParticleCapacity = 0;
    if ((*(void* (*))&Field_weatherParticles) != NULL) {
        Core_Memory_FreeFromLowEnd((*(void* (*))&Field_weatherParticles));
    }
    (*(void* (*))&Field_weatherParticles) = NULL;
}

// decomp: BATTLE.PRG func_800913BC (src/BATTLE/BATTLE.PRG/2842C.c:454)
int Field_SetWeatherAmbientSound(int arg0)
{
    int temp_s3;

    temp_s3 = Field_weatherAmbientSoundId;
    Field_weatherAmbientSoundId = arg0;

    if (arg0 >= 0) {
        if (Field_kWeatherVolumeByIntensity[Field_weatherIntensity] != 0) {
            Audio_PlaySfxCentered(0x200, (short)arg0);
            Audio_FadeSfxVolume(0x200, Field_weatherAmbientSoundId, Field_kWeatherVolumeByIntensity[Field_weatherIntensity], 8);
        } else {
            Audio_StopSfx(0x200, (short)arg0);
        }
    }
    return temp_s3;
}

// decomp: BATTLE.PRG func_80091468 (src/BATTLE/BATTLE.PRG/2842C.c:472)
void Field_SetRoomBackgroundOffset(int arg0, int arg1)
{
    Field_backgroundOffsetX = arg0;
    Field_backgroundOffsetY = arg1;
}

// decomp: BATTLE.PRG func_8009147C (src/BATTLE/BATTLE.PRG/2842C.c:478)
void Field_DrawRoomBackgroundTiles(Field_BackgroundLayout* arg0, int arg1, int arg2, int arg3)
{
    void* buffer = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
    Field_BackgroundTile* tiles = arg0->tiles;
    int cosStep = Core_Gte_Cos(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vz) * 32;
    int sinStep = Core_Gte_Sin(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vz) * 32;
    int row;
    int col;
    TILE* background;

    for (row = 0; row < arg0->height * 32; row += 32) {
        int y = row - arg2;
        POLY_GT4* prim = buffer;

        for (col = 0; col < 1024; col += 32) {
            int x = ((col + arg1) & 1023) - 512;
            int screenX = Core_Gte_Cos(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vz) * x
                        - Core_Gte_Sin(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vz) * y;
            int screenY = Core_Gte_Sin(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vz) * x
                        + Core_Gte_Cos(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vz) * y;
            Field_BackgroundTile* tile;

            if ((screenX < -64 * ONE) || (screenX > 384 * ONE) || (screenY < -64 * ONE)
                || (screenY > 304 * ONE)) {
                continue;
            }

            tile = &tiles[(row / 32) * arg0->width + (col / 32) % arg0->width];

            if ((tile->textureRect.w == 0) && (tile->textureRect.h == 0)) {
                continue;
            }

            setPolyGT4(prim);
            setXY4(prim, screenX / ONE, screenY / ONE, (screenX + cosStep) / ONE,
                (screenY + sinStep) / ONE, (screenX - sinStep) / ONE,
                (screenY + cosStep) / ONE, (screenX + cosStep - sinStep) / ONE,
                (screenY + sinStep + cosStep) / ONE);
            setRGB0(prim, tile->cornerColors[0].r, tile->cornerColors[0].g, tile->cornerColors[0].b);
            setRGB1(prim, tile->cornerColors[1].r, tile->cornerColors[1].g, tile->cornerColors[1].b);
            setRGB2(prim, tile->cornerColors[2].r, tile->cornerColors[2].g, tile->cornerColors[2].b);
            setRGB3(prim, tile->cornerColors[3].r, tile->cornerColors[3].g, tile->cornerColors[3].b);
            setUV4(prim, tile->textureRect.x, tile->textureRect.y, tile->textureRect.x + tile->textureRect.w, tile->textureRect.y,
                tile->textureRect.x, tile->textureRect.y + tile->textureRect.h, tile->textureRect.x + tile->textureRect.w,
                tile->textureRect.y + tile->textureRect.h);
            prim->tpage = arg3 | (tile->cornerColors[1].cd << 7);
            prim->clut = getClut(768, 238);
            buffer += sizeof(POLY_GT4);
            AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 0x1FFC, prim++);
        }
    }

    background = buffer;
    setTile(background);
    setRGB0(background, 0, 0, 0);
    setXY0(background, 0, 0);
    setWH(background, 320, 240);
    AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 0x1FFC, background);
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = background + 1;
}

// decomp: BATTLE.PRG func_800918E8 (src/BATTLE/BATTLE.PRG/2842C.c:539)
void Field_DuckWeatherAmbientSound(int arg0)
{
    if (Field_weatherAmbientSoundId >= 0) {
        if (arg0 != 0) {
            Audio_FadeSfxVolume(0x200, Field_weatherAmbientSoundId, Field_kWeatherVolumeByIntensity[Field_weatherIntensity] >> 2, 8);
        } else {
            Audio_FadeSfxVolume(0x200, Field_weatherAmbientSoundId, Field_kWeatherVolumeByIntensity[Field_weatherIntensity], 8);
        }
    }
}

