// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/Skeleton.h"
#include "Battle/BattleTypes.h"
#include "Battle/Feedback.h"
#include "Battle/ValueLabels.h"
#include "Core/Math/Vector.h"
#include "Core/Memory/Heap.h"
#include "Field/Gimmicks.h"
#include "Field/RoomRendering.h"
#include "Input/Controller.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/Primitives.h"
#include "Render/Render.h"
#include "Render/TextureAnimation.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Settings.h"
#include "State/Timers.h"

// decomp: D_800E861C (D_800E861C@BATTLE/BATTLE.PRG)
int Battle_valueCaptionRects[17]; /* data: contents loaded in Step 5 */

// decomp: D_800E8634 (D_800E8634@BATTLE/BATTLE.PRG)
short Battle_lockedLabelHalfHeights[22]; /* data: contents loaded in Step 5 */

// decomp: D_800E8640 (D_800E8640@BATTLE/BATTLE.PRG)
RECT Battle_lockedLabelPieces[4]; /* data: contents loaded in Step 5 */

// decomp: D_800F1BAC (D_800F1BAC@BATTLE/BATTLE.PRG)
Battle_FloatingLabel (*Battle_floatingLabels)[32];

// decomp: D_800F1CBC (D_800F1CBC@BATTLE/BATTLE.PRG)
DVECTOR* Battle_placedLabelPositions;

// decomp: D_800F2260 (D_800F2260@BATTLE/BATTLE.PRG)
char Battle_actorMarkers[40];

// decomp: D_800F2270 (D_800F2270@BATTLE/BATTLE.PRG)
short Battle_actorMarkersHidden;

// decomp: D_800F2272 (D_800F2272@BATTLE/BATTLE.PRG)
short Battle_abilityTimingCueFrames;

// decomp: D_800F2274 (D_800F2274@BATTLE/BATTLE.PRG)
int Battle_actorMarkerSides;

// decomp: D_800F2278 (D_800F2278@BATTLE/BATTLE.PRG)
short Battle_statusLabelStepTimer;

// decomp: D_800F227A (D_800F227A@BATTLE/BATTLE.PRG)
short Battle_ashleyStatusLabelsSuppressed;

// decomp: D_800F227C (D_800F227C@BATTLE/BATTLE.PRG)
short Battle_placedLabelCount;

// decomp: D_800F227E (D_800F227E@BATTLE/BATTLE.PRG)
short Battle_floatingLabelCount;

// decomp: D_800F2280 (D_800F2280@BATTLE/BATTLE.PRG)
SVECTOR Battle_abilityTimingCuePosition;

// decomp: D_80068F04 (D_80068F04@src/BATTLE/BATTLE.PRG/2842C.c)
int const Battle_kMarkerFrameCorners[][4] = { { vs_getXY(-6, -22), vs_getXY(18, -22),
                                         vs_getXY(-6, 1), vs_getXY(18, 1) },
    { vs_getXY(-17, -22), vs_getXY(7, -22), vs_getXY(-17, 1), vs_getXY(7, 1) },
    { vs_getXY(-10, -29), vs_getXY(20, -29), vs_getXY(-10, 1), vs_getXY(20, 1) },
    { vs_getXY(-19, -29), vs_getXY(9, -29), vs_getXY(-19, 1), vs_getXY(9, 1) },
    { vs_getXY(-9, -16), vs_getXY(11, -16), vs_getXY(-9, -3), vs_getXY(11, -3) },
    { vs_getXY(-10, -16), vs_getXY(10, -16), vs_getXY(-10, -3), vs_getXY(10, -3) } };

// decomp: D_80068F64 (D_80068F64@src/BATTLE/BATTLE.PRG/2842C.c)
u_short const Battle_kMarkerFrameTexCoords[][4] = { { vs_getUV(64, 16), vs_getUV(88, 16),
                                             vs_getUV(64, 39), vs_getUV(88, 39) },
    { vs_getUV(87, 16), vs_getUV(63, 16), vs_getUV(87, 39), vs_getUV(63, 39) },
    { vs_getUV(121, 17), vs_getUV(151, 17), vs_getUV(121, 47), vs_getUV(151, 47) },
    { vs_getUV(150, 17), vs_getUV(120, 17), vs_getUV(150, 47), vs_getUV(120, 47) },
    { vs_getUV(160, 17), vs_getUV(180, 17), vs_getUV(160, 30), vs_getUV(180, 30) },
    { vs_getUV(179, 17), vs_getUV(159, 17), vs_getUV(179, 30), vs_getUV(159, 30) } };

// decomp: D_80068F94 (D_80068F94@src/BATTLE/BATTLE.PRG/2842C.c)
int const Battle_kMarkerIconOffsets[] = { vs_getXY(-2, -16), vs_getXY(-14, -16), vs_getXY(0, -21),
    vs_getXY(-7, -21), vs_getXY(3, -11), vs_getXY(3, -11) };

// decomp: D_80068FAC (D_80068FAC@src/BATTLE/BATTLE.PRG/2842C.c)
RECT const Battle_kLabelCaptionRects[] = { { 0, 64, 36, 12 }, { 0, 64, 24, 12 },
    { 24, 64, 20, 12 }, { 44, 64, 24, 12 }, { 224, 64, 16, 12 }, { 212, 76, 32, 12 },
    { 150, 64, 26, 12 }, { 68, 64, 40, 12 }, { 108, 64, 30, 12 }, { 138, 64, 50, 12 },
    { 188, 64, 36, 12 }, { 0, 76, 52, 12 }, { 52, 76, 30, 12 }, { 144, 88, 58, 12 },
    { 120, 76, 60, 12 }, { 180, 76, 30, 12 }, { 136, 88, 8, 12 }, { 104, 88, 32, 12 },
    { 76, 88, 40, 12 }, { 0, 88, 16, 12 }, { 20, 88, 20, 12 }, { 40, 88, 30, 12 },
    { 72, 88, 32, 12 }, { 102, 76, 16, 12 }, { 220, 100, 18, 12 }, { 152, 52, 26, 12 },
    { 182, 40, 30, 12 }, { 214, 40, 24, 12 }, { 152, 52, 28, 12 }, { 180, 52, 40, 12 },
    { 220, 52, 30, 12 }, { 108, 64, 30, 12 }, { 216, 28, 28, 12 } };

// decomp: D_800690B4 (D_800690B4@BATTLE/BATTLE.PRG)
struct Battle_StatusLabelLayout const Battle_kStatusLabelLayouts[] = { { 1, { 31, 0 }, 0 }, { 2, { 29, 27 }, 0 }, { 2, { 30, 27 }, 0 },
    { 2, { 28, 26 }, 0 }, { 2, { 32, 27 }, 0 }, { 2, { 1, 5 }, 0 }, { 2, { 1, 4 }, 1 },
    { 2, { 2, 5 }, 0 }, { 2, { 2, 4 }, 1 }, { 2, { 3, 5 }, 0 }, { 2, { 3, 4 }, 1 },
    { 1, { 7, 0 }, 2 }, { 1, { 8, 0 }, 2 }, { 1, { 9, 0 }, 2 }, { 1, { 10, 0 }, 2 },
    { 1, { 11, 0 }, 2 }, { 1, { 12, 0 }, 2 }, { 1, { 13, 0 }, 3 }, { 1, { 14, 0 }, 3 },
    { 2, { 15, 5 }, 0 }, { 2, { 15, 4 }, 1 }, { 2, { 16, 19 }, 2 }, { 2, { 16, 20 }, 2 },
    { 2, { 16, 21 }, 2 }, { 2, { 16, 22 }, 2 }, { 2, { 17, 19 }, 2 },
    { 2, { 17, 20 }, 2 }, { 2, { 17, 21 }, 2 }, { 2, { 17, 22 }, 2 }, { 0, { 0, 0 }, 0 },
    { 0, { 0, 0 }, 0 }, { 0, { 0, 0 }, 0 } };

