// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Audio/Seq/Sequencer.hpp"

#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SeqControlCommandsLive.hpp"
#include "Audio/SeqVoiceCommandsLive.hpp"
#include "Legacy/Psx.hpp"

// decomp: D_8002F54C (D_8002F54C@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint16_t VS::Audio::Seq::tickCounter;

// decomp: D_8002F5A0 (D_8002F5A0@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint8_t
    VS::Audio::Seq::kOpcodeLengths[VS::Audio::Seq::kOpcodeLengthTableSize];

// decomp: D_8002F600 (D_8002F600@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint8_t VS::Audio::Seq::kExtendedOpcodeLengths
    [VS::Audio::Seq::kExtendedOpcodeLengthTableSize];

// decomp: D_8002F620 (D_8002F620@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint16_t VS::Audio::Seq::kNoteLengths[VS::Audio::Seq::kNoteLengthCount];

// decomp: g_Sound_ProgramCounter (g_Sound_ProgramCounter@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint32_t VS::Audio::Seq::kIdleProgram;

// decomp: D_8002F63C (D_8002F63C@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint32_t
    VS::Audio::Seq::kSemitonePitches[VS::Audio::Seq::kSemitonePitchTableSize];

// decomp: D_8002F66C (D_8002F66C@SLUS_010.40)
// contents: module image (ModuleData.cpp)
signed char
    VS::Audio::Seq::kNoteRemapTables[VS::Audio::Seq::kNoteRemapTableSize];

// decomp: D_8002F69C (D_8002F69C@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint8_t VS::Audio::Seq::kRandomPitchOffsets
    [VS::Audio::Seq::kRandomPitchOffsetCount];

// decomp: g_Sound_LfoTable (g_Sound_LfoTable@SLUS_010.40)
// contents: module image, relocated (ModulePointers.cpp)
std::int16_t*
    VS::Audio::Seq::kLfoWaveforms[VS::Audio::Seq::kLfoWaveformTableSize];

