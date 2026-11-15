// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/SphereGeometryLive.hpp"
#include "Battle/SphereTargetLabelsLive.hpp"
#include "Battle/TargetingDrawLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Render/SpritesLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Battle/BattleTypes.h"
#include "Battle/Feedback.h"
#include "Battle/Sphere.h"
#include "Battle/TargetPages.h"
#include "Battle/TargetVolume.h"
#include "Battle/TargetingState.h"
#include "Core/Math/FixedPoint.h"
#include "Field/Camera.h"
#include "Field/GimmickPlacementLive.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Menu/Framework/Menus.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Primitives.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: D_800EA234 (D_800EA234@BATTLE/BATTLE.PRG)
short Battle_sphereArcBases[502]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA2C4 (D_800EA2C4@BATTLE/BATTLE.PRG)
short Battle_sphereMirroredVertices[430]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA438 (D_800EA438@BATTLE/BATTLE.PRG)
u_char Battle_sphereShapeCommandStarts[488]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA46C (D_800EA46C@BATTLE/BATTLE.PRG)
u_short Battle_sphereWireframeCommands[218]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA670 (D_800EA670@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteOffsets[126]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA684 (D_800EA684@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteSizes[121]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA698 (D_800EA698@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteTexCoords[116]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA6AC (D_800EA6AC@BATTLE/BATTLE.PRG)
u_char Battle_positionCursorBlinkFrame; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9AC (D_800EB9AC@BATTLE/BATTLE.PRG)
u_char Battle_reuseSphereVolumes; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9B8 (D_800EB9B8@BATTLE/BATTLE.PRG)
Battle_TargetSphereSetupView* Battle_targetSphere; /* data: contents loaded in Step 5 */

// decomp: BATTLE.PRG func_800C0990 (src/BATTLE/BATTLE.PRG/573B8.c:552)
void Battle_DrawSphereWireLine(SVECTOR* start, SVECTOR* end, u_int color, int intensity)
{
    int depth = (start->vz + end->vz) >> 1;
    int dx;
    int dy;
    int ax;
    int ay;
    int angle;
    int shift;
    u_long link;
    u_long* primitive;
    u_long* orderingTable;

    if (depth >= 2048u) {
        return;
    }

    dy = end->vy - start->vy;
    dx = end->vx - start->vx;
    angle = 0;
    shift = dy | dx;

    if (shift) {
        ax = dx;
        ay = dy;

        if (ax < 0) {
            ax = -ax;
        }

        if (ay < 0) {
            ay = -ay;
        }

        shift = ONE / 8;

        if (ax < ay) {
            angle = ax << 9;
            angle /= ay;
            angle = Core_Math_kArcTangentTable[angle];
            angle = shift - angle;
        } else {
            angle = ay << 9;
            angle /= ax;
            angle = Core_Math_kArcTangentTable[angle];
        }

        shift = ONE / 4;

        if (ax != dx) {
            angle = shift - angle;
        }

        angle *= 2;

        if (ay != dy) {
            angle = -angle;
        }
    }

    shift = 30 - (((angle + 256) >> 8) & 14);
    ay = start->vx + ((0x4FC5 << shift) >> 30);
    ax = start->vy + ((0xFC54 << shift) >> 30);

    if (intensity < 0) {
        intensity = 0;
    } else if (intensity >= 64) {
        intensity = 63;
    }

    primitive = (u_long *)(Render_frameScratch.primitiveCursor);
    orderingTable = (u_long *)(Render_frameScratch.orderingTable);
    orderingTable += depth;
    link = *orderingTable & 0xFFFFFF;
    primitive[1] = _get_mode(0, 1, 0);
    primitive[3] = vs_getXY_2(ay, ax);
    primitive[0] = link | 0x04000000;
    primitive[2] = (((color << 8) >> 8) * intensity) | ((color >> 24) << 24);
    primitive[4] = *(u_long*)end;
    *orderingTable = LegacyGpu_Handle(primitive);
    Render_frameScratch.primitiveCursor = primitive + 5;
}

// decomp: BATTLE.PRG func_800C0D78 (src/BATTLE/BATTLE.PRG/58578.c:104)
void Battle_DrawSphere(void)
{
    Battle_TargetVolume* shape = (Battle_TargetVolume*)((uintptr_t)Core_ScratchBase() + 0x378);
    int color = 0x10401;
    short* sine = (short*)((uintptr_t)Core_ScratchBase() + 0x3B0);
    int i;

    if ((*(Battle_TargetSphere* (*))&Battle_targetSphere) == NULL) {
        return;
    }

    for (i = 0; i < 40; ++i) {
        sine[i] = VS::Core::Math::Sine(i * ONE / 32);
    }

    for (i = 0; i < 2; color = 0x10104, ++shape, ++i) {
        MATRIX* m;
        int type;
        int j;

        *shape = (*(Battle_TargetSphere* (*))&Battle_targetSphere)->volumes[i];
        VS::Render::LoadLiveFrameViewMatrix();
        Core_Gte_LoadV0((const SVECTOR *)(shape->position));
        Core_Gte_Rtps();
        Core_Gte_StoreSz3Otz((long *)(&(*(Battle_TargetSphere* (*))&Battle_targetSphere)->depth));
        switch (shape->kind) {
        case 1:
            type = 0;
            break;
        case 2:
            shape->scale.extents[1] <<= 1;
            m = VS::Battle::BuildSphereVolumeTransformEntry(shape->scale.extents, shape->angle);
            for (j = 0; j < 3; ++j) {
                shape->position[j] += m->m[j][1] >> 1;
            }
            type = 1;
            break;
        case 3:
            type = 1;
            break;
        case 4:
            shape->scale.extents[3] += 0x80;
            m = VS::Battle::BuildSphereVolumeTransformEntry(shape->scale.extents, shape->angle);
            for (j = 0; j < 3; ++j) {
                shape->position[j] += m->m[j][1];
            }
            type = 2;
            break;
        case 5:
            type = 2;
            break;
        case 6:
            type = 3;
            break;
        case 7:
            type = 4;
            break;
        default:
            continue;
        }
        VS::Battle::BuildSphereWireframeEntry(type);
        VS::Battle::DrawLiveSphereVolumeWireframe(shape, color);
    }
    VS::Battle::UpdateSphereTargetLabelsEntry();
    Battle_DrawTargetPageHint();
}

// decomp: BATTLE.PRG func_800C282C (src/BATTLE/BATTLE.PRG/58578.c:965)
void* Battle_BuildSphereCameraBasis(void)
{
    Field_SphericalCamera camera;
    int yaw_sine;
    int yaw_cosine;
    int yaw;
    short* p = (short*)((uintptr_t)Core_ScratchBase() + 0x350);
    VS::Field::LiveSyncCameraAnglesFromPosition(&camera);
    yaw = camera.values.yaw;
    yaw_cosine = VS::Core::Math::Cosine(yaw);
    yaw_sine = VS::Core::Math::Sine(yaw);
    p[0] = yaw_cosine;
    p[2] = yaw_sine;
    p[8] = -yaw_sine;
    p[10] = yaw_cosine;
    (*(Battle_TargetSphere* (*))&Battle_targetSphere)->cameraYaw = yaw;
    return (void*)((uintptr_t)Core_ScratchBase() + 0x350);
}

// decomp: BATTLE.PRG func_800C28AC (src/BATTLE/BATTLE.PRG/58578.c:984)
void Battle_DrawSpherePositionCursor(SVECTOR* position, int lock_pitch)
{
    int xy;
    int i;
    u_long* prim;
    u_long* ot = (u_long *)(((u_char*)(Render_frameScratch.orderingTable)) - 4);

    Core_Gte_LoadV0(position);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2((long *)(&xy));

    Battle_positionCursorBlinkFrame = (Battle_positionCursorBlinkFrame + 1) & 0xF;

    if (lock_pitch == 0) {
        prim = (u_long *)(Render_frameScratch.primitiveCursor);
        prim[0] = vs_getTag(Render_PolyFt4, *ot);
        prim[1] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyFt4, 0x80, 0x80, 0x80);
        prim[2] = xy + vs_getXY(-13, -9);
        prim[3] = vs_getUV0Clut(241, 56, 976, 223);
        prim[4] = xy + vs_getXY(-3, -9);
        prim[5] = vs_getUV1Tpage(231, 56, 768, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average);
        prim[6] = xy + vs_getXY(-13, -2);
        prim[7] = vs_getUV(241, 63);
        prim[8] = xy + vs_getXY(-3, -2);
        prim[9] = vs_getUV(231, 63);
        *ot = (u_long)prim;
        prim += 10;
        prim[0] = vs_getTag(Render_PolyFt4, *ot);
        prim[1] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyFt4, 0x80, 0x80, 0x80);
        prim[2] = xy + vs_getXY(-13, 1);
        prim[3] = vs_getUV0Clut(241, 64, 976, 223);
        prim[4] = xy + vs_getXY(-3, 1);
        prim[5] = vs_getUV1Tpage(231, 64, 768, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average);
        prim[6] = xy + vs_getXY(-13, 8);
        prim[7] = vs_getUV(241, 71);
        prim[8] = xy + vs_getXY(-3, 8);
        prim[9] = vs_getUV(231, 71);
        *ot = (u_long)prim;
        prim += 10;
        Render_frameScratch.primitiveCursor = prim;
    }

    for (i = 0; i < 3; ++i) {
        int index = i + lock_pitch * 2;

        prim = VS::Render::QueueLiveSpriteWithTexturePage(
            index == 2 ? Menu_Framework_cursorBrightnessAnimation[Battle_positionCursorBlinkFrame] : 0x80,
            xy + Battle_positionCursorSpriteOffsets[index], Battle_positionCursorSpriteSizes[index], ot);
        prim[4] = Battle_positionCursorSpriteTexCoords[index];
    }
}

// decomp: BATTLE.PRG func_800C2B0C (src/BATTLE/BATTLE.PRG/58578.c:1036)
void Battle_CacheSphereTerrainRow(SVECTOR* position, u_int row)
{
    u_int width;
    u_int height;
    int column;

    if (row >= 9) {
        return;
    }
    memset(&(*(Battle_TargetSphere* (*))&Battle_targetSphere)->terrainRowFlags[row * 9], 0, 9);
    column = VS::Field::LiveCollisionMapDimensions(0);
    width = (column & 0xFFFF) >> 4;
    height = column >> 20;

    for (column = 0; column < 9; ++column) {
        int baseX = position->vx >> 7;
        int tileX = column - 4;
        int x = baseX + tileX;
        int baseZ = position->vz >> 7;
        int tileZ = row - 4;
        int z = baseZ + tileZ;
        int value = 0x10000;

        if ((x < width) & (z < height)) {
            const Field_MpdTileCell* room = VS::Field::LiveCollisionCellOrAbsent(x, z, 0);

            if (room->blocksSphereTerrain) {
                value = 0xF0800000;
                (*(Battle_TargetSphere* (*))&Battle_targetSphere)->terrainRowFlags[row * 9 + column] = 0;
            } else if (room->doorPassability < 5 || (room->occlusionAndFloorFlags >> 1)) {
                int offset;
                int low;
                int high;
                int top;
                int dx;
                int dz;

                Field_MeasureTileStackEntry(x, z, &offset);
                if (offset != 0) {
                    if (Field_gimmickInstances[Field_FindTopGimmickAtTileEntry(x, z, 0)]->objectInfo.objectId < 2) {
                        value = 0;
                        offset += 0x20;
                    } else {
                        offset += 0x10;
                    }
                }
                x <<= 7;
                z <<= 7;
                low = 0x4000;
                high = -0x4000;
                top = -0x4000;
                for (dz = 0; dz < 0x80; dz += 0x20) {
                    for (dx = 0; dx < 0x80; dx += 0x20) {
                        int sample = VS::Field::LiveFloorAndCeilingHeightsAt(x + dx, z + dz);
                        int level = (sample << 17) >> 17;

                        level += offset;

                        if (level < low) {
                            low = level;
                        }
                        if (high < level) {
                            high = level;
                        }
                        level = (sample << 1) >> 17;
                        if (top < level) {
                            top = level;
                        }
                    }
                }
                if (value != 0) {
                    value = low == high;
                }
                if (room->doorPassability - 1 < 4u) {
                    low = high;
                    (*(Battle_TargetSphere* (*))&Battle_targetSphere)->terrainRowFlags[row * 9 + column] = room->doorPassability * 6;
                }
                value |= ((top + 0x30) << 17) | ((low & 0x7FFF) << 2);
            }
        }
        (*(Battle_TargetSphere* (*))&Battle_targetSphere)->terrainHeights[row * 9 + column] = value;
    }
}

}  // extern "C"
