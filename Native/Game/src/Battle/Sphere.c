// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Battle/BattleTypes.h"
#include "Battle/Sphere.h"
#include "Battle/TargetVolume.h"
#include "Battle/Targeting.h"
#include "Battle/TargetingDraw.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Input/Controller.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/Menus.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Primitives.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/TextureAnimation.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"

// decomp: D_800EA234 (D_800EA234@BATTLE/BATTLE.PRG)
short Battle_sphereArcBases[502]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA2C4 (D_800EA2C4@BATTLE/BATTLE.PRG)
short Battle_sphereMirroredVertices[430]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA438 (D_800EA438@BATTLE/BATTLE.PRG)
u_char Battle_sphereShapeCommandStarts[488]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA46C (D_800EA46C@BATTLE/BATTLE.PRG)
u_short Battle_sphereWireframeCommands[218]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA670 (D_800EA670@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteOffsets[126]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA684 (D_800EA684@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteSizes[121]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA698 (D_800EA698@BATTLE/BATTLE.PRG)
int Battle_positionCursorSpriteTexCoords[116]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA6AC (D_800EA6AC@BATTLE/BATTLE.PRG)
u_char Battle_positionCursorBlinkFrame; /* contents: module image (ModuleData.c) */

// decomp: D_800EB9AC (D_800EB9AC@BATTLE/BATTLE.PRG)
u_char Battle_reuseSphereVolumes; /* contents: module image (ModuleData.c) */

// decomp: D_800EB9B8 (D_800EB9B8@BATTLE/BATTLE.PRG)
Battle_TargetSphereSetupView* Battle_targetSphere; /* data: contents loaded in Step 5 */

// decomp: BATTLE.PRG func_800C031C (src/BATTLE/BATTLE.PRG/573B8.c:363)
void Battle_InitSphere(void)
{
    int index;
    int x;
    int y;
    short* edges;

    if (Battle_targetSphere == NULL) {
        Battle_targetSphere = Core_Memory_AllocateFromLowEnd(sizeof *Battle_targetSphere);
    }

    Core_Memory_ZeroWordsBackward(Battle_targetSphere, sizeof *Battle_targetSphere);
    index = 0;
    Battle_targetSphere->selectedTarget = -1;
    edges = Battle_targetSphere->wireEdges;

    for (y = 0;; ++y) {
        for (x = 0; x < 9; ++x) {
            edges[index++] = x + (y << 4) + ((x + 1) << 8) + (y << 12);
        }

        if (y == 9) {
            break;
        }

        for (x = 0; x < 10; ++x) {
            edges[index++] = x + (y << 4) + (x << 8) + ((y + 1) << 12);
        }
    }

    for (x = 0;; ++x) {
        for (y = 9; y > 0; --y) {
            edges[index++] = x + (y << 4) + (x << 8) + ((y - 1) << 12);
        }

        if (x == 9) {
            break;
        }

        for (y = 9; y >= 0; --y) {
            edges[index++] = x + (y << 4) + ((x + 1) << 8) + (y << 12);
        }
    }

    for (y = 9;; --y) {
        for (x = 9; x > 0; --x) {
            edges[index++] = x + (y << 4) + ((x - 1) << 8) + (y << 12);
        }

        if (y == 0) {
            break;
        }

        for (x = 9; x >= 0; --x) {
            edges[index++] = x + (y << 4) + (x << 8) + ((y - 1) << 12);
        }
    }

    for (x = 9;; --x) {
        for (y = 0; y < 9; ++y) {
            edges[index++] = x + (y << 4) + (x << 8) + ((y + 1) << 12);
        }

        if (x == 0) {
            break;
        }

        for (y = 0; y < 10; ++y) {
            edges[index++] = x + (y << 4) + ((x - 1) << 8) + (y << 12);
        }
    }
}

// decomp: BATTLE.PRG func_800C05B4 (src/BATTLE/BATTLE.PRG/573B8.c:436)
void Battle_FreeSphere(void)
{
    if (Battle_targetSphere != NULL) {
        Core_Memory_FreeFromLowEnd(Battle_targetSphere);
        Battle_targetSphere = NULL;
    }
}

// decomp: BATTLE.PRG func_800C05EC (src/BATTLE/BATTLE.PRG/573B8.c:444)
void Battle_SetSphereVolumes(Battle_TargetSphereSpec* arg0, Battle_TargetSphereSpec* arg1, u_char arg2, Battle_TargetPreview* arg3)
{
    Battle_targetSphere->primaryVolume = *arg0;

    if (arg0->kind != 1) {
        Battle_targetSphere->wirePhaseA = 0;
        Battle_targetSphere->wirePhaseC = 0;
    }

    if (Battle_reuseSphereVolumes != 0) {
        Battle_targetSphere->primaryVolume.kind |= 0x80;
        Battle_targetSphere->secondaryVolume.kind = arg1->kind;
        Battle_reuseSphereVolumes = 0;
        return;
    }

    Battle_targetSphere->reserved38 = 0;
    Battle_targetSphere->reserved3C = 0;
    Battle_targetSphere->wireShape = 0xF;

    if (arg1 != NULL) {
        Battle_targetSphere->secondaryVolume = *arg1;
        Battle_targetSphere->secondaryVolume.kind |= 0x80;
        Battle_targetSphere->sphereMode = arg2;
        Battle_targetSphere->targetList1 = arg3;
        return;
    }

    Battle_targetSphere->secondaryVolume.kind = 0;
}

// decomp: BATTLE.PRG func_800C06E0 (src/BATTLE/BATTLE.PRG/573B8.c:475)
void Battle_ClearPrimarySphereVolume(void)
{
    if (Battle_targetSphere != NULL) {
        Battle_targetSphere->primaryVolume.kind = 0;
    }
}

// decomp: BATTLE.PRG func_800C0700 (src/BATTLE/BATTLE.PRG/573B8.c:482)
void Battle_SetSecondarySphereVolume(Battle_TargetSphereSpec* arg0)
{
    Battle_targetSphere->secondaryVolume = *arg0;
    Battle_targetSphere->wireShape = 0;
}

// decomp: BATTLE.PRG func_800C0738 (src/BATTLE/BATTLE.PRG/573B8.c:488)
void Battle_ClearSecondarySphereVolume(void)
{
    if (Battle_targetSphere != NULL) {
        Battle_targetSphere->secondaryVolume.kind = 0;
    }
}

// decomp: BATTLE.PRG func_800C0758 (src/BATTLE/BATTLE.PRG/573B8.c:495)
int Battle_AppendSphereArcVertices(int phase, int segments, int index)
{
    short* sine = (void*)((uintptr_t)Core_ScratchBase() + 0x3B0);
    short (*basis)[4] = (void*)((uintptr_t)Core_ScratchBase() + 0x398);
    int i;
    int component;
    Battle_ShapeVertex* vertex = &Battle_targetSphere->wireVertices[index];

    for (i = 0; i <= segments; ++index, ++i, ++vertex) {
        for (component = 0; component < 3; ++component) {
            vertex->xyz[component] =
                basis[0][component]
                + ((basis[1][component] * sine[(i + phase + 8) & 31]) >> 12)
                + ((basis[2][component] * sine[(i + phase) & 31]) >> 12);
        }

        component = i != 0;

        if (component && segments != 32) {
            component += segments - i < 2;
        }

        vertex->opaque6 = component;
    }

    return index;
}

// decomp: BATTLE.PRG func_800C085C (src/BATTLE/BATTLE.PRG/573B8.c:523)
MATRIX* Battle_BuildSphereVolumeTransform(u_char* scale, int angle)
{
    short* scratch = (void*)((uintptr_t)Core_ScratchBase() + 0x350);
    MATRIX* rotation = (void*)((uintptr_t)Core_ScratchBase() + 0x330);
    int axis;
    int component;

    scratch[0] = -(scale[3] * 16);
    scratch[1] = angle;
    scratch[2] = 0;
    Core_Gte_RotMatrixYXZ((SVECTOR*)scratch, rotation);

    for (axis = 0; axis < 3; ++axis) {
        for (component = 0; component < 3; ++component) {
            scratch[component] = axis == component ? scale[axis] * 32 : 0;
        }

        Core_Gte_ApplyMatrixSV(rotation, (SVECTOR*)scratch, (SVECTOR*)&scratch[8 + axis * 4]);
    }

    for (axis = 0; axis < 3; ++axis) {
        for (component = 0; component < 3; ++component) {
            scratch[axis * 3 + component] = scratch[8 + component * 4 + axis];
        }
    }

    return (void*)scratch;
}

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

    primitive = Render_frameScratch.primitiveCursor;
    orderingTable = Render_frameScratch.orderingTable;
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
        sine[i] = Core_Gte_Sin(i * ONE / 32);
    }

    for (i = 0; i < 2; color = 0x10104, ++shape, ++i) {
        MATRIX* m;
        int type;
        int j;

        *shape = (*(Battle_TargetSphere* (*))&Battle_targetSphere)->volumes[i];
        Render_LoadFrameViewMatrix();
        Core_Gte_LoadV0(shape->position);
        Core_Gte_Rtps();
        Core_Gte_StoreSz3Otz(&(*(Battle_TargetSphere* (*))&Battle_targetSphere)->depth);
        switch (shape->kind) {
        case 1:
            type = 0;
            break;
        case 2:
            shape->scale.extents[1] <<= 1;
            m = Battle_BuildSphereVolumeTransform(shape->scale.extents, shape->angle);
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
            m = Battle_BuildSphereVolumeTransform(shape->scale.extents, shape->angle);
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
        Battle_BuildSphereWireframe(type);
        Battle_DrawSphereVolumeWireframe(shape, color);
    }
    Battle_UpdateSphereTargetLabels();
}

// decomp: BATTLE.PRG func_800C0FA8 (src/BATTLE/BATTLE.PRG/58578.c:170)
void Battle_BuildTargetVolumeTestTransform(Battle_TargetVolume* arg0, Battle_TargetVolumeMatrix* arg1, MATRIX* arg2)
{
    int temp_v0;
    int i;
    char* new_var;

    for (i = 0; i < 3; ++i) {
        arg1->opaque10[i] = arg0->position[i];
        arg1->opaque18[i] = 0x8000 / *(new_var = &arg0->scale.extents[i]);
    }

    arg1->opaque0.vx = arg0->scale.extents[3] * 0x10;
    arg1->opaque0.vy = -arg0->angle;
    arg1->opaque0.vz = 0;

    Core_Gte_RotMatrix(&arg1->opaque0, arg2);
}

// decomp: BATTLE.PRG func_800C1034 (src/BATTLE/BATTLE.PRG/58578.c:188)
int Battle_IsPointInTargetEllipsoid(Battle_TargetVolume* arg0, u_short* arg1)
{
    Battle_TargetVolumeMatrix work;
    MATRIX matrix;
    int sum;
    int term;
    int i;

    Battle_BuildTargetVolumeTestTransform(arg0, &work, &matrix);

    {
        void* ptr;
        void* base;
        int offset;

        i = 0;
        ptr = &work;
        base = ptr;
        offset = 0x10;

        do {
            *(short*)ptr = *arg1++ - *(u_short*)(base + offset);
            offset += 2;
            ++i;
            ptr += 2;
        } while (i < 3);
    }

    Core_Gte_ApplyMatrixSV(&matrix, &work.opaque0, &work.opaque8);

    sum = 0;

    {
        char* base;
        int offset2;
        int offset;

        i = 0;
        base = (char*)&work;
        offset2 = 0x18;
        offset = 8;

        do {
            term = (*(short*)(base + offset) * *(short*)(base + offset2)) >> 12;
            sum += term * term;
            offset2 += 2;
            offset += 2;
            ++i;
        } while (i < 3);
    }

    return (sum >> 16) < 1u;
}

// decomp: BATTLE.PRG func_800C110C (src/BATTLE/BATTLE.PRG/58578.c:242)
int Battle_IsPointInTargetCylinder(Battle_TargetVolume* arg0, u_short* arg1, int arg2)
{
    Battle_TargetVolumeMatrix work;
    MATRIX matrix;
    int sum;
    int term;
    int i;

    Battle_BuildTargetVolumeTestTransform(arg0, &work, &matrix);

    {
        short* ptr;
        void* base;
        int offset;

        i = 0;
        ptr = (short*)&work;
        base = &work;
        offset = 0x10;

        for (; i < 3; ++i) {
            *ptr = *arg1++ - *(u_short*)(base + offset);
            offset += 2;
            ++ptr;
        }
    }

    sum = 0;

    Core_Gte_ApplyMatrixSV(&matrix, &work.opaque0, &work.opaque8);

    if (arg2 != 0) {
        work.opaque8.vy -= arg0->scale.flags.opaque1Bit0 << 4;
    }

    if (!((u_short)-work.opaque8.vy > (arg0->scale.flags.opaque1Bit0 << 5))) {
        char* base;
        int offset2;
        int offset;

        i = 0;
        base = (char*)&work;
        offset2 = 0x18;
        offset = 8;

        for (; i < 3; i += 2) {
            term = (*(short*)(base + offset) * *(short*)(base + offset2)) >> 12;
            sum += term * term;
            offset2 += 4;
            offset += 4;
        }

        sum = (sum >> 16) < 1u;
    }
    return sum;
}

// decomp: BATTLE.PRG func_800C123C (src/BATTLE/BATTLE.PRG/58578.c:299)
int Battle_IsPointInTargetCone(Battle_TargetVolume* arg0, u_short* arg1, int arg2)
{
    Battle_TargetVolumeMatrix work;
    MATRIX matrix;
    int temp1;
    int temp2;
    int divisor;
    int radius;

    Battle_BuildTargetVolumeTestTransform(arg0, &work, &matrix);

    {
        int i;
        void* ptr;
        void* base;
        int offset;

        i = 0;
        ptr = &work;
        base = ptr;
        offset = 0x10;

        do {
            *(short*)ptr = *arg1++ - *(u_short*)(base + offset);
            offset += 2;
            ptr += 2;
            ++i;
        } while ((radius = i) < 3);
    }

    Core_Gte_ApplyMatrixSV(&matrix, &work.opaque0, &work.opaque8);

    if (arg2 != 0) {
        work.opaque8.vy -= arg0->scale.flags.opaque1Bit0 << 5;
    }

    if (!((u_short)-work.opaque8.vy > (arg0->scale.flags.opaque1Bit0 << 5))) {
        divisor = -(((int)work.opaque8.vy << 16) >> 13) / arg0->scale.flags.opaque1Bit0;
        temp1 = (work.opaque8.vx * work.opaque18[0]) >> 12;
        temp2 = ((radius = work.opaque8.vz) * work.opaque18[2]) >> 12;
        radius = 0x100 - divisor;

        if ((temp1 * temp1 + temp2 * temp2) < (radius * radius)) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800C1384 (src/BATTLE/BATTLE.PRG/58578.c:348)
int Battle_IsPointInTargetPrism(Battle_TargetVolume* shape, u_short* position, int angled)
{
    struct {
        int corners[5];
        int normalClip;
    } polygon;
    struct {
        short localPoint[4];
        SVECTOR rotatedPoint;
        short origin[4];
    } work;
    MATRIX matrix;
    int i;
    int x;
    int z;
    int offsetX;
    int offsetZ;
    short* local;
    void* base;
    int offset;

    work.origin[0] = shape->position[0];
    work.origin[1] = shape->position[1];
    work.origin[2] = shape->position[2];
    work.localPoint[0] = shape->scale.extents[3] << 4;
    work.localPoint[1] = -shape->angle;
    work.localPoint[2] = 0;
    local = work.localPoint;
    Core_Gte_RotMatrix((SVECTOR*)work.localPoint, &matrix);
    for (i = 0, base = &work, offset = 16; i < 3; offset += 2, ++i, ++local) {
        *local = *position++ - *(u_short*)(base + offset);
    }
    Core_Gte_ApplyMatrixSV(&matrix, (SVECTOR*)work.localPoint, &work.rotatedPoint);
    if ((u_short)-work.rotatedPoint.vy > shape->scale.extents[1] * ONE / 128) {
        return 0;
    }
    x = shape->scale.extents[0] * ONE / 128;
    z = shape->scale.extents[2] * ONE / 128;
    if (angled) {
        offsetZ = z;
        offsetX = -x;
    } else {
        offsetZ = 0;
        offsetX = offsetZ;
    }
    polygon.corners[0] = (u_short)-x | (-offsetZ << 16);
    polygon.corners[1] = -offsetX | (-z << 16);
    polygon.corners[2] = x | (offsetZ << 16);
    polygon.corners[3] = (u_short)offsetX | (z << 16);
    polygon.corners[4] = (u_short)work.rotatedPoint.vx | (work.rotatedPoint.vz << 16);
    i = 0;
    do {
        Core_Gte_LoadSxy3(polygon.corners[4], polygon.corners[i], polygon.corners[(i + 1) & 3]);
        Core_Gte_Nclip();
        Core_Gte_StoreOpz(&polygon.normalClip);
        if (polygon.normalClip < 0) {
            return 0;
        }
        ++i;
    } while (i < 4);
    return 1;
}

// decomp: BATTLE.PRG func_800C1564 (src/BATTLE/BATTLE.PRG/58578.c:411)
int Battle_IsPointInTargetVolume(Battle_TargetVolume* arg0, u_short* arg1)
{
    Battle_TargetVolumeFlags saved_unk4 = arg0->scale.flags;
    int ret = 0;

    switch (arg0->kind) {
    case 1:
        ret = Battle_IsPointInTargetEllipsoid(arg0, arg1);
        break;
    case 2:
        arg0->scale.flags.opaque1Bit0 <<= 1;
        ret = Battle_IsPointInTargetCylinder(arg0, arg1, 1);
        break;
    case 3:
        ret = Battle_IsPointInTargetCylinder(arg0, arg1, 0);
        break;
    case 4:
        ret = Battle_IsPointInTargetCone(arg0, arg1, 0);
        break;
    case 5:
        arg0->scale.flags.opaque3Bit0 = saved_unk4.opaque3Bit0 + 0x80;
        ret = Battle_IsPointInTargetCone(arg0, arg1, 1);
        break;
    case 6:
        ret = Battle_IsPointInTargetPrism(arg0, arg1, 1);
        break;
    case 7:
        ret = Battle_IsPointInTargetPrism(arg0, arg1, 0);
        break;
    }

    arg0->scale.flags = saved_unk4;
    return ret;
}

// decomp: BATTLE.PRG func_800C1664 (src/BATTLE/BATTLE.PRG/58578.c:446)
void Battle_SetSphereTargets(int arg0, Battle_TargetPreview* arg1, int arg2)
{
    int var_a0;
    int var_v0;

    var_a0 = arg0;
    if ((*(Battle_TargetSphere* (*))&Battle_targetSphere) != NULL) {
        if (var_a0 >= 0x19) {
            var_a0 = 0x18;
        }
        (*(Battle_TargetSphere* (*))&Battle_targetSphere)->targetCount = var_a0;
        (*(Battle_TargetSphere* (*))&Battle_targetSphere)->targetLists[0] = arg1;
        if ((*(Battle_TargetSphere* (*))&Battle_targetSphere)->selectedTarget >= var_a0) {
            var_v0 = 0;
            if (var_a0 == 0) {
                var_v0 = 0xFF;
            }
            (*(Battle_TargetSphere* (*))&Battle_targetSphere)->selectedTarget = var_v0;
        }
        (*(Battle_TargetSphere* (*))&Battle_targetSphere)->targetMode = arg2;
        Battle_UpdateTargetSelectionMenu(0);
    }
}

// decomp: BATTLE.PRG func_800C16DC (src/BATTLE/BATTLE.PRG/58578.c:470)
void Battle_ClearSphereTargets(void)
{
    if ((*(Battle_TargetSphere* (*))&Battle_targetSphere) != NULL) {
        (*(Battle_TargetSphere* (*))&Battle_targetSphere)->targetCount = 0;
    }
}

// decomp: BATTLE.PRG func_800C16FC (src/BATTLE/BATTLE.PRG/58578.c:477)
void Battle_DrawRightTargetLabelLink(int x0, int x1, int count)
{
    int y0;
    int y1 = x1 >> 16;
    int midX;
    int signY = 1;
    int signX = 1;
    int length;
    int dx;
    int i;
    int y1Bits;
    int segmentEnd;
    int segmentStart;

    x1 = (short)x1;
    y0 = x0 >> 16;
    x0 = (short)x0;
    length = y0 - y1;
    if (length < 0) {
        signY = -1;
        length = -length;
    }
    if (length & 1) {
        y0 -= signY;
    }
    length >>= 1;
    midX = x0 + length;
    dx = x1 - midX;
    if (dx < 0) {
        signX = -1;
        dx = -dx;
    }
    length = length * 2 + dx;
    segmentEnd = length;
    segmentStart = 0;
    y1Bits = y1 << 16;
    for (i = 0; i < 16; ++i, segmentEnd += length, segmentStart += length) {
        int level = i - count + 16;
        int gray;
        int color;
        int start;

        if (count < i || level < 0) {
            continue;
        }
        gray = ((level * 3) << 6) >> 4;
        gray |= gray << 8;
        color = gray | (((level * 255 + (16 - level) * 32) >> 4) << 16);
        start = segmentStart >> 4;
        if (start < dx) {
            int end = segmentEnd >> 4;

            if (end < dx) {
                Render_InsertThreeWordPrimitive(Render_frameScratch.overlayOrderingTable,
                    vs_getRGB0Raw(Render_LegacyPrimitiveCode_LineF2SemiTrans, color),
                    ((x1 - start * signX) & 0xFFFF) | y1Bits,
                    ((x1 - (end - 1) * signX) & 0xFFFF) | y1Bits);
            } else {
                int lineColor = vs_getRGB0Raw(Render_LegacyPrimitiveCode_LineF2SemiTrans, color);
                int tail;

                Render_InsertThreeWordPrimitive(Render_frameScratch.overlayOrderingTable, lineColor,
                    ((x1 - start * signX) & 0xFFFF) | y1Bits,
                    ((midX + signX) & 0xFFFF) | y1Bits);
                tail = length * (15 - i);
                if (y1 != y0 - (tail >> 4) * signY) {
                    Render_InsertThreeWordPrimitive(Render_frameScratch.overlayOrderingTable, lineColor,
                        (midX & 0xFFFF) | y1Bits,
                        vs_getXY_2(x0 + (tail >> 5), y0 - ((tail + 16) >> 4) * signY));
                }
            }
        } else {
            int far2 = length * (16 - i);
            int near2 = length * (15 - i);

            Render_InsertThreeWordPrimitive(Render_frameScratch.overlayOrderingTable, vs_getRGB0Raw(Render_LegacyPrimitiveCode_LineF2SemiTrans, color),
                vs_getXY_2(x0 + (far2 >> 5), y0 - (far2 >> 4) * signY),
                vs_getXY_2(x0 + (near2 >> 5), y0 - ((near2 + 16) >> 4) * signY));
        }
    }
    Render_InsertDrawModeCommand(
        _get_mode(0, 0, getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, 0, 0)), Render_frameScratch.overlayOrderingTable);
}

// decomp: BATTLE.PRG func_800C1A40 (src/BATTLE/BATTLE.PRG/58578.c:561)
void Battle_DrawLeftTargetLabelLink(int x0, int x1, int count)
{
    int y0;
    int y1 = x1 >> 16;
    int midX;
    int signY = 1;
    int signX = 1;
    int length;
    int dx;
    int i;
    int y1Bits;
    int segmentEnd;
    int segmentStart;

    x1 = (short)x1;
    y0 = x0 >> 16;
    x0 = (short)x0;
    length = y0 - y1;
    if (length < 0) {
        signY = -1;
        length = -length;
    }
    if (length & 1) {
        y0 -= signY;
    }
    length >>= 1;
    midX = x0 - length;
    dx = midX - x1;
    if (dx < 0) {
        signX = -1;
        dx = -dx;
    }
    length = length * 2 + dx;
    segmentEnd = length;
    segmentStart = 0;
    y1Bits = y1 << 16;
    for (i = 0; i < 16; ++i, segmentEnd += length, segmentStart += length) {
        int level = i - count + 16;
        int gray;
        int color;
        int start;

        if (count < i || level < 0) {
            continue;
        }
        gray = ((level * 3) << 6) >> 4;
        gray |= gray << 8;
        color = gray | (((level * 255 + (16 - level) * 32) >> 4) << 16);
        start = segmentStart >> 4;
        if (start < dx) {
            int end = segmentEnd >> 4;

            if (end < dx) {
                Render_InsertThreeWordPrimitive(Render_frameScratch.overlayOrderingTable,
                    vs_getRGB0Raw(Render_LegacyPrimitiveCode_LineF2SemiTrans, color),
                    ((x1 + start * signX) & 0xFFFF) | y1Bits,
                    ((x1 + (end - 1) * signX) & 0xFFFF) | y1Bits);
            } else {
                int lineColor = vs_getRGB0Raw(Render_LegacyPrimitiveCode_LineF2SemiTrans, color);
                int tail;

                Render_InsertThreeWordPrimitive(Render_frameScratch.overlayOrderingTable, lineColor,
                    ((x1 + start * signX) & 0xFFFF) | y1Bits,
                    ((midX - signX) & 0xFFFF) | y1Bits);
                tail = length * (15 - i);
                if (y1 != y0 - (tail >> 4) * signY) {
                    Render_InsertThreeWordPrimitive(Render_frameScratch.overlayOrderingTable, lineColor,
                        (midX & 0xFFFF) | y1Bits,
                        vs_getXY_2(x0 - (tail >> 5), y0 - ((tail + 16) >> 4) * signY));
                }
            }
        } else {
            int far2 = length * (16 - i);
            int near2 = length * (15 - i);

            Render_InsertThreeWordPrimitive(Render_frameScratch.overlayOrderingTable, vs_getRGB0Raw(Render_LegacyPrimitiveCode_LineF2SemiTrans, color),
                vs_getXY_2(x0 - (far2 >> 5), y0 - (far2 >> 4) * signY),
                vs_getXY_2(x0 - (near2 >> 5), y0 - ((near2 + 16) >> 4) * signY));
        }
    }
    Render_InsertDrawModeCommand(
        _get_mode(0, 0, getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, 0, 0)), Render_frameScratch.overlayOrderingTable);
}

// decomp: BATTLE.PRG func_800C1D84 (src/BATTLE/BATTLE.PRG/58578.c:645)
int Battle_AreTargetLabelsSettled(void)
{
    int i;
    for (i = 0; i < 24; ++i) {
        if ((*(Battle_TargetSphere* (*))&Battle_targetSphere)->labels[i].state > 1) {
            return 0;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_800C1DC4 (src/BATTLE/BATTLE.PRG/58578.c:656)
void Battle_DrawSphereTargetLabel(Battle_SphereTargetLabel* arg0)
{
    int gradient = arg0->gradient;
    u_long* nextPrim = Render_frameScratch.overlayOrderingTable - (arg0->depth * 4 - 8);
    int color0;
    int color1;
    int isLeft;
    int x;
    int y;
    int xy;
    int textXy;
    int color;
    Battle_TargetLabelPrimitive* prim;

    if (arg0->depth != 0) {
        gradient = 8;
    }
    color0 = 8 - gradient;
    color0 = Menu_Framework_GradientStopColor(color0, arg0->fade * 8, 0x80);
    color1 = Menu_Framework_GradientStopColor(gradient, arg0->fade * 8, 0x80);

    xy = arg0->screenX;
    isLeft = xy < 160;
    if (isLeft) {
        y = color0;
        color0 = color1;
        color1 = y;
    }
    y = arg0->offsetY + Battle_targetColumnTopOffset;
    x = xy;
    if (isLeft) {
        x += 0x42;
    } else {
        x += 6;
    }
    color = 0x404040;
    textXy = vs_getXY_2(x, y - 1);
    if (arg0->depth != 0) {
        color = 0x808080;
    }
    Render_DrawCompactTextColor(arg0->text, textXy, color, nextPrim);

    prim = Render_frameScratch.primitiveCursor;
    if (gradient != 0) {
        arg0->gradient = gradient - 1;
    }
    x = arg0->screenX & 0xFFFF;
    gradient = (arg0->screenX < 160) * 2;
    prim->tag = vs_getTag(prim->box, *nextPrim);
    prim->box.tile.colorAndCode = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0);
    prim->box.tile.position = vs_getXY_2(x + 2 - gradient, y + 2);
    prim->box.tile.size = (gradient + 0x48) | (8 << 16);
    prim->box.shadePoly.texturePage = _get_mode(0, 1, 0);
    prim->box.shadePoly.colorZeroAndCode = vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4, color0);
    prim->box.shadePoly.positionZero = x | (y << 16);
    prim->box.shadePoly.colorOne = color1;
    prim->box.shadePoly.positionOne = vs_getXY_2(x + 0x48, y);
    prim->box.shadePoly.colorTwo = color0;
    prim->box.shadePoly.positionTwo = x | ((y + 8) << 16);
    prim->box.shadePoly.colorThree = color1;
    prim->box.shadePoly.positionThree = vs_getXY_2(x + 0x48, y + 8);
    prim->box.texturePageCommand = _get_mode(0, 0, 0);
    *nextPrim = LegacyGpu_Handle(prim);
    ++prim;
    Render_frameScratch.primitiveCursor = prim;

    if (arg0->depth != 0) {
        int sxy;
        int otz;
        int ringXy;

        y += 4;
        Render_LoadFrameViewMatrix();
        Core_Gte_LoadV0(&arg0->worldPosition);
        Core_Gte_Rtps();
        Core_Gte_StoreSxy2(&sxy);
        Core_Gte_StoreOtz(&otz);
        if (arg0->screenX < 160) {
            x = (x + 0x47) & 0xFFFF;
            ringXy = x | (y << 16);
            Battle_DrawLeftTargetLabelLink(sxy, ringXy, arg0->ringSize - 2);
            if (arg0->ringSize >= 18) {
                Battle_DrawLeftTargetLabelLink(sxy, ringXy, (arg0->ringSize + 14) & 0x1F);
            }
        } else {
            ringXy = x | (y << 16);
            Battle_DrawRightTargetLabelLink(sxy, ringXy, arg0->ringSize - 2);
            if (arg0->ringSize >= 18) {
                Battle_DrawRightTargetLabelLink(sxy, ringXy, (arg0->ringSize + 14) & 0x1F);
            }
        }
    }
}

// decomp: BATTLE.PRG func_800C20B4 (src/BATTLE/BATTLE.PRG/58578.c:750)
void Battle_UpdateSphereTargetLabels(void)
{
    int temp_a0;
    int var_s3;
    int var_v1;
    Battle_SphereTargetLabel* var_s1 = (*(Battle_TargetSphere* (*))&Battle_targetSphere)->labels;
    short* var_s0 = &(*(Battle_TargetSphere* (*))&Battle_targetSphere)->labels[0].screenX;

    for (var_s3 = 0; var_s3 < 24; ++var_s3, ++var_s1) {
        var_s0 = &var_s1->screenX;
        int v1 = var_s1->state;

        if (v1 == 0) {
            continue;
        }

        switch (v1) {
        case 0:
            break;
        case 2:
            temp_a0 = *var_s0;

            if (temp_a0 >= 160) {
                for (var_v1 = 0; var_v1 < 16; ++var_v1) {
                    if ((Menu_Framework_rowAnimationSteps[var_v1] + 0xF8) >= temp_a0) {
                        break;
                    }
                }

                if (var_v1 != 0) {
                    *var_s0 = Menu_Framework_rowAnimationSteps[var_v1 - 1] + 0xF8;
                } else {
                    *var_s0 = 0xF8;
                    var_s1->state = 1;
                }
            } else {
                *var_s0 = temp_a0 - 0x20;
            }
            break;

        case 3:
            temp_a0 = *var_s0;

            if (temp_a0 >= 0xA0) {
                *var_s0 = temp_a0 + 0x20;
            } else {
                for (var_v1 = 0; var_v1 < 16; ++var_v1) {
                    if ((temp_a0 + 4) >= -Menu_Framework_rowAnimationSteps[var_v1]) {
                        break;
                    }
                }

                if (var_v1 != 0) {
                    temp_a0 = -Menu_Framework_rowAnimationSteps[var_v1 - 1];
                } else {
                    temp_a0 = 0;
                    var_s1->state = 1;
                }

                *var_s0 = temp_a0 - 4;
            }
            break;
        }

        if ((*var_s0 + 0x7E) < 0x1BEU) {
            Battle_DrawSphereTargetLabel(var_s1);
        } else {
            var_s1->state = 0;
        }
    }
}

// decomp: BATTLE.PRG func_800C2254 (src/BATTLE/BATTLE.PRG/58578.c:821)
int Battle_AppendSphereCircleVertices(int angle, int index)
{
    short (*basis)[3] = (void*)((uintptr_t)Core_ScratchBase() + 0x398);
    int point = 0;
    int phase = angle;
    Battle_ShapeVertex* vertex = &(*(Battle_TargetSphere* (*))&Battle_targetSphere)->wireVertices[index];

    for (; point < 33; ++index, phase += ONE / 32, ++point, ++vertex) {
        int component;

        for (component = 0; component < 3; ++component) {
            int cosine = Core_Gte_Cos(phase);
            int sine = Core_Gte_Sin(phase);

            vertex->xyz[component] = basis[0][component]
                                   + ((basis[1][component] * cosine) >> 12)
                                   + ((basis[2][component] * sine) >> 12);
        }
        component = point != 0;
        vertex->opaque6 = component;
    }
    return index;
}

// decomp: BATTLE.PRG func_800C2368 (src/BATTLE/BATTLE.PRG/58578.c:845)
int Battle_AppendSphereConeVertices(int step, int radius, int index)
{
    short* basis = (short*)((uintptr_t)Core_ScratchBase() + 0x398);
    int base;
    int i;
    Battle_ShapeVertex* vertex = &(*(Battle_TargetSphere* (*))&Battle_targetSphere)->wireVertices[index];

    for (base = 0; base < ONE; base += ONE / 4) {
        int x = 0;
        int y = 0;
        int z = 0;
        int phase = base - step * 4;
        int point;

        for (point = 0; point <= radius; ++index, ++point, phase += step, ++vertex) {
            vertex->xyz[0] = (x * Core_Gte_Cos(phase)) >> 12;
            vertex->xyz[1] = y;
            vertex->xyz[2] = (z * Core_Gte_Sin(phase)) >> 12;
            i = point != 0;
            vertex->opaque6 = i;
            x += ONE / 8;
            y -= ONE / 8;
            z += ONE / 8;
        }
    }
    radius *= ONE / 8;
    for (i = 8; i >= 0; --i) {
        basis[i] = 0;
    }
    basis[1] = -radius;
    basis[3] = radius;
    basis[8] = radius;
    index = Battle_AppendSphereCircleVertices(step * 7, index);
    if (radius >= 5) {
        basis[1] = -ONE / 2;
        basis[3] = ONE / 2;
        basis[8] = ONE / 2;
        index = Battle_AppendSphereCircleVertices(step * 4, index);
    }
    return index;
}

// decomp: BATTLE.PRG func_800C253C (src/BATTLE/BATTLE.PRG/58578.c:887)
void Battle_BuildSphereWireframe(int type)
{
    short* basis = (short*)((uintptr_t)Core_ScratchBase() + 0x398);
    int index = 0;
    int shape = (*(Battle_TargetSphere* (*))&Battle_targetSphere)->wireShape;
    Battle_ShapeVertex* vertices = (*(Battle_TargetSphere* (*))&Battle_targetSphere)->wireVertices;
    int count;
    int command;
    u_short* commands;

    if (shape != 0) {
        (*(Battle_TargetSphere* (*))&Battle_targetSphere)->wireShape = shape - 1;
    }
    if (type < 3u) {
        if (type == 0) {
            (*(Battle_TargetSphere* (*))&Battle_targetSphere)->wirePhaseA += (shape + 1) * 3;
            (*(Battle_TargetSphere* (*))&Battle_targetSphere)->wirePhaseC += (shape + 1) * 11;
        }
        (*(Battle_TargetSphere* (*))&Battle_targetSphere)->wirePhaseB += (shape * 2 + 1) * 7;
        shape += type * 16;
    } else {
        shape = type + 0x2D;
    }

    command = Battle_sphereShapeCommandStarts[shape];
    commands = &Battle_sphereWireframeCommands[command];

    for (count = Battle_sphereShapeCommandStarts[shape + 1] - command; count > 0; --count) {
        short* src;
        int i;
        int j;

        command = *commands++;
        switch ((command >> 14) & 3) {
        case 0:
            src = &Battle_sphereArcBases[(command << 2) & 0x7C];
            for (i = 0; i < 3; ++i) {
                basis[i] = *src++;
            }
            for (i = 4; i < 11; ++i) {
                basis[i] = 0;
            }
            j = (command >> 3) & 0x1C;
            i = *src;
            basis[(0x546465 >> j) & 0xF] = i;
            basis[(0x8A998A >> j) & 0xF] = i;
            i = (command >> 9) & 0x1C;
            if (i == 0) {
                i = 0x20;
            }
            index = Battle_AppendSphereArcVertices((command >> 6) & 0x1C, i, index);
            break;
        case 1:
            index = Battle_AppendSphereConeVertices(command & 0x3FF, ((command >> 10) & 7) + 1, index);
            break;
        case 2:
            src = &Battle_sphereMirroredVertices[command & 0x3FF];
            command = ((command >> 10) & 7) + 2;
            for (j = 0; j < 2; ++j) {
                for (i = command; i > 0; --i) {
                    vertices[index].xyz[0] = src[0];
                    src[0] = -vertices[index].xyz[0];
                    vertices[index].xyz[1] = src[1];
                    vertices[index].xyz[2] = src[2];
                    src[2] = -vertices[index].xyz[2];
                    vertices[index].opaque6 = 1;
                    ++index;
                    src += 3;
                }
                vertices[index - command].opaque6 = 0;
                src -= command * 3;
            }
            break;
        }
    }
    (*(Battle_TargetSphere* (*))&Battle_targetSphere)->vertexCount = index;
}

// decomp: BATTLE.PRG func_800C282C (src/BATTLE/BATTLE.PRG/58578.c:965)
void* Battle_BuildSphereCameraBasis(void)
{
    Field_SphericalCamera camera;
    int temp_a0;
    int temp_s2;
    int yaw;
    short* p = (short*)((uintptr_t)Core_ScratchBase() + 0x350);
    Field_SyncCameraAnglesFromPosition(&camera);
    yaw = camera.values.yaw;
    temp_s2 = Core_Gte_Cos(yaw);
    temp_a0 = Core_Gte_Sin(yaw);
    p[0] = temp_s2;
    p[2] = temp_a0;
    p[8] = -temp_a0;
    p[10] = temp_s2;
    (*(Battle_TargetSphere* (*))&Battle_targetSphere)->cameraYaw = yaw;
    return (void*)((uintptr_t)Core_ScratchBase() + 0x350);
}

// decomp: BATTLE.PRG func_800C28AC (src/BATTLE/BATTLE.PRG/58578.c:984)
void Battle_DrawSpherePositionCursor(SVECTOR* position, int arg1)
{
    int xy;
    int i;
    u_long* prim;
    u_long* ot = Render_frameScratch.orderingTable - 4;

    Core_Gte_LoadV0(position);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2(&xy);

    Battle_positionCursorBlinkFrame = (Battle_positionCursorBlinkFrame + 1) & 0xF;

    if (arg1 == 0) {
        prim = Render_frameScratch.primitiveCursor;
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
        int index = i + arg1 * 2;

        prim = Render_QueueSpriteWithTexturePage(
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
    column = Field_CollisionMapDimensions(0);
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
            Field_MpdTileCell* room = Field_CollisionCellAt(x, z, 0);

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

                Field_MeasureTileStack(x, z, &offset);
                if (offset != 0) {
                    if (Field_gimmickInstances[Field_FindTopGimmickAtTile(x, z, 0)]->objectInfo.objectId < 2) {
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
                        int sample = Field_FloorAndCeilingHeightsAt(x + dx, z + dz);
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

// decomp: BATTLE.PRG func_800C4650 (src/BATTLE/BATTLE.PRG/5BE50.c:65)
void Battle_UpdateSphereTargetHighlights(Battle_TargetLabelLine* arg0, int arg1)
{
    int i;
    int type;

    for (i = 0; i < arg1; i++, arg0++) {
        type = arg0->opaque9;
        if ((type >> 4) == 0) {
            if (Battle_IsPointInTargetVolume(&(*(Battle_TargetSphere* (*))&Battle_targetSphere)->volumes[1], arg0->opaque0)) {
                Actors_EnableModelPalettePulse(type, 0, arg0->opaqueA);
            } else {
                Actors_DisableModelPalettePulse(type, arg0->opaqueA);
            }
        }
    }
}

// decomp: BATTLE.PRG func_800C4734 (src/BATTLE/BATTLE.PRG/5BE50.c:93)
int Battle_SphereInteractionMode(void)
{
    if (((*(Battle_TargetSphere* (*))&Battle_targetSphere) == NULL) || ((*(Battle_TargetSphere* (*))&Battle_targetSphere)->wireShape != 0)) {
        return 0;
    }
    if (((*(Battle_TargetSphere* (*))&Battle_targetSphere)->inputLocked == 0) || (Input_buttonsState & 0x80)) {
        return 1;
    }
    return 2;
}

