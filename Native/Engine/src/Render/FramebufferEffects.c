// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Memory/Heap.h"
#include "Field/RoomLoading.h"
#include "Field/Transitions.h"
#include "Render/Display.h"
#include "Render/FramebufferEffects.h"
#include "Render/ScreenTransition.h"

// decomp: D_800F1A20 (D_800F1A20@BATTLE/BATTLE.PRG)
int Render_framebufferEffectParameter;

// decomp: D_800F1A28 (D_800F1A28@BATTLE/BATTLE.PRG)
SVECTOR Render_framebufferEffectRotation;

// decomp: D_800F1A30 (D_800F1A30@BATTLE/BATTLE.PRG)
VECTOR Render_framebufferEffectScale;

// decomp: D_800F1A40 (D_800F1A40@BATTLE/BATTLE.PRG)
int Render_framebufferEffectUsesPreviousFrame;

// decomp: D_800F1A44 (D_800F1A44@BATTLE/BATTLE.PRG)
u_int Render_framebufferEffectDepth;

// decomp: D_800F1A68 (D_800F1A68@BATTLE/BATTLE.PRG)
Render_FramebufferEffectTranslation Render_framebufferEffectOffset;

// decomp: D_800F1A78 (D_800F1A78@BATTLE/BATTLE.PRG)
CVECTOR Render_framebufferEffectTint;

// decomp: D_800F1A9C (D_800F1A9C@BATTLE/BATTLE.PRG)
char Render_framebufferEffectResetBytes[4];

// decomp: D_800F1ABC (D_800F1ABC@BATTLE/BATTLE.PRG)
Render_LegacyFramebufferEffectPackets* Render_legacyFramebufferEffectPackets;

// decomp: D_800F1B98 (D_800F1B98@BATTLE/BATTLE.PRG)
int Render_framebufferEffectState;

// decomp: D_800F1B9C (D_800F1B9C@BATTLE/BATTLE.PRG)
int Render_framebufferEffectBlendMode;

