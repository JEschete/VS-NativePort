#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Audio/SoundBanks.hpp"
#include "Audio/SoundControl.hpp"
#include "Content/CdDrive.hpp"

// The music slots (func_80044D94 through vs_main_stopMusic, SLUS_010.40
// main.c:9490-9790): four slots, numbered from 1, each holding a music file
// loaded from the disc into the high end of the game heap, and the slot whose
// music is playing. The slot record (vs_main_soundData), the music file
// table, the disc requests, the heap, the playback front end and the sound
// control requests go through MusicHost. Also the zone's music switch
// (func_8007BE10, BATTLE.PRG 146C.c:8579).
namespace VS::Audio {
// The music file table's entries (_musicLBAs and _musicFileSizes, 800 bytes
// each).
inline constexpr std::int32_t kMusicFileCount = 200;

// vs_main_loadMusicSlot's diagnostics (vs_main_nop9): a slot it cannot load
// into, or a buffer the slot still held, and a request it still held.
inline constexpr std::uint32_t kMusicSlotDiagnostic = 0x8E;
inline constexpr std::uint32_t kMusicRequestHeldDiagnostic = 0x98;

// The playing music (currentMusicSlot, playingMusicHandle, currentMusicId
// and currentMusicData).
struct MusicPlayback {
  std::int32_t slot = 0;
  std::uint32_t handle = kNoMusicHandle;
  std::uint32_t music_id = kNoMusicHandle;
  BankBuffer data = BankBuffer::kNone;

  friend bool operator==(const MusicPlayback&, const MusicPlayback&) = default;
};

enum class MusicRefusal : std::uint8_t {
  // A slot number other than 1 to kMusicSlotCount (or 0 where it names every
  // slot); the original indexed the slot arrays with it.
  kSlotOutOfRange,
  // vs_main_freeMusic(0): the original stopped all music, then freed the word
  // before the music buffers.
  kFreeWithoutSlot,
  // A music id past the music file table.
  kMusicFileOutOfRange,
};

class MusicHost {
 public:
  MusicHost() = default;
  MusicHost(const MusicHost&) = delete;
  MusicHost& operator=(const MusicHost&) = delete;
  MusicHost(MusicHost&&) = delete;
  MusicHost& operator=(MusicHost&&) = delete;
  virtual ~MusicHost() = default;

  // Music slot `index` (below kMusicSlotCount).
  virtual MusicSlot Music(std::size_t index) = 0;
  virtual void StoreMusic(std::size_t index, const MusicSlot& slot) = 0;
  virtual MusicPlayback Playback() = 0;
  virtual void StorePlayback(const MusicPlayback& playback) = 0;
  // The music file table; nullopt for an id past it.
  virtual std::optional<Content::DiscFileExtent> MusicFile(std::int32_t id) = 0;

  // The disc request queue (SoundBankHost's).
  virtual std::optional<DiscRequestRef> AllocateRequest(
      const Content::DiscFileExtent& file) = 0;
  virtual void EnqueueRequest(DiscRequestRef request,
                              BankBuffer destination) = 0;
  virtual DiscRequestRef CompleteRefusedRequest(
      const Content::DiscFileExtent& file, BankBuffer destination) = 0;
  virtual Content::DiscRequestState RequestState(DiscRequestRef request) = 0;
  virtual void ReleaseRequest(DiscRequestRef request) = 0;
  // vs_main_updatePlayTimeAndLoading(0).
  virtual void WaitFrame() = 0;
  virtual BankBuffer AllocateBuffer(std::int32_t bytes) = 0;
  virtual void FreeBuffer(BankBuffer buffer) = 0;

  // The playback front end (SoundPlayback.hpp): the started music's handle,
  // 0 when none started.
  virtual std::uint32_t StartMusic(BankBuffer music) = 0;
  virtual std::uint32_t QueueMusicTransition(BankBuffer music) = 0;
  virtual std::uint32_t CrossfadeMusic(BankBuffer music,
                                       std::uint32_t fade_ticks,
                                       std::int32_t target_volume) = 0;
  virtual void StopMusic(std::uint32_t handle) = 0;
  virtual std::int32_t MusicVolume(std::uint32_t handle) = 0;
  virtual std::int32_t MusicPosition() = 0;
  virtual void SaveMusicState() = 0;
  // The sound control requests (SoundControl.hpp).
  virtual void SetMusicBranchThreshold(std::int32_t threshold) = 0;
  virtual void FadeMusicVolume(std::uint32_t handle, Control::Fade fade) = 0;
  virtual void FadeMusicTempoScale(Control::Fade fade) = 0;
  virtual void FadeMusicPitchScale(Control::Fade fade) = 0;
  virtual void StopAllMusic() = 0;
  // The zone's music switch (D_800F18B0, the zone context's
  // isMusicFadeEnabled word).
  virtual void StoreZoneMusicSwitch(std::int32_t enabled) = 0;

