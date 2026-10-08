#include "common.h"
#include "3A1A0.h"
#include <libgpu.h>

void func_8009DF3C(int, int);
int func_800A152C(int, int, int);
int func_800A17BC(int, int, void*, int*);
int func_800A1C10(int actorIndex, int bone, u_short* position, int offsetMode);
MATRIX* func_800A1DE8(int, int, MATRIX*);
void func_800A9EB4(int, int, int);

extern MATRIX D_800F49B8;

/* The second assembly block stores the projected SXY2 and SZ3 through screenPosition ($a2). */
int func_800A141C(int actorIndex, int bone, int screenPosition, int offsetMode)
{
    SVECTOR point;
    MATRIX* boneMatrix;
    D_800F4538_unk68* model;
    D_800F4538_t* actor = vs_battle_actorModels[actorIndex];

    if (actor == NULL) {
        actor = (D_800F4538_t*)D_800F45E0[actorIndex];
        if (actor == NULL) {
            return -1;
        }
    }

    boneMatrix = &actor->bones[bone];
    model = actor->unk0.model;

    __asm__ volatile("lw $t4,0(%0);"
                     "lw $t5,4(%0);"
                     "ctc2 $t4, $0;"
                     "ctc2 $t5, $1;"
                     "lw $t4,8(%0);"
                     "lw $t5,0xc(%0);"
                     "lw $t6,0x10(%0);"
                     "ctc2 $t4, $2;"
                     "ctc2 $t5, $3;"
                     "ctc2 $t6, $4;"
                     "lw $t4,0x14(%0);"
                     "lw $t5,0x18(%0);"
                     "ctc2 $t4, $5;"
                     "lw $t6,0x1c(%0);"
                     "ctc2 $t5, $6;"
                     "ctc2 $t6, $7"
        :
        : "r"(boneMatrix));

    if (offsetMode == 0) {
        point.vx = 0;
    } else {
        if (offsetMode == 1) {
            point.vx = -(u_short)model->armatures[bone].length;
        } else {
            point.vx = -model->armatures[bone].length / 2;
        }
    }

    point.vy = 0;
    point.vz = 0;
    point.pad = 0;

    __asm__ volatile("lwc2	$0,0(%0);"
                     "lwc2	$1,4(%0);"
                     "nop;"
                     "nop;"
                     "rtps;"
                     "swc2 $14,0x0($a2);"
                     "addu $v0,$a2,4;"
                     "swc2 $19,0x0($v0);"
        :
        : "r"(&point));

    return 0;
}

int func_800A152C(int actorIndex, int key, int field)
{
    D_800F4538_t* actor;
    D_800F4538_unk68* model;
    int i;
    int nBones;
    int matched;

    actor = vs_battle_actorModels[actorIndex];

    if (actor == NULL) {
        actor = (D_800F4538_t*)D_800F45E0[actorIndex];
        if (actor == NULL) {
            return -1;
        }
    }

    nBones = actor->unk0.nBones;
    model = actor->unk0.model;

    do {
        if (key == 0xFF) {
            return 0;
        }
    } while (0);

    for (i = 0; i < nBones; ++i) {
        switch (field) {
        case 0:
            if (model->armatures[i].unk6 == (key & 0xFF)) {
                return i;
            }
            break;

        case 1:
            matched = (model->armatures[i].unk7 >> 4) == ((key & 0xFF) + 1);
            if (matched) {
                return i;
            }
            break;

        case 2:
            if ((model->armatures[i].unk7 & 0xF) == ((key & 0xFF) + 1)) {
                return i;
            }
            break;

        case 3:
            if (model->armatures[i].unk8 == (key & 0xFF)) {
                return i;
            }
            break;
        }
    }

    return -4;
}

int func_800A1648(int actorIndex, int bone, int field)
{
    D_800F4538_t* actor;
    D_800F4538_unk68* model;
    u_int value;

    actor = vs_battle_actorModels[actorIndex];
    if (actor == NULL) {
        actor = (D_800F4538_t*)D_800F45E0[actorIndex];
        if (actor == NULL) {
            return -1;
        }
    }
    model = actor->unk0.model;
    switch (field) {
    case 0:
        value = model->armatures[bone].unk6;
        break;
    case 1:
        value = model->armatures[bone].unk7 >> 4;
        if (value == 0) {
            return -3;
        }
        break;
    case 2:
        value = model->armatures[bone].unk7 & 0xF;
        if (value == 0) {
            return -3;
        }
        break;
    }
    return value;
}

