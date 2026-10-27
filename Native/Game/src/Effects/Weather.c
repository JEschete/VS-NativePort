// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Memory/Heap.h"
#include "Effects/Weather.h"
#include "Field/Background.h"
#include "Field/Collision.h"
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
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

// decomp: BATTLE.PRG func_8008E8F8 (src/BATTLE/BATTLE.PRG/146C.c:17445)
void Effects_AllocateWeatherParticles(void)
{
    if (Field_weatherParticles != NULL) {
        Core_Memory_FreeFromLowEnd(Field_weatherParticles);
    }
    Field_weatherParticles = Core_Memory_AllocateFromLowEnd(0x800);
}

// decomp: BATTLE.PRG func_8008EA90 (src/BATTLE/BATTLE.PRG/146C.c:17500)
int Effects_InitializeRoomWeather(int suppress_rain)
{
    Field_MpdEnvironment* room_environment = Field_roomData.environment;

    if (room_environment != 0) {
        Field_MpdEnvironment* selected_environment = room_environment;
        room_environment = Field_roomData.environment;
        if ((suppress_rain == 0) || (selected_environment->weatherKind != 1)) {
            ((int (*)(int))Field_SetRoomWeather)(selected_environment->weatherKind);
        } else {
            ((int (*)(int))Field_SetRoomWeather)(0);
        }
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008EB04 (src/BATTLE/BATTLE.PRG/146C.c:17525)
void Effects_RoomWeatherState(int* weather_kind_output, int* intensity_output)
{
    *weather_kind_output = Field_roomWeatherKind;
    *intensity_output = Field_weatherIntensity;
}

// decomp: BATTLE.PRG func_8008F29C (src/BATTLE/BATTLE.PRG/146C.c:17723)
void Effects_InitializeFallingWeatherParticles(int particle_count, int weather_kind)
{
    int i;
    Field_WeatherParticle* particle_cursor = Field_weatherParticles;
    Field_roomWeatherKind = weather_kind;
    Field_weatherParticleCapacity = particle_count;
    Field_weatherParticleCount = particle_count;

    for (i = 0; i < particle_count; ++i) {
        Effects_ResetFallingWeatherParticle(particle_cursor);
        ++particle_cursor;
    }
}

// decomp: BATTLE.PRG func_8008F30C (src/BATTLE/BATTLE.PRG/146C.c:17737)
void Effects_InitializeScreenWeatherParticles(int particle_count, int weather_kind)
{
    Field_WeatherParticle* particle_cursor;
    int i;

    particle_cursor = Field_weatherParticles;
    Field_screenWeatherFade = 0;
    Field_roomWeatherKind = weather_kind;
    Field_weatherParticleCapacity = particle_count;
    Field_weatherParticleCount = particle_count;

    for (i = 0; i < particle_count; ++i, ++particle_cursor) {
        particle_cursor->speed = Core_Random_Rand() & 0x7F;
        particle_cursor->position.vx = Core_Random_Rand() % 320;
        particle_cursor->position.vy = Core_Random_Rand() % 240;
        particle_cursor->position.vz = Core_Random_Rand() % 256;
        particle_cursor->screenX = -1;
        particle_cursor->screenY = -1;
    }
}

// decomp: BATTLE.PRG func_8008F440 (src/BATTLE/BATTLE.PRG/146C.c:17758)
void Effects_DrawRainParticles(void)
{
    SVECTOR vertices[10];
    long projection;
    long flags;
    LINE_G2* line;
    POLY_FT4* splash;
    int depth;
    Field_WeatherParticle* particle;
    int i;
    int j;
    Field_MpdTileCell* cell;

    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    particle = Field_weatherParticles;
    for (i = 0; i < Field_weatherParticleCount; ++i, ++particle) {
        if (particle->opaqueC == 0) {
            if (Field_areRoomUpdatesPaused == 0) {
                for (j = 0; j < (State_tickSpeed >> 1); ++j) {
                    particle->position.vy += Field_weatherParameters[1] << particle->fallShift;
                    if (particle->fallShift == 0) {
                        particle->position.vx += Field_weatherParameters[0];
                        particle->position.vz += Field_weatherParameters[2];
                    }
                }
            }
            particle->fallShift = 2;
            line = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
            depth =
                Core_Gte_RotTransPers(&particle->position, (long*)&line->x0, &projection, &flags);
            if ((line->x0 >= -639) && (line->x0 < 960) && (depth < 2048)) {
                if ((line->y0 > 0) && (line->y0 < 240) && (line->x0 > 0)
                    && (line->x0 < 320) && (depth > 4)) {
                    setVector(&vertices[0], particle->position.vx - Field_weatherParameters[0],
                        particle->position.vy - Field_weatherParameters[1],
                        particle->position.vz - Field_weatherParameters[2]);
                    Core_Gte_RotTransPers(&vertices[0], (long*)&line->x1, &projection, &flags);
                    setLineG2(line);
                    setSemiTrans(line, 1);
                    setRGB0(line, 96, 96, 128);
                    setRGB1(line, 32, 32, 100);
                    AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + depth, line++);
                    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = line;
                    _insertTPage(depth, getTPage(0, 3, 0, 0));
                    particle->fallShift = 0;
                }
            } else {
                Effects_ResetFallingWeatherParticle(particle);
            }
            if (particle->position.vy > 0) {
                if ((particle->fallShift == 0) && (i & 1)) {
                    cell = Field_CollisionCellAt(
                        particle->position.vx / 128, particle->position.vz / 128, 0);
                    if ((cell != NULL) && (cell->surfaceKind != 31)) {
                        particle->opaqueC = 1;
                        projection =
                            (Field_FloorHeightAt(particle->position.vx, particle->position.vz)
                                << 17)
                            >> 17;
                        particle->position.vy = projection;
                    } else {
                        Effects_ResetFallingWeatherParticle(particle);
                    }
                } else {
                    Effects_ResetFallingWeatherParticle(particle);
                }
            }
        } else {
            setVector(&vertices[0], particle->position.vx - 24, particle->position.vy,
                particle->position.vz - 24);
            setVector(&vertices[1], particle->position.vx + 24, particle->position.vy,
                particle->position.vz - 24);
            setVector(&vertices[2], particle->position.vx - 24, particle->position.vy,
                particle->position.vz + 24);
            setVector(&vertices[3], particle->position.vx + 24, particle->position.vy,
                particle->position.vz + 24);
            splash = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
            depth = Core_Gte_RotTransPers4(&vertices[0], &vertices[1], &vertices[2], &vertices[3],
                (long*)&splash->x0, (long*)&splash->x1, (long*)&splash->x2,
                (long*)&splash->x3, &projection, &flags);
            depth -= 64;
            if ((depth > 0) && (depth < 2048)) {
                setPolyFT4(splash);
                setSemiTrans(splash, 1);
                setUV4(splash, (particle->swayPhase >> 1) * 16, 192,
                    (particle->swayPhase >> 1) * 16 + 16, 192, (particle->swayPhase >> 1) * 16, 208,
                    (particle->swayPhase >> 1) * 16 + 16, 208);
                setRGB0(splash, 128, 128, 128);
                setTPage(splash, 0, 3, 64, 256);
                setClut(splash, 1008, 226);
                AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + depth, splash++);
                ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = splash;
                if (Field_areRoomUpdatesPaused == 0) {
                    particle->swayPhase += State_tickSpeed >> 1;
                }
                if (particle->swayPhase >= 8) {
                    Effects_ResetFallingWeatherParticle(particle);
                }
            } else {
                Effects_ResetFallingWeatherParticle(particle);
            }
        }
    }
}

