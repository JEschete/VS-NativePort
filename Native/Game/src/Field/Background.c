// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "CdDrive.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
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
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/RoomGeometry.h"
#include "Render/Textures.h"

// decomp: D_80068C1C (D_80068C1C@BATTLE/BATTLE.PRG)
int Field_legacyBackgroundGradientPackets[1344]; /* rodata: contents loaded in Step 5 */

// decomp: D_800F1CE0 (D_800F1CE0@BATTLE/BATTLE.PRG)
VECTOR Field_backgroundReferenceVectors[2];

// decomp: D_800F1D78 (D_800F1D78@BATTLE/BATTLE.PRG)
int Field_backgroundGradientColors[5];

// decomp: D_800F1DAC (D_800F1DAC@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Field_backgroundTextureDiscRequest;

// decomp: D_800F1DB0 (D_800F1DB0@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Field_backgroundLayoutDiscRequest;

// decomp: D_800F1DB4 (D_800F1DB4@BATTLE/BATTLE.PRG)
short Field_backgroundOffsetX;

// decomp: D_800F1DB6 (D_800F1DB6@BATTLE/BATTLE.PRG)
short Field_backgroundOffsetY;

// decomp: D_800F1DB8 (D_800F1DB8@BATTLE/BATTLE.PRG)
short Field_backgroundParallaxScaleXFixed12;

// decomp: D_800F1DBA (D_800F1DBA@BATTLE/BATTLE.PRG)
short Field_backgroundParallaxScaleYFixed12;

// decomp: D_800F1DBE (D_800F1DBE@BATTLE/BATTLE.PRG)
short Field_legacyBackgroundTextureReservationId;

// decomp: D_800F1DC6 (D_800F1DC6@BATTLE/BATTLE.PRG)
char Field_isSolidBackground;

// decomp: D_800F1DC8 (D_800F1DC8@BATTLE/BATTLE.PRG)
char Field_legacyBackgroundTexturePage;

// decomp: D_800F1DC9 (D_800F1DC9@BATTLE/BATTLE.PRG)
u_char Field_isTiledBackgroundEnabled;

// decomp: D_800F1DCC (D_800F1DCC@BATTLE/BATTLE.PRG)
Field_BackgroundLayout* Field_loadedBackgroundLayout;

// decomp: D_800F1DD0 (D_800F1DD0@BATTLE/BATTLE.PRG)
u_long* Field_legacyBackgroundTimBuffer;

// decomp: BATTLE.PRG func_8008E88C (src/BATTLE/BATTLE.PRG/146C.c:17431)
void Field_InitializeRoomBackground(void)
{
    Field_backgroundTextureDiscRequest = NULL;
    Field_backgroundLayoutDiscRequest = NULL;
    Field_loadedBackgroundLayout = NULL;
    Field_legacyBackgroundTimBuffer = NULL;
    Field_weatherParticles = NULL;
    Field_isTiledBackgroundEnabled = 0;
    Field_weatherAmbientSoundId = -2;
    Field_roomWeatherKind = 0;
    Field_isWeatherVariationEnabled = 1;
    Field_InitializeRoomBackgroundColors();
}

// decomp: BATTLE.PRG func_8008E938 (src/BATTLE/BATTLE.PRG/146C.c:17453)
void Field_InitializeRoomBackgroundColors(void)
{
    Field_MpdEnvironment* temp_s0;
    int i;

    Field_backgroundOffsetX = 0;
    Field_backgroundOffsetY = 0;
    Field_backgroundParallaxScaleXFixed12 = 0x1000;
    Field_backgroundParallaxScaleYFixed12 = 0x1000;

    if (Field_roomData.environment != NULL) {

        temp_s0 = Field_roomData.environment;

        for (i = 1; i < 5; ++i) {
            if (temp_s0->backgroundColors[0] != temp_s0->backgroundColors[i]) {
                break;
            }
        }
        if ((i == 5) && (temp_s0->backgroundColors[0] & 0xFFFFFF)) {
            Field_isSolidBackground = 1;
        } else {
            Field_isSolidBackground = 0;
            Field_SetRoomBackgroundColors(temp_s0->backgroundColors);
        }
        if ((temp_s0->weatherArea[0].word == 0) && (temp_s0->weatherArea[1].word == 0)) {
            Field_MpdCollisionHeader* collisionMap =
                Field_roomData.collisionMap;
            if (collisionMap != NULL) {
                temp_s0->weatherArea[1].halves[0] = collisionMap->width * 4;
                temp_s0->weatherArea[1].halves[1] = collisionMap->height * 4;
            } else {
                temp_s0->weatherArea[1].halves[0] = 0x20;
                temp_s0->weatherArea[1].halves[1] = 0x20;
            }
        }
        temp_s0->weatherArea[0].halves[0] *= 128;
        temp_s0->weatherArea[0].halves[1] *= 128;
        temp_s0->weatherArea[1].halves[0] *= 128;
        temp_s0->weatherArea[1].halves[1] *= 128;
        temp_s0->weatherArea[1].halves[0] -= temp_s0->weatherArea[0].halves[0];
        temp_s0->weatherArea[1].halves[1] -= temp_s0->weatherArea[0].halves[1];
    }
}