int func_800A1720(int actorIndex, int bone, int* matrix, int* length)
{
    int result = bone;

    if (bone == 0xFD) {
        result = 0;
    }

    result = func_800A17BC(actorIndex, result, matrix, length);

    if (result < 0) {
        return result;
    }

    if (bone == 0xFD) {
        matrix[6] = vs_battle_actorModels[actorIndex]->unk0.position.vy;
    }

    return 0;
}

int func_800A17BC(int actorIndex, int bone, void* matrix, int* length)
{
    D_800F4538_t* actor;
    D_800F4538_t* object;
    vs_battle_wepModels_t* weaponModel;
    int _[22] __attribute__((unused));

    *(MATRIX*)matrix = *func_800A1DE8(actorIndex, bone, &D_800F49B8);

    actor = vs_battle_actorModels[actorIndex];
    if (actor == NULL) {
        object = (D_800F4538_t*)D_800F45E0[actorIndex];
        if (object == NULL) {
            return 0;
        }
        *length = object->unk0.model->armatures[bone].length;
    } else if (bone == 0xFF) {
        *length = 0;
    } else if ((bone & 0xF0) == 0x40) {
        weaponModel = vs_battle_wepModels[actorIndex * 2];
        if (weaponModel == NULL) {
            return 0;
        }
        *length = ((D_800F4538_unk68*)weaponModel->offsets)->armatures[bone - 0x3F].length;
    } else {
        *length = actor->unk0.model->armatures[bone].length;
    }
    return 0;
}

int func_800A190C(int actorIndex, int selector, SVECTOR* position, int offsetMode)
{
    D_800F4538_t* actor;
    int value;

    if (selector == 0xFB) {
        actor = vs_battle_actorModels[actorIndex];

        if (actor == NULL) {
            actor = (D_800F4538_t*)D_800F45E0[actorIndex];
            if (actor == NULL) {
                return -1;
            }

            *position = actor->unk0.position;
            position->vy -= 0xC0;
        } else {
            *position = actor->unk0.position;
            position->vy -= actor->unk646;
        }

        return 0;
    }

    if (selector == 0xFD) {
        *position = vs_battle_actorModels[actorIndex]->unk0.position;
        return 0;
    }

    if ((selector & 0xF0) == 0x40) {
        value = selector;
    } else if (selector == 0xFA) {
        actor = vs_battle_actorModels[actorIndex];
        *position = actor->unk0.position;
        value = actor->targetSphereOffsetY * actor->unk183C / ONE;
        position->vy += value;
        return 0;
    } else {
        value = func_800A152C(actorIndex, selector, 0);

        if (value < 0) {
            return value;
        }
    }

    value = func_800A1C10(actorIndex, value, (u_short*)position, offsetMode);

    if (value < 0) {
        return value;
    }

    return 0;
}

int func_800A1AF8(int actorIndex, int bone, u_short* position, int offsetMode)
{
    int result = func_800A1C10(actorIndex, bone, position, offsetMode);

    if (result < 0) {
        return result;
    }

    return 0;
}

int func_800A1B28(int actorIndex, int key, u_short* position, int offsetMode)
{
    int result = func_800A152C(actorIndex, key, 1);

    if (result < 0) {
        return result;
    }

    result = func_800A1C10(actorIndex, result, position, offsetMode);

    if (result < 0) {
        return result;
    }

    return 0;
}

int func_800A1B9C(int actorIndex, int limb, u_short* position, int offsetMode)
{
    int result = func_800A152C(actorIndex, limb, 2);

    if (result < 0) {
        return result;
    }

    result = func_800A1C10(actorIndex, result, position, offsetMode);

    if (result < 0) {
        return result;
    }

    return 0;
}

INCLUDE_ASM("build/src/BATTLE/BATTLE.PRG/nonmatchings/38C1C", func_800A1C10);

INCLUDE_ASM("build/src/BATTLE/BATTLE.PRG/nonmatchings/38C1C", func_800A1DE8);

INCLUDE_ASM("build/src/BATTLE/BATTLE.PRG/nonmatchings/38C1C", func_800A208C);

void vs_battle_animateCharacter(int, int, int, int);

