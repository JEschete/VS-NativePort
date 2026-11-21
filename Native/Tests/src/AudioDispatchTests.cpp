// Parity of the typed audio dispatch registrations (issue #47) with the
// original tables in SLUS_010.40: D_800301E0 (sound commands), D_800305E0
// (sequence opcodes from 0xA0) and D_80030760 (0xFE extended opcodes).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/MixControlLive.hpp"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Audio/SeqControlCommandsLive.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqVoiceCommandsLive.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/StreamControlLive.hpp"

namespace {

namespace Cutscene = VS::Audio::Cutscene;
namespace Mixer = VS::Audio::Mixer;
namespace Seq = VS::Audio::Seq;
namespace SoundEffects = VS::Audio::SoundEffects;

// SLUS_010.40 loads at this address: file offset = address - kResidentBase.
constexpr std::uint32_t kResidentBase = 0x8000F800U;
constexpr std::uint32_t kCommandTable = 0x800301E0U;         // D_800301E0
constexpr std::uint32_t kOpcodeTable = 0x800305E0U;          // D_800305E0
constexpr std::uint32_t kExtendedOpcodeTable = 0x80030760U;  // D_80030760

template <typename Handler>
struct Binding {
  std::uint32_t address;  // the original handler's PS1 address
  Handler handler;
};

// Every distinct word D_800301E0 stores, with the native handler it names.
constexpr std::array<Binding<VS::Audio::CommandHandler>, 53> kCommandBindings{{
    {0x80016DF8U, &Seq::LiveStartOrResumeMusic},
    {0x80016E74U, &Seq::LiveSaveCurrentMusic},
    {0x80016F0CU, &Seq::LiveStartMusicLayer},
    {0x80016FF4U, &Seq::LiveCrossfadeMusicLayers},
    {0x800171C8U, &Seq::LiveStartMusicWithLoopCounter},
    {0x80017208U, &SoundEffects::LivePlayInterfaceProgramPair},
    {0x80017254U, &SoundEffects::LivePlayInterfaceProgram},
    {0x800172D4U, &SoundEffects::LivePlaySharedProgram},
    {0x8001733CU, &SoundEffects::LivePlayBlobPrograms},
    {0x8001746CU, &SoundEffects::LiveStopPrograms},
    {0x80017498U, &Seq::LiveSetMusicVolume},
    {0x8001754CU, &Seq::LiveFadeMusicVolume},
    {0x8001766CU, &Seq::LiveFadeMusicVolumeFrom},
    {0x80017764U, &Seq::LiveSetDiscVolume},
    {0x80017798U, &Seq::LiveFadeDiscVolume},
    {0x80017800U, &Seq::LiveFadeDiscVolumeFrom},
    {0x80017870U, &SoundEffects::LiveSetProgramVolume},
    {0x80017958U, &SoundEffects::LiveFadeProgramVolume},
    {0x80017AD8U, &SoundEffects::LiveSetSceneVolume},
    {0x80017B54U, &SoundEffects::LiveFadeSceneVolume},
    {0x80017C1CU, &SoundEffects::LiveSetProgramPan},
    {0x80017CFCU, &SoundEffects::LiveFadeProgramPan},
    {0x80017E74U, &SoundEffects::LiveSetScenePan},
    {0x80017EECU, &SoundEffects::LiveFadeScenePan},
    {0x80017FB0U, &Mixer::LiveSetSoundEffectPitchScale},
    {0x80018090U, &Mixer::LiveSlideSoundEffectPitchScale},
    {0x80018218U, &Mixer::LiveSetWorldSoundEffectPitchScale},
    {0x80018270U, &Mixer::LiveSlideWorldSoundEffectPitchScale},
    {0x8001833CU, &Mixer::LiveSetTempoScale},
    {0x80018358U, &Mixer::LiveSlideTempoScale},
    {0x800183C0U, &Mixer::LiveSetTempoScaleRamp},
    {0x80018434U, &Mixer::LiveSetMusicPitchScale},
    {0x80018450U, &Mixer::LiveSlideMusicPitchScale},
    {0x800184B8U, &Mixer::LiveSetMusicPitchScaleRamp},
    {0x8001852CU, &Mixer::LiveStopMusicAndCancelReplacement},
    {0x800185A0U, &Mixer::LiveStopMatchingMusic},
    {0x80018608U, &Mixer::LiveStopWorldSoundEffects},
    {0x800186E4U, &Mixer::LiveEnableStereoOutput},
    {0x80018744U, &Mixer::LiveEnableMonoOutput},
    {0x800187A4U, &Mixer::LiveSetMusicMuteMask},
    {0x800187E4U, &Mixer::LiveSetMusicJumpThreshold},
    {0x800187F8U, &Mixer::LivePauseMusic},
    {0x800188E8U, &Mixer::LiveResumeMusic},
    {0x80018984U, &Mixer::LivePauseWorldSoundEffects},
    {0x80018AA0U, &Mixer::LiveResumeWorldSoundEffects},
    {0x80018B34U, &Cutscene::PauseLiveStream},
    {0x80018B84U, &Cutscene::ResumeLiveStream},
    {0x80018BD8U, &Mixer::LiveIgnoreCommand},
    {0x8001D644U, &Cutscene::ReplaceLiveStream},
    {0x8001D698U, &Cutscene::StopLiveStream},
    {0x8001D6B8U, &Cutscene::SetLiveStreamVolume},
    {0x8001D728U, &Cutscene::SlideLiveStreamVolume},
    {0x8001D7A8U, &Cutscene::SetLiveStreamPan},
}};

// Every distinct word D_800305E0 and D_80030760 store. SoundVM_nop's two
// entry points (0x8001C378 and 0x8001C39C) both name LiveIgnoreInstruction.
constexpr std::array<Binding<VS::Audio::SeqOpcodeHandler>, 96> kSeqBindings{{
    {0x8001B138U, &Seq::LiveFinishChannel},
    {0x8001B20CU, &Seq::LiveFinishChannelOnReplacement},
    {0x8001B248U, &Seq::LiveSetTempo},
    {0x8001B294U, &Seq::LiveSlideTempo},
    {0x8001B334U, &Seq::LiveSetReverbDepth},
    {0x8001B37CU, &Seq::LiveSlideReverbDepth},
    {0x8001B424U, &Seq::LiveJumpRelative},
    {0x8001B450U, &Seq::LiveJumpAtThreshold},
    {0x8001B4A8U, &Seq::LiveCallRelative},
    {0x8001B4E0U, &Seq::LiveReturnFromCall},
    {0x8001B4ECU, &Seq::LiveSetVolumeBalance},
    {0x8001B514U, &Seq::LiveSlideVolumeBalance},
    {0x8001B598U, &Seq::LiveSetVolume},
    {0x8001B5C8U, &Seq::LiveSlideVolume},
    {0x8001B654U, &Seq::LiveSetKeyOnVolumeRamp},
    {0x8001B6E4U, &Seq::LiveEnableFullNoteLength},
    {0x8001B6F8U, &Seq::LiveDisableFullNoteLength},
    {0x8001B70CU, &Seq::LiveSetSequenceControlValue},
    {0x8001B804U, &Seq::LiveSetPan},
    {0x8001B838U, &Seq::LiveSlidePan},
    {0x8001B8C4U, &Seq::LiveSetOctave},
    {0x8001B8E0U, &Seq::LiveIncreaseOctave},
    {0x8001B8F8U, &Seq::LiveDecreaseOctave},
    {0x8001B910U, &Seq::LiveLoadInstrument},
    {0x8001B9B8U, &Seq::LiveLoadSilentInstrument},
    {0x8001BA24U, &Seq::LiveSelectPatch},
    {0x8001BAB8U, &Seq::LiveResetEnvelope},
    {0x8001BB00U, &Seq::LiveSetTranspose},
    {0x8001BB24U, &Seq::LiveAddTranspose},
    {0x8001BB50U, &Seq::LiveSetPitchBendSlide},
    {0x8001BB98U, &Seq::LiveEnablePortamento},
    {0x8001BBD0U, &Seq::LiveDisablePortamento},
    {0x8001BBD8U, &Seq::LiveSetFineTune},
    {0x8001BC38U, &Seq::LiveAddFineTune},
    {0x8001BC9CU, &Seq::LiveEnableVibrato},
    {0x8001BD94U, &Seq::LiveSetVibratoDepth},
    {0x8001BDF4U, &Seq::LiveSlideVibratoDepth},
    {0x8001BE60U, &Seq::LiveDisableVibrato},
    {0x8001BE84U, &Seq::LiveEnableTremolo},
    {0x8001BF34U, &Seq::LiveSetTremoloDepth},
    {0x8001BF58U, &Seq::LiveSlideTremoloDepth},
    {0x8001BFC8U, &Seq::LiveDisableTremolo},
    {0x8001BFECU, &Seq::LiveEnableAutoPan},
    {0x8001C058U, &Seq::LiveSetAutoPanDepth},
    {0x8001C078U, &Seq::LiveSlideAutoPanDepth},
    {0x8001C0E4U, &Seq::LiveDisableAutoPan},
    {0x8001C108U, &Seq::LiveEnableNoise},
    {0x8001C168U, &Seq::LiveDisableNoise},
    {0x8001C1D0U, &Seq::LiveEnableFrequencyModulation},
    {0x8001C244U, &Seq::LiveDisableFrequencyModulation},
    {0x8001C2ACU, &Seq::LiveEnableReverb},
    {0x8001C30CU, &Seq::LiveDisableReverb},
    {0x8001C36CU, &Seq::LiveEnableLegato},
    {0x8001C378U, &Seq::LiveIgnoreInstruction},
    {0x8001C380U, &Seq::LiveEnableSustainedSoundEffect},
    {0x8001C39CU, &Seq::LiveIgnoreInstruction},
    {0x8001C3A4U, &Seq::LiveSetNoiseClock},
    {0x8001C440U, &Seq::LiveSetAttackRate},
    {0x8001C484U, &Seq::LiveSetDecayRate},
    {0x8001C4B8U, &Seq::LiveSetSustainLevel},
    {0x8001C4E8U, &Seq::LiveSetSustainRate},
    {0x8001C52CU, &Seq::LiveSetReleaseRate},
    {0x8001C56CU, &Seq::LiveSetAttackMode},
    {0x8001C5B0U, &Seq::LiveSetSustainMode},
    {0x8001C628U, &Seq::LiveSetReleaseMode},
    {0x8001C66CU, &Seq::LiveSetVoiceAllocationStart},
    {0x8001C68CU, &Seq::LiveResetVoiceAllocationStart},
    {0x8001C69CU, &Seq::LivePushLoop},
    {0x8001C6ECU, &Seq::LiveRepeatLoop},
    {0x8001C780U, &Seq::LiveJumpOnLoopPass},
    {0x8001C7ECU, &Seq::LiveExitLoopOnPass},
    {0x8001C868U, &Seq::LiveRepeatLoopForever},
    {0x8001C8BCU, &Seq::LiveSetNextNoteLength},
    {0x8001C8E4U, &Seq::LiveSetFixedNoteLength},
    {0x8001C92CU, &Seq::LiveEnableDrumKeymap},
    {0x8001C964U, &Seq::LiveMuteInstrument},
    {0x8001C97CU, &Seq::LiveSetTimerDivisors},
    {0x8001C9B4U, &Seq::LiveSetTimerCount},
    {0x8001C9F0U, &Seq::LiveSetDecayAndSustain},
    {0x8001CA2CU, &Seq::LiveEnableTimedNoise},
    {0x8001CA70U, &Seq::LiveSetNoiseToggleDelay},
    {0x8001CAA0U, &Seq::LiveEnableTimedFrequencyModulation},
    {0x8001CAE4U, &Seq::LiveSetFrequencyModulationToggleDelay},
    {0x8001CB14U, &Seq::LiveResetVoiceEffects},
    {0x8001CB7CU, &Seq::LiveEnablePitchSidechain},
    {0x8001CB90U, &Seq::LiveDisablePitchSidechain},
    {0x8001CBA4U, &Seq::LiveEnableVolumeSidechain},
    {0x8001CBB8U, &Seq::LiveDisableVolumeSidechain},
    {0x8001CBCCU, &Seq::LivePlayEmbeddedSoundEffect},
    {0x8001CC84U, &Seq::LiveEnableLoopBoundaryStop},
    {0x8001CC98U, &Seq::LiveSelectNoteRemapTable},
    {0x8001CCC8U, &Seq::LiveMarkChannelKeyed},
    {0x8001CCE8U, &Seq::LiveClearChannelKeyed},
    {0x8001CD08U, &Seq::LiveSetRandomPitchDepth},
    {0x8001CD24U, &Seq::LiveResetRandomPitchDepth},
    {0x8001CD2CU, &Seq::LiveStopOnInvalidInstruction},
}};

template <typename Handler, std::size_t kCount>
constexpr bool AddressesAscend(
    const std::array<Binding<Handler>, kCount>& bindings) {
  return std::ranges::adjacent_find(bindings,
                                    [](const auto& left, const auto& right) {
                                      return left.address >= right.address;
                                    }) == bindings.end();
}
static_assert(AddressesAscend(kCommandBindings));
static_assert(AddressesAscend(kSeqBindings));

std::vector<std::uint8_t> ResidentImage() {
  std::ifstream stream(R"(Native\data\disc\SLUS_010.40)", std::ios::binary);
  if (!stream) SKIP("Extracted SLUS_010.40 unavailable");
  return {std::istreambuf_iterator<char>(stream), {}};
}

// The original table's word `index` (little-endian) in the resident image.
std::uint32_t Word(std::span<const std::uint8_t> image, std::uint32_t table,
                   std::size_t index) {
  const std::size_t offset =
      table - kResidentBase + index * sizeof(std::uint32_t);
  REQUIRE(offset + sizeof(std::uint32_t) <= image.size());
  return std::uint32_t{image[offset]} | std::uint32_t{image[offset + 1]} << 8 |
         std::uint32_t{image[offset + 2]} << 16 |
         std::uint32_t{image[offset + 3]} << 24;
}

// Requires registration[i] to be the native handler bound to the original
// table's word i, and records which original addresses the table used.
template <typename Handler, std::size_t kCount, std::size_t kBindings>
void CheckParity(const Handler (&registration)[kCount], std::uint32_t table,
                 std::span<const std::uint8_t> image,
                 const std::array<Binding<Handler>, kBindings>& bindings,
                 std::set<std::uint32_t>& used) {
  for (std::size_t index = 0; index < kCount; ++index) {
    const std::uint32_t word = Word(image, table, index);
    CAPTURE(index, word);
    const auto bound =
        std::ranges::find(bindings, word, &Binding<Handler>::address);
    REQUIRE(bound != bindings.end());
    CHECK(registration[index] == bound->handler);
    used.insert(word);
  }
}

template <typename Handler, std::size_t kBindings>
void CheckAllUsed(const std::array<Binding<Handler>, kBindings>& bindings,
                  const std::set<std::uint32_t>& used) {
  for (const auto& binding : bindings) {
    CAPTURE(binding.address);
    CHECK(used.contains(binding.address));
  }
}

}  // namespace