// decomp: BATTLE.PRG _drawScreenTransition (src/BATTLE/BATTLE.PRG/146C.c:2223)
void Render_DrawScreenTransition(void* ot)
{
    MATRIX rotTransMatrix;
    VECTOR screenCenter;
    SVECTOR zRotation;
    SVECTOR wipeEdgePos;
    VECTOR projectedPos;
    RECT rect;
    long flag;

    memset(&rect, 0, sizeof rect);

    if (Field_screenTransitionAlpha != 0) {
        switch (Field_screenTransitionEffect) {
        // Fade to black
        case 0:
        case 4:
        case 5:
            setXY4(&Render_legacyTransitionFadePackets[Render_frameBuf], 0, 0, 320, 0, 0,
                240, 320, 240);
            SetDrawMode(&Render_legacyTransitionDrawModes[Render_frameBuf], 0, 1,
                getTPage(0, 2, 0, 0), &rect);
            SetSemiTrans(&Render_legacyTransitionFadePackets[Render_frameBuf], 1);
            setRGB0(&Render_legacyTransitionFadePackets[Render_frameBuf],
                Field_screenTransitionAlpha, Field_screenTransitionAlpha,
                Field_screenTransitionAlpha);
            AddPrim(ot + 4, &Render_legacyTransitionFadePackets[Render_frameBuf]);
            AddPrim(ot + 4, &Render_legacyTransitionDrawModes[Render_frameBuf]);
            break;
        // Fade to white
        case 1:
            setXY4(&Render_legacyTransitionFadePackets[Render_frameBuf], 0, 0, 320, 0, 0,
                240, 320, 240);
            SetDrawMode(&Render_legacyTransitionDrawModes[Render_frameBuf], 0, 1,
                getTPage(0, 1, 0, 0), &rect);

            if (Field_screenTransitionAlpha == 255) {
                SetSemiTrans(&Render_legacyTransitionFadePackets[Render_frameBuf], 0);
            } else {
                SetSemiTrans(&Render_legacyTransitionFadePackets[Render_frameBuf], 1);
            }

            setRGB0(&Render_legacyTransitionFadePackets[Render_frameBuf],
                Field_screenTransitionAlpha, Field_screenTransitionAlpha,
                Field_screenTransitionAlpha);
            AddPrim(ot + 4, &Render_legacyTransitionFadePackets[Render_frameBuf]);
            AddPrim(ot + 4, &Render_legacyTransitionDrawModes[Render_frameBuf]);
            break;
        // Screen wipe
        case 2:
            if ((Field_screenTransitionStep == 4)
                || (Field_screenTransitionStep == 7)) {
                setVector(
                    &zRotation, 0, 0, Field_screenTransitionWipeAngle + ONE / 2);
            } else {
                setVector(&zRotation, 0, 0, Field_screenTransitionWipeAngle);
            }
            setVector(&screenCenter, 160, 120, 0);
            Core_Gte_RotMatrix(&zRotation, &rotTransMatrix);
            Core_Gte_TransMatrix(&rotTransMatrix, &screenCenter);
            Core_Gte_SetRotMatrix(&rotTransMatrix);
            Core_Gte_SetTransMatrix(&rotTransMatrix);

            wipeEdgePos.vz = 0;
            wipeEdgePos.vx = -360;
            wipeEdgePos.vy = -200;
            Core_Gte_RotTrans(&wipeEdgePos, &projectedPos, &flag);
            Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad.x0 =
                projectedPos.vx;
            Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad.y0 =
                projectedPos.vy;

            wipeEdgePos.vx = -360;
            wipeEdgePos.vy = 200;
            Core_Gte_RotTrans(&wipeEdgePos, &projectedPos, &flag);
            Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad.x2 =
                projectedPos.vx;
            Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad.y2 =
                projectedPos.vy;

            wipeEdgePos.vy = -200;
            wipeEdgePos.vx =
                ((((Field_screenTransitionAlpha * 560) / 255) - 720) / 2);
            Core_Gte_RotTrans(&wipeEdgePos, &projectedPos, &flag);
            Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad.x0 =
                Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad.x1 =
                    projectedPos.vx;
            Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad.y0 =
                Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad.y1 =
                    projectedPos.vy;

            wipeEdgePos.vy = 200;
            wipeEdgePos.vx =
                ((((Field_screenTransitionAlpha * 560) / 255) - 720) / 2);
            Core_Gte_RotTrans(&wipeEdgePos, &projectedPos, &flag);
            Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad.x2 =
                Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad.x3 =
                    projectedPos.vx;
            Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad.y2 =
                Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad.y3 =
                    projectedPos.vy;

            wipeEdgePos.vy = -200;
            wipeEdgePos.vx = ((Field_screenTransitionAlpha * 560) / 255) - 360;
            Core_Gte_RotTrans(&wipeEdgePos, &projectedPos, &flag);
            Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad.x1 =
                Render_legacyTransitionEdgePackets[Render_frameBuf].x0 =
                    projectedPos.vx;
            Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad.y1 =
                Render_legacyTransitionEdgePackets[Render_frameBuf].y0 =
                    projectedPos.vy;

            wipeEdgePos.vy = 200;
            wipeEdgePos.vx = ((Field_screenTransitionAlpha * 560) / 255) - 360;
            Core_Gte_RotTrans(&wipeEdgePos, &projectedPos, &flag);
            Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad.x3 =
                Render_legacyTransitionEdgePackets[Render_frameBuf].x2 =
                    projectedPos.vx;
            Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad.y3 =
                Render_legacyTransitionEdgePackets[Render_frameBuf].y2 =
                    projectedPos.vy;

            wipeEdgePos.vy = -200;
            wipeEdgePos.vx = ((Field_screenTransitionAlpha * 560) / 255) - 200;
            Core_Gte_RotTrans(&wipeEdgePos, &projectedPos, &flag);
            Render_legacyTransitionEdgePackets[Render_frameBuf].x1 = projectedPos.vx;
            Render_legacyTransitionEdgePackets[Render_frameBuf].y1 = projectedPos.vy;

            wipeEdgePos.vy = 200;
            wipeEdgePos.vx = ((Field_screenTransitionAlpha * 560) / 255) - 200;
            Core_Gte_RotTrans(&wipeEdgePos, &projectedPos, &flag);
            Render_legacyTransitionEdgePackets[Render_frameBuf].x3 = projectedPos.vx;
            Render_legacyTransitionEdgePackets[Render_frameBuf].y3 = projectedPos.vy;

            AddPrim(
                ot + 4, &Render_legacyTransitionWipePackets[Render_frameBuf].coveredQuad);
            AddPrim(ot + 4, &Render_legacyTransitionWipePackets[Render_frameBuf].edgeQuad);
            AddPrim(ot + 4, &Render_legacyTransitionEdgePackets[Render_frameBuf]);
            SetDrawMode(
                &Render_legacyTransitionDrawModes[Render_frameBuf], 0, 1, 64, &rect);
            AddPrim(ot + 4, &Render_legacyTransitionDrawModes[Render_frameBuf]);
            break;
        }
    }

    if (Field_screenTransitionStep == 1 || Field_screenTransitionStep == 2
        || Field_screenTransitionStep == 5) {
        if (Field_screenTransitionAlpha >= (255 - Field_screenTransitionSpeed)) {
            if (Field_screenTransitionStep == 5) {
                Field_screenTransitionStep = 6;
            }
            Field_screenTransitionAlpha = 255;
        } else {
            Field_screenTransitionAlpha += Field_screenTransitionSpeed;
        }
    } else if (Field_screenTransitionStep == 4
               || Field_screenTransitionStep == 7) {
        if (Field_screenTransitionAlpha <= Field_screenTransitionSpeed) {
            Field_screenTransitionAlpha = 0;
            Field_screenTransitionStep = 0;
        } else {
            Field_screenTransitionAlpha -= Field_screenTransitionSpeed;
        }
    }
}

