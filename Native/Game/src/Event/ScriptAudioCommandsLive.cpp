// Hand-maintained (tools/port/owned.csv).
// Binds the typed event script sound commands (ScriptAudioCommands.cpp) to the
// event VM's opcode slots, the sound effect, bank and music mixers and the
// event's music slot and ambience state.
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "Audio/ActorSoundLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptAudio.hpp"
#include "Event/ScriptAudioCommands.hpp"
#include "Event/ScriptAudioOpcodes.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Legacy/Psx.hpp"

namespace {
namespace event = VS::Event;
namespace audio = VS::Audio;

constexpr char kSkipActive = 2;

void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("event", "%s", message);
    std::abort();
  }
}

const char* RefusalText(event::ScriptAudioRefusal refusal) {
  switch (refusal) {
    case event::ScriptAudioRefusal::kSfxSource:
      return "a sound source past the channel groups";
    case event::ScriptAudioRefusal::kShortCommand:
      return "an instruction shorter than its operands";
  }
  return "an unknown request";
}

int Group(std::uint32_t group) { return static_cast<int>(group); }

class LiveScriptAudioHost final : public event::ScriptAudioHost {
 public:
  void Refuse(event::ScriptAudioRefusal refusal, int value) override {
    VS::Core::Warnf("event", "Script sound command refused %s (%d)",
                    RefusalText(refusal), value);
  }

  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    return VS::Event::ResolveLiveScriptActorReference(reference);
  }
  [[nodiscard]] bool SkipActive() const override {
    return event::skipState == kSkipActive;
  }

  [[nodiscard]] std::uint8_t MusicSlot() const override {
    return static_cast<std::uint8_t>(event::musicSlot);
  }
  void SetMusicSlot(std::uint8_t slot) override {
    event::musicSlot = static_cast<char>(slot);
  }
  [[nodiscard]] bool AmbientSoundSuspended() const override {
    return event::isAmbientSoundSuspended != 0;
  }
  void SetSuspendedAmbientSound(std::int16_t sound) override {
    event::suspendedAmbientSoundId = sound;
  }

  void PlaySfx(std::uint32_t group, int effect, int pan, int volume) override {
    audio::LivePlaySfx(Group(group), effect, pan, volume);
  }
  void PlaySfxCentered(std::uint32_t group, int effect) override {
    audio::LivePlaySfxCentered(Group(group), effect);
  }
  void FadeSfxVolume(std::uint32_t group, int effect, int volume,
                     int ticks) override {
    audio::LiveFadeSfxVolume(Group(group), effect, volume, ticks);
  }
  void FadeSfxPitch(std::uint32_t group, int effect, int pitch_scale,
                    int ticks) override {
    audio::LiveFadeSfxPitch(Group(group), effect, pitch_scale, ticks);
  }
  void FadeSfxPan(std::uint32_t group, int effect, int pan,
                  int ticks) override {
    audio::LiveFadeSfxPan(Group(group), effect, pan, ticks);
  }
  void StopSfx(std::uint32_t group, int effect) override {
    audio::LiveStopSfx(Group(group), effect);
  }
  void StopAllSfx() override { audio::LiveStopAllSfxPlayback(); }
  void PlayActorSound(int actor, int sound) override {
    audio::LivePlayActorScriptSound(actor, sound);
  }

  void LoadSfxBank(int bank, unsigned slot) override {
    audio::LiveLoadSfxBankSlot(bank, slot);
  }
  [[nodiscard]] int PollSfxBankLoad(unsigned slot) override {
    return audio::LivePollSfxBankSlotLoad(slot);
  }
  void FreeSfxBank(unsigned slot) override { audio::LiveFreeSfxBankSlot(slot); }
  void SelectSfxBank(int slot) override { audio::LiveSelectSfxBankSlot(slot); }
  void LoadWaveBank(int bank) override { audio::LiveLoadMappedWaveBank(bank); }
  [[nodiscard]] int UpdateWaveBankLoad() override {
    return audio::LiveUpdateWaveBankLoad();
  }
  [[nodiscard]] int ZoneSoundFile() override {
    return VS::Field::LiveZoneSoundFileId();
  }

  void LoadMusic(int music, int slot) override {
    audio::LiveLoadMusicSlot(music, slot);
  }
  [[nodiscard]] int PollMusicLoad(unsigned slot) override {
    return audio::LivePollMusicSlotLoad(slot);
  }
  void FreeMusic(int slot) override { audio::LiveFreeMusicSlot(slot); }
  void PlayMusic(int slot, int volume, int ticks) override {
    audio::LivePlayMusicSlot(slot, volume, ticks);
  }
  void CrossfadeMusic(int slot, int volume, unsigned ticks) override {
    audio::LiveCrossfadeMusicSlot(slot, volume, ticks);
  }
  void ResumeMusic(int slot) override { audio::LiveResumeMusicSlot(slot); }
  void FadeMusicVolume(int slot, int volume, int ticks) override {
    audio::LiveFadeMusicSlotVolume(slot, volume, ticks);
  }
  void FadeMusicTempo(int slot, int scale, int ticks) override {
    audio::LiveFadeMusicSlotTempo(slot, scale, ticks);
  }
  void FadeMusicPitch(int slot, int scale, int ticks) override {
    audio::LiveFadeMusicSlotPitch(slot, scale, ticks);
  }
  void StopMusic(int slot) override { audio::LiveStopMusicSlotPlayback(slot); }
  [[nodiscard]] int QueryMusicVolume(int slot) override {
    return audio::LiveQueryMusicSlotVolume(slot);
  }
  [[nodiscard]] int MusicVolume(int slot) override {
    return audio::LiveMusicSlotVolume(slot);
  }
  [[nodiscard]] int MusicBeatPosition() override {
    return audio::LiveMusicBeatPosition();
  }
  void SaveMusicState() override { audio::LiveSaveMusicPlaybackState(); }
  void SetMusicJumpThreshold(int threshold) override {
    audio::LiveSetMusicJumpThreshold(threshold);
  }
};

