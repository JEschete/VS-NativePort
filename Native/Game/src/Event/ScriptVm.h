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
extern int (*Event_kOpcodeHandlers[572])(u_char*, short);
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
extern char Event_reservedResetByte;
extern short Event_eventId;
extern short Event_isChainedEventLoaded;
extern short Event_threadWaitFrames[4];
extern char Event_isTextSelectionActive;
extern uintptr_t Event_subroutineReturnCursor;
extern char Event_isSkipForced;
extern char Event_isScriptBackdropEnabled;
extern int (*Event_kSkipOpcodeHandlers[])(u_char*, short);
void Event_RestoreScriptTextureRectangle(void);
int Event_ScriptReportInvalidOpcode(u_char* arg0, short arg1);
int Event_ScriptNoOperation(u_char* arg0, short arg1);
int Event_ScriptYieldFrame(u_char* arg0, short arg1);
int Event_ScriptWaitForEventFile(u_char* arg0, short arg1);
int Event_ScriptBeginSkippableCutscene(u_char* arg0, short arg1);
void Event_FinishSkippedCutscene(u_char* arg0);
intptr_t Event_ScriptEndCutsceneAndJump(u_char* arg0, short arg1);
int Event_ScriptEndCutscene(u_char* arg0, short arg1);
int Event_ScriptWaitForButtonAndYield(u_char* arg0, short arg1);
intptr_t Event_ScriptBranchOnFlagImmediate(u_char* arg0, short arg1);
intptr_t Event_ScriptBranchOnFlagPair(u_char* arg0, short arg1);
intptr_t Event_ScriptJumpForward(u_char* arg0, short arg1);
int Event_ScriptCacheFlagValue(u_char* arg0, short arg1);
intptr_t Event_ScriptBranchOnCachedFlag(u_char* arg0, short arg1);
int Event_ScriptReturnToSavedAddress(u_char* arg0, short arg1);
int Event_ScriptSetLoopCounter(u_char* arg0, short arg1);
intptr_t Event_ScriptLoopBackward(u_char* arg0, short arg1);
int Event_ScriptApplyImmediateFlagOperation(u_char* arg0, short arg1);
int Event_ScriptApplyFlagPairOperation(u_char* arg0, short arg1);
int Event_ScriptStoreInventoryItemPresence(u_char* arg0, short arg1);
int Event_ScriptStoreButtonState(u_char* arg0, short arg1);
int Event_ScriptConfigureTimer(u_char* arg0, short arg1);
int Event_ScriptWaitForButtonOrTimeout(u_char* arg0, short arg1);
int Event_ScriptConfigureEasingCurve(u_char* arg0, short arg1);
intptr_t Event_CommandSpecialOperation(u_char* arg0, short arg1);
int Event_CommandWaitFrames(u_char* arg0, short arg1);
int Event_CommandSetTickSpeed(u_char* arg0, short arg1);
int Event_CommandWaitScriptContext(u_char* arg0, short arg1);
intptr_t Event_CommandStartScriptContext(u_char* arg0, short arg1);
int Event_CommandStopScriptContext(u_char* arg0, short arg1);
int Event_CommandLoadNextEvent(u_char* arg0, short arg1);
int Event_CommandFinishEvent(u_char* arg0, short arg1);
int Event_EventPlaybackState(void);
short Event_PendingEventSelector(void);
int Event_RequestEventStart(void);
void Event_InitializeEventScriptData(u_char* data);
int Event_UpdateEventPlayback(void);
short Event_ExecuteScriptThread(u_char** arg0, short arg1);
void Event_ReleaseScriptActorControl(void);
int Event_OpcodeSize(u_char* arg0);
int Event_ResolveScriptActorReference(u_short arg0);
int Event_EvaluateInterpolationCurve(short mode, short frame, short duration);
void Event_QueueEventScriptLoad(int arg0, void* arg1);
void Event_ResetCutsceneActorsToIdle(void);
void Event_InitializeScriptOpcodeTable(void);

#endif // VS_EVENT_SCRIPTVM_H_DECLS
