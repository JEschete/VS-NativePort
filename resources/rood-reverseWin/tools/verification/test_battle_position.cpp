extern "C" {
#define wchar_t psx_wchar_t
#include "../../src/BATTLE/BATTLE.PRG/38C1C.c"
#undef wchar_t
#include <psx_gte.h>
int puts(const char* text);
}

#define reconstructedMatrixProvider func_800A1DE8

static_assert(sizeof(void*) == 4, "The game ABI requires 32-bit pointers.");
static_assert(sizeof(int) == 4 && sizeof(long) == 4, "The game ABI requires 32-bit words.");
static_assert(sizeof(MATRIX) == 32 && __builtin_offsetof(MATRIX, t) == 20,
    "Matrix layout must match the binary.");
static_assert(__builtin_offsetof(D_800F4538_t, unk0.unk68) == 0x68,
    "Actor skeleton offset must match the binary.");
static_assert(__builtin_offsetof(D_800F45E0_t, unk68) == 0x68,
    "Object skeleton offset must match the binary.");
static_assert(__builtin_offsetof(vs_battle_wepModels_t, offsets) == 0x1C,
    "Weapon skeleton offset must match the binary.");
static_assert(sizeof(_armature_t) == 16
    && __builtin_offsetof(D_800F4538_unk68, armatures) == 0x40,
    "Bone stride and length offset must match the binary.");
static_assert(__builtin_offsetof(D_800F4538_t, unk704.unk0[41]) == 0x84C
    && __builtin_offsetof(D_800F4538_t, unk1848.unk8) == 0x1850
    && __builtin_offsetof(D_800F4538_t, unk1848.unk10) == 0x1858,
    "Special-anchor and movement layouts must match the binary.");
static_assert(__builtin_offsetof(D_800F4538_t, animationId) == 0x5B8,
    "Animation field must match the binary.");
static_assert(__builtin_offsetof(D_800F4538_t, unk183C) == 0x183C,
    "Scale field must match the binary.");
static_assert(__builtin_offsetof(D_800F4538_t, unk1848) == 0x1848,
    "Movement field must match the binary.");

D_800F4538_t* D_800F4538[17];
D_800F45E0_t* D_800F45E0[16];
vs_battle_wepModels_t* vs_battle_wepModels[20];
MATRIX D_800F49B8;

struct SkeletonFixture {
    D_800F4538_unk68 header;
    _armature_t bones[4];
};

static D_800F4538_t actor;
static D_800F45E0_t object;
static vs_battle_wepModels_t weapon;
static SkeletonFixture actorSkeleton;
static SkeletonFixture objectSkeleton;
static SkeletonFixture weaponSkeleton;
static MATRIX selectedMatrix;

static SVECTOR specialLocalPosition;
static int specialMatrixCalls;
void func_800B07DC(D_800F4538_t*)
{
    ++specialMatrixCalls;
    specialLocalPosition = *(SVECTOR*)getScratchAddr(0xEC);
    *(MATRIX*)getScratchAddr(0xF0) = selectedMatrix;
    *(SVECTOR*)getScratchAddr(0xEC) = (SVECTOR){-21, 22, -23, 0};
}

struct CallRecord {
    int kind;
    int actorSlot;
    int value;
    int extra;
};
static CallRecord calls[64];
static int callCount;

void func_800A0204(int slot, int animation, int unused, int mode)
{
    if (unused != 0 || callCount >= 64) __builtin_trap();
    calls[callCount++] = {1, slot, animation, mode};
}
void func_800A9EB4(int slot, int facing, int mode)
{
    calls[callCount++] = {2, slot, facing, mode};
}
void func_8009DF3C(int slot, int mode)
{
    calls[callCount++] = {3, slot, mode, 0};
}
void func_800AA850(int slot, int value, int mode)
{
    calls[callCount++] = {4, slot, value, mode};
}

static int groundHeight;
static bool terrainAvailable;
static u_int terrainWord;
static SVECTOR queriedPosition;
static int queriedX;
static int queriedZ;
static int activeMovementY;
static SVECTOR resolvedDestination;
static int destinationKind;