// decomp: BATTLE.PRG func_8008F9A4 (src/BATTLE/BATTLE.PRG/146C.c:17864)
void Effects_InitializeRisingEmberParticles(int particle_count, int weather_kind)
{
    int i;
    Field_WeatherParticle* particle_cursor = Field_weatherParticles;
    Field_roomWeatherKind = weather_kind;
    Field_weatherParticleCapacity = particle_count;
    Field_weatherParticleCount = particle_count;

    for (i = 0; i < particle_count; ++i) {
        Effects_ResetRisingEmberParticle(particle_cursor);
        ++particle_cursor;
    }
}

// decomp: BATTLE.PRG func_8008FA14 (src/BATTLE/BATTLE.PRG/146C.c:17878)
void Effects_ResetRisingEmberParticle(Field_WeatherParticle* weather_particle)
{
    Field_MpdEnvironment* room_environment = Field_roomData.environment;

    weather_particle->position.vx = room_environment->weatherArea[0].halves[0] + (Core_Random_Rand() % room_environment->weatherArea[1].halves[0]);
    weather_particle->position.vz = room_environment->weatherArea[0].halves[1] + (Core_Random_Rand() % room_environment->weatherArea[1].halves[1]);
    weather_particle->position.vy = (Core_Random_Rand() % 512) + 0x200;
    weather_particle->opaqueC = 0;
    weather_particle->speed = (Core_Random_Rand() & 0xF) + 8;
}