// decomp: D_80069134 (D_80069134@src/BATTLE/BATTLE.PRG/2842C.c)
CVECTOR const Battle_kStatusLabelColors[] = { { 208, 208, 208 }, { 64, 192, 255 },
    { 255, 208, 48 }, { 160, 224, 96 } };

// decomp: D_80069144 (D_80069144@src/BATTLE/BATTLE.PRG/2842C.c)
RECT const Battle_kStatChangeFirstWordRects[] = { { 72, 160, 32, 8 }, { 76, 168, 28, 8 },
    { 78, 176, 26, 8 }, { 78, 184, 26, 8 }, { 82, 192, 22, 8 }, { 82, 200, 22, 8 },
    { 82, 208, 22, 8 } };

// decomp: D_8006917C (D_8006917C@src/BATTLE/BATTLE.PRG/2842C.c)
RECT const Battle_kStatChangeSecondWordRects[] = { { 104, 160, 28, 8 }, { 104, 168, 26, 8 },
    { 104, 176, 30, 8 }, { 104, 184, 36, 8 }, { 104, 192, 32, 8 }, { 104, 200, 18, 8 },
    { 144, 160, 30, 8 }, { 144, 168, 14, 8 }, { 144, 176, 18, 8 }, { 144, 184, 26, 8 },
    { 144, 192, 26, 8 }, { 144, 200, 22, 8 }, { 144, 208, 36, 8 } };

// decomp: BATTLE.PRG func_80093AB4 (src/BATTLE/BATTLE.PRG/2842C.c:1465)
void Battle_ResetFloatingLabels(void)
{
    Battle_actorMarkersHidden = 0;
    Battle_abilityTimingCueFrames = 0;
    Battle_floatingLabelCount = 0;
    Battle_statusLabelStepTimer = 0;
    Battle_ashleyStatusLabelsSuppressed = 0;
    Core_Memory_ZeroAlignedWords(Battle_floatingLabels, sizeof *Battle_floatingLabels);
}

// decomp: BATTLE.PRG func_80093B04 (src/BATTLE/BATTLE.PRG/2842C.c:1475)
int Battle_QueueWorldLockedLabel(u_short* arg0)
{
    Battle_FloatingLabelSpec* temp_v0;

    temp_v0 = ((void* (*)())Battle_ReserveFloatingLabel)();
    temp_v0->opaqueB = 0;
    temp_v0->opaque8 = 3;
    temp_v0->opaque0 = arg0[0];
    temp_v0->opaque2 = arg0[1];
    temp_v0->opaque4 = arg0[2];
    ++Battle_floatingLabelCount;
    return 0;
}

// decomp: BATTLE.PRG func_80093B68 (src/BATTLE/BATTLE.PRG/2842C.c:1544)
int Battle_QueueActorValueLabel(int arg0, int arg1, u_int arg2, int arg3)
{
    SVECTOR position;
    int limb;
    int i;
    int count;
    int frame;
    int flags;
    int result;
    int amount;
    Battle_FloatingLabel* p;

    if (arg1 != -1) {
        limb = Actors_FindActorBoneTag(arg0, arg1, 2);

        if (limb < 0) {
            return -2;
        }
    } else {
        limb = 255;
    }

    i = 0;
    count = 0;
    frame = 0;

    for (p = *Battle_floatingLabels; i < Battle_floatingLabelCount; ++i, ++p) {
        if ((p->key.bytes[0] == 1) && (p->key.bytes[1] == arg0) && (p->position.pad >= 0)) {
            frame = p->frame;
            ++count;
        }
    }

    i = 0;
    flags = 0;

    for (p = *Battle_floatingLabels; i < Battle_floatingLabelCount; ++i, ++p) {
        if ((p->key.bytes[0] == 1) && (p->key.bytes[1] == arg0) && (p->limb == limb)
            && (p->position.pad == count - 1)) {
            flags = 128;
            break;
        }
    }

    p = ((void* (*)())Battle_ReserveFloatingLabel)();

    if (limb != 255) {
        result = Actors_ActorBonePosition(arg0, limb, &p->position, 2);
    } else {
        result = Actors_ActorTaggedBonePosition(arg0, 255, &p->position, 2);
    }

    if (result != 0) {
        return -2;
    }

    if (arg3 & 128) {
        if (Actors_ProjectActorTaggedBone(arg0, 251, &position, 0) != 0) {
            return 0;
        }

        if (position.vx > 320u) {
            return 0;
        }

        if (position.vy > 240u) {
            return 0;
        }
    }

    arg3 &= 127;

    if (Actors_ActorTaggedBonePosition(arg0, 251, &position, 0) != 0) {
        return -2;
    }

    p->scaleX = ONE;
    p->scaleY = ONE;
    applyVector(&p->position, position.vx, position.vy, position.vz, -=);
    p->key.bytes[0] = 1;
    p->position.pad = count;
    p->frame = frame;
    p->key.bytes[1] = arg0;
    p->limb = limb;
    p->colorFlags = arg3 | flags;
    amount = arg2 & 0x7FFF;

    if (amount > 999) {
        p->value = 999 | ((arg2 >> 16) & 0x8000);
    } else {
        p->value = amount | ((arg2 >> 16) & 0x8000);
    }

    sprintf(p->text, "%d", p->value & 0x7FFF);
    p->textLength = strlen(p->text);
    ++Battle_floatingLabelCount;
    return 0;
}

// decomp: BATTLE.PRG vs_battle_renderBattleAbilityTimingResult (src/BATTLE/BATTLE.PRG/2842C.c:1643)
int Battle_QueueAbilityTimingResult(int arg0)
{
    int _[2] __attribute__((unused));
    Battle_FloatingLabel* p = *Battle_floatingLabels;
    int i;

    for (i = 0; i < Battle_floatingLabelCount; ++i, ++p) {
        if (p->key.halfword == 4) {
            Battle_RemoveFloatingLabel(i);
            break;
        }
    }

    p = ((void* (*)())Battle_ReserveFloatingLabel)();
    p->frame = 0;
    p->key.bytes[0] = 4;
    p->key.bytes[1] = 0;
    p->value = arg0;
    p->textLength = Core_Random_Rand() & 7;
    ++Battle_floatingLabelCount;
    return 0;
}

