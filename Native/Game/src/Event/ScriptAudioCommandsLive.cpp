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
#include "Event/ScriptAudioCommands.hpp"
#include "Event/ScriptAudioOpcodes.h"
#include "Field/RoomLoadingLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptVm.h"
#include "Field/RoomLoadingLiveState.h"
#include "Legacy/Psx.h"
}

namespace {
namespace event = VS::Event;
namespace audio = VS::Audio;

constexpr char kSkipActive = 2;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("event", "%s", message);
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
    Core_Warnf("event", "Script sound command refused %s (%d)",
               RefusalText(refusal), value);
  }

  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    return Event_ResolveScriptActorReference(reference);
  }
  [[nodiscard]] bool SkipActive() const override {
    return Event_skipState == kSkipActive;
  }

  [[nodiscard]] std::uint8_t MusicSlot() const override {
    return static_cast<std::uint8_t>(Event_musicSlot);
  }
  void SetMusicSlot(std::uint8_t slot) override {
    Event_musicSlot = static_cast<char>(slot);
  }
  [[nodiscard]] bool AmbientSoundSuspended() const override {
    return Event_isAmbientSoundSuspended != 0;
  }
  void SetSuspendedAmbientSound(std::int16_t sound) override {
    Event_suspendedAmbientSoundId = sound;
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

intptr_t RunCommand(Handler handler, const u_char* command,
                    std::size_t length) {
  Require(command != nullptr, "Script sound command is missing");
  LiveScriptAudioHost host;
  return static_cast<intptr_t>(
      handler(event::ScriptCommand{command, length}, host));
}
}  // namespace

// decomp: func_800BA2E0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2280), opcode 0x80
intptr_t Event_ScriptPlaySoundCommandOpcode(u_char* command,
                                            int16_t /*thread_id*/) {
  return RunCommand(event::ScriptPlaySoundCommand, command,
                    event::kPlaySoundLength);
}

// decomp: func_800BA35C (src/BATTLE/BATTLE.PRG/4A0A8.c:2292), opcode 0x81
intptr_t Event_CommandPlayActorSfxOpcode(u_char* command,
                                         int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayActorSfx, command,
                    event::kPlayActorSfxLength);
}

// decomp: func_800BA39C (src/BATTLE/BATTLE.PRG/4A0A8.c:2298), opcode 0x82
intptr_t Event_CommandSlideSfxVolumeOpcode(u_char* command,
                                           int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideSfxVolume, command,
                    event::kSlideSfxVolumeLength);
}

// decomp: func_800BA3E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2304), opcode 0x83
intptr_t Event_CommandStopAllSfxOpcode(u_char* command, int16_t /*thread_id*/) {
  return RunCommand(event::CommandStopAllSfx, command,
                    event::kStopAllSfxLength);
}

// decomp: func_800BA404 (src/BATTLE/BATTLE.PRG/4A0A8.c:2310), opcode 0x84
intptr_t Event_CommandStopSfxOpcode(u_char* command, int16_t /*thread_id*/) {
  return RunCommand(event::CommandStopSfx, command, event::kStopSfxLength);
}

// decomp: vs_battle_script_loadSfxSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2316),
// opcode 0x85
intptr_t Event_CommandLoadSfxSlotOpcode(u_char* command,
                                        int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadSfxSlot, command,
                    event::kLoadSfxSlotLength);
}

// decomp: vs_battle_script_freeSfxSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2322),
// opcode 0x86
intptr_t Event_CommandWaitSfxSlotReleaseOpcode(u_char* command,
                                               int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitSfxSlotRelease, command,
                    event::kWaitSfxSlotReleaseLength);
}

// decomp: vs_battle_script_freeSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2327), opcode
// 0x87
intptr_t Event_CommandFreeSfxOpcode(u_char* command, int16_t /*thread_id*/) {
  return RunCommand(event::CommandFreeSfx, command, event::kFreeSfxLength);
}

// decomp: vs_battle_script_setCurrentSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2333),
// opcode 0x88
intptr_t Event_CommandSelectSfxBankOpcode(u_char* command,
                                          int16_t /*thread_id*/) {
  return RunCommand(event::CommandSelectSfxBank, command,
                    event::kSelectSfxBankLength);
}

// decomp: vs_battle_script_loadSoundFile (src/BATTLE/BATTLE.PRG/4A0A8.c:2339),
// opcode 0x89
intptr_t Event_CommandLoadEventMusicOpcode(u_char* command,
                                           int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadEventMusic, command,
                    event::kLoadEventMusicLength);
}

// decomp: func_800BA52C (src/BATTLE/BATTLE.PRG/4A0A8.c:2347), opcode 0x8F
intptr_t Event_CommandCrossfadeMusicOpcode(u_char* command,
                                           int16_t /*thread_id*/) {
  return RunCommand(event::CommandCrossfadeMusic, command,
                    event::kCrossfadeMusicLength);
}

// decomp: vs_battle_script_loadMusicSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2354),
// opcode 0x90
intptr_t Event_CommandLoadMusicSlotOpcode(u_char* command,
                                          int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadMusicSlot, command,
                    event::kLoadMusicSlotLength);
}

