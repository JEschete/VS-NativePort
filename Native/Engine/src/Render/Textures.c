// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Runtime.h"
#include "OverlayWorkspace.h"
#include "Render/Render.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"

// decomp: D_800F19A8 (D_800F19A8@BATTLE/BATTLE.PRG)
u_char Render_legacyTexturePageReservations[40];

// decomp: D_800F46A8 (D_800F46A8@BATTLE/BATTLE.PRG)
Render_TextureSlot Render_legacyActorTextureSlots[25];

// decomp: vs_battle_wepTextures (vs_battle_wepTextures@BATTLE/BATTLE.PRG)
Render_TextureSlot Render_legacyWeaponTextureSlots[20];

// decomp: BATTLE.PRG func_8007DF48 (src/BATTLE/BATTLE.PRG/146C.c:9480)
void Render_InitializeTextureSlotReservations(void)
{
    int i;

    for (i = 0; i < 0x20; ++i) {
        Render_legacyTexturePageReservations[i] = 0;
    }

    for (i = 0; i < 0x19; ++i) {
        Render_legacyTexturePageReservations[i] = 0xFF;
    }

    Content_overlayWorkspaceOwner = 0;
}

// decomp: BATTLE.PRG func_8007DF98 (src/BATTLE/BATTLE.PRG/146C.c:9495)
void Render_AdvanceTexturePageRetirements(void)
{
    int i;

    for (i = 0; i < 32; ++i) {
        if (Render_legacyTexturePageReservations[i] == 0xFE) {
            Render_legacyTexturePageReservations[i] = 0;
        }
        if (Render_legacyTexturePageReservations[i] == 0xFD) {
            Render_legacyTexturePageReservations[i] = -2;
        }
    }
}

// decomp: BATTLE.PRG func_8007DFF0 (src/BATTLE/BATTLE.PRG/146C.c:9509)
int Render_ReserveTexturePageRange(u_int arg0, u_int arg1, int arg2)
{
    if (arg0 > 0x18) {
        int var_a3 = 0;
        if ((arg0 + arg1) < 0x21) {
            u_int i;
            for (i = 0; i < arg1; ++i) {
                if (Render_legacyTexturePageReservations[arg0 + i] != 0) {
                    var_a3 = 1;
                }
            }
            if (var_a3 == 0) {
                for (i = 0; i < arg1; ++i) {
                    Render_legacyTexturePageReservations[arg0 + i] = (char)arg2;
                }
                return arg0;
            }
        }
    }
    Core_IgnoreDiagnostic(100, 0);
    return 0;
}

// decomp: BATTLE.PRG func_8007E0A8 (src/BATTLE/BATTLE.PRG/146C.c:9532)
int Render_RetireTexturePageRange(u_int arg0, u_int arg1, int arg2)
{
    int var_s0 = 0;

    if ((25 <= arg0) && ((arg0 + arg1) < 33)) {
        int i;

        for (i = 0; i < arg1; ++i) {
            if (Render_legacyTexturePageReservations[arg0 + i] != arg2) {
                var_s0 = 1;
            }
        }

        if (var_s0 != 0) {
            Core_IgnoreDiagnostic(101, 0);
        } else {
            if (var_s0 < arg1) {
                for (i = 0; i < arg1; ++i) {
                    Render_legacyTexturePageReservations[arg0 + i] = -3;
                }
            }
        }

        return !var_s0;
    }

    Core_IgnoreDiagnostic(101, 0);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_parseTim (src/BATTLE/BATTLE.PRG/146C.c:16935)
void Render_ParseTextureImage(u_long* arg0, TIM_IMAGE* tim)
{
    ++arg0;
    tim->mode = *arg0;
    ++arg0;

    if (tim->mode & 8) {
        tim->crect = (RECT*)(arg0 + 1);
        tim->caddr = (u_long*)arg0 + 3;
        arg0 += *arg0 >> 2;
    } else {
        tim->crect = NULL;
        tim->caddr = NULL;
    }

    tim->prect = (RECT*)(arg0 + 1);
    tim->paddr = arg0 + 3;
}

// decomp: BATTLE.PRG vs_battlePacketBegin (src/BATTLE/BATTLE.PRG/40564.c:110)
DR_MOVE* Render_BeginTextureMovePacket(void)
{
    return ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
}

// decomp: BATTLE.PRG vs_battlePacketEnd (src/BATTLE/BATTLE.PRG/40564.c:114)
void Render_EndTextureMovePacket(DR_MOVE* p)
{
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = p + 1;
}

// decomp: BATTLE.PRG func_800AAC80 (src/BATTLE/BATTLE.PRG/40564.c:1272)
void Render_QueueTextureRectangleMove(RECT* arg0, int arg1, int arg2)
{
    DR_MOVE* drMove = *(DR_MOVE**)((uintptr_t)Core_ScratchBase() + 0x000);
    SetDrawMove(drMove, arg0, arg1, arg2);
    ((void (*)(DR_MOVE*))Render_AppendOverlayPrimitive)(drMove);
    *(DR_MOVE**)((uintptr_t)Core_ScratchBase() + 0x000) = drMove + 1;
}