TEST_CASE("Audio command handlers match the original dispatch table",
          "[audio][dispatch]") {
  const auto image = ResidentImage();
  std::set<std::uint32_t> used;
  CheckParity(Mixer::kCommandHandlers, kCommandTable, image, kCommandBindings,
              used);
  CheckAllUsed(kCommandBindings, used);
}

TEST_CASE("Sequencer opcode handlers match the original dispatch tables",
          "[audio][dispatch]") {
  const auto image = ResidentImage();
  std::set<std::uint32_t> used;
  CheckParity(Seq::kOpcodeHandlers, kOpcodeTable, image, kSeqBindings, used);
  CheckParity(Seq::kExtendedOpcodeHandlers, kExtendedOpcodeTable, image,
              kSeqBindings, used);
  CheckAllUsed(kSeqBindings, used);
}

TEST_CASE("Audio dispatch registrations leave no entry empty",
          "[audio][dispatch]") {
  CHECK(std::ranges::none_of(Mixer::kCommandHandlers,
                             [](auto handler) { return handler == nullptr; }));
  CHECK(std::ranges::none_of(Seq::kOpcodeHandlers,
                             [](auto handler) { return handler == nullptr; }));
  CHECK(std::ranges::none_of(Seq::kExtendedOpcodeHandlers,
                             [](auto handler) { return handler == nullptr; }));
}
