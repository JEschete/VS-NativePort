// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Boot/Session.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Runtime.hpp"
#include "Input/ControllerLive.hpp"
#include "Render/DisplayControlLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/SequencerOutput.h"
#include "Audio/SoundBanks.h"
#include "Battle/Ai/RoomState.h"
#include "Boot/GameLoop.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Ending/Credits/StaffRoll.h"
#include "Input/ControllerLiveState.h"
#include "Input/Controllers.h"
#include "Save/NativeResume.h"
#include "Save/Snapshot.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/PlayTime.h"

// decomp: SLUS_010.40 _loadBattlePrg (src/SLUS_010.40/main.c:8002)
void Boot_LoadBattleOverlays(void)
{
    Content_DiscFile cdFile;
    Content_DiscQueueSlot* slot;

    cdFile.lba = VS_BATTLE_PRG_LBA;
    cdFile.size = VS_BATTLE_PRG_SIZE;
    slot = Content_AllocateDiscRequest(&cdFile);
    Content_EnqueuePriorityDiscRequest(slot, Core_overlaySlots[0]);

    while (slot->state != Content_DiscRequestState_Loaded) {
        State_UpdatePlayTimeAndLoading(0);
    }

    Content_ReleaseDiscRequest(slot);

    cdFile.lba = VS_INITBTL_PRG_LBA;
    cdFile.size = VS_INITBTL_PRG_SIZE;
    slot = Content_AllocateDiscRequest(&cdFile);
    Content_EnqueuePriorityDiscRequest(slot, Core_overlaySlots[1]);

    while (slot->state != Content_DiscRequestState_Loaded) {
        State_UpdatePlayTimeAndLoading(0);
    }

    Content_ReleaseDiscRequest(slot);
    Boot_WaitForOverlayReady();
}

// decomp: SLUS_010.40 _loadTitlePrg (src/SLUS_010.40/main.c:8031)
void Boot_LoadTitleOverlay(void)
{
    Content_DiscFile cdFile;
    Content_DiscQueueSlot* slot;

    cdFile.lba = VS_TITLE_PRG_LBA;
    cdFile.size = VS_TITLE_PRG_SIZE;

    slot = Content_AllocateDiscRequest(&cdFile);
    Content_EnqueuePriorityDiscRequest(slot, Core_overlaySlots[0]);

    while (slot->state != Content_DiscRequestState_Loaded) {
        State_UpdatePlayTimeAndLoading(0);
    }

    Content_ReleaseDiscRequest(slot);
    Boot_WaitForOverlayReady();
}

// decomp: SLUS_010.40 _loadEndingPrg (src/SLUS_010.40/main.c:8050)
void Boot_LoadEndingOverlay(void)
{
    Content_DiscFile cdFile;
    Content_DiscQueueSlot* slot;

    cdFile.lba = VS_ENDING_PRG_LBA;
    cdFile.size = VS_ENDING_PRG_SIZE;

    slot = Content_AllocateDiscRequest(&cdFile);
    Content_EnqueuePriorityDiscRequest(slot, Core_overlaySlots[0]);

    while (slot->state != Content_DiscRequestState_Loaded) {
        State_UpdatePlayTimeAndLoading(0);
    }

    Content_ReleaseDiscRequest(slot);
    Boot_WaitForOverlayReady();
}

// decomp: SLUS_010.40 vs_main_resetGame (src/SLUS_010.40/main.c:8119)
void Boot_ResetToTitle(void)
{
    Save_NativeResetSession();
    SetDispMask(0);
    VS::Input::SetLiveMotorCommand(0, 0, 0);
    VS::Input::SetLiveMotorCommand(0, 1, 0);
    VS::Input::UpdateLiveControllerConnection(0, Input_legacyPadResponseBytes[0]);
    VS::Input::UpdateLiveControllerConnection(0x10, Input_legacyPadResponseBytes[1]);
    Audio_Playback_Shutdown();
    VSync(10);
    VS::Boot::JumpToLiveTitle();
}

// decomp: SLUS_010.40 vs_main_jumpToBattle (src/SLUS_010.40/main.c:8138)
void Boot_EnterBattle(void)
{
    SetDispMask(0);
    VS::Input::SetLiveMotorCommand(0, 0, 0);
    VS::Input::SetLiveMotorCommand(0, 1, 0);
    VS::Input::UpdateLiveControllerConnection(0, Input_legacyPadResponseBytes[0]);
    VS::Input::UpdateLiveControllerConnection(0x10, Input_legacyPadResponseBytes[1]);
    Audio_Control_StopAllMusic();
    Audio_Control_StopSceneSoundEffects();
    Audio_Playback_InvalidateSavedMusicId();
    Audio_Samples_ClearMusicBankIds();
    VS::Render::ShowLiveLoadingScreen();
    Boot_LoadBattleOverlays();
    Boot_gameEntryMode = 1;
    VS::Boot::JumpToLiveGameLoop();
}