// decomp: BATTLE.PRG func_8007D360 (src/BATTLE/BATTLE.PRG/146C.c:9258)
void Render_ResetFramebufferEffect(void)
{
    Render_framebufferEffectState = 0;
    Render_framebufferEffectBlendMode = 0;
    Render_framebufferEffectParameter = 0;
    setVector(&Render_framebufferEffectRotation, 0, 0, 0);
    setVector(&Render_framebufferEffectScale, ONE, ONE, 0);
    Render_framebufferEffectTint.r = 0x80;
    Render_framebufferEffectTint.g = 0x80;
    Render_framebufferEffectTint.b = 0x80;
    Render_framebufferEffectResetBytes[0] = 0x80;
    Render_framebufferEffectResetBytes[1] = 0x80;
    Render_framebufferEffectResetBytes[2] = 0x80;
    Render_framebufferEffectOffset.x = 0;
    Render_framebufferEffectOffset.y = 0;
    Render_framebufferEffectOffset.reserved8 = 0;
    Render_framebufferEffectDepth = 0;
    Render_framebufferEffectUsesPreviousFrame = 0;
}

// decomp: BATTLE.PRG func_8007D3F8 (src/BATTLE/BATTLE.PRG/146C.c:9278)
void Render_InitializeFramebufferEffect(void)
{
    Render_legacyFramebufferEffectPackets = NULL;
    Render_ResetFramebufferEffect();
}

