// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Copy.h"
#include "Event/Dialog.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptDialog.h"
#include "Event/ScriptVm.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "State/Flags.h"
#include "State/StateFlags.h"

// decomp: D_800E9BEC (D_800E9BEC@BATTLE/BATTLE.PRG)
u_char Event_kDialogPlacementOrder[324]; /* contents: module image (ModuleData.cpp) */

// decomp: BATTLE.PRG vs_battle_script_initTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:672)
intptr_t Event_ScriptInitDialogBox(u_char* script, short thread_index __attribute__((unused)))
{
    Menu_TextBoxState* box;
    short speaker_reference;
    short centerY;
    short centerX;

    if ((script[1] & 0xF) == (script[1] >> 4)) {
        centerX = 32;
        centerX = (script[4] * 2) + ((((script[6] * 12) + 8) >> 1) - centerX);
        centerY = script[5] + 0xFFE0 + (((script[7] * 13) + 4) >> 1);
    } else {
        box = Menu_TextBox((script[1] >> 4));
        centerX = box->x + 4;
        centerX += box->charsPerLine * 6;
        centerY = box->y + (((box->lineCount * 13) + 4) >> 1);
    }

    Menu_InitTextBox(script[1] & 0xF, Core_Memory_ReadUnalignedShort(script + 2),
        (script[4] * 2) - 32, script[5] - 32, script[6], script[7], centerX, centerY);

    box = Menu_TextBox(script[1] & 0xF);
    box->opaque2E = script[10];

    speaker_reference = Core_Memory_ReadUnalignedShort(&script[8]);
    if (speaker_reference < 0x2000) {
        box->flags.speakerTexture = Event_ResolveScriptActorReference(speaker_reference);
    }

    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_showTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:707)
