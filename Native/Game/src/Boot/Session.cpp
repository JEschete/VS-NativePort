// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Boot/Session.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "Audio/Control/SoundControl.h"
#include "Audio/Music.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/SoundBanks.h"
#include "Battle/EncounterHooks.h"
#include "Battle/Targeting.h"
#include "Boot/GameLoop.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "Core/Overlays.h"
#include "Core/Random.h"
#include "Core/Runtime.h"
#include "DiscQueue.h"
#include "DiscReader.h"
#include "Ending/Credits/StaffRoll.h"
#include "Event/ScriptVm.h"
#include "Input/Controller.h"
#include "Input/Controllers.h"
#include "Legacy/Psx.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Save/NativeResume.h"
#include "Save/Snapshot.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/PlayTime.h"
#include "Title/TitleMenu.h"

static jmp_buf session_boundary;
static int session_active;

// Native sessions can interrupt original event text. Drop UI borrows before
// title/battle initialization replaces the game heap (SLUS main.c:8119/8344).
static void InvalidateSceneUiBindings(void) {
  memset(Menu_textBoxes, 0, sizeof Menu_textBoxes);
  memset(&Menu_textBoxSelector, 0, sizeof Menu_textBoxSelector);
  memset(&Battle_stringContext, 0, sizeof Battle_stringContext);
  memset(Event_isTextBoxAwaitingClose, 0, sizeof Event_isTextBoxAwaitingClose);
  Menu_sceneNotificationBuffer = nullptr;
  Menu_sceneNotificationTimer = 0;
  Menu_sceneNotificationTimed = 0;
}

// Nonlocal transitions stay inside the copied C call graph; the platform owns
// the surrounding fiber.
int Boot_RunSessionLoop(Boot_SessionEntry title, Boot_SessionEntry battle) {
  if (session_active || title == nullptr || battle == nullptr) return -1;
  InvalidateSceneUiBindings();
  session_active = 1;
  Boot_ResetGameplayInspection();
  const int destination = setjmp(session_boundary);
  const int result = destination == 2 ? battle() : title();
  InvalidateSceneUiBindings();
  Save_NativeResetSession();
  session_active = 0;
  Boot_ResetGameplayInspection();
  return result;
}

static void RequestSession(int destination) {
  if (!session_active) abort();
  InvalidateSceneUiBindings();
  Boot_ResetGameplayInspection();
  Core_Debugf("boot", "native session transition: %s",
              destination == 1 ? "title" : "battle");
  longjmp(session_boundary, destination);
}

static int RunLoadedBattle(void) {
  Boot_ResetGameplayInspection();
  Boot_isGameplayActive = 1;
  Boot_RunGameLoop();
  Boot_ResetGameplayInspection();
  SetDispMask(0);
  Audio_StopAndFreeCurrentMusic();
  return 0;
}

// decomp: SLUS_010.40 vs_main_execTitle (src/SLUS_010.40/main.c:8344)
int Boot_RunTitleAndBattle(void) {
  Core_Debugf("boot", "title session initialization begin");
  Boot_InitializeTitleSession();
  Core_Debugf("boot", "title session initialization complete");
  Boot_LoadTitleOverlay();
  Core_Debugf("boot", "title module ready");
  Boot_gameEntryMode = Title_Run();
  Boot_isClearSavePending = 0;
  Render_ShowLoadingScreen();
  Boot_LoadBattleOverlays();
  return RunLoadedBattle();
}

// decomp: SLUS_010.40 vs_main_exec (src/SLUS_010.40/main.c:8361)
void Boot_RunGame(void) {
  Boot_InitializePlatformServices();
  Boot_RunSessionLoop(Boot_RunTitleAndBattle, RunLoadedBattle);
}

// decomp: SLUS_010.40 vs_overlay_jumpToTitle (src/SLUS_010.40/overlay.c:20).
void Boot_JumpToTitle(int* legacy_stack) {
  (void)legacy_stack;
  Save_NativeResetSession();
  RequestSession(1);
}

// decomp: SLUS_010.40 vs_overlay_jumpToBattle (src/SLUS_010.40/overlay.c:21).
void Boot_JumpToGameLoop(int* legacy_stack) {
  (void)legacy_stack;
  RequestSession(2);
}

// decomp: SLUS_010.40 vs_overlay_wait (src/SLUS_010.40/overlay.c:23); loaded
// native code needs no 4096-NOP cache delay.
void Boot_DelayAfterOverlayLoad(void) {}
}  // extern "C"