// decomp: BATTLE.PRG func_8007D41C (src/BATTLE/BATTLE.PRG/146C.c:9285)
void Render_BuildFramebufferEffectPrimitives(void)
{
    int i;
    POLY_FT4* poly;
    SetPolyFT4(&Render_legacyFramebufferEffectPackets->rotatedStrips[0]);
    setRGB0(&Render_legacyFramebufferEffectPackets->rotatedStrips[0], Render_framebufferEffectTint.r, Render_framebufferEffectTint.g, Render_framebufferEffectTint.b);
    SetSemiTrans(&Render_legacyFramebufferEffectPackets->rotatedStrips[0], 1);
    setUV4(&Render_legacyFramebufferEffectPackets->rotatedStrips[0], 0, 0, 64, 0, 0, 223, 64, 223);
    for (i = 0; i < 10; ++i) {
        (i + Render_legacyFramebufferEffectPackets->rotatedStrips)[0] = Render_legacyFramebufferEffectPackets->rotatedStrips[0];
        poly = (POLY_FT4*)(i * sizeof(POLY_FT4) + (uintptr_t)Render_legacyFramebufferEffectPackets);
        setXY4(poly, i * 64, 0, i * 64 + 64, 0, i * 64, 223, i * 64 + 64, 223);
        Render_legacyFramebufferEffectPackets->rotatedStrips[i].tpage = GetTPage(2, 0, i * 64, 0);
    }
    setUV4(&Render_legacyFramebufferEffectPackets->rotatedStrips[4], 0, 0, 63, 0, 0, 223, 63, 223);
    setUV4(&Render_legacyFramebufferEffectPackets->rotatedStrips[9], 0, 0, 63, 0, 0, 223, 63, 223);
    SetSprt(&Render_legacyFramebufferEffectPackets->flatStrips[0]);
    setRGB0(&Render_legacyFramebufferEffectPackets->flatStrips[0], Render_framebufferEffectTint.r, Render_framebufferEffectTint.g, Render_framebufferEffectTint.b);
    SetSemiTrans(&Render_legacyFramebufferEffectPackets->flatStrips[0], 1);
    setUV0(&Render_legacyFramebufferEffectPackets->flatStrips[0], 0, 0);
    setWH(&Render_legacyFramebufferEffectPackets->flatStrips[0], 64, 223);
    for (i = 0; i < 10; ++i) {
        (i + Render_legacyFramebufferEffectPackets->flatStrips)[0] = Render_legacyFramebufferEffectPackets->flatStrips[0];
    }
}