namespace VS::Audio::Seq {
// decomp: D_800305E0 (D_800305E0@SLUS_010.40)
// contents: typed registration of the original table's handlers, one per
// opcode byte from 0xA0 (each comment gives the original handler address);
// AudioDispatchTests.cpp checks its parity with the table in SLUS_010.40.
// Not const: tests swap entries to capture commands and restore them.
constinit SeqOpcodeHandler kOpcodeHandlers[kOpcodeCount] = {
    &LiveFinishChannel,                      // 0xA0: 0x8001B138
    &LiveLoadInstrument,                     // 0xA1: 0x8001B910
    &LiveSetNextNoteLength,                  // 0xA2: 0x8001C8BC
    &LiveSetVolumeBalance,                   // 0xA3: 0x8001B4EC
    &LiveSetPitchBendSlide,                  // 0xA4: 0x8001BB50
    &LiveSetOctave,                          // 0xA5: 0x8001B8C4
    &LiveIncreaseOctave,                     // 0xA6: 0x8001B8E0
    &LiveDecreaseOctave,                     // 0xA7: 0x8001B8F8
    &LiveSetVolume,                          // 0xA8: 0x8001B598
    &LiveSlideVolume,                        // 0xA9: 0x8001B5C8
    &LiveSetPan,                             // 0xAA: 0x8001B804
    &LiveSlidePan,                           // 0xAB: 0x8001B838
    &LiveSetNoiseClock,                      // 0xAC: 0x8001C3A4
    &LiveSetAttackRate,                      // 0xAD: 0x8001C440
    &LiveSetDecayRate,                       // 0xAE: 0x8001C484
    &LiveSetSustainLevel,                    // 0xAF: 0x8001C4B8
    &LiveSetDecayAndSustain,                 // 0xB0: 0x8001C9F0
    &LiveSetSustainRate,                     // 0xB1: 0x8001C4E8
    &LiveSetReleaseRate,                     // 0xB2: 0x8001C52C
    &LiveResetEnvelope,                      // 0xB3: 0x8001BAB8
    &LiveEnableVibrato,                      // 0xB4: 0x8001BC9C
    &LiveSetVibratoDepth,                    // 0xB5: 0x8001BD94
    &LiveDisableVibrato,                     // 0xB6: 0x8001BE60
    &LiveSetAttackMode,                      // 0xB7: 0x8001C56C
    &LiveEnableTremolo,                      // 0xB8: 0x8001BE84
    &LiveSetTremoloDepth,                    // 0xB9: 0x8001BF34
    &LiveDisableTremolo,                     // 0xBA: 0x8001BFC8
    &LiveSetSustainMode,                     // 0xBB: 0x8001C5B0
    &LiveEnableAutoPan,                      // 0xBC: 0x8001BFEC
    &LiveSetAutoPanDepth,                    // 0xBD: 0x8001C058
    &LiveDisableAutoPan,                     // 0xBE: 0x8001C0E4
    &LiveSetReleaseMode,                     // 0xBF: 0x8001C628
    &LiveSetTranspose,                       // 0xC0: 0x8001BB00
    &LiveAddTranspose,                       // 0xC1: 0x8001BB24
    &LiveEnableReverb,                       // 0xC2: 0x8001C2AC
    &LiveDisableReverb,                      // 0xC3: 0x8001C30C
    &LiveEnableNoise,                        // 0xC4: 0x8001C108
    &LiveDisableNoise,                       // 0xC5: 0x8001C168
    &LiveEnableFrequencyModulation,          // 0xC6: 0x8001C1D0
    &LiveDisableFrequencyModulation,         // 0xC7: 0x8001C244
    &LivePushLoop,                           // 0xC8: 0x8001C69C
    &LiveRepeatLoop,                         // 0xC9: 0x8001C6EC
    &LiveRepeatLoopForever,                  // 0xCA: 0x8001C868
    &LiveResetVoiceEffects,                  // 0xCB: 0x8001CB14
    &LiveEnableLegato,                       // 0xCC: 0x8001C36C
    &LiveIgnoreInstruction,                  // 0xCD: 0x8001C378
    &LiveEnableTimedNoise,                   // 0xCE: 0x8001CA2C
    &LiveSetNoiseToggleDelay,                // 0xCF: 0x8001CA70
    &LiveEnableSustainedSoundEffect,         // 0xD0: 0x8001C380
    &LiveIgnoreInstruction,                  // 0xD1: 0x8001C39C
    &LiveEnableTimedFrequencyModulation,     // 0xD2: 0x8001CAA0
    &LiveSetFrequencyModulationToggleDelay,  // 0xD3: 0x8001CAE4
    &LiveEnablePitchSidechain,               // 0xD4: 0x8001CB7C
    &LiveDisablePitchSidechain,              // 0xD5: 0x8001CB90
    &LiveEnableVolumeSidechain,              // 0xD6: 0x8001CBA4
    &LiveDisableVolumeSidechain,             // 0xD7: 0x8001CBB8
    &LiveSetFineTune,                        // 0xD8: 0x8001BBD8
    &LiveAddFineTune,                        // 0xD9: 0x8001BC38
    &LiveEnablePortamento,                   // 0xDA: 0x8001BB98
    &LiveDisablePortamento,                  // 0xDB: 0x8001BBD0
    &LiveSetFixedNoteLength,                 // 0xDC: 0x8001C8E4
    &LiveSlideVibratoDepth,                  // 0xDD: 0x8001BDF4
    &LiveSlideTremoloDepth,                  // 0xDE: 0x8001BF58
    &LiveSlideAutoPanDepth,                  // 0xDF: 0x8001C078
    &LiveEnableLoopBoundaryStop,             // 0xE0: 0x8001CC84
    &LiveSetRandomPitchDepth,                // 0xE1: 0x8001CD08
    &LiveResetRandomPitchDepth,              // 0xE2: 0x8001CD24
    &LiveStopOnInvalidInstruction,           // 0xE3: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xE4: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xE5: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xE6: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xE7: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xE8: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xE9: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xEA: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xEB: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xEC: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xED: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xEE: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xEF: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF0: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF1: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF2: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF3: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF4: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF5: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF6: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF7: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF8: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xF9: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xFA: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xFB: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xFC: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xFD: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xFE: 0x8001CD2C
    &LiveStopOnInvalidInstruction,           // 0xFF: 0x8001CD2C
};

// decomp: D_80030760 (D_80030760@SLUS_010.40)
// contents: typed registration of the original table's handlers, one per
// operand of the 0xFE extended opcode (each comment gives the original
// handler address); AudioDispatchTests.cpp checks its parity with the table
// in SLUS_010.40. Not const: tests swap entries and restore them.
constinit SeqOpcodeHandler kExtendedOpcodeHandlers[kExtendedOpcodeCount] = {
    &LiveSetTempo,                    // 0xFE 0x00: 0x8001B248
    &LiveSlideTempo,                  // 0xFE 0x01: 0x8001B294
    &LiveSetReverbDepth,              // 0xFE 0x02: 0x8001B334
    &LiveSlideReverbDepth,            // 0xFE 0x03: 0x8001B37C
    &LiveEnableDrumKeymap,            // 0xFE 0x04: 0x8001C92C
    &LiveMuteInstrument,              // 0xFE 0x05: 0x8001C964
    &LiveJumpRelative,                // 0xFE 0x06: 0x8001B424
    &LiveJumpAtThreshold,             // 0xFE 0x07: 0x8001B450
    &LiveJumpOnLoopPass,              // 0xFE 0x08: 0x8001C780
    &LiveExitLoopOnPass,              // 0xFE 0x09: 0x8001C7EC
    &LiveLoadSilentInstrument,        // 0xFE 0x0A: 0x8001B9B8
    &LivePlayEmbeddedSoundEffect,     // 0xFE 0x0B: 0x8001CBCC
    &LiveSetSequenceControlValue,     // 0xFE 0x0C: 0x8001B70C
    &LiveStopOnInvalidInstruction,    // 0xFE 0x0D: 0x8001CD2C
    &LiveCallRelative,                // 0xFE 0x0E: 0x8001B4A8
    &LiveReturnFromCall,              // 0xFE 0x0F: 0x8001B4E0
    &LiveSetVoiceAllocationStart,     // 0xFE 0x10: 0x8001C66C
    &LiveResetVoiceAllocationStart,   // 0xFE 0x11: 0x8001C68C
    &LiveSlideVolumeBalance,          // 0xFE 0x12: 0x8001B514
    &LiveStopOnInvalidInstruction,    // 0xFE 0x13: 0x8001CD2C
    &LiveSelectPatch,                 // 0xFE 0x14: 0x8001BA24
    &LiveSetTimerDivisors,            // 0xFE 0x15: 0x8001C97C
    &LiveSetTimerCount,               // 0xFE 0x16: 0x8001C9B4
    &LiveStopOnInvalidInstruction,    // 0xFE 0x17: 0x8001CD2C
    &LiveStopOnInvalidInstruction,    // 0xFE 0x18: 0x8001CD2C
    &LiveSetKeyOnVolumeRamp,          // 0xFE 0x19: 0x8001B654
    &LiveEnableFullNoteLength,        // 0xFE 0x1A: 0x8001B6E4
    &LiveDisableFullNoteLength,       // 0xFE 0x1B: 0x8001B6F8
    &LiveSelectNoteRemapTable,        // 0xFE 0x1C: 0x8001CC98
    &LiveMarkChannelKeyed,            // 0xFE 0x1D: 0x8001CCC8
    &LiveClearChannelKeyed,           // 0xFE 0x1E: 0x8001CCE8
    &LiveFinishChannelOnReplacement,  // 0xFE 0x1F: 0x8001B20C
};
}  // namespace VS::Audio::Seq

// decomp: D_80037800 (D_80037800@SLUS_010.40)
VS::Audio::CommandParams VS::Audio::Seq::embeddedSoundEffectParams;

// decomp: g_Sound_MasterFadeTimer (g_Sound_MasterFadeTimer@SLUS_010.40)
VS::Audio::LegacyFadeTimer VS::Audio::Seq::masterFade;

// decomp: g_Sound_LfoPhase (g_Sound_LfoPhase@SLUS_010.40)
int VS::Audio::Seq::randomPitchPhase;
