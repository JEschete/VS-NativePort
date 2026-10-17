// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "State/Flags.h"
#include "State/StateFlags.h"

// decomp: BATTLE.PRG func_800BEB9C (src/BATTLE/BATTLE.PRG/4A0A8.c:4263)
void State_ResetZoneScriptFlags(int arg0)
{
    if (arg0 == 0) {
        short i;
        for (i = 64; i < 128; ++i) {
            State_SetScriptFlag(i, 0);
        }
    }
}

// decomp: BATTLE.PRG vs_battle_getStateFlag (src/BATTLE/BATTLE.PRG/4A0A8.c:4275)
char State_ScriptFlag(short arg0)
{
    char* flags = (char*)&Audio_Seq_unusedStateFlags;
    return flags[arg0];
}

// decomp: BATTLE.PRG vs_battle_setStateFlag (src/BATTLE/BATTLE.PRG/4A0A8.c:4281)
void State_SetScriptFlag(short id, char value)
{
    char* flags = (char*)&Audio_Seq_unusedStateFlags;
    flags[id] = value;
}

