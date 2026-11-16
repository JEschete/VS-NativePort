// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/SkeletonLive.hpp"
#include "Battle/FloatingLabelQueueLive.hpp"
#include "Battle/FloatingLabelRenderingLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Feedback.h"
#include "Battle/ValueLabels.h"
#include "Field/RoomRenderingLiveState.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Render.h"
#include "State/Timers.h"

// decomp: D_800E861C (D_800E861C@BATTLE/BATTLE.PRG)
int Battle_valueCaptionRects[17]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8634 (D_800E8634@BATTLE/BATTLE.PRG)
short Battle_lockedLabelHalfHeights[22]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8640 (D_800E8640@BATTLE/BATTLE.PRG)
RECT Battle_lockedLabelPieces[4]; /* contents: module image (ModuleData.cpp) */

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

// decomp: BATTLE.PRG func_800941FC (src/BATTLE/BATTLE.PRG/2842C.c:1757)
void Battle_UpdateAndDrawFloatingLabels(void)
{
    long p;
    Battle_FloatingLabel* label;
    int removed;
    int i;

    if (Battle_abilityTimingCueFrames > 0) {
        --Battle_abilityTimingCueFrames;
        VS::Battle::DrawLiveAbilityTimingCue();
    }

    if (Field_areRoomUpdatesPaused == 0) {
        Battle_statusLabelStepTimer += State_tickSpeed / 2;
    }

    if (Battle_statusLabelStepTimer > 20) {
        label = *Battle_floatingLabels;
        Battle_statusLabelStepTimer = 0;

        for (i = 0; i < VS::Battle::LiveFloatingLabelCount(); ++i, ++label) {
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

    for (i = 0; i < VS::Battle::LiveFloatingLabelCount(); ++i, ++label) {
        SVECTOR position;
        short screen[2];
        long flag;
        int visible;

        if (label->key.bytes[0] == 0) {
            continue;
        }

        if (label->key.bytes[0] < 3) {
            if (VS::Actors::LiveActorTaggedBonePosition(label->key.bytes[1], 251, &position, 0) != 0) {
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
                    VS::Battle::DrawLiveLockedFloatingLabel(screen[0], screen[1], label);
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
                VS::Battle::AdvanceLiveFloatingLabelQueue(label);
                break;
            }

            label->frame += State_tickSpeed / 2;

            if (label->frame >= 10) {
                VS::Battle::AdvanceLiveFloatingLabelQueue(label);
            }
            break;
        case 2:
            if (label->position.pad <= 0) {
                screen[1] -= label->textLength;

                if (label->key.bytes[1] != 0 || Battle_ashleyStatusLabelsSuppressed == 0) {
                    if (label->colorFlags == 2) {
                        VS::Battle::DrawLiveLockedFloatingLabel(screen[0], screen[1], label);
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
                VS::Battle::AdvanceLiveFloatingLabelQueue(label);
            }
            break;
        case 3:
            VS::Battle::DrawLiveLockedFloatingLabel(screen[0], screen[1], label);
            label->frame += State_tickSpeed / 2;

            if (label->frame >= 31) {
                label->key.bytes[0] = 0;
                ++removed;
            }
            break;
        case 4:
            VS::Battle::DrawLiveAbilityTimingResult(44, 48, label);
            label->frame += State_tickSpeed / 2;

            if (label->frame >= 46) {
                label->key.bytes[0] = 0;
                ++removed;
            }
            break;
        case 5:
            VS::Battle::DrawLiveChainCount(44, 64, label);
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
        VS::Battle::CompactLiveFloatingLabels(removed);
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

        i = VS::Battle::DrawLiveValueLabelCaption(x, y, p);
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
    prim = (POLY_FT4 *)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor);

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
        AddPrim(((u_char*)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable)) - 20, prim++);
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

        VS::Battle::DrawLiveStatusRemovalMarker(x, y, width);
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

    prim = (POLY_FT4 *)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor);

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

            AddPrim(((u_char*)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable)) - 20, prim++);
        }

        x += rects[i]->w;
    }

    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
}

// decomp: BATTLE.PRG func_80096768 (src/BATTLE/BATTLE.PRG/2842C.c:2694)
int Battle_QueueEquipmentStatChange(int arg0, int stat_label_kind, int encoded_amount)
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

        for (i = 0; i < VS::Battle::LiveFloatingLabelCount(); ++i, ++p) {
            if ((p->key.bytes[0] == 6) || (p->key.bytes[0] == 7)) {
                ++count;
            }
        }

        p = VS::Battle::ReserveLiveFloatingLabel();
        p->key.bytes[0] = 7;
        p->key.bytes[1] = 0x7F;
        p->position.pad = count;
        p->scaleY = 0;
        p->frame = 0;
        p->colorFlags = arg0;
        p->limb = stat_label_kind;

        if ((encoded_amount & 0x7FFF) >= 1000) {
            p->value = 999 | ((encoded_amount & 0x80000000) >> 16);
        } else {
            p->value = (encoded_amount & 0x7FFF) | ((encoded_amount & 0x80000000) >> 16);
        }

        sprintf((char *)(p->text), "%d", p->value & 0x7FFF);
        p->textLength = strlen((const char *)(p->text));
        VS::Battle::CommitLiveFloatingLabel(p);
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
    POLY_FT4* prim = (POLY_FT4 *)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor);

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
    AddPrim(((u_char*)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable)) + 8, prim++);

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
    AddPrim(((u_char*)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable)) + 8, prim++);

    x += Battle_kStatChangeSecondWordRects[p->limb].w + 2;
    setPolyFT4(prim);
    setRGB0(prim, colors[positive].r, colors[positive].g, colors[positive].b);
    setXY4(prim, x, y - 1, x + 8, y - 1, x, y + 7, x + 8, y + 7);
    setUV4(prim, positive * 8 + 120, 208, positive * 8 + 128, 208, positive * 8 + 120,
        216, positive * 8 + 128, 216);
    setTPage(prim, 0, 0, 768, 0);
    setClut(prim, 848, 223);
    AddPrim(((u_char*)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable)) + 8, prim++);

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
        AddPrim(((u_char*)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable)) + 8, prim++);
    }

    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = prim;
}

// decomp: BATTLE.PRG func_800971D4 (src/BATTLE/BATTLE.PRG/2842C.c:2881)
void Battle_CommitEquipmentStatChanges(void)
{
    int i;
    Battle_FloatingLabel* p = *Battle_floatingLabels;

    for (i = 0; i < VS::Battle::LiveFloatingLabelCount(); ++i, ++p) {
        if (p->key.bytes[0] == 7) {
            p->key.bytes[0] = 6;
        }
    }
}

}  // extern "C"
