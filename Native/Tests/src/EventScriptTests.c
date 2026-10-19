#include <string.h>

#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Ai/RoomState.h"
#include "Boot/Session.h"
#include "Core/Memory/WordCopy.h"
#include "Event/Script.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
#include "Field/CollisionSampling.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomEffects.h"
#include "Legacy/Psx.h"
#include "Render/RoomLighting.h"

static int sessionTitleCalls;
static int sessionBattleCalls;
static int sessionFallthrough;

int Tests_RoomLightingSamples(void) {
  struct {
    unsigned ambient, floorCount, ceilingCount;
    struct {
      short bounds[4];
      short points[3][2];
      unsigned colors[3];
    } triangles[2];
  } lighting = {0x00332211,
                1,
                1,
                {{{0, 0, 128, 128},
                  {{0, 0}, {0, 128}, {128, 0}},
                  {0x0000FF, 0xFF0000, 0x00FF00}},
                 {{0, 0, 128, 128},
                  {{0, 0}, {0, 128}, {128, 0}},
                  {0x806040, 0x806040, 0x806040}}}};
  const Field_RoomRecord saved = Field_roomData;
  Field_roomData.isLoaded = 1;
  Field_roomData.lighting = &lighting;
  Field_roomData.header.lightingLength = sizeof lighting;
  Render_ColorCode color = {0, 0, 0, 0x5A};
  Render_SampleRoomLighting((32 << 16) | 32, 0, &color);
  int passed = color.red == 127 && color.green == 63 && color.blue == 63 &&
               color.code == 0x5A;
  Render_SampleRoomLighting((32 << 16) | 32, 1, &color);
  passed = passed && color.red == 64 && color.green == 96 && color.blue == 128;
  Render_SampleRoomLighting((200 << 16) | 200, 0, &color);
  passed = passed && color.red == 17 && color.green == 34 && color.blue == 51 &&
           color.code == 0;
  Field_roomData.lighting = NULL;
  Render_SampleRoomLighting(0, 0, &color);
  passed =
      passed && color.red == 128 && color.green == 128 && color.blue == 128;
  Field_roomData = saved;
  return passed;
}

_Static_assert(sizeof(Field_RoomEffectCommand) == 28,
               "room-effect command disk size");
_Static_assert(offsetof(Field_RoomEffectCommand, operands) == 4,
               "room-effect operands follow the disk header");
_Static_assert(sizeof(Field_RoomEffectObjectView) ==
                   sizeof(Field_RoomEffectObject),
               "room-effect runtime stride");
_Static_assert(offsetof(Field_RoomEffectObjectView, scriptCursor) ==
                   offsetof(Field_RoomEffectObject, scriptCursor),
               "room-effect runtime cursor view");

int Tests_RoomEffectCommandLayout(void) {
  int geometry[16] = {0};
  Field_RoomEffectObject object = {0};
  object.sceneEntry = geometry;
  int transform[7] = {28, 160, -320, 480, 1024, 2048, 3072};
  Field_SetRoomEffectTransform(&object, (void*)transform);
  int passed = object.position.vx == 160 && object.position.vy == -320 &&
               object.position.vz == 480 && geometry[1] == 10 &&
               geometry[2] == -20 && geometry[3] == 30 && geometry[4] == 1024 &&
               geometry[5] == 2048 && geometry[6] == 3072;
  unsigned velocity[5] = {20, 0x0020FFF0, 0x00070030, 0x00020001, 0x00040003};
  Field_SetRoomEffectVelocity(&object, (void*)velocity);
  passed = passed && object.velocity.vx == -16 && object.velocity.vy == 32 &&
           object.velocity.vz == 48 && object.velocity.pad == 7 &&
           object.angularVelocity.vx == 1 && object.angularVelocity.vy == 2 &&
           object.angularVelocity.vz == 3 && object.angularVelocity.pad == 4;
  return passed;
}