// decomp: BATTLE.PRG func_80093F24 (src/BATTLE/BATTLE.PRG/2842C.c:1666)
int Battle_QueueChainCount(int arg0)
{
    Battle_FloatingLabel* p = *Battle_floatingLabels;
    Battle_FloatingLabel* q;
    int i;

    for (i = 0; i < Battle_floatingLabelCount; ++i, ++p) {
        if (p->key.halfword == 5) {
            Battle_RemoveFloatingLabel(i);
            break;
        }
    }

    if (arg0 == 0) {
        return 0;
    }

    q = ((void* (*)())Battle_ReserveFloatingLabel)();
    q->key.bytes[0] = 5;
    q->frame = 0;
    q->key.bytes[1] = 0;
    q->value = arg0;
    if (q->value >= 1000) {
        q->value = 999;
    }
    ++Battle_floatingLabelCount;
    return 0;
}

// decomp: BATTLE.PRG func_80093FEC (src/BATTLE/BATTLE.PRG/2842C.c:1695)
int Battle_QueueActorStatusLabels(int arg0, int arg1, int arg2, int arg3)
{
    int i;
    int var_s1;

    for (i = 0, var_s1 = 1; i < 29; ++i, var_s1 <<= 1) {
        if (arg2 & var_s1) {
            Battle_QueueActorStatusLabel(arg0, 0xFB, i, arg3);
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009406C (src/BATTLE/BATTLE.PRG/2842C.c:1708)
int Battle_QueueActorStatusLabel(int arg0, int arg1, int arg2, int arg3)
{
    SVECTOR sp10;
    int i = 0;
    int found = 0;
    int frame = 0;
    int kind = 0;
    Battle_FloatingLabel* p = *Battle_floatingLabels;

    for (; i < Battle_floatingLabelCount; ++i, ++p) {
        if ((p->key.bytes[0] == 2) && (p->key.bytes[1] == arg0)) {
            frame = p->textLength + 0xC;
            if (p->position.pad >= 0) {
                kind = p->frame;
                ++found;
            }
        }
    }

    p = ((void* (*)())Battle_ReserveFloatingLabel)();
    if (arg3 & 0x80) {
        if (Actors_ProjectActorTaggedBone(arg0, 0xFB, &sp10, 0) != 0) {
            return 0;
        }
        if ((u_short)sp10.vx >= 0x141) {
            return 0;
        }
        if ((u_short)sp10.vy >= 0xF1) {
            return 0;
        }
    }
    arg3 &= 0x7F;
    p->key.bytes[0] = 2;
    setVector(&p->position, 0, 0, 0);
    p->position.pad = found;
    if (frame < 0x25) {
        p->textLength = frame;
    } else {
        p->textLength = 0;
    }
    p->frame = kind;
    p->key.bytes[1] = arg0;
    p->limb = arg1;
    p->colorFlags = arg3;
    p->value = arg2;
    ++Battle_floatingLabelCount;
    return 0;
}

// decomp: BATTLE.PRG func_800941FC (src/BATTLE/BATTLE.PRG/2842C.c:1757)
void Battle_UpdateAndDrawFloatingLabels(void)
{
    long p;
    Battle_FloatingLabel* label;
    int removed;
    int i;

    if (Battle_abilityTimingCueFrames > 0) {
        --Battle_abilityTimingCueFrames;
        Battle_DrawAbilityTimingCue();
    }

    if (Field_areRoomUpdatesPaused == 0) {
        Battle_statusLabelStepTimer += State_tickSpeed / 2;
    }

    if (Battle_statusLabelStepTimer > 20) {
        label = *Battle_floatingLabels;
        Battle_statusLabelStepTimer = 0;

        for (i = 0; i < Battle_floatingLabelCount; ++i, ++label) {
            if (label->key.bytes[0] == 6 && --label->position.pad == -1) {
                for (p = 0; p < i; ++p) {
                    if ((*Battle_floatingLabels)[p].key.bytes[0] == 6) {
                        (*Battle_floatingLabels)[p].scaleY += 10;
                    }
                }
            }
        }
    }

    removed = 0;
    label = *Battle_floatingLabels;
    Battle_placedLabelCount = 0;

    for (i = 0; i < Battle_floatingLabelCount; ++i, ++label) {
        SVECTOR position;
        short screen[2];
        long flag;
        int visible;

        if (label->key.bytes[0] == 0) {
            continue;
        }

        if (label->key.bytes[0] < 3) {
            if (Actors_ActorTaggedBonePosition(label->key.bytes[1], 251, &position, 0) != 0) {
                label->key.bytes[0] = 0;
                ++removed;
                continue;
            }

            if (position.vy > 0) {
                position.vy = 0;
            }

            addVector(&position, &label->position);
            visible = Core_Gte_RotTransPers(&position, (long*)screen, &p, &flag);

            if (screen[0] < 32) {
                screen[0] = 32;
            }

            if (screen[0] > 288) {
                screen[0] = 288;
            }

            if (screen[1] < 40) {
                screen[1] = 40;
            }

            if (screen[1] > 200) {
                screen[1] = 200;
            }
        } else if (label->key.bytes[0] == 3) {
            visible = Core_Gte_RotTransPers(&label->position, (long*)screen, &p, &flag);
        } else {
            visible = 1;
        }

        if (visible <= 0) {
            label->key.bytes[0] = 0;
            ++removed;
            continue;
        }

        switch (label->key.bytes[0]) {
        case 1:
            if (label->position.pad <= 0) {
                label->colorFlags &= 0x7F;
                screen[1] += 8;
                Battle_PlaceActorValueLabel(screen, label);

                if (label->colorFlags == 2) {
                    Battle_DrawLockedFloatingLabel(screen[0], screen[1], label);
                } else {
                    Battle_DrawActorValueLabel(screen[0], screen[1], label);
                }

                label->frame += State_tickSpeed / 2;

                if (label->frame >= 10) {
                    label->position.pad = -1;
                }

                if (label->frame >= 31) {
                    label->key.bytes[0] = 0;
                    ++removed;
                }
                break;
            }

            screen[1] += 8;
            Battle_PlaceActorValueLabel(screen, label);

            if (label->colorFlags & 0x80) {
                label->colorFlags &= 0x7F;
                Battle_AdvanceFloatingLabelQueue(label);
                break;
            }

            label->frame += State_tickSpeed / 2;

            if (label->frame >= 10) {
                Battle_AdvanceFloatingLabelQueue(label);
            }
            break;
        case 2:
            if (label->position.pad <= 0) {
                screen[1] -= label->textLength;

                if (label->key.bytes[1] != 0 || Battle_ashleyStatusLabelsSuppressed == 0) {
                    if (label->colorFlags == 2) {
                        Battle_DrawLockedFloatingLabel(screen[0], screen[1], label);
                    } else {
                        Battle_DrawActorStatusLabel(screen[0], screen[1], label);
                    }
                }

                label->frame += State_tickSpeed / 2;

                if (label->frame >= 10) {
                    label->position.pad = -1;
                }

                if (label->frame >= 31) {
                    label->key.bytes[0] = 0;
                    ++removed;
                }
                break;
            }

            label->frame += State_tickSpeed / 2;

            if (label->frame >= 10) {
                Battle_AdvanceFloatingLabelQueue(label);
            }
            break;
        case 3:
            Battle_DrawLockedFloatingLabel(screen[0], screen[1], label);
            label->frame += State_tickSpeed / 2;

            if (label->frame >= 31) {
                label->key.bytes[0] = 0;
                ++removed;
            }
            break;
        case 4:
            Battle_DrawAbilityTimingResult(44, 48, label);
            label->frame += State_tickSpeed / 2;

            if (label->frame >= 46) {
                label->key.bytes[0] = 0;
                ++removed;
            }
            break;
        case 5:
            Battle_DrawChainCount(44, 64, label);
            label->frame += State_tickSpeed / 2;

            if (label->frame >= 91) {
                label->key.bytes[0] = 0;
                ++removed;
            }
            break;
        case 6:
            if (label->position.pad >= 0) {
                break;
            }

            Battle_DrawEquipmentStatChange(8, 172, label);

            if (Field_areRoomUpdatesPaused != 0) {
                break;
            }

            label->frame += State_tickSpeed / 2;

            if (label->frame >= 61) {
                label->key.bytes[0] = 0;
                ++removed;
            }
            break;
        case 0:
        case 7:
            break;
        }
    }

    if (removed > 0) {
        Battle_CompactFloatingLabels(removed);
    }
}

// decomp: BATTLE.PRG func_80094844 (src/BATTLE/BATTLE.PRG/2842C.c:1971)
void Battle_PlaceActorValueLabel(short* pos, Battle_FloatingLabel* label)
{
    int i;

    if (label->scaleX == ONE && label->scaleY == ONE) {
        label->scaleX = 0;
        label->scaleY = 0;

        if (Battle_placedLabelCount > 0) {
            int x;
            int y;

            for (i = 0; i < Battle_placedLabelCount; ++i) {
                x = pos[0] - Battle_placedLabelPositions[i].vx;
                y = pos[1] - Battle_placedLabelPositions[i].vy;

                if (ABS(x) < 24 && ABS(y) < 10) {
                    break;
                }
            }

            if (i != Battle_placedLabelCount) {
                int j;
                int dx;
                int dy;

                i = 0;

                for (dy = 0; dy < 48; dy += 8) {
                    for (dx = 0; dx < 32; dx += 8) {
                        for (j = 0; j < Battle_placedLabelCount; ++j) {
                            y = pos[1] + dy - Battle_placedLabelPositions[j].vy;

                            if (ABS(y) < 10) {
                                x = pos[0] + dx - Battle_placedLabelPositions[j].vx;

                                if (ABS(x) < 24) {
                                    break;
                                }
                            }
                        }

                        if (j == Battle_placedLabelCount) {
                            i = 1;
                            goto done;
                        }

                        for (j = 0; j < Battle_placedLabelCount; ++j) {
                            y = pos[1] + dy - Battle_placedLabelPositions[j].vy;

                            if (ABS(y) < 10) {
                                x = pos[0] - dx - Battle_placedLabelPositions[j].vx;

                                if (ABS(x) < 24) {
                                    break;
                                }
                            }
                        }

                        if (j == Battle_placedLabelCount) {
                            i = 1;
                            dx = -dx;
                            goto done;
                        }
                    }
                }

            done:
                if (i) {
                    label->scaleX = dx;
                    label->scaleY = dy;
                }
            }
        }
    }

    pos[0] += label->scaleX;
    pos[1] += label->scaleY;
    Battle_placedLabelPositions[Battle_placedLabelCount].vx = pos[0];
    Battle_placedLabelPositions[Battle_placedLabelCount].vy = pos[1];

    if (pos[0] < 32) {
        pos[0] = 32;
    }

    if (pos[0] > 288) {
        pos[0] = 288;
    }

    if (pos[1] > 200) {
        pos[1] = 200;
    }

    ++Battle_placedLabelCount;
}

// decomp: BATTLE.PRG func_80094AF8 (src/BATTLE/BATTLE.PRG/2842C.c:2067)
void Battle_AdvanceFloatingLabelQueue(Battle_FloatingLabel* arg0)
{
    arg0->frame = 0;
    --arg0->position.pad;
}

// decomp: BATTLE.PRG func_80094B0C (src/BATTLE/BATTLE.PRG/2842C.c:2073)
void Battle_DrawActorValueLabel(int x, int y, Battle_FloatingLabel* p)
{
    CVECTOR colors[] = { { 128, 128, 128, 0 }, { 0, 128, 0, 0 }, { 128, 0, 128, 0 },
        { 0, 128, 128, 0 } };
    signed char bounce[] = { 0, -6, -12, -14, -16, -10, -5, -1, 0, -3, -6, -7, -8, -5, -3,
        -1, 0, -2, -1, 0 };
    int frame = 15;
    int color;
    int offset;
    int i;
    POLY_FT4* prim;

    if (p->colorFlags >= 3) {
        if (!(p->value & 0x8000)) {
            y -= p->frame >> 1;
        } else {
            y += p->frame >> 1;
        }

        i = Battle_DrawValueLabelCaption(x, y, p);
        color = 0;
        offset = 0;
    } else {
        if (p->frame < 16) {
            frame = p->frame;
        }

        color = (p->colorFlags & 1) << 1;
        i = p->textLength * 3;

        if (!(p->value & 0x8000)) {
            offset = bounce[frame];
            ++color;
        } else {
            offset = bounce[frame + 4];
        }
    }

    x += i - 6;
    prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;

    for (i = p->textLength - 1; i >= 0; --i, x -= 6) {
        int digitY = y + offset;

        offset /= 2;

        if (x + 8 >= 328u) {
            continue;
        }

        setPolyFT4(prim);
        setXY4(prim, x, digitY - 6, x + 8, digitY - 6, x, digitY + 6, x + 8, digitY + 6);
        setShadeTex(prim, 0);
        setRGB0(prim, colors[color].r, colors[color].g, colors[color].b);
        setUV4(prim, (p->text[i] - '0') * 8 + 64, 0,
            (p->text[i] - '0') * 8 + 72, 0, (p->text[i] - '0') * 8 + 64, 12,
            (p->text[i] - '0') * 8 + 72, 12);
        setTPage(prim, 0, 0, 768, 0);
        setClut(prim, 832, 223);
        AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 20, prim++);
    }

    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
}

// decomp: BATTLE.PRG func_80094E18 (src/BATTLE/BATTLE.PRG/2842C.c:2138)
int Battle_DrawValueLabelCaption(int x, int y, Battle_FloatingLabel* p)
{
    RECT const* rect = &Battle_kLabelCaptionRects[Battle_valueCaptionRects[p->colorFlags - 3]];
    POLY_FT4* prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
    int sign = p->value;
    int width = (p->textLength * 6 + rect->w + 12) / 2;

    x -= width;
    setPolyFT4(prim);
    setXY4(prim, x, y - 6, x + rect->w, y - 6, x, y + 6, x + rect->w, y + 6);
    setShadeTex(prim, 1);
    setUV4(prim, rect->x, rect->y, rect->x + rect->w, rect->y, rect->x, rect->y + 12,
        rect->x + rect->w, rect->y + 12);
    setTPage(prim, 0, 0, 448, 256);
    setClut(prim, 976, 220);
    sign = (sign & 0x8000) ? 0 : 8;
    AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 20, prim++);
    x += rect->w + 4;
    setPolyFT4(prim);
    setXY4(prim, x, y - 5, x + 8, y - 5, x, y + 5, x + 8, y + 5);
    setUV4(prim, sign + 154, 76, sign + 162, 76, sign + 154, 86, sign + 162, 86);
    setTPage(prim, 0, 0, 768, 0);
    setClut(prim, 864, 223);
    setShadeTex(prim, 1);
    AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 20, prim++);
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
    return width;
}

