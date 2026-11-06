#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's sound opcodes: sound effects on the script's channel
// groups, sound and wave bank slots, and the event music slot. Each handler
// checks its instruction's length, decodes its little-endian operands and
// drives the sound system only through ScriptAudioHost, so a mod can run the
// same commands against any mixer.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptStep.hpp"

namespace VS::Event {

// Instruction lengths in bytes, opcode included, from the event VM's opcode
// size table (BATTLE.PRG D_800E9C30).
inline constexpr std::size_t kPlaySoundLength = 5;
inline constexpr std::size_t kPlayActorSfxLength = 4;
inline constexpr std::size_t kSlideSfxVolumeLength = 5;
inline constexpr std::size_t kStopAllSfxLength = 1;
inline constexpr std::size_t kStopSfxLength = 3;
inline constexpr std::size_t kLoadSfxSlotLength = 3;
inline constexpr std::size_t kWaitSfxSlotReleaseLength = 2;
inline constexpr std::size_t kFreeSfxLength = 2;
inline constexpr std::size_t kSelectSfxBankLength = 2;
inline constexpr std::size_t kLoadEventMusicLength = 1;
inline constexpr std::size_t kCrossfadeMusicLength = 5;
inline constexpr std::size_t kLoadMusicSlotLength = 3;
inline constexpr std::size_t kFreeMusicLength = 2;
inline constexpr std::size_t kPlayMusicLength = 4;
inline constexpr std::size_t kSlideMusicVolumeLength = 3;
inline constexpr std::size_t kSlideMusicTempoLength = 3;
inline constexpr std::size_t kSlideMusicPitchLength = 3;
inline constexpr std::size_t kStopMusicLength = 1;
inline constexpr std::size_t kWaitMusicSlotLength = 2;
inline constexpr std::size_t kPushMusicStateLength = 1;
inline constexpr std::size_t kWaitMusicSlotLoadLength = 2;
inline constexpr std::size_t kWaitMusicTimerLength = 3;
inline constexpr std::size_t kSlideSfxPitchLength = 5;
inline constexpr std::size_t kSlideSfxPanLength = 5;
inline constexpr std::size_t kLoadSoundFileLength = 2;
inline constexpr std::size_t kWaitSoundQueueLength = 1;
inline constexpr std::size_t kResumeMusicSlotLength = 2;
inline constexpr std::size_t kSetMusicJumpThresholdLength = 2;
inline constexpr std::size_t kPlayMenuSelectSfxLength = 5;

// The sound channel groups a script's source byte picks: each is the mask of
// voices that source plays on (BATTLE.PRG D_800E9B34).
inline constexpr std::size_t kScriptSfxSourceCount = 6;
using ScriptSfxChannelGroups = std::array<std::uint32_t, kScriptSfxSourceCount>;
inline constexpr ScriptSfxChannelGroups kScriptSfxChannelGroups{
    0x1, 0x7E, 0x200, 0xFF000, 0xF00000, 0xFFFFFF,
};
// The source byte that also records the suspended ambient sound.
inline constexpr std::uint8_t kAmbientSfxSource = 2;
// The menu-select sound the menu opcode plays on the second group
// (sfx.h VS_SFX_MENUSELECT).
inline constexpr int kMenuSelectSfx = 5;
inline constexpr std::uint32_t kMenuSfxChannelGroup = 0x7E;
// The music slot the zone's event music loads into.
inline constexpr int kEventMusicSlot = 3;

// Script requests the commands refuse rather than overrun.
enum class ScriptAudioRefusal : std::uint8_t {
  kSfxSource,     // a source byte past the channel groups (#307)
  kShortCommand,  // an instruction shorter than its operands
};

// The event state and sound system the sound opcodes read and change.
class ScriptAudioHost {
 public:
  ScriptAudioHost() = default;
  ScriptAudioHost(const ScriptAudioHost&) = delete;
  ScriptAudioHost& operator=(const ScriptAudioHost&) = delete;
  ScriptAudioHost(ScriptAudioHost&&) = delete;
  ScriptAudioHost& operator=(ScriptAudioHost&&) = delete;
  virtual ~ScriptAudioHost() = default;