using Handler = event::ScriptStep (*)(event::ScriptCommand,
                                      event::ScriptAudioHost&);

intptr_t RunCommand(Handler handler, const std::uint8_t* command,
                    std::size_t length) {
  Require(command != nullptr, "Script sound command is missing");
  LiveScriptAudioHost host;
  return static_cast<intptr_t>(
      handler(event::ScriptCommand{command, length}, host));
}
}  // namespace

// decomp: vs_battle_script_80_playSfx (src/BATTLE/BATTLE.PRG/4DEA8.c:2928; was
// func_800BA2E0), opcode 0x80
std::intptr_t VS::Event::ScriptPlaySoundCommandOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptPlaySoundCommand, command,
                    event::kPlaySoundLength);
}

// decomp: vs_battle_script_81 (src/BATTLE/BATTLE.PRG/4DEA8.c:2940; was
// func_800BA35C), opcode 0x81
std::intptr_t VS::Event::CommandPlayActorSfxOpcode(std::uint8_t* command,
                                                   std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayActorSfx, command,
                    event::kPlayActorSfxLength);
}

// decomp: vs_battle_script_82_fadeSfxVolume
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2946; was func_800BA39C), opcode 0x82
std::intptr_t VS::Event::CommandSlideSfxVolumeOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideSfxVolume, command,
                    event::kSlideSfxVolumeLength);
}

// decomp: vs_battle_script_83_stopAllSfx (src/BATTLE/BATTLE.PRG/4DEA8.c:2952;
// was func_800BA3E4), opcode 0x83
std::intptr_t VS::Event::CommandStopAllSfxOpcode(std::uint8_t* command,
                                                 std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandStopAllSfx, command,
                    event::kStopAllSfxLength);
}