int Tests_CollisionHeightPatterns(void) {
  struct {
    Field_MpdCollisionHeader header;
    unsigned short floor[2];
    unsigned short ceiling[2];
    signed char patterns[2][16];
  } collision = {{2, 1, 0, 2}, {0x0800, 0x1801}, {0x5001, 0xF800}, {{0}}};
  collision.patterns[0][0] = -2;
  collision.patterns[0][1] = 5;
  collision.patterns[0][4] = 7;
  collision.patterns[1][0] = 8;
  collision.patterns[1][15] = -16;
  const Field_RoomRecord saved = Field_roomData;
  memset(&Field_roomData, 0, sizeof Field_roomData);
  Field_roomData.collisionMap = &collision.header;
  Field_roomData.header.collisionMapLength = sizeof collision;
  int passed = Field_SampleCollisionHeightCode(0, 0, 0) == 0x82 &&
               Field_SampleCollisionHeightCode(1, 0, 0) == 9 &&
               Field_SampleCollisionHeightCode(0, 1, 0) == 11 &&
               Field_SampleCollisionHeightCode(4, 0, 0) == 20 &&
               Field_SampleCollisionHeightCode(7, 3, 0) == 0xFC &&
               Field_SampleCollisionHeightCode(0, 0, 1) == 48 &&
               Field_SampleCollisionHeightCode(4, 0, 1) == 0xFA &&
               Field_SampleCollisionHeightCode(8, 0, 0) == 0x80 &&
               Field_SampleCollisionHeightCode(0, 4, 0) == 0x80 &&
               Field_SampleCollisionHeightCode(0, -1, 0) == 0x80 &&
               Field_SampleCollisionHeightCode(0, 0, 2) == 0x80;
  Field_roomData.header.collisionMapLength--;
  passed = passed && Field_SampleCollisionHeightCode(7, 3, 0) == 0x80;
  Field_roomData = saved;
  return passed;
}

int Tests_RoomAiMapStorage(void) {
  const Field_RoomRecord saved = Field_roomData;
  Field_MpdCollisionHeader dimensions = {2, 3, 0, 6};
  Battle_Ai_RoomAiMap map;
  memset(&Field_roomData, 0, sizeof Field_roomData);
  Field_roomData.collisionMap = &dimensions;
  int passed = Battle_Ai_BuildRoomAiMap(&map) == 1 && map.state.maxTileX == 2 &&
               map.state.maxTileZ == 3 && map.state.defaultActionDelay == 150 &&
               map.flags[0][0] == 0x8000 && map.flags[31][31] == 0x8000 &&
               (unsigned char*)map.flags >= (unsigned char*)(&map.state + 1) &&
               Battle_Ai_DecodeCollisionFlags(0xFFFFFFFFu) == 0x387F;
  for (unsigned bit = 0; bit < 32; ++bit) {
    const unsigned input = 1u << bit;
    const unsigned expected =
        ((input >> 23) & 15) | ((input >> 23) & 0x10) | ((input >> 12) & 0x20) |
        ((input >> 22) & 0x40) | ((input >> 20) & 0x800) |
        ((input >> 18) & 0x1000) | ((input >> 16) & 0x2000);
    passed = passed && Battle_Ai_DecodeCollisionFlags(input) == expected;
  }
  unsigned auxiliary[0x498 / 4] = {0};
  auxiliary[0x450 / 4] = (2u << 29) | (99u << 21) | (1u << 20);
  auxiliary[0x494 / 4] = 0x03020A09;
  Field_roomData.section6 = auxiliary;
  Field_roomData.header.section6Length = sizeof auxiliary;
  passed = passed && Battle_Ai_BuildRoomAiMap(&map) == 1 &&
           map.state.maxTileX == 9 && map.state.maxTileZ == 10 &&
           map.state.minTileX == 2 && map.state.minTileZ == 3 &&
           map.state.defaultActionDelay == 99 &&
           map.state.allowsNonSpellActions == 1;
  Field_roomData.header.section6Length--;
  passed = passed && Battle_Ai_BuildRoomAiMap(&map) == 0;
  int samples[16];
  for (unsigned index = 0; index < 16; ++index) samples[index] = 128;
  samples[5] = 192;
  passed = passed && Battle_Ai_SelectTileHeight(samples, 128) == 192;
  const int invalid_sample = (int)0x80000080u;
  for (size_t index = 0; index < sizeof samples / sizeof samples[0]; ++index) {
    samples[index] = invalid_sample;
  }
  samples[0] = 256;
  passed = passed && Battle_Ai_SelectTileHeight(samples, 128) == samples[10];
  Field_roomData = saved;
  return passed;
}