// decomp: SLUS_010.40 vs_main_showEndingAndReturnToTitle (src/SLUS_010.40/main.c:8158)
void Boot_RunEndingAndReturnToTitle(void)
{
    Boot_isGameplayActive = 0;
    SetDispMask(0);
    VS::Input::SetLiveMotorCommand(0, 0, 0);
    VS::Input::SetLiveMotorCommand(0, 1, 0);
    VS::Input::UpdateLiveControllerConnection(0, Input_legacyPadResponseBytes[0]);
    VS::Input::UpdateLiveControllerConnection(0x10, Input_legacyPadResponseBytes[1]);
    Audio_Control_StopAllMusic();
    Audio_Control_StopSceneSoundEffects();
    Save_CaptureAshleySnapshot();
    Boot_LoadEndingOverlay();
    Ending_Credits_RunStaffRoll();
    SetDispMask(0);
    VS::Input::SetLiveMotorCommand(0, 0, 0);
    VS::Input::SetLiveMotorCommand(0, 1, 0);
    VS::Input::UpdateLiveControllerConnection(0, Input_legacyPadResponseBytes[0]);
    VS::Input::UpdateLiveControllerConnection(0x10, Input_legacyPadResponseBytes[1]);
    Audio_Playback_Shutdown();
    Boot_isClearSavePending = 1;
    VS::Boot::JumpToLiveTitle();
}

// decomp: SLUS_010.40 _sysInit (src/SLUS_010.40/main.c:8286)
void Boot_InitializePlatformServices(void)
{
    SetVideoMode(MODE_NTSC);
    SetDispMask(0);
    InitCARD(0);
    StartCARD();
    PadInitDirect((unsigned char *)(Input_legacyPadResponseBytes[0]), (unsigned char *)(Input_legacyPadResponseBytes[1]));
    VS::Input::ConfigureLiveControllerActuators(0, Input_legacyPadResponseBytes[0]);
    VS::Input::ConfigureLiveControllerActuators(0x10, Input_legacyPadResponseBytes[1]);
    PadStartCom();
    VS::Input::UnlockLiveAnalogModeSwitch();
    VS::Input::ResetLiveVibrationPatterns();
    Audio_Native_EnableReverb(0);
    DsInit();
    VS::Core::InitializeLiveRandomShuffleTable();
    Boot_isSoftResetEnabled = 1;
    Boot_isClearSavePending = 0;
}

// decomp: SLUS_010.40 _sysReinit (src/SLUS_010.40/main.c:8309)
void Boot_InitializeTitleSession(void)
{
    Save_NativeResetSession();
    int i;
    RECT rect0 __attribute__((unused));
    RECT rect1 __attribute__((unused)) = { .x = 819, .y = 819, .w = 819, .h = 0 };
    RECT rect2 = { .x = 0, .y = 0, .w = 1024, .h = 512 };

    ClearImage(&rect2, 0, 0, 0);
    VS::Render::ShowLiveLoadingScreen();
    Audio_Native_EnableReverb(1);
    VSyncCallback(VS::Core::IgnoreCall);
    VS::Core::Memory::ResetLiveGameHeap();
    Battle_Ai_InvalidateSessionBindings();
    Content_InitializeDiscQueue();
    VS::Content::ResetLiveDiscDrive();
    Audio_InitializeMenuSoundBanks();
    VS::Input::ResetLiveVibrationPatterns();
    Boot_isGameplayActive = 0;
    VS::Input::UnlockLiveAnalogModeSwitch();

    for (i = 31; i >= 0; --i) {
        Input_buttonHeldFrames[i] = 0;
    }
    State_playTime.t.hours = 0;
    State_playTime.t.minutes = 0;
    State_playTime.t.seconds = 0;
    State_playTime.t.frames = 0;
    State_ashleySnapshot.stats.isPending = 0;
}

// decomp: SLUS_010.40 vs_main_wait (src/SLUS_010.40/main.c:8369)
void Boot_WaitForOverlayReady(void) { VS::Boot::DelayAfterOverlayLoad(); }

// decomp: SLUS_010.40 vs_main_enableReset (src/SLUS_010.40/main.c:8371)
void Boot_SetSoftResetEnabled(int enable) { Boot_isSoftResetEnabled = enable; }

}  // extern "C"
