// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/SoundBanks.h"
#include "Audio/Stream.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"

// decomp: SLUS_010.40 vs_main_streamXa (src/SLUS_010.40/main.c:9358)
int Audio_BeginXaStream(int sector, int byteCount)
{
    if (Content_discDriveState.state == Content_DiscDriveState_Idle) {
        Content_discDriveState.startSector = sector;
        Content_discDriveState.byteCount = byteCount;
        Content_discDriveState.ringReadIndex = 0;
        Content_discDriveState.ringWriteIndex = 0;
        Content_discDriveState.destinationSector = 0;
        Content_discDriveState.bufferedSectors = 0;
        Content_discDriveState.sectorsReceived = 0;
        Content_discDriveState.failureCount = 0;
        Content_discDriveState.ringSectorsWritten = 0;
        Content_discDriveState.state = Content_DiscDriveState_ReadXaInit;
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_checkStreamXaEnd (src/SLUS_010.40/main.c:9376)
int Audio_RequestXaStreamStop(void)
{
    if (Content_discDriveState.state == Content_DiscDriveState_StreamXa) {
        Content_discDriveState.state = Content_DiscDriveState_StreamXaEnd;
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 func_800464DC (src/SLUS_010.40/main.c:10310)
void Audio_StopCutsceneStream(void) { Audio_Control_StopCutsceneStream(); }

// decomp: SLUS_010.40 func_800464FC (src/SLUS_010.40/main.c:10312)
int Audio_PlayCutsceneStreamSlot(int arg0, int arg1, int arg2)
{
    if ((arg0 - 1u) < 3) {
        if (Audio_soundState.sfxStreamData[arg0 - 1] != 0) {
            Audio_Control_FadeCutsceneVolume(0, arg2);
            Audio_Control_StartCutsceneStream(Audio_soundState.sfxStreamData[arg0 - 1], arg1, 1);
            return 1;
        }
    }
    return 0;
}

// decomp: SLUS_010.40 func_80046578 (src/SLUS_010.40/main.c:10324)
void Audio_PlayCutsceneStreamSlotCentered(int arg0) { Audio_PlayCutsceneStreamSlot(arg0, 0x80, 0x7F); }

// decomp: SLUS_010.40 func_8004659C (src/SLUS_010.40/main.c:10326)
int Audio_FreeCutsceneStreamSlot(int arg0)
{
    if ((arg0 - 1u) < 3) {
        if (Audio_soundState.sfxStreamData[arg0 - 1] != 0) {
            Audio_Control_StopCutsceneStream();
            Core_Memory_FreeFromHighEnd(Audio_soundState.sfxStreamData[arg0 - 1]);
            Audio_soundState.sfxStreamData[arg0 - 1] = 0;
            return 1;
        }
    }
    return 0;
}

// decomp: SLUS_010.40 func_800468DC (src/SLUS_010.40/main.c:10450)
void Audio_SetXaVolumeMaximum(void) { Audio_Control_SetDiscVolume(0x7FFF); }