// decomp: BATTLE.PRG func_8009506C (src/BATTLE/BATTLE.PRG/2842C.c:2167)
void Battle_DrawLockedFloatingLabel(int arg0, int arg1, Battle_FloatingLabel* arg2)
{
    POLY_FT4* prim;
    int step;
    int y0;
    int y1;
    int i;

    if (arg2->frame < 4) {
        step = arg2->frame;
    } else if (arg2->frame >= 27) {
        step = 30 - arg2->frame;
    } else {
        step = 4;
    }

    arg0 -= 16;
    y0 = arg1 - Battle_lockedLabelHalfHeights[step];
    y1 = arg1 + Battle_lockedLabelHalfHeights[step];
    prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;

    for (i = 0; i < 4; ++i) {
        if ((arg0 < 320) && ((arg0 + Battle_lockedLabelPieces[i].w) >= 0)) {
            setPolyFT4(prim);
            setXY4(prim, arg0, y0, arg0 + Battle_lockedLabelPieces[i].w, y0, arg0, y1,
                arg0 + Battle_lockedLabelPieces[i].w, y1);
            setShadeTex(prim, 1);
            setUV4(prim, Battle_lockedLabelPieces[i].x, Battle_lockedLabelPieces[i].y,
                Battle_lockedLabelPieces[i].x + Battle_lockedLabelPieces[i].w, Battle_lockedLabelPieces[i].y, Battle_lockedLabelPieces[i].x,
                Battle_lockedLabelPieces[i].y + 10, Battle_lockedLabelPieces[i].x + Battle_lockedLabelPieces[i].w,
                Battle_lockedLabelPieces[i].y + 10);
            setSemiTrans(prim, 1);
            setTPage(prim, 0, 0, 768, 0);
            setClut(prim, 864, 223);
            AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 20, prim++);
        }

        arg0 += Battle_lockedLabelPieces[i].w;
    }

    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
}

