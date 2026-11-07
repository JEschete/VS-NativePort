// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTVM_H_TYPES
#define VS_EVENT_SCRIPTVM_H_TYPES
#include "Legacy/Psx.h"


// decomp: D_800F4B8B (D_800F4B8B@BATTLE/BATTLE.PRG): a view into another global
#define Event_weatherTweenDuration (*(char*)(&Event_weatherTween.durationFrames))

#endif // VS_EVENT_SCRIPTVM_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTVM_H_DECLS)
#define VS_EVENT_SCRIPTVM_H_DECLS

extern char Event_skipPaletteResetCommand[336];
extern char Event_kSkipSoundRestoreMarker[328];
extern short Event_kSceneTimerModes[158];
extern short Event_kSceneTimerResetScopes[156];
extern short Event_kSpecialOperationTenArguments[152];
extern u_char Event_kOpcodeSizes[256];
extern intptr_t (*Event_kOpcodeHandlers[256])(u_char*, short);
extern u_char* Event_textBoxFrameStyles[16];
extern u_short Event_customEasingCurves[66];
extern u_short* Event_dialogStrings;
extern char Event_cachedFlagValue;
extern short Event_isFieldCameraRestoreScripted;
extern char Event_isStartConfirmed;
extern char* Event_vibrationPatterns[10];
extern u_short Event_eventSource;
extern short Event_roomChangeState;
extern char Event_isScanlineOverlayEnabled;
extern char Event_isMenuTransitionActive;
extern char Event_isTextBoxAwaitingClose[80];
extern char Event_loopCounters[64];
extern char Event_stopsMusicOnSkip;
extern short Event_isMainThreadFinished;
extern void* Event_chainedEventBuffer;
extern void* Event_scriptBuffer;
extern char Event_playbackPhase;
extern u_char* Event_threadCursors[6];
extern char Event_weaponTransitionPending;
extern short Event_eventId;
extern short Event_isChainedEventLoaded;
extern short Event_threadWaitFrames[4];
extern char Event_isTextSelectionActive;
extern uintptr_t Event_subroutineReturnCursor;
extern char Event_isSkipForced;
extern char Event_isScriptBackdropEnabled;
extern intptr_t (*Event_kSkipOpcodeHandlers[256])(u_char*, short);
void Event_RestoreScriptTextureRectangle(void);
intptr_t Event_ScriptReportInvalidOpcode(u_char* instruction, short thread_index);
intptr_t Event_ScriptWaitForEventFile(u_char* instruction, short thread_index);
intptr_t Event_ScriptWaitForButtonAndYield(u_char* instruction, short thread_index);
intptr_t Event_ScriptConfigureTimer(u_char* instruction, short thread_index);
intptr_t Event_ScriptWaitForButtonOrTimeout(u_char* instruction, short thread_index);
intptr_t Event_ScriptConfigureEasingCurve(u_char* instruction, short thread_index);
intptr_t Event_CommandSpecialOperation(u_char* instruction, short thread_index);
intptr_t Event_CommandLoadNextEvent(u_char* instruction, short thread_index);
intptr_t Event_CommandFinishEvent(u_char* instruction, short thread_index);
int Event_EventPlaybackState(void);
short Event_PendingEventSelector(void);
int Event_RequestEventStart(void);
int Event_OpcodeSize(u_char* instruction);
int Event_ResolveScriptActorReference(u_short actor_reference);
void Event_QueueEventScriptLoad(int arg0, void* destination_buffer);
void Event_ResetCutsceneActorsToIdle(void);
void Event_InitializeScriptOpcodeTable(void);

#endif // VS_EVENT_SCRIPTVM_H_DECLS