// decomp: BATTLE.PRG func_8008FAC8 (src/BATTLE/BATTLE.PRG/146C.c:17889)
void Effects_DrawRisingEmberParticles(void)
{
    long dx;
    long dy;
    POLY_G4* prim;
    int depth;
    Field_WeatherParticle* particle;
    int i;

    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    particle = Field_weatherParticles;
    for (i = 0; i < Field_weatherParticleCount; ++i, ++particle) {
        if (particle->opaqueC != 0) {
            continue;
        }
        if (Field_areRoomUpdatesPaused == 0) {
            particle->position.vy -= particle->speed * State_tickSpeed;
            if (particle->position.vy < -1024) {
                Effects_ResetRisingEmberParticle(particle);
            }
        }
        prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
        depth = Core_Gte_RotTransPers(&particle->position, (long*)&prim->x1, &dx, &dy);
        if ((prim->y1 <= 0) || (prim->y1 >= 240) || (prim->x1 <= 0) || (prim->x1 >= 320)
            || (depth <= 64) || (depth >= 2048)) {
            continue;
        }
        particle->position.vy += 192;
        depth = Core_Gte_RotTransPers(&particle->position, (long*)&prim->x2, &dx, &dy);
        particle->position.vy -= 192;
        dx = (prim->x2 - prim->x1) / 32;
        dy = (prim->y2 - prim->y1) / 32;
        prim->x0 = prim->x1 + dx + dy;
        prim->y0 = prim->y1 + dy - dx;
        prim->x3 = prim->x1 + dx - dy;
        prim->y3 = dx + (prim->y1 + dy);
        setPolyG4(prim);
        setSemiTrans(prim, 1);
        setRGB1(prim, 192, 64, 64);
        setRGB0(prim, 32, 8, 32);
        setRGB3(prim, 32, 8, 32);
        setRGB2(prim, 0, 0, 0);
        AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + depth, prim++);
        ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
        _insertTPage(depth, getTPage(0, 1, 0, 0));
    }
}