int Tests_AudioModesWithoutSavedMusic(void) {
  Audio_MusicConfig primary = {0};
  Audio_MusicConfig* savedPrimary = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* savedSecondary = Audio_Seq_secondaryMusicConfig;
  const Audio_DriverFlags savedDriver = Audio_Driver_controlLatches;
  const Audio_VoiceScheduler savedScheduler = Audio_Mixer_voiceScheduler;
  const Audio_VoiceModeMasks savedModes = Audio_Mixer_voiceModes;
  const unsigned savedStream = Audio_Cutscene_streamVoices;
  Audio_Seq_activeMusicConfig = &primary;
  Audio_Seq_secondaryMusicConfig = NULL;
  memset(&Audio_Mixer_voiceScheduler, 0, sizeof Audio_Mixer_voiceScheduler);
  Audio_Cutscene_streamVoices = 0;
  Audio_Driver_controlLatches.updateFlags = 0x100;
  Audio_Mixer_CommitPlaybackVoices();
  const int passed = Audio_Mixer_voiceModes.noise == 0 &&
                     Audio_Mixer_voiceModes.reverb == 0 &&
                     Audio_Mixer_voiceModes.fm == 0 &&
                     (Audio_Driver_controlLatches.updateFlags & 0x100) == 0;
  Audio_Seq_activeMusicConfig = savedPrimary;
  Audio_Seq_secondaryMusicConfig = savedSecondary;
  Audio_Mixer_voiceScheduler = savedScheduler;
  Audio_Mixer_voiceModes = savedModes;
  Audio_Cutscene_streamVoices = savedStream;
  Audio_Driver_SetVoiceReverbMask(savedModes.reverb);
  Audio_Driver_SetVoiceNoiseMask(savedModes.noise);
  Audio_Driver_SetVoiceFrequencyModulationMask(savedModes.fm);
  Audio_Driver_controlLatches = savedDriver;
  return passed;
}

static int TestTitleSession(void) {
  ++sessionTitleCalls;
  if (sessionTitleCalls == 1) {
    Boot_JumpToTitle(NULL);
    ++sessionFallthrough;
  }
  if (sessionTitleCalls == 2) {
    Boot_JumpToGameLoop(NULL);
    ++sessionFallthrough;
  }
  return 17;
}

static int TestBattleSession(void) {
  ++sessionBattleCalls;
  Boot_JumpToTitle(NULL);
  ++sessionFallthrough;
  return 0;
}

int Tests_NativeSessionTransitions(void) {
  sessionTitleCalls = sessionBattleCalls = sessionFallthrough = 0;
  int passed = Boot_RunSessionLoop(TestTitleSession, TestBattleSession) == 17 &&
               sessionTitleCalls == 3 && sessionBattleCalls == 1 &&
               sessionFallthrough == 0;
  passed = passed &&
           Boot_RunSessionLoop(TestTitleSession, TestBattleSession) == 17 &&
           sessionTitleCalls == 4;
  return passed;
}

// 573B8.c:67 mixes control codes with addresses; every dispatch boundary must
// retain native pointer width.
typedef intptr_t (*TestOpcodeHandler)(u_char*, short);
_Static_assert(_Generic(Event_kOpcodeHandlers[0],
                   TestOpcodeHandler: 1,
                   default: 0),
               "normal opcode ABI");
