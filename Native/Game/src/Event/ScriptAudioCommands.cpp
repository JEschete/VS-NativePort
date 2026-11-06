// Hand-maintained (tools/port/owned.csv).
#include "Event/ScriptAudioCommands.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Event {
namespace {

// Operand offsets after the opcode byte.
constexpr std::size_t kFirstOperand = 1;
constexpr std::size_t kSecondOperand = 2;
constexpr std::size_t kThirdOperand = 3;
constexpr std::size_t kFourthOperand = 4;

// The beat counter's whole beats sit above its 16-bit fraction.
constexpr unsigned kBeatShift = 16;
// A music fade that silences the slot it leaves.
constexpr int kSilent = 0;
// A slot poll result that reads as still loading.
constexpr int kStillLoading = 1;

// Refuses an instruction shorter than its operands.
[[nodiscard]] bool Fits(ScriptCommand command, std::size_t length,
                        ScriptAudioHost& host) {
  if (command.size() >= length) {
    return true;
  }
  host.Refuse(ScriptAudioRefusal::kShortCommand,
              static_cast<int>(command.size()));
  return false;
}

// The channel group a source byte names, refusing one past the groups.
[[nodiscard]] std::optional<std::uint32_t> ChannelGroup(std::uint8_t source,
                                                        ScriptAudioHost& host) {
  if (source < kScriptSfxChannelGroups.size()) {
    return kScriptSfxChannelGroups[source];
  }
  host.Refuse(ScriptAudioRefusal::kSfxSource, source);
  return std::nullopt;
}

[[nodiscard]] ScriptStep HoldWhile(bool busy) {
  return busy ? ScriptStep::kHold : ScriptStep::kAdvance;
}

// A release ends the thread's frame while a cutscene skip runs.
[[nodiscard]] ScriptStep AfterRelease(const ScriptAudioHost& host) {
  return host.SkipActive() ? ScriptStep::kAdvanceAndYield
                           : ScriptStep::kAdvance;
}

[[nodiscard]] std::uint8_t Byte(ScriptCommand command, std::size_t offset) {
  return OperandByte(command, offset);
}

[[nodiscard]] int SignedByte(ScriptCommand command, std::size_t offset) {
  return static_cast<std::int8_t>(OperandByte(command, offset));
}

}  // namespace