intptr_t Event_ScriptShowDialogText(u_char* script, short thread_index __attribute__((unused)))
{
    int text_result;

    if (Event_isTextBoxAwaitingClose[script[1]] == 0) {
        Menu_SetTextBoxText(script[1], (char*)&Event_dialogStrings[Event_dialogStrings[script[2]]]);
        if (script[3] == 0) {
            Event_isTextBoxAwaitingClose[script[1]] = 1;
            return 1;
        }
        return 0;
    }

    text_result = Menu_SetTextBoxText(script[1], 0);

    if (text_result >= 0) {
        if (text_result > 0) {
            Audio_Seq_unusedStateFlags.textBoxSelection = text_result;
        }
        Event_isTextBoxAwaitingClose[script[1]] = 0;
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG vs_battle_script_dismissTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:733)
intptr_t Event_ScriptDismissDialogBox(u_char* instruction, short thread_index)
{
    Menu_DismissTextBox(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B6B98 (src/BATTLE/BATTLE.PRG/4A0A8.c:741)
int Event_CalcDialogAnchorPlacement(
    SVECTOR* origin, u_char* script, Event_DialogPlacement* result, u_char direction)
{
    short dx;
    short dy;
    short width;
    short height;
    int delta;
    short x;
    short y;

    dx = 0;
    dy = 0;
    result->anchorOffset = ((Battle_TextAnchorOffset (*)(int, int, int, int))Menu_TextBoxAnchor)(
        script[5] & 0xF, (origin->vz ^ direction) * 4, script[6], script[7]);
    result->screenX = x = origin->vx + result->anchorOffset.x;
    result->screenY = y = origin->vy + result->anchorOffset.y;
    height = script[7] * 13 + 4;
    width = script[6] * 12 + 8;

    if (x < 4) {
        result->screenX = 4;
        delta = 4 - x;
        dx = delta;
    } else if (x + width > 320) {
        width = 320 - width;
        result->screenX = width;
        dx = width - x;
    }

    if (y < 4) {
        result->screenY = 4;
        delta = 4 - y;
        dy = delta;
    } else if (height + y > 216) {
        height = 216 - height;
        result->screenY = height;
        dy = height - y;
    }

    if (dx == 0 && dy == 0) {
        return 1;
    }
    result->clampDistance = Core_Math_SquareRootInteger(dx * dx + dy * dy);
    return 0;
}

// decomp: BATTLE.PRG func_800B6D48 (src/BATTLE/BATTLE.PRG/4A0A8.c:794)
intptr_t Event_ScriptInitActorDialogBox(u_char* script, short thread_index __attribute__((unused)))
{
    Event_DialogPlacement placements[4];
    SVECTOR origin;
    Menu_TextBoxState* box;
    Event_DialogPlacement* best = NULL;
    u_int width;
    u_int height;
    int texture;
    short i;

    Event_ActorDialogAnchor(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(script + 2)), &origin);
    origin.vz = ((origin.vz - ONE / 4) & 0xC00) >> 10;

    for (i = 0; i < 4; ++i) {
        if (Event_CalcDialogAnchorPlacement(&origin, script, &placements[i], Event_kDialogPlacementOrder[i])) {
            best = &placements[i];
            break;
        }
    }

    if (best == NULL) {
        best = placements;
        for (i = 1; i < 4; ++i) {
            if (best->clampDistance > placements[i].clampDistance) {
                best = &placements[i];
            }
        }
    }

    width = script[6] * 12 + 8;
    height = script[7] * 13 + 4;
    Menu_InitTextBox(script[1], Core_Memory_ReadUnalignedShort(script + 4), best->screenX, best->screenY,
        script[6], script[7], best->screenX + (width >> 1), best->screenY + (height >> 1));

    box = Menu_TextBox(script[1]);
    // BUG: texture is never initialized
    box->flags.speakerTexture = texture;
    box->flags.alignment = best->anchorOffset.alignment;
    box->flags.framePart = best->anchorOffset.opaque3Bit2;
    box->opaque2E = script[8];
    return 0;
}

// decomp: BATTLE.PRG func_800B6F8C (src/BATTLE/BATTLE.PRG/4A0A8.c:838)
intptr_t Event_ScriptWaitForDialogDismissal(u_char* instruction, short thread_index)
{
    return Menu_DismissTextBox(instruction[1]) != 0;
}

// decomp: BATTLE.PRG vs_battle_script_setStringContextInt (src/BATTLE/BATTLE.PRG/4A0A8.c:916)
intptr_t Event_ScriptSetDialogInteger(u_char* instruction, short thread_index)
{
    int source_flag_id = Core_Memory_ReadUnalignedShort(instruction + 2) & 0x3FF;
    switch (instruction[3] >> 6) {
    case 0:
        Battle_stringContext.integers[instruction[1]] = State_ScriptFlag(source_flag_id);
        break;
    case 1:
        Battle_stringContext.integers[instruction[1]] =
            (signed char)State_ScriptFlag(source_flag_id);
        break;
    case 2:
        Battle_stringContext.integers[instruction[1]] =
            State_ScriptFlag(source_flag_id)
            | ((State_ScriptFlag(source_flag_id + 1)) << 8);
        break;
    case 3:
        Battle_stringContext.integers[instruction[1]] =
            (short)(State_ScriptFlag(source_flag_id)
                    | ((State_ScriptFlag(source_flag_id + 1)) << 8));
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B7358 (src/BATTLE/BATTLE.PRG/4A0A8.c:941)
intptr_t Event_ScriptSetDialogTransitionDuration(u_char* instruction, short thread_index)
{
    Menu_TextBox(instruction[1] >> 5)->openDuration = instruction[1] & 0x1F;
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_selectTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:947)
intptr_t Event_ScriptSelectDialogChoice(u_char* script, short thread_index __attribute__((unused)))
{
    if (Event_skipState == 2) {
        return 0;
    }

    if (Event_isTextSelectionActive != 0) {
        short selected_choice = Menu_SelectTextBoxChoice(script[1]);
        if ((selected_choice << 0x10) != 0) {
            State_SetScriptFlag(0x10, selected_choice);
            Event_isTextSelectionActive = 0;
            return 0;
        }
    } else {
        Menu_SelectTextBoxChoice(script[1]);
        Event_isTextSelectionActive = 1;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800B9564 (src/BATTLE/BATTLE.PRG/4A0A8.c:1897)
intptr_t Event_ScriptSetDialogSpeaker(u_char* instruction, short thread_index)
{
    Menu_TextBoxState* text_box;
    u_short speaker_reference;
    int speaker_texture;

    speaker_reference = Core_Memory_ReadUnalignedShort(instruction + 2);
    text_box = Menu_TextBox(instruction[1] & 0xF);

    if (speaker_reference < 0x2000) {
        speaker_texture = Event_ResolveScriptActorReference(speaker_reference);
    } else {
        speaker_texture = 0x20;
    }
    text_box->flags.speakerTexture = speaker_texture;
    text_box->opaque2E = instruction[1] >> 4;
    return 0;
}

}  // extern "C"