// decomp: BATTLE.PRG func_80095258 (src/BATTLE/BATTLE.PRG/2842C.c:2210)
void Battle_DrawActorStatusLabel(int x, int y, Battle_FloatingLabel* p)
{
    short slide[] = { 32, 14, 6, 2 };
    short fade[] = { 5, 9, 13, 15 };
    RECT const* rects[2];
    int step;
    int width;
    int i;
    POLY_FT4* prim;

    if (Battle_kStatusLabelLayouts[p->value].count == 0) {
        return;
    }
    if (p->frame < 4) {
        step = p->frame;

        if (!(p->colorFlags & 1)) {
            x += slide[step];
        }
    } else if (p->frame < 6) {
        step = 5;
    } else if (p->frame >= 27) {
        step = 30 - p->frame;

        if (!(p->colorFlags & 1)) {
            x -= slide[step];
        }
    } else {
        step = 4;

        if (p->frame >= 25) {
            step = 5;
        }
    }
    if ((p->colorFlags & 1) && p->frame >= 15) {
        width = p->frame - 15;

        if (width > 4) {
            width = 4;
        }

        Battle_DrawStatusRemovalMarker(x, y, width);
    }

    width = 0;

    for (i = 0; i < Battle_kStatusLabelLayouts[p->value].count; ++i) {
        rects[i] = &Battle_kLabelCaptionRects[Battle_kStatusLabelLayouts[p->value].labels[i]];
        width += rects[i]->w;
    }

    x -= width / 2;

    if (p->colorFlags & 1) {
        width = 0;
    } else {
        width = Battle_kStatusLabelLayouts[p->value].spacing;
    }

    prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;

    for (i = 0; i < Battle_kStatusLabelLayouts[p->value].count; ++i) {
        if (x < 320 && x + rects[i]->w >= 0) {
            setPolyFT4(prim);
            setXY4(
                prim, x, y - 6, x + rects[i]->w, y - 6, x, y + 6, x + rects[i]->w, y + 6);
            setShadeTex(prim, 0);

            if (step == 5) {
                setRGB0(prim, Battle_kStatusLabelColors[width].r / 2 + 96, Battle_kStatusLabelColors[width].g / 2 + 96,
                    Battle_kStatusLabelColors[width].b / 2 + 96);
            } else if (step == 4) {
                setRGB0(prim, Battle_kStatusLabelColors[width].r / 2, Battle_kStatusLabelColors[width].g / 2,
                    Battle_kStatusLabelColors[width].b / 2);
            } else {
                setRGB0(prim, Battle_kStatusLabelColors[width].r * fade[step] / 32,
                    Battle_kStatusLabelColors[width].g * fade[step] / 32,
                    Battle_kStatusLabelColors[width].b * fade[step] / 32);
            }

            setUV4(prim, rects[i]->x, rects[i]->y, rects[i]->x + rects[i]->w, rects[i]->y,
                rects[i]->x, rects[i]->y + 12, rects[i]->x + rects[i]->w,
                rects[i]->y + 12);
            setSemiTrans(prim, 1);

            if (step != 4) {
                setTPage(prim, 0, 1, 448, 256);
                setClut(prim, 960, 220);
            } else {
                setTPage(prim, 0, 0, 448, 256);
                setClut(prim, 976, 220);
            }

            AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 20, prim++);
        }

        x += rects[i]->w;
    }

    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
}

// decomp: BATTLE.PRG func_800957D0 (src/BATTLE/BATTLE.PRG/2842C.c:2312)
void Battle_DrawStatusRemovalMarker(int arg0, int arg1, int arg2)
{
    void** p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    POLY_FT4* prim = (POLY_FT4*)p[0];

    setlen(prim, 9);
    setcode(prim, 0x2C);
    arg0 -= 8;
    arg2 <<= 4;
    setXY4(
        prim, arg0, arg1 - 5, arg0 + 16, arg1 - 5, arg0, arg1 + 5, arg0 + 16, arg1 + 5);
    setUV4(
        prim, arg2 + 0x40, 0x30, arg2 + 0x50, 0x30, arg2 + 0x40, 0x3A, arg2 + 0x50, 0x3A);
    prim->tpage = 0x17;
    prim->clut = 0x373E;
    setShadeTex(prim, 1);
    AddPrim((u_long*)p[1] - 5, prim++);
    p[0] = prim;
}