// decomp: BATTLE.PRG func_8008FDC4 (src/BATTLE/BATTLE.PRG/146C.c:17938)
void Effects_DrawSnowParticles(void)
{
    SVECTOR unused[6] __attribute__((unused));
    long projection;
    long flags;
    TILE* prim;
    int depth;
    Field_WeatherParticle* particle;
    short x;
    short y;
    int i;
    int j;

    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    particle = Field_weatherParticles;
    for (i = 0; i < Field_weatherParticleCount; ++i, ++particle) {
        if (particle->opaqueC == 0) {
            if (Field_areRoomUpdatesPaused == 0) {
                for (j = 0; j < (State_tickSpeed >> 1); ++j) {
                    if (particle->fallShift != 0) {
                        particle->position.vy += Field_weatherParameters[1] << particle->fallShift;
                    } else {
                        particle->speed += (Core_Random_Rand() & 15) - 7;
                        particle->speed &= 127;
                        if (particle->swayPhase != 0) {
                            particle->position.vx +=
                                (Field_weatherParameters[0] >> 3) + (Core_Gte_Sin(particle->speed << 5) >> 9);
                            particle->position.vz +=
                                (Field_weatherParameters[2] >> 3) - (Core_Gte_Cos(particle->speed << 5) >> 9);
                            particle->position.vy -= Core_Gte_Sin(particle->swayPhase << 6) >> 9;
                            ++particle->swayPhase;
                            if (particle->swayPhase > 32) {
                                particle->swayPhase = 0;
                            }
                        } else {
                            particle->position.vx +=
                                (Field_weatherParameters[0] >> 3) + (Core_Gte_Sin(particle->speed << 5) >> 10);
                            particle->position.vz +=
                                (Field_weatherParameters[2] >> 3) - (Core_Gte_Cos(particle->speed << 5) >> 10);
                            if ((Core_Random_Rand() & 127) == 0) {
                                particle->swayPhase = 1;
                            }
                        }
                        particle->position.vy += Field_weatherParameters[1] >> 3;
                    }
                }
            }
            particle->fallShift = 2;
            prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
            depth =
                Core_Gte_RotTransPers(&particle->position, (long*)&prim->x0, &projection, &flags);
            if ((prim->x0 >= -639) && (prim->x0 < 960) && (depth < 2048)) {
                if (depth > 4) {
                    x = prim->x0;
                    y = prim->y0;
                    if ((y > 0) && (y < 240) && (x > 0) && (x < 320)) {
                        setTile(prim);
                        setSemiTrans(prim, 1);
                        if (i & 1) {
                            setWH(prim, 2, 2);
                        } else {
                            setWH(prim, 1, 1);
                        }
                        setRGB0(prim, 255, 255, 255);
                        AddPrim(
                            (u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + depth, prim++);
                        particle->fallShift = 0;
                    }
                    projection = x + (Core_Gte_Sin(i << 5) >> 6);
                    flags = y - (Core_Gte_Cos(i << 5) >> 6);
                    if ((flags > 0) && (flags < 240) && (projection > 0)
                        && (projection < 320) && (depth > 4)) {
                        setTile(prim);
                        setXY0(prim, projection, flags);
                        setSemiTrans(prim, 1);
                        if ((i & 1) == 0) {
                            setWH(prim, 2, 2);
                        } else {
                            setWH(prim, 1, 1);
                        }
                        setRGB0(prim, 255, 255, 255);
                        AddPrim(
                            (u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + depth, prim++);
                        particle->fallShift = 0;
                    }
                    if (particle->fallShift == 0) {
                        ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
                        _insertTPage(depth, getTPage(0, 3, 64, 256));
                    }
                }
            } else {
                Effects_ResetFallingWeatherParticle(particle);
            }
            if (particle->position.vy > 0) {
                if ((particle->fallShift == 0) && (i & 1)) {
                    particle->opaqueC = 1;
                    particle->swayPhase = 0;
                    projection =
                        (Field_FloorHeightAt(particle->position.vx, particle->position.vz)
                            << 17)
                        >> 17;
                    particle->position.vy = projection;
                } else {
                    Effects_ResetFallingWeatherParticle(particle);
                }
            }
        } else {
            prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
            depth =
                Core_Gte_RotTransPers(&particle->position, (long*)&prim->x0, &projection, &flags);
            if ((prim->y0 > 0) && (prim->y0 < 240) && (prim->x0 > 0) && (prim->x0 < 320)
                && (depth > 4)) {
                setTile(prim);
                setSemiTrans(prim, 1);
                setWH(prim, 2, 2);
                setRGB0(prim, 128 - (particle->swayPhase << 4), 128 - (particle->swayPhase << 4),
                    128 - (particle->swayPhase << 4));
                AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + depth, prim++);
                ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
                _insertTPage(depth, getTPage(0, 3, 0, 0));
                if (Field_areRoomUpdatesPaused == 0) {
                    particle->swayPhase += State_tickSpeed >> 1;
                }
                if (particle->swayPhase >= 8) {
                    Effects_ResetFallingWeatherParticle(particle);
                }
            } else {
                Effects_ResetFallingWeatherParticle(particle);
            }
        }
    }
}

// decomp: BATTLE.PRG func_80090434 (src/BATTLE/BATTLE.PRG/146C.c:18072)
void Effects_DrawScreenWeatherParticles(void)
{
    SVECTOR unused[4] __attribute__((unused));
    VECTOR origin;
    VECTOR velocity;
    long flags;
    void* cursor;
    TILE* prim;
    Field_WeatherParticle* particle;
    int x;
    int y;
    int color;
    int brightness;
    int fade;
    int j;

    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Core_Gte_RotTrans((SVECTOR*)Field_backgroundReferenceVectors, &origin, &flags);
    Core_Gte_RotTrans((SVECTOR*)Field_weatherParameters, &velocity, &flags);
    cursor = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
    setVector(&origin, (velocity.vx - origin.vx) >> 3, (velocity.vy - origin.vy) >> 3,
        (velocity.vz - origin.vz) >> 3);
    if (Field_areRoomUpdatesPaused == 0) {
        if (Field_screenWeatherFade != 0) {
            Field_screenWeatherFade -= 8;
        }
    } else if (Field_screenWeatherFade < 96) {
        Field_screenWeatherFade += 8;
    }
    particle = Field_weatherParticles;
    prim = cursor;
    for (flags = 0; flags < Field_weatherParticleCount; ++flags, ++particle) {
        if (Field_areRoomUpdatesPaused == 0) {
            for (j = 0; j < (State_tickSpeed >> 1); ++j) {
                particle->speed += (Core_Random_Rand() & 15) - 7;
                particle->speed &= 127;
                if (particle->swayPhase != 0) {
                    particle->position.vx += origin.vx + (Core_Gte_Sin(particle->speed << 5) >> 9);
                    particle->position.vz += origin.vz - (Core_Gte_Cos(particle->speed << 5) >> 9);
                    particle->position.vy -= Core_Gte_Sin(particle->swayPhase << 6) >> 9;
                    ++particle->swayPhase;
                    if (particle->swayPhase > 32) {
                        particle->swayPhase = 0;
                    }
                } else {
                    particle->position.vx +=
                        origin.vx + (Core_Gte_Sin(particle->speed << 5) >> 10);
                    particle->position.vz +=
                        origin.vz - (Core_Gte_Cos(particle->speed << 5) >> 10);
                    if ((Core_Random_Rand() & 127) == 0) {
                        particle->swayPhase = 1;
                    }
                }
                particle->position.vy += origin.vy;
                particle->position.vz &= 255;
                if (particle->position.vx < 0) {
                    particle->position.vx += 320;
                }
                if (particle->position.vx > 320) {
                    particle->position.vx -= 320;
                }
                if (particle->position.vy < 0) {
                    particle->position.vy += 240;
                }
                if (particle->position.vy > 240) {
                    particle->position.vy -= 240;
                }
            }
        }
        x = ((particle->position.vx - 160) << 12) / ((particle->position.vz << 2) + 3072)
          + 160;
        y = ((particle->position.vy - 120) << 12) / ((particle->position.vz << 2) + 3072)
          + 120;
        brightness = (255 - particle->position.vz) / 4;
        fade = Field_screenWeatherFade - 32;
        color = brightness - fade;
        if (color < 0) {
            color = 0;
        }
        if ((x < 320u) && (y < 240u)) {
            setTile(prim);
            setXY0(prim, x, y);
            setSemiTrans(prim, 1);
            if (flags & 1) {
                setWH(prim, 2, 2);
            } else {
                setWH(prim, 1, 1);
            }
            setRGB0(prim, color, color, color);
            AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 4, prim++);
            cursor += sizeof(TILE);
        }
        if (((u_short)particle->screenX < 320) && ((u_short)particle->screenY < 240)) {
            setTile(prim);
            setXY0(prim, particle->screenX, particle->screenY);
            setSemiTrans(prim, 1);
            color >>= 1;
            if (flags & 1) {
                setWH(prim, 2, 2);
            } else {
                setWH(prim, 1, 1);
            }
            setRGB0(prim, color, color, color);
            AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 4, prim++);
            cursor += sizeof(TILE);
            setTile(prim);
            setXY0(prim, x - ((x - particle->screenX) * 2), y - ((y - particle->screenY) * 2));
            setSemiTrans(prim, 1);
            color >>= 1;
            if (flags & 1) {
                setWH(prim, 2, 2);
            } else {
                setWH(prim, 1, 1);
            }
            setRGB0(prim, color, color, color);
            AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 4, prim++);
            cursor += sizeof(TILE);
        }
        if (Field_areRoomUpdatesPaused == 0) {
            particle->screenX = x;
            particle->screenY = y;
        }
    }
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = cursor;
    _insertTPage(4, getTPage(0, 1, 64, 256));
}