SVECTOR* func_800A4A24(int kind)
{
    destinationKind = kind;
    calls[callCount++] = {8, 0, kind, 0};
    return &resolvedDestination;
}
int func_800A3500(int targetX, int targetZ)
{
    calls[callCount++] = {9, 0, targetX, targetZ};
    return groundHeight;
}
void func_800AE4FC(D_800F4538_unk0* actorHeader, int mode)
{
    calls[callCount++] = {10, actorHeader->unkF, mode, 0};
}

int func_800A6EE8(SVECTOR* position, int offsetX, int offsetZ, int mode)
{
    queriedPosition = *position;
    queriedX = offsetX;
    queriedZ = offsetZ;
    calls[callCount++] = {5, 0, mode, 0};
    return groundHeight;
}
u_int* func_800A8D64(SVECTOR* destination, int mode)
{
    queriedPosition = *destination;
    calls[callCount++] = {6, 0, mode, 0};
    return terrainAvailable ? &terrainWord : NULL;
}
void func_800AC690(int slot, D_800F4538_t* movingActor)
{
    calls[callCount++] = {7, slot, movingActor->unk0.position.vx,
        movingActor->unk0.position.vz};
    movingActor->unk0.position.vy = (short)activeMovementY;
}

static void resetFixture()
{
    for (int slot = 0; slot < 17; ++slot) D_800F4538[slot] = NULL;
    for (int slot = 0; slot < 16; ++slot) D_800F45E0[slot] = NULL;
    for (int slot = 0; slot < 20; ++slot) vs_battle_wepModels[slot] = NULL;
    actor = {};
    object = {};
    weapon = {};
    actorSkeleton = {};
    objectSkeleton = {};
    weaponSkeleton = {};
    selectedMatrix = {};
    selectedMatrix.m[0][0] = ONE;
    selectedMatrix.m[1][1] = ONE;
    selectedMatrix.m[2][2] = ONE;
    D_800F49B8 = selectedMatrix;
    actor.unk0.unk68 = &actorSkeleton.header;
    object.unk68 = (D_800F45E0_unk68*)&objectSkeleton.header;
    weapon.offsets = (_wepModelOffsets*)&weaponSkeleton.header;
    specialMatrixCalls = 0;
    callCount = 0;
    groundHeight = 0;
    terrainAvailable = true;
    activeMovementY = 0;
    resolvedDestination = (SVECTOR){192, 0, 64, 0};
    destinationKind = -1;
    gte_reset();
}

static bool expectPosition(int actorSlot, int boneSelector, int mode,
    unsigned int expectedX, unsigned int expectedY, unsigned int expectedZ)
{
    if ((boneSelector & 0xF0) == 0x40) {
        vs_battle_wepModels_t* model = vs_battle_wepModels[actorSlot * 2];
        if (model != NULL) {
            copyBoneMatrix((MATRIX*)((u_char*)model + 0x40
                + (boneSelector - 0x40) * sizeof(MATRIX)), &selectedMatrix);
        }
    } else if (D_800F4538[actorSlot] != NULL) {
        D_800F4538[actorSlot]->bones[boneSelector] = selectedMatrix;
    } else if (D_800F45E0[actorSlot] != NULL) {
        copyBoneMatrix((MATRIX*)&D_800F45E0[actorSlot]->unk6C[boneSelector], &selectedMatrix);
    }
    u_short output[4] = {0xA111, 0xB222, 0xC333, 0xD444};
    return func_800A1C10(actorSlot, boneSelector, output, mode) == 0
        && output[0] == (u_short)expectedX && output[1] == (u_short)expectedY
        && output[2] == (u_short)expectedZ && output[3] == 0xD444;
}