// decomp: BATTLE.PRG func_800958A4 (src/BATTLE/BATTLE.PRG/2842C.c:2332)
void Battle_DrawAbilityTimingResult(int x, int y, Battle_FloatingLabel* p)
{
    char* timing[] = { "$TOO   FAST!", "$FAST!", "", "$SLOW!", "$TOO   SLOW!" };
    char* success[] = { "$COOL!", "$GOOD!", "$EXCELLENT!", "$RIGHT   ON!", "$PERFECT!",
        "$WELL   TIMED!", "$NICE!", "$GREAT!" };
    short offsets[] = { 64, 16, 4, 1 };
    int colors[] = { vs_getRGB888(0, 128, 128), vs_getRGB888(32, 128, 96),
        vs_getRGB888(64, 128, 64), vs_getRGB888(96, 128, 32), vs_getRGB888(128, 128, 0) };
    int step;

    if (p->frame < 4) {
        step = p->frame;
        x -= offsets[step];
    } else if (p->frame >= 42) {
        step = 45 - p->frame;
        x -= offsets[step];
    }

    if (p->value != 2) {
        Render_DrawCompactTextColor(timing[p->value], vs_getXY_2(x, y), colors[p->value],
            ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 16);
    } else {
        Render_DrawCompactTextColor(success[p->textLength], vs_getXY_2(x, y),
            vs_getRGB888(64, 128, 64), ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 16);
    }
}

// decomp: BATTLE.PRG func_80095A4C (src/BATTLE/BATTLE.PRG/2842C.c:2359)
void Battle_DrawChainCount(int x, int y, Battle_FloatingLabel* p)
{
    char text[16];
    short offsets[] = { 64, 16, 4, 1 };

    if ((p->frame < 4) && (p->value == 1)) {
        x -= offsets[p->frame];
    } else if (p->frame >= 87) {
        x -= offsets[90 - p->frame];
    }
    if (p->value == 1) {
        sprintf(text, "$1 CHAIN");
    } else {
        sprintf(text, "$%d CHAINS", p->value);
    }

    Render_DrawCompactTextColor(text, vs_getXY_2(x, y), vs_getRGB888(160, 64, 16),
        ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 16);
}

// decomp: BATTLE.PRG func_80095B70 (src/BATTLE/BATTLE.PRG/2842C.c:2379)
void Battle_SetActorMarkersHidden(int arg0) { Battle_actorMarkersHidden = arg0; }

// decomp: BATTLE.PRG func_80095B7C (src/BATTLE/BATTLE.PRG/2842C.c:2381)
void Battle_SetActorMarker(int arg0, int arg1)
{
    if (arg0 != 0) {
        if (arg1 & 0x80) {
            Battle_actorMarkers[arg0] |= 0x80;
            return;
        }
        if (arg1 & 0x40) {
            Battle_actorMarkers[arg0] &= 0x7F;
            return;
        }
        Battle_actorMarkers[arg0] &= 0x80;
        Battle_actorMarkers[arg0] |= arg1 & 0x7F;
        return;
    }
    memset(Battle_actorMarkers, arg1, 0x10);
}

// decomp: BATTLE.PRG func_80095C18 (src/BATTLE/BATTLE.PRG/2842C.c:2399)
void Battle_DrawActorMarker(int actor, int kind)
{
    SVECTOR position;
    SVECTOR other;
    int _[2] __attribute__((unused));
    long side;
    int xy;
    u_long* prim;
    u_long* ot;

    if (!(kind & 8)) {
        int flags;
        int z;
        int const* corners;
        u_short const* uv;

        if (kind & 0x80) {
            kind &= 7;

            if (kind < 1 || kind > 4) {
                kind = 5;
            }
        }
        if (Actors_ProjectActorTaggedBone(actor, 251, &position, 0)) {
            return;
        }
        if (Actors_ProjectActorTaggedBone(0, 251, &other, 0)) {
            return;
        }

        z = position.vz - 16;
        xy = *(int*)&position;

        if (other.vx + 32 < position.vx) {
            Battle_actorMarkerSides &= ~(1 << actor);
        }
        if (position.vx < other.vx - 32) {
            Battle_actorMarkerSides |= 1 << actor;
        }
        if (z < 64) {
            z = 64;
        } else if (z >= 2048) {
            z = 2047;
        }

        ot = (u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + z;
        side = (Battle_actorMarkerSides >> actor) & 1;

        switch (kind) {
        case 0:
            return;
        case 1:
        case 2:
        case 3:
            while (kind) {
                --kind;
                Render_QueueSprite(128, xy + kind * 6 + Battle_kMarkerIconOffsets[side], vs_getWH(5, 5),
                    ot)[4] = vs_getUV0Clut(90, 18, 992, 220);
            }
            break;
        case 4:
            flags = 0;

            if (position.vx < 16) {
                position.vx = 16;
                flags = 1;
            }
            if (position.vx > 304) {
                position.vx = 304;
                flags |= 2;
            }
            if (flags) {
                if (position.vy < 40) {
                    position.vy = 40;
                    flags |= 4;
                }
            } else if (position.vy < 8) {
                position.vy = 40;
                flags = 4;
            }
            if (position.vy > 208) {
                position.vy = 208;
                flags |= 8;
            }
            if (flags) {
                Battle_DrawOffscreenActorArrow(&position, flags);
                return;
            }

            side += 2;
            Render_QueueSprite(128, xy + Battle_kMarkerIconOffsets[side], vs_getWH(7, 12), ot)[4] =
                vs_getUV0Clut(152, 17, 992, 220);
            break;
        case 5:
            Render_QueueSprite(128, xy + Battle_kMarkerIconOffsets[side] + vs_getXY(2, -3),
                vs_getWH(16, 16), ot)[4] = vs_getUV0Clut(152, 32, 992, 220);
            break;
        case 6:
            side += 4;
            break;
        case 7:
            prim = Render_QueueSprite(128, xy + vs_getXY(0, -22), vs_getWH(7, 12), ot);
            prim[1] = _get_mode(0, 0, getTPage(0, 0, 448, 256));
            prim[4] = vs_getUV0Clut(183, 17, 992, 220);
            return;
        }

        prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
        corners = Battle_kMarkerFrameCorners[side];
        uv = Battle_kMarkerFrameTexCoords[side];
        prim[0] = vs_getTag(Render_PolyFt4, *ot);
        prim[1] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyFt4, 128, 128, 128);
        prim[2] = Core_Math_AddPackedScreenCoordinates(xy, corners[0]);
        prim[3] = uv[0] | (getClut(992, 220) << 16);
        prim[4] = Core_Math_AddPackedScreenCoordinates(xy, corners[1]);
        prim[5] = uv[1] | (getTPage(0, 0, 448, 256) << 16);
        prim[6] = Core_Math_AddPackedScreenCoordinates(xy, corners[2]);
        prim[7] = uv[2];
        prim[8] = Core_Math_AddPackedScreenCoordinates(xy, corners[3]);
        prim[9] = uv[3];
        *ot = ((u_long)prim << 8) >> 8;
        ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim + 10;
    } else {
        long depth;

        if (Actors_ActorTaggedBonePosition(actor, 255, &other, 0) == 0) {
            if (Actors_ActorTaggedBonePosition(0, 255, &position, 0)) {
                return;
            }

            position.vx -= other.vx;
            position.vz -= other.vz;
            side = ABS(position.vx) + ABS(position.vz);

            if (side < 352) {
                Battle_actorMarkerSides &= ~(1 << actor);
            }
            if (side > 416) {
                Battle_actorMarkerSides |= 1 << actor;
            }

            side = (Battle_actorMarkerSides >> actor) & 1;

            if (side) {
                return;
            }

            Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
            Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
            other.vy -= 64;
            Core_Gte_RotTransPers(&other, (long*)&position, &depth, &side);
            ot = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable;
            position.vx -= 8;
            position.vy -= 8;
            xy = *(int*)&position;
            depth = Field_GimmickSubtype(actor);

            if (depth >= 0) {
                if (depth == 0) {
                    ++Battle_actorMarkers[actor];

                    if (!(Battle_actorMarkers[actor] & 7)) {
                        Battle_actorMarkers[actor] = 0;
                    }
                }

                prim = Render_QueueSprite(128, xy, vs_getWH(16, 16), ot);
                prim[1] = _get_mode(0, 0, getTPage(0, 0, 768, 0));
                prim[4] = ((depth << 4) + 96) | vs_getUV0Clut(0, 32, 976, 223);
                return;
            }
        }

        Battle_actorMarkers[actor] = 0;
    }
}