// decomp: BATTLE.PRG func_80090A1C (src/BATTLE/BATTLE.PRG/146C.c:18200)
void Effects_ResetFallingWeatherParticle(Field_WeatherParticle* weather_particle)
{
    Field_MpdEnvironment* room_environment;

    room_environment = Field_roomData.environment;
    if (room_environment != NULL) {
        weather_particle->position.vx =
            room_environment->weatherArea[0].halves[0] + (Core_Random_Rand() % room_environment->weatherArea[1].halves[0]);
        weather_particle->position.vz =
            room_environment->weatherArea[0].halves[1] + (Core_Random_Rand() % room_environment->weatherArea[1].halves[1]);
    } else {
        weather_particle->position.vx = Core_Random_Rand() % 4096;
        weather_particle->position.vz = Core_Random_Rand() % 4096;
    }
    weather_particle->position.vy = (Core_Random_Rand() % 512) - 2560;
    weather_particle->fallShift = 2;
    weather_particle->opaqueC = 0;
    weather_particle->swayPhase = 0;
    weather_particle->speed = Core_Random_Rand() & 0x7F;
}

// decomp: BATTLE.PRG func_80090B28 (src/BATTLE/BATTLE.PRG/146C.c:18221)
void Effects_RecalculateWeatherVelocity(void)
{
    int elevation_cosine;
    int horizontal_direction_product;
    int direction_component_product;

    horizontal_direction_product = Core_Gte_Sin(Field_weatherParameters[5]);
    elevation_cosine = Core_Gte_Cos(Field_weatherParameters[4]);
    horizontal_direction_product *= Field_weatherParameters[6];
    if (horizontal_direction_product < 0) {
        horizontal_direction_product += 0xFFF;
    }
    direction_component_product = (horizontal_direction_product >> 0xC) * elevation_cosine;
    if (direction_component_product < 0) {
        direction_component_product += 0xFFF;
    }
    Field_weatherParameters[0] = (direction_component_product >> 0xC);
    horizontal_direction_product = Core_Gte_Cos(Field_weatherParameters[5]);
    elevation_cosine = Core_Gte_Cos(Field_weatherParameters[4]);
    horizontal_direction_product = -horizontal_direction_product;
    horizontal_direction_product *= Field_weatherParameters[6];
    if (horizontal_direction_product < 0) {
        horizontal_direction_product += 0xFFF;
    }
    direction_component_product = (horizontal_direction_product >> 0xC) * elevation_cosine;
    if (direction_component_product < 0) {
        direction_component_product += 0xFFF;
    }
    Field_weatherParameters[2] = (direction_component_product >> 0xC);
    direction_component_product = Core_Gte_Sin(Field_weatherParameters[4]) * Field_weatherParameters[6];
    if (direction_component_product < 0) {
        direction_component_product += 0xFFF;
    }
    Field_weatherParameters[1] = (direction_component_product >> 0xC);
}