  virtual void Refuse(ScriptAudioRefusal refusal, int value) = 0;

  // Actor references and the event VM.
  [[nodiscard]] virtual int ResolveActor(std::uint16_t reference) = 0;
  [[nodiscard]] virtual bool SkipActive() const = 0;

  // The music slot the music commands address, and the ambient sound a
  // suspended ambience resumes with.
  [[nodiscard]] virtual std::uint8_t MusicSlot() const = 0;
  virtual void SetMusicSlot(std::uint8_t slot) = 0;
  [[nodiscard]] virtual bool AmbientSoundSuspended() const = 0;
  virtual void SetSuspendedAmbientSound(std::int16_t sound) = 0;

  // Sound effects on a channel group.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void PlaySfx(std::uint32_t group, int effect, int pan,
                       int volume) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void PlaySfxCentered(std::uint32_t group, int effect) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void FadeSfxVolume(std::uint32_t group, int effect, int volume,
                             int ticks) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void FadeSfxPitch(std::uint32_t group, int effect, int pitch_scale,
                            int ticks) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void FadeSfxPan(std::uint32_t group, int effect, int pan,
                          int ticks) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void StopSfx(std::uint32_t group, int effect) = 0;
  virtual void StopAllSfx() = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void PlayActorSound(int actor, int sound) = 0;

  // Sound effect banks and wave banks.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void LoadSfxBank(int bank, unsigned slot) = 0;
  [[nodiscard]] virtual int PollSfxBankLoad(unsigned slot) = 0;
  virtual void FreeSfxBank(unsigned slot) = 0;
  virtual void SelectSfxBank(int slot) = 0;
  virtual void LoadWaveBank(int bank) = 0;
  [[nodiscard]] virtual int UpdateWaveBankLoad() = 0;
  [[nodiscard]] virtual int ZoneSoundFile() = 0;

  // Music slots.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void LoadMusic(int music, int slot) = 0;
  [[nodiscard]] virtual int PollMusicLoad(unsigned slot) = 0;
  virtual void FreeMusic(int slot) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void PlayMusic(int slot, int volume, int ticks) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void CrossfadeMusic(int slot, int volume, unsigned ticks) = 0;
  virtual void ResumeMusic(int slot) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void FadeMusicVolume(int slot, int volume, int ticks) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void FadeMusicTempo(int slot, int scale, int ticks) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the sound API order.
  virtual void FadeMusicPitch(int slot, int scale, int ticks) = 0;
  virtual void StopMusic(int slot) = 0;
  [[nodiscard]] virtual int QueryMusicVolume(int slot) = 0;
  [[nodiscard]] virtual int MusicVolume(int slot) = 0;
  [[nodiscard]] virtual int MusicBeatPosition() = 0;
  virtual void SaveMusicState() = 0;
  virtual void SetMusicJumpThreshold(int threshold) = 0;
};