// decomp: BATTLE.PRG func_8009627C (src/BATTLE/BATTLE.PRG/2842C.c:2582)
void Battle_DrawActorMarkers(void)
{
    int i;

    if (Battle_actorMarkersHidden == 0) {
        for (i = 1; i < 16; ++i) {
            if (Battle_actorMarkers[i] != 0) {
                Battle_DrawActorMarker(i, Battle_actorMarkers[i]);
            }
        }
    }
}

// decomp: BATTLE.PRG func_800962E4 (src/BATTLE/BATTLE.PRG/2842C.c:2595)
void* Battle_ReserveFloatingLabel(void)
{
    Battle_FloatingLabel* p;
    int i;

    if (Battle_floatingLabelCount < 32) {
        p = &(*Battle_floatingLabels)[Battle_floatingLabelCount];
    } else {
        for (i = 0; i < 31; ++i) {
            Core_Memory_CopyAlignedWords(
                &(*Battle_floatingLabels)[i], &(*Battle_floatingLabels)[i + 1], sizeof(*Battle_floatingLabels)[i]);
        }
        Core_Memory_ZeroAlignedWords(&(*Battle_floatingLabels)[31], sizeof(*Battle_floatingLabels)[31]);
        p = &(*Battle_floatingLabels)[31];
    }
    return p;
}

// decomp: BATTLE.PRG func_8009639C (src/BATTLE/BATTLE.PRG/2842C.c:2613)
void Battle_CompactFloatingLabels(int arg0)
{
    int j;
    int i;

    for (i = 0; i < arg0; ++i) {
        for (j = 0; j < Battle_floatingLabelCount; ++j) {
            if ((*Battle_floatingLabels)[j].key.bytes[0] == 0) {
                Battle_RemoveFloatingLabel(j);
                break;
            }
        }
    }
}

// decomp: BATTLE.PRG func_80096444 (src/BATTLE/BATTLE.PRG/2842C.c:2628)
void Battle_RemoveFloatingLabel(int arg0)
{
    int i;

    --Battle_floatingLabelCount;
    if (arg0 != Battle_floatingLabelCount) {
        for (i = arg0; i < Battle_floatingLabelCount; ++i) {
            Core_Memory_CopyAlignedWords(
                &(*Battle_floatingLabels)[i], &(*Battle_floatingLabels)[i + 1], sizeof(*Battle_floatingLabels)[i]);
        }
    }
    Core_Memory_ZeroAlignedWords(&(*Battle_floatingLabels)[Battle_floatingLabelCount], sizeof(*Battle_floatingLabels)[Battle_floatingLabelCount]);
}

// decomp: BATTLE.PRG func_8009651C (src/BATTLE/BATTLE.PRG/2842C.c:2642)
void Battle_DrawOffscreenActorArrow(SVECTOR* position, int direction)
{
    int dx[] = { -8, 0, 7, 10, 7, 0, -8, -11 };
    int dy[] = { -8, -11, -8, 0, 7, 10, 7, 0 };
    int x = position->vx;
    int y = position->vy;
    int index;
    POLY_FT4* prim;

    if (direction == 4) {
        index = 0;
    } else if (direction == 6) {
        index = 1;
    } else if (direction == 2) {
        index = 2;
    } else if (direction == 10) {
        index = 3;
    } else if (direction == 8) {
        index = 4;
    } else if (direction == 9) {
        index = 5;
    } else if (direction == 1) {
        index = 6;
    } else if (direction == 5) {
        index = 7;
    } else {
        return;
    }

    prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
    setPolyFT4(prim);
    setShadeTex(prim, 1);
    setXY0(prim, dx[index] + x, dy[index] + y);
    index += 2;
    index &= 7;
    prim->x1 = dx[index] + x;
    prim->y1 = dy[index] + y;
    index += 2;
    index &= 7;
    prim->x3 = dx[index] + x;
    prim->y3 = dy[index] + y;
    index += 2;
    index &= 7;
    prim->x2 = dx[index] + x;
    prim->y2 = dy[index] + y;
    setUV4(prim, 48, 48, 63, 48, 48, 63, 63, 63);
    setTPage(prim, 0, 0, 768, 0);
    setClut(prim, 976, 223);
    AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable - 16, prim++);
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
}

// decomp: BATTLE.PRG func_80096768 (src/BATTLE/BATTLE.PRG/2842C.c:2694)
int Battle_QueueEquipmentStatChange(int arg0, int arg1, int arg2)
{
    int _[2] __attribute__((unused));
    int i;
    int count;
    Battle_FloatingLabel* p;

    if ((Input_settings.weaponStatChange && (arg0 == 240))
        || (Input_settings.armorStatChange && (arg0 != 240))) {
        switch (arg0) {
        case 1:
            arg0 = 4;
            break;

        case 2:
            arg0 = 2;
            break;

        case 3:
            arg0 = 3;
            break;

        case 4:
            arg0 = 6;
            break;

        case 6:
            arg0 = 5;
            break;

        case 241:
            arg0 = 1;
            break;

        case 240:
            arg0 = 0;
            break;
        }

        p = *Battle_floatingLabels;
        count = 0;

        for (i = 0; i < Battle_floatingLabelCount; ++i, ++p) {
            if ((p->key.bytes[0] == 6) || (p->key.bytes[0] == 7)) {
                ++count;
            }
        }

        p = ((void* (*)())Battle_ReserveFloatingLabel)();
        p->key.bytes[0] = 7;
        p->key.bytes[1] = 0x7F;
        p->position.pad = count;
        p->scaleY = 0;
        p->frame = 0;
        p->colorFlags = arg0;
        p->limb = arg1;

        if ((arg2 & 0x7FFF) >= 1000) {
            p->value = 999 | ((arg2 & 0x80000000) >> 16);
        } else {
            p->value = (arg2 & 0x7FFF) | ((arg2 & 0x80000000) >> 16);
        }

        sprintf(p->text, "%d", p->value & 0x7FFF);
        p->textLength = strlen(p->text);
        ++Battle_floatingLabelCount;
    }

    return 0;
}

