// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
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
int Render_ReserveTexturePageRange(u_int first_page, u_int page_count, int owner_tag)
{
    if (first_page > 0x18) {
        int range_occupied = 0;
        if ((first_page + page_count) < 0x21) {
            u_int i;
            for (i = 0; i < page_count; ++i) {
                if (Render_legacyTexturePageReservations[first_page + i] != 0) {
                    range_occupied = 1;
                }
            }
            if (range_occupied == 0) {
                for (i = 0; i < page_count; ++i) {
                    Render_legacyTexturePageReservations[first_page + i] = (char)owner_tag;
                }
                return first_page;
            }
        }
    }
    Core_IgnoreDiagnostic(100, 0);
    return 0;
}

// decomp: BATTLE.PRG func_8007E0A8 (src/BATTLE/BATTLE.PRG/146C.c:9532)
int Render_RetireTexturePageRange(u_int first_page, u_int page_count, int owner_tag)
{
    int owner_mismatch = 0;

    if ((25 <= first_page) && ((first_page + page_count) < 33)) {
        int i;

        for (i = 0; i < page_count; ++i) {
            if (Render_legacyTexturePageReservations[first_page + i] != owner_tag) {
                owner_mismatch = 1;
            }
        }

        if (owner_mismatch != 0) {
            Core_IgnoreDiagnostic(101, 0);
        } else {
            if (owner_mismatch < page_count) {
                for (i = 0; i < page_count; ++i) {
                    Render_legacyTexturePageReservations[first_page + i] = -3;
                }
            }
        }

        return !owner_mismatch;
    }

    Core_IgnoreDiagnostic(101, 0);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_parseTim (src/BATTLE/BATTLE.PRG/146C.c:16935)
void Render_ParseTextureImage(u_long* image_cursor, TIM_IMAGE* tim)
{
    ++image_cursor;
    tim->mode = *image_cursor;
    ++image_cursor;

    if (tim->mode & 8) {
        tim->crect = (RECT*)(image_cursor + 1);
        tim->caddr = (u_long*)image_cursor + 3;
        image_cursor += *image_cursor >> 2;
    } else {
        tim->crect = NULL;
        tim->caddr = NULL;
    }

    tim->prect = (RECT*)(image_cursor + 1);
    tim->paddr = image_cursor + 3;
}

// decomp: BATTLE.PRG vs_battlePacketBegin (src/BATTLE/BATTLE.PRG/40564.c:110)
DR_MOVE* Render_BeginTextureMovePacket(void)
{
    return (DR_MOVE *)(((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor);
}

// decomp: BATTLE.PRG vs_battlePacketEnd (src/BATTLE/BATTLE.PRG/40564.c:114)
void Render_EndTextureMovePacket(DR_MOVE* p)
{
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = p + 1;
}

// decomp: BATTLE.PRG func_800AAC80 (src/BATTLE/BATTLE.PRG/40564.c:1272)
void Render_QueueTextureRectangleMove(RECT* source_rectangle, int destination_x, int destination_y)
{
    DR_MOVE* drMove = *(DR_MOVE**)((uintptr_t)Core_ScratchBase() + 0x000);
    SetDrawMove(drMove, source_rectangle, destination_x, destination_y);
    ((void (*)(DR_MOVE*))Render_AppendOverlayPrimitive)(drMove);
    *(DR_MOVE**)((uintptr_t)Core_ScratchBase() + 0x000) = drMove + 1;
}

}  // extern "C"
