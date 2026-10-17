// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Random.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Input/Controllers.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Loading.h"
#include "Render/Palette.h"
#include "State/PlayTime.h"
#include "State/Timers.h"

// decomp: D_8005DFD4 (D_8005DFD4@SLUS_010.40)
short Render_legacyViewportLeft;

// decomp: D_8005DFD6 (D_8005DFD6@SLUS_010.40)
short Render_viewportWidth;

// decomp: D_8005DFD8 (D_8005DFD8@SLUS_010.40)
short Render_legacyViewportTop;

// decomp: D_8005DFDA (D_8005DFDA@SLUS_010.40)
u_short Render_viewportHeight;

// decomp: vs_main_drawEnv (vs_main_drawEnv@SLUS_010.40)
DRAWENV Render_legacyDrawEnvironments[2];

// decomp: vs_main_dispEnv (vs_main_dispEnv@SLUS_010.40)
DISPENV Render_legacyDisplayEnvironments[2];

// decomp: vs_main_frameBuf (vs_main_frameBuf@SLUS_010.40)
int Render_frameBuf;

// decomp: D_8004A52C (D_8004A52C@SLUS_010.40)
int Render_isFrameStepPaused = 0;

// decomp: BATTLE.PRG func_800760CC (src/BATTLE/BATTLE.PRG/146C.c:5959)
void Render_ConfigureFieldDisplay(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    u_int temp_s3;

    temp_s3 = arg1 - 16;
    Core_Gte_SetGeomOffset(arg0 / 2, ((int)(temp_s3 + (temp_s3 >> 0x1F)) >> 1) + 0x10);
    Core_Gte_SetGeomScreen(arg2);
    SetDefDrawEnv(Render_legacyDrawEnvironments, 0, 0, arg0, temp_s3);
    SetDefDispEnv(Render_legacyDisplayEnvironments, arg0, 0, arg0, temp_s3);
    SetDefDrawEnv(&Render_legacyDrawEnvironments[1], arg0, 0, arg0, temp_s3);
    SetDefDispEnv(&Render_legacyDisplayEnvironments[1], 0, 0, arg0, temp_s3);
    setRECT(&Render_legacyDisplayEnvironments[0].screen, 0, 8, 256, 224);
    setRECT(&Render_legacyDisplayEnvironments[1].screen, 0, 8, 256, 224);
    Render_legacyViewportLeft = 0;
    Render_legacyViewportTop = 0;
    Render_viewportWidth = arg0;
    Render_viewportHeight = temp_s3;
    Render_legacyDrawEnvironments[1].dtd = 1;
    Render_legacyDrawEnvironments[0].dtd = 1;
    Render_legacyDrawEnvironments[1].dfe = 0;
    Render_legacyDrawEnvironments[0].dfe = 0;
    Render_legacyDrawEnvironments[1].isbg = 0;
    Render_legacyDrawEnvironments[0].isbg = 0;
    setRGB0(&Render_legacyDrawEnvironments[0], arg3, arg4, arg5);
    setRGB0(&Render_legacyDrawEnvironments[1], arg3, arg4, arg5);
    PutDispEnv(&Render_legacyDisplayEnvironments[Render_frameBuf]);
    PutDrawEnv(&Render_legacyDrawEnvironments[Render_frameBuf]);
}

// decomp: BATTLE.PRG func_8007629C (src/BATTLE/BATTLE.PRG/146C.c:5988)
int Render_PresentFieldFrame(u_long* otag)
{
    int ret;
    u_int temp_a0;

    Render_frameBuf = Render_frameBuf == 0;
    Core_Random_NextBounded(0);
    Core_Gte_SetGeomOffset(160, 112);
    ret = State_UpdatePlayTimeAndLoading(State_tickSpeed);
    if (Render_isFrameStepPaused != 0) {
        while (1) {
            State_UpdatePlayTimeAndLoading(2);
            Input_buttonsState = (u_short)Input_ReadControllerPort(0, *Input_legacyPadResponseBytes);
            Input_buttonsState |= Input_ReadControllerPort(16, Input_legacyPadResponseBytes[1])
                                 << 16;
            Input_UpdateControllerConnection(0, Input_legacyPadResponseBytes[0]);
            Input_UpdateControllerConnection(16, Input_legacyPadResponseBytes[1]);
            Input_buttonsPressed.allButtons =
                ~Input_previousButtons & Input_buttonsState;
            temp_a0 = Input_buttonsPressed.allButtons >> 16;
            Input_releasedButtons =
                Input_previousButtons & ~Input_buttonsState;
            Input_previousButtons = Input_buttonsState;
            if (temp_a0 & 0x800) {
                Render_isFrameStepPaused ^= 1;
            }
            if ((temp_a0 & 0x100) || (Render_isFrameStepPaused == 0)) {
                break;
            }
        }
    }
    PutDispEnv(&Render_legacyDisplayEnvironments[Render_frameBuf]);
    PutDrawEnv(&Render_legacyDrawEnvironments[Render_frameBuf]);
    Render_CommitClutBuffer();
    DrawOTag(otag);
    return ret;
}