// decomp: BATTLE.PRG func_8007D734 (src/BATTLE/BATTLE.PRG/146C.c:9311)
void Render_DrawFramebufferEffect(void* ordering_table)
{
    MATRIX transform;
    VECTOR translation;
    SVECTOR vertex;
    VECTOR projected;
    RECT clip;
    long flag;
    int i;

    memset(&clip, 0, sizeof(clip));

    if (Render_framebufferEffectState == 2) {
        translation.vx = Render_framebufferEffectOffset.x + 160;
        translation.vy = Render_framebufferEffectOffset.y + 112;
        Core_Gte_RotMatrix(&Render_framebufferEffectRotation, &transform);
        Core_Gte_TransMatrix(&transform, &translation);
        Core_Gte_ScaleMatrix(&transform, &Render_framebufferEffectScale);
        Core_Gte_SetRotMatrix(&transform);
        Core_Gte_SetTransMatrix(&transform);

        if ((Render_framebufferEffectRotation.vz & 0xFFF) || Render_framebufferEffectScale.vx != ONE
            || Render_framebufferEffectScale.vy != Render_framebufferEffectScale.vx) {
            for (i = 0; i < 5; ++i) {
                POLY_FT4* topLeft;
                POLY_FT4* topRight;
                POLY_FT4* bottomLeft;
                POLY_FT4* bottomRight;

                setRGB0(&Render_legacyFramebufferEffectPackets->rotatedStrips[(Render_frameBuf * 5) + i], Render_framebufferEffectTint.r,
                    Render_framebufferEffectTint.g, Render_framebufferEffectTint.b);

                if (!(Render_framebufferEffectRoll & 0xFFF) && Render_framebufferEffectUsesPreviousFrame) {
                    Render_legacyFramebufferEffectPackets->rotatedStrips[(Render_frameBuf * 5) + i].tpage = GetTPage(
                        2, Render_framebufferEffectBlendMode, i * 64 + ((Render_frameBuf ^ 1) * 320), 0);
                } else {
                    Render_legacyFramebufferEffectPackets->rotatedStrips[(Render_frameBuf * 5) + i].tpage = GetTPage(
                        2, Render_framebufferEffectBlendMode, i * 64 + ((Render_frameBuf & 1) * 320), 0);
                }

                setVector(&vertex, i * 64 - 160, -112, 0);
                Core_Gte_RotTrans(&vertex, &projected, &flag);
                topLeft = &Render_legacyFramebufferEffectPackets->rotatedStrips[(Render_frameBuf * 5) + i];
                topLeft->x0 = projected.vx;
                topLeft->y0 = projected.vy;
                setVector(&vertex, i * 64 - 96, -112, 0);
                Core_Gte_RotTrans(&vertex, &projected, &flag);
                topRight = &Render_legacyFramebufferEffectPackets->rotatedStrips[(Render_frameBuf * 5) + i];
                topRight->x1 = projected.vx;
                topRight->y1 = projected.vy;
                setVector(&vertex, i * 64 - 160, 111, 0);
                Core_Gte_RotTrans(&vertex, &projected, &flag);
                bottomLeft = &Render_legacyFramebufferEffectPackets->rotatedStrips[(Render_frameBuf * 5) + i];
                bottomLeft->x2 = projected.vx;
                bottomLeft->y2 = projected.vy;
                setVector(&vertex, i * 64 - 96, 111, 0);
                Core_Gte_RotTrans(&vertex, &projected, &flag);
                bottomRight = &Render_legacyFramebufferEffectPackets->rotatedStrips[(Render_frameBuf * 5) + i];
                bottomRight->x3 = projected.vx;
                bottomRight->y3 = projected.vy;
                AddPrim(ordering_table + (Render_framebufferEffectDepth * 4),
                    &Render_legacyFramebufferEffectPackets->rotatedStrips[(Render_frameBuf * 5) + i]);
            }
        } else {
            for (i = 0; i < 5; ++i) {
                SPRT* sprite;
                int index;
                u_short tpage;

                setRGB0(&Render_legacyFramebufferEffectPackets->flatStrips[(Render_frameBuf * 5) + i], Render_framebufferEffectTint.r,
                    Render_framebufferEffectTint.g, Render_framebufferEffectTint.b);
                index = (Render_frameBuf * 5) + i;
                sprite = (SPRT*)(index * sizeof(SPRT) + (uintptr_t)Render_legacyFramebufferEffectPackets
                                 + sizeof(Render_legacyFramebufferEffectPackets->rotatedStrips));
                sprite->x0 = Render_framebufferEffectOffset.x + i * 64;
                sprite->y0 = Render_framebufferEffectOffset.y;
                AddPrim(ordering_table + (Render_framebufferEffectDepth * 4), &Render_legacyFramebufferEffectPackets->flatStrips[index]);
                tpage = Render_framebufferEffectUsesPreviousFrame ? GetTPage(2, Render_framebufferEffectBlendMode,
                                         i * 64 + ((Render_frameBuf ^ 1) * 320), 0)
                                   : GetTPage(2, Render_framebufferEffectBlendMode,
                                         i * 64 + ((Render_frameBuf & 1) * 320), 0);
                SetDrawMode(&Render_legacyFramebufferEffectPackets->drawModes[(Render_frameBuf * 5) + i], 0, 1,
                    tpage, &clip);
                AddPrim(ordering_table + (Render_framebufferEffectDepth * 4),
                    &Render_legacyFramebufferEffectPackets->drawModes[(Render_frameBuf * 5) + i]);
            }
        }
    }
    if (Render_framebufferEffectState == 1) {
        Render_framebufferEffectState = 2;
        return;
    }
    if (Render_framebufferEffectState >= 3) {
        ++Render_framebufferEffectState;

        if (Render_framebufferEffectState >= 5) {
            Render_framebufferEffectState = 0;

            if (Render_legacyFramebufferEffectPackets != NULL) {
                Core_Memory_FreeFromHighEnd(Render_legacyFramebufferEffectPackets);
                Render_legacyFramebufferEffectPackets = NULL;
            }
        }
    }
}

// decomp: BATTLE.PRG func_8007DD50 (src/BATTLE/BATTLE.PRG/146C.c:9417)
void Render_SetFramebufferEffectEnabled(int enabled)
{
    if (enabled != 0) {
        Render_framebufferEffectState = 1;
        if (Render_legacyFramebufferEffectPackets == NULL) {
            Render_legacyFramebufferEffectPackets = Core_Memory_AllocateFromHighEnd(sizeof *Render_legacyFramebufferEffectPackets);
            Render_BuildFramebufferEffectPrimitives();
        }
    } else {
        Render_framebufferEffectState = 3;
    }
}

// decomp: BATTLE.PRG func_8007DDAC (src/BATTLE/BATTLE.PRG/146C.c:9430)
void Render_SetFramebufferEffectRotation(int roll_angle) { Render_framebufferEffectRoll = roll_angle; }