// decomp: vs_battle_script_freeMusic (src/BATTLE/BATTLE.PRG/4A0A8.c:2360),
// opcode 0x91
intptr_t Event_CommandFreeMusicOpcode(u_char* command, int16_t /*thread_id*/) {
  return RunCommand(event::CommandFreeMusic, command, event::kFreeMusicLength);
}

// decomp: func_800BA5E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2366), opcode 0x92
intptr_t Event_CommandPlayMusicOpcode(u_char* command, int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayMusic, command, event::kPlayMusicLength);
}

// decomp: func_800BA61C (src/BATTLE/BATTLE.PRG/4A0A8.c:2372), opcode 0x93
intptr_t Event_CommandSlideMusicVolumeOpcode(u_char* command,
                                             int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideMusicVolume, command,
                    event::kSlideMusicVolumeLength);
}

// decomp: func_800BA650 (src/BATTLE/BATTLE.PRG/4A0A8.c:2378), opcode 0x94
intptr_t Event_CommandSlideMusicTempoOpcode(u_char* command,
                                            int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideMusicTempo, command,
                    event::kSlideMusicTempoLength);
}

// decomp: func_800BA684 (src/BATTLE/BATTLE.PRG/4A0A8.c:2384), opcode 0x95
intptr_t Event_CommandSlideMusicPitchOpcode(u_char* command,
                                            int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideMusicPitch, command,
                    event::kSlideMusicPitchLength);
}

// decomp: func_800BA6B8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2390), opcode 0x96
intptr_t Event_CommandStopMusicOpcode(u_char* command, int16_t /*thread_id*/) {
  return RunCommand(event::CommandStopMusic, command, event::kStopMusicLength);
}

// decomp: func_800BA6E0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2396), opcode 0x97
intptr_t Event_CommandWaitMusicSlotOpcode(u_char* command,
                                          int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitMusicSlot, command,
                    event::kWaitMusicSlotLength);
}

// decomp: func_800BA72C (src/BATTLE/BATTLE.PRG/4A0A8.c:2408), opcode 0x98
intptr_t Event_CommandPushMusicStateOpcode(u_char* command,
                                           int16_t /*thread_id*/) {
  return RunCommand(event::CommandPushMusicState, command,
                    event::kPushMusicStateLength);
}

// decomp: vs_battle_script_clearMusicLoadSlot
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2414), opcode 0x99
intptr_t Event_CommandWaitMusicSlotLoadOpcode(u_char* command,
                                              int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitMusicSlotLoad, command,
                    event::kWaitMusicSlotLoadLength);
}

// decomp: vs_battle_script_loadSoundFile2 (src/BATTLE/BATTLE.PRG/4A0A8.c:2419),
// opcode 0x9D
intptr_t Event_CommandLoadSoundFileOpcode(u_char* command,
                                          int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadSoundFile, command,
                    event::kLoadSoundFileLength);
}

// decomp: vs_battle_script_processSoundQueue
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2425), opcode 0x9E
intptr_t Event_CommandWaitSoundQueueOpcode(u_char* command,
                                           int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitSoundQueue, command,
                    event::kWaitSoundQueueLength);
}

// decomp: func_800BA7BC (src/BATTLE/BATTLE.PRG/4A0A8.c:2430), opcode 0x9A
intptr_t Event_CommandWaitMusicTimerOpcode(u_char* command,
                                           int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitMusicTimer, command,
                    event::kWaitMusicTimerLength);
}

// decomp: func_800BA800 (src/BATTLE/BATTLE.PRG/4A0A8.c:2436), opcode 0x9B
intptr_t Event_CommandSlideSfxPitchOpcode(u_char* command,
                                          int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideSfxPitch, command,
                    event::kSlideSfxPitchLength);
}

// decomp: func_800BA848 (src/BATTLE/BATTLE.PRG/4A0A8.c:2442), opcode 0x9C
intptr_t Event_CommandSlideSfxPanOpcode(u_char* command,
                                        int16_t /*thread_id*/) {
  return RunCommand(event::CommandSlideSfxPan, command,
                    event::kSlideSfxPanLength);
}

// decomp: func_800BA890 (src/BATTLE/BATTLE.PRG/4A0A8.c:2448), opcode 0x9F
intptr_t Event_CommandResumeMusicSlotOpcode(u_char* command,
                                            int16_t /*thread_id*/) {
  return RunCommand(event::CommandResumeMusicSlot, command,
                    event::kResumeMusicSlotLength);
}

// decomp: func_800BAAF0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2513), opcode 0xA8
intptr_t Event_CommandSetMusicJumpThresholdOpcode(u_char* command,
                                                  int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetMusicJumpThreshold, command,
                    event::kSetMusicJumpThresholdLength);
}

// decomp: vs_battle_script_playMenuSelectSfx
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2874), opcode 0xF2
intptr_t Event_CommandPlayMenuSelectSfxOpcode(u_char* command,
                                              int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayMenuSelectSfx, command,
                    event::kPlayMenuSelectSfxLength);
}