// decomp: vs_battle_script_84_evictSfx (src/BATTLE/BATTLE.PRG/4DEA8.c:2958; was
// func_800BA404), opcode 0x84
std::intptr_t VS::Event::CommandStopSfxOpcode(std::uint8_t* command,
                                              std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandStopSfx, command, event::kStopSfxLength);
}

// decomp: vs_battle_script_85_loadSfxSlot (src/BATTLE/BATTLE.PRG/4DEA8.c:2964;
// was vs_battle_script_loadSfxSlot), opcode 0x85
std::intptr_t VS::Event::CommandLoadSfxSlotOpcode(std::uint8_t* command,
                                                  std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadSfxSlot, command,
                    event::kLoadSfxSlotLength);
}

// decomp: vs_battle_script_86_freeSfxSlot (src/BATTLE/BATTLE.PRG/4DEA8.c:2970;
// was vs_battle_script_freeSfxSlot), opcode 0x86
std::intptr_t VS::Event::CommandWaitSfxSlotReleaseOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitSfxSlotRelease, command,
                    event::kWaitSfxSlotReleaseLength);
}

// decomp: vs_battle_script_87_freeSfx (src/BATTLE/BATTLE.PRG/4DEA8.c:2975; was
// vs_battle_script_freeSfx), opcode 0x87
std::intptr_t VS::Event::CommandFreeSfxOpcode(std::uint8_t* command,
                                              std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandFreeSfx, command, event::kFreeSfxLength);
}

// decomp: vs_battle_script_88_setCurrentSfx
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2981; was vs_battle_script_setCurrentSfx),
// opcode 0x88
std::intptr_t VS::Event::CommandSelectSfxBankOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSelectSfxBank, command,
                    event::kSelectSfxBankLength);
}

// decomp: vs_battle_script_89_loadSoundFile
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2987; was vs_battle_script_loadSoundFile),
// opcode 0x89
std::intptr_t VS::Event::CommandLoadEventMusicOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadEventMusic, command,
                    event::kLoadEventMusicLength);
}

// decomp: vs_battle_script_8F_crossfadeMusic
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2995; was func_800BA52C), opcode 0x8F
std::intptr_t VS::Event::CommandCrossfadeMusicOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandCrossfadeMusic, command,
                    event::kCrossfadeMusicLength);
}

// decomp: vs_battle_script_90_loadMusicSlot
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3002; was vs_battle_script_loadMusicSlot),
// opcode 0x90
std::intptr_t VS::Event::CommandLoadMusicSlotOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadMusicSlot, command,
                    event::kLoadMusicSlotLength);
}

// decomp: vs_battle_script_91_freeMusic (src/BATTLE/BATTLE.PRG/4DEA8.c:3008;
// was vs_battle_script_freeMusic), opcode 0x91
std::intptr_t VS::Event::CommandFreeMusicOpcode(std::uint8_t* command,
                                                std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandFreeMusic, command, event::kFreeMusicLength);
}

// decomp: vs_battle_script_92_playMusic (src/BATTLE/BATTLE.PRG/4DEA8.c:3014;
// was func_800BA5E8), opcode 0x92
std::intptr_t VS::Event::CommandPlayMusicOpcode(std::uint8_t* command,
                                                std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayMusic, command, event::kPlayMusicLength);
}

// decomp: vs_battle_script_93_fadeMusicVolume
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3020; was func_800BA61C), opcode 0x93
std::intptr_t VS::Event::CommandSlideMusicVolumeOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideMusicVolume, command,
                    event::kSlideMusicVolumeLength);
}

// decomp: vs_battle_script_94_fadeMusicTempo
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3026; was func_800BA650), opcode 0x94
std::intptr_t VS::Event::CommandSlideMusicTempoOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideMusicTempo, command,
                    event::kSlideMusicTempoLength);
}

// decomp: vs_battle_script_95_fadeMusicPitch
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3032; was func_800BA684), opcode 0x95
std::intptr_t VS::Event::CommandSlideMusicPitchOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideMusicPitch, command,
                    event::kSlideMusicPitchLength);
}