// decomp: BATTLE.PRG func_8008EAE4 (src/BATTLE/BATTLE.PRG/146C.c:17517)
void Field_SetBackgroundParallaxScale(int arg0, int arg1)
{
    Field_backgroundParallaxScaleXFixed12 = arg0;
    Field_backgroundParallaxScaleYFixed12 = arg1;
}

// decomp: BATTLE.PRG func_8008EAF8 (src/BATTLE/BATTLE.PRG/146C.c:17523)
void Field_SetTiledBackgroundEnabled(int arg0) { Field_isTiledBackgroundEnabled = arg0; }

// decomp: BATTLE.PRG func_8008EB24 (src/BATTLE/BATTLE.PRG/146C.c:17531)
int* Field_RoomBackgroundColors(void) { return Field_backgroundGradientColors; }

// decomp: BATTLE.PRG func_8008EB30 (src/BATTLE/BATTLE.PRG/146C.c:17533)
void Field_SetRoomBackgroundColors(int* arg0)
{
    int i;
    int* var_a0;

    Core_Memory_CopyAlignedWords(Field_backgroundGradientColors, arg0, sizeof Field_backgroundGradientColors);

    var_a0 = Field_legacyBackgroundGradientPackets;

    var_a0 += 2;
    var_a0[0] = (*arg0++ & 0xFFFFFF) | 0x30000000;
    ++var_a0;

    for (i = 0; i < 8; ++i) {
        var_a0 += 2;
        var_a0[0] = (*arg0 & 0xFFFFFF) | 0x30000000;
        ++var_a0;
    }

    ++arg0;

    for (i = 0; i < 16; ++i) {
        var_a0 += 2;
        var_a0[0] = (*arg0 & 0xFFFFFF) | 0x30000000;
        ++var_a0;
    }

    ++arg0;

    for (i = 0; i < 8; ++i) {
        var_a0 += 2;
        var_a0[0] = (*arg0 & 0xFFFFFF) | 0x30000000;
        ++var_a0;
    }

    var_a0[2] = (arg0[1] & 0xFFFFFF) | 0x30000000;
}