  virtual void Diagnose(std::uint32_t code) = 0;
  virtual void Refuse(MusicRefusal refusal) = 0;
};

// func_8007BE10: stores the zone's music switch and returns it.
std::int32_t SetZoneMusicSwitchEnabled(MusicHost& host, std::int32_t enabled);
// func_80044D94: the playing music's position.
std::int32_t MusicBeatPosition(MusicHost& host);
// func_80044DB4.
void SaveMusicPlaybackState(MusicHost& host);
// func_80044DD4.
void SetMusicJumpThreshold(MusicHost& host, std::int32_t threshold);
// Stops slot `id`'s music (and forgets the playing music when it is that
// slot's), or every slot's when `id` is 0. Returns 1, or 0 when `id` is past
// the slots (refused).
std::int32_t StopMusicSlotPlayback(MusicHost& host, std::int32_t id);
// Plays slot `id`'s music as a queued transition, a crossfade or a fresh
// start faded toward `volume`, when the slot holds music. Returns the
// playing handle, or 0 for slot 0 or a slot past the slots (refused).
std::int32_t ResumeMusicSlot(MusicHost& host, std::int32_t id);
std::int32_t CrossfadeMusicSlot(MusicHost& host, std::int32_t id,
                                std::int32_t target_volume,
                                std::uint32_t fade_ticks);
std::int32_t PlayMusicSlot(MusicHost& host, std::int32_t id,
                           Control::Fade volume);
std::int32_t CurrentMusicSlot(MusicHost& host);
// The playing slot's music id, 0 when none plays (or the recorded slot is
// past the slots, refused).
std::int32_t CurrentMusicId(MusicHost& host);
// Starts loading music `id` into `target_slot` (1 to 4) unless the slot holds
// it already. Returns the slot, or 0 for an id or slot it cannot load
// (diagnosed) or an id past the music file table (refused).
std::int32_t LoadMusicSlot(MusicHost& host, std::int32_t id,
                           std::int32_t target_slot);
std::int32_t LoadMusicIntoFreeSlot(MusicHost& host, std::int32_t id);
// kLoadInFlight while the slot's load is in flight, kLoadIdle once it loaded
// (or when the slot has none, or is past the slots), kLoadFailed when the
// read failed. The slot then keeps the failed marker, which polls as
// kLoadIdle; the original read the request through the marker instead.
std::int32_t PollMusicSlotLoad(MusicHost& host, std::uint32_t slot);
std::int32_t PollMusicLoads(MusicHost& host);
std::int32_t LoadMusicSlotBlocking(MusicHost& host, std::int32_t id,
                                   std::int32_t target_slot);
std::int32_t LoadFreeMusicSlotBlocking(MusicHost& host, std::int32_t id);
// Stops slot `id`'s music and frees its buffer. Returns 1, or 0 when the slot
// held none. Slot 0 stops every slot's music, then is refused.
std::int32_t FreeMusicSlot(MusicHost& host, std::int32_t id);
// Slot `id`'s music volume, 0 when it plays none (or is past the slots,
// refused).
std::int32_t MusicSlotVolume(MusicHost& host, std::int32_t id);
std::int32_t QueryMusicSlotVolume(MusicHost& host, std::int32_t id);
// Fade the playing music when slot `id` is the playing slot. Return 1, or 0
// when it is not.
std::int32_t FadeMusicSlotVolume(MusicHost& host, std::int32_t id,
                                 Control::Fade volume);
std::int32_t FadeMusicSlotTempo(MusicHost& host, std::int32_t id,
                                Control::Fade tempo_scale);
std::int32_t FadeMusicSlotPitch(MusicHost& host, std::int32_t id,
                                Control::Fade pitch_scale);
void StopAndFreeCurrentMusic(MusicHost& host);
}  // namespace VS::Audio