// decomp: vs_battle_script_96_stopMusic (src/BATTLE/BATTLE.PRG/4DEA8.c:3038;
// was func_800BA6B8), opcode 0x96
std::intptr_t VS::Event::CommandStopMusicOpcode(std::uint8_t* command,
                                                std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandStopMusic, command, event::kStopMusicLength);
}

// decomp: vs_battle_script_97_waitMusicSilent
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3044; was func_800BA6E0), opcode 0x97
std::intptr_t VS::Event::CommandWaitMusicSlotOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitMusicSlot, command,
                    event::kWaitMusicSlotLength);
}

// decomp: vs_battle_script_98_pushMusicState
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3056; was func_800BA72C), opcode 0x98
std::intptr_t VS::Event::CommandPushMusicStateOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandPushMusicState, command,
                    event::kPushMusicStateLength);
}

// decomp: vs_battle_script_99_clearMusicLoadSlot
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3062; was
// vs_battle_script_clearMusicLoadSlot), opcode 0x99
std::intptr_t VS::Event::CommandWaitMusicSlotLoadOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitMusicSlotLoad, command,
                    event::kWaitMusicSlotLoadLength);
}

// decomp: vs_battle_script_9D_loadSoundFile2
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3067; was vs_battle_script_loadSoundFile2),
// opcode 0x9D
std::intptr_t VS::Event::CommandLoadSoundFileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadSoundFile, command,
                    event::kLoadSoundFileLength);
}

// decomp: vs_battle_script_9E_processSoundQueue
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3073; was vs_battle_script_processSoundQueue),
// opcode 0x9E
std::intptr_t VS::Event::CommandWaitSoundQueueOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitSoundQueue, command,
                    event::kWaitSoundQueueLength);
}

// decomp: vs_battle_script_9A_waitMusicBar (src/BATTLE/BATTLE.PRG/4DEA8.c:3078;
// was func_800BA7BC), opcode 0x9A
std::intptr_t VS::Event::CommandWaitMusicTimerOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitMusicTimer, command,
                    event::kWaitMusicTimerLength);
}

// decomp: vs_battle_script_9B_fadeSfxPitch (src/BATTLE/BATTLE.PRG/4DEA8.c:3084;
// was func_800BA800), opcode 0x9B
std::intptr_t VS::Event::CommandSlideSfxPitchOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideSfxPitch, command,
                    event::kSlideSfxPitchLength);
}

// decomp: vs_battle_script_9C_fadeSfxPan (src/BATTLE/BATTLE.PRG/4DEA8.c:3090;
// was func_800BA848), opcode 0x9C
std::intptr_t VS::Event::CommandSlideSfxPanOpcode(std::uint8_t* command,
                                                  std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideSfxPan, command,
                    event::kSlideSfxPanLength);
}

// decomp: vs_battle_script_9F_queueMusic (src/BATTLE/BATTLE.PRG/4DEA8.c:3096;
// was func_800BA890), opcode 0x9F
std::intptr_t VS::Event::CommandResumeMusicSlotOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandResumeMusicSlot, command,
                    event::kResumeMusicSlotLength);
}

// decomp: vs_battle_script_A8_setMusicJumpThreshold
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3163; was func_800BAAF0), opcode 0xA8
std::intptr_t VS::Event::CommandSetMusicJumpThresholdOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetMusicJumpThreshold, command,
                    event::kSetMusicJumpThresholdLength);
}

// decomp: vs_battle_script_F2_playMenuSelectSfx
// (src/BATTLE/BATTLE.PRG/4DEA8.c:3523; was vs_battle_script_playMenuSelectSfx),
// opcode 0xF2
std::intptr_t VS::Event::CommandPlayMenuSelectSfxOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayMenuSelectSfx, command,
                    event::kPlayMenuSelectSfxLength);
}