// decomp: BATTLE.PRG func_8008EC48 (src/BATTLE/BATTLE.PRG/146C.c:17571)
void Field_DrawRoomBackground(VECTOR* arg0)
{
    VECTOR position;
    MATRIX matrix;
    VECTOR* angles;
    POLY_FT4* prim;
    int red;
    int green;
    int blue;

    if ((Field_isTiledBackgroundEnabled != 0) && (Field_loadedBackgroundLayout != NULL)) {
        if (((Field_loadedBackgroundLayout->width - 1) < 31u) && ((Field_loadedBackgroundLayout->height - 1) < 31u)) {
            int x = (((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vy * Field_backgroundParallaxScaleXFixed12) / ONE
                  - Field_backgroundOffsetX;
            int y = (((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vx * Field_backgroundParallaxScaleYFixed12) / ONE;
            y = Field_backgroundOffsetY - y;
            Field_DrawRoomBackgroundTiles(Field_loadedBackgroundLayout, x, y, Field_legacyBackgroundTexturePage);
        }
    } else if (Field_isSolidBackground != 0) {
        prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
        setPolyFT4(prim);
        setXY4(prim, 0, 0, 320, 0, 0, 240, 320, 240);
        setUV4(prim, 248, 248, 249, 248, 248, 249, 249, 249);
        if (Field_roomTint.isActive != 0) {
            red = 128 + Field_roomTint.red * 4;
            green = 128 + Field_roomTint.green * 4;
            blue = 128 + Field_roomTint.blue * 4;
            red = Core_Math_ClampSignedShort(red, 0, 255);
            green = Core_Math_ClampSignedShort(green, 0, 255);
            blue = Core_Math_ClampSignedShort(blue, 0, 255);
            setRGB0(prim, red, green, blue);
        } else {
            setRGB0(prim, 128, 128, 128);
        }
        setTPage(prim, 0, 0, 64, 256);
        setClut(prim, 784, 226);
        AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 0x1FFC, prim++);
        ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
    } else {
        angles = arg0;
        if (angles == NULL) {
            if (Field_roomData.environment != NULL) {
                angles = &Field_roomData.environment->backgroundRotation;
            } else {
                angles = Field_backgroundReferenceVectors;
            }
        }
        setVector(&position, ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.position.vx / ONE,
            ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.position.vy / ONE,
            ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.position.vz / ONE);
        applyVector(&position, ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.farClip,
            ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.farClip,
            ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.farClip, *=);
        applyVector(&position, ONE, ONE, ONE, /=);
        Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
        Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
        if (((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.farClip != ONE) {
            Render_BuildScaleCompensatedRoomView(&position, angles, &matrix);
        } else {
            Render_BuildRoomGeometryViewMatrix(&position, angles, &matrix);
        }
        Core_Gte_SetRotMatrix(&matrix);
        Core_Gte_SetTransMatrix(&matrix);
        Render_DrawSkyDome(Field_legacyBackgroundGradientPackets);
    }
}

// decomp: BATTLE.PRG func_8008EFCC (src/BATTLE/BATTLE.PRG/146C.c:17638)
int Field_QueueTiledBackgroundLoad(int arg0)
{
    u_short* temp_s0;
    int new_var;
    Content_DiscFile* temp_s2;
    Content_DiscFile* temp_s3;
    int var_a0;
    int var_s0;

    temp_s0 = Field_roomData.tiledBackground;
    Field_legacyBackgroundTexturePage = arg0;
    if (temp_s0 != 0) {
        Field_legacyBackgroundTextureReservationId = *temp_s0;
        if (Render_ReserveTexturePageRange(Field_legacyBackgroundTexturePage, Field_legacyBackgroundTextureReservationId, 5) != 0) {
            new_var = 0x800;
            temp_s2 = (Content_DiscFile*)(temp_s0 + 0x20);
            temp_s3 = (Content_DiscFile*)(temp_s0 + 0x42);
            var_a0 = temp_s2->size;
            var_s0 = temp_s3->size;
            if ((var_a0 & 0x7FF) != 0) {
                var_a0 = (var_a0 + new_var) - (var_a0 & 0x7FF);
            }
            if ((var_s0 & 0x7FF) != 0) {
                var_s0 = (var_s0 + new_var) - (var_s0 & 0x7FF);
            }
            Field_legacyBackgroundTimBuffer = Core_Memory_AllocateFromHighEnd(var_a0);
            Field_loadedBackgroundLayout = Core_Memory_AllocateFromHighEnd(var_s0);
            if ((Field_legacyBackgroundTimBuffer == 0) || (Field_loadedBackgroundLayout == 0)) {
                return 0;
            }
            Field_backgroundTextureDiscRequest = Content_AllocateDiscRequest(temp_s2);
            Field_backgroundLayoutDiscRequest = Content_AllocateDiscRequest(temp_s3);
            Content_EnqueueDiscRequest(Field_backgroundTextureDiscRequest, Field_legacyBackgroundTimBuffer);
            Content_EnqueueDiscRequest(Field_backgroundLayoutDiscRequest, Field_loadedBackgroundLayout);
            return 1;
        }
    }
    Field_backgroundTextureDiscRequest = 0;
    Field_backgroundLayoutDiscRequest = 0;
    return 0;
}

// decomp: BATTLE.PRG func_8008F0FC (src/BATTLE/BATTLE.PRG/146C.c:17680)
int Field_FinishTiledBackgroundLoad(void)
{
    if ((Field_backgroundTextureDiscRequest != NULL) && (Field_backgroundTextureDiscRequest->state == 4)) {
        TIM_IMAGE tim;

        *Field_legacyBackgroundTimBuffer = 0x10;
        Render_ParseTextureImage(Field_legacyBackgroundTimBuffer, &tim);
        if (tim.paddr != NULL) {
            tim.prect->x = (tim.prect->x & 0x3F) + ((Field_legacyBackgroundTexturePage & 0xF) << 6);
            tim.prect->y = (char)tim.prect->y + ((Field_legacyBackgroundTexturePage & 0x10) * 0x10);
            LoadImage(tim.prect, (u_long*)tim.paddr);
        }
        if (tim.caddr != NULL) {
            Render_LoadClutColors((void*)tim.caddr, 0xE, 0, 0x100);
        }
        Content_ReleaseDiscRequest(Field_backgroundTextureDiscRequest);
        Field_backgroundTextureDiscRequest = NULL;
        return 0;
    }

    if ((Field_backgroundLayoutDiscRequest != NULL) && (Field_backgroundLayoutDiscRequest->state == 4)) {
        Core_Memory_FreeFromHighEnd(Field_legacyBackgroundTimBuffer);
        Field_legacyBackgroundTimBuffer = NULL;
        Content_ReleaseDiscRequest(Field_backgroundLayoutDiscRequest);
        Field_backgroundLayoutDiscRequest = NULL;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008F234 (src/BATTLE/BATTLE.PRG/146C.c:17710)
int Field_ReleaseTiledBackground(void)
{
    Field_isTiledBackgroundEnabled = 0;
    if (Field_loadedBackgroundLayout != NULL) {
        Field_isTiledBackgroundEnabled = 0;
        Render_RetireTexturePageRange(Field_legacyBackgroundTexturePage, Field_legacyBackgroundTextureReservationId, 5);
        Core_Memory_FreeFromHighEnd(Field_loadedBackgroundLayout);
        Field_loadedBackgroundLayout = NULL;
        return 1;
    }
    return 0;
}