// decomp: BATTLE.PRG func_8007DDB8 (src/BATTLE/BATTLE.PRG/146C.c:9432)
void Render_SetFramebufferEffectScale(Render_FramebufferEffectTranslation* scale)
{
    Render_framebufferEffectScale.vx = scale->x;
    Render_framebufferEffectScale.vy = scale->y;
}

// decomp: BATTLE.PRG func_8007DDD4 (src/BATTLE/BATTLE.PRG/146C.c:9438)
void Render_SetFramebufferEffectTint(CVECTOR* tint) { Render_framebufferEffectTint = *tint; }

// decomp: BATTLE.PRG func_8007DDF8 (src/BATTLE/BATTLE.PRG/146C.c:9440)
void Render_SetFramebufferEffectOffset(Render_FramebufferEffectTranslation* offset) { Render_framebufferEffectOffset = *offset; }

// decomp: BATTLE.PRG func_8007DE2C (src/BATTLE/BATTLE.PRG/146C.c:9442)
void Render_SetFramebufferEffectBlendMode(int blend_mode) { Render_framebufferEffectBlendMode = blend_mode; }

// decomp: BATTLE.PRG func_8007DE38 (src/BATTLE/BATTLE.PRG/146C.c:9444)
void Render_SetFramebufferEffectParameter(int arg0) { Render_framebufferEffectParameter = arg0; }

// decomp: BATTLE.PRG func_8007DE44 (src/BATTLE/BATTLE.PRG/146C.c:9446)
void Render_SetFramebufferEffectDepth(u_int ordering_depth)
{
    if (ordering_depth < 0x800) {
        Render_framebufferEffectDepth = ordering_depth;
    }
}

// decomp: BATTLE.PRG func_8007DE5C (src/BATTLE/BATTLE.PRG/146C.c:9453)
void Render_SetFramebufferPreviousFrame(int use_previous_frame) { Render_framebufferEffectUsesPreviousFrame = use_previous_frame; }

// decomp: BATTLE.PRG func_8007DE68 (src/BATTLE/BATTLE.PRG/146C.c:9455)
int Render_FramebufferEffectState(void) { return Render_framebufferEffectState; }

// decomp: BATTLE.PRG func_8007DE78 (src/BATTLE/BATTLE.PRG/146C.c:9457)
short Render_FramebufferEffectRotation(void) { return Render_framebufferEffectRoll; }

// decomp: BATTLE.PRG func_8007DE88 (src/BATTLE/BATTLE.PRG/146C.c:9459)
void Render_FramebufferEffectScale(int* output)
{
    output[0] = Render_framebufferEffectScale.vx;
    output[1] = Render_framebufferEffectScale.vx;
    output[2] = 0;
}

// decomp: BATTLE.PRG func_8007DEA8 (src/BATTLE/BATTLE.PRG/146C.c:9466)
void Render_FramebufferEffectTint(CVECTOR* output) { *output = Render_framebufferEffectTint; }

// decomp: BATTLE.PRG func_8007DECC (src/BATTLE/BATTLE.PRG/146C.c:9468)
void Render_FramebufferEffectOffset(Render_FramebufferEffectTranslation* output) { *output = Render_framebufferEffectOffset; }

// decomp: BATTLE.PRG func_8007DF00 (src/BATTLE/BATTLE.PRG/146C.c:9470)
int Render_FramebufferEffectBlendMode(void) { return Render_framebufferEffectBlendMode; }

// decomp: BATTLE.PRG func_8007DF10 (src/BATTLE/BATTLE.PRG/146C.c:9472)
int Render_FramebufferEffectParameter(void) { return Render_framebufferEffectParameter; }

// decomp: BATTLE.PRG func_8007DF20 (src/BATTLE/BATTLE.PRG/146C.c:9474)
int Render_FramebufferEffectDepth(void) { return Render_framebufferEffectDepth; }

// decomp: BATTLE.PRG func_8007DF30 (src/BATTLE/BATTLE.PRG/146C.c:9476)
int Render_UsesFramebufferPreviousFrame(void) { return Render_framebufferEffectUsesPreviousFrame; }