int main()
{
    extern void* mapTestScratchpad();
    if (mapTestScratchpad() != (void*)0x1F800000) return 100;
    resetFixture();
    u_short missingOutput[4] = {11, 22, 33, 44};
    if (func_800A1C10(2, 1, missingOutput, 0) != -1
        || missingOutput[0] != 11 || missingOutput[1] != 22
        || missingOutput[2] != 33 || missingOutput[3] != 44) return 1;

    D_800F4538[2] = &actor;
    selectedMatrix.t[0] = 0x12345;
    selectedMatrix.t[1] = -2;
    selectedMatrix.t[2] = 0x18003;
    gte_set_ctrl(GTE_C_TRX, 77);
    if (!expectPosition(2, 1, 0, 0x2345, 0xFFFE, 0x8003)
        || gte_get_ctrl(GTE_C_TRX) != 0) return 2;

    selectedMatrix.t[0] = 100;
    selectedMatrix.t[1] = 200;
    selectedMatrix.t[2] = 300;
    actorSkeleton.bones[1].unk0 = 9;
    if (!expectPosition(2, 1, 1, 91, 200, 300)) return 3;
    if (!expectPosition(2, 1, 2, 96, 200, 300)) return 4;
    actorSkeleton.bones[1].unk0 = -9;
    if (!expectPosition(2, 1, -7, 104, 200, 300)) return 5;
    actorSkeleton.bones[1].unk0 = 8;
    if (!expectPosition(2, 1, 2, 96, 200, 300)) return 17;
    actorSkeleton.bones[1].unk0 = -8;
    if (!expectPosition(2, 1, 2, 104, 200, 300)) return 18;
    actorSkeleton.bones[1].unk0 = 0x10003;
    if (!expectPosition(2, 1, 1, 97, 200, 300)) return 6;
    if (!expectPosition(2, 1, 2, 100 - 32769, 200, 300)) return 7;
    actorSkeleton.bones[1].unk0 = (-2147483647 - 1);
    if (!expectPosition(2, 1, 2, 100, 200, 300)) return 8;

    D_800F4538[2] = NULL;
    D_800F45E0[2] = &object;
    objectSkeleton.bones[1].unk0 = 20;
    if (!expectPosition(2, 1, 1, 80, 200, 300)) return 9;
    D_800F4538[2] = &actor;
    actorSkeleton.bones[1].unk0 = 30;
    if (!expectPosition(2, 1, 1, 70, 200, 300)) return 10;

    if (func_800A1C10(2, 0x40, missingOutput, 1) != -1
        || missingOutput[0] != 11 || missingOutput[1] != 22
        || missingOutput[2] != 33 || missingOutput[3] != 44) return 11;
    vs_battle_wepModels[4] = &weapon;
    if (!expectPosition(2, 0x40, 0, 100, 200, 300)) return 12;
    weaponSkeleton.bones[1].unk0 = 31;
    if (!expectPosition(2, 0x40, 1, 69, 200, 300)) return 13;
    weaponSkeleton.bones[2].unk0 = 33;
    if (!expectPosition(2, 0x41, 2, 84, 200, 300)) return 14;

    selectedMatrix.m[0][0] = ONE / 2;
    selectedMatrix.m[1][0] = -ONE;
    selectedMatrix.m[2][0] = ONE / 4;
    actorSkeleton.bones[1].unk0 = 3;
    if (!expectPosition(2, 1, 1, 98, 203, 299)) return 15;

    selectedMatrix.t[0] = 40000;
    selectedMatrix.t[1] = -40000;
    selectedMatrix.t[2] = 70000;
    actorSkeleton.bones[1].unk0 = 0;
    gte_set_data(GTE_D_SXY2, 0x12345678);
    D_800F49B8.m[0][0] = 8192;
    if (!expectPosition(2, 1, 1, 80000, -40000, 70000)
        || (int32_t)gte_get_data(GTE_D_IR1) != -32768
        || (gte_get_ctrl(GTE_C_FLAG) & GTE_FLAG_IR1_SAT) == 0
        || gte_get_data(GTE_D_SXY2) != 0x12345678) return 16;

    resetFixture();
    if (reconstructedMatrixProvider(2, 1, NULL) != NULL) return 19;
    D_800F4538[2] = &actor;
    if (reconstructedMatrixProvider(2, 0x40, NULL) != NULL) return 20;
    actor.bones[1] = selectedMatrix;
    actor.bones[1].m[0][0] = 5000;
    actor.bones[1].m[1][0] = 6000;
    actor.bones[1].m[2][0] = 7000;
    actor.bones[1].m[0][1] = 8000;
    actor.bones[1].m[1][1] = 9000;
    actor.bones[1].m[2][1] = 10000;
    actor.bones[1].m[0][2] = 11000;
    actor.bones[1].m[1][2] = 12000;
    actor.bones[1].m[2][2] = 13000;
    ((short*)&actor.bones[1])[9] = 0x1357;
    actor.bones[1].t[0] = 0x1000A;
    actor.bones[1].t[1] = -11;
    actor.bones[1].t[2] = 12;
    D_800F49B8 = {};
    D_800F49B8.m[0][1] = ONE;
    D_800F49B8.m[1][0] = -ONE;
    D_800F49B8.m[2][2] = ONE;
    D_800F49B8.t[0] = 3;
    D_800F49B8.t[1] = 4;
    D_800F49B8.t[2] = 5;
    MATRIX ignoredDestination = selectedMatrix;
    MATRIX* transformed = reconstructedMatrixProvider(2, 1, &ignoredDestination);
    if (transformed != (MATRIX*)getScratchAddr(0xF0)
        || transformed->t[0] != -15 || transformed->t[1] != -7
        || transformed->t[2] != 7 || ((short*)transformed)[9] != 0x1357
        || ignoredDestination.m[0][0] != ONE || ignoredDestination.t[0] != 0) return 21;
    for (int column = 0; column < 3; ++column) {
        if (transformed->m[0][column] != actor.bones[1].m[1][column]
            || transformed->m[1][column] != -actor.bones[1].m[0][column]
            || transformed->m[2][column] != actor.bones[1].m[2][column]) return 22;
    }
    D_800F49B8.m[0][1] = 32767;
    if (reconstructedMatrixProvider(2, 1, NULL)->m[0][0] != 32767) return 23;
    resetFixture();
    D_800F4538[2] = &actor;
    actor.unk704.unk0[41] = (SVECTOR){-3, 5, -7, 0};
    actor.unk183C = ONE;
    transformed = reconstructedMatrixProvider(2, 0xFF, NULL);
    if (specialMatrixCalls != 1 || specialLocalPosition.vx != -2
        || specialLocalPosition.vy != 2 || specialLocalPosition.vz != -4
        || transformed->t[0] != -21 || transformed->t[1] != 22
        || transformed->t[2] != -23) return 24;
    resetFixture();
    actor.unk0.unkF = 7;
    actor.unk1848.unk6 = -123;
    actor.unk1848.unk10 = (SVECTOR){9, -9, 4, 0};
    actor.unk1848.unk8 = 0x00800000;
    actor.unk0.position = (SVECTOR){-129, 20, 32767, 0};
    actor.unk0.unk5D = 93;
    func_800A2790(&actor);
    if (actor.unk0.position.vx != -125 || actor.unk0.position.vy != 16
        || actor.unk0.position.vz != -32767 || actor.unk0.currentTileX != 0
        || actor.unk0.currentTileZ != (u_char)-255 || actor.unk0.unk5D != 93
        || !actor.unk0.unkA_5 || callCount != 2
        || calls[0].kind != 4 || calls[0].actorSlot != 7 || calls[0].value != -123
        || calls[0].extra != 12 || calls[1].value != 0x26) return 25;
    resetFixture();
    actor.unk1848.unk10 = (SVECTOR){2, -3, 4, 0};
    actor.unk1848.unk8 = 0x40000000;
    func_800A2790(&actor);
    if (actor.unk0.position.vx != -128 || actor.unk0.position.vy != 64
        || actor.unk0.position.vz != 0 || calls[1].value != 0x28) return 26;
    resetFixture();
    actor.unk1848.unk8 = 11;
    actor.animationId = 0x1F;
    func_800A2790(&actor);
    if (callCount != 1 || calls[0].kind != 4) return 27;
    resetFixture();
    actor.unk1848.unk8 = 0x80000000;
    func_800A2790(&actor);
    if (calls[1].value != 1) return 28;
    resetFixture();
    actor.unk0.unkA_5 = 1;
    actor.unk0.position = (SVECTOR){-129, 100, 255, 0};
    actor.unk1848.unk10 = (SVECTOR){2, 999, 3, 0};
    groundHeight = -3000;
    func_800A25EC(&actor);
    if (callCount != 1 || actor.unk0.unkA_5 || actor.unk0.position.vx != -129
        || queriedX != 2 || queriedZ != 3 || calls[0].value != 1) return 29;
    callCount = 0;
    groundHeight = 149;
    terrainAvailable = false;
    func_800A25EC(&actor);
    if (callCount != 2 || queriedPosition.vx != -127 || queriedPosition.vz != 258
        || queriedPosition.vy != 149 || actor.unk0.position.vx != -129) return 30;
    callCount = 0;
    terrainAvailable = true;
    actor.unk0.unk5D = 99;
    func_800A25EC(&actor);
    if (actor.unk0.position.vx != -127 || actor.unk0.position.vy != 149
        || actor.unk0.position.vz != 258 || actor.unk0.currentTileX != 0
        || actor.unk0.currentTileZ != 2 || actor.unk0.unk5D != 0) return 31;
    resetFixture();
    actor.unk0.unkF = 8;
    actor.unk0.position.vy = 100;
    actor.unk0.unk34 = (SVECTOR){11, 22, 33, 44};
    actor.unk0.unkA_3 = 3;
    actor.unk0.unk9_6 = 1;
    actor.unk0.unkA_6 = 1;
    actor.unk181A = 9;
    groundHeight = 164;
    func_800A25EC(&actor);
    if (actor.unk0.position.vy != 100 || actor.unk0.unkA_0 != 3
        || actor.unk0.unkA_3 != 0 || actor.unk0.unk9_6 != 0 || !actor.unk0.unkA_6
        || actor.unk181A != 0 || actor.unk0.unk34.vx || actor.unk0.unk34.vy
        || actor.unk0.unk34.vz || actor.unk0.unk34.pad != 44
        || callCount != 3 || calls[2].kind != 1 || calls[2].value != 0x2F) return 32;
    resetFixture();
    actor.unk0.position.vy = 100;
    groundHeight = 163;
    func_800A25EC(&actor);
    if (actor.unk0.position.vy != 163 || actor.unk0.unkA_0 != 0 || callCount != 2) return 33;
    resetFixture();
    actor.unk0.unkA_0 = 2;
    actor.unk0.unkF = 3;
    actor.unk1848.unk10 = (SVECTOR){129, 0, -129, 0};
    groundHeight = 800;
    activeMovementY = -12;
    func_800A25EC(&actor);
    if (actor.unk0.position.vy != -12 || actor.unk0.currentTileX != 1
        || actor.unk0.currentTileZ != 255 || callCount != 3
        || calls[2].kind != 7 || calls[2].actorSlot != 3
        || calls[2].value != 129 || calls[2].extra != -129) return 34;
    resetFixture();
    u_int projection[3] = {11, 22, 33};
    if (func_800A141C(2, 1, projection, 0) != -1 || projection[0] != 11
        || projection[1] != 22 || projection[2] != 33) return 35;
    D_800F4538[2] = &actor;
    selectedMatrix.t[0] = 100;
    selectedMatrix.t[1] = 50;
    selectedMatrix.t[2] = 1000;
    actor.bones[1] = selectedMatrix;
    gte_set_ctrl(GTE_C_H, 1000);
    if (func_800A141C(2, 1, projection, 0) != 0
        || (short)projection[0] != 100 || (short)(projection[0] >> 16) != 50
        || projection[1] != 1000 || projection[2] != 33) return 36;
    actorSkeleton.bones[1].unk0 = 9;
    if (func_800A141C(2, 1, projection, 1) != 0 || (short)projection[0] != 91) return 37;
    if (func_800A141C(2, 1, projection, 2) != 0 || (short)projection[0] != 96) return 38;
    D_800F4538[2] = NULL;
    D_800F45E0[2] = &object;
    copyBoneMatrix((MATRIX*)&object.unk6C[1], &selectedMatrix);
    objectSkeleton.bones[1].unk0 = 20;
    if (func_800A141C(2, 1, projection, 1) != 0 || (short)projection[0] != 80
        || projection[1] != 1000) return 39;
    selectedMatrix.t[0] = 32767;
    selectedMatrix.t[2] = 70000;
    copyBoneMatrix((MATRIX*)&object.unk6C[1], &selectedMatrix);
    if (func_800A141C(2, 1, projection, 0) != 0 || projection[1] != 65535
        || (gte_get_ctrl(GTE_C_FLAG) & GTE_FLAG_SZ3_SAT) == 0) return 40;
    resetFixture();
    u_char targetDescriptor[4] = {1, 2, 2, 0xA5};
    actor.unk0.unkA_0 = 3;
    actor.unk0.unk11 = 88;
    func_800A208C(&actor, targetDescriptor, 20, 0);
    if (callCount != 0 || actor.unk0.unk11 != 88) return 41;
    resetFixture();
    actor.unk0.unkF = 2;
    actor.unk5B9 = 30;
    actor.unk5AC_25 = 3;
    actor.unk5EC.pad = 0x55AA;
    func_800A208C(&actor, targetDescriptor, 20, 0x18001);
    if (destinationKind != 2 || actor.unk0.unk11 != 6 || actor.unk5AC_25 != 1
        || actor.unk0.unk34.vx != 17 || actor.unk0.unk34.vy != -18
        || actor.unk0.unk34.vz != 5 || actor.unk1814 != 192 || actor.unk1818 != 64
        || actor.unk1816 != 11 || actor.unk181A != 2 || actor.unk0.unkA_0 != 2
        || ((u_char*)&actor.unk5EC)[3] != 0xA5 || actor.unk5EC.pad != 0x55AA
        || callCount != 4 || calls[1].kind != 10 || calls[1].value != 12
        || calls[2].kind != 1 || calls[2].value != 0x2F
        || calls[3].kind != 2 || calls[3].value != -32767) return 42;
    resetFixture();
    targetDescriptor[1] = 0;
    func_800A208C(&actor, targetDescriptor, 20, 11);
    if (calls[0].kind != 9 || calls[0].value != 192 || calls[0].extra != 320
        || actor.unk1816 != 17 || actor.unk0.unk34.vx != 11
        || actor.unk0.unk34.vy != -27 || actor.unk0.unk34.vz != 18) return 43;
    resetFixture();
    targetDescriptor[1] = 2;
    resolvedDestination.vy = 10;
    func_800A208C(&actor, targetDescriptor, 20, 11);
    if (actor.unk1816 != 12 || actor.unk0.unk34.vx != 16
        || actor.unk0.unk34.vy != -18 || actor.unk0.unk34.vz != 5
        || callCount != 3 || calls[1].value != 0x2F) return 44;
    resetFixture();
    actor.unk0.unkA_6 = 1;
    resolvedDestination.vy = 300;
    func_800A208C(&actor, targetDescriptor, 17, 11);
    if (actor.unk1816 != 35 || actor.unk0.unk34.vx != 5
        || actor.unk0.unk34.vy != -6 || actor.unk0.unk34.vz != 1
        || actor.unk5AC_25 != 2 || calls[1].value != 0x23) return 45;
    resetFixture();
    resolvedDestination = (SVECTOR){32000, 10, -32000, 0};
    func_800A208C(&actor, targetDescriptor, 20, 11);
    if (actor.unk1816 != 1600 || actor.unk0.unk34.vx != 20
        || actor.unk0.unk34.vz != -20 || actor.unk0.unk34.vy != -2403) return 46;
    resetFixture();
    if (func_800A152C(2, 0xFF, 0) != -1 || func_800A1648(2, 0, 0) != -1) return 47;
    D_800F4538[2] = &actor;
    actor.unk0.nBones = 3;
    actorSkeleton.bones[0].unk6 = 5;
    actorSkeleton.bones[1].unk6 = 7;
    actorSkeleton.bones[2].unk7 = 0x21;
    actorSkeleton.bones[2].unk8 = 9;
    if (func_800A152C(2, 0x107, 0) != 1 || func_800A152C(2, 1, 1) != 2
        || func_800A152C(2, 0, 2) != 2 || func_800A152C(2, 9, 3) != 2
        || func_800A152C(2, 0xFF, 4) != 0 || func_800A152C(2, 33, 0) != -4) return 48;
    if (func_800A1648(2, 0, 0) != 5 || func_800A1648(2, 2, 1) != 2
        || func_800A1648(2, 2, 2) != 1 || func_800A1648(2, 0, 1) != -3
        || func_800A1648(2, 0, 2) != -3) return 49;
    D_800F4538[2] = NULL;
    D_800F45E0[2] = &object;
    object.unk0 = 3;
    objectSkeleton.bones[1].unk6 = 4;
    if (func_800A152C(2, 4, 0) != 1 || func_800A1648(2, 1, 0) != 4) return 50;
    resetFixture();
    D_800F4538[2] = &actor;
    actor.unk0.position = (SVECTOR){11, 400, 22, 99};
    actor.unk646 = 100;
    SVECTOR positionOutput;
    if (func_800A190C(2, 0xFB, &positionOutput, 0) != 0
        || positionOutput.vy != 300 || positionOutput.pad != 99) return 51;
    if (func_800A190C(2, 0xFD, &positionOutput, 0) != 0 || positionOutput.vy != 400) return 52;
    actor.unk65A = -3;
    actor.unk183C = ONE / 2;
    if (func_800A190C(2, 0xFA, &positionOutput, 0) != 0 || positionOutput.vy != 399) return 53;
    D_800F4538[2] = NULL;
    D_800F45E0[2] = &object;
    object.unk1E = 400;
    if (func_800A190C(2, 0xFB, &positionOutput, 0) != 0 || positionOutput.vy != 208) return 54;
    resetFixture();
    D_800F4538[2] = &actor;
    actor.unk0.nBones = 3;
    actorSkeleton.bones[1].unk6 = 7;
    actorSkeleton.bones[1].unk7 = 0x21;
    actor.bones[1] = selectedMatrix;
    actor.bones[1].t[0] = 31;
    u_short wrapperOutput[4] = {0, 0, 0, 77};
    if (func_800A1AF8(2, 1, wrapperOutput, 0) != 0 || wrapperOutput[0] != 31
        || func_800A1B28(2, 1, wrapperOutput, 0) != 0 || wrapperOutput[0] != 31
        || func_800A1B9C(2, 0, wrapperOutput, 0) != 0 || wrapperOutput[0] != 31
        || func_800A1B28(2, 44, wrapperOutput, 0) != -4 || wrapperOutput[3] != 77) return 55;
    MATRIX matrixOutput;
    int boneLength = 123;
    actorSkeleton.bones[1].unk0 = 17;
    if (func_800A17BC(2, 1, &matrixOutput, &boneLength) != 0
        || boneLength != 17 || matrixOutput.t[0] != 31) return 56;
    actor.bones[0] = selectedMatrix;
    actor.unk0.position.vy = -88;
    if (func_800A1720(2, 0xFD, (int*)&matrixOutput, &boneLength) != 0
        || matrixOutput.t[1] != -88) return 57;
    resetFixture();
    D_800F4538[2] = &actor;
    actor.unk0.facing = 0xFF0;
    func_800A249C(2, 0x10);
    if (actor.unk1848.unk0 != 6 || callCount != 1
        || calls[0].value != 0x11 || calls[0].extra != 10) return 58;
    callCount = 0;
    actor.unk0.facing = 0x10;
    func_800A249C(2, 0xFF0);
    if (callCount != 1 || calls[0].value != 0x12) return 59;
    callCount = 0;
    actor.unk0.unkA_6 = 1;
    func_800A249C(2, 0x40);
    if (calls[0].value != 0x1F) return 60;
    callCount = 0;
    actor.unk0.unkA_5 = 1;
    func_800A249C(2, 0x40);
    if (callCount != 0) return 61;
    actor.unk0.unkA_5 = 0;
    func_800A2574(2, -100);
    if (callCount != 2 || calls[0].kind != 2 || calls[0].value != -100
        || calls[1].kind != 3 || calls[1].value != 3 || actor.unk0.unkB_4 != 6) return 62;
    callCount = 0;
    actor.unk0.position = (SVECTOR){32767, 10, -129, 42};
    actor.unk1848.unk10 = (SVECTOR){2, 3, 4, 99};
    func_800A291C(&actor);
    if (actor.unk0.position.vx != -32767 || actor.unk0.position.vy != 13
        || actor.unk0.position.vz != -125 || actor.unk0.position.pad != 42
        || actor.unk0.currentTileX != (u_char)-255 || callCount != 1
        || calls[0].kind != 4 || calls[0].extra != 12) return 63;
    puts("Battle unit: 63 behavior groups passed; all 17 routines exercised.");
    return 0;
}

#undef __attribute__
extern "C" __declspec(dllimport) void* __stdcall VirtualAlloc(void*, unsigned long,
    unsigned long, unsigned long);
void* mapTestScratchpad()
{
    return VirtualAlloc((void*)0x1F800000, 4096, 0x3000, 4);
}