// decomp: BATTLE.PRG func_8009695C (src/BATTLE/BATTLE.PRG/2842C.c:2765)
void Battle_DrawEquipmentStatChange(int x, int y, Battle_FloatingLabel* p)
{
    CVECTOR colors[] = { { 128, 64, 0, 0 }, { 0, 64, 128, 0 } };
    int negative;
    int positive;
    int i;
    POLY_FT4* prim = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;

    y -= p->scaleY;
    negative = p->value & 0x8000;
    positive = !negative;

    setPolyFT4(prim);
    setRGB0(prim, colors[positive].r, colors[positive].g, colors[positive].b);
    setXY4(prim, x, y, x + Battle_kStatChangeFirstWordRects[p->colorFlags].w, y, x, y + Battle_kStatChangeFirstWordRects[p->colorFlags].h,
        x + Battle_kStatChangeFirstWordRects[p->colorFlags].w, y + Battle_kStatChangeFirstWordRects[p->colorFlags].h);
    setUV4(prim, Battle_kStatChangeFirstWordRects[p->colorFlags].x, Battle_kStatChangeFirstWordRects[p->colorFlags].y,
        Battle_kStatChangeFirstWordRects[p->colorFlags].x + Battle_kStatChangeFirstWordRects[p->colorFlags].w, Battle_kStatChangeFirstWordRects[p->colorFlags].y,
        Battle_kStatChangeFirstWordRects[p->colorFlags].x, Battle_kStatChangeFirstWordRects[p->colorFlags].y + Battle_kStatChangeFirstWordRects[p->colorFlags].h,
        Battle_kStatChangeFirstWordRects[p->colorFlags].x + Battle_kStatChangeFirstWordRects[p->colorFlags].w,
        Battle_kStatChangeFirstWordRects[p->colorFlags].y + Battle_kStatChangeFirstWordRects[p->colorFlags].h);
    setTPage(prim, 0, 0, 768, 0);
    setClut(prim, 848, 223);
    AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 8, prim++);

    x += Battle_kStatChangeFirstWordRects[p->colorFlags].w + 2;
    setPolyFT4(prim);
    setRGB0(prim, colors[positive].r, colors[positive].g, colors[positive].b);
    setXY4(prim, x, y, x + Battle_kStatChangeSecondWordRects[p->limb].w, y, x, y + Battle_kStatChangeSecondWordRects[p->limb].h,
        x + Battle_kStatChangeSecondWordRects[p->limb].w, y + Battle_kStatChangeSecondWordRects[p->limb].h);
    setUV4(prim, Battle_kStatChangeSecondWordRects[p->limb].x, Battle_kStatChangeSecondWordRects[p->limb].y,
        Battle_kStatChangeSecondWordRects[p->limb].x + Battle_kStatChangeSecondWordRects[p->limb].w, Battle_kStatChangeSecondWordRects[p->limb].y,
        Battle_kStatChangeSecondWordRects[p->limb].x, Battle_kStatChangeSecondWordRects[p->limb].y + Battle_kStatChangeSecondWordRects[p->limb].h,
        Battle_kStatChangeSecondWordRects[p->limb].x + Battle_kStatChangeSecondWordRects[p->limb].w,
        Battle_kStatChangeSecondWordRects[p->limb].y + Battle_kStatChangeSecondWordRects[p->limb].h);
    setTPage(prim, 0, 0, 768, 0);
    setClut(prim, 848, 223);
    AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 8, prim++);

    x += Battle_kStatChangeSecondWordRects[p->limb].w + 2;
    setPolyFT4(prim);
    setRGB0(prim, colors[positive].r, colors[positive].g, colors[positive].b);
    setXY4(prim, x, y - 1, x + 8, y - 1, x, y + 7, x + 8, y + 7);
    setUV4(prim, positive * 8 + 120, 208, positive * 8 + 128, 208, positive * 8 + 120,
        216, positive * 8 + 128, 216);
    setTPage(prim, 0, 0, 768, 0);
    setClut(prim, 848, 223);
    AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 8, prim++);

    x += 2;
    x += p->textLength * 5;

    for (i = p->textLength - 1; i >= 0; --i, x -= 5) {
        setPolyFT4(prim);
        setRGB0(prim, colors[positive].r, colors[positive].g, colors[positive].b);
        prim->x0 = x;
        prim->x1 = x + 6;
        prim->y0 = y - 1;
        prim->y1 = y - 1;
        prim->x2 = x;
        prim->y2 = y + 9;
        prim->x3 = x + 6;
        prim->y3 = y + 9;
        setUV4(prim, (p->text[i] - '0') * 6, 0, (p->text[i] - '0') * 6 + 6,
            0, (p->text[i] - '0') * 6, 10, (p->text[i] - '0') * 6 + 6, 10);
        setTPage(prim, 0, 0, 768, 0);
        setClut(prim, 832, 223);
        AddPrim(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 8, prim++);
    }

    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
}

// decomp: BATTLE.PRG func_80096FF0 (src/BATTLE/BATTLE.PRG/2842C.c:2838)
void Battle_ShowAbilityTimingCue(int arg0)
{
    int d;

    if (!Input_settings.abilityTimingDisplay) {
        return;
    }

    if (Actors_ProjectActorTaggedBone(0, 0xFB, &Battle_abilityTimingCuePosition, 0) != 0) {
        return;
    }

    if (Battle_abilityTimingCuePosition.vy < 40) {
        d = 40 - Battle_abilityTimingCuePosition.vy;
        Battle_abilityTimingCuePosition.vy = 40;
        if (Battle_abilityTimingCuePosition.vx + d < 288) {
            Battle_abilityTimingCuePosition.vx += d;
        } else {
            Battle_abilityTimingCuePosition.vx -= d;
        }
        Battle_abilityTimingCuePosition.vy += d;
    }

    Battle_abilityTimingCueFrames += arg0;
}

// decomp: BATTLE.PRG func_800970BC (src/BATTLE/BATTLE.PRG/2842C.c:2864)
void Battle_DrawAbilityTimingCue(void)
{
    void** p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    POLY_FT4* prim = (POLY_FT4*)p[0];

    setPolyFT4(prim);
    setXY4(prim, Battle_abilityTimingCuePosition.vx - 11, Battle_abilityTimingCuePosition.vy - 32, Battle_abilityTimingCuePosition.vx + 11,
        Battle_abilityTimingCuePosition.vy - 32, Battle_abilityTimingCuePosition.vx - 11, Battle_abilityTimingCuePosition.vy, Battle_abilityTimingCuePosition.vx + 11,
        Battle_abilityTimingCuePosition.vy);
    setUV4(prim, 0xC0, 0, 0xD6, 0, 0xC0, 0x20, 0xD6, 0x20);
    setRGB0(prim, 0xF0, 0x40, 0x20);
    setTPage(prim, 0, 0, 448, 256);
    setClut(prim, 960, 221);
    AddPrim((u_long*)p[1] - 5, prim++);
    p[0] = prim;
}

// decomp: BATTLE.PRG func_800971D4 (src/BATTLE/BATTLE.PRG/2842C.c:2881)
void Battle_CommitEquipmentStatChanges(void)
{
    int i;
    Battle_FloatingLabel* p = *Battle_floatingLabels;

    for (i = 0; i < Battle_floatingLabelCount; ++i, ++p) {
        if (p->key.bytes[0] == 7) {
            p->key.bytes[0] = 6;
        }
    }
}

// decomp: BATTLE.PRG func_8009722C (src/BATTLE/BATTLE.PRG/2842C.c:2893)
void Battle_SuppressAshleyStatusLabels(void) { Battle_ashleyStatusLabelsSuppressed = 1; }