ScriptStep ScriptPlaySoundCommand(ScriptCommand command,
                                  ScriptAudioHost& host) {
  if (!Fits(command, kPlaySoundLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t source = Byte(command, kFirstOperand);
  const auto group = ChannelGroup(source, host);
  if (!group) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t effect = Byte(command, kSecondOperand);
  if (source == kAmbientSfxSource) {
    if (!host.AmbientSoundSuspended()) {
      return ScriptStep::kAdvance;
    }
    host.SetSuspendedAmbientSound(effect);
  }
  host.PlaySfx(*group, effect, Byte(command, kThirdOperand),
               Byte(command, kFourthOperand));
  return ScriptStep::kAdvance;
}

ScriptStep CommandPlayActorSfx(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kPlayActorSfxLength, host)) {
    const auto reference =
        static_cast<std::uint16_t>(OperandHalf(command, kFirstOperand));
    host.PlayActorSound(host.ResolveActor(reference),
                        Byte(command, kThirdOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandSlideSfxVolume(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kSlideSfxVolumeLength, host)) {
    return ScriptStep::kAdvance;
  }
  if (const auto group = ChannelGroup(Byte(command, kFirstOperand), host)) {
    host.FadeSfxVolume(*group, Byte(command, kSecondOperand),
                       Byte(command, kThirdOperand),
                       Byte(command, kFourthOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandStopAllSfx(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kStopAllSfxLength, host)) {
    host.StopAllSfx();
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandStopSfx(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kStopSfxLength, host)) {
    return ScriptStep::kAdvance;
  }
  if (const auto group = ChannelGroup(Byte(command, kFirstOperand), host)) {
    host.StopSfx(*group, Byte(command, kSecondOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandLoadSfxSlot(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kLoadSfxSlotLength, host)) {
    host.LoadSfxBank(Byte(command, kFirstOperand),
                     Byte(command, kSecondOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandWaitSfxSlotRelease(ScriptCommand command,
                                     ScriptAudioHost& host) {
  if (!Fits(command, kWaitSfxSlotReleaseLength, host)) {
    return ScriptStep::kAdvance;
  }
  return HoldWhile(host.PollSfxBankLoad(Byte(command, kFirstOperand)) != 0);
}

ScriptStep CommandFreeSfx(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kFreeSfxLength, host)) {
    return ScriptStep::kAdvance;
  }
  host.FreeSfxBank(Byte(command, kFirstOperand));
  return AfterRelease(host);
}

ScriptStep CommandSelectSfxBank(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kSelectSfxBankLength, host)) {
    host.SelectSfxBank(Byte(command, kFirstOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandLoadEventMusic(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kLoadEventMusicLength, host)) {
    const auto music = static_cast<std::int16_t>(host.ZoneSoundFile());
    host.LoadMusic(music, kEventMusicSlot);
    host.LoadWaveBank(music);
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandCrossfadeMusic(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kCrossfadeMusicLength, host)) {
    host.FadeMusicVolume(host.MusicSlot(), kSilent,
                         Byte(command, kFourthOperand));
    const std::uint8_t slot = Byte(command, kFirstOperand);
    host.SetMusicSlot(slot);
    host.CrossfadeMusic(slot, Byte(command, kSecondOperand),
                        Byte(command, kThirdOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandLoadMusicSlot(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kLoadMusicSlotLength, host)) {
    host.LoadMusic(Byte(command, kFirstOperand), Byte(command, kSecondOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandFreeMusic(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kFreeMusicLength, host)) {
    return ScriptStep::kAdvance;
  }
  host.FreeMusic(Byte(command, kFirstOperand));
  return AfterRelease(host);
}

ScriptStep CommandPlayMusic(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kPlayMusicLength, host)) {
    const std::uint8_t slot = Byte(command, kFirstOperand);
    host.SetMusicSlot(slot);
    host.PlayMusic(slot, Byte(command, kSecondOperand),
                   Byte(command, kThirdOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandSlideMusicVolume(ScriptCommand command,
                                   ScriptAudioHost& host) {
  if (Fits(command, kSlideMusicVolumeLength, host)) {
    host.FadeMusicVolume(host.MusicSlot(), Byte(command, kFirstOperand),
                         Byte(command, kSecondOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandSlideMusicTempo(ScriptCommand command,
                                  ScriptAudioHost& host) {
  if (Fits(command, kSlideMusicTempoLength, host)) {
    host.FadeMusicTempo(host.MusicSlot(), SignedByte(command, kFirstOperand),
                        Byte(command, kSecondOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandSlideMusicPitch(ScriptCommand command,
                                  ScriptAudioHost& host) {
  if (Fits(command, kSlideMusicPitchLength, host)) {
    host.FadeMusicPitch(host.MusicSlot(), SignedByte(command, kFirstOperand),
                        Byte(command, kSecondOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandStopMusic(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kStopMusicLength, host)) {
    host.StopMusic(host.MusicSlot());
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandWaitMusicSlot(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kWaitMusicSlotLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t slot = Byte(command, kFirstOperand);
  if (host.QueryMusicVolume(slot) != 0) {
    if (host.MusicVolume(slot) != 0) {
      return ScriptStep::kHold;
    }
    host.StopMusic(slot);
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandPushMusicState(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kPushMusicStateLength, host)) {
    host.SaveMusicState();
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandWaitMusicSlotLoad(ScriptCommand command,
                                    ScriptAudioHost& host) {
  if (!Fits(command, kWaitMusicSlotLoadLength, host)) {
    return ScriptStep::kAdvance;
  }
  return HoldWhile(host.PollMusicLoad(Byte(command, kFirstOperand)) ==
                   kStillLoading);
}

ScriptStep CommandLoadSoundFile(ScriptCommand command, ScriptAudioHost& host) {
  if (Fits(command, kLoadSoundFileLength, host)) {
    host.LoadWaveBank(Byte(command, kFirstOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandWaitSoundQueue(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kWaitSoundQueueLength, host)) {
    return ScriptStep::kAdvance;
  }
  return HoldWhile(host.UpdateWaveBankLoad() == kStillLoading);
}

ScriptStep CommandWaitMusicTimer(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kWaitMusicTimerLength, host)) {
    return ScriptStep::kAdvance;
  }
  const int beat = host.MusicBeatPosition() >> kBeatShift;
  return HoldWhile(beat < OperandHalf(command, kFirstOperand));
}

ScriptStep CommandSlideSfxPitch(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kSlideSfxPitchLength, host)) {
    return ScriptStep::kAdvance;
  }
  if (const auto group = ChannelGroup(Byte(command, kFirstOperand), host)) {
    host.FadeSfxPitch(*group, Byte(command, kSecondOperand),
                      SignedByte(command, kThirdOperand),
                      Byte(command, kFourthOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandSlideSfxPan(ScriptCommand command, ScriptAudioHost& host) {
  if (!Fits(command, kSlideSfxPanLength, host)) {
    return ScriptStep::kAdvance;
  }
  if (const auto group = ChannelGroup(Byte(command, kFirstOperand), host)) {
    host.FadeSfxPan(*group, Byte(command, kSecondOperand),
                    Byte(command, kThirdOperand),
                    Byte(command, kFourthOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandResumeMusicSlot(ScriptCommand command,
                                  ScriptAudioHost& host) {
  if (Fits(command, kResumeMusicSlotLength, host)) {
    const std::uint8_t slot = Byte(command, kFirstOperand);
    host.SetMusicSlot(slot);
    host.ResumeMusic(slot);
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandSetMusicJumpThreshold(ScriptCommand command,
                                        ScriptAudioHost& host) {
  if (Fits(command, kSetMusicJumpThresholdLength, host)) {
    host.SetMusicJumpThreshold(Byte(command, kFirstOperand));
  }
  return ScriptStep::kAdvance;
}

ScriptStep CommandPlayMenuSelectSfx(ScriptCommand command,
                                    ScriptAudioHost& host) {
  if (Fits(command, kPlayMenuSelectSfxLength, host)) {
    host.PlaySfxCentered(kMenuSfxChannelGroup, kMenuSelectSfx);
  }
  return ScriptStep::kAdvance;
}

}  // namespace VS::Event