void func_800A249C(int actorIndex, int facing)
{
    int animation;
    int delta;
    int current;
    D_800F4538_t* actor = vs_battle_actorModels[actorIndex];

    if (actor->unk0.unkA_5) {
        return;
    }

    delta = facing - actor->unk0.facing;

    if (delta > 0x800) {
        delta -= 0x1000;
    }

    if (delta < -0x800) {
        delta += 0x1000;
    }

    if (delta == 0) {
        return;
    }

    actor->movement.mode = 6;

    current = actor->animationId;

    if ((current == 0x76) || (actor->animationId == 0x75) || actor->unk0.unkC_0) {
        return;
    }

    animation = 0x1F;

    if (!(actor->unk0.unkA_6)) {

        animation = 0x11;

        if (delta < 0) {
            animation = 0x12;
        }
    }

    vs_battle_animateCharacter(actorIndex, animation, 0, 0xA);
}

void func_800A2574(int actorIndex, short facing)
{
    D_800F4538_t* actor = vs_battle_actorModels[actorIndex];
    func_800A9EB4(actorIndex, facing, 8);
    func_8009DF3C(actorIndex, 3);
    actor->unk0.unkB_4 = 6;
}

int func_800A6EE8(SVECTOR*, int, int, int);
void* func_800A8D64(SVECTOR*, int);
void func_800AC690(int, D_800F4538_t*);

void func_800A25EC(D_800F4538_t* actor)
{
    SVECTOR destination;
    u_int flags;

    /* Offset 8 is the original 32-bit word holding unk0 flags unk9_6, unkA_0, unkA_3 and unkA_5. */
    ((u_int*)actor)[2] &= ~0x200000;
    destination.vx = actor->movement.unk10.vx;
    destination.vy = 0;
    destination.vz = actor->movement.unk10.vz;
    destination.vy = func_800A6EE8(&actor->unk0.position, destination.vx, destination.vz, 1);
    if (destination.vy == -3000) {
        return;
    }

    destination.vx += actor->unk0.position.vx;
    destination.vz += actor->unk0.position.vz;
    if (func_800A8D64(&destination, 0) == NULL) {
        return;
    }

    actor->unk0.position.vx = destination.vx;
    actor->unk0.position.vz = destination.vz;
    if (((u_int*)actor)[2] & 0x70000) {
        func_800AC690(actor->unk0.index, actor);
    } else {
        short height = destination.vy;

        if (height > actor->unk0.position.vy && height - actor->unk0.position.vy >= 64) {
            setVector(&actor->unk0.unk34, 0, 0, 0);
            vs_battle_animateCharacter(actor->unk0.index, 0x2F, 0, 4);
            actor->unk181A = 0;
            flags = ((u_int*)actor)[2];
            flags &= ~0x180000;
            flags &= ~0x4000;
            flags &= ~0x70000;
            flags |= 0x30000;
            ((u_int*)actor)[2] = flags;
        } else {
            actor->unk0.position.vy = height;
        }
    }

    actor->unk0.currentTileX = actor->unk0.position.vx / 128;
    actor->unk0.currentTileZ = actor->unk0.position.vz / 128;
    actor->unk0.unk5D = 0;
}

void func_800AA850(int, int, int);

void func_800A2790(D_800F4538_t* actor)
{
    VECTOR displacement;
    int scale = actor->movement.unk8;
    int animation;

    displacement.vx = actor->movement.unk10.vx * scale / 0x1000000;
    displacement.vy = actor->movement.unk10.vy * scale / 0x1000000;
    displacement.vz = actor->movement.unk10.vz * scale / 0x1000000;
    addVector(&actor->unk0.position, &displacement);

    actor->unk0.currentTileX = actor->unk0.position.vx / 128;
    actor->unk0.currentTileZ = actor->unk0.position.vz / 128;
    /* Sets unk0.unkA_5 through the 32-bit flag word the original code writes. */
    ((u_int*)actor)[2] |= 0x200000;

    func_800AA850(actor->unk0.index, actor->movement.facing, 12);

    animation = 1;
    if (scale > 0) {
        if (displacement.vy < -2) {
            animation = 0x20;
        } else if (displacement.vy > 2) {
            animation = 0x22;
        } else {
            animation = 0x1F;
        }
        if (scale >= 12) {
            animation += 6;
        }
    }

    if (actor->animationId != animation) {
        vs_battle_animateCharacter(actor->unk0.index, animation, 0, 4);
    }
}

void func_800A291C(D_800F4538_t* actor)
{
    addVector(&actor->unk0.position, &actor->movement.unk10);

    actor->unk0.currentTileX = actor->unk0.position.vx / 128;
    actor->unk0.currentTileZ = actor->unk0.position.vz / 128;

    func_800AA850(actor->unk0.index, actor->movement.facing, 12);
}