// decomp: func_800BA2E0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2280), opcode 0x80:
// plays a sound on a source's group; the ambient source plays only while the
// ambience is suspended and records the sound it resumes with.
ScriptStep ScriptPlaySoundCommand(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA35C (src/BATTLE/BATTLE.PRG/4A0A8.c:2292), opcode 0x81
ScriptStep CommandPlayActorSfx(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA39C (src/BATTLE/BATTLE.PRG/4A0A8.c:2298), opcode 0x82
ScriptStep CommandSlideSfxVolume(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA3E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2304), opcode 0x83
ScriptStep CommandStopAllSfx(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA404 (src/BATTLE/BATTLE.PRG/4A0A8.c:2310), opcode 0x84
ScriptStep CommandStopSfx(ScriptCommand command, ScriptAudioHost& host);
// decomp: vs_battle_script_loadSfxSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2316),
// opcode 0x85
ScriptStep CommandLoadSfxSlot(ScriptCommand command, ScriptAudioHost& host);
// decomp: vs_battle_script_freeSfxSlot (src/BATTLE/BATTLE.PRG/4A0A8.c:2322),
// opcode 0x86: holds while the slot's bank loads.
ScriptStep CommandWaitSfxSlotRelease(ScriptCommand command,
                                     ScriptAudioHost& host);
// decomp: vs_battle_script_freeSfx (src/BATTLE/BATTLE.PRG/4A0A8.c:2327),
// opcode 0x87: also ends the frame while a cutscene skip runs.
ScriptStep CommandFreeSfx(ScriptCommand command, ScriptAudioHost& host);
// decomp: vs_battle_script_setCurrentSfx
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2333), opcode 0x88
ScriptStep CommandSelectSfxBank(ScriptCommand command, ScriptAudioHost& host);
// decomp: vs_battle_script_loadSoundFile
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2339), opcode 0x89: loads the zone's music
// and wave bank.
ScriptStep CommandLoadEventMusic(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA52C (src/BATTLE/BATTLE.PRG/4A0A8.c:2347), opcode 0x8F:
// fades the current slot out and crossfades another in.
ScriptStep CommandCrossfadeMusic(ScriptCommand command, ScriptAudioHost& host);
// decomp: vs_battle_script_loadMusicSlot
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2354), opcode 0x90
ScriptStep CommandLoadMusicSlot(ScriptCommand command, ScriptAudioHost& host);
// decomp: vs_battle_script_freeMusic (src/BATTLE/BATTLE.PRG/4A0A8.c:2360),
// opcode 0x91: also ends the frame while a cutscene skip runs.
ScriptStep CommandFreeMusic(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA5E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2366), opcode 0x92
ScriptStep CommandPlayMusic(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA61C (src/BATTLE/BATTLE.PRG/4A0A8.c:2372), opcode 0x93
ScriptStep CommandSlideMusicVolume(ScriptCommand command,
                                   ScriptAudioHost& host);
// decomp: func_800BA650 (src/BATTLE/BATTLE.PRG/4A0A8.c:2378), opcode 0x94
ScriptStep CommandSlideMusicTempo(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA684 (src/BATTLE/BATTLE.PRG/4A0A8.c:2384), opcode 0x95
ScriptStep CommandSlideMusicPitch(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA6B8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2390), opcode 0x96
ScriptStep CommandStopMusic(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA6E0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2396), opcode 0x97:
// holds while a playing slot is audible, then stops it.
ScriptStep CommandWaitMusicSlot(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA72C (src/BATTLE/BATTLE.PRG/4A0A8.c:2408), opcode 0x98
ScriptStep CommandPushMusicState(ScriptCommand command, ScriptAudioHost& host);
// decomp: vs_battle_script_clearMusicLoadSlot
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2414), opcode 0x99: holds while the slot's
// music loads.
ScriptStep CommandWaitMusicSlotLoad(ScriptCommand command,
                                    ScriptAudioHost& host);
// decomp: vs_battle_script_loadSoundFile2
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2419), opcode 0x9D
ScriptStep CommandLoadSoundFile(ScriptCommand command, ScriptAudioHost& host);
// decomp: vs_battle_script_processSoundQueue
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2425), opcode 0x9E: holds while a wave bank
// loads.
ScriptStep CommandWaitSoundQueue(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA7BC (src/BATTLE/BATTLE.PRG/4A0A8.c:2430), opcode 0x9A:
// holds until the music reaches a beat.
ScriptStep CommandWaitMusicTimer(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA800 (src/BATTLE/BATTLE.PRG/4A0A8.c:2436), opcode 0x9B
ScriptStep CommandSlideSfxPitch(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA848 (src/BATTLE/BATTLE.PRG/4A0A8.c:2442), opcode 0x9C
ScriptStep CommandSlideSfxPan(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BA890 (src/BATTLE/BATTLE.PRG/4A0A8.c:2448), opcode 0x9F
ScriptStep CommandResumeMusicSlot(ScriptCommand command, ScriptAudioHost& host);
// decomp: func_800BAAF0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2513), opcode 0xA8
ScriptStep CommandSetMusicJumpThreshold(ScriptCommand command,
                                        ScriptAudioHost& host);
// decomp: vs_battle_script_playMenuSelectSfx
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2874), opcode 0xF2
ScriptStep CommandPlayMenuSelectSfx(ScriptCommand command,
                                    ScriptAudioHost& host);

}  // namespace VS::Event