_Static_assert(_Generic(Event_kSkipOpcodeHandlers[0],
                   TestOpcodeHandler: 1,
                   default: 0),
               "skip opcode ABI");
_Static_assert(_Generic(Event_legacySkipHandlerCopy[0],
                   TestOpcodeHandler: 1,
                   default: 0),
               "copied skip opcode ABI");
_Static_assert(sizeof(Event_kSkipOpcodeHandlers) ==
                   256 * sizeof(TestOpcodeHandler),
               "complete skip table");

int Tests_EventScriptNativeBranches(int skip) {
  TestOpcodeHandler savedNormal[4];
  TestOpcodeHandler skipCopy[256];
  TestOpcodeHandler* savedSkipCopy = Event_legacySkipHandlerCopy;
  uintptr_t savedReturn = Event_subroutineReturnCursor;
  char savedSkip = Event_skipState;
  u_char savedYieldSize = Event_kOpcodeSizes[2];
  u_char script[] = {0, 4, 0, 0, 1, 0, 0, 0, 2, 3};
  u_char* cursor = script;
  int passed;
  memcpy(savedNormal, Event_kOpcodeHandlers, sizeof savedNormal);
  Core_Memory_CopyAlignedWords(skipCopy, Event_kSkipOpcodeHandlers,
                               sizeof skipCopy);
  passed = memcmp(skipCopy, Event_kSkipOpcodeHandlers, sizeof skipCopy) == 0;
  skipCopy[0] = Event_kOpcodeHandlers[0] = Event_ScriptJumpForward;
  skipCopy[1] = Event_kOpcodeHandlers[1] = Event_ScriptReturnToSavedAddress;
  skipCopy[2] = Event_kOpcodeHandlers[2] = Event_ScriptYieldFrame;
  skipCopy[3] = Event_kOpcodeHandlers[3] = Event_CommandStopScriptContext;
  Event_legacySkipHandlerCopy = skipCopy;
  Event_skipState = skip ? 2 : 0;
  Event_subroutineReturnCursor = (uintptr_t)&script[8];
  Event_kOpcodeSizes[2] = 1;
  // Forward jump, saved-address return, advance-and-yield, then thread
  // termination.
  passed &= Event_ExecuteScriptThread(&cursor, 1) == 0 && cursor == &script[9];
  passed &= Event_ExecuteScriptThread(&cursor, 1) == 0 && cursor == NULL;
  memcpy(Event_kOpcodeHandlers, savedNormal, sizeof savedNormal);
  Event_kOpcodeSizes[2] = savedYieldSize;
  Event_subroutineReturnCursor = savedReturn;
  Event_skipState = savedSkip;
  Event_legacySkipHandlerCopy = savedSkipCopy;
  return passed;
}

int Tests_EventScriptBranchHandlers(void) {
  char savedFlag = Event_cachedFlagValue;
  u_char* savedThread = Event_threadCursors[2];
  u_char conditional[] = {0, 7, 4, 0, 0};
  u_char backward[] = {0, 0, 0, 0, 0, 0xFF, 4, 0};
  u_char context[] = {0, 2, 5, 0, 0, 0};
  int passed;
  Event_cachedFlagValue = 7;
  passed = Event_ScriptBranchOnCachedFlag(conditional, 1) ==
           (intptr_t)&conditional[4];
  Event_cachedFlagValue = 6;
  passed &= Event_ScriptBranchOnCachedFlag(conditional, 1) == 0;
  passed &= Event_ScriptLoopBackward(&backward[4], 1) == (intptr_t)backward;
  passed &=
      Event_CommandStartScriptContext(context, 1) == (intptr_t)&context[5];
  passed &= Event_threadCursors[2] == &context[4];
  Event_threadCursors[2] = savedThread;
  Event_cachedFlagValue = savedFlag;
  return passed;
}