// decomp: BATTLE.PRG func_8007C404 (src/BATTLE/BATTLE.PRG/146C.c:8770)
void Render_EnableDisplay(void) { Render_ShowDisplay(); }

// decomp: BATTLE.PRG func_8007C424 (src/BATTLE/BATTLE.PRG/146C.c:8772)
void Render_DisableDisplay(void) { Render_HideDisplay(); }

// decomp: BATTLE.PRG func_80089CE4 (src/BATTLE/BATTLE.PRG/146C.c:15250)
void Render_ShowDisplay(void) { SetDispMask(1); }

// decomp: BATTLE.PRG func_80089D04 (src/BATTLE/BATTLE.PRG/146C.c:15252)
void Render_HideDisplay(void) { SetDispMask(0); }

// decomp: SLUS_010.40 _initScreen (src/SLUS_010.40/main.c:8069)
void Render_InitializeLoadingDisplay(int w, int h, int arg2 __attribute__((unused)),
    __attribute__((unused)) int arg3, __attribute__((unused)) int arg4,
    __attribute__((unused)) int arg5)
{
    SetDefDispEnv(Render_legacyDisplayEnvironments, 0, 0, w, h - 16);
    setRECT(&Render_legacyDisplayEnvironments[0].screen, 0, 8, 256, 224);
    PutDispEnv(Render_legacyDisplayEnvironments);
}

// decomp: src/SLUS_010.40/main.c:8078
#define IMG_W(header) (header.widthLow + (header.widthHigh << 8))
// decomp: src/SLUS_010.40/main.c:8079
#define IMG_H(header) (header.heightLow + (header.heightHigh << 8))
// decomp: SLUS_010.40 _displayLoadingScreen (src/SLUS_010.40/main.c:8081)
void Render_ShowLoadingScreen(void)
{
    RECT rect;

    Render_InitializeLoadingDisplay(0x140, 0xF0, Render_projectionDistance, 0, 0, 0);
    setRECT(&rect, 0, 0, 1024, 512);
    ClearImage2(&rect, 0, 0, 0);
    VSync(2);
    setRECT(&rect, (320 - IMG_W(Render_legacyLoadingImageHeader)) / 2,
        (224 - IMG_H(Render_legacyLoadingImageHeader)) / 2, IMG_W(Render_legacyLoadingImageHeader),
        IMG_H(Render_legacyLoadingImageHeader));
    LoadImage(&rect, (u_long*)&Render_legacyLoadingImageHeader + 1);
    setRECT(&rect, ((320 - IMG_W(Render_legacyLoadingImageHeader)) / 2) + 320,
        (224 - IMG_H(Render_legacyLoadingImageHeader)) / 2, IMG_W(Render_legacyLoadingImageHeader),
        IMG_H(Render_legacyLoadingImageHeader));
    LoadImage(&rect, (u_long*)&Render_legacyLoadingImageHeader + 1);
    SetDispMask(1);
}
#undef IMG_H
#undef IMG_W

// decomp: src/SLUS_010.40/main.c:8078
#define IMG_W(header) (header.widthLow + (header.widthHigh << 8))
// decomp: src/SLUS_010.40/main.c:8079
#define IMG_H(header) (header.heightLow + (header.heightHigh << 8))
// decomp: SLUS_010.40 _bufferLoadingScreen (src/SLUS_010.40/main.c:8102)
void Render_UploadLoadingScreenBackBuffer(void)
{
    RECT rect;

    if (Render_frameBuf != 0) {
        setRECT(&rect, (320 - IMG_W(Render_legacyLoadingImageHeader)) / 2,
            (224 - IMG_H(Render_legacyLoadingImageHeader)) / 2, IMG_W(Render_legacyLoadingImageHeader),
            IMG_H(Render_legacyLoadingImageHeader));
    } else {
        setRECT(&rect, (320 - IMG_W(Render_legacyLoadingImageHeader)) / 2 + 320,
            (224 - IMG_H(Render_legacyLoadingImageHeader)) / 2, IMG_W(Render_legacyLoadingImageHeader),
            IMG_H(Render_legacyLoadingImageHeader));
    }
    LoadImage(&rect, Render_loadingScreenPixels);
}
#undef IMG_H
#undef IMG_W

